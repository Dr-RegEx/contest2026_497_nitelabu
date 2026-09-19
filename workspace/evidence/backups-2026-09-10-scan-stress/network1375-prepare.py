"""Run original4.1.114 once after stack configuration repair."""
import importlib.util,json,re,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'case':'PREPARATION_ONLY','status':'FAIL','receipt':'build1354-network-sendbuf.sha256'}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=90,reset_input=False)
 k.require(not re.search(r'Assertion failed|\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|curl: \(\d+\)',out,re.I),'command failed: '+t)
 return out
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1)
  k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 with k.existing_uart() as p:
  for t in ['ifup wlan0','wapi reconnect wlan0','renew wlan0','ifconfig']:cmd(p,t)
  cmd(p,'iperf2 -v')
  cmd(p,'free')
  r['status']='PASS'
except Exception as e:r['error']=str(e)
finally:
 (D/'network1375-prepare-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:600] if isinstance(value,str) else value) for key,value in r.items()}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
