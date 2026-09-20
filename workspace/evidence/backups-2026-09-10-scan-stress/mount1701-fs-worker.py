"""Mount existing3MiB scratch, never format; capture capacity before original test."""
from pathlib import Path
import importlib.util,hashlib,re
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r=D/'build1701-fs-worker.sha256';digest,name=r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'firmware mismatch')
boot=(D/'logs/boot1701-fs-worker.log').read_text()
k.require(digest in boot and 'XTS_1701_BOOT=READY' in boot,'boot evidence missing')
print('XTS_RECEIPT='+str(r.resolve()),flush=True);print('IMAGE_SHA256='+digest,flush=True)
with k.existing_uart() as p:
 def cmd(t):
  out=k.transport.run_command(p,t,timeout=40,reset_input=False);print(out,flush=True)
  k.require(not re.search(r'nsh:.*(?:failed|not found)',out),'shell error')
  k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'fault')
  return out
 mounts=cmd('mount');k.require('/data type' not in mounts,'already mounted')
 cmd('ps');cmd('mkdir -p /data');cmd('mount -t littlefs /dev/xtsflash /data')
 k.require('/data type littlefs' in cmd('mount'),'mount failed')
 cmd('ls -l /data');cmd('df');cmd('free')
 print('FS1701_MOUNT_ONLY_COMPLETE',flush=True)
