/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_ecc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/clock.h>
#include <nuttx/crypto/crypto.h>

#include "esp_crypto_lock.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ecc_ll.h"

static void s31_ecc_clear(void)
{
  uint32_t zero[12] = {0};

  ecc_ll_write_param(ECC_PARAM_PX, (const uint8_t *)zero, sizeof(zero));
  ecc_ll_write_param(ECC_PARAM_PY, (const uint8_t *)zero, sizeof(zero));
  ecc_ll_write_param(ECC_PARAM_K, (const uint8_t *)zero, sizeof(zero));
  ecc_ll_write_param(ECC_PARAM_QX, (const uint8_t *)zero, sizeof(zero));
  ecc_ll_write_param(ECC_PARAM_QY, (const uint8_t *)zero, sizeof(zero));
  ecc_ll_write_param(ECC_PARAM_QZ, (const uint8_t *)zero, sizeof(zero));
}

int up_ecc_p256_point_mult(unsigned char result[64],
                           const unsigned char point[64],
                           const unsigned char scalar[32])
{
  uint32_t x[8];
  uint32_t y[8];
  uint32_t key[8];
  uint32_t start;
  unsigned int nonzero = 0;
  int ret = OK;
  int i;

  if (result == NULL || point == NULL || scalar == NULL)
    {
      return -EINVAL;
    }

  for (i = 0; i < 32; i++)
    {
      ((uint8_t *)x)[i] = point[31 - i];
      ((uint8_t *)y)[i] = point[63 - i];
      ((uint8_t *)key)[i] = scalar[31 - i];
      nonzero |= scalar[i];
    }

  if (nonzero == 0)
    {
      explicit_bzero(key, sizeof(key));
      return -EINVAL;
    }

  esp_crypto_ecc_lock_acquire();
  PERIPH_RCC_ATOMIC()
    {
      ecc_ll_enable_bus_clock(true);
      ecc_ll_power_up();
      ecc_ll_reset_register();
    }

  ecc_ll_disable_interrupt();
  ecc_ll_clear_interrupt();
  s31_ecc_clear();
  ecc_ll_set_curve(ECC_CURVE_SECP256R1);
  ecc_ll_set_mode(ECC_MODE_VERIFY_THEN_POINT_MUL);
  ecc_ll_enable_constant_time_point_mul(true);
  ecc_ll_write_param(ECC_PARAM_K, (const uint8_t *)key, sizeof(key));
  ecc_ll_write_param(ECC_PARAM_PX, (const uint8_t *)x, sizeof(x));
  ecc_ll_write_param(ECC_PARAM_PY, (const uint8_t *)y, sizeof(y));
  ecc_ll_start_calc();
  start = clock_systime_ticks();
  while (!ecc_ll_is_calc_finished())
    {
      if ((uint32_t)(clock_systime_ticks() - start) > MSEC2TICK(1000))
        {
          ret = -ETIMEDOUT;
          break;
        }
    }

  if (ret == OK && !ecc_ll_get_verification_result())
    {
      ret = -EINVAL;
    }

  if (ret == OK)
    {
      ecc_ll_read_param(ECC_PARAM_PX, (uint8_t *)x, sizeof(x));
      ecc_ll_read_param(ECC_PARAM_PY, (uint8_t *)y, sizeof(y));
      for (i = 0; i < 32; i++)
        {
          result[i] = ((uint8_t *)x)[31 - i];
          result[i + 32] = ((uint8_t *)y)[31 - i];
        }
    }

  PERIPH_RCC_ATOMIC()
    {
      ecc_ll_reset_register();
    }

  s31_ecc_clear();
  esp_crypto_ecc_lock_release();
  explicit_bzero(key, sizeof(key));
  explicit_bzero(x, sizeof(x));
  explicit_bzero(y, sizeof(y));
  cryptinfo("S31_ECC_P256_POINT_HW status=%d\n", ret);
  return ret;
}
