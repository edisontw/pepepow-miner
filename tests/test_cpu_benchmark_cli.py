import json
import os
import signal
import subprocess
import sys
import time


def fail(message):
    print(f"FAIL: {message}", file=sys.stderr)
    return 1


def main():
    if len(sys.argv) != 2:
        return fail("expected the benchmark executable path")
    executable = sys.argv[1]

    help_result = subprocess.run([executable, "--help"], capture_output=True, text=True, timeout=5)
    if help_result.returncode != 0 or "--threads N|all" not in help_result.stdout:
        return fail("--help did not describe the benchmark options")

    invalid = subprocess.run(
        [executable, "--threads", "0"], capture_output=True, text=True, timeout=5
    )
    if invalid.returncode != 2 or "1..256" not in invalid.stderr:
        return fail("invalid thread count did not produce a clear usage error")

    too_long = subprocess.run(
        [executable, "--seconds", "1801"], capture_output=True, text=True, timeout=5
    )
    if too_long.returncode != 2 or "1..1800" not in too_long.stderr:
        return fail("benchmark duration above the 30-minute resource cap was not rejected")

    measured = subprocess.run(
        [executable, "--benchmark", "--threads", "all", "--seconds", "1"],
        capture_output=True,
        text=True,
        timeout=10,
    )
    if measured.returncode != 0:
        return fail(f"benchmark exited {measured.returncode}: {measured.stderr}")
    try:
        report = json.loads(measured.stdout)
    except json.JSONDecodeError as error:
        return fail(f"benchmark did not emit one JSON report: {error}")

    required = {
        "status",
        "mode",
        "os",
        "compiler",
        "threads",
        "logical_cpus",
        "requested_seconds",
        "elapsed_seconds",
        "hashes",
        "hashrate_hps",
        "process_cpu_seconds",
        "cpu_utilization_pct_of_logical_capacity",
        "memory_current_bytes",
        "memory_peak_bytes",
        "startup_ms",
        "stop_reason",
        "stop_latency_ms",
        "nonce_batches_complete",
        "final_batch_hashes",
        "nonce_batches_disjoint",
        "header_hex",
    }
    if not required.issubset(report):
        return fail(f"benchmark report omitted fields: {sorted(required - set(report))}")
    if report["status"] != "ok" or report["mode"] != "offline":
        return fail("benchmark did not identify itself as an offline run")
    if report["threads"] != report["logical_cpus"] or report["threads"] < 1:
        return fail("--threads all did not select the reported logical CPU count")
    if report["hashes"] <= 0 or report["elapsed_seconds"] <= 0 or report["hashrate_hps"] <= 0:
        return fail("benchmark metrics must be measured positive values")
    measured_rate = report["hashes"] / report["elapsed_seconds"]
    if abs(measured_rate - report["hashrate_hps"]) > max(1.0, measured_rate * 1e-8):
        return fail("reported H/s does not match the measured hash count and time")
    if report["stop_reason"] != "duration" or report["stop_latency_ms"] is None:
        return fail("duration stop and cooperative stop latency were not recorded")
    if report["nonce_batches_disjoint"] is not True:
        return fail("benchmark did not assert disjoint nonce batches")
    current = report["memory_current_bytes"]
    peak = report["memory_peak_bytes"]
    if current is not None and peak is not None and peak < current:
        return fail("peak working set was smaller than the current working set")
    if len(report["header_hex"]) != 160:
        return fail("benchmark header was not a complete fixed 80-byte value")
    if report["hashes"] != (
        report["nonce_batches_complete"] * 10000 + report["final_batch_hashes"]
    ):
        return fail("completed and partial nonce batches do not account for every measured hash")

    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    interrupted_run = subprocess.Popen(
        [executable, "--threads", "2", "--seconds", "1800"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=creationflags,
    )
    time.sleep(0.25)
    stop_requested_at = time.monotonic()
    if os.name == "nt":
        interrupted_run.send_signal(signal.CTRL_BREAK_EVENT)
    else:
        interrupted_run.send_signal(signal.SIGINT)
    try:
        interrupted_stdout, interrupted_stderr = interrupted_run.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        interrupted_run.kill()
        interrupted_run.communicate()
        return fail("benchmark did not stop promptly after an interrupt")
    external_stop_latency = time.monotonic() - stop_requested_at
    if interrupted_run.returncode != 0:
        return fail(
            f"interrupted benchmark exited {interrupted_run.returncode}: {interrupted_stderr}"
        )
    try:
        interrupted_report = json.loads(interrupted_stdout)
    except json.JSONDecodeError as error:
        return fail(f"interrupted benchmark did not emit JSON: {error}")
    if interrupted_report.get("stop_reason") != "interrupt" or external_stop_latency >= 2:
        return fail("normal interrupt did not stop workers and report its stop reason")

    print(
        "PASS: benchmark CLI help/errors, logical-CPU selection, measured metrics, "
        f"timed stop, and interrupt stop ({external_stop_latency * 1000:.2f} ms)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
