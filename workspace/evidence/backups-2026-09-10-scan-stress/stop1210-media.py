import importlib.util,time
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 for cmd,prompt in [('close 0',b'mediatool> '),('q',b'nsh> ')]:
  print('XTS_COMMAND='+cmd,flush=True)
  for byte in (cmd+'\n').encode():p.write(bytes([byte]));time.sleep(.003)
  end=time.monotonic()+30;out=b''
  while time.monotonic()<end:
   chunk=p.read(p.in_waiting or 1);out+=chunk
   if chunk:print(chunk.decode(errors='replace'),end='',flush=True)
   if prompt in out:break
  k.require(prompt in out,'cleanup prompt missing; no reset')
 k.command(p,'ps');k.command(p,'free')
