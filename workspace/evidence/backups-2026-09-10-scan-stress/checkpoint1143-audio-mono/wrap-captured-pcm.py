"""Wrap verified exported PCM without resampling or normalization."""
import argparse,array,hashlib,json,math,wave
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('wav',type=Path);a=p.parse_args()
r=json.loads((a.capture/'result.json').read_text());assert r['status']=='TRANSFER_COMPLETE'
fmt=r['recording_format'];assert fmt['bits']==16 and fmt['requested_capture_seconds']==10
channels,rate=fmt['channels'],fmt['rate_hz'];assert (channels,rate) in ((2,44100),(1,16000))
raw=Path(r['local_file']).read_bytes();assert hashlib.sha256(raw).hexdigest()==r['local_sha256'] and len(raw)%(channels*2)==0
assert not a.wav.exists()
with wave.open(str(a.wav),'wb') as w:
 w.setparams((channels,2,rate,0,'NONE','not compressed'));w.writeframes(raw)
v=array.array('h',raw);stats=[]
for ch in (v[i::channels] for i in range(channels)):
 avg=sum(ch)/len(ch);stats.append({'min':min(ch),'max':max(ch),'mean':avg,'ac_rms':math.sqrt(sum((x-avg)**2 for x in ch)/len(ch)),'nonzero_samples':sum(x!=0 for x in ch),'sample_count':len(ch)})
review={'duration_seconds':len(raw)/(rate*channels*2),'channels':stats,'wav_sha256':hashlib.sha256(a.wav.read_bytes()).hexdigest(),'sample_bytes_unchanged':True,'human_listening':'PENDING'}
(a.capture/'pcm-review.json').write_text(json.dumps(review,indent=2)+'\n');print(json.dumps(review,indent=2))
