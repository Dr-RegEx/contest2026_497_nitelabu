import importlib.util,json,subprocess,hashlib,time,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'FAIL','scope':'DIAGNOSTIC_ONLY','receipt':'build1619-spawn-parent-env.sha256','samples':[]}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=40,reset_input=False)
 k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'target fault')
 k.require(not re.search(r'nsh:.*(?:failed|not found)|Could not open',out),'command failed: '+t)
 return out
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(b.returncode==1 and not b.stderr.strip(),'UART busy')
 with k.existing_uart() as p:
  r['preflight_tcp']=cmd(p,'cat /proc/net/tcp')
  r['preflight_iob']=cmd(p,'cat /proc/iobinfo')
  r['network']=cmd(p,'ifconfig');k.require('RUNNING' in r['network'],'not associated')
  r['launch']=cmd(p,'iperf2 -c 192.168.1.29 -i 1 -p 5002 -t 20 > /tmp/tcp1620.log &')
  for i in range(8):
   time.sleep(1)
   r['samples'].append({'host_monotonic':time.monotonic(),'tcp':cmd(p,'cat /proc/net/tcp'),'iob':cmd(p,'cat /proc/iobinfo'),'stats':cmd(p,'ifconfig')})
  r['ps']=cmd(p,'ps')
  r['output']=cmd(p,'cat /tmp/tcp1620.log')
  r['status']='QUEUE_SAMPLED_REVIEW_REQUIRED'
except Exception as e:r['error']=str(e)
(D/'network1620-queue-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(r['status'],flush=True)
raise SystemExit(0 if r['status']=='QUEUE_SAMPLED_REVIEW_REQUIRED' else 1)
