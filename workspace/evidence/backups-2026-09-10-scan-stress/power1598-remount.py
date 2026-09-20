"""Recover original power_off_test01 after user-confirmed actual power loss; no reset."""
import importlib.util,hashlib,json,os,termios,tty,re,subprocess
from pathlib import Path
D=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(sp);sp.loader.exec_module(k)
r={'case':'4.1.6 preparation only','round':1,'status':'FAIL','user_message':'我拔快了','receipt':'build1023-flat-category-fs-name.sha256','host_reset':False,'formatted':False}
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
  k.command(p,'ls -l /data/p1583');k.command(p,'df');r['status']='MOUNT_READY_NO_TEST_STARTED'
except Exception as exc:r['error']=str(exc)
(D/'power1598-remount-result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False),flush=True)
raise SystemExit(0 if r['status']=='MOUNT_READY_NO_TEST_STARTED' else 1)
