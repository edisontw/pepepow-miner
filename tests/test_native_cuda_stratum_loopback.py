"""Run a strict physical-GPU CUDA search through a local Stratum mock.

This test is intentionally opt-in. Unlike the product contract loopbacks, it
does not substitute a fake CUDA engine or skip when a device is unavailable.
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path


def read_line(peer: socket.socket) -> dict:
    data = bytearray()
    while not data.endswith(b"\n"):
        part = peer.recv(1)
        if not part:
            raise AssertionError("miner closed before completing the mock session")
        data.extend(part)
        if len(data) > 65536:
            raise AssertionError("miner sent an oversized Stratum frame")
    return json.loads(data)


def send_line(peer: socket.socket, value: dict) -> None:
    peer.sendall(json.dumps(value, separators=(",", ":")).encode("ascii") + b"\n")


def require_cuda_device(engine: Path, oracle: Path, env: dict[str, str]) -> dict:
    header = bytes(80)
    target = bytes(32)
    with tempfile.TemporaryDirectory(prefix="pepepow-cuda-device-required-") as directory:
        cancel_file = Path(directory) / "cancel"
        result = subprocess.run(
            [
                str(engine), "--header-hex", header.hex(), "--nonce-start", "0",
                "--nonce-count", "1", "--target-hex", target.hex(),
                "--batch-size", "1", "--timeout-ms", "10000",
                "--cpu-oracle", str(oracle), "--cancel-file", str(cancel_file),
            ],
            capture_output=True,
            text=True,
            timeout=15,
            env=env,
        )
    if result.returncode != 0:
        raise AssertionError(
            f"physical CUDA device initialization failed ({result.returncode}): "
            f"{result.stdout}\n{result.stderr}"
        )
    try:
        report = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise AssertionError(f"CUDA engine did not emit one JSON result: {result.stdout}") from exc
    device = report.get("device") or {}
    if (report.get("status") != "ok" or report.get("processed_count") != 1 or
            not str(device.get("name", "")).startswith("NVIDIA")):
        raise AssertionError(f"CUDA engine did not prove a live NVIDIA execution: {report}")
    return report


def serve(listener: socket.socket, submissions: list[dict], errors: list[str]) -> None:
    try:
        listener.settimeout(25)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(25)
            subscribe = read_line(peer)
            assert subscribe.get("method") == "mining.subscribe", subscribe
            send_line(peer, {
                "id": subscribe["id"],
                "result": [[
                    ["mining.set_difficulty", "loopback"],
                    ["mining.notify", "loopback"],
                ], "aabbccdd", 4],
                "error": None,
            })

            authorize = read_line(peer)
            assert authorize.get("method") == "mining.authorize", authorize
            send_line(peer, {"id": authorize["id"], "result": True, "error": None})
            peer.sendall(
                b'{"id":null,"method":"mining.set_difficulty","params":[0.0000015]}\n'
                b'{"id":null,"method":"mining.notify","params":["gpu-loopback",'
                b'"0000000000000000000000000000000000000000000000000000000000000000",'
                b'"01000000","ffffffff",[],"20000000","1d00ffff","65a0bc00",true]}\n'
            )
            submit = read_line(peer)
            assert submit.get("method") == "mining.submit", submit
            submissions.append(submit)
            send_line(peer, {"id": submit["id"], "result": True, "error": None})
    except BaseException as exc:
        errors.append(repr(exc))
    finally:
        listener.close()


def nvidia_sample() -> str | None:
    try:
        result = subprocess.run(
            [
                "nvidia-smi", "--query-gpu=name,memory.used,utilization.gpu",
                "--format=csv,noheader,nounits",
            ],
            capture_output=True,
            text=True,
            timeout=3,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def run_loopback(miner: Path, engine: Path, oracle: Path,
                 env: dict[str, str]) -> tuple[dict, list[str], dict, list[dict], dict]:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    submissions: list[dict] = []
    server_errors: list[str] = []
    server_thread = threading.Thread(
        target=serve, args=(listener, submissions, server_errors), daemon=True
    )
    server_thread.start()
    command = [
        str(miner), "--mine", "--endpoint", f"stratum+tcp://127.0.0.1:{port}",
        "--login", "mock-wallet.worker", "--extranonce2", "00000000",
        "--mode", "POOL", "--threads", "1", "--responses", "1",
        "--max-runtime-seconds", "30", "--submit-interval-ms", "100",
        "--device", "cuda", "--cuda-engine", str(engine),
        "--cpu-oracle", str(oracle), "--cuda-batch-size", "128",
    ]
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env
    )
    samples: list[str] = []
    deadline = time.monotonic() + 35
    try:
        while process.poll() is None and time.monotonic() < deadline:
            sample = nvidia_sample()
            if sample:
                samples.append(sample)
            time.sleep(0.15)
        if process.poll() is None:
            process.kill()
            raise AssertionError("CUDA Stratum loopback exceeded its 35-second bound")
        stdout, stderr = process.communicate(timeout=3)
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()
        server_thread.join(timeout=27)
        if server_thread.is_alive():
            listener.close()
            server_thread.join(timeout=2)
    if server_thread.is_alive():
        raise AssertionError("local Stratum mock server did not stop")
    if server_errors:
        raise AssertionError("local Stratum mock failed: " + "; ".join(server_errors))
    if process.returncode != 0:
        raise AssertionError(f"miner failed ({process.returncode}):\n{stdout}\n{stderr}")
    events = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
    candidates = [event for event in events if event.get("event") == "candidate"]
    accepted = [event for event in events if event.get("event") == "share_accepted"]
    summary = next((event for event in events if event.get("event") == "summary"), None)
    if len(candidates) != 1 or len(submissions) != 1 or len(accepted) != 1:
        raise AssertionError(
            f"expected one candidate, submit, and mock acceptance: {events}; "
            f"submissions={len(submissions)}"
        )
    candidate = candidates[0]
    nonce = int(candidate["nonce"], 16)
    header = bytes.fromhex(candidate["header_hex"])
    digest = bytes.fromhex(candidate["digest_raw"])
    if (len(header) != 80 or len(digest) != 32 or
            int.from_bytes(header[76:80], "little") != nonce):
        raise AssertionError("candidate is missing a complete header/digest or correct nonce bytes")
    if len(submissions[0].get("params", [])) != 5 or \
            submissions[0]["params"][4].lower() != candidate["nonce"].lower():
        raise AssertionError("Stratum submit nonce differs from the verified candidate")
    return candidate, samples, summary or {}, events, submissions[0]


def independent_core_recheck(oracle: Path, candidate: dict, env: dict[str, str]) -> None:
    nonce = int(candidate["nonce"], 16)
    with tempfile.TemporaryDirectory(prefix="pepepow-cuda-loopback-oracle-") as directory:
        output = Path(directory) / "digest.bin"
        result = subprocess.run(
            [
                str(oracle), "--header-hex", candidate["header_hex"],
                "--nonce-start", str(nonce), "--nonce-count", "1",
                "--output", str(output),
            ],
            capture_output=True,
            text=True,
            timeout=10,
            env=env,
        )
        if result.returncode != 0:
            raise AssertionError("independent pinned-Core CPU recheck failed: " + result.stderr)
        transcript = output.read_bytes()
    if (transcript[:8] != b"PPWDIG1\0" or
            int.from_bytes(transcript[8:12], "little") != 1 or
            transcript[12:] != bytes.fromhex(candidate["digest_raw"])):
        raise AssertionError("independent Core digest differs from all 32 candidate bytes")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--miner", required=True, type=Path)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--cpu-oracle", required=True, type=Path)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    args = parser.parse_args()
    for label, path in (("miner", args.miner), ("CUDA engine", args.engine),
                        ("CPU oracle", args.cpu_oracle)):
        if not path.is_file():
            parser.error(f"{label} executable does not exist: {path}")

    env = os.environ.copy()
    env["PEPEPOW_CUDA_REQUIRE_DEVICE"] = "1"
    cuda_root = Path(os.environ.get(
        "CUDA_PATH", r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.3"
    ))
    prepend = [
        str(cuda_root / "bin"),
        r"D:\Dev\pepepow-cuda-toolkit-13.3.1-overlay\bin",
        r"D:\Dev\msys64\ucrt64\bin",
    ]
    env["PATH"] = os.pathsep.join(prepend + [env.get("PATH", "")])

    device = require_cuda_device(args.engine, args.cpu_oracle, env)
    candidate, samples, summary, events, submit = run_loopback(
        args.miner, args.engine, args.cpu_oracle, env
    )
    independent_core_recheck(args.cpu_oracle, candidate, env)

    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    evidence = {
        "result": "PASS",
        "scope": "native Windows NVIDIA device; local Stratum mock only; no public pool/share",
        "device_preflight": device,
        "nonce_hex": candidate["nonce"],
        "header_hex": candidate["header_hex"],
        "digest_hex": candidate["digest_raw"],
        "miner_cpu_candidate_recheck": "PASS; candidate event is emitted after result validation",
        "independent_pinned_core_recheck": "PASS; full 32-byte digest matches",
        "submit": submit,
        "mock_response": "accepted",
        "summary": {
            key: summary[key]
            for key in (
                "accepted", "rejected", "pool_errors", "stale_candidates",
                "hashes", "elapsed_seconds", "stop_reason",
            )
            if key in summary
        },
        "nvidia_smi_samples": samples,
    }
    (args.evidence_dir / "events.jsonl").write_text(
        "\n".join(json.dumps(event, sort_keys=True) for event in events) + "\n",
        encoding="utf-8",
    )
    (args.evidence_dir / "summary.json").write_text(
        json.dumps(evidence, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({
        "result": evidence["result"],
        "device": device["device"],
        "nonce_hex": candidate["nonce"],
        "digest_hex": candidate["digest_raw"],
        "cpu_recheck": evidence["independent_pinned_core_recheck"],
        "mock_response": evidence["mock_response"],
        "submit": submit,
        "summary": evidence["summary"],
        "nvidia_smi_samples": samples,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
