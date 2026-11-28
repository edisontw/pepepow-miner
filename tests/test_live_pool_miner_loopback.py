"""Exercise the opt-in public-transport CLI against a loopback Stratum mock."""

from __future__ import annotations

import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


def read_line(peer: socket.socket) -> dict:
    data = bytearray()
    while not data.endswith(b"\n"):
        part = peer.recv(1)
        if not part:
            raise AssertionError("client closed before completing the mock session")
        data.extend(part)
        if len(data) > 65536:
            raise AssertionError("client sent an oversized Stratum frame")
    return json.loads(data)


def send_line(peer: socket.socket, value: dict) -> None:
    peer.sendall(json.dumps(value, separators=(",", ":")).encode("ascii") + b"\n")


def serve(listener: socket.socket, errors: list[BaseException], submissions: list[dict]) -> None:
    try:
        listener.settimeout(10)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(10)
            subscribe = read_line(peer)
            assert subscribe["method"] == "mining.subscribe"
            send_line(peer, {"id": subscribe["id"], "result": [[
                ["mining.set_difficulty", "mock"], ["mining.notify", "mock"]
            ], "aabbccdd", 4], "error": None})

            authorize = read_line(peer)
            assert authorize["method"] == "mining.authorize"
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            notify = (
                b'{"id":null,"method":"mining.set_difficulty","params":[0.0000015]}\n'
                b'{"id":null,"method":"mining.notify","params":["mock-job",'
                b'"0000000000000000000000000000000000000000000000000000000000000000",'
                b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",true]}\n'
            )
            peer.sendall(notify)

            for index in range(2):
                submit = read_line(peer)
                assert submit["method"] == "mining.submit"
                submissions.append(submit)
                if index == 0:
                    # Repeat the same work with clean_jobs=false while the
                    # client awaits this submit reply. It must not reset nonce progress.
                    peer.sendall(
                        b'{"id":null,"method":"mining.notify","params":["mock-job",'
                        b'"0000000000000000000000000000000000000000000000000000000000000000",'
                        b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",false]}\n'
                    )
                send_line(peer, {"id": submit["id"], "result": True, "error": None})
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def serve_cuda_error(listener: socket.socket, errors: list[BaseException]) -> None:
    try:
        listener.settimeout(10)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(10)
            subscribe = read_line(peer)
            assert subscribe["method"] == "mining.subscribe"
            send_line(peer, {"id": subscribe["id"], "result": [[
                ["mining.set_difficulty", "mock"], ["mining.notify", "mock"]
            ], "aabbccdd", 4], "error": None})

            authorize = read_line(peer)
            assert authorize["method"] == "mining.authorize"
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[0.0000015]}\n'
                b'{"id":null,"method":"mining.notify","params":["cuda-error-job",'
                b'"0000000000000000000000000000000000000000000000000000000000000000",'
                b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",true]}\n'
            )
            if peer.recv(1):
                raise AssertionError("failed CUDA search must not submit a share")
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def run_cuda_error_event(binary: str, error_engine: str) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    errors: list[BaseException] = []
    server = threading.Thread(target=serve_cuda_error, args=(listener, errors), daemon=True)
    server.start()
    command = [
        binary, "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{port}",
        "--login", "mock-wallet.worker", "--extranonce2", "00000000",
        "--mode", "POOL", "--threads", "1", "--responses", "1",
        "--max-runtime-seconds", "10", "--device", "cuda",
        "--cuda-engine", error_engine, "--cpu-oracle", binary,
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    finally:
        server.join(timeout=2)
    if server.is_alive():
        raise AssertionError("CUDA error loopback server did not stop")
    if errors:
        raise errors[0]
    if result.returncode == 0:
        raise AssertionError("a failed CUDA engine must fail the mining run")
    events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    failure = next((item for item in events if item.get("event") == "search_error"), None)
    if not failure or failure.get("device") != "cuda" or failure.get("code") != "engine_failure":
        raise AssertionError(f"missing structured CUDA failure event: {events}")
    message = failure.get("message", "")
    if 'mock CUDA engine: init "failed" at C:\\gpu\\driver' not in message:
        raise AssertionError(f"CUDA error message lost quoting or backslashes: {failure}")
    if not message.endswith("invalid=?"):
        raise AssertionError(f"non-ASCII diagnostic bytes were not sanitized: {failure}")
    if any(item.get("event") in {"candidate", "submit_sent"} for item in events):
        raise AssertionError(f"a failed CUDA search emitted a share candidate: {events}")
    if "CUDA search failed: " not in result.stderr:
        raise AssertionError("human-readable CUDA diagnostic was not written to stderr")


def wait_for_signal_or_abort(signal: threading.Event, abort: threading.Event,
                             timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while not signal.is_set() and not abort.is_set():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        signal.wait(min(0.05, remaining))
    return signal.is_set()


def serve_cuda_job_replacement(listener: socket.socket,
                               errors: list[BaseException],
                               first_engine_ready: threading.Event,
                               replacement_engine_ready: threading.Event,
                               abort: threading.Event) -> None:
    try:
        listener.settimeout(10)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(10)
            subscribe = read_line(peer)
            assert subscribe["method"] == "mining.subscribe"
            send_line(peer, {"id": subscribe["id"], "result": [[
                ["mining.set_difficulty", "mock"], ["mining.notify", "mock"]
            ], "aabbccdd", 4], "error": None})

            authorize = read_line(peer)
            assert authorize["method"] == "mining.authorize"
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            send_line(peer, {"id": None, "method": "mining.set_difficulty",
                             "params": [0.0000015]})
            send_line(peer, {"id": None, "method": "mining.notify", "params": [
                "cuda-stale-job",
                "0000000000000000000000000000000000000000000000000000000000000000",
                "01000000", "ffffffff", [], "20000000", "1d00ffff", "65a0bc00", True,
            ]})
            if not wait_for_signal_or_abort(first_engine_ready, abort, 8):
                if not abort.is_set():
                    raise AssertionError("CUDA engine did not start for the first job")
                return

            send_line(peer, {"id": None, "method": "mining.notify", "params": [
                "cuda-replacement-job",
                "0000000000000000000000000000000000000000000000000000000000000000",
                "01000000", "ffffffff", [], "20000000", "1d00ffff", "65a0bc00", True,
            ]})
            if not wait_for_signal_or_abort(replacement_engine_ready, abort, 8):
                if not abort.is_set():
                    raise AssertionError("CUDA engine did not start for the replacement job")
                return

            # The client should remain silent while its replacement job is being
            # searched. Any bytes here would be an unexpected share submission.
            peer.settimeout(0.25)
            try:
                data = peer.recv(1)
            except socket.timeout:
                data = b""
            if data:
                raise AssertionError("stale CUDA work submitted data to the mock pool")
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def wait_for_ready_lines(path: Path, expected: int, process: subprocess.Popen[str],
                         timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            lines = read_marker_lines(path)
        except FileNotFoundError:
            lines = []
        if lines.count("ready") >= expected:
            return True
        if process.poll() is not None:
            return False
        time.sleep(0.01)
    return False


def read_marker_lines(path: Path) -> list[str]:
    try:
        return path.read_text(encoding="ascii").splitlines()
    except FileNotFoundError:
        return []


def run_cuda_job_replacement(binary: str, cancel_engine: str) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    first_engine_ready = threading.Event()
    replacement_engine_ready = threading.Event()
    abort = threading.Event()
    errors: list[BaseException] = []
    markers: list[str] = []
    with tempfile.TemporaryDirectory(prefix="pepepow-cuda-replace-") as directory:
        ready_file = Path(directory) / "engine-ready.txt"
        server = threading.Thread(
            target=serve_cuda_job_replacement,
            args=(listener, errors, first_engine_ready, replacement_engine_ready, abort),
            daemon=True,
        )
        server.start()
        environment = os.environ.copy()
        environment["PEPEPOW_TEST_CUDA_READY_FILE"] = str(ready_file)
        command = [
            binary, "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{port}",
            "--login", "mock-wallet.worker", "--extranonce2", "00000000",
            "--mode", "POOL", "--threads", "1", "--responses", "1",
            "--max-runtime-seconds", "10", "--device", "cuda",
            "--cuda-engine", cancel_engine, "--cpu-oracle", binary,
        ]
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=environment)
        try:
            if not wait_for_ready_lines(ready_file, 1, process, 8):
                if process.poll() is None:
                    raise AssertionError("first CUDA mock engine did not become ready before timeout")
                stdout, stderr = process.communicate()
                raise AssertionError(
                    "first CUDA mock engine exited before becoming ready: "
                    f"code={process.returncode}; stdout={stdout}; stderr={stderr}")
            first_engine_ready.set()
            if not wait_for_ready_lines(ready_file, 2, process, 8):
                if process.poll() is None:
                    raise AssertionError("job replacement did not launch a new CUDA search before timeout")
                stdout, stderr = process.communicate()
                raise AssertionError(
                    "miner exited before starting the replacement CUDA search: "
                    f"code={process.returncode}; stdout={stdout}; stderr={stderr}")
            replacement_engine_ready.set()
            try:
                stdout, stderr = process.communicate(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()
                raise AssertionError("CUDA job replacement loopback did not stop") from None
            markers = read_marker_lines(ready_file)
        finally:
            abort.set()
            if process.poll() is None:
                process.kill()
                process.communicate()
            server.join(timeout=2)
            if server.is_alive():
                listener.close()
                server.join(timeout=2)
    if server.is_alive():
        raise AssertionError("CUDA job replacement loopback server did not stop")
    if errors:
        raise errors[0]
    if markers.count("ready") != 2 or markers.count("cancelled") != 2:
        raise AssertionError(f"both CUDA mock searches must observe cancellation: {markers}")
    if process.returncode == 0:
        raise AssertionError("the mock transport closure should end the bounded test run")
    events = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
    cancel = next((item for item in events if item.get("event") == "search_cancel_requested"), None)
    if not cancel or cancel.get("job_id") != "cuda-stale-job":
        raise AssertionError(f"stale CUDA search was not cancelled: {events}; stderr={stderr}")
    stale = next((item for item in events if item.get("event") == "stale_candidate"), None)
    if not stale or stale.get("job_id") != "cuda-stale-job" or \
            stale.get("search_status") != "cancelled":
        raise AssertionError(f"replaced CUDA work was not classified as stale: {events}")
    if not any(item.get("event") == "search_started" and
               item.get("job_id") == "cuda-replacement-job" for item in events):
        raise AssertionError(f"replacement CUDA job was not started: {events}")
    if any(item.get("event") in {"candidate", "submit_sent"} for item in events):
        raise AssertionError(f"a stale CUDA candidate reached submission: {events}")


def serve_cuda_failover_endpoint(listener: socket.socket,
                                 errors: list[BaseException],
                                 job_id: str,
                                 engine_ready: threading.Event,
                                 abort: threading.Event) -> None:
    try:
        listener.settimeout(10)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(10)
            subscribe = read_line(peer)
            assert subscribe["method"] == "mining.subscribe"
            send_line(peer, {"id": subscribe["id"], "result": [[
                ["mining.set_difficulty", "mock"], ["mining.notify", "mock"]
            ], "aabbccdd", 4], "error": None})

            authorize = read_line(peer)
            assert authorize["method"] == "mining.authorize"
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            send_line(peer, {"id": None, "method": "mining.set_difficulty",
                             "params": [0.0000015]})
            send_line(peer, {"id": None, "method": "mining.notify", "params": [
                job_id,
                "0000000000000000000000000000000000000000000000000000000000000000",
                "01000000", "ffffffff", [], "20000000", "1d00ffff", "65a0bc00", True,
            ]})
            if not wait_for_signal_or_abort(engine_ready, abort, 8):
                if not abort.is_set():
                    raise AssertionError(f"CUDA engine did not start for {job_id}")
                return
            # Closing each local session forces the child process to observe the
            # transport cancellation before the next endpoint is attempted.
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def run_cuda_reconnect_failover(binary: str, cancel_engine: str) -> None:
    primary = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    primary.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    primary.bind(("127.0.0.1", 0))
    primary.listen(1)
    primary_port = primary.getsockname()[1]
    fallback = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    fallback.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    fallback.bind(("127.0.0.1", 0))
    fallback.listen(1)
    fallback_port = fallback.getsockname()[1]

    primary_engine_ready = threading.Event()
    fallback_engine_ready = threading.Event()
    abort = threading.Event()
    errors: list[BaseException] = []
    stdout = ""
    stderr = ""
    markers: list[str] = []
    process: subprocess.Popen[str] | None = None
    servers: list[threading.Thread] = []
    with tempfile.TemporaryDirectory(prefix="pepepow-cuda-failover-") as directory:
        ready_file = Path(directory) / "engine-ready.txt"
        servers = [
            threading.Thread(target=serve_cuda_failover_endpoint,
                             args=(primary, errors, "cuda-primary-job",
                                   primary_engine_ready, abort), daemon=True),
            threading.Thread(target=serve_cuda_failover_endpoint,
                             args=(fallback, errors, "cuda-fallback-job",
                                   fallback_engine_ready, abort), daemon=True),
        ]
        for server in servers:
            server.start()
        environment = os.environ.copy()
        environment["PEPEPOW_TEST_CUDA_READY_FILE"] = str(ready_file)
        command = [
            binary, "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{primary_port}",
            "--fallback-endpoint", f"stratum+tcp://127.0.0.1:{fallback_port}",
            "--login", "mock-wallet.worker", "--extranonce2", "00000000",
            "--mode", "POOL", "--threads", "1", "--responses", "1",
            "--max-runtime-seconds", "10", "--retries", "1",
            "--device", "cuda", "--cuda-engine", cancel_engine,
            "--cpu-oracle", binary,
        ]
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=environment)
        try:
            if not wait_for_ready_lines(ready_file, 1, process, 8):
                if process.poll() is None:
                    raise AssertionError("CUDA engine did not start on the primary endpoint before timeout")
                stdout, stderr = process.communicate()
                raise AssertionError(
                    "CUDA primary session exited before engine startup: "
                    f"code={process.returncode}; stdout={stdout}; stderr={stderr}")
            primary_engine_ready.set()
            if not wait_for_ready_lines(ready_file, 2, process, 8):
                if process.poll() is None:
                    raise AssertionError("CUDA engine did not restart on the fallback endpoint before timeout")
                stdout, stderr = process.communicate()
                raise AssertionError(
                    "CUDA run exited before fallback engine startup: "
                    f"code={process.returncode}; stdout={stdout}; stderr={stderr}")
            fallback_engine_ready.set()
            try:
                stdout, stderr = process.communicate(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()
                raise AssertionError("CUDA reconnect/failover loopback did not stop") from None
            markers = read_marker_lines(ready_file)
        finally:
            abort.set()
            if process.poll() is None:
                process.kill()
                process.communicate()
            for server in servers:
                server.join(timeout=2)
            for listener in (primary, fallback):
                listener.close()
            for server in servers:
                if server.is_alive():
                    server.join(timeout=2)

    if process is None or process.returncode == 0:
        raise AssertionError("the two closed mock sessions should end before any share reply")
    if any(server.is_alive() for server in servers):
        raise AssertionError("CUDA reconnect/failover mock servers did not stop")
    if errors:
        raise errors[0]
    if markers.count("ready") != 2 or markers.count("cancelled") != 2:
        raise AssertionError(f"both CUDA session children must observe cancellation: {markers}")

    events = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
    connected = [item for item in events if item.get("event") == "connected"]
    expected_transports = [
        f"stratum+tcp://127.0.0.1:{primary_port}",
        f"stratum+tcp://127.0.0.1:{fallback_port}",
    ]
    if [item.get("transport") for item in connected] != expected_transports or \
            [item.get("mode") for item in connected] != ["POOL", "POOL"]:
        raise AssertionError(f"CUDA reconnect changed endpoints or pool mode: {connected}; stderr={stderr}")
    if not any(item.get("event") == "retry_wait" for item in events):
        raise AssertionError(f"CUDA reconnect skipped its bounded retry transition: {events}")
    started_jobs = [item.get("job_id") for item in events
                    if item.get("event") == "search_started"]
    if started_jobs != ["cuda-primary-job", "cuda-fallback-job"]:
        raise AssertionError(f"CUDA session did not restart on the fallback job: {started_jobs}")
    if any(item.get("event") in {"candidate", "submit_sent"} for item in events):
        raise AssertionError(f"closed mock sessions emitted a share candidate: {events}")


def serve_cuda_candidate(listener: socket.socket,
                         errors: list[BaseException],
                         submissions: list[dict]) -> None:
    try:
        listener.settimeout(10)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(10)
            subscribe = read_line(peer)
            assert subscribe["method"] == "mining.subscribe"
            send_line(peer, {"id": subscribe["id"], "result": [[
                ["mining.set_difficulty", "mock"], ["mining.notify", "mock"]
            ], "aabbccdd", 4], "error": None})

            authorize = read_line(peer)
            assert authorize["method"] == "mining.authorize"
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[0.0000015]}\n'
                b'{"id":null,"method":"mining.notify","params":["cuda-candidate-job",'
                b'"0000000000000000000000000000000000000000000000000000000000000000",'
                b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",true]}\n'
            )

            submit = read_line(peer)
            assert submit["method"] == "mining.submit"
            submissions.append(submit)
            send_line(peer, {"id": submit["id"], "result": True, "error": None})
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def run_cuda_candidate_submission(binary: str, candidate_engine: str) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    errors: list[BaseException] = []
    submissions: list[dict] = []
    server = threading.Thread(
        target=serve_cuda_candidate,
        args=(listener, errors, submissions),
        daemon=True,
    )
    server.start()
    command = [
        binary, "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{port}",
        "--login", "mock-wallet.worker", "--extranonce2", "00000000",
        "--mode", "POOL", "--threads", "1", "--responses", "1",
        "--max-runtime-seconds", "10", "--device", "cuda",
        "--cuda-engine", candidate_engine, "--cpu-oracle", binary,
        "--cuda-batch-size", "128",
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    finally:
        server.join(timeout=12)
    if server.is_alive():
        raise AssertionError("CUDA candidate loopback server did not stop")
    if errors:
        raise errors[0]
    if result.returncode != 0:
        raise AssertionError(
            f"CUDA candidate loopback failed: stdout={result.stdout}; stderr={result.stderr}"
        )

    events = [json.loads(line) for line in result.stdout.splitlines()
              if line.startswith("{")]
    candidates = [item for item in events if item.get("event") == "candidate"]
    if len(candidates) != 1:
        raise AssertionError(f"expected one CPU-rechecked CUDA candidate: {events}")
    candidate = candidates[0]
    if candidate.get("job_id") != "cuda-candidate-job" or \
            candidate.get("nonce") != "00000001":
        raise AssertionError(f"candidate identity or nonce was incorrect: {candidate}")
    header_hex = candidate.get("header_hex", "")
    digest_hex = candidate.get("digest_raw", "")
    if len(header_hex) != 160 or any(ch not in "0123456789abcdef" for ch in header_hex):
        raise AssertionError(f"candidate did not expose the full 80-byte header: {candidate}")
    if header_hex[-8:] != "01000000":
        raise AssertionError(f"candidate nonce was not encoded little-endian in the header: {candidate}")
    if len(digest_hex) != 64 or any(ch not in "0123456789abcdef" for ch in digest_hex):
        raise AssertionError(f"candidate did not expose the complete 32-byte digest: {candidate}")
    if any(item.get("event") in {"search_error", "duplicate_candidate"}
           for item in events):
        raise AssertionError(f"candidate path reported a search/duplicate error: {events}")

    if len(submissions) != 1:
        raise AssertionError(f"expected exactly one mock submit: {submissions}")
    submit = submissions[0]
    if submit.get("params") != [
        "mock-wallet.worker", "cuda-candidate-job", "00000000", "65a0bc00", "00000001"
    ]:
        raise AssertionError(f"submit did not preserve job fields and nonce: {submit}")
    accepted = [item for item in events if item.get("event") == "share_accepted"]
    summary = next((item for item in events if item.get("event") == "summary"), None)
    if len(accepted) != 1 or summary is None or summary.get("accepted") != 1:
        raise AssertionError(f"mock accepted reply was not accounted once: {events}")
    if summary.get("hashes", 0) < 2:
        raise AssertionError(f"CPU recheck/search accounting omitted processed work: {summary}")
    if any(item.get("event") == "submit_sent" and
           (item.get("job_id") != candidate["job_id"] or
            item.get("nonce") != candidate["nonce"]) for item in events):
        raise AssertionError(f"miner submitted work other than the candidate: {events}")

def main() -> int:
    args = sys.argv[2:]
    binary = str(Path(sys.argv[1]).resolve())
    supports_cuda = False
    error_engine = None
    cancel_engine = None
    candidate_engine = None
    index = 0
    while index < len(args):
        if args[index] == "--supports-cuda" and not supports_cuda:
            supports_cuda = True
            index += 1
        elif args[index] == "--cuda-error-engine" and error_engine is None and index + 1 < len(args):
            error_engine = str(Path(args[index + 1]).resolve())
            index += 2
        elif args[index] == "--cuda-cancel-engine" and cancel_engine is None and index + 1 < len(args):
            cancel_engine = str(Path(args[index + 1]).resolve())
            index += 2
        elif args[index] == "--cuda-candidate-engine" and candidate_engine is None and index + 1 < len(args):
            candidate_engine = str(Path(args[index + 1]).resolve())
            index += 2
        else:
            raise SystemExit("invalid live miner loopback test arguments")
    if (error_engine or cancel_engine or candidate_engine) and not supports_cuda:
        raise SystemExit("CUDA engine fixtures require --supports-cuda")
    invalid_batch = subprocess.run(
        [
            binary, "--mine", "--endpoint", "stratum+tcp://127.0.0.1:1",
            "--login", "mock-wallet.worker", "--extranonce2", "00000000",
            "--mode", "POOL", "--threads", "1", "--responses", "1",
            "--max-runtime-seconds", "10", "--batch-size", "10000001",
        ],
        capture_output=True, text=True, timeout=5,
    )
    if invalid_batch.returncode != 2 or "Invalid live-mining arguments" not in invalid_batch.stderr:
        raise AssertionError("live CLI must reject batch sizes above its documented cap")
    if supports_cuda:
        cuda_without_oracles = subprocess.run(
            [
                binary, "--mine", "--endpoint", "stratum+tcp://127.0.0.1:1",
                "--login", "mock-wallet.worker", "--extranonce2", "00000000",
                "--mode", "POOL", "--threads", "1", "--responses", "1",
                "--max-runtime-seconds", "10", "--device", "cuda",
            ],
            capture_output=True, text=True, timeout=5,
        )
        if cuda_without_oracles.returncode != 2:
            raise AssertionError("CUDA mode must require both the engine and pinned-Core CPU oracle")
        cpu_with_cuda_paths = subprocess.run(
            [
                binary, "--mine", "--endpoint", "stratum+tcp://127.0.0.1:1",
                "--login", "mock-wallet.worker", "--extranonce2", "00000000",
                "--mode", "POOL", "--threads", "1", "--responses", "1",
                "--max-runtime-seconds", "10", "--device", "cpu",
                "--cuda-engine", "unused-engine", "--cpu-oracle", "unused-oracle",
            ],
            capture_output=True, text=True, timeout=5,
        )
        if cpu_with_cuda_paths.returncode != 2:
            raise AssertionError("CPU mode must reject unused CUDA engine paths")
        cuda_with_missing_files = subprocess.run(
            [
                binary, "--mine", "--endpoint", "stratum+tcp://127.0.0.1:1",
                "--login", "mock-wallet.worker", "--extranonce2", "00000000",
                "--mode", "POOL", "--threads", "1", "--responses", "1",
                "--max-runtime-seconds", "10", "--device", "cuda",
                "--cuda-engine", "__missing_cuda_engine__",
                "--cpu-oracle", "__missing_pinned_core_oracle__",
            ],
            capture_output=True, text=True, timeout=5,
        )
        if cuda_with_missing_files.returncode != 2:
            raise AssertionError("CUDA mode must reject missing engine/oracle files before connecting")
    else:
        cuda_on_cpu_binary = subprocess.run(
            [
                binary, "--mine", "--endpoint", "stratum+tcp://127.0.0.1:1",
                "--login", "mock-wallet.worker", "--extranonce2", "00000000",
                "--mode", "POOL", "--threads", "1", "--responses", "1",
                "--max-runtime-seconds", "10", "--device", "cuda",
            ],
            capture_output=True, text=True, timeout=5,
        )
        if cuda_on_cpu_binary.returncode != 2:
            raise AssertionError("the CPU-only executable must not accept CUDA mode")
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    submissions: list[dict] = []
    errors: list[BaseException] = []
    server = threading.Thread(target=serve, args=(listener, errors, submissions), daemon=True)
    server.start()
    command = [
        binary, "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{port}",
        "--login", "mock-wallet.worker", "--extranonce2", "00000000",
        "--mode", "POOL", "--threads", "1", "--responses", "2",
        "--max-runtime-seconds", "10", "--batch-size", "10001",
        "--submit-interval-ms", "100",
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    finally:
        server.join(timeout=2)
    if server.is_alive():
        print("mock server did not stop", file=sys.stderr)
        return 1
    if errors:
        raise errors[0]
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return 1
    events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    candidates = [item for item in events if item.get("event") == "candidate"]
    duplicate_candidates = [item for item in events if item.get("event") == "duplicate_candidate"]
    summary = next((item for item in events if item.get("event") == "summary"), None)
    if [item["nonce"] for item in candidates] != ["00000000", "00000001"]:
        raise AssertionError(f"expected two distinct sequential nonces: {candidates}")
    if duplicate_candidates:
        raise AssertionError(f"an unchanged notify reset nonce progress: {duplicate_candidates}")
    if len(submissions) != 2 or summary is None or summary.get("accepted") != 2:
        raise AssertionError(f"missing bounded share accounting: {summary}; submits={submissions}")
    if any(item["params"][0] != "mock-wallet.worker" for item in submissions):
        raise AssertionError("submit login did not preserve the authorized worker identity")
    if error_engine:
        run_cuda_error_event(binary, error_engine)
    if cancel_engine:
        run_cuda_job_replacement(binary, cancel_engine)
        run_cuda_reconnect_failover(binary, cancel_engine)
    if candidate_engine:
        run_cuda_candidate_submission(binary, candidate_engine)
    print("PASS: opt-in live-mode and CUDA candidate loopback contracts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
