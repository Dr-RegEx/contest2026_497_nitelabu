#!/usr/bin/env python3
#
# SPDX-License-Identifier: Apache-2.0
#
"""Run the ESP32-S31 NuttX USB console/NSH bring-up smoke test."""

import argparse
import os
import re
import sys
import time
from typing import Optional

import serial
from serial.tools import list_ports


PROMPT = b"nsh> "
UART_BYTE_DELAY = 0.010
NSH_COMMAND_MAX = 63
ESPRESSIF_USB_VID = 0x303A
USB_SERIAL_JTAG_PID = 0x1001
BOOT_FAILURE_MARKERS = (
    # PANIC() prints "Assertion failed panic:", so neither a lowercase
    # "assertion" nor an uppercase "PANIC" would match a real one.
    b"Assertion failed",
    b"invalid header",
    b"CPU1 bring-up failed",
    b"SHA-256 comparison failed",
    b"S31SM:M-TRAP",
    b"TEST:FAIL stage=smp-scheduler",
)
CPU1_BOOT_MARKERS = {
    "smp": b"TEST:PASS stage=smp-scheduler rounds=64",
}
E1_SSID_ENV = "S31_E1_WIFI_SSID"
E1_PASSWORD_ENV = "S31_E1_WIFI_PASSWORD"
CONSOLE_SLOW_AFTER = 0.0
CONSOLE_RECOVERY_TIMEOUT = 0.0


def ping_passed(output: str, expected: int) -> bool:
    """Require exactly one complete ping summary with all replies received."""

    summaries = re.findall(
        r"(?m)^\s*(\d+) packets transmitted, (\d+) received, "
        r"(\d+)% packet loss(?:,|\s*$)", output)
    return (expected > 0 and len(summaries) == 1 and
            tuple(map(int, summaries[0])) == (expected, expected, 0))


def busy_count_ladder(value: str) -> tuple[int, ...]:
    """Parse a strictly increasing comma-separated busy-count ladder."""

    try:
        counts = tuple(int(item) for item in value.split(","))
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "busy counts must be comma-separated integers") from error

    if (not counts or any(count < 2 or count > 256 for count in counts) or
            any(left >= right for left, right in zip(counts, counts[1:]))):
        raise argparse.ArgumentTypeError(
            "busy counts must increase strictly from 2 through 256")
    return counts


def collect_until_prompt(port: serial.Serial, timeout: float,
                         diagnostic_label: str = "DIRECT") -> bytes:
    """Collect UART output until the NSH prompt appears.

    The timeout is an idle timeout, so a command that keeps producing output
    is not cut off just because the whole of it does not fit in one window;
    ps on an SMP build is long enough to hit that.  A separate absolute cap
    still bounds a command that streams forever.
    """

    now = time.monotonic()
    deadline = now + timeout
    hard_deadline = now + max(4.0 * timeout, timeout + 10.0)
    last_activity = now
    slow_started = None
    recovery_deadline = None
    output = bytearray()
    while True:
        now = time.monotonic()
        if (CONSOLE_SLOW_AFTER > 0.0 and slow_started is None and
                now - last_activity >= CONSOLE_SLOW_AFTER):
            slow_started = now
            recovery_deadline = now + CONSOLE_RECOVERY_TIMEOUT
            deadline = max(deadline, recovery_deadline)
            hard_deadline = max(hard_deadline, recovery_deadline)
            print("CONSOLE_SLOW="
                  f"label={diagnostic_label} "
                  f"idle_seconds={now - last_activity:.3f}")

        if now >= min(deadline, hard_deadline):
            break

        chunk = port.read(port.in_waiting or 1)
        if chunk:
            output.extend(chunk)
            last_activity = time.monotonic()
            deadline = last_activity + timeout
            if recovery_deadline is not None:
                deadline = max(deadline, recovery_deadline)

        if PROMPT in output:
            time.sleep(0.08)
            output.extend(port.read(port.in_waiting))
            if slow_started is not None:
                print("CONSOLE_RECOVERED="
                      f"label={diagnostic_label} "
                      f"seconds={time.monotonic() - slow_started:.3f}")
            return bytes(output)

    raise TimeoutError(output.decode("utf-8", errors="replace"))


def collect_until_marker(port: serial.Serial, marker: bytes,
                         timeout: float, prefix: bytes = b"") -> bytes:
    """Collect UART output until a specific marker appears."""

    deadline = time.monotonic() + timeout
    output = bytearray(prefix)
    while time.monotonic() < deadline:
        output.extend(port.read(port.in_waiting or 1))
        if marker in output:
            return bytes(output)

    raise TimeoutError(output.decode("utf-8", errors="replace"))


def hard_reset(port: serial.Serial) -> None:
    """Pulse the board reset line while leaving the download strap inactive."""

    port.reset_input_buffer()
    port.dtr = False
    port.rts = True
    time.sleep(0.20)
    port.rts = False


def find_usb_console() -> str:
    """Return the only attached Espressif USB Serial/JTAG console."""

    devices = [
        info.device for info in list_ports.comports()
        if info.vid == ESPRESSIF_USB_VID and
        info.pid == USB_SERIAL_JTAG_PID
    ]
    if len(devices) != 1:
        rendered = ", ".join(devices) if devices else "none"
        raise ValueError(
            "expected one Espressif USB Serial/JTAG device, found "
            f"{rendered}; specify --port explicitly")
    return devices[0]


def redact_text(value: str, redactions: tuple[str, ...]) -> str:
    """Remove secrets from a command or captured UART transcript."""

    for secret in redactions:
        if secret:
            value = value.replace(secret, "<redacted>")
    return value


def check_command_length(command: str) -> None:
    """Reject a command the target console would silently truncate.

    The NSH line reader stops accepting a command line past 63 characters and
    runs what it has, without any diagnostic:  a 68-character wget line was
    observed reaching the server as a request for a truncated path.  Failing
    here names the real problem instead of leaving a probe to report that the
    process it expected never appeared.
    """

    if len(command) > NSH_COMMAND_MAX:
        raise ValueError(
            f"NSH command is {len(command)} characters, the console truncates "
            f"past {NSH_COMMAND_MAX}: {command}")


def nsh_escape(value: str) -> str:
    """Escape one NSH argument without exposing it through an env command."""

    if not value or any(char in value for char in "\0\r\n"):
        raise ValueError("NSH arguments must be non-empty single-line strings")

    special = " \t\\'\"`$#;|<>&"
    return "".join(f"\\{char}" if char in special else char
                   for char in value)


def run_command(port: serial.Serial, command: str,
                timeout: float = 3.0,
                redactions: tuple[str, ...] = (),
                reset_input: bool = True) -> str:
    """Run one NSH command and return its output through the next prompt."""

    check_command_length(command)

    if reset_input:
        port.reset_input_buffer()
    # Pace host bytes so this smoke test remains valid for both the USB packet
    # transport and the UART fallback device.  Do not call Serial.flush()
    # here: on macOS it maps to an unbounded tcdrain(), which can hang the
    # host test forever after a target-side fatal trap stops consuming USB.

    for byte in command.encode("utf-8") + b"\r\n":
        port.write(bytes((byte,)))
        time.sleep(UART_BYTE_DELAY)

    visible_command = redact_text(command, redactions)
    output = collect_until_prompt(port, timeout, visible_command)
    rendered = redact_text(output.decode("utf-8", errors="replace"),
                           redactions)
    print(f"--- CMD: {visible_command} ---")
    print(rendered.strip())
    return rendered


def run_command_until_output_line(port: serial.Serial, command: str,
                                  marker: str,
                                  timeout: float) -> str:
    """Run a command and wait for its standalone output marker.

    Unlike ``run_command``, this ignores an asynchronous stale prompt that
    arrives while the paced command bytes are still being transmitted.  That
    matters for the busy U-mode preemption probe, where completion of the
    preceding background launch can race the next command echo.  A short
    marker also measures when the shell ran, without folding slow console TX
    completion into scheduler latency.
    """

    check_command_length(command)

    port.reset_input_buffer()
    for byte in command.encode("utf-8") + b"\r\n":
        port.write(bytes((byte,)))
        time.sleep(UART_BYTE_DELAY)

    pattern = re.compile(
        rb"(?:^|\r?\n)" + re.escape(marker.encode("utf-8")) + rb"\r?\n")
    deadline = time.monotonic() + timeout
    output = bytearray()
    while time.monotonic() < deadline:
        output.extend(port.read(port.in_waiting or 1))
        match = pattern.search(output)
        if match is not None:
            rendered = output.decode("utf-8", errors="replace")
            print(f"--- CMD: {command} ---")
            print(rendered.strip())
            return rendered

    raise TimeoutError(output.decode("utf-8", errors="replace"))


def sv32_fault_probe_started(output: str, mode: str) -> bool:
    """Confirm that the requested fault probe reached U-mode execution.

    On SMP, the U-mode marker and the other CPU's fault diagnostics can be
    interleaved at byte granularity by the native USB console.  The command
    echo and verified ELF load remain intact even when the marker itself is
    split.  The caller additionally requires the mode-specific exception,
    process termination, and a live-shell recovery marker, so a PID suffix is
    redundant and must not be used as a serial framing assumption.
    """

    return (f"/system/bin/sv32test {mode}" in output and
            "APPVERIFY:PASS /system/bin/sv32test" in output)


def synchronize_time(port: serial.Serial, timeout: float = 45.0) -> bool:
    """Start the NTP daemon and wait until X.509 date checks are meaningful."""

    initial = run_command(port, "date")
    # CONFIG_BUILD_KERNEL runs the configured one-shot client in the calling
    # process.  It collects multiple samples before returning, so bound the
    # foreground command with the full synchronization window rather than a
    # short shell-command timeout.

    started = run_command(port, "ntpcstart", timeout=timeout)
    deadline = time.monotonic() + timeout
    latest = initial

    while time.monotonic() < deadline:
        latest = run_command(port, "date")
        match = re.search(r"\b(20\d{2})\b", latest)
        if match is not None and int(match.group(1)) >= 2025:
            print("E1_TIME_SYNC=PASS")
            return "failed" not in started.lower()
        time.sleep(1.0)

    print("E1_TIME_SYNC=FAIL")
    return False


def run_foreground_sleep(port: serial.Serial) -> tuple[str, float]:
    """Run foreground sleep and return its output and wall-clock duration."""

    port.reset_input_buffer()
    start = time.monotonic()
    for byte in b"sleep 2\r\n":
        port.write(bytes((byte,)))
        time.sleep(UART_BYTE_DELAY)

    output = collect_until_prompt(port, 5.0)
    elapsed = time.monotonic() - start
    rendered = output.decode("utf-8", errors="replace")
    print("--- CMD: sleep 2 ---")
    print(rendered.strip())
    print(f"FOREGROUND_SLEEP_SECONDS={elapsed:.3f}")
    return rendered, elapsed


def run_busy_preemption(port: serial.Serial) -> tuple[str, str, float]:
    """Start a syscall-free U task and measure shell scheduling latency."""

    marker = b"SV32TEST:BUSY BEGIN"
    busy_output = run_command(port, "/system/bin/sv32test busy &",
                              timeout=5.0)
    if marker not in busy_output.encode("utf-8"):
        combined = collect_until_marker(
            port, marker, 2.0, prefix=busy_output.encode("utf-8"))
        rendered = combined.decode("utf-8", errors="replace")
        print("--- U-MODE BUSY START ---")
        print(rendered.strip())
        busy_output = rendered

    start = time.monotonic()
    probe = run_command_until_output_line(
        port, "echo UOK", "UOK", timeout=2.0)
    elapsed = time.monotonic() - start
    print(f"UMODE_PREEMPT_SECONDS={elapsed:.3f}")
    return busy_output, probe, elapsed


def busy_tasks(output: str) -> dict[int, Optional[int]]:
    """Extract ordinary busy task PIDs and currently reported CPUs."""

    tasks: dict[int, Optional[int]] = {}
    for line in output.splitlines():
        fields = line.split()
        command = fields[-1].rsplit("/", 1)[-1]
        if len(fields) < 5 or command != "busy":
            continue

        try:
            tid = int(fields[0])
            pid = int(fields[1])
        except ValueError:
            continue

        if tid == pid:
            tasks[pid] = int(fields[3]) if fields[3] in ("0", "1") else None

    return tasks


def start_auto_busy(port: serial.Serial, expected_rr_ms: Optional[int] = None,
                    existing_pids: Optional[set[int]] = None
                    ) -> tuple[int, int, str]:
    """Start one unpinned busy App and return its PID and observed CPU."""

    marker = b"BUSY:START"
    output = run_command(port, "/system/bin/busy &", timeout=8.0)
    encoded = output.encode("utf-8")
    if marker not in encoded:
        encoded = collect_until_marker(port, marker, 2.0, prefix=encoded)
        output = encoded.decode("utf-8", errors="replace")
        print("--- BUSY START MARKER ---")
        print(output.strip())

    match = re.search(
        r"BUSY:START\s+pid=?(\d+)\s*mode=auto\s*cpu=(\d+)\s*"
        r"[^\r\n]*rr-ms=(\d+)", output)
    if match is None:
        # Native USB/JTAG can lose a short output fragment while both cores
        # are saturated.  The App may still have launched successfully, so
        # recover its PID from the scheduler rather than leaving an orphaned
        # busy task running merely because its one-line marker was damaged.

        for _ in range(3):
            tasks = run_command(port, "ps", timeout=4.0,
                                reset_input=False)
            candidates = [
                (pid, cpu if cpu is not None else 0)
                for pid, cpu in busy_tasks(tasks).items()
                if existing_pids is None or pid not in existing_pids
            ]

            if candidates:
                pid, cpu = max(candidates)
                print(f"BUSY_START_RECOVERED_FROM_PS={pid},{cpu}")
                return pid, cpu, output + tasks

        raise ValueError("unable to parse or recover BUSY:START marker")

    rr_ms = int(match.group(3))
    if expected_rr_ms is not None and rr_ms != expected_rr_ms:
        raise ValueError(
            f"busy reported RR={rr_ms} ms, expected {expected_rr_ms} ms")

    return int(match.group(1)), int(match.group(2)), output


def run_smp_busy_suite(port: serial.Serial, rounds: int,
                       expected_rr_ms: Optional[int]) -> bool:
    """Verify two ordinary background busy Apps saturate the SMP system."""

    passed = True
    observed_cpus: set[int] = set()
    for round_index in range(1, rounds + 1):
        pid0, cpu0, _ = start_auto_busy(port, expected_rr_ms)
        pid1, cpu1, _ = start_auto_busy(port, expected_rr_ms, {pid0})
        observed_cpus.update((cpu0, cpu1))

        # CONFIG_SCHED_CPULOAD_TIMECONSTANT=2 reports a rolling window.  Wait
        # beyond one complete window so preceding idle time cannot turn two
        # fully running CPUs into a transient ~50% measurement.

        time.sleep(5.0)

        load_output = run_command(port, "cat /proc/cpuload", timeout=8.0)
        load_match = re.search(r"(?m)^\s*([0-9]+(?:\.[0-9]+)?)%\s*$",
                               load_output)
        load = float(load_match.group(1)) if load_match is not None else -1.0
        tasks = run_command(port, "ps", timeout=4.0)
        pid0_row = re.search(rf"(?m)^\s*{pid0}\s+{pid0}\s+", tasks)
        pid1_row = re.search(rf"(?m)^\s*{pid1}\s+{pid1}\s+", tasks)

        launch_ok = pid0 != pid1 and cpu0 in (0, 1) and cpu1 in (0, 1)
        load_ok = load >= 95.0
        tasks_ok = (pid0_row is not None and pid1_row is not None and
                    "busy" in tasks)
        round_ok = launch_ok and load_ok and tasks_ok
        passed = passed and round_ok
        print(f"SMP_BUSY_ROUND_{round_index}_PIDS={pid0},{pid1}")
        print(f"SMP_BUSY_ROUND_{round_index}_START_CPUS={cpu0},{cpu1}")
        print(f"SMP_BUSY_ROUND_{round_index}_LOAD={load:.1f}")
        print(f"SMP_BUSY_ROUND_{round_index}_RESULT="
              f"{'PASS' if round_ok else 'FAIL'}")

        kill0 = run_command(port, f"kill {pid0}", timeout=3.0)
        time.sleep(0.3)
        kill1 = run_command(port, f"kill {pid1}", timeout=3.0,
                            reset_input=False)
        time.sleep(0.5)
        after_kill = run_command(port, "ps", timeout=4.0,
                                 reset_input=False)
        pid0_alive = re.search(
            rf"(?m)^\s*{pid0}\s+{pid0}\s+", after_kill)
        pid1_alive = re.search(
            rf"(?m)^\s*{pid1}\s+{pid1}\s+", after_kill)
        kill_output = kill0 + kill1 + after_kill
        kill_fault_free = not any(marker in kill_output for marker in (
            "riscv_exception:", "Segmentation fault", "PANIC"))
        kill_ok = (pid0_alive is None and pid1_alive is None and
                   kill_fault_free)
        passed = passed and kill_ok
        print(f"SMP_BUSY_ROUND_{round_index}_KILL_RESULT="
              f"{'PASS' if kill_ok else 'FAIL'}")

    print("SMP_BUSY_OBSERVED_START_CPUS=" +
          ",".join(str(cpu) for cpu in sorted(observed_cpus)))
    print(f"SMP_BUSY_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def parse_tlbshoot_stats(output: str) -> Optional[dict]:
    """Parse /dev/tlbshoot counters emitted by the S31 shootdown port."""

    match = re.search(
        r"TLBSHOOT: send=(\d+) ack=(\d+) timeout=(\d+) "
        r"range=(\d+) global=(\d+)",
        output)
    if match is None:
        return None

    return {
        "send": int(match.group(1)),
        "ack": int(match.group(2)),
        "timeout": int(match.group(3)),
        "range": int(match.group(4)),
        "global": int(match.group(5)),
    }


def parse_s31stat(output: str) -> Optional[dict]:
    """Parse /dev/s31stat monitor replay and address-environment counters.

    Legacy images also emitted release masks and Wi-Fi rearm-poller counters.
    Keep accepting those fields so one host tool can compare old and new
    images, but require only the per-CPU monitor lines from the current ABI.
    """

    stats: dict = {"cpu": {}}

    for match in re.finditer(
            r"MONIRQ: cpu=(\d+) defer=(\d+) replay=(\d+) ack=(\d+) "
            r"drop=(\d+)(?: release=0x([0-9a-f]+))? "
            r"pending=0x([0-9a-f]+)",
            output):
        cpu_stats = {
            "defer": int(match.group(2)),
            "replay": int(match.group(3)),
            "ack": int(match.group(4)),
            "drop": int(match.group(5)),
            "pending": int(match.group(7), 16),
        }
        if match.group(6):
            cpu_stats["release"] = int(match.group(6), 16)
        stats["cpu"][int(match.group(1))] = cpu_stats

    for match in re.finditer(
            r"MONIRQ: cpu=(\d+).* sreplay=(\d+)", output):
        cpu = int(match.group(1))
        if cpu in stats["cpu"]:
            stats["cpu"][cpu]["sreplay"] = int(match.group(2))

    if not stats["cpu"]:
        return None

    match = re.search(
        r"REARM: wakeups=(\d+) polls=(\d+) drained=(\d+) found=(\d+) "
        r"remote=(\d+) maxrun=(\d+)",
        output)
    if match is not None:
        stats["rearm"] = {
            "wakeups": int(match.group(1)),
            "polls": int(match.group(2)),
            "drained": int(match.group(3)),
            "found": int(match.group(4)),
            "remote": int(match.group(5)),
            "maxrun": int(match.group(6)),
        }

    match = re.search(
        r"ADDRENV: created=(\d+) fail=(\d+) destroyed=(\d+) live=(\d+) "
        r"livemax=(\d+) pages=(\d+) pgt=(\d+)",
        output)
    if match is not None:
        stats["addrenv"] = {
            "created": int(match.group(1)),
            "fail": int(match.group(2)),
            "destroyed": int(match.group(3)),
            "live": int(match.group(4)),
            "live_max": int(match.group(5)),
            "pages_freed": int(match.group(6)),
            "pgt_freed": int(match.group(7)),
        }

    return stats


def report_s31stat(port: serial.Serial, label: str) -> Optional[dict]:
    """Sample /dev/s31stat and emit it as parseable baseline data.

    This is observation only.  The deferral rate and the worst-case replay
    latency have no established bound yet, so they are reported rather than
    asserted; only the address environment balance below is an invariant.
    """

    # Saturating the receive path slows the console, so this read needs more
    # headroom than an idle-system command.

    stats = parse_s31stat(
        run_command(port, "cat /dev/s31stat", timeout=12.0))
    if stats is None:
        print(f"S31STAT_{label}_PARSE=FAIL")
        return None

    for cpu, values in sorted(stats["cpu"].items()):
        for key in ("defer", "replay", "ack", "drop", "sreplay"):
            if key not in values:
                continue
            print(f"S31STAT_{label}_CPU{cpu}_{key.upper()}={values[key]}")
        if "release" in values:
            print(f"S31STAT_{label}_CPU{cpu}_RELEASE="
                  f"0x{values['release']:x}")
        print(f"S31STAT_{label}_CPU{cpu}_PENDING=0x{values['pending']:x}")

    if "rearm" in stats:
        for key, value in stats["rearm"].items():
            print(f"S31STAT_{label}_REARM_{key.upper()}={value}")

    if "addrenv" in stats:
        for key, value in stats["addrenv"].items():
            print(f"S31STAT_{label}_ADDRENV_{key.upper()}={value}")

    return stats


def check_addrenv_balance(stats: Optional[dict], label: str) -> bool:
    """Assert the create/destroy balance the kernel counters must satisfy.

    up_addrenv_create() counts the environment as owned before its first
    allocation and every failure path calls up_addrenv_destroy() exactly once,
    so created - destroyed must equal live at any sampling point.
    """

    if stats is None or "addrenv" not in stats:
        print(f"S31STAT_{label}_ADDRENV_BALANCE=SKIP")
        return True

    addrenv = stats["addrenv"]
    expected = addrenv["created"] - addrenv["destroyed"]
    balanced = expected == addrenv["live"]
    print(f"S31STAT_{label}_ADDRENV_BALANCE="
          f"{'PASS' if balanced else 'FAIL'}")
    return balanced


def check_addrenv_returned(before: Optional[dict], after: Optional[dict],
                           label: str, strict: bool = True) -> bool:
    """Assert every process started between two samples returned its addrenv.

    This replaces inferring the G.2.1b leak from `free` deltas, which cannot
    distinguish a retained address environment from allocator fragmentation.

    Only a boundary where nothing is deliberately left running can be gated.
    A suite that ends with a background task alive reports a positive live
    delta that no counter can tell apart from a leak, and the suite that later
    kills that task reports a negative one, so those boundaries pass
    ``strict=False`` and serve only to localise a leak that the whole-run
    comparison already caught.
    """

    if (before is None or after is None or
            "addrenv" not in before or "addrenv" not in after):
        print(f"S31STAT_{label}_ADDRENV_RETURNED=SKIP")
        return True

    created = after["addrenv"]["created"] - before["addrenv"]["created"]
    destroyed = after["addrenv"]["destroyed"] - before["addrenv"]["destroyed"]
    leaked = after["addrenv"]["live"] - before["addrenv"]["live"]

    print(f"S31STAT_{label}_ADDRENV_CREATED_DELTA={created}")
    print(f"S31STAT_{label}_ADDRENV_DESTROYED_DELTA={destroyed}")
    print(f"S31STAT_{label}_ADDRENV_LIVE_DELTA={leaked}")

    returned = leaked == 0
    if not strict:
        print(f"S31STAT_{label}_ADDRENV_RETURNED="
              f"{'PASS' if returned else 'INFO'}")
        return True

    print(f"S31STAT_{label}_ADDRENV_RETURNED="
          f"{'PASS' if returned else 'FAIL'}")
    return returned


def running_task_pids(output: str) -> set[int]:
    """Return the PIDs of every task leader listed by ps.

    Kernel threads report PID 0 and threads share their task PID, so only rows
    whose TID equals their PID identify something the shell can kill.
    """

    pids = set()
    for line in output.splitlines():
        fields = line.split()
        if (len(fields) >= 2 and fields[0].isdigit() and
                fields[0] == fields[1]):
            pids.add(int(fields[0]))

    return pids


def read_task_pids(port: serial.Serial, attempts: int = 4) -> set[int]:
    """Read ps until it returns a plausible task list.

    Saturating the receive path starves the console enough that ps can answer
    with only its header before a stale prompt closes the read.  The system
    always runs several kernel threads, so an implausibly short list means the
    output was truncated rather than that nothing is running.
    """

    for attempt in range(attempts):
        if attempt:
            time.sleep(1.0)

        pids = running_task_pids(run_command(port, "ps", timeout=10.0))
        if len(pids) >= 2:
            return pids

    return set()


def wait_victims_gone(port: serial.Serial, victims: list, label: str,
                      timeout: float = 6.0) -> bool:
    """Wait until none of the killed victims is listed by ps any more.

    A kill only marks the signal pending; the task dies where it next runs.
    Under saturated receive that can take a moment, so the check is bounded
    rather than immediate.
    """

    wanted = {pid for pid in victims if pid is not None}
    if not wanted:
        return True

    deadline = time.monotonic() + timeout
    alive = wanted
    while True:
        pids = read_task_pids(port, attempts=2)
        if not pids:
            # ps gave nothing usable, so this says nothing about the victims.

            print(f"{label}_VICTIM_EXIT=UNKNOWN")
            return True

        alive = wanted & pids
        if not alive or time.monotonic() >= deadline:
            break

        time.sleep(0.5)

    if alive:
        print(f"{label}_VICTIM_EXIT=FAIL pids=" +
              ",".join(str(pid) for pid in sorted(alive)))
        return False

    print(f"{label}_VICTIM_EXIT=PASS")
    return True


def announced_pid(output: str) -> Optional[int]:
    """Return the PID from NSH's "<cmd> [<pid>:<priority>]" launch notice."""

    match = re.search(r"\[(\d+):(\d+)\]", output)
    if match is None:
        return None

    pid = int(match.group(1))
    return pid if pid > 0 else None


def launch_background(port: serial.Serial, command: str,
                      label: str, claim_delay: float = 1.5) -> Optional[int]:
    """Start a background NSH job and report its PID.

    NSH announces the spawned task as "<cmd> [<pid>:<priority>]", which arrives
    on the same line as the launch and therefore survives a console that is
    dropping later output.  Diffing ps is kept as a fallback because it is the
    only option when the notice itself is lost.

    A refused start and an unclaimable PID must not be reported the same way:
    the first is the target running out of a resource, while the second leaves a
    task running that the caller can no longer kill, which later looks like a
    leaked address environment.
    """

    before = read_task_pids(port)
    output = run_command(port, f"{command} &", timeout=10.0)
    refused = "Out of memory" in output

    pid = announced_pid(output)
    if pid is not None:
        print(f"NET_LEAK_LOAD_{label}_PID={pid}")
        return pid

    time.sleep(claim_delay)
    created = sorted(read_task_pids(port) - before)
    if not created:
        print(f"NET_LEAK_LOAD_{label}="
              f"{'ENOMEM' if refused else 'PID_UNKNOWN'}")

        # Report what the target actually said.  ENOMEM, EBUSY and a lost
        # launch notice have different causes, and the errno text is the only
        # thing that separates them without another probe.  The echoed command
        # is not an error, so match on the diagnostic prefixes NSH and the apps
        # use instead of on anything containing the command name.

        error = "NONE"
        for line in output.splitlines():
            stripped = line.strip()
            if stripped.startswith(("wget:", "sh:", "nsh: ")) or \
                    "Out of memory" in stripped or \
                    "resource busy" in stripped:
                error = stripped[:120]
                break

        print(f"NET_LEAK_LOAD_{label}_ERROR={error}")
        return None

    pid = created[-1]
    print(f"NET_LEAK_LOAD_{label}_PID={pid}")
    return pid


def start_net_load(port: serial.Serial, stream_url: str) -> dict[str, int]:
    """Start the background bulk receive load, returning its PID.

    A concurrent ICMP flood was tried as a second generator and rejected: once
    the stream fills the receive window there is no IOB left for ping, so it
    only emits ENETUNREACH on stderr, which corrupts the parsing of every later
    command.  The stream alone already produces back-to-back receive interrupts
    plus transmit interrupts for its acknowledgements, and writes no flash.
    """

    pids = {}
    wget_pid = launch_background(
        port, f"wget -o /dev/null {stream_url}", "WGET")
    if wget_pid is not None:
        pids["WGET"] = wget_pid

    return pids


def stop_net_load(port: serial.Serial, pids: dict[str, int]) -> bool:
    """Terminate the load generators, reporting whether all of them are gone.

    A generator that outlives the probe still owns its address environment, so
    the caller must not read a surviving generator as a leak.
    """

    for pid in pids.values():
        run_command(port, f"kill {pid}", timeout=5.0)
        time.sleep(0.3)

    time.sleep(1.0)
    alive = read_task_pids(port)
    stopped = True
    for label, pid in pids.items():
        gone = pid not in alive
        stopped = stopped and gone
        print(f"NET_LEAK_LOAD_{label}_STOPPED={'PASS' if gone else 'FAIL'}")

    return stopped


def report_net_leak_kmem(port: serial.Serial, label: str) -> None:
    """Sample heap/page capacity when a saturated teardown round fails."""

    try:
        stats = memory_stats(run_command(port, "free", timeout=12.0))
    except ValueError:
        print(f"S31STAT_{label}_KMEM=PARSE_FAIL")
        return

    print(f"S31STAT_{label}_KMEM_FREE={stats['Kmem'][2]}")
    print(f"S31STAT_{label}_KMEM_MAXFREE={stats['Kmem'][4]}")
    print(f"S31STAT_{label}_PAGE_FREE={stats['Page'][2]}")


def report_iobinfo(port: serial.Serial, label: str) -> None:
    """Sample the fixed IOB pool.

    A socket that cannot be created has two very different causes that `free`
    cannot separate: the pool being legitimately held by the saturating load,
    or connections that were orphaned by a teardown defect and never gave
    their buffers back.  Only the pool itself distinguishes them, so record it
    wherever a socket fails rather than inferring capacity from the heap.
    """

    try:
        output = run_command(port, "cat /proc/iobinfo", timeout=12.0)
    except TimeoutError:
        print(f"IOBINFO_{label}=READ_TIMEOUT")
        return

    # The file is a header line followed by one line of four counters.

    lines = output.splitlines()
    for index, line in enumerate(lines[:-1]):
        if line.split()[:4] != ["ntotal", "nfree", "nwait", "nthrottle"]:
            continue

        values = lines[index + 1].split()
        if len(values) >= 4 and all(value.isdigit() for value in values[:4]):
            print(f"IOBINFO_{label}="
                  f"ntotal={values[0]} nfree={values[1]} "
                  f"nwait={values[2]} nthrottle={values[3]}")
            return

    print(f"IOBINFO_{label}=PARSE_FAIL")


def run_net_leak_suite(port: serial.Serial, stream_url: str, rounds: int,
                       victim: str = "busy",
                       ssid: Optional[str] = None,
                       password: Optional[str] = None,
                       victim_claim_delay: float = 1.5,
                       victim_settle: float = 0.5) -> bool:
    """Hammer process exit while the Wi-Fi receive path is saturated.

    With an idle network the G.2.1b address environment leak appears in roughly
    one full run out of four, which is too rare to bisect.  Bulk receive
    traffic multiplies the interrupt density and therefore the number of
    preemption points inside group teardown, so this probe samples the kernel
    counters every round to localise a leak to the round that caused it rather
    than reporting one suite total.

    The victim selects what the rounds tear down: ``busy`` kills two compute
    loops that own nothing but their address environment, while ``stream``
    kills one task blocked in TCP receive, which additionally exercises socket
    teardown on the exit path.

    A stream victim that cannot be created is a failure.  Earlier versions of
    this probe treated that as harmless IOB back-pressure, which masked the
    connection/devif-callback leak fixed in tcp_close_work().
    """

    if ssid is not None and password is not None:
        _, _, _, address = connect_sta(port, ssid, password)
        print(f"NET_LEAK_STA_IPV4={address or 'NONE'}")
        if address is None:
            print("NET_LEAK_SUITE_RESULT=FAIL")
            return False

    pids = start_net_load(port, stream_url)
    load_ok = "WGET" in pids
    print(f"NET_LEAK_LOAD_STARTED={'PASS' if load_ok else 'FAIL'}")
    if not load_ok:
        print("NET_LEAK_SUITE_RESULT=FAIL")
        return False

    leaked_rounds = []
    passed = True
    completed_rounds = 0
    before = report_s31stat(port, "NET_LEAK_ROUND_0")

    # Baseline the pool under load, so a later failure can be compared against
    # it instead of against an idle system.

    report_iobinfo(port, "NET_LEAK_ROUND_0")
    for round_index in range(1, rounds + 1):
        launched = time.monotonic()
        if victim == "stream":
            # Kill a task blocked in TCP receive rather than a compute loop,
            # so the round exercises socket teardown on the exit path.

            victims = [
                launch_background(port, f"wget -o /dev/null {stream_url}",
                                  f"VICTIM_{round_index}",
                                  victim_claim_delay)
            ]
            if victims == [None]:
                # Failing to create a new victim is the externally visible
                # regression caused by an orphaned connection/callback pool.
                # Record capacity for attribution, but never turn it into a
                # successful early exit.

                print(f"NET_LEAK_ROUND_{round_index}_VICTIM=START_FAIL")
                print(f"NET_LEAK_VICTIM_START_FAILED_AT_ROUND={round_index}")
                report_net_leak_kmem(port, f"NET_LEAK_ROUND_{round_index}")
                report_iobinfo(port, f"NET_LEAK_ROUND_{round_index}")
                passed = False
                break
        else:
            pid0, _, _ = start_auto_busy(port)
            victims = [pid0, start_auto_busy(
                port, existing_pids={pid0})[0]]

        time.sleep(victim_settle)

        # Report the measured launch-to-kill delay.  NSH announces the PID, so
        # the configured settle time is the whole delay; when the announcement
        # is lost the ps fallback adds its own, and only a measurement shows
        # which of the two a round actually used.

        print("NET_LEAK_VICTIM_TIMING="
              f"round={round_index} "
              f"kill_delay={time.monotonic() - launched:.3f} "
              f"settle={victim_settle:.3f}")
        for index, pid in enumerate(victims):
            run_command(port, f"kill {pid}", timeout=6.0,
                        reset_input=index == 0)
            time.sleep(0.3)

        time.sleep(0.5)

        # A victim that survives its own kill is invisible to the per-round
        # conservation check: its address environment stays live for every
        # remaining round, so each round's delta is still zero.  Name it here
        # in the round that failed to reap it.

        exited = wait_victims_gone(port, victims,
                                   f"NET_LEAK_ROUND_{round_index}")

        after = report_s31stat(port, f"NET_LEAK_ROUND_{round_index}")
        label = f"NET_LEAK_ROUND_{round_index}"
        report_net_leak_kmem(port, label)
        round_ok = check_addrenv_returned(before, after, label) and exited
        if not round_ok:
            leaked_rounds.append(round_index)

        passed = passed and round_ok
        before = after
        completed_rounds = round_index

    passed = stop_net_load(port, pids) and passed
    print(f"NET_LEAK_COMPLETED_ROUNDS={completed_rounds}")
    print("NET_LEAK_LEAKED_ROUNDS=" +
          (",".join(str(index) for index in leaked_rounds) or "NONE"))
    print(f"NET_LEAK_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def run_tlb_stress_suite(port: serial.Serial, rounds: int, pages: int,
                         mode: str, require_shootdown: bool) -> bool:
    """Exercise same-addrenv heap grow across CPUs and check shootdown stats.

    When ``require_shootdown`` is true (production G.2.1b), the suite expects
    TLBSTRESS:PASS plus non-zero send/ack counters and zero timeouts.  The
    negative A/B profile passes ``require_shootdown=False`` so a reproduced
    failure can be recorded without demanding counters that do not exist.
    """

    before = None
    after = None
    if require_shootdown:
        before = parse_tlbshoot_stats(
            run_command(port, "cat /dev/tlbshoot", timeout=4.0))

    command = (f"/system/bin/tlbstress --mode {mode} "
               f"--rounds {rounds} --pages {pages}")
    output = run_command(port, command, timeout=max(30.0, rounds * 2.0))

    if require_shootdown:
        after = parse_tlbshoot_stats(
            run_command(port, "cat /dev/tlbshoot", timeout=4.0,
                        reset_input=False))

    pass_marker = "TLBSTRESS:PASS" in output
    fail_marker = "TLBSTRESS:FAIL" in output
    fault_free = not any(marker in output for marker in (
        "riscv_exception:", "Segmentation fault", "PANIC", "page fault"))
    app_ok = pass_marker and not fail_marker and fault_free

    shootdown_ok = True
    if require_shootdown:
        if before is None or after is None:
            shootdown_ok = False
        else:
            send_delta = after["send"] - before["send"]
            ack_delta = after["ack"] - before["ack"]
            timeout_delta = after["timeout"] - before["timeout"]
            print(f"TLB_STRESS_SEND_DELTA={send_delta}")
            print(f"TLB_STRESS_ACK_DELTA={ack_delta}")
            print(f"TLB_STRESS_TIMEOUT_DELTA={timeout_delta}")
            shootdown_ok = (send_delta > 0 and ack_delta > 0 and
                            timeout_delta == 0 and
                            after["timeout"] == 0)

    print(f"TLB_STRESS_MODE={mode}")
    print(f"TLB_STRESS_ROUNDS={rounds}")
    print(f"TLB_STRESS_PAGES={pages}")
    print(f"TLB_STRESS_APP_RESULT={'PASS' if app_ok else 'FAIL'}")
    print(f"TLB_STRESS_SHOOTDOWN_RESULT="
          f"{'PASS' if shootdown_ok else 'FAIL'}")
    passed = app_ok and shootdown_ok
    print(f"TLB_STRESS_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def run_smp_busy_scale_suite(port: serial.Serial, counts: tuple[int, ...],
                             expected_rr_ms: Optional[int]) -> bool:
    """Increase the number of unpinned busy Apps and clean each step."""

    initial_tasks = busy_tasks(run_command(port, "ps", timeout=8.0))
    if initial_tasks:
        print("SMP_BUSY_SCALE_PREEXISTING_PIDS=" +
              ",".join(str(pid) for pid in sorted(initial_tasks)))
        print("SMP_BUSY_SCALE_SUITE_RESULT=FAIL")
        return False

    passed = True
    max_stable = 0
    for count in counts:
        before = memory_free_bytes(run_command(port, "free", timeout=5.0))
        pids: set[int] = set()
        start_cpus: set[int] = set()
        launch_ok = True
        launch_start = time.monotonic()

        for _ in range(count):
            try:
                pid, cpu, _ = start_auto_busy(
                    port, expected_rr_ms, pids)
            except (TimeoutError, ValueError) as error:
                launch_ok = False
                rendered = str(error).replace("\r", " ").replace("\n", " ")
                print(f"SMP_BUSY_SCALE_COUNT_{count}_LAUNCH_ERROR="
                      f"{rendered}")
                break

            if pid in pids:
                launch_ok = False
                print(f"SMP_BUSY_SCALE_COUNT_{count}_DUPLICATE_PID={pid}")
                break

            pids.add(pid)
            start_cpus.add(cpu)

        launch_elapsed = time.monotonic() - launch_start
        time.sleep(5.0)

        shell_start = time.monotonic()
        try:
            shell_output = run_command_until_output_line(
                port, "echo SMP_BUSY_SCALE_SHELL_OK",
                "SMP_BUSY_SCALE_SHELL_OK", timeout=3.0)
            shell_elapsed = time.monotonic() - shell_start
            shell_ok = ("SMP_BUSY_SCALE_SHELL_OK" in shell_output and
                        shell_elapsed <= 2.0)
        except TimeoutError as error:
            shell_elapsed = time.monotonic() - shell_start
            shell_ok = False
            print(f"SMP_BUSY_SCALE_COUNT_{count}_SHELL_ERROR="
                  f"{str(error)}")

        load_output = run_command(port, "cat /proc/cpuload", timeout=8.0,
                                  reset_input=False)
        load_match = re.search(r"(?m)^\s*([0-9]+(?:\.[0-9]+)?)%\s*$",
                               load_output)
        load = float(load_match.group(1)) if load_match is not None else -1.0
        tasks_output = run_command(port, "ps", timeout=12.0,
                                   reset_input=False)
        actual = busy_tasks(tasks_output)
        pids.update(actual)
        during = memory_free_bytes(run_command(
            port, "free", timeout=5.0, reset_input=False))

        launch_ok = launch_ok and len(pids) == count
        tasks_ok = len(actual) == count
        load_ok = load >= 95.0
        run_ok = launch_ok and tasks_ok and load_ok and shell_ok

        print(f"SMP_BUSY_SCALE_COUNT_{count}_PIDS={len(pids)}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_START_CPUS=" +
              ",".join(str(cpu) for cpu in sorted(start_cpus)))
        print(f"SMP_BUSY_SCALE_COUNT_{count}_LAUNCH_SECONDS="
              f"{launch_elapsed:.3f}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_SHELL_SECONDS="
              f"{shell_elapsed:.3f}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_LOAD={load:.1f}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_KMEM_USED="
              f"{before[0] - during[0]}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_PAGE_USED="
              f"{before[1] - during[1]}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_RUN_RESULT="
              f"{'PASS' if run_ok else 'FAIL'}")

        kill_ok = True
        for pid in sorted(pids):
            try:
                run_command(port, f"kill {pid}", timeout=4.0,
                            reset_input=False)
            except TimeoutError as error:
                kill_ok = False
                print(f"SMP_BUSY_SCALE_COUNT_{count}_KILL_ERROR={pid}:"
                      f"{str(error)[-300:]}")

        time.sleep(1.0)
        after_tasks_output = run_command(port, "ps", timeout=20.0,
                                         reset_input=False)
        remaining = busy_tasks(after_tasks_output)
        for pid in sorted(remaining):
            try:
                run_command(port, f"kill -9 {pid}", timeout=4.0,
                            reset_input=False)
            except TimeoutError:
                kill_ok = False

        if remaining:
            time.sleep(1.0)
            after_tasks_output = run_command(port, "ps", timeout=20.0,
                                             reset_input=False)
            remaining = busy_tasks(after_tasks_output)

        recovered = memory_free_bytes(wait_memory_recovery(port, before))
        page_recovered = recovered[1] >= before[1]
        cleanup_ok = kill_ok and not remaining and page_recovered
        step_ok = run_ok and cleanup_ok
        passed = passed and step_ok
        if step_ok:
            max_stable = count

        print(f"SMP_BUSY_SCALE_COUNT_{count}_KMEM_RECOVERY_DRIFT="
              f"{recovered[0] - before[0]}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_PAGE_RECOVERY_DRIFT="
              f"{recovered[1] - before[1]}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_CLEANUP_RESULT="
              f"{'PASS' if cleanup_ok else 'FAIL'}")
        print(f"SMP_BUSY_SCALE_COUNT_{count}_RESULT="
              f"{'PASS' if step_ok else 'FAIL'}")

        if not step_ok:
            break

    print(f"SMP_BUSY_SCALE_MAX_STABLE={max_stable}")
    print(f"SMP_BUSY_SCALE_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def uptime_seconds(output: str) -> Optional[int]:
    """Extract NSH uptime's HH:MM:SS value."""

    match = re.search(r"(?m)^(\d+):(\d{2}):(\d{2}) up ", output)
    if match is None:
        return None

    hours, minutes, seconds = (int(value) for value in match.groups())
    return hours * 3600 + minutes * 60 + seconds


def memory_free_bytes(output: str) -> tuple[int, int]:
    """Extract free kernel-heap and page-pool bytes from NSH free output."""

    kmem = re.search(
        r"(?m)^\s*\d+\s+\d+\s+(\d+)\s+\d+\s+\d+\s+"
        r"\d+\s+\d+\s+Kmem\s*$", output)
    page = re.search(
        r"(?m)^\s*\d+\s+\d+\s+(\d+)\s+\d+\s+Page\s*$", output)
    if kmem is None or page is None:
        raise ValueError("unable to parse Kmem/Page free counters")

    return int(kmem.group(1)), int(page.group(1))


def wait_memory_recovery(port: serial.Serial,
                         baseline: tuple[int, int],
                         timeout: float = 20.0) -> str:
    """Wait for deferred SMP task/address-environment teardown to finish.

    Waiting for the counters to hold still is not enough: an exited process
    can keep its address environment for over a second, long enough for
    several consecutive free samples to agree on the pre-teardown value.  So
    wait for the known baseline to come back instead, and return the last
    sample either way so that a timeout shows up as a failed check.
    """

    deadline = time.monotonic() + timeout
    while True:
        output = run_command(port, "free", timeout=8.0)
        latest = memory_free_bytes(output)
        if latest[1] >= baseline[1] and latest[0] + 1024 >= baseline[0]:
            return output
        if time.monotonic() >= deadline:
            # Sample the kernel counters at the stuck point.  The page pool
            # staying high has two different causes that `free` cannot tell
            # apart: an address environment that was never destroyed, or one
            # that was destroyed without its pages returning to the pool.

            report_s31stat(port, "MEMORY_RECOVERY_TIMEOUT")
            return output
        time.sleep(0.25)


def memory_stats(output: str) -> dict[str, tuple[int, ...]]:
    """Parse all numeric Kmem/Page fields from NSH free output."""

    result: dict[str, tuple[int, ...]] = {}
    for line in output.splitlines():
        fields = line.split()
        if fields and fields[-1] in ("Kmem", "Page"):
            try:
                result[fields[-1]] = tuple(int(value)
                                            for value in fields[:-1])
            except ValueError:
                continue

    if len(result.get("Kmem", ())) < 5 or len(result.get("Page", ())) < 4:
        raise ValueError("unable to parse complete Kmem/Page counters")
    return result


def settled_memory(port, timeout: float = 8.0, streak: int = 3,
                   interval: float = 0.25) -> str:
    """Sample NSH free until the counters hold still for a while.

    A task that has just exited can still be holding its address environment
    when the next command runs, so a sample taken right afterwards reports
    the pages as in use.  Two back to back free commands are only about a
    hundred milliseconds apart, which is short enough that both land inside
    that window and agree on the wrong value, so require the reading to
    repeat across a real time span instead.
    """

    deadline = time.monotonic() + timeout
    current = run_command(port, "free", timeout=8.0)
    matches = 1
    while time.monotonic() < deadline:
        time.sleep(interval)
        candidate = run_command(port, "free", timeout=8.0)
        if memory_free_bytes(candidate) == memory_free_bytes(current):
            matches += 1
            if matches >= streak:
                return candidate
        else:
            matches = 1

        current = candidate

    return current


def print_memory_snapshot(label: str, output: str) -> None:
    """Emit stable key/value memory metrics for an E.1 checkpoint."""

    stats = memory_stats(output)
    kmem = stats["Kmem"]
    page = stats["Page"]
    print(f"E1_MEMORY_{label}_KMEM_TOTAL={kmem[0]}")
    print(f"E1_MEMORY_{label}_KMEM_USED={kmem[1]}")
    print(f"E1_MEMORY_{label}_KMEM_FREE={kmem[2]}")
    print(f"E1_MEMORY_{label}_KMEM_MAXFREE={kmem[4]}")
    print(f"E1_MEMORY_{label}_PAGE_TOTAL={page[0]}")
    print(f"E1_MEMORY_{label}_PAGE_USED={page[1]}")
    print(f"E1_MEMORY_{label}_PAGE_FREE={page[2]}")
    print(f"E1_MEMORY_{label}_PAGE_MAXFREE={page[3]}")


def stack_metrics(output: str) -> dict[str, tuple[int, int, int]]:
    """Extract stack size, used bytes, and remaining bytes from NSH ps."""

    result: dict[str, tuple[int, int, int]] = {}

    # STACK USED FILLED% [CPU%] COMMAND.  The CPU column only exists on SMP
    # builds, so keep it optional instead of folding it into the command.

    pattern = re.compile(
        r"(?m)^.*?\s(\d{7})\s+(\d{7})\s+\d+\.\d+%!?"
        r"(?:\s+\d+\.\d+%)?\s+(.+?)\s*$")
    for match in pattern.finditer(output):
        size = int(match.group(1))
        used = int(match.group(2))
        command = match.group(3)
        result[command] = (size, used, size - used)
    return result


def print_stack_snapshot(label: str, output: str) -> None:
    """Emit stack headroom for every task visible in an E.1 checkpoint."""

    for command, (size, used, remaining) in stack_metrics(output).items():
        key = re.sub(r"[^A-Za-z0-9]+", "_", command).strip("_").upper()
        print(f"E1_STACK_{label}_{key}_SIZE={size}")
        print(f"E1_STACK_{label}_{key}_USED={used}")
        print(f"E1_STACK_{label}_{key}_REMAINING={remaining}")


def wlan_ipv4(output: str) -> Optional[str]:
    """Extract the assigned IPv4 address from NuttX ifconfig output."""

    match = re.search(
        r"(?m)^\s*inet(?:\s+addr:|\s+)(\d{1,3}(?:\.\d{1,3}){3})\b",
        output)
    if match is None or match.group(1) == "0.0.0.0":
        return None
    return match.group(1)


def marker_pid(output: str, marker: str) -> Optional[int]:
    """Extract a task PID from an application marker."""

    match = re.search(rf"{re.escape(marker)} pid=(\d+)", output)
    return int(match.group(1)) if match is not None else None


def obtain_dhcp(port: serial.Serial,
                attempts: int = 3) -> tuple[str, str, Optional[str]]:
    """Run one DHCP client at a time and reject stale fallback addresses.

    Current NSH implicitly starts DHCP for ``ifconfig wlan0 0.0.0.0``.
    Preserve an existing nonzero gateway when clearing a static address so
    NSH does not implicitly start DHCP.  renew reports failure even if an old
    address remains, so require both a clean result and an assigned IP.
    """

    if attempts < 1:
        raise ValueError("DHCP attempts must be positive")
    initial = run_command(port, "ifconfig wlan0")
    if wlan_ipv4(initial) is not None:
        gateway = re.search(r"\bDRaddr:(\d{1,3}(?:\.\d{1,3}){3})\b", initial)
        if gateway is None or gateway.group(1) == "0.0.0.0":
            raise RuntimeError("Cannot clear static IP without a nonzero gateway")
        run_command(port, "ifconfig wlan0 0.0.0.0 gw " + gateway.group(1),
                    timeout=8.0)
    transcript = ""
    interface = ""
    address = None
    for attempt in range(1, attempts + 1):
        if attempt > 1:
            time.sleep(2.0)
        renew = run_command(port, "renew wlan0", timeout=45.0)
        interface = run_command(port, "ifconfig wlan0")
        transcript = renew + interface
        address = wlan_ipv4(interface)
        failed = any(marker in renew.lower()
                     for marker in ("failed", "error:", "nsh:"))
        if address is not None and not failed:
            return transcript, interface, address

        address = None

    return transcript, interface, address


def connect_sta(port: serial.Serial, ssid: str,
                password: str) -> tuple[str, str, str, Optional[str]]:
    """Associate with WPA2 and obtain a lease, returning every transcript.

    Both the E.1 suite and the network-load leak probe need an associated
    station, but only E.1 needs the rest of that suite, so the association
    itself is shared while the assertions stay with each caller.
    """

    escaped_ssid = nsh_escape(ssid)
    escaped_password = nsh_escape(password)
    redactions = (password, escaped_password, ssid, escaped_ssid)

    psk = run_command(port, f"wapi psk wlan0 {escaped_password} 3 2",
                      timeout=8.0, redactions=redactions)
    essid = run_command(port, f"wapi essid wlan0 {escaped_ssid} 1",
                        timeout=20.0, redactions=redactions)
    dhcp, _, address = obtain_dhcp(port)
    return psk, essid, dhcp, address


def app_hash(output: str, path: str) -> Optional[str]:
    """Extract an APPVERIFY PASS digest for one executable path."""

    match = re.search(
        rf"APPVERIFY:PASS {re.escape(path)} sha256=([0-9a-f]{{64}})",
        output)
    return match.group(1) if match is not None else None


def reset_and_collect(port: serial.Serial, timeout: float = 4.0) -> str:
    """Reset and collect a complete boot through the first prompt."""

    hard_reset(port)
    output = collect_until_prompt(port, timeout)
    rendered = output.decode("utf-8", errors="replace")
    print("--- E0 RESET CONSOLE ---")
    print(rendered.strip())
    return rendered


def run_e0_suite(port: serial.Serial, leak_iterations: int) -> bool:
    """Exercise Flash AppFS integrity, persistence, recovery, and leaks."""

    checks: dict[str, bool] = {}

    listing = run_command(port, "ls -l /apps")
    original_output = run_command(port, "/apps/sv32test ok", timeout=8.0)
    original_hash = app_hash(original_output, "/apps/sv32test")
    checks["E0_LITTLEFS_MOUNT"] = (
        "sv32test" in listing and "sv32test.sha256" in listing and
        original_hash is not None and "SV32TEST:OK PASS" in original_output)

    negative_before = memory_free_bytes(run_command(port, "free"))
    samples = (
        ("truncated", True),
        ("badelf", False),
        ("badhash", True),
        ("badsegment", True),
        ("oversize", True),
    )
    rejected = True
    for name, verifier_log_expected in samples:
        path = f"/system/bin/sv32test.{name}"
        output = run_command(port, path, timeout=8.0)
        rejected = rejected and "SV32TEST:" not in output
        if verifier_log_expected:
            rejected = rejected and f"APPVERIFY:REJECT {path}" in output

    alive = run_command(port, "echo E0_NEGATIVE_ALIVE")
    negative_after = memory_free_bytes(run_command(port, "free"))
    checks["E0_CORRUPT_REJECT"] = rejected and \
        "E0_NEGATIVE_ALIVE" in alive
    checks["E0_REJECT_NO_LEAK"] = (
        negative_after[1] == negative_before[1] and
        abs(negative_after[0] - negative_before[0]) <= 1024)

    install_commands = (
        "cp /system/bin/sv32test.v2 /apps/sv32test.new",
        "cp /system/bin/sv32test.v2.sha256 /apps/sv32test.new.sha256",
        "mv /apps/sv32test.new /apps/sv32test",
        "mv /apps/sv32test.new.sha256 /apps/sv32test.sha256",
    )
    install_ok = True
    for command in install_commands:
        output = run_command(port, command, timeout=8.0)
        install_ok = install_ok and "failed" not in output.lower()

    v2_output = run_command(port, "/apps/sv32test ok", timeout=8.0)
    v2_hash = app_hash(v2_output, "/apps/sv32test")
    checks["E0_ATOMIC_REPLACE"] = (
        install_ok and "SV32TEST:OK PASS" in v2_output and
        v2_hash is not None and original_hash is not None and
        v2_hash != original_hash)

    run_command(port, "echo E0_PERSIST > /apps/persist.txt")
    persistence_boot = reset_and_collect(port)
    marker = run_command(port, "cat /apps/persist.txt")
    persisted_v2 = run_command(port, "/apps/sv32test ok", timeout=8.0)
    checks["E0_REBOOT_PERSIST"] = (
        "AppFS MTD boundary checks: PASS" in persistence_boot and
        "E0_PERSIST" in marker and
        app_hash(persisted_v2, "/apps/sv32test") == v2_hash and
        "SV32TEST:OK PASS" in persisted_v2)

    run_command(port,
                "cp /system/bin/sv32test /apps/.sv32test.powerloss",
                timeout=8.0)
    run_command(port,
                "cp /system/bin/sv32test.sha256 "
                "/apps/.sv32test.powerloss.sha256", timeout=8.0)
    powerloss_boot = reset_and_collect(port)
    after_powerloss = run_command(port, "/apps/sv32test ok", timeout=8.0)
    checks["E0_POWERLOSS_STAGING"] = (
        "AppFS writable LittleFS" in powerloss_boot and
        app_hash(after_powerloss, "/apps/sv32test") == v2_hash and
        "SV32TEST:OK PASS" in after_powerloss)

    run_command(port, "rm /apps/sv32test")
    run_command(port, "rm /apps/sv32test.sha256")
    deleted_now = run_command(port, "/apps/sv32test ok", timeout=8.0)
    delete_boot = reset_and_collect(port)
    deleted_after = run_command(port, "/apps/sv32test ok", timeout=8.0)
    checks["E0_DELETE_PERSIST"] = (
        "SV32TEST:OK PASS" not in deleted_now and
        "SV32TEST:OK PASS" not in deleted_after and
        "installed seed application" not in delete_boot)

    restore_commands = (
        "cp /system/bin/sv32test /apps/sv32test.new",
        "cp /system/bin/sv32test.sha256 /apps/sv32test.new.sha256",
        "mv /apps/sv32test.new /apps/sv32test",
        "mv /apps/sv32test.new.sha256 /apps/sv32test.sha256",
    )
    for command in restore_commands:
        run_command(port, command, timeout=8.0)

    restored = run_command(port, "/apps/sv32test ok", timeout=8.0)
    checks["E0_RESTORE"] = (
        app_hash(restored, "/apps/sv32test") == original_hash and
        "SV32TEST:OK PASS" in restored)

    # The preceding reset recreated the volatile root filesystem, including
    # /proc.  Mount procfs again before reading allocator counters.

    run_command(port, "mkdir /proc")
    run_command(port, "mount -t procfs /proc")
    leak_before = memory_free_bytes(run_command(port, "free"))
    repeated_ok = True
    for _ in range(leak_iterations):
        output = run_command(port, "/apps/sv32test ok", timeout=8.0)
        repeated_ok = repeated_ok and "SV32TEST:OK PASS" in output

    leak_after = memory_free_bytes(run_command(port, "free"))
    kmem_drift = leak_after[0] - leak_before[0]
    page_drift = leak_after[1] - leak_before[1]
    checks["E0_REPEAT_NO_LEAK"] = (
        repeated_ok and page_drift == 0 and abs(kmem_drift) <= 1024)
    print(f"E0_LEAK_ITERATIONS={leak_iterations}")
    print(f"E0_KMEM_FREE_DRIFT={kmem_drift}")
    print(f"E0_PAGE_FREE_DRIFT={page_drift}")

    for name, passed in checks.items():
        print(f"{name}_RESULT={'PASS' if passed else 'FAIL'}")

    passed = all(checks.values())
    print(f"E0_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def run_e1_suite(port: serial.Serial, ssid: str, password: str,
                 rounds: int) -> bool:
    """Exercise WPA2 STA, DHCP/DNS/TLS/cloud, recovery, and isolation."""

    checks: dict[str, bool] = {}
    escaped_ssid = nsh_escape(ssid)
    escaped_password = nsh_escape(password)
    redactions = (password, escaped_password, ssid, escaped_ssid)

    scan = run_command(port, "wapi scan wlan0", timeout=20.0,
                       redactions=redactions)
    checks["E1_SCAN"] = (
        re.search(r"(?i)\b[0-9a-f]{2}(?::[0-9a-f]{2}){5}\b", scan)
        is not None)

    memory_before = run_command(port, "free")
    print_memory_snapshot("BEFORE_STA", memory_before)

    psk, essid, dhcp, address = connect_sta(port, ssid, password)
    checks["E1_STA_CONFIG"] = all(
        marker not in (psk + essid).lower()
        for marker in ("failed", "error", "invalid"))
    checks["E1_DHCP"] = (
        address is not None and "failed" not in dhcp.lower())
    print(f"E1_STA_IPV4={address or 'NONE'}")
    print(f"E1_DHCP_TRANSCRIPT="
          f"{'PASS' if 'failed' not in dhcp.lower() else 'FAIL'}")

    checks["E1_TIME_SYNC"] = synchronize_time(port)

    memory_after_dhcp = run_command(port, "free")
    print_memory_snapshot("AFTER_DHCP", memory_after_dhcp)
    stacks_after_dhcp = run_command(port, "ps")
    print_stack_snapshot("AFTER_DHCP", stacks_after_dhcp)

    cloud = run_command(port, f"e1net -r {rounds}",
                        timeout=max(90.0, rounds * 35.0))
    checks["E1_DNS"] = cloud.count("E1NET:DNS=PASS") == rounds
    checks["E1_TCP"] = cloud.count("E1NET:TCP=PASS") == rounds
    checks["E1_TLS_CHAIN"] = (
        cloud.count("E1NET:TLS=PASS") == rounds and
        cloud.count("verify=chain") == rounds)
    checks["E1_CLOUD_HTTP"] = (
        cloud.count("E1NET:HTTP=PASS") == rounds and
        f"E1NET:RESULT=PASS rounds={rounds}" in cloud)

    memory_after_tls = settled_memory(port)
    print_memory_snapshot("AFTER_TLS", memory_after_tls)

    forever_start = run_command(port, "/system/bin/sv32test forever &",
                                timeout=5.0)
    if "SV32TEST:FOREVER BEGIN" not in forever_start:
        extra = collect_until_marker(port, b"SV32TEST:FOREVER BEGIN", 3.0)
        rendered = extra.decode("utf-8", errors="replace")
        print("--- U-MODE FOREVER START ---")
        print(rendered.strip())
        forever_start += rendered

    forever_pid = marker_pid(forever_start, "SV32TEST:FOREVER BEGIN")
    concurrent_start = run_command(port, f"e1net -r {rounds} &",
                                   timeout=8.0, reset_input=False)
    if "E1NET:START" not in concurrent_start:
        extra = collect_until_marker(port, b"E1NET:START", 5.0)
        rendered = extra.decode("utf-8", errors="replace")
        print("--- U-MODE E1NET START ---")
        print(rendered.strip())
        concurrent_start += rendered

    e1net_pid = marker_pid(concurrent_start, "E1NET:START")
    concurrent_probe = run_command(port, "echo E1_CONCURRENT_SHELL_OK",
                                   timeout=3.0, reset_input=False)
    concurrent_ps = run_command(port, "ps", timeout=5.0,
                                reset_input=False)
    print_stack_snapshot("CONCURRENT", concurrent_ps)
    concurrent_memory = run_command(port, "free", timeout=5.0,
                                    reset_input=False)
    print_memory_snapshot("CONCURRENT", concurrent_memory)

    if e1net_pid is not None:
        concurrent_wait = run_command(
            port, f"wait {e1net_pid}", timeout=max(90.0, rounds * 35.0),
            reset_input=False)
    else:
        concurrent_wait = ""

    concurrent_cloud = (concurrent_start + concurrent_probe +
                        concurrent_ps + concurrent_memory + concurrent_wait)
    if forever_pid is not None:
        kill_output = run_command(port, f"kill -9 {forever_pid}", timeout=5.0,
                                  reset_input=False)
    else:
        kill_output = ""

    time.sleep(2.0)
    tasks_after_kill = run_command(port, "ps")
    checks["E1_UMODE_CONCURRENCY"] = (
        forever_pid is not None and e1net_pid is not None and
        "E1_CONCURRENT_SHELL_OK" in concurrent_probe and
        "sv32test forever" in concurrent_ps and
        "e1net" in concurrent_ps and
        f"E1NET:RESULT=PASS rounds={rounds}" in concurrent_cloud)
    checks["E1_UMODE_TERMINATE"] = (
        forever_pid is not None and "failed" not in kill_output.lower() and
        "sv32test forever" not in tasks_after_kill)

    required_tasks = (
        "lpwork", "esp_timer", "wifi", "/system/bin/init",
        "netdev-wlan0",
    )
    task_stacks = stack_metrics(stacks_after_dhcp)
    has_idle = any(
        command == "Idle_Task" or "IDLE" in command
        for command in task_stacks)
    checks["E1_STACK_HEADROOM"] = (
        "STACK" in stacks_after_dhcp and "USED" in stacks_after_dhcp and
        has_idle and
        all(any(command == required or command.startswith(required + " ")
                for command in task_stacks)
            for required in required_tasks) and
        all(remaining >= 256
            for _size, _used, remaining in task_stacks.values()))

    wifi_register = run_command(port, "/system/bin/sv32test wifi-reg-read",
                                timeout=5.0)
    after_register = run_command(port, "echo AFTER_WIFI_REG_READ")
    wifi_private = run_command(port, "/system/bin/sv32test kernel-read",
                               timeout=5.0)
    after_private = run_command(port, "echo AFTER_WIFI_PRIVATE_READ")
    checks["E1_WIFI_REGISTER_DENY"] = (
        ("WIFI_REG_READ BEGIN" in wifi_register or
         ("wifi-reg-read" in wifi_register and
          "APPVERIFY:PASS /system/bin/sv32test" in wifi_register)) and
        "Load page fault" in wifi_register and
        "MTVAL: 20104000" in wifi_register and
        "Segmentation fault" in wifi_register and
        "SV32TEST:WIFI_REG_READ FAIL" not in wifi_register)
    checks["E1_WIFI_PRIVATE_DENY"] = (
        ("KERNEL_READ BEGIN" in wifi_private or
         ("kernel-read" in wifi_private and
          "APPVERIFY:PASS /system/bin/sv32test" in wifi_private)) and
        "Load page fault" in wifi_private and
        "MTVAL: 2f000000" in wifi_private and
        "Segmentation fault" in wifi_private and
        "SV32TEST:KERNEL_READ FAIL" not in wifi_private)

    down = run_command(port, "ifdown wlan0", timeout=10.0)
    up = run_command(port, "ifup wlan0", timeout=10.0)
    reassociate = run_command(port, f"wapi essid wlan0 {escaped_ssid} 1",
                              timeout=20.0, redactions=redactions)
    dhcp_after, interface_after, reconnect_address = obtain_dhcp(port)
    reconnect_cloud = run_command(port, "e1net -r 1", timeout=90.0)
    checks["E1_RECONNECT"] = (
        "ifdown wlan0...OK" in down and "ifup wlan0...OK" in up and
        "failed" not in reassociate.lower() and
        "failed" not in dhcp_after.lower() and
        reconnect_address is not None and
        "E1NET:RESULT=PASS rounds=1" in reconnect_cloud)
    checks["E1_FAULT_RECOVERY"] = (
        "AFTER_WIFI_REG_READ" in after_register and
        "AFTER_WIFI_PRIVATE_READ" in after_private and
        "E1NET:TLS=PASS" in reconnect_cloud)
    print(f"E1_RECONNECT_IPV4={reconnect_address or 'NONE'}")

    tls_free = memory_free_bytes(memory_after_tls)
    memory_final = wait_memory_recovery(port, tls_free)
    print_memory_snapshot("FINAL", memory_final)
    final_free = memory_free_bytes(memory_final)
    checks["E1_MEMORY_RECOVERY"] = (
        abs(final_free[0] - tls_free[0]) <= 4096 and
        abs(final_free[1] - tls_free[1]) <= 4096)

    for name, passed in checks.items():
        print(f"{name}_RESULT={'PASS' if passed else 'FAIL'}")

    passed = all(checks.values())
    print(f"E1_SUITE_RESULT={'PASS' if passed else 'FAIL'}")
    return passed


def main() -> int:
    global CONSOLE_RECOVERY_TIMEOUT
    global CONSOLE_SLOW_AFTER

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",
                        help="serial device (default: auto-detect USB console)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="UART baud rate (default: %(default)s)")
    parser.add_argument("--boots", type=int, default=3,
                        help="number of consecutive reset tests")
    parser.add_argument(
        "--cpu1-smp", dest="cpu1_mode", action="store_const", const="smp",
        help="require the G.2.0 64-round SMP scheduler test on each boot")
    parser.add_argument("--e0", action="store_true",
                        help="also run destructive Stage E.0 AppFS tests")
    parser.add_argument("--e1", action="store_true",
                        help="also run Stage E.1 Wi-Fi/network/cloud tests")
    parser.add_argument("--e1-rounds", type=int, default=3,
                        help="TLS/cloud rounds per E.1 run")
    parser.add_argument("--leak-iterations", type=int, default=25,
                        help="successful ELF loads in the E.0 leak test")
    parser.add_argument(
        "--smp-busy-rounds", type=int, default=0,
        help="launch/measure/kill two unpinned busy Apps for N rounds")
    parser.add_argument(
        "--smp-busy-counts", type=busy_count_ladder, default=(),
        help="increasing count ladder, for example 2,4,8,16,32")
    parser.add_argument(
        "--expect-rr-ms", type=int,
        help="require each intact busy marker to report this RR interval")
    parser.add_argument(
        "--smp-busy-only", action="store_true",
        help="after boot checks, run only the requested SMP busy suites")
    parser.add_argument(
        "--tlb-stress", action="store_true",
        help="run the G.2.1b cross-core addrenv TLB stress suite")
    parser.add_argument(
        "--tlb-stress-rounds", type=int, default=64,
        help="tlbstress heap-grow rounds (default: %(default)s)")
    parser.add_argument(
        "--tlb-stress-pages", type=int, default=16,
        help="pages allocated per tlbstress round (default: %(default)s)")
    parser.add_argument(
        "--tlb-stress-mode", choices=("auto", "pin"), default="auto",
        help="tlbstress scheduling mode (default: %(default)s)")
    parser.add_argument(
        "--tlb-stress-require-shootdown", action="store_true", default=True,
        help="require /dev/tlbshoot send/ack deltas (default)")
    parser.add_argument(
        "--tlb-stress-no-require-shootdown",
        dest="tlb_stress_require_shootdown", action="store_false",
        help="do not require shootdown counters (negative A/B profile)")
    parser.add_argument(
        "--net-leak-url",
        help="stream URL from platform/tools/net_busy_server.py; enables the "
             "addrenv leak probe under saturated Wi-Fi receive load")
    parser.add_argument(
        "--net-leak-rounds", type=int, default=20,
        help="spawn/exit rounds under network load (default: %(default)s)")
    parser.add_argument(
        "--net-leak-victim", choices=("busy", "stream"), default="busy",
        help="kill compute loops or a task blocked in TCP receive each round "
             "(default: %(default)s)")
    parser.add_argument(
        "--net-leak-victim-claim-delay", type=float, default=1.5,
        help="seconds to wait before claiming a stream victim PID "
             "(default: %(default)s)")
    parser.add_argument(
        "--net-leak-victim-settle", type=float, default=0.5,
        help="seconds from PID claim to terminating the victim "
             "(default: %(default)s)")
    parser.add_argument(
        "--net-leak-only", action="store_true",
        help="after boot checks, run only the saturated teardown probe")
    parser.add_argument(
        "--console-slow-after", type=float, default=0.0,
        help="report a command with no console bytes for this many seconds")
    parser.add_argument(
        "--console-recovery-timeout", type=float, default=0.0,
        help="after a slow-console report, keep collecting for this many "
             "seconds before failing")
    args = parser.parse_args()

    if args.boots < 1:
        parser.error("--boots must be at least 1")
    if args.leak_iterations < 1:
        parser.error("--leak-iterations must be at least 1")
    if args.e1_rounds < 1 or args.e1_rounds > 20:
        parser.error("--e1-rounds must be between 1 and 20")
    if args.smp_busy_rounds < 0 or args.smp_busy_rounds > 20:
        parser.error("--smp-busy-rounds must be between 0 and 20")
    if args.expect_rr_ms is not None and args.expect_rr_ms < 1:
        parser.error("--expect-rr-ms must be positive")
    if args.tlb_stress_rounds < 1 or args.tlb_stress_rounds > 1024:
        parser.error("--tlb-stress-rounds must be between 1 and 1024")
    if args.tlb_stress_pages < 1 or args.tlb_stress_pages > 256:
        parser.error("--tlb-stress-pages must be between 1 and 256")
    if (args.net_leak_victim_claim_delay < 0.0 or
            args.net_leak_victim_claim_delay > 10.0):
        parser.error(
            "--net-leak-victim-claim-delay must be between 0 and 10")
    if (args.net_leak_victim_settle < 0.0 or
            args.net_leak_victim_settle > 10.0):
        parser.error("--net-leak-victim-settle must be between 0 and 10")
    if args.console_slow_after < 0.0:
        parser.error("--console-slow-after cannot be negative")
    if args.console_recovery_timeout < 0.0:
        parser.error("--console-recovery-timeout cannot be negative")
    if ((args.console_slow_after == 0.0) !=
            (args.console_recovery_timeout == 0.0)):
        parser.error(
            "--console-slow-after and --console-recovery-timeout must be "
            "enabled together")
    CONSOLE_SLOW_AFTER = args.console_slow_after
    CONSOLE_RECOVERY_TIMEOUT = args.console_recovery_timeout
    if (args.smp_busy_only and args.smp_busy_rounds < 1 and
            not args.smp_busy_counts):
        parser.error(
            "--smp-busy-only requires --smp-busy-rounds or "
            "--smp-busy-counts")
    if args.smp_busy_only and (args.e0 or args.e1 or args.tlb_stress):
        parser.error(
            "--smp-busy-only cannot be combined with --e0, --e1, or "
            "--tlb-stress")
    if args.net_leak_only and args.net_leak_url is None:
        parser.error("--net-leak-only requires --net-leak-url")
    if args.net_leak_only and (
            args.smp_busy_only or args.smp_busy_rounds or
            args.smp_busy_counts or args.e0 or args.e1 or args.tlb_stress):
        parser.error(
            "--net-leak-only cannot be combined with SMP busy, E.0, E.1, "
            "or TLB stress suites")
    if args.net_leak_url is not None:
        if args.net_leak_rounds < 1 or args.net_leak_rounds > 200:
            parser.error("--net-leak-rounds must be between 1 and 200")

        load_command = f"wget -o /dev/null {args.net_leak_url} &"
        if len(load_command) > NSH_COMMAND_MAX:
            parser.error(
                f"--net-leak-url yields a {len(load_command)}-character NSH "
                f"command and the console truncates past {NSH_COMMAND_MAX}; "
                "serve the payload under a shorter name")

    e1_ssid = os.environ.get(E1_SSID_ENV)
    e1_password = os.environ.get(E1_PASSWORD_ENV)
    if (args.e1 or args.net_leak_url) and (not e1_ssid or not e1_password):
        parser.error(
            f"--e1 and --net-leak-url require {E1_SSID_ENV} and "
            f"{E1_PASSWORD_ENV}")

    if args.port is None:
        try:
            args.port = find_usb_console()
        except ValueError as error:
            parser.error(str(error))

    port = serial.Serial(args.port, args.baud, timeout=0.05,
                         write_timeout=1.0, exclusive=True)
    port.dtr = False
    port.rts = False
    time.sleep(0.10)

    try:
        boot_ok = True
        cpu1_boot_ok = True
        for attempt in range(1, args.boots + 1):
            hard_reset(port)
            output = collect_until_prompt(port, 3.0)
            failed_marker = any(marker in output
                                for marker in BOOT_FAILURE_MARKERS)
            passed = (b"NuttShell (NSH)" in output and
                      PROMPT in output and not failed_marker)
            cpu1_passed = (args.cpu1_mode is None or
                           CPU1_BOOT_MARKERS[args.cpu1_mode] in output)
            if args.cpu1_mode is not None:
                passed = passed and cpu1_passed
            boot_ok = boot_ok and passed
            cpu1_boot_ok = cpu1_boot_ok and cpu1_passed
            print(f"BOOT_{attempt}={'PASS' if passed else 'FAIL'}")
            if args.cpu1_mode is not None:
                print(f"CPU1_BOOT_{attempt}="
                      f"{'PASS' if cpu1_passed else 'FAIL'}")
            # Always show the first boot as the reference console, and any
            # failed boot, otherwise an intermittent failure leaves no trace.

            if attempt == 1 or not passed:
                print(f"--- BOOT_{attempt}_CONSOLE ---")
                print(output.decode("utf-8", errors="replace").strip())
                print(f"--- END_BOOT_{attempt}_CONSOLE ---")

        print(f"CONSECUTIVE_BOOT_RESULT={'PASS' if boot_ok else 'FAIL'}")
        if args.cpu1_mode is not None:
            print(f"CPU1_BOOT_MODE={args.cpu1_mode.upper()}")
            print("CPU1_CONSECUTIVE_BOOT_RESULT="
                  f"{'PASS' if cpu1_boot_ok else 'FAIL'}")

        if args.smp_busy_only:
            mount = run_command(port, "mount")
            if not re.search(r"(?m)^\s*/proc type procfs\s*$", mount):
                run_command(port, "mkdir /proc")
                run_command(port, "mount -t procfs /proc")

            smp_busy_ok = (run_smp_busy_suite(
                port, args.smp_busy_rounds, args.expect_rr_ms)
                if args.smp_busy_rounds else True)
            smp_scale_ok = (run_smp_busy_scale_suite(
                port, args.smp_busy_counts, args.expect_rr_ms)
                if args.smp_busy_counts else True)
            overall = boot_ok and smp_busy_ok and smp_scale_ok
            print(f"TEST_RESULT={'PASS' if overall else 'FAIL'}")
            return 0 if overall else 1

        if args.net_leak_only:
            mount = run_command(port, "mount")
            if not re.search(r"(?m)^\s*/proc type procfs\s*$", mount):
                run_command(port, "mkdir /proc")
                run_command(port, "mount -t procfs /proc")

            before = report_s31stat(port, "NET_LEAK_ONLY_BASELINE")
            net_leak_ok = run_net_leak_suite(
                port, args.net_leak_url, args.net_leak_rounds,
                args.net_leak_victim, e1_ssid, e1_password,
                args.net_leak_victim_claim_delay,
                args.net_leak_victim_settle)
            after = report_s31stat(port, "NET_LEAK_ONLY_FINAL")
            addrenv_ok = check_addrenv_returned(
                before, after, "NET_LEAK_ONLY")
            overall = boot_ok and net_leak_ok and addrenv_ok
            print(f"TEST_RESULT={'PASS' if overall else 'FAIL'}")
            return 0 if overall else 1

        uname = run_command(port, "uname -a")
        before = run_command(port, "uptime")
        console = run_command(port, "echo CONSOLE_NSH_OK")
        mount = run_command(port, "mount")
        if not re.search(r"(?m)^\s*/proc type procfs\s*$", mount):
            run_command(port, "mkdir /proc")
            mount = run_command(port, "mount -t procfs /proc")
        heap = run_command(port, "free")
        tasks = run_command(port, "ps")
        timer_start = run_command(port, "sleep 2 &")
        time.sleep(3.0)
        after = run_command(port, "uptime")
        tasks_after_timer = run_command(port, "ps")
        foreground_sleep, foreground_elapsed = run_foreground_sleep(port)
        foreground_probe = run_command(port, "echo FOREGROUND_SLEEP_OK")
        busy_output, preempt_probe, preempt_elapsed = \
            run_busy_preemption(port)
        time.sleep(5.2)
        tasks_after_busy = run_command(port, "ps")
        sv32_illegal = run_command(port, "/system/bin/sv32test illegal",
                                   timeout=5.0)
        after_illegal = run_command(port, "echo AFTER_ILLEGAL")
        sv32_ok = run_command(port, "/system/bin/sv32test ok", timeout=5.0)
        heap_grow_before = memory_free_bytes(run_command(port, "free"))
        sv32_heap_grow = run_command(
            port, "/system/bin/sv32test heap-grow", timeout=8.0)
        heap_grow_after = memory_free_bytes(
            wait_memory_recovery(port, heap_grow_before))
        sv32_text = run_command(port, "/system/bin/sv32test write-text",
                                timeout=5.0)
        after_text = run_command(port, "echo AFTER_WRITE_TEXT")
        sv32_data = run_command(port, "/system/bin/sv32test exec-data",
                                timeout=5.0)
        after_data = run_command(port, "echo AFTER_EXEC_DATA")
        sv32_kernel = run_command(port, "/system/bin/sv32test kernel-read",
                                  timeout=5.0)
        after_kernel = run_command(port, "echo AFTER_KERNEL_READ")

        before_seconds = uptime_seconds(before)
        after_seconds = uptime_seconds(after)
        timer_progressed = (before_seconds is not None and
                            after_seconds is not None and
                            after_seconds >= before_seconds + 2)
        heap_match = re.search(
            r"(?m)^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+"
            r"(\d+)\s+(\d+)\s+(\d+)\s+Kmem\s*$", heap)

        checks = {
            "UNAME": ("NuttX" in uname and
                      "esp32s31-core-function-board" in uname),
            "TIMER": ("sleep" not in tasks_after_timer.lower() and
                      timer_progressed and
                      1.8 <= foreground_elapsed <= 5.0 and
                      "FOREGROUND_SLEEP_OK" in foreground_probe),
            "CONSOLE": "CONSOLE_NSH_OK" in console,
            "PROCFS_MOUNT": ("failed" not in mount.lower() and
                             "error" not in mount.lower()),
            "HEAP": heap_match is not None,
            "PS_IDLE": ("Idle_Task" in tasks or
                        ("CPU0 IDLE" in tasks and "CPU1 IDLE" in tasks)),
            "PS_NSH": "/system/bin/init" in tasks,
            "UMODE_PREEMPT": (
                "SV32TEST:BUSY BEGIN" in busy_output and
                re.search(r"(?:^|\r?\n)UOK\r?\n", preempt_probe)
                is not None and
                preempt_elapsed <= 1.5 and
                "sv32test" not in tasks_after_busy),
            "UMODE_ILLEGAL_ISOLATION": (
                sv32_fault_probe_started(sv32_illegal, "illegal") and
                "Illegal instruction" in sv32_illegal and
                "Segmentation fault" in sv32_illegal and
                "SV32TEST:ILLEGAL FAIL" not in sv32_illegal and
                "AFTER_ILLEGAL" in after_illegal),
            "SV32_OK": "SV32TEST:OK PASS" in sv32_ok,
            "SV32_HEAP_GROW": (
                "/system/bin/sv32test heap-grow" in sv32_heap_grow and
                "APPVERIFY:PASS /system/bin/sv32test" in sv32_heap_grow and
                # Native USB console can interleave CPU1 diagnostics into the
                # U-mode marker and drop the colon; require the stable payload.
                ("SV32TEST:HEAP_GROW PASS" in sv32_heap_grow or
                 "HEAP_GROW PASS" in sv32_heap_grow) and
                "checksum=00008025" in sv32_heap_grow),
            "SV32_HEAP_RECOVERY": (
                heap_grow_after[1] >= heap_grow_before[1] and
                heap_grow_after[0] + 1024 >= heap_grow_before[0]),
            "SV32_TEXT_RX": (
                sv32_fault_probe_started(sv32_text, "write-text") and
                "Store/AMO page fault" in sv32_text and
                "Segmentation fault" in sv32_text and
                "SV32TEST:WRITE_TEXT FAIL" not in sv32_text),
            "SV32_DATA_NX": (
                sv32_fault_probe_started(sv32_data, "exec-data") and
                "Instruction page fault" in sv32_data and
                "Segmentation fault" in sv32_data and
                "SV32TEST:EXEC_DATA FAIL" not in sv32_data),
            "SV32_KERNEL_DENY": (
                sv32_fault_probe_started(sv32_kernel, "kernel-read") and
                "Load page fault" in sv32_kernel and
                "MTVAL: 2f000000" in sv32_kernel and
                "Segmentation fault" in sv32_kernel and
                "SV32TEST:KERNEL_READ FAIL" not in sv32_kernel),
            "SV32_FAULT_RECOVERY": (
                "AFTER_WRITE_TEXT" in after_text and
                "AFTER_EXEC_DATA" in after_data and
                "AFTER_KERNEL_READ" in after_kernel),
        }

        if heap_match is not None:
            print(f"HEAP_TOTAL={heap_match.group(1)}")
            print(f"HEAP_USED={heap_match.group(2)}")
            print(f"HEAP_FREE={heap_match.group(3)}")

        for name, passed in checks.items():
            print(f"{name}_RESULT={'PASS' if passed else 'FAIL'}")

        command_ok = all(checks.values())
        print(f"COMMAND_SUITE_RESULT={'PASS' if command_ok else 'FAIL'}")
        # Sample between suites so a leaked address environment can be blamed
        # on the suite that created it instead of only showing up in the total.

        monstat_before = report_s31stat(port, "BASELINE")
        stages = [("BASELINE", monstat_before)]

        smp_busy_ok = run_smp_busy_suite(
            port, args.smp_busy_rounds, args.expect_rr_ms) \
            if args.smp_busy_rounds else True
        if args.smp_busy_rounds:
            stages.append(("SMP_BUSY", report_s31stat(port, "SMP_BUSY")))

        smp_scale_ok = run_smp_busy_scale_suite(
            port, args.smp_busy_counts, args.expect_rr_ms) \
            if args.smp_busy_counts else True
        if args.smp_busy_counts:
            stages.append(("SMP_SCALE", report_s31stat(port, "SMP_SCALE")))

        e0_ok = run_e0_suite(port, args.leak_iterations) if args.e0 else True

        e1_ok = run_e1_suite(port, e1_ssid, e1_password, args.e1_rounds) \
            if args.e1 else True
        if args.e1:
            stages.append(("E1", report_s31stat(port, "E1")))

        net_leak_ok = run_net_leak_suite(
            port, args.net_leak_url, args.net_leak_rounds,
            args.net_leak_victim,
            None if args.e1 else e1_ssid,
            None if args.e1 else e1_password,
            args.net_leak_victim_claim_delay,
            args.net_leak_victim_settle) \
            if args.net_leak_url else True
        if args.net_leak_url:
            stages.append(("NET_LEAK", report_s31stat(port, "NET_LEAK")))

        tlb_ok = run_tlb_stress_suite(
            port, args.tlb_stress_rounds, args.tlb_stress_pages,
            args.tlb_stress_mode, args.tlb_stress_require_shootdown) \
            if args.tlb_stress else True
        if args.tlb_stress:
            stages.append(("TLB_STRESS", report_s31stat(port, "TLB_STRESS")))

        monstat_after = report_s31stat(port, "FINAL")
        stages.append(("FINAL", monstat_after))
        addrenv_ok = (check_addrenv_balance(monstat_before, "BASELINE") and
                      check_addrenv_balance(monstat_after, "FINAL"))

        # The E.0 suite resets the board, which restarts every counter, so the
        # cross-suite delta is only meaningful without it.

        if args.e0:
            print("S31STAT_SUITE_ADDRENV_RETURNED=SKIP")
        else:
            for (_, before), (name, after) in zip(stages, stages[1:]):
                check_addrenv_returned(before, after, name, strict=False)

            addrenv_ok = check_addrenv_returned(
                monstat_before, monstat_after, "SUITE") and addrenv_ok

        overall = (boot_ok and command_ok and smp_busy_ok and smp_scale_ok and
                   e0_ok and e1_ok and net_leak_ok and tlb_ok and addrenv_ok)
        print(f"TEST_RESULT={'PASS' if overall else 'FAIL'}")
        return 0 if overall else 1
    except (TimeoutError, serial.SerialTimeoutException) as error:
        print("CONSOLE_TIMEOUT=FAIL", file=sys.stderr)
        print(str(error), file=sys.stderr)
        return 1
    finally:
        port.rts = False
        port.dtr = False
        port.close()


if __name__ == "__main__":
    raise SystemExit(main())
