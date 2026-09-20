from pathlib import Path
import subprocess,json,shlex,re
D=Path(__file__).resolve().parent
R=D.parent.parent.parent
N=R/'openvela-dev/nuttx'
def function(text,name):
 a=text.index(name+'(');a=text.rfind('\n',0,a)+1;b=text.index('{',a);n=1;e=b+1
 while n:
  n+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
prefix=r'''
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#define CONFIG_MM_TASK_HEAP 1
#define FAR
#define DEBUGASSERT assert
#define MAX(a,b) ((a)>(b)?(a):(b))
#define up_get_dcache_linesize() 16
#define audinfo(...) ((void)0)
struct mm_heap_s {int id;} heaps[2]={{1},{2}};
struct mm_heap_s *current=&heaps[0];
#define USR_HEAP current
struct ap_buffer_s {struct {int channels;} i; int crefs,nmaxbytes,nbytes,flags,lock; uint8_t *samp; struct mm_heap_s *heap;};
struct audio_buf_desc_s {size_t numbytes; union {struct ap_buffer_s **pbuffer;struct ap_buffer_s *buffer;} u;};
union header {max_align_t align;struct {struct mm_heap_s *heap;} info;};
int wrong_heap, live;
static void *allocate(size_t size){union header *h=malloc(sizeof(*h)+size); assert(h);h->info.heap=current;live++;return h+1;}
static void mm_free(struct mm_heap_s *heap,void *p){if(!p)return;union header *h=(union header *)p-1;wrong_heap+=h->info.heap!=heap;live--;free(h);}
#define lib_umalloc(n) allocate(n)
#define lib_umemalign(a,n) allocate(n)
#define lib_ufree(p) mm_free(current,p)
#define nxmutex_init(p) (*(p)=0)
#define nxmutex_destroy(p) ((void)0)
#define nxmutex_lock(p) ((void)0)
#define nxmutex_unlock(p) ((void)0)
'''
suffix=r'''
int main(void){struct ap_buffer_s *apb;struct audio_buf_desc_s d={.numbytes=512,.u.pbuffer=&apb};
assert(apb_alloc(&d)>0);assert(live==2);apb->crefs=2;
apb_free(apb);assert(live==2);
current=&heaps[1];apb_free(apb);assert(live==0);
return wrong_heap?10:0;}
'''
for mode,path,expected in [('before',D/'lib_buffer.c.before',10),('after',N/'libs/libc/audio/lib_buffer.c',0)]:
 text=path.read_text();c=D/(mode+'.c');exe=D/(mode+'.elf');c.write_text(prefix+function(text,'apb_alloc')+function(text,'apb_free')+suffix)
 subprocess.run(['cc','-std=c11','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
 q=subprocess.run([str(exe)]);print(mode,q.returncode,'expected',expected,flush=True);assert q.returncode==expected
build=R/'openvela-dev/out/esp32s31-xts-flat-audio-final'
e=next(x for x in json.loads((build/'compile_commands.json').read_text()) if x['file'].endswith('/audio/lib_buffer.c'))
a=shlex.split(e['command']);a=a[:a.index('-o')]
flat=(build/'include/nuttx/config.h').read_text();kernel=(R/'openvela-dev/out/esp32s31-ble-kernel1565/include/nuttx/config.h').read_text()+'\n#define CONFIG_AUDIO 1\n'
for mode,cfg in [('flat',flat),('kernel',kernel)]:
 inc=D/mode/'include/nuttx';inc.mkdir(parents=True,exist_ok=True);(inc/'config.h').write_text(cfg)
 q=subprocess.run([a[0],'-I'+str(inc.parent)]+a[1:]+['-c',str(N/'libs/libc/audio/lib_buffer.c'),'-o',str(D/(mode+'.o'))],cwd=e['directory'],text=True,capture_output=True)
 (D/(mode+'.log')).write_text(q.stdout+q.stderr);print(mode,'RV32 compile',q.returncode,flush=True);assert q.returncode==0,q.stderr
