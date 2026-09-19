from pathlib import Path
import hashlib,json
root=Path('/home/regex/work/esp32s31-openvela')
d=Path(__file__).resolve().parent
p=root/'openvela-dev/external/ffmpeg/ffmpeg/libavfilter/af_asubgraph.c'
o=root/'openvela-dev/external/ffmpeg/ffmpeg/libavutil/opt.c'
s=p.read_text(); opt=o.read_text()
def fn(text,name):
    start=text.rfind('\n',0,text.index(name+'('))+1
    a=text.index('{',start); depth=1;b=a+1
    while depth:
        depth+=(text[b]=='{')-(text[b]=='}');b+=1
    return text[start:b]
def struct(name):
    a=s.index('typedef struct '+name+' {');b=s.index('} '+name+';',a)+len('} '+name+';')
    return s[a:b]
priv=struct('SubGraphPriv')
old=priv.replace('SubGraphPriv','OldSubGraphPriv').replace('    unsigned nb_outputs;\n','').replace('    int nb_inputs;','    int nb_inputs;\n    int nb_outputs;')
drain=fn(s,'asubgraph_drain_mix')
fragments='\n\n'.join([struct('SubGraphInstance'),struct('SubGraphFormats'),priv,old,fn(opt,'opt_array_pcount'),drain,drain.replace('asubgraph_drain_mix','old_drain_mix').replace('ret == AVERROR(EAGAIN)','ret = AVERROR(EAGAIN)')])
fragments += '\n\n' + fn(s,'asubgraph_output_frame')
(d/'actual-fragments.c').write_text(fragments+'\n')
(d/'manifest.json').write_text(json.dumps({str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest(),str(o.relative_to(root)):hashlib.sha256(o.read_bytes()).hexdigest()},indent=2)+'\n')
