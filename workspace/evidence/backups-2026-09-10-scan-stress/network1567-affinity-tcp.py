"""Short network regression after the SMP affinity fix; not original xTS."""
import importlib.util,json,hashlib,subprocess,re
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
r={'scope':'DIAGNOSTIC_ONLY','receipt':'build1565-simd-int8.sha256','status':'FAIL'}
for line in (D/r['receipt']).read_text().splitlines():
 digest,name=line.split(maxsplit=1)
 k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
busy=subprocess.run(['fuser','/dev/ttyUSB0'],capture_output=True)
k.require(busy.returncode==1 and not busy.stderr.strip(),'UART busy')
def command(p,cmd,strict=True):
 out=k.transport.run_command(p,cmd,timeout=75,reset_input=False)
 k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),'target fault')
 if strict:k.require(not re.search(r'\bERROR:|\bfailed\b|nsh:',out,re.I),'command failure')
 return out
try:
 with k.existing_uart() as p:
  for cmd in ['ifup wlan0','wapi reconnect wlan0','renew wlan0','ifconfig']:
   command(p,cmd)
  before=command(p,'ping -c 3 192.168.1.1')
  k.require('3 packets transmitted, 3 received, 0% packet loss' in before,'gateway not ready')
  host=subprocess.run(['/mnt/c/Windows/System32/ping.exe','-n','3','-w','1500','192.168.1.60'],capture_output=True,timeout=20)
  (D/'logs/host1567-peer-ping.log').write_bytes(host.stdout+host.stderr)
  r['host_ping_returncode']=host.returncode
  peer=command(p,'ping -c 3 192.168.1.29',False)
  r['board_peer_ping']='3 packets transmitted, 3 received, 0% packet loss' in peer
  command(p,'arp -i wlan0 -a 192.168.1.29',False)
  out=command(p,'iperf2 -c 192.168.1.29 -i 1 -p 5002 -t 10')
  k.require(re.search(r'0\.00-1[0-9]\.\d+ sec.*(?:KBytes|MBytes)',out),'missing TCP total')
  r['status']='TCP_SHORT_COMPLETED'
  command(p,'free')
except Exception as e:r['error']=str(e)
(D/'network1567-affinity-tcp-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r),flush=True)
raise SystemExit(0 if r['status']=='TCP_SHORT_COMPLETED' else 1)
