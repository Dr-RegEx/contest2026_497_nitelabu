from pathlib import Path
import json,shlex,subprocess,re
r=Path('/home/regex/work/esp32s31-openvela')
d=r/'backups/2026-09-10-scan-stress/es8311-kernel-worker-review'
b=r/'openvela-dev/out/esp32s31-xts-flat-audio-final'
src=r/'openvela-dev/nuttx/drivers/audio/es8311.c'
e=next(e for e in json.loads((b/'compile_commands.json').read_text()) if e['file'].endswith('/drivers/audio/es8311.c'))
a=shlex.split(e['command']);a=a[:a.index('-o')]
flat=(b/'include/nuttx/config.h').read_text()
kernel=(r/'openvela-dev/out/esp32s31-wifi1078/include/nuttx/config.h').read_text()
audio='\n'.join(x for x in flat.splitlines() if re.match(r'#define CONFIG_(AUDIO|ES8311|ESP32S31_I2S|I2S)',x))
# Inject only existing audio driver options into the real kernel config.
kernel += '\n'+audio+'\n'
results={}
for mode,cfg in [('flat',flat),('kernel',kernel),('kernel-nonshared',kernel+'\n#undef CONFIG_ES8311_SHARED_DUPLEX\n')]:
 inc=d/mode/'include/nuttx';inc.mkdir(parents=True,exist_ok=True);(inc/'config.h').write_text(cfg)
 cmd=[a[0],'-I'+str(inc.parent),'-I'+str(src.parent)]+a[1:]
 q=subprocess.run(cmd+['-c',str(src),'-o',str(d/(mode+'.o'))],cwd=e['directory'],capture_output=True,text=True)
 (d/(mode+'-syntax.log')).write_text(q.stdout+q.stderr)
 results[mode+'_compile']=q.returncode
 assert q.returncode==0,q.stderr
 if mode=='flat':
  outputs=[]
  for name,file in [('before',d/'es8311.c.before'),('after',src)]:
   q=subprocess.run(cmd+['-E','-P','-x','c',str(file)],cwd=e['directory'],capture_output=True,text=True);assert q.returncode==0,q.stderr
   text=q.stdout
   # Only source locations and insignificant preprocessing whitespace differ.
   text=re.sub(r'"[^"\n]*es8311\.c(?:\.before)?"','"es8311.c"',text)
   text=re.sub(r'(__assert\([^;]*?),\s*\d+\s*,',r'\1, LINE,',text)
   text=re.sub(r'\s+','',text)
   text=text.replace('(priv)->threadid','priv->threadid')
   outputs.append(text)
  results['flat_preprocessed_identical']=outputs[0]==outputs[1]
  if outputs[0]!=outputs[1]:
   import difflib
   (d/'flat-diff.txt').write_text('\n'.join(difflib.unified_diff(outputs[0].split(';'),outputs[1].split(';'))))
(d/'check-result.json').write_text(json.dumps(results,indent=2)+'\n')
print(results)
