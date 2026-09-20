#!/usr/bin/env python3
"""Diagnostic only: try two extended advertising instances on BLE1725."""
import hashlib, importlib.util, json, re, subprocess, time
from pathlib import Path
import serial

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("transport", ROOT / "openvela-dev/nuttx/tools/espressif/esp32s31_nuttx_smoke.py")
transport = importlib.util.module_from_spec(spec); spec.loader.exec_module(transport)
receipt = ROOT / "backups/2026-09-10-scan-stress/build1725-ble-static.sha256"
assert all(hashlib.sha256(Path(p.split(maxsplit=1)[1]).read_bytes()).hexdigest() == p.split()[0] for p in receipt.read_text().splitlines())
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1
out = ROOT / "backups/2026-09-10-scan-stress/ble1742-ext-multi"; out.mkdir()
log_path = out / "uart.log"; records = []
with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1, exclusive=True) as port, log_path.open("x") as log:
    def rec(s): log.write(s); log.flush(); print(s, end="", flush=True)
    def shell(c):
        rec("\nHOST_COMMAND " + c + "\n"); s = transport.run_command(port, c, timeout=25); rec(s); return s
    def bt(c, pattern=None, timeout=35):
        rec("\nBTTOOL " + c + "\n"); port.write((c + "\r\n").encode()); s=""; end=time.monotonic()+timeout
        while time.monotonic()<end:
            b=port.read(port.in_waiting or 1)
            if b:
                p=b.decode(errors="replace"); s+=p; rec(p)
            if pattern and re.search(pattern,s): return s
            if pattern is None and "bttool> " in s: return s
        raise TimeoutError(s[-500:])
    transport.hard_reset(port); boot=transport.collect_until_prompt(port,30,"BLE1742"); rec(boot.decode(errors="replace"))
    for c in ("ls /dev/ttyHCI0", "mkdir /data", "mount -t tmpfs /data", "mkdir /data/misc", "mkdir /data/misc/bt"): shell(c)
    bt("bttool"); bt("enable", r"Adapter state changed:\s*2\b")
    for name, opt in (("ext1", ""), ("ext2", "-R random_id -O 01:02:03:04:05:06")):
        try:
            s=bt(f"adv start {opt} -i 160 -n {name} -m ext".replace("  "," "), r"on_advertising_start_cb, handle:.*adv_id:\d+, status:\d+")
            m=re.search(r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:(\d+), status:(\d+)",s)
            records.append({"name":name,"options":opt,"handle":m.group(1) if m else None,"adv_id":int(m.group(2)) if m else None,"status":int(m.group(3)) if m else None})
        except Exception as e: records.append({"name":name,"options":opt,"error":repr(e)})
    rec("\nPHONE_SCAN_READY case=ext-multi window_seconds=20\n"); time.sleep(20)
    for x in records:
        if x.get("handle"):
            try: bt("adv stop -h " + x["handle"], r"on_advertising_stopped_cb", timeout=20); x["stop_callback"]=True
            except Exception as e: x["stop_error"]=repr(e)
    try: bt("disable", r"Adapter state changed:\s*0\b", timeout=20)
    except Exception: pass
result={"status":"EXT_MULTI_DIAGNOSTIC","scope":"NOT_XTS_PASS","records":records,"phone_observation":"pending","uart_log":str(log_path)}
(out/"result.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print(json.dumps(result,ensure_ascii=False), flush=True)
