"""End-to-end loopback test using the pinned-Core offline miner binary."""
import json
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


def check_real_core_timeout(binary, notify_template, extranonce2):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(5)
    process = subprocess.Popen(
        [binary, str(listener.getsockname()[1]), "mock.worker", "0", "10000",
         extranonce2, "--max-ms", "1"],
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
            )
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

    check_real_core_timeout(binary, notify_template, extranonce2)
    print("PASS: pinned-Core loopback replacement, full digest, mock accounting, stale cancellation and real-core timeout")


if __name__ == "__main__":
    main()
