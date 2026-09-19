import importlib.util,json,subprocess,hashlib,time
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'FAIL','scope':'DIAGNOSTIC_ONLY','receipt':'build1565-simd-int8.sha256','rounds':[]}
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(b.returncode==1 and not b.stderr.strip(),'UART busy')
 with k.existing_uart() as p:
  out=k.transport.run_command(p,'ifconfig',timeout=15,reset_input=False);k.require('RUNNING' in out,'network missing')
  for mode in (0,1):
   print('ACK_PROBE_MODE='+str(mode),flush=True)
   out=k.transport.run_command(p,'iperf2 -c 192.168.1.29 -i 1 -p 5002 -t 10',timeout=60,reset_input=False)
   k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'target fault')
   r['rounds'].append({'mode':mode,'output':out})
   k.require('connect failed' not in out and '0.00-1' in out,'TCP diagnostic did not transfer data')
   time.sleep(1)
  r['status']='COMMANDS_RETURNED_REVIEW_REQUIRED'
except Exception as e:r['error']=str(e)
(D/'network1608-ack-probe-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(r['status'],flush=True)
