"""Read-only timing/evidence audit; never opens UART or edits raw results."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time


def audit(directory, tag):
    state = json.loads((directory / f"{tag}-longrun.json").read_text())
    raw_path = directory / "logs" / f"{tag}-longrun-uart.log"
    raw = raw_path.read_text(errors="replace")
    marker = re.search(r"(?m)^HOST_COMMAND .* showinfo -i 60 &\s*$", raw)
    monitored = raw[marker.end():] if marker else ""
    rows = re.findall(
        r"(?m)^\[CPU[01]\]\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+"
        r"(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)%\s*$", monitored)
    faults = sorted(set(re.findall(
        r"ESP-ROM:|Assertion failed|S31SM:M-TRAP|Segmentation fault|"
        r"kasan_report:|kasan_panic:|kasan detected|AddressSanitizer|"
        r"\bERROR\b|get total info fail|program complete!", monitored)))
    samples = state.get("samples", [])
    result = {
        "tag": tag,
        "audit_utc": datetime.now(timezone.utc).isoformat(),
        "raw_script_labels": {key: state.get(key) for key in
                              ("standby", "time_consistency")},
        "raw_script_error": state.get("error"),
        "resource_rows": len(rows),
        "uart_log_age_seconds": time.time() - raw_path.stat().st_mtime,
        "fault_markers_after_monitor_start": faults,
        "sample_count": len(samples),
        "note": "Audit only. No raw PASS label is adopted automatically. "
                "Clock-source discrepancy and uninterrupted monitoring "
                "must be reviewed before formal acceptance.",
    }
    if rows:
        result["resource_extrema"] = {
            "minimum_free": min(int(row[2]) for row in rows),
            "maximum_used": max(int(row[1]) for row in rows),
            "maximum_reported_peak": max(int(row[3]) for row in rows),
            "maximum_cpu_percent": max(float(row[7]) for row in rows),
        }
    if not samples:
        result["timing_review"] = "NO_DATE_SAMPLES"
        return result

    first, last = samples[0], samples[-1]
    board_start = datetime.fromisoformat(first["board_utc"]).replace(
        tzinfo=timezone.utc).timestamp()
    board_last = datetime.fromisoformat(last["board_utc"]).replace(
        tzinfo=timezone.utc).timestamp()
    wall_lower = last["host_before"] - first["host_after"]
    wall_upper = last["host_after"] - first["host_before"]
    monotonic_delta = last["elapsed_seconds"] - first["elapsed_seconds"]
    result["duration_at_last_date_sample"] = {
        "host_realtime_interval_seconds": [wall_lower, wall_upper],
        "board_date_interval_seconds": [board_last - board_start - 1,
                                        board_last - board_start + 1],
        "host_monotonic_seconds": monotonic_delta,
        "monotonic_minus_realtime_midpoint_seconds":
            monotonic_delta - (wall_lower + wall_upper) / 2,
    }
    result["last_time_error_interval_seconds"] = last[
        "error_seconds_interval"]
    result["sampled_duration_at_least_12h_on_both_axes"] = (
        wall_lower >= 43200 and board_last - board_start - 1 >= 43200)
    result["sampled_duration_at_least_24h_on_both_axes"] = (
        wall_lower >= 86400 and board_last - board_start - 1 >= 86400)
    result["last_time_error_within_2s"] = max(
        abs(v) for v in last["error_seconds_interval"]) <= 2
    result["timing_review"] = "REQUIRED"
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default="xts864")
    parser.add_argument("--directory", type=Path,
                        default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    if not re.fullmatch(r"xts\d+[a-z]?", args.tag):
        parser.error("expected a numbered xTS evidence tag")
    print(json.dumps(audit(args.directory, args.tag), indent=2))
