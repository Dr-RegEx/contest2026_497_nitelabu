"""Check the real ECC source's complete random buffers and P-256 operations.

Host RNG checks requested byte counts, then fills using getrandom. Both
native and RV32-style no-int128 arithmetic are exercised; no board access.
"""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--before',action='store_true')
parser.add_argument('--hardware',action='store_true')
args=parser.parse_args()
root=Path('/home/regex/work/esp32s31-openvela')
out=root/'backups/2026-09-10-scan-stress/host-ecc-random'
out.mkdir(exist_ok=True)
inc=out/'include'
for name in ('crypto/ecc.h','nuttx/macro.h'):
    p=inc/name
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text('#include "'+str(root/'openvela-dev/nuttx/include'/name)+'"\n')
for name in ('nuttx/config.h','nuttx/crypto/crypto.h','nuttx/clock.h','debug.h',
             'esp_crypto_lock.h','esp_private/periph_ctrl.h','hal/ecc_ll.h'):
    p=inc/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('/* Host shim. */\n')
source=(root/'openvela-dev/nuttx/crypto/ecc.c').read_text()
if args.hardware:
    source='#include "'+str(root/'backups/2026-09-10-scan-stress/host-ecc-point-shim.h')+'"\n'+source

if args.before:
    for name in ('l_private','l_random','k'):
        source=source.replace(f'arc4random_buf({name}, sizeof({name}));', f'arc4random_buf({name}, NUM_ECC_DIGITS);')
code=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/random.h>
#include <errno.h>
#define FAR
static unsigned int random_calls;
static void checked_random(void *p,size_t n) {
  if (n!=32) { fprintf(stderr,"ECC_RANDOM=FAIL requested=%zu expected=32\n",n); exit(42); }
  random_calls++;
  unsigned char *b=p;
  while (n) {
    ssize_t r=getrandom(b,n,0);
    if (r<0 && errno==EINTR) continue;
    assert(r>0); b+=r; n-=r;
  }
}
#define arc4random_buf checked_random
'''+source+r'''
int main(void) {
  uint8_t pub1[33],pub2[33],priv1[32],priv2[32],sig[64],hash[32]={0};
  uint8_t x[32],y[32],d[32],uncomp[33],shared1[32],shared2[32];
  assert(ecc_make_key(pub1,priv1)==1);
  assert(ecc_make_key(pub2,priv2)==1);
  assert(ecc_make_key_uncomp(x,y,d)==1);
  memcpy(uncomp+1,x,32); uncomp[0]=2+(y[31]&1);
  assert(ecdsa_sign(priv1,hash,sig)==1);
  assert(ecdsa_verify(pub1,hash,sig)==1);
  hash[0]^=1; assert(ecdsa_verify(pub1,hash,sig)==0); hash[0]^=1;
  assert(ecdsa_sign(d,hash,sig)==1);
  assert(ecdsa_verify(uncomp,hash,sig)==1);
  assert(ecdh_shared_secret(pub2,priv1,shared1)==1);
  assert(ecdh_shared_secret(pub1,priv2,shared2)==1);
  assert(memcmp(shared1,shared2,32)==0);
#ifdef CONFIG_CRYPTO_ECC_P256_POINT_MULT
  assert(hw_calls==5);
  for(size_t i=0;i<sizeof(hw_regs);i++)assert(((unsigned char *)hw_regs)[i]==0);
  puts("HOST_ECC_POINT=PASS actual point adapter + keygen/sign integration calls=5 registers-cleared; OpenSSL primitive only");
#endif
  printf("HOST_ECC_RANDOM=PASS full_random_calls=%u keygen/sign/verify/reject/shared-secret arithmetic_int128=%d\n",random_calls,SUPPORTS_INT128);
  return 0;
}
'''
p=out/('before.c' if args.before else 'after.c')
p.write_text(code)
for portable in (False,True):
    exe=out/('before' if args.before else 'after')
    flags=['-U__SIZEOF_INT128__'] if portable else []
    subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined',*flags,'-I',str(inc),str(p),'-lcrypto','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
