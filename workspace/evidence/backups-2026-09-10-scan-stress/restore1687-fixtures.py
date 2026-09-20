from pathlib import Path
import importlib.util,re,json
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('k',D/'xts-kvdb-suite.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
with k.existing_uart() as p:
 def cmd(t,timeout=120):
  out=k.transport.run_command(p,t,timeout=timeout,reset_input=False);print(out,flush=True)
  k.require(not re.search(r'nsh:.*(?:failed|not found)|files differ|\b(?:ERROR|FAILED)\b',out),'command failed')
  return out
 cmd('cd /');cmd('ls -l /data/s07_1687');cmd('df')
 cmd('cp /tmp/fs1687-persist.db /data/persist.db')
 cmd('cmp /tmp/fs1687-persist.db /data/persist.db')
 cmd('cp /tmp/fs1687-audio.aac /data/audio_file.aac')
 cmd('cmp /tmp/fs1687-audio.aac /data/audio_file.aac')
 cmd('ls -l /data/audio_file.aac');cmd('df')
 print('AUDIO1687_RESTORED_COMPARE_OK',flush=True)
r=json.loads((D/'fs1687-result.json').read_text());r['audio_restore']='audio and persist.db restored and byte compared by restore1687-audio.py; RAM copy retained';r['review']='original random write stopped on errno28 ENOSPC; no performance conclusion';(D/'fs1687-restoration.json').write_text(json.dumps(r,indent=2)+'\n')
