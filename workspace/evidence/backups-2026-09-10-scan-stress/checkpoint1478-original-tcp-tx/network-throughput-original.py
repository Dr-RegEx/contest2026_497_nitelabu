"""Original300-second TX commands; stream evidence, do not infer rate acceptance."""
import argparse,importlib.util,json,time,re,hashlib
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
a=argparse.ArgumentParser();a.add_argument('protocol',choices=['tcp','udp']);a.add_argument('--host',required=True);a.add_argument('--result',type=Path,required=True);args=a.parse_args()
k.require(not args.result.exists(),'result already exists')
portno=5002 if args.protocol=='tcp' else 5004
command=f'iperf2 -c {args.host} -i 1 -p {portno} -t 300'
if args.protocol=='udp':command+=' -u -b 40M'
k.transport.check_command_length(command)
r={'case':'5.1.23' if args.protocol=='tcp' else '5.1.25','command':command,'status':'FAIL','rate_acceptance':'pending criterion and both endpoint logs'}
text='';start=time.monotonic()
try:
 with k.existing_uart() as p:
  for byte in (command+'\r\n').encode():p.write(bytes([byte]));time.sleep(.001)
  deadline=start+390
  while time.monotonic()<deadline:
   if p.in_waiting:
    chunk=p.read(p.in_waiting).decode(errors='replace');text+=chunk;print(chunk,end='',flush=True)
    if re.search(r'nsh> ',text):break
   time.sleep(.02)
  else:raise RuntimeError('390s observation ended without shell prompt; preserve target, do not restart automatically')
  r['wall_seconds']=time.monotonic()-start
  summary=bool(re.search(r'0\.0+\s*-\s*30[0-9]\.\d+\s+sec',text))
  faults=bool(re.search(r'Assertion failed|Segmentation fault|PANIC:|KASAN:|nsh:.*(?:not found|failed|too many arguments)',text))
  r['full_duration_summary']=summary
  r['status']='EXECUTION_COMPLETE_RATE_PENDING' if summary and not faults else 'FAIL'
except Exception as e:r['error']=str(e)
r['uart_sha256']=hashlib.sha256(text.encode()).hexdigest()
args.result.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
raise SystemExit(0 if r['status']=='EXECUTION_COMPLETE_RATE_PENDING' else 1)
