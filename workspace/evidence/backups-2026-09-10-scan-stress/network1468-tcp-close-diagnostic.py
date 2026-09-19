"""Ten-second TCP shutdown diagnostic; not an original xTS pass."""
import importlib.util,json
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'scope':'DIAGNOSTIC_ONLY','receipt':'build1461-tcp-shutdown-diag.sha256','status':'FAIL'}
try:
 with k.existing_uart() as p:
  k.transport.run_command(p,'ps',timeout=20,reset_input=False)
  out=k.transport.run_command(p,'iperf2 -c 192.168.1.29 -i 1 -p 5002 -t 10',timeout=60,reset_input=False)
  r['shutdown_lines']=[line for line in out.splitlines() if 'S31_TCP_SHUTDOWN' in line]
  r['status']='DIAGNOSTIC_RETURNED'
except Exception as e:r['error']=str(e)
(D/'network1468-tcp-close-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r),flush=True)
