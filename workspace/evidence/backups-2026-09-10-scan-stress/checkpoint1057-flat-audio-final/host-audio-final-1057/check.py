from pathlib import Path
import hashlib, subprocess
base = Path(__file__).resolve().parent
src = Path('/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c')
s = src.read_text()
enqueue = s[s.index('static int dup_enqueue('):s.index('static int dup_receive(')]
retire = s[s.index('          done = NULL;'):s.index('          nxmutex_lock(&g_lock);', s.index('          done = NULL;'))]
structs = s[s.index('struct dup_request_s'):s.index('struct dup_desc_s')]
pre = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#define OK 0
#define AUDIO_APB_FINAL 1
#define DUP_SLOTS 4
#define DUP_BLOCK 512
#define DUP_LIMIT 8
#define DUP_BYTES_PER_SECOND 176400
#define MSEC2TICK(x) (x)
struct i2s_dev_s { int unused; };
struct ap_buffer_s { size_t nbytes, nmaxbytes, curbyte; unsigned flags; uint8_t *samp; int refs, id; };
typedef void (*i2s_callback_t)(struct i2s_dev_s *, struct ap_buffer_s *, void *, int);
static int g_lock, g_wake, locked, ncalls, ids[8];
static void nxmutex_lock(int *p) { assert(!locked); locked=1; }
static void nxmutex_unlock(int *p) { assert(locked); locked=0; }
static void nxsem_post(int *p) {}
static clock_t clock_systime_ticks(void) { return 0; }
#define kmm_zalloc(n) calloc(1,n)
#define kmm_free(p) free(p)
static void apb_reference(struct ap_buffer_s *p) { p->refs++; }
static void apb_free(struct ap_buffer_s *p) { p->refs--; }
static void callback(struct i2s_dev_s *d, struct ap_buffer_s *p, void *a, int r)
{ assert(!locked); assert(r==0); ids[ncalls++]=p->id; }
'''
post = r'''
int main(void) {
 struct dup_endpoint_s ep={.active=true};
 uint8_t data[4096]={0};
 struct ap_buffer_s a={.nbytes=4096,.nmaxbytes=4096,.samp=data,.id=1};
 struct ap_buffer_s z={.nbytes=0,.nmaxbytes=4096,.samp=data,.id=2,.flags=AUDIO_APB_FINAL};
 struct ap_buffer_s bad=z; bad.flags=0;
 assert(dup_enqueue(&ep.dev,&bad,callback,NULL,0,false)==-EINVAL);
 assert(dup_enqueue(&ep.dev,&a,callback,NULL,0,false)==0);
 assert(dup_enqueue(&ep.dev,&z,callback,NULL,0,false)==0);
 assert(ncalls==0 && ep.pending==2 && ep.tail->bytes==0);
 retire(&ep); assert(ncalls==0 && ep.pending==2);
 ep.head->scheduled=4096; ep.head->completed=3584;
 retire(&ep); assert(ncalls==0 && ep.pending==2);
 ep.head->completed=4096;
 retire(&ep); assert(ncalls==2 && ids[0]==1 && ids[1]==2);
 assert(ep.pending==0 && ep.head==NULL && ep.tail==NULL);
 assert(a.refs==0 && z.refs==0);
 ncalls=0;
 assert(dup_enqueue(&ep.dev,&z,callback,NULL,0,false)==0);
 assert(ncalls==0); retire(&ep); assert(ncalls==1 && ids[0]==2);
 ep.rx=true; bad.nmaxbytes=0; bad.flags=AUDIO_APB_FINAL;
 assert(dup_enqueue(&ep.dev,&bad,callback,NULL,0,true)==-EINVAL);
 puts("PASS actual enqueue and worker retirement: empty TX FINAL is deferred, ordered after DMA completion, exactly once outside lock; zero nonfinal/RX rejected");
}
'''
code = pre + structs + enqueue + '\nstatic void retire(struct dup_endpoint_s *ep) {\nstruct dup_request_s *req,*done,*last; nxmutex_lock(&g_lock);\n' + retire + '\n}\n' + post
(base/'actual-final.c').write_text(code)
(base/'source.sha256').write_text(hashlib.sha256(src.read_bytes()).hexdigest()+'  '+str(src)+'\n')
subprocess.run(['gcc','-g','-fsanitize=address,undefined','-o',str(base/'check'),str(base/'actual-final.c')],check=True)
r = subprocess.run([str(base/'check')],capture_output=True,text=True,check=True)
(base/'result.log').write_text(r.stdout+r.stderr)
print(r.stdout)
