"""Execute original5.1.13 once on the isolated947 raw Flash device."""
from pathlib import Path
import hashlib,re,subprocess,sys
import serial
root=Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0,str(root/'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport
p=root/'backups/2026-09-10-scan-stress'
digest,name=(p/'build947-flat-flash-raw.sha256').read_text().strip().split(maxsplit=1)
image=Path(name)
assert hashlib.sha256(image.read_bytes()).hexdigest()==digest
config=(image.parent/'.config').read_text()
for option in ['BUILD_FLAT','ESP32S31_XTS_FLASH_RAW','ESP32S31_SPIFLASH_PSRAM_STACK']:
 assert 'CONFIG_'+option+'=y\n' in config
busy=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True)
assert busy.returncode==1 and not busy.stderr.strip()
print('IMAGE_SHA256='+digest,flush=True)
with serial.Serial('/dev/ttyUSB0',115200,timeout=.1,write_timeout=1,exclusive=True) as port:
 transport.hard_reset(port)
 boot=transport.collect_until_prompt(port,30).decode(errors='replace');print(boot,flush=True)
 assert 'NuttShell (NSH)' in boot and 'offset=0xc00000 size=0x100000' in boot
 assert not any(m.decode() in boot for m in transport.BOOT_FAILURE_MARKERS)
 def cmd(s):
  result=transport.run_command(port,s,timeout=120)
  assert not any(m.decode() in result for m in transport.BOOT_FAILURE_MARKERS)
  assert not re.search(r'ERROR|failed|nsh:|timed out|I/O error',result,re.I),s
  return result
 mounts=cmd('mount');assert '/data type' not in mounts and '/apps type' not in mounts
 assert 'xtsraw' in cmd('ls /dev/xtsraw')
 cmd('free')
 for direction,command in [('write','dd if=/dev/zero of=/dev/xtsraw bs=4096 count=64'),('read','dd if=/dev/xtsraw of=/dev/null bs=4096 count=64')]:
  result=cmd(command)
  stats=re.findall(r'262144bytes copied, (\d+) usec, (\d+) KB/s',result)
  assert len(stats)==1 and int(stats[0][0])>0, result
  print('XTS_5.1.13_'+direction.upper()+'=PASS bytes=262144 usec='+stats[0][0]+' KB_per_s='+stats[0][1],flush=True)
 cmd('free')
 print('XTS_5.1.13=PASS original_dd_bs4096_count64 no_speed_threshold',flush=True)
