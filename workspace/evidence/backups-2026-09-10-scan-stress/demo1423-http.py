"""Validate existing LAN demo on current1413 pair; not an xTS case."""
from pathlib import Path
import importlib.util,json,re,subprocess,hashlib
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'FAIL','receipt':'build1413-tx-backpressure.sha256','scope':'existing HTTP demo only; audio/BLE not integrated'}
try:
 for line in (D/r['receipt']).read_text().splitlines():
  h,n=line.split(maxsplit=1);k.require(hashlib.sha256(Path(n).read_bytes()).hexdigest()==h,'receipt mismatch')
 with k.existing_uart() as p:
  out=k.transport.run_command(p,'s31demo --status',timeout=20,reset_input=False)
  k.require('S31_DEMO_STATUS=OK' in out,'demo status failed')
  out=k.transport.run_command(p,'s31demo wlan0 &',timeout=20,reset_input=False)
  match=re.search(r's31demo \[([0-9]+):',out)
  k.require(match is not None,'server PID missing')
  r['server_pid']=int(match.group(1))
  for route,name in [('/', 'demo1423-page.html'),('/status.json','demo1423-status.json')]:
   response=subprocess.run(['/mnt/c/Windows/System32/curl.exe','--silent','--show-error','--fail','--noproxy','*','--max-time','20','http://192.168.1.60:8080'+route],capture_output=True,timeout=25)
   (D/name).write_bytes(response.stdout)
   k.require(response.returncode==0,'PC HTTP request failed: '+response.stderr.decode(errors='replace'))
  data=json.loads((D/'demo1423-status.json').read_text())
  k.require(data['board']=='ESP32-S31' and data['ipv4']=='192.168.1.60','JSON board identity mismatch')
  k.require('Function-CoreBoard-1' in (D/'demo1423-page.html').read_text(),'demo page identity missing')
  r['http_status']=data
  r['status']='PASS'
except Exception as e:r['error']=str(e)
finally:
 (D/'demo1423-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps(r),flush=True)
