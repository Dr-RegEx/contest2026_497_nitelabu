import importlib.util
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 for t in ['ifconfig','wapi show wlan0','ping -c 3 192.168.1.1','ping -c 3 192.168.1.29','arp -a','free']:
  k.transport.run_command(p,t,timeout=35,reset_input=False)
