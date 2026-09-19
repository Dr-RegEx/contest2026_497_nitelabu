from pathlib import Path
import hashlib
import importlib.util
import re
import subprocess
import serial

ROOT = Path(__file__).resolve().parents[2]
transport_path = ROOT / "openvela-dev/nuttx/tools/espressif/esp32s31_nuttx_smoke.py"
spec = importlib.util.spec_from_file_location("transport", transport_path)
transport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transport)

receipt = ROOT / "backups/2026-09-10-scan-stress/build1057-flat-audio-final.sha256"
digest, image = receipt.read_text().strip().split(maxsplit=1)
assert hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest
busy = subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True)
assert busy.returncode == 1 and not busy.stderr.strip()

out = ROOT / "backups/2026-09-10-scan-stress/audio-playback1708"
out.mkdir()
log_path = out / "uart.log"
with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, log_path.open("x") as log:
    port.write(b"\r\n")
    boot = transport.collect_until_prompt(port, 30, "AUDIO_PLAYBACK1708")
    log.write(boot.decode(errors="replace"))
    assert b"nsh> " in boot
    result = transport.run_command(port, "cmocka_driver_audio -a 2 -p /data/xts-sequential.pcm", timeout=60)
    log.write("\nHOST_COMMAND cmocka_driver_audio -a 2 -p /data/xts-sequential.pcm\n" + result)
    assert "Start Playback." in result
    assert "[       OK ] drivertest_audio" in result
    assert re.search(r"\[  PASSED  \] 1 test\(s\)\.", result)
    assert not re.search(r"FAILED|ERROR|failed|nsh:", result, re.I)
    (out / "result.json").write_text('{"status":"PROGRAM_PASS","audio_acceptance":"LISTENING_REQUIRED","image_sha256":"' + digest + '"}\n')
    print(result, end="")
