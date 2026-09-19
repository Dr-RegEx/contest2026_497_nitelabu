"""Original4.1.117 login/list/get/put/quit with PC as FTP client."""
import importlib.util,json,re,hashlib
from ftplib import FTP
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'case':'4.1.117','status':'FAIL','receipt':'build1332-network-dns.sha256'}
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=90,reset_input=False)
 k.require(not re.search(r'Assertion failed|\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|curl: \(\d+\)',out,re.I),'command failed: '+t)
 return out
try:
 source=Path('/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16/http-fixture/x1327.bin')
 expected=hashlib.sha256(source.read_bytes()).hexdigest()
 with k.existing_uart() as p:
  info=cmd(p,'ifconfig')
  address=re.search(r'inet addr:(192\.168\.1\.\d+)',info).group(1)
  mounts=cmd(p,'mount');k.require(not re.search(r' on /data ',mounts),'data already mounted')
  cmd(p,'mount -t tmpfs /data')
  cmd(p,'curl -o /data/x http://192.168.1.29:8000/x1327.bin')
  cmd(p,'ftpd_start -4 &')
  with FTP(timeout=60) as ftp:
   print(ftp.connect(address,21),flush=True)
   print(ftp.login('ftp',''),flush=True)
   ftp.set_pasv(True);print(ftp.cwd('/data'),flush=True);ftp.retrlines('LIST')
   with (D/'ftp1335-download.bin').open('xb') as f:print(ftp.retrbinary('RETR x',f.write),flush=True)
   with source.open('rb') as f:print(ftp.storbinary('STOR upload1335.bin',f),flush=True)
   with (D/'ftp1335-readback.bin').open('xb') as f:print(ftp.retrbinary('RETR upload1335.bin',f.write),flush=True)
   print(ftp.quit(),flush=True)
  cmd(p,'ls -l /data')
  for name in ['ftp1335-download.bin','ftp1335-readback.bin']:
   data=(D/name).read_bytes();k.require(len(data)==65536 and hashlib.sha256(data).hexdigest()==expected,'FTP content mismatch')
  r.update(status='PASS',bytes_per_direction=65536,sha256=expected)
except Exception as e:r['error']=str(e)
finally:
 (D/'network1335-ftpd-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:600] if isinstance(value,str) else value) for key,value in r.items()}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
