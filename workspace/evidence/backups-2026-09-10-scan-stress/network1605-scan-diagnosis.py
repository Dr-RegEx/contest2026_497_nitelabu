"""Three scans to identify current environmental variation; not xTS PASS."""
import importlib.util,json,hashlib,subprocess,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
s=importlib.util.spec_from_file_location('wifi',D/'xts-wifi-lifecycle.py');w=importlib.util.module_from_spec(s);s.loader.exec_module(w)
r={'scope':'DIAGNOSTIC_ONLY','receipt':'build1565-simd-int8.sha256','status':'FAIL','scans':[]}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=75,reset_input=False)
 rows,diag=w.split_scan_output(out)
 k.require(not re.search(r'Assertion failed|\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|ESP-ROM:|timed out',diag,re.I),'command failure: '+t)
 return out,rows
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 busy=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(busy.returncode==1 and not busy.stderr.strip(),'UART busy')
 with k.existing_uart() as p:
  for t in ['ifup wlan0','wapi reconnect wlan0','renew wlan0']:cmd(p,t)
  out,_=cmd(p,'ifconfig');k.require('RUNNING' in out and '192.168.1.' in out,'association missing')
  for i in range(3):
   out,rows=cmd(p,'wapi scan wlan0 '+k.transport.nsh_escape("Kal'tsit"));k.require(rows,'empty scan');r['scans'].append(rows)
  r['status']='DIAGNOSTIC_COMPLETE'
except Exception as e:r['error']=str(e)
(D/'network1605-scan-diagnosis-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r),flush=True)
