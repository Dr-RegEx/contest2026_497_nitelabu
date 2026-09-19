"""Run original4.1.113 HTTP file download and size comparison."""
import importlib.util,json,re,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'case':'4.1.113','status':'FAIL','receipt':'build1328-network-stack.sha256'}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=90,reset_input=False)
 k.require(not re.search(r'Assertion failed|\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|curl: \(\d+\)',out,re.I),'command failed: '+t)
 return out
try:
 for line in (D/r['receipt']).read_text().splitlines():
  digest,name=line.split(maxsplit=1)
  k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 source=Path('/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16/http-fixture/x1327.bin')
 k.require(source.stat().st_size==65536,'host fixture size mismatch')
 r['source_bytes']=source.stat().st_size
 r['source_sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
 with k.existing_uart() as p:
  mounts=cmd(p,'mount')
  k.require(not re.search(r' on /data ',mounts),'data already mounted; inspect before use')
  cmd(p,'mount -t tmpfs /data')
  cmd(p,'curl -o /data/x http://192.168.1.29:8000/x1327.bin')
  rc=cmd(p,'echo $?')
  k.require(re.search(r'(?m)^0\r?$',rc),'curl return code nonzero')
  listing=cmd(p,'ls -l /data/x')
  k.require(re.search(r'\b65536\b',listing),'download size mismatch')
  r['target_bytes']=65536
  r['status']='PASS'
except Exception as e:r['error']=str(e)
finally:
 (D/'network1331-http-download-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:600] if isinstance(value,str) else value) for key,value in r.items()}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
