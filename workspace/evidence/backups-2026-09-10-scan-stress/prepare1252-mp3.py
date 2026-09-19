import importlib.util
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('kv',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as port:
 k.command(port,'rm /data/audio_file.aac')
 k.command(port,'ls /data')
 k.command(port,'free')
print('Removed only archived AAC tmpfs copy; original host attachment unchanged',flush=True)
