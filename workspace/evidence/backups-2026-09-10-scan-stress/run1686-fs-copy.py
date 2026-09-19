"""Original random read/write on1684; temporarily stage archived audio fixture in RAM."""
from pathlib import Path
import importlib.util,hashlib,re,json
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'INCOMPLETE','receipt':'build1684-fs-copy.sha256','case':'5.1.7','audio_restore':'not moved'}
k.require(not (D/'fs1686-result.json').exists(),'result exists')
for receipt in [r['receipt'],'fs1685-scratch-before.sha256']:
 for line in (D/receipt).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
k.require('FS1685_MOUNT_ONLY_COMPLETE' in (D/'logs/mount1685-fs-copy.log').read_text(),'mount missing')
k.transport.NSH_COMMAND_MAX=127
try:
 with k.existing_uart() as p:
  def cmd(t,timeout=60):
   out=k.transport.run_command(p,t,timeout=timeout,reset_input=False);print(out,flush=True)
   k.require(not re.search(r'nsh:.*(?:failed|not found)|\b(?:ERROR|FAILED)\b',out),'command error')
   k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'fault')
   return out
  k.require('/data type littlefs' in cmd('mount'),'mount')
  k.require(not re.search(r'^\s*\d+.*(?:vela_fs_|kvdbd)',cmd('ps'),re.M),'active workload')
  cmd('cp /data/audio_file.aac /tmp/fs1686-audio.aac')
  cmd('cmp /data/audio_file.aac /tmp/fs1686-audio.aac')
  cmd('rm /data/audio_file.aac');r['audio_restore']='pending; RAM copy /tmp/fs1686-audio.aac plus whole volume backup'
  cmd('df');cmd('mkdir /data/s07_1686');cmd('cd /data/s07_1686')
  out=cmd('vela_fs_random_read_and_write_test writeCount=10 readCount=1000 mountPath=.',1800)
  for op,count in [('Read',1000),('Write',10)]:
   m=re.search(r'random'+op+r'Test : '+op.lower()+r' count='+str(count)+r', takes ([0-9.]+) seconds',out)
   k.require(m is not None,'missing result '+op);r[op.lower()+'_seconds']=float(m[1])
  cmd('cd /');cmd('ls -l /data/s07_1686')
  cmd('cp /tmp/fs1686-audio.aac /data/audio_file.aac',180)
  cmd('cmp /tmp/fs1686-audio.aac /data/audio_file.aac',120)
  r['audio_restore']='restored; byte comparison complete'
  cmd('rm /tmp/fs1686-audio.aac');cmd('df');cmd('free')
  r['status']='EXECUTION_COMPLETE_PERFORMANCE_PENDING'
except Exception as e:r['error']=str(e)
(D/'fs1686-result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
