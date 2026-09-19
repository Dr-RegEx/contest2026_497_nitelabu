/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_ecdsa.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <crypto/cryptodev.h>
#include <nuttx/clock.h>
#include <nuttx/crypto/crypto.h>

#include "esp_crypto_lock.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ecc_ll.h"
#include "hal/ecdsa_ll.h"
#include "esp32s31_ecdsa.h"

/* Only verification consumes ordinary public parameters. Do not configure
 * the HAL's eFuse key selection or its hardware-key signing modes here.
 */

static int s31_ecdsa_wait(uint32_t state)
{
  uint32_t start = clock_systime_ticks();

  while (ecdsa_ll_get_state() != state)
    {
      if ((uint32_t)(clock_systime_ticks() - start) > MSEC2TICK(1000))
        {
          return -ETIMEDOUT;
        }
    }

  return OK;
}

static void s31_ecdsa_reset(void)
{
  /* The locked LL reset helper includes an unbounded idle loop. Keep the
   * same reset sequence, but wait separately with IRQs enabled and a timeout.
   */

  PERIPH_RCC_ATOMIC()
    {
      HP_SYS_CLKRST.crypto_ctrl0.reg_crypto_ecdsa_rst_en = 1;
      HP_SYS_CLKRST.crypto_ctrl0.reg_crypto_ecdsa_rst_en = 0;
      HP_SYS_CLKRST.crypto_ctrl0.reg_crypto_rst_en = 0;
    }
}

static int s31_ecdsa_verify(struct cryptkop *krp)
{
  static const uint8_t indexes[5] = {0, 1, 3, 4, 5};
  static const ecdsa_ll_param_t params[5] =
    {
      ECDSA_PARAM_QAX, ECDSA_PARAM_QAY, ECDSA_PARAM_R,
      ECDSA_PARAM_S, ECDSA_PARAM_Z
    };
  uint32_t buffers[5][8];
  const uint8_t *input;
  uint8_t *output;
  int ret;
  int i;
  int j;

  if (krp == NULL)
    {
      return -EINVAL;
    }

  krp->krp_status = -EINVAL;
  if (krp->krp_op != CRK_ECDSA_SECP256R1_VERIFY ||
      krp->krp_iparams != 6 || krp->krp_oparams != 0)
    {
      return OK;
    }

  /* The existing cryptodev ABI passes affine X/Y and leaves parameter 2
   * reserved (its software key generator does not populate this field).
   */

  for (i = 0; i < 5; i++)
    {
      if (krp->krp_param[indexes[i]].crp_nbits != 256 ||
          krp->krp_param[indexes[i]].crp_p == NULL)
        {
          return OK;
        }
    }

  for (i = 0; i < 5; i++)
    {
      input = (const uint8_t *)krp->krp_param[indexes[i]].crp_p;
      output = (uint8_t *)buffers[i];
      for (j = 0; j < 32; j++)
        {
          output[j] = input[31 - j];
        }
    }

  esp_crypto_ecdsa_lock_acquire();
  PERIPH_RCC_ATOMIC()
    {
      ecdsa_ll_enable_bus_clock(true);
      ecc_ll_enable_bus_clock(true);
      ecc_ll_power_up();
      ecc_ll_reset_register();
    }

  s31_ecdsa_reset();
  ret = s31_ecdsa_wait(ECDSA_STATE_IDLE);
  if (ret == OK)
    {
      ecdsa_ll_disable_intr(ECDSA_INT_CALC_DONE);
      ecdsa_ll_disable_intr(ECDSA_INT_SHA_RELEASE);
      ecdsa_ll_set_mode(ECDSA_MODE_SIGN_VERIFY);
      ecdsa_ll_set_curve(ECDSA_CURVE_SECP256R1);
      ecdsa_ll_set_z_mode(ECDSA_Z_USER_PROVIDED);
      ecdsa_ll_set_stage(ECDSA_STAGE_START_CALC);
      ret = s31_ecdsa_wait(ECDSA_STATE_LOAD);
    }

  if (ret == OK)
    {
      for (i = 0; i < 5; i++)
        {
          ecdsa_ll_write_param(params[i], (const uint8_t *)buffers[i], 32);
        }

      ecdsa_ll_set_stage(ECDSA_STAGE_LOAD_DONE);
      ret = s31_ecdsa_wait(ECDSA_STATE_IDLE);
    }

  if (ret == OK)
    {
      /* Match cryptodev: zero means valid, nonzero rejects the signature. */

      ret = ecdsa_ll_get_operation_result() ? OK : 1;
    }

  s31_ecdsa_reset();
  PERIPH_RCC_ATOMIC()
    {
      ecc_ll_reset_register();
    }

  esp_crypto_ecdsa_lock_release();
  explicit_bzero(buffers, sizeof(buffers));
  krp->krp_status = ret;
  cryptinfo("S31_ECDSA_P256_VERIFY_HW status=%d\n", ret);
  return OK;
}

void esp32s31_ecdsa_initialize(void)
{
  int algorithms[CRK_ALGORITHM_MAX + 1] = {0};
  int id;
  int ret;

  id = crypto_get_driverid(0);
  if (id < 0)
    {
      syslog(LOG_ERR, "S31 ECDSA driver allocation failed: %d\n", id);
      return;
    }

  algorithms[CRK_ECDSA_SECP256R1_VERIFY] = CRYPTO_ALG_FLAG_SUPPORTED;
  ret = crypto_kregister(id, algorithms, s31_ecdsa_verify);
  if (ret < 0)
    {
      syslog(LOG_ERR, "S31 ECDSA registration failed: %d\n", ret);
      return;
    }

  syslog(LOG_INFO, "S31 hardware crypto: ECDSA P256 verify only\n");
}
