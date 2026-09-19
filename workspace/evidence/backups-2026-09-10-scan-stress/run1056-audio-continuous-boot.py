from pathlib import Path
import sys,serial,subprocess,hashlib,re
D=Path(__file__).resolve().parent
sys.path.insert(0,str(D.parents[1]/'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as t
r=D/'build1054-flat-audio-continuous.sha256';h,n=r.read_text().strip().split(maxsplit=1);assert hashlib.sha256(Path(n).read_bytes()).hexdigest()==h
b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);assert b.returncode==1 and not b.stderr.strip()
print('XTS_RECEIPT='+str(r),flush=True)
with serial.Serial('/dev/ttyUSB0',115200,timeout=.1,write_timeout=1,exclusive=True) as p:
 t.hard_reset(p);boot=t.collect_until_prompt(p,30);print(boot.decode(errors='replace'),flush=True)
 assert b'NuttShell' in boot and not any(m in boot for m in t.BOOT_FAILURE_MARKERS)
 def cmd(s):
  o=t.run_command(p,s,timeout=20);assert not re.search(r'ERROR|failed|nsh:',o,re.I);return o
 assert '/data type' not in cmd('mount')
 cmd('ls /dev/audio');cmd('mkdir /data');cmd('mount -t tmpfs /data');cmd('free')
 print('AUDIO_BOOT_TMPFS=READY no_recording_started',flush=True)
