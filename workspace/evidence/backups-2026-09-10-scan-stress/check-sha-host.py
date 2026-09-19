"""Exercise real S31 SHA buffering with original xTS vectors, no board access.

OpenSSL compression transforms replace only SHA peripheral operations. This
checks padding, streaming and state restore, not target registers or locking.
"""
from pathlib import Path
import subprocess
import argparse

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hmac',action='store_true')
args=parser.parse_args()

ROOT = Path('/home/regex/work/esp32s31-openvela')
OUT = ROOT / 'backups/2026-09-10-scan-stress/host-sha'
OUT.mkdir(exist_ok=True)
INCLUDE = OUT / 'include'
for name in ('nuttx/config.h', 'debug.h', 'nuttx/clock.h',
             'nuttx/crypto/crypto.h', 'nuttx/kmalloc.h', 'nuttx/mutex.h',
             'esp_crypto_lock.h', 'esp_private/periph_ctrl.h', 'hal/sha_ll.h'):
    path = INCLUDE / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('/* Host shim, no hardware access. */\n')
path = INCLUDE / 'crypto/cryptodev.h'
path.parent.mkdir(exist_ok=True)
path.write_text('#include "' + str(ROOT / 'openvela-dev/nuttx/include/crypto/cryptodev.h') + '"\n')
code = r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#define FAR
#define CODE
#define OK 0
#define CONFIG_TESTING_CRYPTO_HASH_HUGE_BLOCK 1
#define NXMUTEX_INITIALIZER 0
typedef int mutex_t;
static int nxmutex_lock(mutex_t *m) { (void)m; return 0; }
static void nxmutex_unlock(mutex_t *m) { (void)m; }
#define kmm_zalloc(n) calloc(1, n)
#define kmm_free(p) free(p)
#define MSEC2TICK(n) (n)
static uint32_t clock_systime_ticks(void) { return clock() * 1000 / CLOCKS_PER_SEC; }
#define PERIPH_RCC_ATOMIC() for (int once = 1; once; once = 0)
#define cryptinfo(...) ((void)0)
typedef enum {SHA1=0, SHA2_256=2, SHA2_512=4} esp_sha_type;
static esp_sha_type hw_mode;
static SHA_CTX c1;
static SHA256_CTX c256;
static SHA512_CTX c512;
static unsigned char hw_block[128];
static unsigned int hw_calls, comparisons;
static void esp_crypto_sha_aes_lock_acquire(void) {}
static void esp_crypto_sha_aes_lock_release(void) {}
static void sha_ll_enable_bus_clock(bool on) { (void)on; }
static void sha_ll_reset_register(void) {
  memset(&c1, 0, sizeof(c1)); memset(&c256, 0, sizeof(c256));
  memset(&c512, 0, sizeof(c512)); memset(hw_block, 0, sizeof(hw_block));
}
static void sha_ll_set_mode(esp_sha_type mode) { hw_mode=mode; }
static void sha_ll_fill_text_block(const void *p, size_t words) {
  assert((uintptr_t)p % 4 == 0);
  assert(words == (hw_mode == SHA2_512 ? 32 : 16));
  memcpy(hw_block, p, words * 4);
}
static void sha_ll_continue_block(esp_sha_type mode) {
  assert(mode == hw_mode);
  if (mode == SHA1) SHA1_Transform(&c1, hw_block);
  else if (mode == SHA2_256) SHA256_Transform(&c256, hw_block);
  else SHA512_Transform(&c512, hw_block);
  hw_calls++;
}
static void sha_ll_start_block(esp_sha_type mode) {
  if (mode == SHA1) SHA1_Init(&c1);
  else if (mode == SHA2_256) SHA256_Init(&c256);
  else SHA512_Init(&c512);
  sha_ll_continue_block(mode);
}
static bool sha_ll_busy(void) { return false; }
static uint64_t readbe(const unsigned char *p, int n) {
  uint64_t v=0; for (int i=0;i<n;i++) v=(v<<8)|p[i]; return v;
}
static void writebe(unsigned char *p, uint64_t v, int n) {
  for (int i=n-1;i>=0;i--) { p[i]=v; v >>= 8; }
}
static void sha_ll_read_digest(esp_sha_type mode, void *p, size_t words) {
  unsigned char *b=p;
  if (mode == SHA1) {
    assert(words == 5);
    uint32_t h[5]={c1.h0,c1.h1,c1.h2,c1.h3,c1.h4};
    for (int i=0;i<5;i++) writebe(b+4*i,h[i],4);
  } else if (mode == SHA2_256) {
    assert(words == 8);
    for (int i=0;i<8;i++) writebe(b+4*i,c256.h[i],4);
  } else {
    assert(words == 16);
    for (int i=0;i<8;i++) writebe(b+8*i,c512.h[i],8);
  }
}
static void sha_ll_write_digest(esp_sha_type mode, const void *p, size_t words) {
  const unsigned char *b=p;
  if (mode == SHA1) {
    assert(words == 5);
    c1.h0=readbe(b,4); c1.h1=readbe(b+4,4); c1.h2=readbe(b+8,4);
    c1.h3=readbe(b+12,4); c1.h4=readbe(b+16,4);
  } else if (mode == SHA2_256) {
    assert(words == 8);
    for (int i=0;i<8;i++) c256.h[i]=readbe(b+4*i,4);
  } else {
    assert(words == 16);
    for (int i=0;i<8;i++) c512.h[i]=readbe(b+8*i,8);
  }
}
'''
if args.hmac:
    code += '\n#define CONFIG_ESP32S31_CRYPTO_HMAC 1\n'
code += '\n#include "' + str(ROOT / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sha.c') + '"\n'
code += r'''
int crypto_get_driverid(uint8_t flags) { (void)flags; return 0; }
int crypto_register(uint32_t id, int *algs,
    int (*create)(uint32_t *, struct cryptoini *),
    int (*closefn)(uint64_t), int (*process)(struct cryptop *)) {
  (void)id; (void)algs; (void)create; (void)closefn; (void)process; return 0;
}
static uint32_t create(int alg) {
  uint32_t id; struct cryptoini ini={.cri_alg=alg};
  assert(s31_sha_new(&id,&ini)==0); return id;
}
static void update(uint32_t id, int alg, const void *input, size_t n) {
  struct cryptodesc d={.crd_alg=alg,.crd_len=n,.crd_flags=CRD_F_UPDATE};
  struct cryptop p={.crp_sid=id,.crp_desc=&d,.crp_buf=(void *)input,.crp_ilen=n};
  assert(s31_sha_process(&p)==0);
}
static void finish(uint32_t id, int alg, const void *expected, size_t n) {
  unsigned char out[64];
  struct cryptodesc d={.crd_alg=alg};
  struct cryptop p={.crp_sid=id,.crp_desc=&d,.crp_mac=(void *)out,.crp_olen=n};
  assert(s31_sha_process(&p)==0);
  if (memcmp(out,expected,n)) { fprintf(stderr,"digest mismatch alg=%d\n",alg); abort(); }
  assert(s31_sha_free(id)==0); comparisons++;
}
'''
source = (ROOT / 'openvela-dev/apps/testing/drivers/crypto/hash.c').read_text()
start = source.index('typedef struct tb\n')
end = source.index('\nstatic void syshash_free', start)
code += source[start:end] + '\n'
code += r'''
int main(void) {
  int algs[3]={CRYPTO_SHA1,CRYPTO_SHA2_256,CRYPTO_SHA2_512};
  unsigned char million[1000]; memset(million,'a',sizeof(million));
  for (int a=0;a<3;a++) {
    const tb *cases=a==2?g_sha512_testcase:g_sha_testcase;
    int size=a==0?20:a==1?32:64;
    for (int i=0;i<3;i++) {
      uint32_t id=create(algs[a]);
      if (i==2) for (int j=0;j<1000;j++) update(id,algs[a],million,1000);
      else update(id,algs[a],cases[i].data,cases[i].datalen);
      const void *result=a==0?(const void *)g_sha1_result[i]:
                         a==1?(const void *)g_sha256_result[i]:g_sha512_result[i];
      finish(id,algs[a],result,size);
    }
  }
  unsigned char *huge=malloc(600*1024); assert(huge); memset(huge,'a',600*1024);
  for (int a=0;a<3;a++) {
    uint32_t id=create(algs[a]); update(id,algs[a],huge,600*1024);
    const void *result=a==0?(const void *)g_sha1_huge_block_result:
                       a==1?(const void *)g_sha256_huge_block_result:g_sha512_huge_block_result;
    finish(id,algs[a],result,a==0?20:a==1?32:64);
  }
  free(huge);
  /* Interleave two different algorithms through the same reset peripheral.
   * Each one crosses a block and a partial update before finalization.
   */
  uint32_t id1=create(CRYPTO_SHA1),id2=create(CRYPTO_SHA2_512);
  unsigned char r1[20],r2[64];
  SHA1((const unsigned char *)million,150,r1);
  SHA512((const unsigned char *)million,257,r2);
  update(id1,CRYPTO_SHA1,million,65); update(id2,CRYPTO_SHA2_512,million,129);
  update(id1,CRYPTO_SHA1,million,85); update(id2,CRYPTO_SHA2_512,million,128);
  finish(id1,CRYPTO_SHA1,r1,20); finish(id2,CRYPTO_SHA2_512,r2,64);
  printf("HOST_SHA=PASS original_vectors=12 interleaved=2 comparisons=%u compression_calls=%u; not hardware evidence\n",comparisons,hw_calls);
  return 0;
}
'''
if args.hmac:
    source=(ROOT / 'openvela-dev/apps/testing/drivers/crypto/hmac.c').read_text()
    start=source.index('struct tb\n')
    end=source.index('\nstatic int syshmac',start)
    tables=source[start:end].replace('struct tb','struct hmac_tb').replace('g_','hm_')
    checks=r'''
static void check_hmac(void) {
  int algs[2]={CRYPTO_SHA1_HMAC,CRYPTO_SHA2_256_HMAC};
  for (int a=0;a<2;a++) for (unsigned int i=0;i<3;i++) {
    const struct hmac_tb *v=&hm_testcase[i];
    for (int split=0;split<2;split++) {
      uint32_t id;
      struct cryptoini ini={.cri_alg=algs[a],.cri_key=(void *)v->key,.cri_klen=v->keylen*8};
      assert(s31_sha_new(&id,&ini)==0);
      if (split) {
        update(id,algs[a],v->data,1);
        update(id,algs[a],v->data+1,v->datalen-1);
      } else update(id,algs[a],v->data,v->datalen);
      finish(id,algs[a],a==0?hm_sha1_result[i]:hm_sha256_result[i],a==0?20:32);
    }
  }
  unsigned char key[65]={0}; uint32_t id;
  struct cryptoini ini={.cri_alg=CRYPTO_SHA2_256_HMAC,.cri_key=key,.cri_klen=sizeof(key)*8};
  assert(s31_sha_new(&id,&ini)==-ENOTSUP);
  puts("HOST_HMAC_SHA=PASS original_vectors=6 single/split=12 long-key-rejection=1; SHA compression shim only");
}
'''
    code=code.replace('int main(void) {',tables+'\n'+checks+'\nint main(void) {\n  check_hmac();')
# Avoid collision with OpenSSL's one-shot SHA1 function name.
code = code.replace('SHA1=0', 'HW_SHA1=0').replace('mode == SHA1', 'mode == HW_SHA1')
code = code.replace('\n#include "' + str(ROOT / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sha.c'), '\n#define SHA1 HW_SHA1\n#include "' + str(ROOT / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sha.c'))
code = code.replace('\nint crypto_get_driverid', '\n#undef SHA1\nint crypto_get_driverid')
path = OUT / 'check.c'
path.write_text(code)
subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined','-Wno-deprecated-declarations','-I',str(INCLUDE),str(path),'-lcrypto','-o',str(OUT/'check')],check=True)
subprocess.run([str(OUT/'check')],check=True)
