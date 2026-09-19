"""Original5.1.26: 100 surrounding-AP scans after association."""
import importlib.util,json,re,time,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
def load(name,file):
 s=importlib.util.spec_from_file_location(name,D/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
k=load('kv','xts-kvdb-suite.py');w=load('wifi','xts-wifi-lifecycle.py')
r={'case':'5.1.26','status':'FAIL','receipt':'build1332-network-dns.sha256','completed':0,'scans':[]}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=60,reset_input=False)
 rows,diag=w.split_scan_output(out)
 k.require(not re.search(r'Assertion failed|\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|ESP-ROM:|timed out',diag,re.I),'command failure: '+t)
 return out,rows
start=time.monotonic()
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 with k.existing_uart() as p:
  for t in ['ifup wlan0','wapi reconnect wlan0','renew wlan0','ifconfig']:cmd(p,t)
  for i in range(1,101):
   out,rows=cmd(p,'wapi scan wlan0');k.require(rows,'empty scan result')
   r['scans'].append(rows);r['completed']=i
   print('ASSOCIATED_SCAN_ROUND='+str(i)+' PASS',flush=True)
  r['list_variants']=len({tuple(rows) for rows in r['scans']})
  k.require(r['list_variants']>1,'no changing scan results observed')
  out,_=cmd(p,'ifconfig');k.require('RUNNING' in out,'association lost')
  r['status']='PASS'
except Exception as e:r['error']=str(e)
finally:
 r['elapsed_seconds']=time.monotonic()-start
 (D/'network1343-associated-scan-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:500] if isinstance(value,str) else value) for key,value in r.items() if key!='scans'}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
