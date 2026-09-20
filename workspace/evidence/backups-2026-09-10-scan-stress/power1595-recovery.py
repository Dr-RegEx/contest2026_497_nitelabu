"""Recover original power_off_test02 after user-confirmed actual power loss; no reset."""
import importlib.util,hashlib,json,os,termios,tty,re,subprocess
from pathlib import Path
D=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(sp);sp.loader.exec_module(k)
r={'case':'4.1.5','round':3,'status':'FAIL','user_confirmation':'创建测试第3轮已断电并插回','receipt':'build1023-flat-category-fs-name.sha256','host_reset':False,'formatted':False}
try:
 digest,name=(D/r['receipt']).read_text().split();k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
 k.require(subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True).returncode==1,'UART busy')
 fd=os.open('/dev/ttyUSB0',os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
 try:
  tty.setraw(fd,termios.TCSANOW);a=termios.tcgetattr(fd);a[2]=(a[2]|termios.CLOCAL|termios.CREAD)&~termios.HUPCL;a[4]=a[5]=termios.B115200;termios.tcsetattr(fd,termios.TCSANOW,a)
 finally:os.close(fd)
 print('UART_LINE_CODING_ONLY; NO_DTR_RTS; NO_HOST_RESET',flush=True)
 with k.existing_uart() as p:
  p.write(b'\n');print(k.transport.collect_until_prompt(p,10).decode(errors='replace'),flush=True)
  mounts=k.command(p,'mount');k.require('/data type' not in mounts,'unexpected existing mount')
  k.command(p,'mkdir -p /data');k.command(p,'mount -t littlefs /dev/xtsflash /data')
  k.require('/data type littlefs' in k.command(p,'mount'),'mount missing')
  listing=k.command(p,'ls -l /data/p1583/testdir');r['persisted_created_files']=len(re.findall(r'\s\d+\.txt(?:\r?\n|$)',listing));k.require(r['persisted_created_files']>0,'missing created files')
  out=k.command(p,'power_off_test02 /data/p1583',120);k.require('Poweroff test open api passed' in out and 'The file system is functioning normally' in out and 'not functioning properly' not in out,'original recovery check not passed')
  listing=k.command(p,'ls -l /data/p1583');k.require('testdir' not in listing,'original cleanup incomplete')
  k.command(p,'df');r['status']='ROUND_PASS'
except Exception as exc:r['error']=str(exc)
(D/'power1595-recovery-result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False),flush=True)
raise SystemExit(0 if r['status']=='ROUND_PASS' else 1)
