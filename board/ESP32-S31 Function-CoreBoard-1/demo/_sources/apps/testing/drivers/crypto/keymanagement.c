/****************************************************************************
 * apps/testing/drivers/crypto/keymanagement.c
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <crypto/cryptodev.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

typedef enum
{
  SYNC = 0,
  ASYNC,
}
crypto_mode_t;

typedef struct crypto_context_s
{
  int fd;
  int cryptodev_fd;
  struct crypt_kop cryptk;
  struct cryptkop  cryptkcb;
  struct pollfd fds;
  unsigned char bufcb[64];
}
crypto_context_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static crypto_context_t g_keymgmt_ctx;

static FAR char *g_aescbc_key =
  "\x06\xa9\x21\x40\x36\xb8\xa1\x5b\x51\x2e\x03\xd5\x34\x12\x00\x06";
static FAR char *g_aescbc_iv =
  "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30\xb4\x22\xda\x80\x2c\x9f\xac\x41";
static FAR char *g_aescbc_plain = "Single block msg";
static FAR char *g_aescbc_cipher =
  "\xe3\x53\x77\x9c\x10\x79\xae\xb8\x27\x08\x94\x2d\xbe\x77\x18\x1a";

static unsigned char g_aescmac_key[16] =
{
    0x2b, 0x7e, 0x15, 0x16,     0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88,     0x09, 0xcf, 0x4f, 0x3c
};

static unsigned char g_aescmac_message[] =
{
    0x6b, 0xc1, 0xbe, 0xe2,     0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11,     0x73, 0x93, 0x17, 0x2a,
    0xae, 0x2d, 0x8a, 0x57,     0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac,     0x45, 0xaf, 0x8e, 0x51,
    0x30, 0xc8, 0x1c, 0x46,     0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19,     0x1a, 0x0a, 0x52, 0xef,
    0xf6, 0x9f, 0x24, 0x45,     0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b,     0xe6, 0x6c, 0x37, 0x10
};

static unsigned char g_aescmac_mac[] =
{
    0x51, 0xf0, 0xbe, 0xbf,     0x7e, 0x3b, 0x9d, 0x92,
    0xfc, 0x49, 0x74, 0x17,     0x79, 0x36, 0x3c, 0xfe
};

/* Example RSA-2048 keypair, for test purposes */

static const char g_rsa_n_2048[] =
"B11CD52A2175035B3AE01EAD94670442"
"65E521123F2E66ECE0458F4C21ACDA52"
"CEFE57E1D5F03F85B6973C9385FF5309"
"9EB35D760B3B321C8953B4C86B3525FD"
"327464C0C4A7E60D5729FC2B7837D44F"
"EC082DB712E77A7FFCAFED1ABD3F1D0C"
"E1FD31EB57A082CA43D1AE182A705AE5"
"F16E3C4BDF6D959B42F331020B5D0113"
"2271A9468C2D87DA73FEB828053909CC"
"355C1536D0131E7BE88FB3173CEBB402"
"68A706D3F7CA17F38CE56402B304D775"
"562779240BE2DBF07D91AB68A6D17A61"
"DE9019630F3D9DA8A2438D21D0EEF282"
"8D5BF379BCD5C70B3043C510140F0C48"
"D6254F0F1A71BC1C43E1F75D7A55FC4B"
"83D269E9336FE91E3F648153FDBD4CAF";

static const char g_rsa_e_2048[] = "010001";

static const char g_rsa_d_2048[] =
"1572685DB1AF79B89268914E73AA05DA"
"047CBA847B2223548BD3E086578D96E2"
"702E5C75FC721B3F15DD3A78A2C1484C"
"E1EAD66674F5B61BBE85645EE09C8EFD"
"1A1EFE1459BBFB1FA26E731AD073F241"
"BB534235E9141EC160D7ECDE500C5478"
"3AE56E2611765E601FD6443EA8E4F21C"
"E61F732DBCE0883D44DE2E11114F31D1"
"EBB01624BF9C4D2C22F2239E9A475305"
"17E2D870002872449D2680D9D729F1FE"
"573D8FA6304FE503922193323367B012"
"031CE97B1F0C103AB446543D23438BBF"
"8F09C142AD9105FE1F55E984C4C79220"
"6C5EDD7FF4CF9EEF4DA98A05C1258821"
"AE9917C76F6343F2865125B18E1CC827"
"C4DECC980B2DCCB405D68D869C8ED801";

static const char g_rsa_p_2048[] =
"E2202080A3A73D44F72E72FDFF6411EB"
"C933DB799611817841E6DA580868949F"
"10C64FB6406421100DD2FB88FDAB3217"
"07B695463403EDE2DACDD8ACA53B2758"
"9DAAB39115D32510494500693F0368DE"
"63296164D5705B06E142B7B6522E09D6"
"34DD53D060601B26121E0BEFE3C89BF1"
"57A48F1FCA43CAD0FC10658FC25627F5";

static const char g_rsa_q_2048[] =
"C883060955E8F9CBAD62A397828B3FBB"
"F10FF5008FED67F2A33444173463DC06"
"2465A782E1022E00AF6C0AEF2C41CABB"
"9F820840AA383E59E0AAC63C47C23A29"
"6BF352D6A843A586A09B2AD3FACFF5C5"
"3EDD9B68F55865DBA4E99415449C94CB"
"B407C695348FF90871A598C2A0698FB0"
"764A31CAB55EF5FC26158C5FC7190F93";

static const char g_rsa_dp_2048[] =
"4571144A2CF7C8EB0AF5AABEB1D56B63"
"7B707F700153F2EC996069A12B43E290"
"4EB877F64223FDBB4E638277795F3F8C"
"AF03B527D0057CE273CB4F4505A0FADE"
"4DDD043403FDC99E0CC231F42033CDDA"
"AD9D2199B3AA436A037757AF97E12788"
"2EC7E15B569D6CF8EE0B68230D52A801"
"1FA71B84E9BA93E845E04B3030C00099";

static const char g_rsa_dq_2048[] =
"AC096F1677758B4DAC1823D08D1B38B5"
"E261148F44EE26EFD203B82048BD1D3C"
"CFE1C055D504EFBF7AC3B2A5FF4CFBCA"
"FB52B00DFB7E8250CF28F72925508A62"
"9BD28BD2CD502D2753898EB78E4CFBCC"
"1EE238C3572E5F46933A2DEA09926740"
"57D90C135CA3E2C1D1F0891F044F4E0F"
"D38E592659737E8EB78669888BE4FEAD";

static const char g_rsa_qp_2048[] =
"E106721379123664B25749C811397BE6"
"F5C08BA81F1BDA70F605AEC84D70A4B2"
"73E5EBF9E8056F7FD875A66712D1D5EF"
"D11577ADA1D3C8100C58314E5D8EE237"
"B7EAD19FDA726E87B5A3E19722C15B5A"
"5885A32D2FC4685507E7CA63CF0E7CE2"
"ECB130EB3EC473E15504C53EFF5FB236"
"8BF4FFE4D0E49AD75C915C2989AF2558";

static unsigned char g_message_sha256[] =
{
  0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92,
  0x81, 0xa1, 0xc7, 0xe2, 0x84, 0xd7, 0x3e, 0x67,
  0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97, 0x20, 0x0e,
  0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void crypto_free(FAR crypto_context_t *ctx)
{
  if (ctx->cryptodev_fd != 0)
    {
      close(ctx->cryptodev_fd);
      ctx->cryptodev_fd = 0;
    }

  if (ctx->fd != 0)
    {
      close(ctx->fd);
      ctx->fd = 0;
    }
}

static int crypto_init(FAR crypto_context_t *ctx)
{
  memset(ctx, 0, sizeof(crypto_context_t));
  ctx->cryptk.crk_arg = &ctx->cryptkcb;

  if ((ctx->fd = open("/dev/crypto", O_RDWR, 0)) < 0)
    {
      perror("CRIOGET");
      crypto_free(ctx);
      return 1;
    }

  if (ioctl(ctx->fd, CRIOGET, &ctx->cryptodev_fd) == -1)
    {
      perror("CRIOGET");
      crypto_free(ctx);
      return 1;
    }

  return 0;
}

static int keyid_operation(FAR crypto_context_t *ctx,
                           int op, FAR int *keyid,
                           int paramin, int paramout,
                           FAR unsigned char *buf, int buflen,
                           bool is_sync)
{
  ctx->cryptk.crk_op = op;
  ctx->cryptk.crk_iparams = paramin;
  ctx->cryptk.crk_oparams = paramout;
  ctx->cryptk.crk_param[0].crp_p = (caddr_t)keyid;
  ctx->cryptk.crk_param[0].crp_nbits = sizeof(int) * 8;
  ctx->cryptk.crk_param[1].crp_p = (caddr_t)buf;
  ctx->cryptk.crk_param[1].crp_nbits = buflen * 8;
  if (!is_sync)
    {
      ctx->cryptk.crk_flags |= CRYPTO_F_CBIMM;
      ctx->cryptkcb.krp_op = op;
      ctx->cryptkcb.krp_iparams = paramin;
      ctx->cryptkcb.krp_oparams = paramout;
      ctx->cryptkcb.krp_param[0].crp_p = (caddr_t)keyid;
      ctx->cryptkcb.krp_param[0].crp_nbits = sizeof(int) * 8;
      ctx->cryptkcb.krp_flags |= CRYPTO_F_CBIMM;
      ctx->cryptkcb.krp_param[1].crp_p = (caddr_t)ctx->bufcb;
      ctx->cryptkcb.krp_param[1].crp_nbits = buflen * 8;
    }

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &ctx->cryptk) == -1)
    {
      perror("CIOCKEY");
      return -1;
    }

  if (!is_sync)
    {
      memset(&ctx->fds, 0, sizeof(struct pollfd));
      ctx->fds.fd = ctx->cryptodev_fd;
      ctx->fds.events = POLLIN;

      if (poll(&ctx->fds, 1, -1) < 0)
        {
          perror("poll");
          return -1;
        }

      if (ctx->fds.revents & POLLIN)
        {
          if (ioctl(ctx->cryptodev_fd, CIOCKEYRET, &ctx->cryptk) == -1)
            {
              perror("CIOCKEYRET");
              return -1;
            }
        }
      else
        {
          perror("poll back");
          return -1;
        }
    }

  return ctx->cryptk.crk_status;
}

static int keymanagement_test(int mode)
{
  unsigned char data[32];
  unsigned char buf[32];
  int keyid = -1;

  arc4random_buf(data, sizeof(data));

  assert_int_equal(crypto_init(&g_keymgmt_ctx), 0);

  /* testcase 1: check keyid */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY, &keyid,
                                   0, 1, NULL, 0, mode == SYNC), 0);

  /* testcase 2: import and export key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY, &keyid,
                                   2, 0, data, sizeof(data),
                                   mode == SYNC), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_EXPORT_KEY, &keyid,
                                   1, 1, buf, sizeof(buf), mode == SYNC), 0);

  assert_int_equal(memcmp(buf, data, sizeof(data)), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &keyid,
                                   1, 0, NULL, 0, mode == SYNC), 0);

  /* testcase 3: load/unload key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_LOAD_KEY, &keyid,
                                   1, 0, NULL, 0, mode == SYNC), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_UNLOAD_KEY, &keyid,
                                   1, 0, NULL, 0, mode == SYNC), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &keyid,
                                   1, 0, NULL, 0, mode == SYNC), 0);

  crypto_free(&g_keymgmt_ctx);
  return 0;
}

static void test_keymgmt_sync(void **state)
{
  assert_int_equal(keymanagement_test(SYNC), 0);
}

static void test_keymgmt_async(void **state)
{
  assert_int_equal(keymanagement_test(ASYNC), 0);
}

static int aescbc_crypt(FAR crypto_context_t *ctx,
                        FAR const char *key, size_t klen,
                        FAR const char *iv, FAR const char *in,
                        FAR unsigned char *out, size_t len, int encrypt)
{
  struct session_op session;
  struct crypt_op cryp;

  memset(&session, 0, sizeof(session));
  session.cipher = CRYPTO_AES_CBC;
  session.key = (caddr_t)key;
  session.op = encrypt ? COP_ENCRYPT : COP_DECRYPT;
  session.flags |= SOP_F_KEYID;
  session.keylen = klen;
  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCGSESSION, &session), 0);

  memset(&cryp, 0, sizeof(cryp));
  cryp.ses = session.ses;
  cryp.op = encrypt ? COP_ENCRYPT : COP_DECRYPT;
  cryp.flags = 0;
  cryp.len = len;
  cryp.olen = len;
  cryp.ivlen = 16;
  cryp.src = (caddr_t)in;
  cryp.dst = (caddr_t)out;
  cryp.iv = (caddr_t)iv;
  cryp.mac = 0;
  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCCRYPT, &cryp), 0);
  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCFSESSION, &session.ses), 0);
  return 0;
}

static void test_keymgmt_aescbc(void **state)
{
  unsigned char buf[64];
  int keyid = -1;

  assert_int_equal(crypto_init(&g_keymgmt_ctx), 0);

  /* step 1: allocate one useable keyid */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY, &keyid,
                                   0, 1, NULL, 0, TRUE), 0);

  /* step 2: import key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY, &keyid,
                                   2, 0, (unsigned char *)g_aescbc_key,
                                   16, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &keyid,
                                   1, 0, NULL, 0, TRUE), 0);

  /* step 3: calculate out */

  assert_int_equal(aescbc_crypt(&g_keymgmt_ctx, (char *)&keyid,
                                sizeof(uint32_t), g_aescbc_iv,
                                g_aescbc_plain, buf, 16, 1), 0);

  assert_int_equal(memcmp(buf, g_aescbc_cipher, 16), 0);

  /* step 4: remove key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &keyid,
                                   1, 0, NULL, 0, TRUE), 0);

  crypto_free(&g_keymgmt_ctx);
}

static int aescmac_crypt(FAR crypto_context_t *ctx,
                         FAR const unsigned char *key, size_t klen,
                         FAR const unsigned char *message, size_t len,
                         FAR unsigned char *output, size_t outlen)
{
  struct session_op session;
  struct crypt_op cryp;

  memset(&session, 0, sizeof(session));
  session.cipher = CRYPTO_AES_CMAC;
  session.key = (caddr_t)key;
  session.keylen = klen;
  session.mac = CRYPTO_AES_128_CMAC;
  session.mackey = (caddr_t)key;
  session.mackeylen = klen;
  session.flags |= SOP_F_KEYID;

  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCGSESSION, &session), 0);

  memset(&cryp, 0, sizeof(cryp));
  cryp.ses = session.ses;
  cryp.op = COP_ENCRYPT;

  if (len > 0)
    {
      cryp.flags |= COP_FLAG_UPDATE;
      cryp.len = len;
      cryp.src = (caddr_t)message;
      assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCCRYPT, &cryp), 0);
    }

  cryp.flags = 0;
  cryp.len = 0;
  cryp.olen = outlen;
  cryp.src = NULL;
  cryp.mac = (caddr_t)output;

  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCCRYPT, &cryp), 0);
  assert_int_equal(ioctl(ctx->cryptodev_fd, CIOCFSESSION, &session.ses), 0);
  return 0;
}

static void test_keymgmt_aescmac(void **state)
{
  unsigned char buf[64];
  int keyid = -1;

  assert_int_equal(crypto_init(&g_keymgmt_ctx), 0);

  /* step 1: allocate one useable keyid */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY, &keyid,
                                   0, 1, NULL, 0, TRUE), 0);

  /* step 2: import key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY, &keyid,
                                   2, 0, g_aescmac_key, 16, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &keyid,
                                   1, 0, NULL, 0, TRUE), 0);

  /* step 3: calculate out */

  assert_int_equal(aescmac_crypt(&g_keymgmt_ctx, (unsigned char *)&keyid,
                                 sizeof(uint32_t), g_aescmac_message,
                                 64, buf, 16), 0);

  assert_int_equal(memcmp(buf, g_aescmac_mac, 16), 0);

  /* step 4: remove key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &keyid,
                                   1, 0, NULL, 0, TRUE), 0);

  crypto_free(&g_keymgmt_ctx);
}

static void mpi_get_digit(FAR uint8_t *d, char c)
{
  if (c >= '0' && c <= '9')
    {
      *d = c - '0';
    }

  if (c >= 'A' && c <= 'F')
    {
      *d = c - 'A' + 10;
    }

  if (c >= 'a' && c <= 'f')
    {
      *d = c - 'a' + 10;
    }
}

static void mpi_from_string(FAR unsigned char *buf,
                            FAR const char *s,
                            size_t slen)
{
  int i;
  int j;
  uint8_t d = 0;

  for (i = slen, j = 0; i > 0; i--, j++)
    {
      mpi_get_digit(&d, s[i - 1]);
      buf[j / 2] |= d << ((j % 2) << 2);
    }
}

static int
test_keymgmt_rsa_crypt(FAR crypto_context_t *ctx,
                       int nkeyid, int ekeyid, int dkeyid, int pkeyid,
                       int qkeyid, int dpkeyid, int dqkeyid, int qpkeyid,
                       FAR unsigned char *message, size_t len,
                       FAR unsigned char *output, size_t outlen,
                       bool encrypt)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;
  cryptk.crk_flags |= SOP_F_KEYID;
  cryptk.crk_param[0].crp_p = (caddr_t)message;
  cryptk.crk_param[0].crp_nbits = len * 8;
  if (encrypt)
    {
      cryptk.crk_op = CRK_MOD_EXP;
      cryptk.crk_iparams = 3;
      cryptk.crk_oparams = 1;
      cryptk.crk_param[1].crp_p = (caddr_t)&nkeyid;
      cryptk.crk_param[1].crp_nbits = sizeof(nkeyid) * 8;
      cryptk.crk_param[2].crp_p = (caddr_t)&ekeyid;
      cryptk.crk_param[2].crp_nbits = sizeof(ekeyid) * 8;
      cryptk.crk_param[3].crp_p = (caddr_t)output;
      cryptk.crk_param[3].crp_nbits = outlen * 8;
      cryptk.crk_keytype = CRYPTO_KEY_TYPE_PUBLIC;
      cryptk.crk_optype = CRYPTO_OP_ENCRYPT;
    }
  else
    {
      cryptk.crk_op = CRK_MOD_EXP_CRT;
      cryptk.crk_iparams = 9;
      cryptk.crk_oparams = 1;
      cryptk.crk_param[1].crp_p = (caddr_t)&nkeyid;
      cryptk.crk_param[1].crp_nbits = sizeof(nkeyid) * 8;
      cryptk.crk_param[2].crp_p = (caddr_t)&ekeyid;
      cryptk.crk_param[2].crp_nbits = sizeof(ekeyid) * 8;
      cryptk.crk_param[3].crp_p = (caddr_t)&dkeyid;
      cryptk.crk_param[3].crp_nbits = sizeof(dkeyid) * 8;
      cryptk.crk_param[4].crp_p = (caddr_t)&pkeyid;
      cryptk.crk_param[4].crp_nbits = sizeof(pkeyid) * 8;
      cryptk.crk_param[5].crp_p = (caddr_t)&qkeyid;
      cryptk.crk_param[5].crp_nbits = sizeof(qkeyid) * 8;
      cryptk.crk_param[6].crp_p = (caddr_t)&dpkeyid;
      cryptk.crk_param[6].crp_nbits = sizeof(dpkeyid) * 8;
      cryptk.crk_param[7].crp_p = (caddr_t)&dqkeyid;
      cryptk.crk_param[7].crp_nbits = sizeof(dqkeyid) * 8;
      cryptk.crk_param[8].crp_p = (caddr_t)&qpkeyid;
      cryptk.crk_param[8].crp_nbits = sizeof(qpkeyid) * 8;
      cryptk.crk_param[9].crp_p = (caddr_t)output;
      cryptk.crk_param[9].crp_nbits = outlen * 8;
      cryptk.crk_keytype = CRYPTO_KEY_TYPE_PRIVATE;
      cryptk.crk_optype = CRYPTO_OP_DECRYPT;
    }

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("rsa calculate failed");
      return -1;
    }

  return cryptk.crk_status;
}

static int
test_keymgmt_rsa_sign(FAR crypto_context_t *ctx,
                      int nkeyid, int ekeyid, int dkeyid, int pkeyid,
                      int qkeyid, int dpkeyid, int dqkeyid, int qpkeyid,
                      FAR unsigned char *hash, size_t hashlen,
                      FAR unsigned char *sig, size_t siglen)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_op = CRK_RSA_PKCS15_SIGN;
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;
  cryptk.crk_iparams = 9;
  cryptk.crk_oparams = 1;

  cryptk.crk_param[0].crp_p = (caddr_t)hash;
  cryptk.crk_param[0].crp_nbits = hashlen * 8;
  cryptk.crk_param[1].crp_p = (caddr_t)&nkeyid;
  cryptk.crk_param[1].crp_nbits = sizeof(nkeyid) * 8;
  cryptk.crk_param[2].crp_p = (caddr_t)&ekeyid;
  cryptk.crk_param[2].crp_nbits = sizeof(ekeyid) * 8;
  cryptk.crk_param[3].crp_p = (caddr_t)&dkeyid;
  cryptk.crk_param[3].crp_nbits = sizeof(dkeyid) * 8;
  cryptk.crk_param[4].crp_p = (caddr_t)&pkeyid;
  cryptk.crk_param[4].crp_nbits = sizeof(pkeyid) * 8;
  cryptk.crk_param[5].crp_p = (caddr_t)&qkeyid;
  cryptk.crk_param[5].crp_nbits = sizeof(qkeyid) * 8;
  cryptk.crk_param[6].crp_p = (caddr_t)&dpkeyid;
  cryptk.crk_param[6].crp_nbits = sizeof(dpkeyid) * 8;
  cryptk.crk_param[7].crp_p = (caddr_t)&dqkeyid;
  cryptk.crk_param[7].crp_nbits = sizeof(dqkeyid) * 8;
  cryptk.crk_param[8].crp_p = (caddr_t)&qpkeyid;
  cryptk.crk_param[8].crp_nbits = sizeof(qpkeyid) * 8;
  cryptk.crk_param[9].crp_p = (caddr_t)sig;
  cryptk.crk_param[9].crp_nbits = siglen * 8;
  cryptk.crk_keytype = CRYPTO_KEY_TYPE_PRIVATE;
  cryptk.crk_optype = CRYPTO_OP_SIGN;
  cryptk.crk_flags |= SOP_F_KEYID;

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("rsa calculate failed");
      return -1;
    }

  return cryptk.crk_status;
}

static int test_keymgmt_rsa_verify(FAR crypto_context_t *ctx,
                                   int nkeyid, int ekeyid,
                                   FAR unsigned char *hash, size_t hashlen,
                                   FAR unsigned char *pad, size_t padlen,
                                   FAR unsigned char *sig, size_t siglen)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_op = CRK_RSA_PKCS15_VERIFY;
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;
  cryptk.crk_iparams = 5;
  cryptk.crk_oparams = 0;

  cryptk.crk_param[0].crp_p = (caddr_t)sig;
  cryptk.crk_param[0].crp_nbits = siglen * 8;
  cryptk.crk_param[1].crp_p = (caddr_t)&nkeyid;
  cryptk.crk_param[1].crp_nbits = sizeof(nkeyid) * 8;
  cryptk.crk_param[2].crp_p = (caddr_t)&ekeyid;
  cryptk.crk_param[2].crp_nbits = sizeof(ekeyid) * 8;
  cryptk.crk_param[3].crp_p = (caddr_t)pad;
  cryptk.crk_param[3].crp_nbits = padlen * 8;
  cryptk.crk_param[4].crp_p = (caddr_t)hash;
  cryptk.crk_param[4].crp_nbits = hashlen * 8;
  cryptk.crk_keytype = CRYPTO_KEY_TYPE_PUBLIC;
  cryptk.crk_optype = CRYPTO_OP_VERIFY;
  cryptk.crk_flags |= SOP_F_KEYID;

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("CIOCKEY");
      return -1;
    }

  return cryptk.crk_status;
}

static void test_keymgmt_rsa_2048_sha256(void **state)
{
  unsigned char n[256];
  unsigned char e[6];
  unsigned char d[256];
  unsigned char p[128];
  unsigned char q[128];
  unsigned char dp[128];
  unsigned char dq[128];
  unsigned char qp[128];
  unsigned char cipher[256];
  unsigned char plain[32];
  unsigned char sign[256];
  int nkeyid = -1;
  int ekeyid = -1;
  int dkeyid = -1;
  int pkeyid = -1;
  int qkeyid = -1;
  int dpkeyid = -1;
  int dqkeyid = -1;
  int qpkeyid = -1;

  memset(n, 0, sizeof(n));
  memset(e, 0, sizeof(e));
  memset(d, 0, sizeof(d));
  memset(p, 0, sizeof(p));
  memset(q, 0, sizeof(q));
  memset(dp, 0, sizeof(dp));
  memset(dq, 0, sizeof(dq));
  memset(qp, 0, sizeof(qp));
  memset(cipher, 0, sizeof(cipher));
  memset(plain, 0, sizeof(plain));
  memset(sign, 0, sizeof(sign));
  mpi_from_string(n, g_rsa_n_2048, strlen(g_rsa_n_2048));
  mpi_from_string(e, g_rsa_e_2048, strlen(g_rsa_e_2048));
  mpi_from_string(d, g_rsa_d_2048, strlen(g_rsa_d_2048));
  mpi_from_string(p, g_rsa_p_2048, strlen(g_rsa_p_2048));
  mpi_from_string(q, g_rsa_q_2048, strlen(g_rsa_q_2048));
  mpi_from_string(dp, g_rsa_dp_2048, strlen(g_rsa_dp_2048));
  mpi_from_string(dq, g_rsa_dq_2048, strlen(g_rsa_dq_2048));
  mpi_from_string(qp, g_rsa_qp_2048, strlen(g_rsa_qp_2048));

  assert_int_equal(crypto_init(&g_keymgmt_ctx), 0);

  /* step 1: allocate keyid for n/e/d/p/q/dp/dq/pq */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &nkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &ekeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &dkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &pkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &qkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &dpkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &dqkeyid, 0, 1, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_ALLOCATE_KEY,
                                   &qpkeyid, 0, 1, NULL, 0, TRUE), 0);

  /* step 2: import key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &nkeyid, 2, 0, n, sizeof(n), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &nkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &ekeyid, 2, 0, e, sizeof(e), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &ekeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &dkeyid, 2, 0, d, sizeof(d), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &dkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &pkeyid, 2, 0, p, sizeof(p), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &pkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &qkeyid, 2, 0, q, sizeof(q), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &qkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &dpkeyid, 2, 0, dp, sizeof(dp), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &dpkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &dqkeyid, 2, 0, dq, sizeof(dq), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &dqkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_IMPORT_KEY,
                                   &qpkeyid, 2, 0, qp, sizeof(qp), TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_SAVE_KEY, &qpkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  /* step 3: test RSA-2048 caculate in/out */

  assert_int_equal(test_keymgmt_rsa_crypt(&g_keymgmt_ctx, nkeyid, ekeyid,
                                          dkeyid, pkeyid, qkeyid,
                                          dpkeyid, dqkeyid, qpkeyid,
                                          g_message_sha256,
                                          sizeof(g_message_sha256),
                                          cipher, sizeof(cipher), TRUE), 0);

  assert_int_equal(test_keymgmt_rsa_crypt(&g_keymgmt_ctx, nkeyid, ekeyid,
                                          dkeyid, pkeyid, qkeyid,
                                          dpkeyid, dqkeyid, qpkeyid,
                                          cipher, sizeof(cipher),
                                          plain, sizeof(plain), FALSE), 0);

  assert_int_equal(memcmp(g_message_sha256, plain, sizeof(plain)), 0);

  printf("keymgmt_rsa_2048_sha256 encrypt/decrypt success\n");

  /* step 3: test RSA-2048 sign/verify */

  assert_int_equal(test_keymgmt_rsa_sign(&g_keymgmt_ctx, nkeyid, ekeyid,
                                         dkeyid, pkeyid, qkeyid,
                                         dpkeyid, dqkeyid, qpkeyid,
                                         g_message_sha256,
                                         sizeof(g_message_sha256),
                                         sign, sizeof(sign)), 0);

  assert_int_equal(test_keymgmt_rsa_verify(&g_keymgmt_ctx, nkeyid, ekeyid,
                                           g_message_sha256,
                                           sizeof(g_message_sha256),
                                           NULL, 0, sign, sizeof(sign)), 0);

  printf("keymgmt_rsa_2048_sha256 sign/verify success\n");

  /* step 4: remove key */

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &nkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &ekeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &dkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &pkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &qkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &dpkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &dqkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  assert_int_equal(keyid_operation(&g_keymgmt_ctx, CRK_DELETE_KEY, &qpkeyid,
                                   1, 0, NULL, 0, TRUE), 0);

  crypto_free(&g_keymgmt_ctx);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest keymgmt_tests[] =
    {
      cmocka_unit_test(test_keymgmt_sync),
      cmocka_unit_test(test_keymgmt_async),
      cmocka_unit_test(test_keymgmt_aescbc),
      cmocka_unit_test(test_keymgmt_aescmac),
      cmocka_unit_test(test_keymgmt_rsa_2048_sha256),
    };

  return cmocka_run_group_tests(keymgmt_tests, NULL, NULL);
}
