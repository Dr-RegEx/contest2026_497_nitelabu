"""Original provisioning then4.1.26 show and4.1.25 sense twice; retain outputs."""
import getpass
import hashlib
import importlib.util
import json
from pathlib import Path
import re
D=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py')
k=importlib.util.module_from_spec(spec);spec.loader.exec_module(k)
result_path=D/'network1435-info-result.json'
k.require(not result_path.exists(),'result already exists')
receipt=D/'build1430-wapi-info.sha256'
for line in receipt.read_text().splitlines():
 digest,name=line.split(maxsplit=1)
 k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'receipt mismatch')
password=getpass.getpass('Test AP password (hidden): ')
k.require(8<=len(password)<=63,'invalid WPA2 passphrase length')
secrets=(k.transport.nsh_escape(password),password)
r={'status':'FAIL','receipt':str(receipt),'case':['4.1.26','4.1.25'],'captures':[]}
try:
 with k.existing_uart() as p:
  commands=['ifup wlan0','wapi mode wlan0 2','wapi psk wlan0 '+secrets[0]+' 3',
            'wapi essid wlan0 '+k.transport.nsh_escape("Kal'tsit")+' 1',
            'renew wlan0','ifconfig','wapi show wlan0','wapi sense wlan0','wapi sense wlan0']
  for command in commands:
   output=k.transport.run_command(p,command,timeout=90,redactions=secrets,reset_input=False)
   if command in ['ifconfig','wapi show wlan0','wapi sense wlan0']:
    r['captures'].append({'command':command,'output':k.transport.redact_text(output,secrets)})
   k.require(not re.search(r'Assertion failed|PANIC:|KASAN:|ESP-ROM:|\bERROR:|\bfailed\b|timed out|nsh:',output,re.I),'command failure')
   if command=='ifconfig':k.require('RUNNING' in output,'not associated')
  r['status']='CAPTURE_COMPLETE_REVIEW_REQUIRED'
except Exception as error:r['error']=k.transport.redact_text(str(error),secrets)
finally:
 result_path.write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:value for key,value in r.items() if key!='captures'}),flush=True)
raise SystemExit(0 if r['status']=='CAPTURE_COMPLETE_REVIEW_REQUIRED' else 1)
