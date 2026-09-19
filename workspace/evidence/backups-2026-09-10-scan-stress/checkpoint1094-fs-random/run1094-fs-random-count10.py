"""Published5.1.7 permits situational counts: read1000/write10, existing scratch, no reset/format."""
from pathlib import Path
import importlib.util, hashlib, re, subprocess
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r=D/'build1032-flat-category-fs-large.sha256';digest,name=r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'image mismatch')
cfg=(Path(name).parent/'.config').read_text();k.require('CONFIG_NSH_LINELEN=128\n' in cfg and 'CONFIG_ESP32S31_XTS_FLASH_LARGE=y\n' in cfg,'large profile')
k.check_mount_evidence((D/'checkpoint1092-fs-remount/mount-evidence.log').read_text(),r,digest)
b=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True);k.require(b.returncode==1 and not b.stderr.strip(),'busy UART')
# This receipted profile has 128 characters, unlike transport's older 64-byte profile.
k.transport.NSH_COMMAND_MAX=127
print('XTS_RECEIPT='+str(r),flush=True)
with k.existing_uart() as p:
 def cmd(x,t=120):
  out=k.command(p,x,t);k.require(not re.search(r'\b(fail|skip)\b',out,re.I),'test error');return out
 k.require('/data type littlefs' in cmd('mount'),'mount')
 k.require(not re.search(r'\bkvdbd\b|vela_fs_',cmd('ps')),'other workload')
 cmd('df');cmd('mkdir /data/s07c');cmd('cd /data/s07c')
 out=cmd('vela_fs_random_read_and_write_test writeCount=10 readCount=1000 mountPath=.',1800)
 for op,count in [('Read',1000),('Write',10)]:
  k.require(re.search(r'random'+op+r'Test : '+op.lower()+r' count='+str(count)+r', takes [0-9.]+ seconds',out),'missing count '+op)
 print('XTS_CASE=5.1.7 EXECUTION_COMPLETE original_program_read1000_write10_document_allowed_counts timings_in_log no_read_value_comparison',flush=True)
 cmd('cd /');cmd('ls -l /data/s07c');cmd('df')
