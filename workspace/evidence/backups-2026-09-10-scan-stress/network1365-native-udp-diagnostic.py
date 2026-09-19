"""OriginalDIAGNOSTIC_ONLY: board UDP server, existing PC Linux iperf client300s."""
import importlib.util,json,re,time,subprocess,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'case':'DIAGNOSTIC_ONLY','status':'FAIL','receipt':'build1354-network-sendbuf.sha256','duration_seconds':5,'offered_load':'40M','host_topology':'Native Windows iperf2, direct WLAN; diagnostic5s not original300s acceptance'}
client=None;capture=''
try:
 with k.existing_uart() as p:
  out=k.transport.run_command(p,'ifconfig',timeout=20,reset_input=False)
  m=re.search(r'inet addr:(192\.168\.1\.\d+)',out);k.require(m is not None and 'RUNNING' in out,'connected board required');address=m.group(1)
  out=k.transport.run_command(p,'iperf2 -s -p 5003 -i 1 -u &',timeout=20,reset_input=False)
  m=re.search(r'iperf2 \[(\d+):',out);k.require(m is not None,'server PID missing');server_pid=int(m.group(1));r['server_pid']=server_pid
  hostfile=D/'logs/host1365-udp-rx.log';k.require(not hostfile.exists(),'host log exists')
  with hostfile.open('x') as log:
   client=subprocess.Popen(['/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16/iperf2.exe','-c',address,'-i','1','-p','5003','-t','5','-u','-b','40M'],stdout=log,stderr=subprocess.STDOUT)
   start=time.monotonic()
   while time.monotonic()-start<30:
    if p.in_waiting:
     chunk=p.read(p.in_waiting).decode(errors='replace');capture+=chunk;print(chunk,end='',flush=True)
    if client.poll() is not None:break
    time.sleep(.05)
   else:raise RuntimeError('host client30s diagnostic observation timeout; preserve target server')
   r['client_exit']=client.returncode;r['wall_seconds']=time.monotonic()-start
  # Allow the board to emit its final connection summary, without another run.
  deadline=time.monotonic()+3
  while time.monotonic()<deadline:
   if p.in_waiting:
    chunk=p.read(p.in_waiting).decode(errors='replace');capture+=chunk;print(chunk,end='',flush=True)
   time.sleep(.05)
  tasks=k.transport.run_command(p,'ps',timeout=20,reset_input=False)
  row=next((line for line in tasks.splitlines() if re.match(r'\s*'+str(server_pid)+r'\s',line)),None)
  if row and 'iperf2' in row:k.transport.run_command(p,'kill -15 '+str(server_pid),timeout=20,reset_input=False)
  host=hostfile.read_text()
  full=lambda text:bool(re.search(r'0\.0+\s*-\s*30[0-9]\.\d+\s+sec',text))
  r['host_full_duration']=full(host);r['board_summary_present']=bool(re.search(r'0\.0+\s*-\s*[1-9][0-9]*\.\d+\s+sec',capture))
  r['rate_acceptance']='Numeric standard not specified in original; actual rates retained for review'
  if client.returncode==0 and r['board_summary_present']:r['status']='DIAGNOSTIC_RECEIVED'
except Exception as e:r['error']=str(e)
finally:
 r['board_capture_sha256']=hashlib.sha256(capture.encode()).hexdigest()
 (D/'network1365-udp-rx-result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
raise SystemExit(0 if r['status']=='DIAGNOSTIC_RECEIVED' else 1)
