"""Original4.1.115, interactive SCP with verified host key and private password."""
import importlib.util,json,time,re,hashlib,subprocess
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
credentials=json.loads((D/'host-ssh-private/credentials.json').read_text())
secret=credentials['password']
fingerprint=subprocess.check_output([str(D/'host-ssh-venv/bin/python'),'-c','import asyncssh,sys; print(asyncssh.read_private_key(sys.argv[1]).get_fingerprint("sha256"))',str(D/'host-ssh-private/host-rsa')],text=True).strip()
r={'case':'4.1.115','status':'FAIL','receipt':'build1332-network-dns.sha256','host_fingerprint':fingerprint}
def safe(t):return t.replace(secret,'<redacted>')
def cmd(p,t):
 out=k.transport.run_command(p,t,timeout=30,reset_input=False)
 k.require(not re.search(r'Assertion failed|nsh:|PANIC:|KASAN:',out),'command failure: '+t)
 return out
try:
 with k.existing_uart() as p:
  cmd(p,'free');cmd(p,'mount')
  listing=cmd(p,'ls -l /data')
  k.require('scp1349' not in listing,'target already exists')
  command='scp xts@192.168.1.29:/x1348.bin /data/scp1349'
  k.transport.check_command_length(command)
  print('--- CMD: '+command+' ---',flush=True)
  for byte in (command+'\r\n').encode():p.write(bytes([byte]));time.sleep(.001)
  text='';sent=set();deadline=time.monotonic()+120
  while time.monotonic()<deadline:
   if p.in_waiting:
    text+=p.read(p.in_waiting).decode(errors='replace')
   if 'Do you trust the host key'<REDACTED_CREDENTIAL>'trust' not in sent:
    if fingerprint not in text:continue
    p.write(b'yes\n');sent.add('trust')
   if 'do you agree ?'<REDACTED_CREDENTIAL>'save' not in sent:
    p.write(b'\n');sent.add('save')
   if 'try a specific key? (y/n)'<REDACTED_CREDENTIAL>'key' not in sent:
    p.write(b'\n');sent.add('key')
   if 'Password: '<REDACTED_CREDENTIAL>'password' not in sent:
    k.require('trust' in sent,'password refused without verified peer')
    p.write((secret+'\n').encode());sent.add('password')
   if re.search(r'nsh> ',text):break
   time.sleep(.02)
  else:raise RuntimeError('SCP interactive timeout: '+safe(text))
  print(safe(text),flush=True)
  k.require(not re.search(r'Assertion failed|PANIC:|KASAN:|Error (?:opening|connecting|writing|reading)|Authentication failed|nsh:',text,re.I),'SCP error')
  k.require('password' in sent,'authentication not reached')
  rc=cmd(p,'echo $?');k.require(re.search(r'(?m)^0\r?$',rc),'nonzero SCP exit')
  listing=cmd(p,'ls -l /data/scp1349');k.require(re.search(r'\b65536\b',listing),'target size mismatch')
  r.update(status='PASS',source_bytes=65536,target_bytes=65536,verified_host=True)
except Exception as e:r['error']=safe(str(e))
finally:
 (D/'network1349-scp-download-result.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({key:(value[:600] if isinstance(value,str) else value) for key,value in r.items()}),flush=True)
raise SystemExit(0 if r['status']=='PASS' else 1)
