/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_crypto.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <crypto/cryptodev.h>
#include <nuttx/clock.h>
#include <nuttx/crypto/crypto.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>

#include "esp_crypto_lock.h"
#include "esp_private/periph_ctrl.h"
#include "hal/aes_ll.h"
#include "esp32s31_sha.h"
#include "esp32s31_ecdsa.h"

#define S31_AES_BLOCK_SIZE 16

struct s31_crypto_session_s
{
  struct s31_crypto_session_s *next;
  uint32_t id;
  uint8_t key[64];
  uint8_t iv[S31_AES_BLOCK_SIZE];
  uint8_t keybytes;
  uint8_t algorithm;
  bool started;
};

static mutex_t g_session_lock = NXMUTEX_INITIALIZER;
static struct s31_crypto_session_s *g_sessions;
static uint32_t g_next_session;

static struct s31_crypto_session_s *s31_find_session(uint32_t id)
{
  struct s31_crypto_session_s *session;

  for (session = g_sessions; session != NULL; session = session->next)
    {
      if (session->id == id)
        {
          break;
        }
    }

  return session;
}

static int s31_newsession(uint32_t *id, struct cryptoini *cri)
{
  struct s31_crypto_session_s *session;
  bool valid;
  int ret;

  /* S31 has AES-128/256, not AES-192.  Do not advertise chained algorithms
   * or key IDs: this first backend consumes ordinary volatile session keys.
   */

  if (id == NULL || cri == NULL || cri->cri_next != NULL ||
      cri->cri_key == NULL || (cri->cri_flags & CRD_F_KEYID) != 0)
    {
      return -EINVAL;
    }

  valid = cri->cri_alg == CRYPTO_AES_CBC &&
          (cri->cri_klen == 128 || cri->cri_klen == 256);
#ifdef CONFIG_ESP32S31_CRYPTO_AES_MODES
  valid |= cri->cri_alg == CRYPTO_AES_CTR &&
           (cri->cri_klen == 128 || cri->cri_klen == 256);
  valid |= cri->cri_alg == CRYPTO_AES_XTS &&
           (cri->cri_klen == 256 || cri->cri_klen == 512);
#endif
  if (!valid)
    {
      return -EINVAL;
    }

  session = kmm_zalloc(sizeof(*session));
  if (session == NULL)
    {
      return -ENOMEM;
    }

  ret = nxmutex_lock(&g_session_lock);
  if (ret < 0)
    {
      kmm_free(session);
      return ret;
    }

  do
    {
      g_next_session++;
    }
  while (g_next_session == 0 || s31_find_session(g_next_session) != NULL);

  session->id = g_next_session;
  session->keybytes = cri->cri_klen / 8;
  session->algorithm = cri->cri_alg;
  memcpy(session->key, cri->cri_key, session->keybytes);
  session->next = g_sessions;
  g_sessions = session;
  *id = session->id;
  nxmutex_unlock(&g_session_lock);
  return OK;
}

static int s31_freesession(uint64_t id)
{
  struct s31_crypto_session_s **link;
  struct s31_crypto_session_s *session;
  int ret;

  ret = nxmutex_lock(&g_session_lock);
  if (ret < 0)
    {
      return ret;
    }

  for (link = &g_sessions; *link != NULL; link = &(*link)->next)
    {
      if ((*link)->id == (uint32_t)id)
        {
          session = *link;
          *link = session->next;
          explicit_bzero(session, sizeof(*session));
          kmm_free(session);
          nxmutex_unlock(&g_session_lock);
          return OK;
        }
    }

  nxmutex_unlock(&g_session_lock);
  return -EINVAL;
}

static int s31_aes_block(uint8_t *block)
{
  uint32_t start = clock_systime_ticks();

  aes_ll_write_block(block);
  aes_ll_start_transform();

  /* PIO completes in IDLE; DONE is the DMA completion state.  Keep IRQs
   * enabled and bound the wait, unlike the HAL's unbounded polling helper.
   */

  while (aes_ll_get_state() != ESP_AES_STATE_IDLE)
    {
      if ((uint32_t)(clock_systime_ticks() - start) > MSEC2TICK(20))
        {
          return -ETIMEDOUT;
        }
    }

  aes_ll_read_block(block);
  return OK;
}

static int s31_aes_key(const uint8_t *key, int bytes, bool encrypt)
{
  aes_ll_set_mode(encrypt ? ESP_AES_ENCRYPT : ESP_AES_DECRYPT, bytes);
  return aes_ll_write_key(key, bytes / 4) == bytes ? OK : -EIO;
}

static void s31_xts_advance(uint8_t *tweak)
{
  unsigned int carry = 0;
  unsigned int next;
  int i;

  for (i = 0; i < S31_AES_BLOCK_SIZE; i++)
    {
      next = tweak[i] >> 7;
      tweak[i] = (tweak[i] << 1) | carry;
      carry = next;
    }

  tweak[0] ^= 0x87 & -(int)carry;
}

static int s31_process(struct cryptop *crp)
{
  struct s31_crypto_session_s *session;
  struct cryptodesc *crd;
  const uint8_t *input;
  uint8_t *output;
  uint8_t block[S31_AES_BLOCK_SIZE];
  uint8_t previous[S31_AES_BLOCK_SIZE];
  bool encrypt;
  int remaining;
  int ret;
  int i;

  if (crp == NULL || crp->crp_desc == NULL)
    {
      return -EINVAL;
    }

  crd = crp->crp_desc;
  if (crd->crd_next != NULL ||
      crd->crd_skip < 0 || crd->crd_len < 0 ||
      crd->crd_len % S31_AES_BLOCK_SIZE != 0 ||
      crd->crd_skip > crp->crp_ilen ||
      crd->crd_len > crp->crp_ilen - crd->crd_skip ||
      crd->crd_len > crp->crp_olen ||
      crp->crp_buf == NULL || crp->crp_dst == NULL ||
      (crp->crp_ivlen != 0 && crp->crp_ivlen != S31_AES_BLOCK_SIZE) ||
      (crp->crp_ivlen != 0 && crp->crp_iv == NULL))
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_session_lock);
  if (ret < 0)
    {
      return ret;
    }

  session = s31_find_session((uint32_t)crp->crp_sid);
  if (session == NULL || session->algorithm != crd->crd_alg ||
      (!(crd->crd_flags & CRD_F_UPDATE) &&
       crp->crp_ivlen != S31_AES_BLOCK_SIZE) ||
      ((crd->crd_flags & CRD_F_UPDATE) && !session->started))
    {
      nxmutex_unlock(&g_session_lock);
      return -EINVAL;
    }

  if (!(crd->crd_flags & CRD_F_UPDATE))
    {
      memcpy(session->iv, crp->crp_iv, S31_AES_BLOCK_SIZE);
    }

  /* Share ownership with other HAL AES/SHA consumers.  A session mutex also
   * protects stream state and prevents close racing with an active request.
   */

  esp_crypto_sha_aes_lock_acquire();
  PERIPH_RCC_ATOMIC()
    {
      aes_ll_enable_bus_clock(true);
      aes_ll_reset_register();
    }

  aes_ll_dma_enable(false);
  encrypt = (crd->crd_flags & CRD_F_ENCRYPT) != 0;
  ret = OK;
  if (session->algorithm == CRYPTO_AES_XTS)
    {
      /* Encrypt the data-unit IV with key 2 only at the start. Streaming
       * updates retain the already advanced tweak without re-encrypting it.
       */

      if (!(crd->crd_flags & CRD_F_UPDATE))
        {
          ret = s31_aes_key(session->key + session->keybytes / 2,
                           session->keybytes / 2, true);
          if (ret == OK)
            {
              ret = s31_aes_block(session->iv);
            }
        }

      if (ret == OK)
        {
          ret = s31_aes_key(session->key, session->keybytes / 2, encrypt);
        }
    }
  else
    {
      ret = s31_aes_key(session->key, session->keybytes,
                       encrypt || session->algorithm == CRYPTO_AES_CTR);
    }

  input = (const uint8_t *)crp->crp_buf + crd->crd_skip;
  output = (uint8_t *)crp->crp_dst;
  remaining = crd->crd_len;
  while (ret == OK && remaining > 0)
    {
      if (session->algorithm == CRYPTO_AES_CTR)
        {
          memcpy(block, session->iv, sizeof(block));
          ret = s31_aes_block(block);
          if (ret < 0)
            {
              break;
            }

          for (i = 0; i < S31_AES_BLOCK_SIZE; i++)
            {
              block[i] ^= input[i];
            }

          /* Match cryptodev's RFC 3686 low 32-bit counter semantics. */

          for (i = S31_AES_BLOCK_SIZE - 1; i >= 12; i--)
            {
              if (++session->iv[i] != 0)
                {
                  break;
                }
            }
        }
      else
        {
          memcpy(block, input, sizeof(block));
          if (encrypt || session->algorithm == CRYPTO_AES_XTS)
            {
              for (i = 0; i < S31_AES_BLOCK_SIZE; i++)
                {
                  block[i] ^= session->iv[i];
                }
            }
          else
            {
              memcpy(previous, block, sizeof(previous));
            }

          ret = s31_aes_block(block);
          if (ret < 0)
            {
              break;
            }

          if (session->algorithm == CRYPTO_AES_XTS)
            {
              for (i = 0; i < S31_AES_BLOCK_SIZE; i++)
                {
                  block[i] ^= session->iv[i];
                }

              s31_xts_advance(session->iv);
            }
          else if (encrypt)
            {
              memcpy(session->iv, block, sizeof(block));
            }
          else
            {
              for (i = 0; i < S31_AES_BLOCK_SIZE; i++)
                {
                  block[i] ^= session->iv[i];
                }

              memcpy(session->iv, previous, sizeof(previous));
            }
        }

      memcpy(output, block, sizeof(block));
      input += sizeof(block);
      output += sizeof(block);
      remaining -= sizeof(block);
    }

  /* Remove key and plaintext remnants from the peripheral after use. */

  PERIPH_RCC_ATOMIC()
    {
      aes_ll_reset_register();
    }

  esp_crypto_sha_aes_lock_release();
  session->started = ret == OK;
  if (ret == OK && session->algorithm == CRYPTO_AES_CBC &&
      crp->crp_ivlen == S31_AES_BLOCK_SIZE)
    {
      memcpy(crp->crp_iv, session->iv, S31_AES_BLOCK_SIZE);
    }

  explicit_bzero(block, sizeof(block));
  explicit_bzero(previous, sizeof(previous));
  cryptinfo("S31_AES_%s_HW bytes=%d status=%d\n",
            session->algorithm == CRYPTO_AES_CBC ? "CBC" :
            session->algorithm == CRYPTO_AES_CTR ? "CTR" : "XTS",
            crd->crd_len, ret);
  nxmutex_unlock(&g_session_lock);
  return ret;
}

void hwcr_init(void)
{
  int algorithms[CRYPTO_ALGORITHM_MAX + 1] = {0};
  int id;
  int ret;

#ifdef CONFIG_ESP32S31_CRYPTO_SHA
  esp32s31_sha_initialize();
#endif
#ifdef CONFIG_ESP32S31_CRYPTO_ECDSA_VERIFY
  esp32s31_ecdsa_initialize();
#endif

  id = crypto_get_driverid(0);
  if (id < 0)
    {
      syslog(LOG_ERR, "S31 AES-CBC driver allocation failed: %d\n", id);
      return;
    }

  algorithms[CRYPTO_AES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
#ifdef CONFIG_ESP32S31_CRYPTO_AES_MODES
  algorithms[CRYPTO_AES_CTR] = CRYPTO_ALG_FLAG_SUPPORTED;
  algorithms[CRYPTO_AES_XTS] = CRYPTO_ALG_FLAG_SUPPORTED;
#endif
  ret = crypto_register(id, algorithms, s31_newsession, s31_freesession,
                        s31_process);
  if (ret < 0)
    {
      syslog(LOG_ERR, "S31 AES-CBC registration failed: %d\n", ret);
      return;
    }

  syslog(LOG_INFO, "S31 hardware crypto: AES-CBC 128/256-bit PIO\n");
#ifdef CONFIG_ESP32S31_CRYPTO_AES_MODES
  syslog(LOG_INFO, "S31 hardware crypto: AES-CTR/XTS full-block PIO\n");
#endif
}
