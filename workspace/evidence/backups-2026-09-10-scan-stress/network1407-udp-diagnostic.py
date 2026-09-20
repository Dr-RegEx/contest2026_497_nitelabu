"""10-second fault-localization run; never an original xTS PASS."""
from pathlib import Path
import importlib.util,json
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'status':'DIAGNOSTIC_ONLY','receipt':'build1403-poll-notify.sha256','duration':10,'xts_pass':False}
try:
 with k.existing_uart() as p:
  out=k.transport.run_command(p,'iperf2 -c 192.168.1.29 -i 1 -p 5004 -t 10 -u -b 40M',timeout=45,reset_input=False)
  r['returned_to_shell']=True
  r['queue_wait_marker']='S31 UDP wait:' in out
  r['pacing_marker']='S31 UDP pacing:' in out
  for c in ['ps','free']:k.transport.run_command(p,c,timeout=15,reset_input=False)
except Exception as e:r['error']=str(e)
(D/'network1407-udp-diagnostic-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r),flush=True)
