/****************************************************************************
 * apps/testing/drivers/crypto/crc32.c
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <crypto/cryptodev.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct crypto_context
{
  int fd;
  int crypto_fd;
  struct session_op session;
  struct crypt_op cryp;
}
crypto_context;

typedef struct tb
{
  FAR char *data;
  int datalen;
}
tb;

typedef struct crc32_params
{
  uint32_t polynomial;
  uint32_t initial;
  uint32_t xorout;
  bool reflectin;
  bool reflectout;
}
cp;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const tb g_crc32_testcases[] =
{
    /* testcase 1-7: Individual testing */

    {
      "",
      0,
    },
    {
      "a",
      1,
    },
    {
      "abc",
      3,
    },
    {
      "message digest",
      14,
    },
    {
      "abcdefghijklmnopqrstuvwxyz",
      26,
    },
    {
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
      62,
    },
    {
      "123456789012345678901234567890123456789"
      "01234567890123456789012345678901234567890",
      80,
    },

    /* testcase 8: test case 7 is divided into 8 parts */

    {
      "1234567890",
      10,
    }
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void syscrc32_free(FAR crypto_context *ctx)
{
  if (ctx->crypto_fd != 0)
    {
      close(ctx->crypto_fd);
      ctx->crypto_fd = 0;
    }

  if (ctx->fd != 0)
    {
      close(ctx->fd);
      ctx->fd = 0;
    }
}

static int syscrc32_init(FAR crypto_context *ctx)
{
  memset(ctx, 0, sizeof(crypto_context));
  if ((ctx->fd = open("/dev/crypto", O_RDWR, 0)) < 0)
    {
      warn("CRIOGET");
      syscrc32_free(ctx);
      return 1;
    }

  if (ioctl(ctx->fd, CRIOGET, &ctx->crypto_fd) == -1)
    {
      warn("CRIOGET");
      syscrc32_free(ctx);
      return 1;
    }

  return 0;
}

static int syscrc32_start(FAR crypto_context *ctx, FAR cp *key)
{
  ctx->session.mac = CRYPTO_CRC32;
  ctx->session.mackey = (caddr_t) key;
  ctx->session.mackeylen = sizeof(cp);
  if (ioctl(ctx->crypto_fd, CIOCGSESSION, &ctx->session) == -1)
    {
      warn("CIOCGSESSION");
      syscrc32_free(ctx);
      return 1;
    }

  ctx->cryp.ses = ctx->session.ses;
  return 0;
}

static int syscrc32_update(FAR crypto_context *ctx, FAR const char *s,
                           size_t len)
{
  ctx->cryp.op = COP_ENCRYPT;
  ctx->cryp.flags |= COP_FLAG_UPDATE;
  ctx->cryp.src = (caddr_t) s;
  ctx->cryp.len = len;
  if (ioctl(ctx->crypto_fd, CIOCCRYPT, &ctx->cryp) == -1)
    {
      warn("CIOCCRYPT");
      return 1;
    }

  return 0;
}

static int syscrc32_finish(FAR crypto_context *ctx, FAR uint32_t *out)
{
  ctx->cryp.flags = 0;
  ctx->cryp.mac = (caddr_t) out;

  if (ioctl(ctx->crypto_fd, CIOCCRYPT, &ctx->cryp) == -1)
    {
      warn("CIOCCRYPT");
      return 1;
    }

  if (ioctl(ctx->crypto_fd, CIOCFSESSION, &ctx->session.ses) == -1)
    {
      warn("CIOCFSESSION");
      return 1;
    };

  return 0;
}

static int match(const uint32_t a, const uint32_t b)
{
  if (a == b)
    {
      return 0;
    }

  warnx("crc32 mismatch");

  printf("%02x\n", a);
  printf("%02x\n", b);
  printf("\n");

  return 1;
}

static void test_crc32_calc(FAR cp *params, FAR uint32_t *result)
{
  crypto_context ctx;
  uint32_t output;
  int i;

  assert_int_equal(syscrc32_init(&ctx), 0);

  /* testcase 1-7: test crc32 vector */

  for (i = 0; i < sizeof(g_crc32_testcases) / sizeof(tb) - 1; i++)
    {
      assert_int_equal(syscrc32_start(&ctx, params), 0);
      assert_int_equal(syscrc32_update(&ctx,
                                       g_crc32_testcases[i].data,
                                       g_crc32_testcases[i].datalen), 0);

      assert_int_equal(syscrc32_finish(&ctx, &output), 0);
      assert_int_equal(match(result[i], output), 0);
    }

  /* testcase 8: test segmented computing capabilities in crc32 mode */

  assert_int_equal(syscrc32_start(&ctx, params), 0);
  for (i = 0; i < 8; i++)
    {
      assert_int_equal(syscrc32_update(&ctx,
                                       g_crc32_testcases[7].data,
                                       g_crc32_testcases[7].datalen), 0);
    }

  assert_int_equal(syscrc32_finish(&ctx, &output), 0);
  assert_int_equal(match(result[7], output), 0);
  syscrc32_free(&ctx);
}

static void test_crc32_case1(void **state)
{
  /* CRC-32/ISO-HDLC */

  cp params =
    {
      0xedb88320,
      0x00000000,
      0x00000000,
      true,
      false,
    };

  uint32_t result[] =
    {
      0x00000000, 0xb2364bc0, 0x75ce58c0, 0xbb741ae0,
      0xa24cfcc0, 0x32344b80, 0x121cfd80, 0x121cfd80,
    };

  test_crc32_calc(&params, result);
}

static void test_crc32_case2(void **state)
{
  /* CRC-32/IEEE-802.3 */

  cp params =
    {
      0x04c11db7,
      0xffffffff,
      0xffffffff,
      true,
      true
    };

  uint32_t result[] =
    {
      0x00000000, 0xe8b7be43, 0x352441c2, 0x20159d7f,
      0x4c2750bd, 0x1fc2e6d2, 0x7ca94a72, 0x7ca94a72,
    };

  test_crc32_calc(&params, result);
}

static void test_crc32_case3(void **state)
{
  /* CRC-32/P4 */

  cp params =
    {
      0xf4acfb13,
      0xffffffff,
      0xffffffff,
      true,
      true
    };

  uint32_t result[] =
    {
      0x00000000, 0xaa7b2b08, 0xebe963c8, 0x572300c2,
      0x9bed5706, 0xadd7278e, 0x226aba31, 0x226aba31,
    };

  test_crc32_calc(&params, result);
}

static void test_crc32_case4(void **state)
{
  /* CRC-32C */

  cp params =
    {
      0x1edc6f41,
      0xffffffff,
      0xffffffff,
      true,
      true
    };

  uint32_t result[] =
    {
      0x00000000, 0xc1d04330, 0x364b3fb7, 0x02bd79d0,
      0x9ee6ef25, 0xa245d57d, 0x477a6781, 0x477a6781,
    };

  test_crc32_calc(&params, result);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest crc32_tests[] =
    {
      cmocka_unit_test(test_crc32_case1),
      cmocka_unit_test(test_crc32_case2),
      cmocka_unit_test(test_crc32_case3),
      cmocka_unit_test(test_crc32_case4),
    };

  return cmocka_run_group_tests(crc32_tests, NULL, NULL);
}
