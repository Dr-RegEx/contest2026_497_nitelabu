"""Diagnostic only: inspect tasks after reconnect and background DHCP."""
import importlib.util,time
from pathlib import Path
D=Path(__file__).resolve().parent;s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 for t in ['ifup wlan0','wapi reconnect wlan0','renew wlan0','ifconfig','wapi disconnect wlan0','wapi reconnect wlan0','renew wlan0 &']:
  k.transport.run_command(p,t,timeout=45,reset_input=False)
 time.sleep(15)
 for t in ['ps','ifconfig','free']:
  k.transport.run_command(p,t,timeout=20,reset_input=False)
