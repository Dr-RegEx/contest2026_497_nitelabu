/****************************************************************************
 * apps/testing/drivers/crypto/rsa.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <crypto/cryptodev.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SHA256_PLAIN_LEN         32
#define MOD_512_LEN              64
#define PRIVATE_512_KEYLEN       64
#define PRIME_512_LEN            32
#define PADDING_512_SHA256_LEN   (MOD_512_LEN - SHA256_PLAIN_LEN)
#define MOD_2048_LEN             256
#define PRIVATE_2048_KEYLEN      256
#define PRIME_2048_LEN           128
#define PADDING_2048_SHA256_LEN  (MOD_2048_LEN - SHA256_PLAIN_LEN)

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct rsa_data_s
{
  unsigned char *e;
  unsigned char *d;
  unsigned char *n;
  unsigned char *p;
  unsigned char *q;
  unsigned char *dp;
  unsigned char *dq;
  unsigned char *qp;
  size_t elen;
  size_t dlen;
  size_t nlen;
  size_t plen;
  size_t qlen;
  size_t dplen;
  size_t dqlen;
  size_t qplen;
}
rsa_data_t;

typedef struct crypto_context_s
{
  int fd;
  int cryptodev_fd;
}
crypto_context_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Example RSA-512 keypair, for test purposes */

static const char g_rsa_n_512[] =
"883E99178C6AE77BD396E6183A51DCDA"
"E03BD5C3BBB8D6EE632350CAECC3E2E5"
"93ECB808CFB25E087E8C7A675498F29D"
"7D3BD30D2E46C99190276D40244EC58B";

static const char g_rsa_e_512[] = "010001";

static const char g_rsa_d_512[] =
"03A9581A1B2F0DC4D1F8E8C0AE4C31A4"
"8E09137229EBB8DC73F6B44782BF3EC3"
"653A9F8203A5182C056C4CEE34116165"
"846ECA0457812F55E5A7AE362047BED1";

static const char g_rsa_p_512[] =
"C010A09FD38DDD6FDC6B0EC03584319A"
"7DEAD79E151E89153C4A87984F875CED";

static const char g_rsa_q_512[] =
"B599121C7EE6D24E31A9A0ABF89595C0"
"88084B80A2DBD5A45394AA24AE51D557";

static const char g_rsa_dp_512[] =
"15CC4D4E96660D71BAA473E220B0C628"
"708E0CB6A652F6DFF01DCD51DE04D0E9";

static const char g_rsa_dq_512[] =
"4046EEF7311FC85EAF0B6878966373FF"
"12E744BA426AE782E3A6A2569B7111BF";

static const char g_rsa_qp_512[] =
"01495732FA39FFAECB82EBADF4DA1B1C"
"A441E0D4588CB45CA1F7A3CEDDE7C987";

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

static const unsigned char g_message_sha256[] =
"CDC76E5C9914FB9281A1C7E284D73E67"
"F1809A48A497200E046D39CCC7112CD0";

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void pkcs1_v15_padding(FAR unsigned char *padding,
                              size_t pad_len)
{
  int i;

  /* this padding is only for rsa sign */

  *padding++ = 0;
  *padding++ = 0x02;
  for (i = 0; i < pad_len - 3; i++)
    {
      *padding++ = rand() % 256;
    }

  *padding++ = 0;
}

static void rsa_be32toh(FAR uint8_t *dst, size_t len, FAR const uint8_t *src)
{
  int i;
  size_t rlen = len / 4;
  uint32_t *dst32 = (uint32_t *)dst;
  uint32_t *src32 = (uint32_t *)src;

  for (i = 0; i < rlen; i++)
    {
      dst32[i] = be32toh(src32[rlen - i - 1]);
    }
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

static void rsa_data_free(FAR rsa_data_t *data)
{
  if (data->e)
    {
      free(data->e);
    }

  if (data->d)
    {
      free(data->d);
    }

  if (data->n)
    {
      free(data->n);
    }

  if (data->p)
    {
      free(data->p);
    }

  if (data->q)
    {
      free(data->q);
    }

  if (data->dp)
    {
      free(data->dp);
    }

  if (data->dq)
    {
      free(data->dq);
    }

  if (data->qp)
    {
      free(data->qp);
    }
}

static int rsa_data_init(FAR rsa_data_t *data,
                         FAR const char *e, size_t elen,
                         FAR const char *d, size_t dlen,
                         FAR const char *n, size_t nlen,
                         FAR const char *p, size_t plen,
                         FAR const char *q, size_t qlen,
                         FAR const char *dp, size_t dplen,
                         FAR const char *dq, size_t dqlen,
                         FAR const char *qp, size_t qplen)
{
  data->elen = elen;
  data->dlen = dlen;
  data->nlen = nlen;
  data->plen = plen;
  data->qlen = qlen;
  data->dplen = dplen;
  data->dqlen = dqlen;
  data->qplen = qplen;
  data->e = zalloc(elen);
  data->d = zalloc(dlen);
  data->n = zalloc(nlen);
  data->p = zalloc(plen);
  data->q = zalloc(qlen);
  data->dp = zalloc(dplen);
  data->dq = zalloc(dqlen);
  data->qp = zalloc(qplen);
  if (!data->e || !data->d || !data->n || !data->p ||
      !data->q || !data->dp || !data->dq || !data->qp)
    {
      rsa_data_free(data);
      return -1;
    }

  mpi_from_string(data->e, e, strlen(e));
  mpi_from_string(data->d, d, strlen(d));
  mpi_from_string(data->n, n, strlen(n));
  mpi_from_string(data->p, p, strlen(p));
  mpi_from_string(data->q, q, strlen(q));
  mpi_from_string(data->dp, dp, strlen(dp));
  mpi_from_string(data->dq, dq, strlen(dq));
  mpi_from_string(data->qp, qp, strlen(qp));
  return 0;
}

static void cryptodev_free(FAR crypto_context_t *ctx)
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

static int cryptodev_init(FAR crypto_context_t *ctx)
{
  memset(ctx, 0, sizeof(crypto_context_t));
  if ((ctx->fd = open("/dev/crypto", O_RDWR, 0)) < 0)
    {
      perror("CRIOGET");
      cryptodev_free(ctx);
      return 1;
    }

  if (ioctl(ctx->fd, CRIOGET, &ctx->cryptodev_fd) == -1)
    {
      perror("CRIOGET");
      cryptodev_free(ctx);
      return 1;
    }

  return 0;
}

static int
cryptodev_rsa_calc_crt(FAR crypto_context_t *ctx, FAR rsa_data_t *data,
                       FAR const unsigned char *input, size_t ilen,
                       FAR const unsigned char *output, size_t olen,
                       bool decrypt)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;

  cryptk.crk_param[0].crp_p = (caddr_t)input;
  cryptk.crk_param[0].crp_nbits = ilen * 8;
  if (decrypt)
    {
      cryptk.crk_iparams = 9;
      cryptk.crk_oparams = 1;
      cryptk.crk_param[1].crp_p = (caddr_t)data->n;
      cryptk.crk_param[1].crp_nbits = data->nlen * 8;
      cryptk.crk_param[2].crp_p = (caddr_t)data->e;
      cryptk.crk_param[2].crp_nbits = data->elen * 8;
      cryptk.crk_param[3].crp_p = (caddr_t)data->d;
      cryptk.crk_param[3].crp_nbits = data->dlen * 8;
      cryptk.crk_param[4].crp_p = (caddr_t)data->p;
      cryptk.crk_param[4].crp_nbits = data->plen * 8;
      cryptk.crk_param[5].crp_p = (caddr_t)data->q;
      cryptk.crk_param[5].crp_nbits = data->qlen * 8;
      cryptk.crk_param[6].crp_p = (caddr_t)data->dp;
      cryptk.crk_param[6].crp_nbits = data->dplen * 8;
      cryptk.crk_param[7].crp_p = (caddr_t)data->dq;
      cryptk.crk_param[7].crp_nbits = data->dqlen * 8;
      cryptk.crk_param[8].crp_p = (caddr_t)data->qp;
      cryptk.crk_param[8].crp_nbits = data->qplen * 8;
      cryptk.crk_param[9].crp_p = (caddr_t)output;
      cryptk.crk_param[9].crp_nbits = olen * 8;
      cryptk.crk_op = CRK_MOD_EXP_CRT;
      cryptk.crk_keytype = CRYPTO_KEY_TYPE_PRIVATE;
      cryptk.crk_optype = CRYPTO_OP_DECRYPT;
    }
  else
    {
      cryptk.crk_iparams = 3;
      cryptk.crk_oparams = 1;
      cryptk.crk_param[1].crp_p = (caddr_t)data->n;
      cryptk.crk_param[1].crp_nbits = data->nlen * 8;
      cryptk.crk_param[2].crp_p = (caddr_t)data->e;
      cryptk.crk_param[2].crp_nbits = data->elen * 8;
      cryptk.crk_param[3].crp_p = (caddr_t)output;
      cryptk.crk_param[3].crp_nbits = olen * 8;
      cryptk.crk_op = CRK_MOD_EXP;
      cryptk.crk_keytype = CRYPTO_KEY_TYPE_PUBLIC;
      cryptk.crk_optype = CRYPTO_OP_ENCRYPT;
    }

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("rsa calculate failed");
      return -1;
    }

  return cryptk.crk_status;
}

static int cryptodev_rsa_sign(
            FAR crypto_context_t *ctx, FAR rsa_data_t *data,
            FAR const unsigned char *sig, size_t sig_len,
            FAR const unsigned char *hash, size_t hash_len,
            FAR const unsigned char *padding, size_t pad_len)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_op = CRK_RSA_PKCS15_SIGN;
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;
  cryptk.crk_iparams = 9;
  cryptk.crk_oparams = 1;

  cryptk.crk_param[0].crp_p = (caddr_t)hash;
  cryptk.crk_param[0].crp_nbits = hash_len * 8;
  cryptk.crk_param[1].crp_p = (caddr_t)data->n;
  cryptk.crk_param[1].crp_nbits = data->nlen * 8;
  cryptk.crk_param[2].crp_p = (caddr_t)data->e;
  cryptk.crk_param[2].crp_nbits = data->elen * 8;
  cryptk.crk_param[3].crp_p = (caddr_t)data->d;
  cryptk.crk_param[3].crp_nbits = data->dlen * 8;
  cryptk.crk_param[4].crp_p = (caddr_t)data->p;
  cryptk.crk_param[4].crp_nbits = data->plen * 8;
  cryptk.crk_param[5].crp_p = (caddr_t)data->q;
  cryptk.crk_param[5].crp_nbits = data->qlen * 8;
  cryptk.crk_param[6].crp_p = (caddr_t)data->dp;
  cryptk.crk_param[6].crp_nbits = data->dplen * 8;
  cryptk.crk_param[7].crp_p = (caddr_t)data->dq;
  cryptk.crk_param[7].crp_nbits = data->dqlen * 8;
  cryptk.crk_param[8].crp_p = (caddr_t)data->qp;
  cryptk.crk_param[8].crp_nbits = data->qplen * 8;
  cryptk.crk_param[9].crp_p = (caddr_t)sig;
  cryptk.crk_param[9].crp_nbits = sig_len * 8;
  cryptk.crk_keytype = CRYPTO_KEY_TYPE_PRIVATE;
  cryptk.crk_optype = CRYPTO_OP_SIGN;

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("rsa calculate failed");
      return -1;
    }

  return cryptk.crk_status;
}

static int cryptodev_rsa_verify(
            FAR crypto_context_t *ctx, FAR rsa_data_t *data,
            FAR const unsigned char *sig, size_t sig_len,
            FAR const unsigned char *hash, size_t hash_len,
            FAR const unsigned char *padding, size_t pad_len)
{
  struct crypt_kop cryptk;

  memset(&cryptk, 0, sizeof(struct crypt_kop));
  cryptk.crk_op = CRK_RSA_PKCS15_VERIFY;
  cryptk.crk_padding = CRYPTO_RSA_PKCS1_PADDING;
  cryptk.crk_hash = CRYPTO_RSA_SHA256;
  cryptk.crk_iparams = 5;
  cryptk.crk_oparams = 0;

  cryptk.crk_param[0].crp_p = (caddr_t)sig;
  cryptk.crk_param[0].crp_nbits = sig_len * 8;
  cryptk.crk_param[1].crp_p = (caddr_t)data->n;
  cryptk.crk_param[1].crp_nbits = data->nlen * 8;
  cryptk.crk_param[2].crp_p = (caddr_t)data->e;
  cryptk.crk_param[2].crp_nbits = data->elen * 8;
  cryptk.crk_param[3].crp_p = (caddr_t)padding;
  cryptk.crk_param[3].crp_nbits = pad_len * 8;
  cryptk.crk_param[4].crp_p = (caddr_t)hash;
  cryptk.crk_param[4].crp_nbits = hash_len * 8;
  cryptk.crk_keytype = CRYPTO_KEY_TYPE_PUBLIC;
  cryptk.crk_optype = CRYPTO_OP_VERIFY;

  if (ioctl(ctx->cryptodev_fd, CIOCKEY, &cryptk) == -1)
    {
      perror("CIOCKEY");
      return -1;
    }

  return cryptk.crk_status;
}

static void test_rsa_512_sha256(void **state)
{
  crypto_context_t ctx;
  rsa_data_t rsa_data;
  unsigned char cipher[MOD_512_LEN];
  unsigned char plain[SHA256_PLAIN_LEN];
  unsigned char data[MOD_512_LEN];
  unsigned char padding[PADDING_512_SHA256_LEN];
  unsigned char sig[MOD_512_LEN];
  unsigned char data_he[MOD_512_LEN];
  unsigned char pad_he[PADDING_512_SHA256_LEN];
  unsigned char hash_he[SHA256_PLAIN_LEN];

  assert_int_equal(cryptodev_init(&ctx), 0);

  assert_int_equal(rsa_data_init(&rsa_data,
                                 g_rsa_e_512, strlen(g_rsa_e_512),
                                 g_rsa_d_512, MOD_512_LEN,
                                 g_rsa_n_512, MOD_512_LEN,
                                 g_rsa_p_512, PRIME_512_LEN,
                                 g_rsa_q_512, PRIME_512_LEN,
                                 g_rsa_dp_512, PRIME_512_LEN,
                                 g_rsa_dq_512, PRIME_512_LEN,
                                 g_rsa_qp_512, PRIME_512_LEN), 0);

  /* test 1: encrypt with private key and decrypt with public key */

  memset(cipher, 0, sizeof(cipher));
  memset(plain, 0, sizeof(plain));

  /* encrypt with public key: g_message ^ ctx.data.e % n = cipher */

  assert_int_equal(cryptodev_rsa_calc_crt(&ctx, &rsa_data, g_message_sha256,
                                          SHA256_PLAIN_LEN, cipher,
                                          MOD_512_LEN, FALSE), 0);

  /* dencrypt with private key: cipher ^ ctx.data.e % n = plain */

  assert_int_equal(cryptodev_rsa_calc_crt(&ctx, &rsa_data, cipher,
                                          MOD_512_LEN, plain,
                                          SHA256_PLAIN_LEN, TRUE), 0);

  assert_int_equal(memcmp(g_message_sha256, plain, SHA256_PLAIN_LEN), 0);

  printf("test_rsa_512_sha256 encrypt/decrypt success\n");

  /* test 2: rsa sign and verify */

  memset(data, 0, sizeof(data));
  memset(padding, 0, sizeof(padding));
  memset(sig, 0, sizeof(sig));

  /* pkcs15 v1 padding for sign: 0x00 0x02 (... random) 0x00 */

  pkcs1_v15_padding(padding, PADDING_512_SHA256_LEN);
  memcpy(data, padding, PADDING_512_SHA256_LEN);
  memcpy(data + PADDING_512_SHA256_LEN, g_message_sha256, SHA256_PLAIN_LEN);

  rsa_be32toh(data_he, MOD_512_LEN, data);
  rsa_be32toh(pad_he, PADDING_512_SHA256_LEN, padding);
  rsa_be32toh(hash_he, SHA256_PLAIN_LEN, g_message_sha256);

  /* sign with private key: (hash + padding) ^ d % n = sig */

  assert_int_equal(cryptodev_rsa_sign(&ctx, &rsa_data, sig, sizeof(sig),
                                      hash_he, sizeof(hash_he),
                                      pad_he, sizeof(pad_he)), 0);

  /* verify with public key: sig ^ e % n = (hash + padding) */

  assert_int_equal(cryptodev_rsa_verify(&ctx, &rsa_data, sig, sizeof(sig),
                                        hash_he, sizeof(hash_he),
                                        pad_he, sizeof(pad_he)), 0);

  printf("test_rsa_512_sha256 sign/verify success\n");

  rsa_data_free(&rsa_data);
  cryptodev_free(&ctx);
}

static void test_rsa_2048_sha256(void **state)
{
  crypto_context_t ctx;
  rsa_data_t rsa_data;
  unsigned char cipher[MOD_2048_LEN];
  unsigned char plain[SHA256_PLAIN_LEN];
  unsigned char data[MOD_2048_LEN];
  unsigned char padding[PADDING_2048_SHA256_LEN];
  unsigned char sig[MOD_2048_LEN];
  unsigned char data_he[MOD_2048_LEN];
  unsigned char pad_he[PADDING_2048_SHA256_LEN];
  unsigned char hash_he[SHA256_PLAIN_LEN];

  assert_int_equal(cryptodev_init(&ctx), 0);

  assert_int_equal(rsa_data_init(&rsa_data,
                                 g_rsa_e_2048, strlen(g_rsa_e_2048),
                                 g_rsa_d_2048, MOD_2048_LEN,
                                 g_rsa_n_2048, MOD_2048_LEN,
                                 g_rsa_p_2048, PRIME_2048_LEN,
                                 g_rsa_q_2048, PRIME_2048_LEN,
                                 g_rsa_dp_2048, PRIME_2048_LEN,
                                 g_rsa_dq_2048, PRIME_2048_LEN,
                                 g_rsa_qp_2048, PRIME_2048_LEN), 0);

  /* test 1: encrypt with private key and decrypt with public key */

  memset(cipher, 0, sizeof(cipher));
  memset(plain, 0, sizeof(plain));

  /* encrypt with public key: g_message ^ ctx.data.e % n = cipher */

  assert_int_equal(cryptodev_rsa_calc_crt(&ctx, &rsa_data, g_message_sha256,
                                          SHA256_PLAIN_LEN, cipher,
                                          MOD_2048_LEN, FALSE), 0);

  /* dencrypt with private key: cipher ^ ctx.data.e % n = plain */

  assert_int_equal(cryptodev_rsa_calc_crt(&ctx, &rsa_data, cipher,
                                          MOD_2048_LEN, plain,
                                          SHA256_PLAIN_LEN, TRUE), 0);

  assert_int_equal(memcmp(g_message_sha256, plain, SHA256_PLAIN_LEN), 0);

  printf("test_rsa_2048_sha256 encrypt/decrypt success\n");

  /* test 2: rsa sign and verify */

  memset(data, 0, sizeof(data));
  memset(padding, 0, sizeof(padding));
  memset(sig, 0, sizeof(sig));

  /* pkcs15 v1 padding for sign: 0x00 0x02 (... random) 0x00 */

  pkcs1_v15_padding(padding, PADDING_2048_SHA256_LEN);
  memcpy(data, padding, PADDING_2048_SHA256_LEN);
  memcpy(data + PADDING_2048_SHA256_LEN, g_message_sha256, SHA256_PLAIN_LEN);

  rsa_be32toh(data_he, MOD_2048_LEN, data);
  rsa_be32toh(pad_he, PADDING_2048_SHA256_LEN, padding);
  rsa_be32toh(hash_he, SHA256_PLAIN_LEN, g_message_sha256);

  /* sign with private key: (hash + padding) ^ d % n = sig */

  assert_int_equal(cryptodev_rsa_sign(&ctx, &rsa_data, sig, sizeof(sig),
                                      hash_he, sizeof(hash_he),
                                      pad_he, sizeof(pad_he)), 0);

  /* verify with public key: sig ^ e % n = (hash + padding) */

  assert_int_equal(cryptodev_rsa_verify(&ctx, &rsa_data, sig, sizeof(sig),
                                        hash_he, sizeof(hash_he),
                                        pad_he, sizeof(pad_he)), 0);

  printf("test_rsa_2048_sha256 sign/verify success\n");

  rsa_data_free(&rsa_data);
  cryptodev_free(&ctx);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest rsa_tests[] =
    {
      cmocka_unit_test(test_rsa_512_sha256),
      cmocka_unit_test(test_rsa_2048_sha256),
    };

  return cmocka_run_group_tests(rsa_tests, NULL, NULL);
}
