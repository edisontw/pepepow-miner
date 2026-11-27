"""Exercise the opt-in public-transport CLI against a loopback Stratum mock."""

from __future__ import annotations

import json
import socket
import subprocess
import sys
import threading
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


def main() -> int:
    args = sys.argv[2:]
    binary = str(Path(sys.argv[1]).resolve())
    supports_cuda = False
    error_engine = None
    index = 0
    while index < len(args):
        if args[index] == "--supports-cuda" and not supports_cuda:
            supports_cuda = True
            index += 1
        elif args[index] == "--cuda-error-engine" and error_engine is None and index + 1 < len(args):
            error_engine = str(Path(args[index + 1]).resolve())
            index += 2
        else:
            raise SystemExit("invalid live miner loopback test arguments")
    if error_engine and not supports_cuda:
        raise SystemExit("CUDA error engine fixture requires --supports-cuda")
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
    print("PASS: opt-in live-mode loopback handshake, continuous nonce advance, bounded replies, and JSON events")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
