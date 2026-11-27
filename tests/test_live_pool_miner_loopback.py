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
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[0.0000015]}\n'
                b'{"id":null,"method":"mining.notify","params":["mock-job",'
                b'"0000000000000000000000000000000000000000000000000000000000000000",'
                b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",true]}\n'
            )

            for _ in range(2):
                submit = read_line(peer)
                assert submit["method"] == "mining.submit"
                submissions.append(submit)
                send_line(peer, {"id": submit["id"], "result": True, "error": None})
    except BaseException as exc:  # Preserve server-thread assertions for the caller.
        errors.append(exc)
    finally:
        listener.close()


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_live_pool_miner_loopback.py LIVE_MINER", file=sys.stderr)
        return 2
    binary = str(Path(sys.argv[1]).resolve())
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
    summary = next((item for item in events if item.get("event") == "summary"), None)
    if [item["nonce"] for item in candidates] != ["00000000", "00000001"]:
        raise AssertionError(f"expected two distinct sequential nonces: {candidates}")
    if len(submissions) != 2 or summary is None or summary.get("accepted") != 2:
        raise AssertionError(f"missing bounded share accounting: {summary}; submits={submissions}")
    if any(item["params"][0] != "mock-wallet.worker" for item in submissions):
        raise AssertionError("submit login did not preserve the authorized worker identity")
    print("PASS: opt-in live-mode loopback handshake, continuous nonce advance, bounded replies, and JSON events")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
