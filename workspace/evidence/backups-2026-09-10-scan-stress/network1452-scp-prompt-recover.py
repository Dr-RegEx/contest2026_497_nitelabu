import importlib.util
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 p.write(b'\n')
 out=k.transport.collect_until_prompt(p,15)
 print(out.decode(errors='replace'))
 k.require(b'nsh> ' in out,'shell not recovered')
