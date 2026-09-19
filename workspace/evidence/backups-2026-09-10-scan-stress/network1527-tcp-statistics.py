"""One short TCP diagnosis with before/after stack counters; not an xTS rerun."""
import importlib.util,json,hashlib,subprocess
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'scope':'DIAGNOSTIC_ONLY','receipt':'build1493-wifi-rx-window.sha256','status':'FAIL'}
for line in (D/r['receipt']).read_text().splitlines():
 digest,name=line.split(maxsplit=1)
 k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
busy=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True)
k.require(busy.returncode==1 and not busy.stderr.strip(),'UART busy')
try:
 with k.existing_uart() as p:
  for command in ['ifconfig', 'cat /proc/net/tcp', 'iperf2 -c 192.168.1.29 -i 1 -p 5002 -t 10', 'ifconfig', 'cat /proc/net/tcp', 'free']:
   print('XTS_COMMAND='+command,flush=True)
   out=k.transport.run_command(p,command,timeout=75,reset_input=False)
   k.require(not any(x in out for x in ('connect failed', 'PANIC', 'Assertion failed', 'ERROR:')), 'command failed')
  r['status']='DIAGNOSTIC_RETURNED'
except Exception as e:r['error']=str(e)
(D/'network1527-tcp-statistics-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r),flush=True)
