import argparse,ast,contextlib,datetime,hashlib,io,json,pathlib,subprocess,sys,tempfile,traceback,types
import time as realtime
import _strptime
R=pathlib.Path('/home/regex/work/esp32s31-openvela');D=R/'backups/2026-09-10-scan-stress';OUT=pathlib.Path(tempfile.mkdtemp(prefix='s31-longrun-clock-'))
class Clock:
 def __init__(self,jump=False):self.elapsed=0.;self.jump=jump
 def time(self):return 1700000000+self.elapsed+(10800 if self.jump and self.elapsed>=3600 else 0)
 def monotonic(self):return 100+self.elapsed*1.05
 def sleep(self,n):self.elapsed+=max(n,0)
class Port:
 def __init__(self,c):self.c=c;self.queue=b'';self.cmd=b'';self.monitor=None;self.board_base=1700000000;self.commands=[];self.resets=0
 def __enter__(self):return self
 def __exit__(self,*a):pass
 @property
 def in_waiting(self):return len(self.queue)
 def read(self,n):
  if self.queue:out=self.queue[:n];self.queue=self.queue[n:];return out
  assert self.monitor is not None
  self.c.elapsed=max(self.c.elapsed,self.monitor)+60;self.monitor=self.c.elapsed
  return b'[CPU0] 528380 5652 522728 38784 507896 7 2 0.3%\r\n'
 def write(self,data):
  self.cmd+=data
  if b'\r' not in self.cmd:return len(data)
  cmd=self.cmd.split(b'\r')[0].decode();self.cmd=b'';self.commands.append(cmd)
  if cmd.startswith('date -u -s '):
   stamp=cmd.split('"')[1];self.board_base=datetime.datetime.strptime(stamp,'%b %d %H:%M:%S %Y').replace(tzinfo=datetime.timezone.utc).timestamp()-self.c.elapsed;reply='time set'
  elif cmd.startswith('date -u +'):reply=datetime.datetime.fromtimestamp(self.board_base+self.c.elapsed,datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%S')
  elif cmd=='showinfo -i 60 &':self.monitor=self.c.elapsed;reply='showinfo [7:100]'
  elif cmd=='ifconfig':reply='wlan0 at DOWN inet addr:0.0.0.0'
  elif cmd=='ps':reply='7 7 showinfo -i 60'
  elif cmd in ('ifdown wlan0','uname -a','free'):reply='OK'
  else:raise AssertionError('unexpected command '+cmd)
  self.queue+=(reply+'\r\nnsh> ').encode();return len(data)
def run(label,source,busy=False,jump=False):
 case=OUT/label;d=case/'backups/2026-09-10-scan-stress';(d/'logs').mkdir(parents=True)
 b=case/'firmware';b.mkdir();(b/'.config').write_text(''.join('CONFIG_'+n+'=y\n' for n in ('SMP','BUILD_KERNEL','MM_KASAN','MM_KASAN_INSTRUMENT_ALL','SYSTEM_RESMONITOR','SCHED_CPULOAD_SYSCLK','NETINIT_NETLOCAL'))+'CONFIG_FS_HEAPSIZE=0\n')
 for n in ('nuttx.bin','appfs.img'):(b/n).write_bytes(b'FAKE HOST FIXTURE')
 receipt=case/'receipt.sha256';receipt.write_text(''.join(hashlib.sha256((b/n).read_bytes()).hexdigest()+'  '+str(b/n)+'\n' for n in ('nuttx.bin','appfs.img')))
 c=Clock(jump);port=Port(c);opened=[]
 def serialopen(*a,**k):opened.append(True);return port
 def reset(p):p.resets+=1;p.queue=b'CPU1 SMP online\r\nNuttShell (NSH)\r\nnsh> '
 fake={'time':types.SimpleNamespace(**{**vars(realtime), 'time':c.time, 'monotonic':c.monotonic, 'sleep':c.sleep}),'serial':types.SimpleNamespace(Serial=serialopen),'esp32s31_nuttx_smoke':types.SimpleNamespace(hard_reset=reset),'subprocess':types.SimpleNamespace(run=lambda *a,**k:types.SimpleNamespace(returncode=0 if busy else 1,stderr=b''))}
 saved={n:sys.modules.get(n) for n in fake};sys.modules.update(fake)
 oldargv=sys.argv;sys.argv=['xts-longrun','--receipt',str(receipt),'--tag','xts999']
 tree=ast.parse(source.read_text())
 for node in tree.body:
  if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='ROOT' for t in node.targets):node.value=ast.Call(func=ast.Name(id='Path',ctx=ast.Load()),args=[ast.Constant(str(case))],keywords=[])
 ast.fix_missing_locations(tree);error=None
 with (case/'host-simulation.log').open('w') as log,contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
  try:exec(compile(tree,str(source),'exec'),{'__name__':'host_simulation'})
  except BaseException as e:error=repr(e)
 for n,mod in saved.items():
  if mod is None:sys.modules.pop(n,None)
  else:sys.modules[n]=mod
 sys.argv=oldargv
 statepath=d/'xts999-longrun.json';s=json.loads(statepath.read_text()) if statepath.exists() else None
 return {'label':label,'state':s,'error':error,'opened':bool(opened),'resets':port.resets,'commands':port.commands}
old=run('old_fast_monotonic',D/'checkpoint983-longrun-clock-fix/xts-longrun-before.py')
new=run('fixed_fast_monotonic',D/'xts-longrun.py')
busy=run('fixed_busy_uart',D/'xts-longrun.py',busy=True)
jump=run('fixed_forward_wall_step',D/'xts-longrun.py',jump=True)
assert old['error'] is None and old['state']['standby']=='PASS'
assert old['state']['samples'][2]['host_before']-old['state']['samples'][0]['host_after']<43200
assert new['error'] is None,new['error'];s=new['state'];assert len(s['samples'])==5 and s['standby']==s['time_consistency']=='PASS'
base=s['samples'][0]['host_after']
for i,item in enumerate(s['samples'][1:],1):assert item['host_before']-base>=i*21600
assert new['resets']==1 and len([x for x in new['commands'] if x.startswith('date -u +')])==5
assert busy['error'] and not busy['opened'] and not busy['commands']
assert jump['error'] and jump['state']['standby']!='PASS' and jump['state']['time_consistency']!='PASS'
summary={'status':'HOST_SIMULATION_PASS_ONLY','old_12h_label_actual_seconds':old['state']['samples'][2]['host_before']-old['state']['samples'][0]['host_after'],'new_scheduled_elapsed_seconds':[x['host_before']-base for x in s['samples'][1:]],'busy_uart_opened':busy['opened'],'wall_forward_step_rejected':bool(jump['error']),'real_target_access':False}
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(OUT);print(json.dumps(summary,indent=2))
