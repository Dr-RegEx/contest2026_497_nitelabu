import importlib.util,json
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 for t in ['ifup wlan0','wapi reconnect wlan0','renew wlan0','ifconfig']:
  o=k.transport.run_command(p,t,timeout=75,reset_input=False)
  k.require(not any(m.decode() in o for m in k.transport.BOOT_FAILURE_MARKERS),'target fault')
  k.require('failed' not in o.lower(),'network setup failure')
 k.require('RUNNING' in o and '192.168.1.60' in o,'network missing')
print('NETWORK1624_READY',flush=True)
