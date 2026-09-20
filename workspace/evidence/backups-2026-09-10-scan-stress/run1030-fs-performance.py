"""Published four performance modes and dd on existing isolated scratch."""
from pathlib import Path
import hashlib,importlib.util,re,subprocess,math
D=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(sp);sp.loader.exec_module(k)
r=D/'build1023-flat-category-fs-name.sha256';digest,name=r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'image mismatch')
k.check_mount_evidence((D/'checkpoint1027-fs-name-mount/mount-evidence.log').read_text(),r,digest)
b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(b.returncode==1 and not b.stderr.strip(),'UART occupied')
print('XTS_RECEIPT='+str(r),flush=True)
with k.existing_uart() as p:
 def cmd(x,t=120):
  out=k.command(p,x,t);k.require(not re.search(r'\bfail\b',out,re.I),'operation failed');return out
 cmd('cd /');k.require('/data type littlefs' in cmd('mount'),'mount')
 k.require(not re.search(r'\bkvdbd\b|vela_fs_',cmd('ps')),'active workload')
 cmd('df');cmd('mkdir /data/s11')
 for mode,label in enumerate(['write','read','random write','random read'],1):
  # getopt permits attached values; this stays within NSH_MAXARGUMENTS=7.
  out=cmd(f'performance_test -d/data/s11 -b4096 -c64 -m{mode}',600)
  match=re.search(label+r' speed is (\S+)',out);k.require(match is not None,'missing rate')
  rate=float(match[1]);k.require(math.isfinite(rate) and rate>0,'invalid rate')
  print(f'XTS_PERF_MODE={mode} rate_KiB_s={rate}',flush=True)
 listing=cmd('ls -l /data/s11');k.require(re.search(r'\b262144\s+performance_test',listing),'wrong size')
 print('XTS_CASE=5.1.11 MEASURED modes=1,2,3,4 bytes=262144 no_private_speed_threshold',flush=True)
 cmd('df');cmd('mkdir /data/s12')
 for x in ['dd if=/dev/zero of=/data/s12/payload bs=4096 count=64','dd if=/data/s12/payload of=/dev/null bs=4096 count=64']:
  out=cmd(x);k.require(re.search(r'262144bytes copied, [1-9][0-9]* usec, [1-9][0-9]* KB/s',out),'missing bytes/rate')
 k.require(re.search(r'\b262144\s+payload',cmd('ls -l /data/s12')),'dd size')
 print('XTS_CASE=5.1.12 MEASURED bytes=262144 no_private_speed_threshold',flush=True)
 cmd('df')
