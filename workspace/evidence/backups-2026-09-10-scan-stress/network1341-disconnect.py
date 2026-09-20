"""Original4.1.31 public ping before/after disconnect, no retries."""
import importlib.util,json,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'case':'4.1.31','status':'FAIL','receipt':'build1332-network-dns.sha256'}
def cmd(p,t):return k.transport.run_command(p,t,timeout=90,reset_input=False)
try:
 with k.existing_uart() as p:
  cmd(p,'ifconfig')
  before=cmd(p,'ping www.baidu.com')
  k.require(re.search(r'\b(?:[1-9]|10) received',before),'public ping prerequisite failed')
  out=cmd(p,'wapi disconnect wlan0')
  k.require(not re.search(r'Assertion failed|\bERROR:|PANIC:|KASAN:',out),'disconnect command failed')
  after=cmd(p,'ping www.baidu.com')
  k.require(re.search(r'100% (?:packet )?loss|Network is unreachable|network unreachable|Network unreachable|Name or service not known|Failed to resolve|failed to resolve',after,re.I),'ping did not demonstrate loss of connectivity')
  k.require(not re.search(r'Assertion failed|PANIC:|KASAN:',after),'system failure')
  cmd(p,'ifconfig');r['status']='PASS'
except Exception as e:r['error']=str(e)
finally:
 (D/'network1341-disconnect-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:600] if isinstance(value,str) else value) for key,value in r.items()}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
