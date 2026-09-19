from pathlib import Path
import json,shlex,subprocess,re
D=Path(__file__).resolve().parent
R=D.parent.parent.parent
N=R/'openvela-dev/nuttx'
B=R/'openvela-dev/out/esp32s31-xts-flat-media-volume1254'
flat=(B/'include/nuttx/config.h').read_text()
entries=json.loads((B/'compile_commands.json').read_text())
kernel=(R/'openvela-dev/out/esp32s31-ble-kernel1565/include/nuttx/config.h').read_text()+'\n'+'\n'.join(x for x in flat.splitlines() if re.match(r'#define CONFIG_(AUDIO|ES8311|ESP32S31_I2S|I2S)',x))+'\n'
results={}
for mode,cfg in [('flat1254',flat),('kernel1254',kernel)]:
 inc=D/mode/'include/nuttx';inc.mkdir(parents=True,exist_ok=True);(inc/'config.h').write_text(cfg)
 for file in ['drivers/audio/es8311.c','arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c']:
  e=next(x for x in entries if x['file'].endswith('/'+file));a=shlex.split(e['command']);a=a[:a.index('-o')]
  cmd=[a[0],'-I'+str(inc.parent)]+a[1:]+['-c',str(N/file),'-o',str(D/(mode+'-'+Path(file).stem+'.o'))]
  q=subprocess.run(cmd,cwd=e['directory'],capture_output=True,text=True)
  (D/(mode+'-'+Path(file).stem+'.log')).write_text(q.stdout+q.stderr)
  results[mode+'-'+Path(file).stem]=q.returncode
(D/'result1254.json').write_text(json.dumps(results,indent=2)+'\n')
print(results)
assert not any(results.values())
