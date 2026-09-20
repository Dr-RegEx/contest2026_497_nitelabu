"""Bounded regression for1430 BSSID getter; preserve the prior saved profile."""
import importlib.util,json,re,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'FAIL','case':'REGRESSION_ONLY','receipt':'build1430-wapi-info.sha256','original_profile_restored':False}
profile='/apps/s31-wapi-xts.conf';backup='/apps/s31-wapi-before1436.conf'
for line in (D/r['receipt']).read_text().splitlines():
 h,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==h,'receipt mismatch')
def command(p,t):
 out=k.transport.run_command(p,t,timeout=90,reset_input=False)
 k.require(not re.search(r'\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|ESP-ROM:|timed out',out,re.I),'command failure')
 return out
try:
 with k.existing_uart() as p:
  listing=command(p,'ls /apps')
  k.require('s31-wapi-before1436.conf' not in listing,'backup exists')
  k.require('s31-wapi-xts.conf' in listing,'original profile missing')
  command(p,'cp '+profile+' '+backup)
  try:
   command(p,'wapi save_config wlan0')
   command(p,'wapi reconnect wlan0')
   command(p,'renew wlan0')
   state=command(p,'ifconfig');k.require('RUNNING' in state,'association missing')
   out=command(p,'ping 192.168.1.1')
   k.require(re.search(r'\b10 received',out),'gateway ping incomplete')
   r['status']='PASS'
  finally:
   command(p,'cp '+backup+' '+profile)
   r['original_profile_restored']=True
except Exception as e:r['status']='FAIL';r['error']=str(e)
finally:
 (D/'network1436-save-reconnect-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps(r),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
