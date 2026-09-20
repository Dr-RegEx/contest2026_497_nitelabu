"""Original5.1.27: 100 specified-SSID scans after association."""
import importlib.util,json,re,time,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
def load(name,file):
 s=importlib.util.spec_from_file_location(name,D/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
k=load('kv','xts-kvdb-suite.py');w=load('wifi','xts-wifi-lifecycle.py')
r={'case':'5.1.27','status':'FAIL','receipt':'build1354-network-sendbuf.sha256','completed':0,'scans':[]}
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
  out,_=cmd(p,'ifconfig');k.require('RUNNING' in out and '192.168.1.60' in out,'associated DHCP prerequisite missing')
  scan_command='wapi scan wlan0 '+k.transport.nsh_escape("Kal'tsit")
  for i in range(1,101):
   out,rows=cmd(p,scan_command);k.require(rows,'empty scan result')
   k.require(all(row.split('\t',4)[4]=="Kal'tsit" for row in rows),'unexpected SSID returned')
   r['scans'].append(rows);r['completed']=i
   print('ASSOCIATED_SCAN_ROUND='+str(i)+' PASS',flush=True)
  r['list_variants']=len({tuple(rows) for rows in r['scans']})
  r['identity_variants']=len({tuple(sorted(tuple(row.split('\t')[j] for j in (0,1,3,4)) for row in rows)) for rows in r['scans']})
  out,_=cmd(p,'ifconfig');k.require('RUNNING' in out,'association lost')
  r['status']='PASS' if r['list_variants']==1 else 'EXECUTION_COMPLETE_VARIATION_REVIEW_REQUIRED'
except Exception as e:r['error']=str(e)
finally:
 r['elapsed_seconds']=time.monotonic()-start
 (D/'network1378-directed-scan-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:500] if isinstance(value,str) else value) for key,value in r.items() if key!='scans'}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
