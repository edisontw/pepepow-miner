"""End-to-end loopback test using the pinned-Core offline miner binary."""
import json
import os
import signal
import select
import time
import queue
import socket
import subprocess
import sys
import threading
from pathlib import Path


def read_line(sock):
    data = bytearray()
    while len(data) <= 65536:
        chunk = sock.recv(1)
        if not chunk:
            raise AssertionError("client closed the loopback connection")
        data += chunk
        if chunk == b"\n":
            return bytes(data)
    raise AssertionError("client sent an oversized JSONL frame")


def notify_for(template, job_id):
    frame = json.loads(template)
    frame["params"][0] = job_id
    return (json.dumps(frame, separators=(",", ":")) + "\n").encode("ascii")


def check_real_core_timeout(binary, notify_template, extranonce2, drip=False):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(5)
    process = subprocess.Popen(
        [binary, str(listener.getsockname()[1]), "mock.worker", "0", "10000",
         extranonce2, "--max-ms", "180" if drip else "1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )
    events = queue.Queue()

    def read_stdout():
        assert process.stdout is not None
        for line in process.stdout:
            try:
                events.put(json.loads(line))
            except json.JSONDecodeError as error:
                events.put(error)
                return
        events.put(EOFError("timeout-case miner closed stdout"))

    threading.Thread(target=read_stdout, daemon=True).start()

    def until(predicate, timeout=5):
        while True:
            item = events.get(timeout=timeout)
            if isinstance(item, BaseException):
                raise AssertionError(str(item))
            if predicate(item):
                return item

    try:
        peer, address = listener.accept()
        assert address[0] == "127.0.0.1"
        peer.settimeout(5)
        with peer:
            assert json.loads(read_line(peer))["method"] == "mining.subscribe"
            peer.sendall((
                '{"id":1,"result":[[["mining.set_difficulty","s"],'
                '["mining.notify","s"]],"aabbccdd",4],"error":null}\n'
            ).encode("ascii"))
            assert json.loads(read_line(peer))["method"] == "mining.authorize"
            peer.sendall(b'{"id":2,"result":true,"error":null}\n')
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[1e15]}\n'
                + notify_for(notify_template, "timeout-job")
                + (b"{" if drip else b"")
            )
            started = time.monotonic()
            if drip:
                while process.poll() is None and time.monotonic() - started < 1.25:
                    try:
                        peer.sendall(b" ")
                    except (BrokenPipeError, ConnectionResetError):
                        break
                    time.sleep(0.04)
                assert time.monotonic() - started < 1.1, "drip blocked mining deadline"
            timeout_event = until(lambda item: item.get("event") == "timeout")
            assert timeout_event["job_id"] == "timeout-job"
            summary = until(lambda item: item.get("event") == "summary")
            assert summary["accepted"] == 0 and summary["rejected"] == 0
        status = process.wait(timeout=5)
        stderr = process.stderr.read() if process.stderr else ""
        assert status == 4, (status, stderr)
        assert not stderr, stderr
    finally:
        listener.close()
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def check_connection_fault(binary, mode):
    expected_status = 130 if mode == "cancel_pending" else 1
    listener = None
    process = None
    if mode == "connection_refused":
        reservation = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
        reservation.close()
        process = subprocess.Popen(
            [binary, str(port), "mock.worker", "0", "1", "00000000"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
    else:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(5)
        port = listener.getsockname()[1]
        process = subprocess.Popen(
            [binary, str(port), "mock.worker", "0", "1", "00000000"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )

    try:
        if mode != "connection_refused":
            peer, address = listener.accept()
            with peer:
                assert address[0] == "127.0.0.1"
                peer.settimeout(3)
                assert json.loads(read_line(peer)) == {
                    "id": 1, "method": "mining.subscribe", "params": []
                }
                if mode == "no_subscribe_reply":
                    pass  # Keep the socket open until the bounded read timeout.
                elif mode == "cancel_pending":
                    peer.sendall(b'{"id":1,')
                    process.send_signal(signal.SIGTERM)
                    assert peer.recv(1) == b""
                elif mode == "midstream_close":
                    peer.shutdown(socket.SHUT_RDWR)
                elif mode == "truncated_subscribe":
                    peer.sendall(b'{"id":1,"result":')
                    peer.shutdown(socket.SHUT_WR)
                elif mode == "invalid_subscribe":
                    peer.sendall(b'{"id":1,"result":false,"error":null}\n')
                elif mode in ("rejected_authorize", "invalid_authorize", "no_authorize_reply"):
                    peer.sendall((
                        b'{"id":1,"result":[[["mining.set_difficulty","s"],'
                        b'["mining.notify","s"]],"aabbccdd",4],"error":null}\n'
                    ))
                    assert json.loads(read_line(peer)) == {
                        "id": 2,
                        "method": "mining.authorize",
                        "params": ["mock.worker", "x"],
                    }
                    if mode == "rejected_authorize":
                        peer.sendall(b'{"id":2,"result":false,"error":null}\n')
                    elif mode == "invalid_authorize":
                        peer.sendall(b'{"id":2,"result":null,"error":null}\n')
                else:
                    raise AssertionError(f"unknown connection fault: {mode}")
                stdout, stderr = process.communicate(timeout=6)
        else:
            stdout, stderr = process.communicate(timeout=5)

        assert process.returncode == expected_status, (mode, process.returncode, stdout, stderr)
        assert process.poll() is not None, (mode, "miner process did not exit")
        print(f"PASS: bounded loopback fault {mode} (exit {expected_status}; no child process left running)")
    finally:
        if listener is not None:
            listener.close()
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def check_submit_without_reply(binary, notify_template, extranonce2):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    assert listener.getsockname()[0] == "127.0.0.1"
    listener.listen(1)
    listener.settimeout(5)
    process = subprocess.Popen(
        [binary, str(listener.getsockname()[1]), "mock.worker", "0", "10000",
         extranonce2, "--max-ms", "10000"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    peer = None
    try:
        peer, address = listener.accept()
        assert address[0] == "127.0.0.1"
        peer.settimeout(5)
        assert json.loads(read_line(peer)) == {
            "id": 1, "method": "mining.subscribe", "params": []
        }
        peer.sendall((
            '{"id":1,"result":[[["mining.set_difficulty","s"],'
            '["mining.notify","s"]],"aabbccdd",4],"error":null}\n'
        ).encode("ascii"))
        assert json.loads(read_line(peer))["method"] == "mining.authorize"
        peer.sendall(b'{"id":2,"result":true,"error":null}\n')
        peer.sendall(
            b'{"id":null,"method":"mining.set_difficulty","params":[98.304]}\n'
            + notify_for(notify_template, "unanswered-submit-job")
        )
        submit = json.loads(read_line(peer))
        assert submit["method"] == "mining.submit", submit
        assert submit["params"][1] == "unanswered-submit-job", submit
        # Every notification is valid and arrives faster than the frame timeout.
        # Send a late accepted reply only if the client is incorrectly still alive.
        started = time.monotonic()
        while process.poll() is None and time.monotonic() - started < 3.0:
            try:
                peer.sendall(b'{"id":null,"method":"mining.set_difficulty","params":[98.304]}\n')
            except (BrokenPipeError, ConnectionResetError):
                break
            time.sleep(0.08)
        elapsed = time.monotonic() - started
        if process.poll() is None and elapsed >= 3.0:
            peer.sendall(json.dumps({"id": submit["id"], "result": True, "error": None}).encode() + b"\n")
        stdout, stderr = process.communicate(timeout=4)
        assert 1.3 <= elapsed < 2.9, (elapsed, stdout, stderr)
        assert process.returncode == 1, (process.returncode, stdout, stderr)
        events = [json.loads(line) for line in stdout.splitlines()]
        assert any(event.get("event") == "submit_sent" for event in events), events
        summary = next(event for event in events if event.get("event") == "summary")
        assert summary["accepted"] == 0 and summary["rejected"] == 0, summary
        assert peer.recv(1) == b"", "miner left the socket open after a missing submit reply"
        print("PASS: missing submit reply times out, accounts no share, and closes the socket")
    finally:
        listener.close()
        if peer is not None:
            peer.close()
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def check_reconnect(binary, notify_template, extranonce2, failover):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as first, \
            socket.socket(socket.AF_INET, socket.SOCK_STREAM) as second:
        for listener in (first, second):
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(5)
        options = ["--retries", "2"]
        if failover:
            options += ["--fallback-port", str(second.getsockname()[1]), "--mode", "SOLO"]
        command = [binary, str(first.getsockname()[1]), "mock.worker", "0", "10", extranonce2]
        with subprocess.Popen(command + options, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True) as process:
            def establish(listener, request_id):
                peer, address = listener.accept()
                assert address[0] == "127.0.0.1"
                peer.settimeout(4)
                try:
                    assert json.loads(read_line(peer)) == {
                        "id": request_id, "method": "mining.subscribe", "params": []
                    }
                    peer.sendall(json.dumps({
                        "id": request_id, "result": [[["mining.set_difficulty", "s"],
                            ["mining.notify", "s"]], "aabbccdd", 4], "error": None,
                    }).encode() + b"\n")
                    assert json.loads(read_line(peer)) == {
                        "id": request_id + 1, "method": "mining.authorize",
                        "params": ["mock.worker", "x"],
                    }
                    peer.sendall(json.dumps({"id": request_id + 1, "result": True,
                                             "error": None}).encode() + b"\n")
                    peer.sendall(b'{"id":null,"method":"mining.set_difficulty","params":[98.304]}\n')
                    return peer
                except BaseException:
                    peer.close()
                    raise

            try:
                with establish(first, 1) as peer:
                    peer.sendall(notify_for(notify_template, "interrupted"))
                    assert json.loads(read_line(peer))["id"] == 3
                    peer.sendall(b'{"id":3,')
                with establish(second if failover else first, 4) as peer:
                    peer.sendall(notify_for(notify_template, "interrupted"))
                    assert not select.select([peer], [], [], 0.2)[0], "duplicate submit"
                    peer.sendall(notify_for(notify_template, "recovered"))
                    assert json.loads(read_line(peer))["params"][1] == "recovered"
                    # A complete reply carrying the retired session's ID cannot count.
                    peer.sendall(b'{"id":3,"result":true,"error":null}\n')
                    assert peer.recv(1) == b""
                with establish(first, 7) as peer:
                    peer.sendall(notify_for(notify_template, "third-session"))
                    submit = json.loads(read_line(peer))
                    assert submit["id"] == 9 and submit["params"][1] == "third-session", submit
                    peer.sendall(b'{"id":9,"result":true,"error":null}\n')
                stdout, stderr = process.communicate(timeout=5)
                assert process.returncode == 0, (stdout, stderr)
                events = [json.loads(line) for line in stdout.splitlines()]
                connected = [event for event in events if event["event"] == "connected"]
                assert [event["connection_generation"] for event in connected] == [1, 3, 5]
                assert {event["mode"] for event in connected} == {"SOLO" if failover else "POOL"}
                assert events[-1]["accepted"] == 1
                assert any(event["event"] == "duplicate_candidate" for event in events)
                print(f"PASS: reconnect, stale response and retry recovery (failover={failover})")
            finally:
                if process.poll() is None:
                    process.kill()


def check_retry_exhaustion(binary, cancel=False):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reservation:
        reservation.bind(("127.0.0.1", 0))
        command = [binary, str(reservation.getsockname()[1]), "mock.worker", "0", "1",
                   "00000000", "--retries", "2", "--backoff-ms", "2000" if cancel else "20"]
        with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True) as process:
            try:
                if cancel:
                    events = queue.Queue()
                    threading.Thread(target=lambda: events.put(process.stdout.readline()),
                                     daemon=True).start()
                    assert json.loads(events.get(timeout=4))["event"] == "retry_wait"
                    started = time.monotonic()
                    process.send_signal(signal.SIGTERM)
                    process.communicate(timeout=1)
                    assert process.returncode == 130 and time.monotonic() - started < 1
                else:
                    stdout, stderr = process.communicate(timeout=4)
                    assert process.returncode == 1, stderr
                    assert [json.loads(line)["backoff_ms"] for line in stdout.splitlines()] == [20, 40]
                print(f"PASS: bounded retry (cancel={cancel})")
            finally:
                if process.poll() is None:
                    process.kill()


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_offline_stratum_miner_e2e.py OFFLINE_MINER")
    binary = str(Path(sys.argv[1]).resolve())
    fixture_dir = Path(__file__).parent / "vectors/stratum"
    transcript = (fixture_dir / "pool-loopback.jsonl").read_text(encoding="ascii").splitlines()
    expected = json.loads((fixture_dir / "pool-loopback-expected.json").read_text())
    notify_template = transcript[3]
    original_notify = json.loads(notify_template)
    ntime = original_notify["params"][7]
    extranonce2 = expected["extranonce2"]

    valid_prefix = [binary, "1", "mock.worker"]
    valid_suffix = [extranonce2]
    invalid_invocations = [
        valid_prefix + ["4294967295", "2"] + valid_suffix,
        valid_prefix + ["0", "0"] + valid_suffix,
        valid_prefix + ["0", "10001"] + valid_suffix,
        [binary, "65536", "mock.worker", "0", "1", extranonce2],
        valid_prefix + ["0", "1"] + valid_suffix + ["--max-ms", "0"],
        valid_prefix + ["0", "1"] + valid_suffix + ["--responses", "0"],
        valid_prefix + ["0", "1"] + valid_suffix + ["--scale", "inf"],
        valid_prefix + ["0", "1"] + valid_suffix +
        ["--max-ms", "10", "--max-ms", "20"],
        valid_prefix + ["0", "1"] + valid_suffix + ["--unknown", "1"],
    ]
    for command in invalid_invocations:
        rejected = subprocess.run(command, capture_output=True, text=True, timeout=2)
        assert rejected.returncode == 2, (command, rejected.returncode, rejected.stderr)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    assert listener.getsockname()[0] == "127.0.0.1"
    listener.listen(1)
    listener.settimeout(5)
    port = listener.getsockname()[1]
    process = subprocess.Popen(
        [binary, str(port), "mock.worker", "0", "10000", extranonce2,
         "--responses", "2", "--max-ms", "10000"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )
    output_events = queue.Queue()

    def read_stdout():
        assert process.stdout is not None
        for line in process.stdout:
            try:
                output_events.put(json.loads(line))
            except json.JSONDecodeError as error:
                output_events.put(error)
                return
        output_events.put(EOFError("offline miner closed stdout"))

    reader = threading.Thread(target=read_stdout, daemon=True)
    reader.start()

    def next_event(timeout=5):
        item = output_events.get(timeout=timeout)
        if isinstance(item, BaseException):
            raise AssertionError(str(item))
        return item

    def until(predicate, timeout=5):
        while True:
            item = next_event(timeout)
            if predicate(item):
                return item

    try:
        peer, address = listener.accept()
        assert address[0] == "127.0.0.1"
        peer.settimeout(5)
        with peer:
            subscribe = json.loads(read_line(peer))
            assert subscribe == {
                "id": 1, "method": "mining.subscribe", "params": []
            }
            peer.sendall((
                '{"id":1,"result":[[["mining.set_difficulty","0102030405060708"],'
                '["mining.notify","0102030405060708"]],"aabbccdd",4],"error":null}\n'
            ).encode("ascii"))
            authorize = json.loads(read_line(peer))
            assert authorize == {
                "id": 2,
                "method": "mining.authorize",
                "params": ["mock.worker", "x"],
            }
            peer.sendall(b'{"id":2,"result":true,"error":null}\n')

            # This first job is intentionally too hard for the 10,000 nonce
            # window, keeping the real Core worker active until job replacement.
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[1e15]}\n'
                + notify_for(notify_template, "stale-job")
            )
            until(lambda item: item.get("event") == "search_started" and
                 item.get("job_id") == "stale-job")

            # set_difficulty applies to the following notify. The replacement
            # uses the frozen pool fixture, whose nonce 2 digest is independently
            # recorded in pool-loopback-expected.json.
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[98.304]}\n'
                + notify_for(notify_template, "accepted-job")
            )
            until(lambda item: item.get("event") == "search_cancel_requested" and
                 item.get("job_id") == "stale-job")
            stale = until(lambda item: item.get("event") == "stale_candidate" and
                          item.get("job_id") == "stale-job")
            assert stale["search_status"] == "cancelled", stale
            accepted_candidate = until(lambda item: item.get("event") == "candidate" and
                                       item.get("job_id") == "accepted-job")
            assert accepted_candidate["nonce"] == f'{expected["expected_nonce"]:08x}'
            assert accepted_candidate["header_hex"] == expected["header_hex"]
            assert accepted_candidate["digest_raw"] == expected["digest_raw_hex"]

            accepted_submit = json.loads(read_line(peer))
            assert accepted_submit == {
                "id": 3,
                "method": "mining.submit",
                "params": ["mock.worker", "accepted-job", extranonce2,
                           ntime, f'{expected["expected_nonce"]:08x}'],
            }
            peer.sendall(
                b'{"id":3,"result":true,"error":null}\n'
                + notify_for(notify_template, "rejected-job")
            )

            rejected_candidate = until(lambda item: item.get("event") == "candidate" and
                                       item.get("job_id") == "rejected-job")
            assert rejected_candidate["header_hex"] == expected["header_hex"]
            assert rejected_candidate["digest_raw"] == expected["digest_raw_hex"]
            rejected_submit = json.loads(read_line(peer))
            assert rejected_submit == {
                "id": 4,
                "method": "mining.submit",
                "params": ["mock.worker", "rejected-job", extranonce2,
                           ntime, f'{expected["expected_nonce"]:08x}'],
            }
            peer.sendall(b'{"id":4,"result":false,"error":null}\n')

            summary = until(lambda item: item.get("event") == "summary")
            assert summary == {
                "event": "summary",
                "accepted": 1,
                "rejected": 1,
                "pool_errors": 0,
                "stale_candidates": 1,
            }, summary
        status = process.wait(timeout=5)
        stderr = process.stderr.read() if process.stderr else ""
        assert status == 0, (status, stderr)
        assert not stderr, stderr
    except BaseException:
        process.kill()
        process.wait(timeout=5)
        if process.stderr:
            sys.stderr.write(process.stderr.read())
        raise
    finally:
        listener.close()
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)

    check_connection_fault(binary, "connection_refused")
    check_connection_fault(binary, "no_subscribe_reply")
    check_connection_fault(binary, "no_authorize_reply")
    if os.name != "nt":
        check_connection_fault(binary, "cancel_pending")
    check_connection_fault(binary, "midstream_close")
    check_connection_fault(binary, "truncated_subscribe")
    check_connection_fault(binary, "invalid_subscribe")
    check_connection_fault(binary, "rejected_authorize")
    check_connection_fault(binary, "invalid_authorize")
    check_submit_without_reply(binary, notify_template, extranonce2)
    check_real_core_timeout(binary, notify_template, extranonce2)
    check_real_core_timeout(binary, notify_template, extranonce2, drip=True)
    check_reconnect(binary, notify_template, extranonce2, failover=False)
    check_reconnect(binary, notify_template, extranonce2, failover=True)
    check_retry_exhaustion(binary)
    if os.name != "nt":
        check_retry_exhaustion(binary, cancel=True)
    print("PASS: pinned-Core loopback replacement, full digest, mock accounting, stale cancellation, timeout, and bounded connection faults")


if __name__ == "__main__":
    main()
