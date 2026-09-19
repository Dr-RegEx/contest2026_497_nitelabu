from pathlib import Path
import hashlib,json
src=Path('/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c')
out=Path('/home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/host-duplex-review')
s=src.read_text()
def function(name):
    start=s.index('static void '+name+'(')
    a=s.index('{',start); depth=1; b=a+1
    while depth:
        depth+=(s[b]=='{')-(s[b]=='}'); b+=1
    return s[start:b]
structs=s[s.index('struct dup_request_s'):s.index('static struct dup_endpoint_s g_ep')]
parts={name:function(name) for name in ['dup_stop_dma','dup_cancel','dup_service']}
preamble=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#define DUP_BLOCK 512
#define DUP_SLOTS 4
#define OK 0
#define DUP_RX_ERRORS 0x3c
#define DMA_DESCRIPTOR_BUFFER_OWNER_CPU 0
#define DMA_DESCRIPTOR_BUFFER_OWNER_DMA 1
struct i2s_dev_s { const void *ops; };
struct ap_buffer_s { uint8_t *samp; size_t curbyte,nbytes,nmaxbytes; };
typedef void (*i2s_callback_t)(struct i2s_dev_s *,struct ap_buffer_s *,void *,int);
/* Host-only descriptor/LL stand-ins: no claim about register or DMA timing. */
typedef struct dma_descriptor_s {
 struct { unsigned int size,length,suc_eof,err_eof,owner; } dw0;
 uint8_t *buffer;
 struct dma_descriptor_s *next;
} dma_descriptor_t;
static clock_t clock_systime_ticks(void) { return 10; }
#define DUP_DMA 0
#define ahb_dma_ll_rx_enable_interrupt(...) ((void)0)
#define ahb_dma_ll_tx_enable_interrupt(...) ((void)0)
#define ahb_dma_ll_rx_stop(...) ((void)0)
#define ahb_dma_ll_tx_stop(...) ((void)0)
#define ahb_dma_ll_rx_reset_channel(...) ((void)0)
#define ahb_dma_ll_tx_reset_channel(...) ((void)0)
#define i2s_ll_rx_stop(...) ((void)0)
#define i2s_ll_tx_stop(...) ((void)0)
#define esp32s31_audio_pa(...) ((void)0)
'''
globals=r'''
static struct dup_endpoint_s g_ep[2];
static struct dup_desc_s g_desc[2][DUP_SLOTS];
static uint8_t g_data[2][DUP_SLOTS][DUP_BLOCK];
static void clear_state(void) {
 memset(g_ep,0,sizeof(g_ep)); memset(g_desc,0,sizeof(g_desc)); memset(g_data,0,sizeof(g_data));
 for(unsigned d=0;d<2;d++) {
  g_ep[d].rx=d; g_ep[d].active=true; g_ep[d].dma_running=true;
  for(unsigned i=0;i<DUP_SLOTS;i++) g_desc[d][i].desc.dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
 }
}
static uint8_t sample(size_t index) { return (uint8_t)((index*17+index/251+23)%256); }
static void requests(struct dup_request_s *r, struct ap_buffer_s *a, uint8_t data[3][4224], unsigned dir) {
 memset(r,0,3*sizeof(*r)); memset(a,0,3*sizeof(*a));
 for(unsigned n=0;n<3;n++) {
  a[n].samp=data[n]; a[n].curbyte=dir?0:128; a[n].nbytes=a[n].curbyte+4096; a[n].nmaxbytes=4224;
  r[n].apb=&a[n];r[n].bytes=4096;r[n].next=n<2?&r[n+1]:NULL;
 }
 g_ep[dir].head=r;g_ep[dir].tail=&r[2];g_ep[dir].pending=3;
}
'''
tests=r'''
int main(void) {
 struct dup_request_s r[2][3];struct ap_buffer_s a[2][3];uint8_t data[2][3][4224];
 clear_state(); memset(data,0,sizeof(data)); requests(r[1],a[1],data[1],1);
 for(size_t block=0;block<24;block++) {
  unsigned slot=block%DUP_SLOTS;
  for(size_t k=0;k<DUP_BLOCK;k++) g_data[1][slot][k]=sample(block*DUP_BLOCK+k);
  g_desc[1][slot].desc.dw0.length=DUP_BLOCK;g_desc[1][slot].desc.dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
  dup_service(&g_ep[1]);
  assert(g_desc[1][slot].desc.dw0.owner==DMA_DESCRIPTOR_BUFFER_OWNER_DMA);
  for(unsigned n=0;n<3;n++) {
   size_t available=(block+1)*DUP_BLOCK;
   size_t expect=available<=n*4096?0:available-n*4096;if(expect>4096)expect=4096;
   assert(r[1][n].completed==expect);
  }
 }
 for(size_t n=0;n<3;n++)for(size_t k=0;k<4096;k++)assert(data[1][n][k]==sample(n*4096+k));
 puts("PASS actual dup_service RX: three 4096-byte requests, 24 descriptor completions, all bytes ordered");
 clear_state(); requests(r[0],a[0],data[0],0);
 for(size_t n=0;n<3;n++)for(size_t k=0;k<4096;k++)data[0][n][128+k]=sample(n*4096+k);
 for(unsigned slot=0;slot<DUP_SLOTS;slot++)g_desc[0][slot].desc.dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
 dup_service(&g_ep[0]);
 for(size_t block=0;block<24;block++) {
  unsigned slot=block%DUP_SLOTS;
  assert(g_desc[0][slot].desc.dw0.owner==DMA_DESCRIPTOR_BUFFER_OWNER_DMA);
  for(size_t k=0;k<DUP_BLOCK;k++)assert(g_data[0][slot][k]==sample(block*DUP_BLOCK+k));
  assert(g_ep[0].slotreq[slot]==&r[0][block/8]);
  g_desc[0][slot].desc.dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;dup_service(&g_ep[0]);
  for(unsigned n=0;n<3;n++) {
   size_t available=(block+1)*DUP_BLOCK;
   size_t expect=available<=n*4096?0:available-n*4096;if(expect>4096)expect=4096;
   assert(r[0][n].completed==expect);assert(r[0][n].scheduled<=4096);
  }
 }
 for(unsigned n=0;n<3;n++)assert(r[0][n].scheduled==4096&&r[0][n].completed==4096);
 for(unsigned slot=0;slot<DUP_SLOTS;slot++)assert(!g_ep[0].slotreq[slot]&&!g_ep[0].slotbytes[slot]);
 puts("PASS actual dup_service TX: three 4096-byte requests, curbyte offset, ordered ring payloads, exact final completions");
 for(unsigned stop=0;stop<2;stop++) {
  clear_state(); requests(r[0],a[0],data[0],0);requests(r[1],a[1],data[1],1);
  for(unsigned d=0;d<2;d++)for(unsigned slot=0;slot<DUP_SLOTS;slot++) {
   g_ep[d].slotreq[slot]=&r[d][slot%3];g_ep[d].slotbytes[slot]=512;
  }
  dup_cancel(&g_ep[stop],-ECANCELED);
  assert(!g_ep[stop].active&&g_ep[!stop].active&&g_ep[!stop].dma_running);
  assert(g_ep[1].dma_running); /* RX clocks continue while either direction is active. */
  for(unsigned n=0;n<3;n++) {
   assert(r[stop][n].result==-ECANCELED&&r[stop][n].completed==4096);
   assert(r[!stop][n].result==0&&r[!stop][n].completed==0);
  }
  for(unsigned slot=0;slot<DUP_SLOTS;slot++)assert(!g_ep[stop].slotreq[slot]&&!g_ep[stop].slotbytes[slot]);
  dup_cancel(&g_ep[!stop],-ECANCELED);assert(!g_ep[0].dma_running&&!g_ep[1].dma_running);
 }
 puts("PASS actual dup_cancel/dup_stop_dma: direction isolation, slot references cleared, RX clock state retained until both stop");
 puts("HOST LOGIC ONLY: not ISR/concurrency, real register, DMA timing, codec, callback dispatch, or board/xTS proof");
}
'''
(out/'duplex-source-logic.c').write_text(preamble+structs+globals+'\n'.join(parts.values())+tests)
(out/'source-manifest.json').write_text(json.dumps({'source':str(src),'sha256':hashlib.sha256(s.encode()).hexdigest(),'extracted_functions':list(parts),'structs':'dup_request_s, dup_endpoint_s, dup_desc_s verbatim','scope':'actual source functions; host ABI, descriptor and LL stand-ins; no firmware/hardware proof'},indent=2)+'\n')
(out/'actual-fragments.c').write_text(structs+'\n'.join(parts.values()))
