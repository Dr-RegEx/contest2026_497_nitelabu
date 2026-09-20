"""Original5.1.7 on1690 with verified working space; reversible fixture staging."""
from pathlib import Path
import importlib.util,hashlib,re,json
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'INCOMPLETE','receipt':'build1701-fs-worker.sha256','case':'5.1.7','staged':[],'restored':[]}
result=D/'fs1701-result.json';k.require(not result.exists(),'result exists')
for receipt in [r['receipt'],'fs1685-scratch-before.sha256']:
 for line in (D/receipt).read_text().splitlines():
  digest,name=line.split(maxsplit=1);k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
k.require('FS1701_MOUNT_ONLY_COMPLETE' in (D/'logs/mount1701-fs-worker.log').read_text(),'mount evidence missing')
k.transport.NSH_COMMAND_MAX=127
fixtures=[('/data/audio_file.aac','/tmp/fs1701-audio'),('/data/persist.db','/tmp/fs1701-db'),('/data/frag1164/Fragment_test_largefile','/tmp/fs1701-frag')]
try:
 with k.existing_uart() as p:
  def cmd(t,timeout=120):
   out=k.transport.run_command(p,t,timeout=timeout,reset_input=False);print(out,flush=True)
   k.require(not re.search(r'nsh:.*(?:failed|not found)|files differ|\b(?:ERROR|FAILED)\b',out),'command error')
   k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'fault')
   return out
  k.require('/data type littlefs' in cmd('mount'),'mount')
  k.require(not re.search(r'^\s*\d+.*(?:vela_fs_|kvdbd)',cmd('ps'),re.M),'active workload')
  safe_restore=True
  try:
   for src,tmp in fixtures:
    cmd('cp '+src+' '+tmp);cmd('cmp '+src+' '+tmp);cmd('rm '+src);r['staged'].append(src)
   df=cmd('df');m=re.search(r'4096\s+768\s+\d+\s+(\d+)\s+/data',df)
   k.require(m is not None and int(m[1])>=650,'insufficient working space')
   r['free_blocks_before']=int(m[1]);cmd('mkdir /data/s07_1701');cmd('cd /data/s07_1701')
   out=cmd('vela_fs_random_read_and_write_test writeCount=10 readCount=1000 mountPath=.',1800)
   for op,count in [('Read',1000),('Write',10)]:
    m=re.search(r'random'+op+r'Test : '+op.lower()+r' count='+str(count)+r', takes ([0-9.]+) seconds',out)
    k.require(m is not None,'missing result '+op);r[op.lower()+'_seconds']=float(m[1])
   tasks=cmd('ps')
   k.require('flash_io' in tasks,'dedicated Flash worker not observed')
   r['worker_observed']=True
   r['status']='EXECUTION_COMPLETE_PERFORMANCE_PENDING'
  except TimeoutError:
   safe_restore=False
   r['status']='OBSERVATION_TIMEOUT_PRESERVE_LIVE_WORKLOAD'
   raise
  finally:
   if safe_restore:
    cmd('cd /')
    for src,tmp in fixtures:
     if src in r['staged']:
      cmd('cp '+tmp+' '+src,180);cmd('cmp '+tmp+' '+src,120);r['restored'].append(src)
    cmd('df');cmd('free')
except Exception as e:r['error']=str(e)
result.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
