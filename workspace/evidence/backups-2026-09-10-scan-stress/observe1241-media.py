import importlib.util,time
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
print('PASSIVE_OBSERVATION_ONLY',flush=True)
with k.existing_uart() as p:
 end=time.monotonic()+15
 while time.monotonic()<end:
  chunk=p.read(p.in_waiting or 1)
  if chunk:print(chunk.decode(errors='replace'),end='',flush=True)
