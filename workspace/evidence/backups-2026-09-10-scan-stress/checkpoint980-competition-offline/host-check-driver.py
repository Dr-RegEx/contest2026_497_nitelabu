from pathlib import Path
import contextlib, hashlib, importlib.util, io, json, subprocess, sys, tempfile
root=Path('/home/regex/work/esp32s31-openvela')
out=Path(tempfile.mkdtemp(prefix='s31-offline-check-'))
(out/'nuttx').mkdir(); (out/'nuttx/config.h').write_text('#define CONFIG_EXAMPLES_S31DEMO_PORT 8080\n')
subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(out),str(root/'openvela-dev/apps/examples/s31demo/s31demo_main.c'),'-o',str(out/'s31demo')],check=True)
for interface,expected in [('lo',0),('missing_s31',1)]:
 r=subprocess.run(['strace','-e','trace=network,ioctl','-o',str(out/(interface+'.trace')),str(out/'s31demo'),'--status',interface],capture_output=True,text=True)
 (out/(interface+'.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==expected
 trace=(out/(interface+'.trace')).read_text()
 assert all(call not in trace for call in ('bind(', 'listen(', 'connect(', 'sendto(', 'sendmsg(', 'accept('))
 assert ('S31_DEMO_STATUS=OK' in r.stdout)==(expected==0)
sys.path.insert(0,str(root/'openvela-dev/nuttx/tools/espressif'))
spec=importlib.util.spec_from_file_location('demo',root/'openvela-dev/nuttx/tools/espressif/esp32s31_demo.py'); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
def forbidden(*args,**kwargs): raise AssertionError('offline path attempted networking/reset/credential prompt')
m.getpass.getpass=forbidden; m.transport.hard_reset=forbidden; m.transport.connect_sta=forbidden; m.production.boot=forbidden
@contextlib.contextmanager
def fake_uart(path): yield object()
m.offline_uart=fake_uart
firm=out/'firmware';firm.mkdir(); (firm/'.config').write_text('CONFIG_NETINIT_NETLOCAL=y\n# CONFIG_NETINIT_DHCPC is not set\n')
for name in ('nuttx.bin','appfs.img'):(firm/name).write_bytes(name.encode())
receipt=out/'receipt.sha256';receipt.write_text(''.join(hashlib.sha256((firm/n).read_bytes()).hexdigest()+'  '+str(firm/n)+'\n' for n in ('nuttx.bin','appfs.img')))
for bad_id in (False,True):
 commands=[]
 def fake_command(port,text,**kwargs):
  assert kwargs.get('reset_input') is False
  commands.append(text)
  if text=='s31demo --status':return 'S31_DEMO_STATUS=OK\r\n'
  if text=='s31led 1':return 'S31_LED=PASS frames=4 brightness=8 final=off optical=unverified\n'
  if '-rfd' in text:return 'READ Bus: 0 Addr: 18 Subaddr: fd Value: '+('00' if bad_id else '83')+'\n'
  if '-rfe' in text:return 'READ Bus: 0 Addr: 18 Subaddr: fe Value: 11\n'
  assert text in ('free','ps','df -h');return 'resource output\n'
 m.transport.run_command=fake_command
 sys.argv=['demo','--offline','--port','FAKE','--receipt',str(receipt),'--log',str(out/('bad.log' if bad_id else 'good.log'))]
 with contextlib.redirect_stdout(io.StringIO()): result=m.main()
 assert result==(1 if bad_id else 0)
 assert len(commands)==(3 if bad_id else 7)
(out/'summary.json').write_text(json.dumps({'status':'HOST_PASS_ONLY','cases':['actual C --status loopback and invalid interface','strace no network listener or traffic calls','offline Python no credential/reset/association path','bad codec ID stops presentation'],'target_run':False},indent=2)+'\n')
print(out)
