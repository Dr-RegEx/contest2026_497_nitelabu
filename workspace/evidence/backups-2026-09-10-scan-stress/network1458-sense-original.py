"""Original4.1.25 two RSSI queries after1457; never execute concurrently."""
import hashlib,importlib.util,json,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
result_path=D/'network1458-sense-result.json'
k.require(not result_path.exists(),'result exists')
prior=D/'network1457-reconfigure100-result.json'
k.require(prior.exists(),'1457 still active or terminal state unavailable')
receipt=D/'build1455-wifi-state-origin.sha256'
for line in receipt.read_text().splitlines():
 digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'image mismatch')
r={'case':'4.1.25','status':'FAIL','receipt':receipt.name,'captures':[]}
try:
 with k.existing_uart() as p:
  out=k.transport.run_command(p,'ifconfig',timeout=30,reset_input=False)
  wlan=re.search(r'wlan0.*?(?=\n\w+\s|nsh>|\Z)',out,re.S)
  k.require(wlan is not None and 'RUNNING' in wlan.group(),'not associated')
  for _ in range(2):
   out=k.transport.run_command(p,'wapi sense wlan0',timeout=30,reset_input=False)
   r['captures'].append(out)
   k.require(not re.search(r'ERROR:|failed|nsh:|PANIC:|Assertion failed',out,re.I),'RSSI command error')
  r['status']='TWO_QUERIES_COMPLETE_REVIEW_REQUIRED'
except Exception as e:r['error']=str(e)
finally:
 result_path.write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({a:b for a,b in r.items() if a!='captures'}),flush=True)
raise SystemExit(0 if r['status']=='TWO_QUERIES_COMPLETE_REVIEW_REQUIRED' else 1)
