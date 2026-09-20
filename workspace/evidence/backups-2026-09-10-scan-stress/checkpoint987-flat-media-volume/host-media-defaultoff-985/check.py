from pathlib import Path
import json,shlex,subprocess,tempfile,shutil,difflib
r=Path('/home/regex/work/esp32s31-openvela');b=r/'openvela-dev/out/esp32s31-xts-flat-audio';out=Path(tempfile.mkdtemp(prefix='s31-codec-defaultoff-'))
(out/'include/nuttx').mkdir(parents=True);shutil.copy2(b/'include/nuttx/config.h',out/'include/nuttx/config.h')
assert 'define CONFIG_ES8311_SHARED_DUPLEX' not in (out/'include/nuttx/config.h').read_text()
c=json.loads((b/'compile_commands.json').read_text());e=next(e for e in c if e['file'].endswith('/drivers/audio/es8311.c'))
a=shlex.split(e['command']);a=a[:a.index('-o')]
paths=[r/'backups/2026-09-10-scan-stress/checkpoint968-flat-audio/source/openvela-dev/nuttx/drivers/audio/es8311.c',r/'openvela-dev/nuttx/drivers/audio/es8311.c']
for label,src in zip(('old','new'),paths):
 cmd=[a[0],'-I'+str(out/'include'),'-I'+str(r/'openvela-dev/nuttx/drivers/audio')]+a[1:]+['-E','-P','-fmacro-prefix-map='+str(src)+'=drivers/audio/es8311.c',str(src)]
 q=subprocess.run(cmd,cwd=e['directory'],capture_output=True,text=True);(out/(label+'.stderr')).write_text(q.stderr);assert q.returncode==0,q.stderr
 (out/(label+'.i')).write_text(q.stdout)
diff=''.join(difflib.unified_diff((out/'old.i').read_text().splitlines(True),(out/'new.i').read_text().splitlines(True),fromfile='968-defaultoff',tofile='current-defaultoff'))
(out/'comparison.diff').write_text(diff);print(out);print(diff[:7000])
