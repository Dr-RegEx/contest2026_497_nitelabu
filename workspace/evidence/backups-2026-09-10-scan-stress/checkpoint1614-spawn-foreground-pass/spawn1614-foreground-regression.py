import importlib.util,json,subprocess,hashlib,time,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'FAIL','scope':'SPAWN_REDIRECTION_REGRESSION','receipt':'build1612-spawn-parent-env.sha256'}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=40,reset_input=False)
 k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'target fault')
 return out
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(b.returncode==1 and not b.stderr.strip(),'UART busy')
 with k.existing_uart() as p:
  r['launch']=cmd(p,'s31simd > /tmp/simd1614.log')
  k.require('failed' not in r['launch'].lower(),'background launch failed')
  time.sleep(3)
  r['redirected_output']=cmd(p,'cat /tmp/simd1614.log')
  k.require('SIMD arithmetic and full-bank sleep-switch: PASS' in r['redirected_output'],'SIMD regression not complete')
  r['open_failure']=cmd(p,'s31simd > /missing1612/out &')
  k.require('failed' in r['open_failure'].lower() or 'No such' in r['open_failure'],'missing path failure not reported')
  r['after_failure']=cmd(p,'s31simd')
  k.require('SIMD arithmetic and full-bank sleep-switch: PASS' in r['after_failure'],'post-error spawn regression failed')
  r['cleanup']=cmd(p,'rm /tmp/simd1614.log')
  r['free']=cmd(p,'free')
  r['status']='PASS'
except Exception as e:r['error']=str(e)
(D/'spawn1614-regression-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(r['status'],flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
