from pathlib import Path
import re,hashlib,json
root=Path('/home/regex/work/esp32s31-openvela')
p=root/'backups/2026-09-10-scan-stress/host-media-caps-985'
paths=[root/'openvela-dev/nuttx/drivers/audio/es8311.c',root/'openvela-dev/apps/audioutils/alsa-lib/pcm_hw.c',root/'openvela-dev/nuttx/include/nuttx/audio/audio.h',root/'openvela-dev/nuttx/include/nuttx/audio/es8311.h',root/'openvela-dev/apps/audioutils/alsa-lib/include/alsa/pcm.h',root/'openvela-dev/nuttx/include/nuttx/fs/ioctl.h']
a,b,h,e,pcm,io=[x.read_text() for x in paths]
def func(s,name):
 m=re.search(r'static int '+name+r'\([^;]*?\)\s*\{',s); start=m.start(); i=m.end(); depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
f=func(a,'es8311_getcaps')
queries='\n'.join(func(b,'snd_pcm_hw_query_'+n) for n in ['format','channel','rate'])
names=set(re.findall(r'\bAUDIO_[A-Z0-9_]+',f+queries))|{'AUDIO_FMT_MP3'}
macros='\n'.join(line for line in h.splitlines() if re.match(r'#define\s+(\w+)',line) and re.match(r'#define\s+(\w+)',line)[1] in names)
macros+='\n'+next(x for x in h.splitlines() if x.startswith('#define AUDIOIOC_GETCAPS '))
for name in ['_IOC','_AUDIOIOCBASE','_AUDIOIOC']:
 macros+='\n'+next(x for x in io.splitlines() if re.match(r'#define '+name+r'(?:\(|\s)',x))
structs=h[h.index('struct audio_enc_wma_s'):h.index('struct audio_caps_desc_s')]
lower=e[e.index('struct es8311_lower_s'):e.index('/****************************************************************************',e.index('struct es8311_lower_s'))]
enum=re.search(r'typedef enum\s*\{[^{}]*SND_PCM_STREAM_PLAYBACK[^{}]*\}\s*snd_pcm_stream_t;',pcm)[0]
preamble='''#include <assert.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <stdio.h>\n#include <string.h>\n#include <errno.h>\n#define FAR\n#define DEBUGASSERT assert\n#define audinfo(...) ((void)0)\n#define CONFIG_ES8311_SHARED_DUPLEX 1\n'''
stub='''
/* Host routing shells only; capability structures/constants above are verbatim headers. */
struct audio_lowerhalf_s { void *ops; };
struct es8311_dev_s { struct audio_lowerhalf_s dev; const struct es8311_lower_s *lower; };
static struct es8311_lower_s lower[2]={{.capture=false},{.capture=true}};
static struct es8311_dev_s devs[2]={{.lower=&lower[0]},{.lower=&lower[1]}};
'''
route='''
static int host_ioctl(int fd, int cmd, unsigned long arg) {
 assert(fd==0||fd==1); assert(cmd==AUDIOIOC_GETCAPS);
 return es8311_getcaps(&devs[fd].dev,0,(struct audio_caps_s *)arg);
}
#define ioctl host_ioctl
'''
test=r'''
int main(void) {
 unsigned int values[32]; struct audio_caps_s caps;
 for(int capture=0;capture<2;capture++) {
  snd_pcm_stream_t stream=capture?SND_PCM_STREAM_CAPTURE:SND_PCM_STREAM_PLAYBACK;
  memset(values,0,sizeof(values));int ret=snd_pcm_hw_query_format(capture,values,32);
#ifdef CONFIG_ES8311_PCM_CAPS
  assert(ret==1&&values[0]==AUDIO_SUBFMT_PCM_S16_LE);
#else
  assert(ret==-EPERM);
#endif
  assert(snd_pcm_hw_query_channel(capture,stream,values,32)==2);
#ifdef CONFIG_ES8311_PCM_CAPS
  assert(values[0]==2&&values[1]==2);
#else
  assert(values[0]==1&&values[1]==2);
#endif
  assert(snd_pcm_hw_query_rate(capture,stream,values,32)==1&&values[0]==48000);
  memset(&caps,0,sizeof(caps));caps.ac_len=sizeof(caps);caps.ac_type=AUDIO_TYPE_QUERY;caps.ac_subtype=AUDIO_TYPE_QUERY;
  es8311_getcaps(&devs[capture].dev,0,&caps);
  assert(caps.ac_controls.b[0]==((capture?AUDIO_TYPE_INPUT:AUDIO_TYPE_OUTPUT)|AUDIO_TYPE_FEATURE));
  caps.ac_type=capture?AUDIO_TYPE_OUTPUT:AUDIO_TYPE_INPUT;caps.ac_subtype=AUDIO_TYPE_QUERY;
  es8311_getcaps(&devs[capture].dev,0,&caps);assert(caps.ac_channels==0&&caps.ac_controls.hw[0]==0);
  caps.ac_type=AUDIO_TYPE_QUERY;caps.ac_subtype=AUDIO_FMT_MP3;
  es8311_getcaps(&devs[capture].dev,0,&caps);assert(caps.ac_controls.b[0]==AUDIO_SUBFMT_END);
  caps.ac_subtype=AUDIO_FMT_PCM;es8311_getcaps(&devs[capture].dev,0,&caps);
#ifdef CONFIG_ES8311_PCM_CAPS
  assert(caps.ac_controls.b[0]==AUDIO_SUBFMT_PCM_S16_LE&&caps.ac_controls.b[1]==AUDIO_SUBFMT_END);
#else
  assert(caps.ac_controls.b[0]==AUDIO_SUBFMT_END);
#endif
 }
#ifdef CONFIG_ES8311_PCM_CAPS
 puts("PCM_CAPS ON: actual ALSA queries return only S16_LE, channels 2..2, rate 48000 for playback and capture; wrong direction empty; MP3 END; PCM list END");
#else
 puts("PCM_CAPS OFF: actual ALSA format query returns -EPERM; historical channels 1..2/rate48000 unchanged; wrong direction empty; MP3/PCM END");
#endif
 puts("HOST ONLY: actual extracted getcaps and ALSA query bodies; no hardware or media playback proof");
}
'''
(p/'caps-source-check.c').write_text(preamble+macros+'\n'+structs+lower+enum+stub+f+route+queries+test)
(p/'actual-functions.c').write_text(f+'\n'+queries)
(p/'source-manifest.json').write_text(json.dumps({str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths},indent=2)+'\n')
