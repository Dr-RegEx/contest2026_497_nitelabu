"""Check actual verify adapter and asymmetric selector without board access.

OpenSSL replaces the ECDSA peripheral. Checks endian/ABI, invalid signature,
bounded timeout and selection; does not establish target hardware behavior.
"""
from pathlib import Path
import subprocess
root=Path('/home/regex/work/esp32s31-openvela')
out=root/'backups/2026-09-10-scan-stress/host-ecdsa';out.mkdir(exist_ok=True)
inc=out/'include'
for name in ('nuttx/config.h','debug.h','nuttx/clock.h','nuttx/crypto/crypto.h',
             'esp_crypto_lock.h','esp_private/periph_ctrl.h','hal/ecc_ll.h','hal/ecdsa_ll.h'):
    p=inc/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('/* Host shim only. */\n')
p=inc/'crypto/cryptodev.h';p.parent.mkdir(exist_ok=True)
p.write_text('#include "'+str(root/'openvela-dev/nuttx/include/crypto/cryptodev.h')+'"\n')
code=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#define FAR
#define CODE
#define OK 0
#define PERIPH_RCC_ATOMIC() for (int once=1;once;once=0)
#define MSEC2TICK(n) (n)
#define cryptinfo(...) ((void)0)
static uint32_t ticks;
static uint32_t clock_systime_ticks(void) { return ++ticks; }
struct {struct {int reg_crypto_ecdsa_rst_en;int reg_crypto_rst_en;}crypto_ctrl0;}HP_SYS_CLKRST;
typedef enum {ECDSA_PARAM_R,ECDSA_PARAM_S,ECDSA_PARAM_Z,ECDSA_PARAM_QAX,ECDSA_PARAM_QAY}ecdsa_ll_param_t;
enum {ECDSA_STATE_IDLE,ECDSA_STATE_LOAD,ECDSA_STATE_GET,ECDSA_STATE_BUSY};
enum {ECDSA_STAGE_START_CALC,ECDSA_STAGE_LOAD_DONE,ECDSA_STAGE_GET_DONE};
enum {ECDSA_MODE_SIGN_VERIFY,ECDSA_CURVE_SECP256R1,ECDSA_Z_USER_PROVIDED,ECDSA_INT_CALC_DONE,ECDSA_INT_SHA_RELEASE};
static uint8_t hw_params[5][32];
static int hw_state,hw_result,hw_stuck,hw_calls;
static void esp_crypto_ecdsa_lock_acquire(void) {}
static void esp_crypto_ecdsa_lock_release(void) {}
static void ecdsa_ll_enable_bus_clock(bool on) {assert(on);}
static void ecc_ll_enable_bus_clock(bool on) {assert(on);}
static void ecc_ll_power_up(void) {}
static void ecc_ll_reset_register(void) {}
static void ecdsa_ll_disable_intr(int type) {(void)type;}
static void ecdsa_ll_set_mode(int mode) {assert(mode==ECDSA_MODE_SIGN_VERIFY);}
static void ecdsa_ll_set_curve(int curve) {assert(curve==ECDSA_CURVE_SECP256R1);}
static void ecdsa_ll_set_z_mode(int mode) {assert(mode==ECDSA_Z_USER_PROVIDED);}
static int ecdsa_ll_get_state(void) {return hw_stuck?ECDSA_STATE_BUSY:hw_state;}
static void ecdsa_ll_write_param(ecdsa_ll_param_t param,const uint8_t *p,int n) {
  assert(n==32 && hw_state==ECDSA_STATE_LOAD);memcpy(hw_params[param],p,n);
}
static void ecdsa_ll_set_stage(int stage) {
  if(stage==ECDSA_STAGE_START_CALC){hw_state=ECDSA_STATE_LOAD;return;}
  assert(stage==ECDSA_STAGE_LOAD_DONE);
  EC_KEY *key=EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);assert(key);
  const EC_GROUP *group=EC_KEY_get0_group(key);
  EC_POINT *point=EC_POINT_new(group);assert(point);
  BIGNUM *x=BN_lebin2bn(hw_params[ECDSA_PARAM_QAX],32,NULL);
  BIGNUM *y=BN_lebin2bn(hw_params[ECDSA_PARAM_QAY],32,NULL);
  BIGNUM *r=BN_lebin2bn(hw_params[ECDSA_PARAM_R],32,NULL);
  BIGNUM *s=BN_lebin2bn(hw_params[ECDSA_PARAM_S],32,NULL);
  ECDSA_SIG *sig=ECDSA_SIG_new();assert(x&&y&&r&&s&&sig);
  assert(EC_POINT_set_affine_coordinates(group,point,x,y,NULL));
  assert(EC_KEY_set_public_key(key,point));assert(ECDSA_SIG_set0(sig,r,s));
  uint8_t digest[32];for(int i=0;i<32;i++)digest[i]=hw_params[ECDSA_PARAM_Z][31-i];
  hw_result=ECDSA_do_verify(digest,32,sig,key)==1;hw_calls++;
  hw_state=ECDSA_STATE_IDLE;
  ECDSA_SIG_free(sig);EC_POINT_free(point);EC_KEY_free(key);BN_free(x);BN_free(y);
}
static int ecdsa_ll_get_operation_result(void){return hw_result;}
'''
code+='\n#include "'+str(root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_ecdsa.c')+'"\n'
code+=r'''
int crypto_get_driverid(uint8_t flags){(void)flags;return 0;}
int crypto_kregister(uint32_t id,int *algs,int(*process)(struct cryptkop *)){
  (void)id;(void)algs;(void)process;return 0;
}
static int g_crypto_lock;
static int nxmutex_lock(int *m){(void)m;return 0;}
static void nxmutex_unlock(int *m){(void)m;}
static struct cryptocap crypto_drivers[2];
static int crypto_drivers_num=2;
int cryptodevallowsoft=1;
static int selected;
static int soft_process(struct cryptkop *p){selected=1;p->krp_status=0;return 0;}
static int hard_process(struct cryptkop *p){selected=2;p->krp_status=0;return 0;}
'''
source=(root/'openvela-dev/nuttx/crypto/crypto.c').read_text();a=source.index('int crypto_kinvoke(');b=source.index('\n/*',a+20)
code+=source[a:b]+'\n'
source=(root/'openvela-dev/apps/testing/drivers/crypto/ecdsa.c').read_text();a=source.index('static unsigned char sha256_message');b=source.index('\n};',a)+3
code+=source[a:b]+'\n'
code+=r'''
int main(void){
  uint8_t inputs[6][32]={{0}};
  EC_KEY *key=EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);assert(key);
  assert(EC_KEY_generate_key(key));
  const EC_GROUP *group=EC_KEY_get0_group(key);
  const EC_POINT *point=EC_KEY_get0_public_key(key);
  BIGNUM *x=BN_new(),*y=BN_new();assert(x&&y);
  assert(EC_POINT_get_affine_coordinates(group,point,x,y,NULL));
  assert(BN_bn2binpad(x,inputs[0],32)==32);assert(BN_bn2binpad(y,inputs[1],32)==32);
  ECDSA_SIG *sig=ECDSA_do_sign(sha256_message,32,key);assert(sig);
  const BIGNUM *r,*s;ECDSA_SIG_get0(sig,&r,&s);
  assert(BN_bn2binpad(r,inputs[3],32)==32);assert(BN_bn2binpad(s,inputs[4],32)==32);
  memcpy(inputs[5],sha256_message,32);
  struct cryptkop op={.krp_op=CRK_ECDSA_SECP256R1_VERIFY,.krp_iparams=6};
  for(int i=0;i<6;i++){op.krp_param[i].crp_p=(void *)inputs[i];op.krp_param[i].crp_nbits=256;}
  assert(s31_ecdsa_verify(&op)==0 && op.krp_status==0);
  inputs[5][0]^=1;assert(s31_ecdsa_verify(&op)==0 && op.krp_status==1);inputs[5][0]^=1;
  op.krp_param[0].crp_nbits=248;assert(s31_ecdsa_verify(&op)==0 && (int)op.krp_status==-EINVAL);
  op.krp_param[0].crp_nbits=256;hw_stuck=1;
  assert(s31_ecdsa_verify(&op)==0 && (int)op.krp_status==-ETIMEDOUT);hw_stuck=0;
  assert(hw_calls==2);
  crypto_drivers[0].cc_flags=CRYPTOCAP_F_SOFTWARE;
  crypto_drivers[0].cc_kprocess=soft_process;crypto_drivers[1].cc_kprocess=hard_process;
  crypto_drivers[0].cc_kalg[op.krp_op]=crypto_drivers[1].cc_kalg[op.krp_op]=CRYPTO_ALG_FLAG_SUPPORTED;
  assert(crypto_kinvoke(&op)==0 && selected==2 && op.krp_hid==1);
  crypto_drivers[1].cc_kalg[op.krp_op]=0;
  assert(crypto_kinvoke(&op)==0 && selected==1 && op.krp_hid==0);
  cryptodevallowsoft=0;assert(crypto_kinvoke(&op)==0 && (int)op.krp_status==-ENODEV);
  op.krp_op=CRK_ALGORITHM_MAX+1;assert(crypto_kinvoke(&op)==-EINVAL);
  ECDSA_SIG_free(sig);EC_KEY_free(key);BN_free(x);BN_free(y);
  puts("HOST_ECDSA_VERIFY=PASS valid/invalid/length/timeout; selector hardware-first/software-only-if-allowed/bounds PASS; no target evidence");
  return 0;
}
'''
p=out/'check.c';p.write_text(code)
subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined','-Wno-deprecated-declarations','-I',str(inc),str(p),'-lcrypto','-o',str(out/'check')],check=True)
subprocess.run([str(out/'check')],check=True)
