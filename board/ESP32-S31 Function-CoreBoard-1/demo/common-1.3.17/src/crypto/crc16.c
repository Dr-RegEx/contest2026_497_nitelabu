/****************************************************************************
 * apps/testing/drivers/crypto/crc16.c
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

typedef struct crc16_params
{
  uint16_t polynomial;
  uint16_t initial;
  uint16_t xorout;
  bool reflectin;
  bool reflectout;
}
cp;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const tb g_crc16_testcases[] =
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

static void syscrc16_free(FAR crypto_context *ctx)
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

static int syscrc16_init(FAR crypto_context *ctx)
{
  memset(ctx, 0, sizeof(crypto_context));
  if ((ctx->fd = open("/dev/crypto", O_RDWR, 0)) < 0)
    {
      warn("CRIOGET");
      syscrc16_free(ctx);
      return 1;
    }

  if (ioctl(ctx->fd, CRIOGET, &ctx->crypto_fd) == -1)
    {
      warn("CRIOGET");
      syscrc16_free(ctx);
      return 1;
    }

  return 0;
}

static int syscrc16_start(FAR crypto_context *ctx, FAR cp *key)
{
  ctx->session.mac = CRYPTO_CRC16;
  ctx->session.mackey = (caddr_t) key;
  ctx->session.mackeylen = sizeof(cp);
  if (ioctl(ctx->crypto_fd, CIOCGSESSION, &ctx->session) == -1)
    {
      warn("CIOCGSESSION");
      syscrc16_free(ctx);
      return 1;
    }

  ctx->cryp.ses = ctx->session.ses;
  return 0;
}

static int syscrc16_update(FAR crypto_context *ctx, FAR const char *s,
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

static int syscrc16_finish(FAR crypto_context *ctx, FAR uint16_t *out)
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

static int match(const uint16_t a, const uint16_t b)
{
  if (a == b)
    {
      return 0;
    }

  warnx("crc16 mismatch");

  printf("%02x\n", a);
  printf("%02x\n", b);
  printf("\n");

  return 1;
}

static void test_crc16_calc(FAR cp *params, FAR uint16_t *result)
{
  crypto_context ctx;
  uint16_t output;
  int i;

  assert_int_equal(syscrc16_init(&ctx), 0);

  /* testcase 1-7: test crc16 vector */

  for (i = 0; i < sizeof(g_crc16_testcases) / sizeof(tb) - 1; i++)
    {
      assert_int_equal(syscrc16_start(&ctx, params), 0);
      assert_int_equal(syscrc16_update(&ctx,
                                       g_crc16_testcases[i].data,
                                       g_crc16_testcases[i].datalen), 0);

      assert_int_equal(syscrc16_finish(&ctx, &output), 0);
      assert_int_equal(match(result[i], output), 0);
    }

  /* testcase 8: test segmented computing capabilities in crc16 mode */

  assert_int_equal(syscrc16_start(&ctx, params), 0);
  for (i = 0; i < 8; i++)
    {
      assert_int_equal(syscrc16_update(&ctx,
                                       g_crc16_testcases[7].data,
                                       g_crc16_testcases[7].datalen), 0);
    }

  assert_int_equal(syscrc16_finish(&ctx, &output), 0);
  assert_int_equal(match(result[7], output), 0);
  syscrc16_free(&ctx);
}

static void test_crc16_case1(void **state)
{
  /* CRC-16/CCITT */

  cp params =
    {
      0x1021,
      0x0000,
      0x0000,
      true,
      true,
    };

  uint16_t result[] =
    {
      0x0000, 0x728f, 0x58e9, 0x7bd5,
      0x80b0, 0xd7d5, 0x4375, 0x4375,
    };

  test_crc16_calc(&params, result);
}

static void test_crc16_case2(void **state)
{
  /* CRC-16/CCITT-FALSE */

  cp params =
    {
      0x1021,
      0xffff,
      0x0000,
      false,
      false
    };

  uint16_t result[] =
    {
      0xffff, 0x9d77, 0x514a, 0x32cc,
      0x53e2, 0xb46b, 0xdadf, 0xdadf,
    };

  test_crc16_calc(&params, result);
}

static void test_crc16_case3(void **state)
{
  /* CRC-16/IBM */

  cp params =
    {
      0x8005,
      0x0000,
      0x0000,
      true,
      true
    };

  uint16_t result[] =
    {
      0x0000, 0xe8c1, 0x9738, 0x3b44,
      0x9c1d, 0x57b4, 0x5ec7, 0x5ec7,
    };

  test_crc16_calc(&params, result);
}

static void test_crc16_case4(void **state)
{
  /* CRC-16/XMODEM */

  cp params =
    {
      0x1021,
      0x0000,
      0x0000,
      false,
      false
    };

  uint16_t result[] =
    {
      0x0000, 0x7c87, 0x9dd6, 0x9ba6,
      0x63ac, 0x7db0, 0xe73a, 0xe73a,
    };

  test_crc16_calc(&params, result);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest crc16_tests[] =
    {
      cmocka_unit_test(test_crc16_case1),
      cmocka_unit_test(test_crc16_case2),
      cmocka_unit_test(test_crc16_case3),
      cmocka_unit_test(test_crc16_case4),
    };

  return cmocka_run_group_tests(crc16_tests, NULL, NULL);
}
