/****************************************************************************
 * apps/testing/drivers/crypto/crc8.c
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

typedef struct crc8_params
{
  uint8_t polynomial;
  uint8_t initial;
  uint8_t xorout;
  bool reflectin;
  bool reflectout;
}
cp;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const tb g_crc8_testcases[] =
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

static void syscrc8_free(FAR crypto_context *ctx)
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

static int syscrc8_init(FAR crypto_context *ctx)
{
  memset(ctx, 0, sizeof(crypto_context));
  if ((ctx->fd = open("/dev/crypto", O_RDWR, 0)) < 0)
    {
      warn("CRIOGET");
      syscrc8_free(ctx);
      return 1;
    }

  if (ioctl(ctx->fd, CRIOGET, &ctx->crypto_fd) == -1)
    {
      warn("CRIOGET");
      syscrc8_free(ctx);
      return 1;
    }

  return 0;
}

static int syscrc8_start(FAR crypto_context *ctx, FAR cp *key)
{
  ctx->session.mac = CRYPTO_CRC8;
  ctx->session.mackey = (caddr_t) key;
  ctx->session.mackeylen = sizeof(cp);
  if (ioctl(ctx->crypto_fd, CIOCGSESSION, &ctx->session) == -1)
    {
      warn("CIOCGSESSION");
      syscrc8_free(ctx);
      return 1;
    }

  ctx->cryp.ses = ctx->session.ses;
  return 0;
}

static int syscrc8_update(FAR crypto_context *ctx, FAR const char *s,
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

static int syscrc8_finish(FAR crypto_context *ctx, FAR uint8_t *out)
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

static int match(const uint8_t a, const uint8_t b)
{
  if (a == b)
    {
      return 0;
    }

  warnx("crc8 mismatch");

  printf("%02x\n", a);
  printf("%02x\n", b);
  printf("\n");

  return 1;
}

static void test_crc8_calc(FAR cp *params, FAR uint8_t *result)
{
  crypto_context ctx;
  uint8_t output;
  int i;

  assert_int_equal(syscrc8_init(&ctx), 0);

  /* testcase 1-7: test crc8 vector */

  for (i = 0; i < sizeof(g_crc8_testcases) / sizeof(tb) - 1; i++)
    {
      assert_int_equal(syscrc8_start(&ctx, params), 0);
      assert_int_equal(syscrc8_update(&ctx,
                                      g_crc8_testcases[i].data,
                                      g_crc8_testcases[i].datalen), 0);

      assert_int_equal(syscrc8_finish(&ctx, &output), 0);
      assert_int_equal(match(result[i], output), 0);
    }

  /* testcase 8: test segmented computing capabilities in crc8 mode */

  assert_int_equal(syscrc8_start(&ctx, params), 0);
  for (i = 0; i < 8; i++)
    {
      assert_int_equal(syscrc8_update(&ctx,
                                      g_crc8_testcases[7].data,
                                      g_crc8_testcases[7].datalen), 0);
    }

  assert_int_equal(syscrc8_finish(&ctx, &output), 0);
  assert_int_equal(match(result[7], output), 0);
  syscrc8_free(&ctx);
}

static void test_crc8_case1(void **state)
{
  /* CRC-8/Koopman */

  cp params =
    {
      0xa6,
      0x00,
      0x00,
      true,
      false,
    };

  uint8_t result[] =
    {
      0x00, 0xa4, 0xa6, 0x86,
      0x68, 0x96, 0x50, 0x50,
    };

  test_crc8_calc(&params, result);
}

static void test_crc8_case2(void **state)
{
  /* CRC-8/CCITT */

  cp params =
    {
      0x07,
      0x00,
      0x00,
      true,
      false
    };

  uint8_t result[] =
    {
      0x00, 0x9b, 0x0f, 0x0b,
      0xc6, 0x0e, 0x69, 0x69,
    };

  test_crc8_calc(&params, result);
}

static void test_crc8_case3(void **state)
{
  /* CRC-8/SAE J1850 */

  cp params =
    {
      0x1d,
      0xff,
      0xff,
      false,
      false
    };

  uint8_t result[] =
    {
      0x00, 0xb2, 0x9a, 0x2d,
      0xd2, 0x1f, 0x92, 0x92,
    };

  test_crc8_calc(&params, result);
}

static void test_crc8_case4(void **state)
{
  /* CRC-16/AUTOSAR H2F */

  cp params =
    {
      0x2f,
      0xff,
      0xff,
      false,
      false
    };

  uint8_t result[] =
    {
      0x00, 0x07, 0x41, 0xc1,
      0xb4, 0x20, 0x0c, 0x0c,
    };

  test_crc8_calc(&params, result);
}

static void test_crc8_case5(void **state)
{
  /* CRC-8/ROHC */

  cp params =
    {
      0x07,
      0xff,
      0x00,
      true,
      true
    };

  uint8_t result[] =
    {
      0xff, 0x16, 0x24, 0xdc,
      0x39, 0x91, 0x2b, 0x2b,
    };

  test_crc8_calc(&params, result);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest crc8_tests[] =
    {
      cmocka_unit_test(test_crc8_case1),
      cmocka_unit_test(test_crc8_case2),
      cmocka_unit_test(test_crc8_case3),
      cmocka_unit_test(test_crc8_case4),
      cmocka_unit_test(test_crc8_case5),
    };

  return cmocka_run_group_tests(crc8_tests, NULL, NULL);
}
