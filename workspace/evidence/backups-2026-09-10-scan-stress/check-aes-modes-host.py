"""Check actual S31 mode code against the original xTS vector tables.

Only the AES PIO primitive is replaced by host OpenSSL ECB. This checks mode
and streaming logic, not S31 hardware, interrupts, locks or timing. AES-192
vectors remain explicitly unsupported by this hardware backend.
"""
from pathlib import Path
import subprocess

ROOT = Path('/home/regex/work/esp32s31-openvela')
OUT = ROOT / 'backups/2026-09-10-scan-stress/host-aes-modes'
OUT.mkdir(exist_ok=True)
INCLUDE = OUT / 'include'
for name in ('nuttx/config.h', 'debug.h', 'nuttx/clock.h',
             'nuttx/crypto/crypto.h', 'nuttx/kmalloc.h', 'nuttx/mutex.h',
             'esp_crypto_lock.h', 'esp_private/periph_ctrl.h', 'hal/aes_ll.h'):
    path = INCLUDE / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('/* Host shim; no target hardware access. */\n')
path = INCLUDE / 'crypto/cryptodev.h'
path.parent.mkdir(exist_ok=True)
path.write_text('#include "' + str(ROOT / 'openvela-dev/nuttx/include/crypto/cryptodev.h') + '"\n')

shim = r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#define FAR
#define CODE
#define OK 0
#define CONFIG_ESP32S31_CRYPTO_AES_MODES 1
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
#define ESP_AES_ENCRYPT 1
#define ESP_AES_DECRYPT 0
#define ESP_AES_STATE_IDLE 0
static unsigned char hw_key[32], hw_block[16];
static int hw_bytes, hw_encrypt;
static unsigned int hw_calls;
static void esp_crypto_sha_aes_lock_acquire(void) {}
static void esp_crypto_sha_aes_lock_release(void) {}
static void aes_ll_enable_bus_clock(bool on) { (void)on; }
static void aes_ll_reset_register(void) { memset(hw_key, 0, sizeof(hw_key)); }
static void aes_ll_dma_enable(bool on) { assert(!on); }
static void aes_ll_set_mode(int mode, int bytes) { hw_encrypt=mode; hw_bytes=bytes; }
static int aes_ll_write_key(const uint8_t *key, int words) {
  assert(words * 4 == hw_bytes && (hw_bytes == 16 || hw_bytes == 32));
  memcpy(hw_key, key, words * 4); return words * 4;
}
static void aes_ll_write_block(const uint8_t *p) { memcpy(hw_block, p, 16); }
static void aes_ll_start_transform(void) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  unsigned char result[32]; int length, final;
  assert(ctx);
  assert(EVP_CipherInit_ex(ctx, hw_bytes == 16 ? EVP_aes_128_ecb() : EVP_aes_256_ecb(), NULL, hw_key, NULL, hw_encrypt));
  assert(EVP_CIPHER_CTX_set_padding(ctx, 0));
  assert(EVP_CipherUpdate(ctx, result, &length, hw_block, 16));
  assert(EVP_CipherFinal_ex(ctx, result + length, &final));
  assert(length + final == 16);
  memcpy(hw_block, result, 16); hw_calls++;
  EVP_CIPHER_CTX_free(ctx);
}
static int aes_ll_get_state(void) { return ESP_AES_STATE_IDLE; }
static void aes_ll_read_block(uint8_t *p) { memcpy(p, hw_block, 16); }
'''
driver = ROOT / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_crypto.c'
code = shim + '\n#include "' + str(driver) + '"\n'
code += r'''
int crypto_get_driverid(uint8_t flags) { (void)flags; return 0; }
int crypto_register(uint32_t id, int *algs,
    int (*create)(uint32_t *, struct cryptoini *),
    int (*closefn)(uint64_t), int (*process)(struct cryptop *)) {
  (void)id; (void)algs; (void)create; (void)closefn; (void)process; return 0;
}
static unsigned int comparisons;
static void check(int alg, const void *key, int keylen, const void *iv,
                  const void *plain, const void *cipher, int len) {
  for (int encrypt = 0; encrypt < 2; encrypt++) {
    for (int stream = 0; stream < 2; stream++) {
      struct cryptoini ini = {.cri_alg=alg, .cri_key=(void *)key, .cri_klen=keylen*8};
      unsigned char output[512], ivcopy[16]; uint32_t id;
      assert(len <= sizeof(output)); memcpy(ivcopy, iv, 16);
      assert(s31_newsession(&id, &ini) == 0);
      for (int offset = 0; offset < len;) {
        int size = stream ? 16 : len;
        struct cryptodesc desc = {0}; struct cryptop op = {0};
        desc.crd_alg=alg; desc.crd_len=size;
        desc.crd_flags=(encrypt ? CRD_F_ENCRYPT : 0) | (offset ? CRD_F_UPDATE : 0);
        op.crp_desc=&desc; op.crp_sid=id; op.crp_ilen=op.crp_olen=size;
        op.crp_buf=(char *)(encrypt ? plain : cipher)+offset;
        op.crp_dst=(char *)output+offset;
        op.crp_iv=offset ? NULL : ivcopy; op.crp_ivlen=offset ? 0 : 16;
        int ret=s31_process(&op);
        if (ret) { fprintf(stderr, "alg=%d process=%d\n", alg, ret); abort(); }
        offset += size;
      }
      if (memcmp(output, encrypt ? cipher : plain, len)) {
        fprintf(stderr, "vector mismatch alg=%d encrypt=%d stream=%d len=%d\n", alg, encrypt, stream, len); abort();
      }
      if (alg != CRYPTO_AES_CBC) assert(!memcmp(ivcopy, iv, 16));
      assert(s31_freesession(id) == 0); comparisons++;
    }
  }
}
static int unhex(const char *text, unsigned char *out) {
  int count=0; char *end;
  while (*text) { out[count++]=strtoul(text, &end, 16); assert(end != text); text=end; while (*text==' ') text++; }
  return count;
}
'''
sources = ROOT / 'openvela-dev/apps/testing/drivers/crypto'
cbc = (sources / 'aescbc.c').read_text()
start = cbc.index('struct tb\n')
code += cbc[start:cbc.index('\n};', start)+3] + '\n'
ctr = (sources / 'aesctr.c').read_text()
start = ctr.index('enum\n')
end = ctr.index('\n};', ctr.index('static const g_tests[]'))+3
code += ctr[start:end] + '\n'
xts = (sources / 'aesxts.c').read_text()
start = xts.index('struct aes_xts_tv\n')
end = xts.index('\n};', xts.index('static const struct aes_xts_tv'))+3
code += xts[start:end] + '\n'
code += r'''
int main(void) {
  unsigned int cbc=0, ctr=0, xts=0, rejected192=0;
  for (unsigned int i=0; i<sizeof(g_testcase)/sizeof(g_testcase[0]); i++) {
    const struct tb *v=&g_testcase[i];
    check(CRYPTO_AES_CBC, v->key, 16, v->iv, v->plain, v->cipher, v->len); cbc++;
  }
  for (unsigned int i=0; i<sizeof(g_tests)/sizeof(g_tests[0]); i++) {
    unsigned char data[4][512]; int sizes[4];
    for (int j=0;j<4;j++) sizes[j]=unhex(g_tests[i].data[j], data[j]);
    if (sizes[0] == 24) {
      struct cryptoini ini={.cri_alg=CRYPTO_AES_CTR, .cri_klen=192, .cri_key=data[0]}; uint32_t id;
      assert(s31_newsession(&id,&ini) == -EINVAL); rejected192++; continue;
    }
    check(CRYPTO_AES_CTR, data[0], sizes[0], data[1], data[2], data[3], sizes[2]); ctr++;
  }
  for (unsigned int i=0;i<sizeof(g_aes_xts_test_vectors)/sizeof(g_aes_xts_test_vectors[0]);i++) {
    const struct aes_xts_tv *v=&g_aes_xts_test_vectors[i];
    check(CRYPTO_AES_XTS,v->key,v->key_len,v->data_unit,v->plaintext,v->ciphertext,v->text_len); xts++;
  }
  assert(!g_sessions);
  printf("HOST_AES_MODES=PASS CBC_vectors=%u CTR_vectors=%u XTS_vectors=%u comparisons=%u ECB_calls=%u AES192_rejected=%u\n",cbc,ctr,xts,comparisons,hw_calls,rejected192);
  puts("HOST_ONLY: OpenSSL replaces the AES PIO primitive; no S31 hardware verdict.");
}
'''
source = OUT / 'vectors.c'
# Exercise the actual framework selector as well: unsupported hardware key
# lengths may fall back, hardware-only and allocation failures may not.
framework = (ROOT / 'openvela-dev/nuttx/crypto/crypto.c').read_text()
start = framework.index('int crypto_newsession(')
end = framework.index('\n}\n', start) + 3
selector = r'''
static struct cryptocap *crypto_drivers;
static int crypto_drivers_num;
static mutex_t g_crypto_lock;
'''
selector += framework[start:end]
selector += r'''
static unsigned int software_calls;
static int fake_software(uint32_t *id, struct cryptoini *ini) {
  assert(ini->cri_klen == 192); *id=42; software_calls++; return 0;
}
static int fake_nomem(uint32_t *id, struct cryptoini *ini) {
  (void)id; (void)ini; return -ENOMEM;
}
static void check_selector(void) {
  struct cryptocap drivers[2]={0}; unsigned char key[32]={0}; uint64_t id;
  struct cryptoini ini={.cri_alg=CRYPTO_AES_CTR,.cri_key=key,.cri_klen=128};
  crypto_drivers=drivers; crypto_drivers_num=2;
  drivers[0].cc_alg[CRYPTO_AES_CTR]=CRYPTO_ALG_FLAG_SUPPORTED;
  drivers[0].cc_newsession=s31_newsession;
  drivers[1].cc_alg[CRYPTO_AES_CTR]=CRYPTO_ALG_FLAG_SUPPORTED;
  drivers[1].cc_newsession=fake_software; drivers[1].cc_flags=CRYPTOCAP_F_SOFTWARE;
  assert(crypto_newsession(&id,&ini,0)==0 && (id>>32)==0);
  assert(software_calls==0 && drivers[0].cc_sessions==1);
  assert(s31_freesession(id)==0);
  ini.cri_klen=192;
  assert(crypto_newsession(&id,&ini,0)==0 && (id>>32)==1 && (uint32_t)id==42);
  assert(software_calls==1 && drivers[1].cc_sessions==1);
  assert(crypto_newsession(&id,&ini,1)==-EINVAL && software_calls==1);
  drivers[0].cc_newsession=fake_nomem;
  assert(crypto_newsession(&id,&ini,0)==-ENOMEM && software_calls==1);
  puts("HOST_CRYPTO_SELECTOR=PASS hardware-preference AES192-fallback hardware-only ENOMEM-preserved");
}
'''
code = code.replace('int main(void) {', selector + '\nint main(void) {\n  check_selector();')
source.write_text(code)
binary = OUT / 'vectors'
subprocess.run(['cc', '-O1', '-g', '-fsanitize=address,undefined',
                '-I'+str(INCLUDE), str(source), '-lcrypto', '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
