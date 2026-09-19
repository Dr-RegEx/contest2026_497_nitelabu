"""Recovery evidence only; no reset, clock setting, flash or automatic PASS."""
import os,select,time,json,re,fcntl
from pathlib import Path
from datetime import datetime,timezone
D=Path(__file__).resolve().parent
original=json.loads((D/'xts864-longrun.json').read_text())
first=original['samples'][0]
end=first['host_after']+86405
state={'status':'RECOVERY_RUNNING','original_tag':'xts864','gap':'Original UART log ended 2026-09-16 06:33:23+08; resumed after WSL restart around08:12; no continuous-log claim','samples':[],'automatic_acceptance':False}
state=json.loads((D/'xts864-recovery.json').read_text())
state['status']='RECOVERY_RUNNING'
state.pop('error',None)
state['schedule']='Only remaining original18h and24h checks; no hourly sampling'
state['host_logger_restart_reason']='User requested original6h cadence only; board not reset'
targets=[first['host_after']+64800, end]
def save():
 p=D/'xts864-recovery.json';tmp=p.with_suffix('.tmp');tmp.write_text(json.dumps(state,indent=2)+'\n');tmp.replace(p)
f=os.open('/dev/ttyUSB0',os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
try:
 with (D/'logs/xts864-recovery-uart.log').open('a',buffering=1) as log:
  rolling='';next_sample=targets.pop(0)
  log.write('\nHOST_SCHEDULE_UPDATE remaining original18h and24h only; no reset/time setting\n')
  save()
  while True:
   if select.select([f],[],[],.2)[0]:
    b=os.read(f,65536)
    if not b:raise RuntimeError('serial disconnected')
    text=b.decode(errors='replace');log.write(text);rolling=(rolling+text)[-16384:]
    if re.search(r'ESP-ROM:|Assertion failed|S31SM:M-TRAP|kasan_report:|Segmentation fault',rolling):raise RuntimeError('board reset or fault')
   now=time.time()
   if now>=next_sample:
    log.write('\nHOST_DATE_BEFORE='+str(now)+'\n')
    os.write(f,b'date -u\r');out='';deadline=time.monotonic()+15
    before=now
    while time.monotonic()<deadline:
     if select.select([f],[],[],.2)[0]:
      text=os.read(f,8192).decode(errors='replace');log.write(text);out+=text
      if 'nsh> ' in out:break
    after=time.time()
    match=re.search(r'(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun),\s+\w+\s+\d+\s+\d+:\d+:\d+\s+\d{4}',out)
    if not match:raise RuntimeError('date response missing')
    board=datetime.strptime(match[0],'%a, %b %d %H:%M:%S %Y').replace(tzinfo=timezone.utc).timestamp()
    state['samples'].append({'host_before':before,'host_after':after,'board_utc':datetime.fromtimestamp(board,timezone.utc).isoformat(),'error_seconds_interval':[board-after,board+1-before],'elapsed_host_seconds':before-first['host_after']})
    save();print(json.dumps(state['samples'][-1]),flush=True)
    if before>=end:
     state['status']='COMPLETE_REVIEW_REQUIRED';save();break
    next_sample=targets.pop(0)
except BaseException as e:
 state['status']='INTERRUPTED';state['error']=repr(e);save();raise
finally:os.close(f)
