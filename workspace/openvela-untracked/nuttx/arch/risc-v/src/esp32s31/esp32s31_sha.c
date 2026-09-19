/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_sha.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
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
#include "hal/sha_ll.h"
#include "esp32s31_sha.h"

struct s31_sha_session_s
{
  struct s31_sha_session_s *next;
  uint64_t bytes;
  uint32_t state[16];
  uint32_t block[32];
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  uint8_t opad[64];
  bool hmac;
#endif
  uint32_t id;
  uint16_t used;
  uint16_t blocksize;
  uint8_t digestsize;
  uint8_t algorithm;
  esp_sha_type mode;
  bool first;
  bool finished;
};

static mutex_t g_sha_lock = NXMUTEX_INITIALIZER;
static struct s31_sha_session_s *g_sha_sessions;
static uint32_t g_sha_id;

static struct s31_sha_session_s *s31_sha_find(uint32_t id)
{
  struct s31_sha_session_s *session;

  for (session = g_sha_sessions; session != NULL; session = session->next)
    {
      if (session->id == id)
        {
          break;
        }
    }

  return session;
}

static int s31_sha_new(uint32_t *id, struct cryptoini *cri)
{
  struct s31_sha_session_s *session;
  esp_sha_type mode;
  int digestsize;
  int ret;

  if (id == NULL || cri == NULL || cri->cri_next != NULL ||
      (cri->cri_flags & CRD_F_KEYID) != 0)
    {
      return -EINVAL;
    }

  switch (cri->cri_alg)
    {
      case CRYPTO_SHA1:
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
      case CRYPTO_SHA1_HMAC:
#endif
        mode = SHA1;
        digestsize = 20;
        break;
      case CRYPTO_SHA2_256:
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
      case CRYPTO_SHA2_256_HMAC:
#endif
        mode = SHA2_256;
        digestsize = 32;
        break;
      case CRYPTO_SHA2_512:
        mode = SHA2_512;
        digestsize = 64;
        break;
      default:
        return -ENOTSUP;
    }

#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  if (cri->cri_alg == CRYPTO_SHA1_HMAC ||
      cri->cri_alg == CRYPTO_SHA2_256_HMAC)
    {
      /* The original xTS uses ordinary short keys. Longer keys are left
       * to the software driver rather than adding a new key-hashing path.
       */

      if (cri->cri_klen < 0 || cri->cri_klen > 512 ||
          cri->cri_klen % 8 != 0 ||
          (cri->cri_klen != 0 && cri->cri_key == NULL))
        {
          return -ENOTSUP;
        }
    }
  else
#endif
  if (cri->cri_klen != 0)
    {
      return -EINVAL;
    }

  session = kmm_zalloc(sizeof(*session));
  if (session == NULL)
    {
      return -ENOMEM;
    }

  ret = nxmutex_lock(&g_sha_lock);
  if (ret < 0)
    {
      kmm_free(session);
      return ret;
    }

  do
    {
      g_sha_id++;
    }
  while (g_sha_id == 0 || s31_sha_find(g_sha_id) != NULL);

  session->id = g_sha_id;
  session->algorithm = cri->cri_alg;
  session->mode = mode;
  session->digestsize = digestsize;
  session->blocksize = mode == SHA2_512 ? 128 : 64;
  session->first = true;
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  session->hmac = cri->cri_alg == CRYPTO_SHA1_HMAC ||
                  cri->cri_alg == CRYPTO_SHA2_256_HMAC;
  if (session->hmac)
    {
      uint8_t *ipad = (uint8_t *)session->block;
      unsigned int i;

      memset(ipad, 0x36, session->blocksize);
      memset(session->opad, 0x5c, session->blocksize);
      for (i = 0; i < cri->cri_klen / 8; i++)
        {
          ipad[i] ^= cri->cri_key[i];
          session->opad[i] ^= cri->cri_key[i];
        }

      session->used = session->blocksize;
      session->bytes = session->blocksize;
    }
#endif
  session->next = g_sha_sessions;
  g_sha_sessions = session;
  *id = session->id;
  nxmutex_unlock(&g_sha_lock);
  return OK;
}

static int s31_sha_free(uint64_t id)
{
  struct s31_sha_session_s **link;
  struct s31_sha_session_s *session;
  int ret;

  ret = nxmutex_lock(&g_sha_lock);
  if (ret < 0)
    {
      return ret;
    }

  for (link = &g_sha_sessions; *link != NULL; link = &(*link)->next)
    {
      if ((*link)->id == (uint32_t)id)
        {
          session = *link;
          *link = session->next;
          explicit_bzero(session, sizeof(*session));
          kmm_free(session);
          nxmutex_unlock(&g_sha_lock);
          return OK;
        }
    }

  nxmutex_unlock(&g_sha_lock);
  return -EINVAL;
}

/* Caller owns the shared AES/SHA peripheral and the session mutex. Every
 * block uses aligned SRAM, including input originally in a private MMU heap.
 * State words are the HAL's raw register representation, not CPU-endian
 * integers. The reference PIO implementation copies these bytes as digest.
 */

static int s31_sha_block(struct s31_sha_session_s *session)
{
  uint32_t start;
  uint32_t nonzero = 0;
  unsigned int i;

  sha_ll_fill_text_block(session->block, session->blocksize / 4);
  if (session->first)
    {
      sha_ll_start_block(session->mode);
    }
  else
    {
      sha_ll_continue_block(session->mode);
    }

  start = clock_systime_ticks();
  while (sha_ll_busy())
    {
      if ((uint32_t)(clock_systime_ticks() - start) > MSEC2TICK(20))
        {
          return -ETIMEDOUT;
        }
    }

  sha_ll_read_digest(session->mode, session->state, session->digestsize / 4);
  for (i = 0; i < session->digestsize / 4; i++)
    {
      nonzero |= session->state[i];
    }

  if (nonzero == 0)
    {
      return -EIO;
    }

  session->first = false;
  session->used = 0;
  return OK;
}

static int s31_sha_update(struct s31_sha_session_s *session,
                          const uint8_t *input, size_t length)
{
  size_t take;
  int ret;

  if (length > (UINT64_MAX >> 3) - session->bytes)
    {
      return -EOVERFLOW;
    }

  session->bytes += length;
  while (length > 0)
    {
      take = session->blocksize - session->used;
      if (take > length)
        {
          take = length;
        }

      memcpy((uint8_t *)session->block + session->used, input, take);
      session->used += take;
      input += take;
      length -= take;
      if (session->used == session->blocksize)
        {
          ret = s31_sha_block(session);
          if (ret < 0)
            {
              return ret;
            }
        }
    }

  return OK;
}

static int s31_sha_final(struct s31_sha_session_s *session, void *digest)
{
  uint8_t *block = (uint8_t *)session->block;
  uint64_t bits = session->bytes << 3;
  unsigned int limit = session->blocksize -
                       (session->mode == SHA2_512 ? 16 : 8);
  int ret;
  int i;

  block[session->used++] = 0x80;
  if (session->used > limit)
    {
      memset(block + session->used, 0, session->blocksize - session->used);
      ret = s31_sha_block(session);
      if (ret < 0)
        {
          return ret;
        }
    }

  memset(block + session->used, 0, session->blocksize - session->used);
  for (i = 0; i < 8; i++)
    {
      block[session->blocksize - 1 - i] = bits >> (8 * i);
    }

  ret = s31_sha_block(session);
  if (ret == OK)
    {
      memcpy(digest, session->state, session->digestsize);
    }

  return ret;
}

static int s31_sha_process(struct cryptop *crp)
{
  struct s31_sha_session_s *session;
  struct cryptodesc *crd;
  bool update;
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  uint32_t inner[16];
  uint64_t messagebytes;
#endif
  int ret;

  if (crp == NULL || crp->crp_desc == NULL)
    {
      return -EINVAL;
    }

  crd = crp->crp_desc;
  update = (crd->crd_flags & CRD_F_UPDATE) != 0;
  if (crd->crd_next != NULL || crd->crd_skip < 0 || crd->crd_len < 0 ||
      crd->crd_skip > crp->crp_ilen ||
      crd->crd_len > crp->crp_ilen - crd->crd_skip ||
      (crd->crd_flags & ~CRD_F_UPDATE) != 0 ||
      (update && crd->crd_len > 0 && crp->crp_buf == NULL) ||
      (!update && (crd->crd_len != 0 || crp->crp_mac == NULL)))
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_sha_lock);
  if (ret < 0)
    {
      return ret;
    }

  session = s31_sha_find((uint32_t)crp->crp_sid);
  if (session == NULL || session->algorithm != crd->crd_alg ||
      session->finished || (!update && crp->crp_olen < session->digestsize))
    {
      nxmutex_unlock(&g_sha_lock);
      return -EINVAL;
    }

  esp_crypto_sha_aes_lock_acquire();
  PERIPH_RCC_ATOMIC()
    {
      sha_ll_enable_bus_clock(true);
      sha_ll_reset_register();
    }

  sha_ll_set_mode(session->mode);
  if (!session->first)
    {
      sha_ll_write_digest(session->mode, session->state,
                          session->digestsize / 4);
    }

  ret = OK;
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  if (session->hmac && session->first)
    {
      ret = s31_sha_block(session);
    }
#endif

  if (ret == OK && update)
    {
      /* Avoid pointer arithmetic on a NULL empty-update buffer. */

      ret = crd->crd_len == 0 ? OK :
            s31_sha_update(session,
                           (const uint8_t *)crp->crp_buf + crd->crd_skip,
                           crd->crd_len);
    }
  else if (ret == OK)
    {
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
      if (session->hmac)
        {
          messagebytes = session->bytes - session->blocksize;
          ret = s31_sha_final(session, inner);
          if (ret == OK)
            {
              session->first = true;
              session->used = session->blocksize;
              session->bytes = session->blocksize;
              memcpy(session->block, session->opad, session->blocksize);
              ret = s31_sha_block(session);
            }

          if (ret == OK)
            {
              ret = s31_sha_update(session, (const uint8_t *)inner,
                                   session->digestsize);
            }

          if (ret == OK)
            {
              ret = s31_sha_final(session, crp->crp_mac);
            }

          session->bytes = messagebytes;
          explicit_bzero(inner, sizeof(inner));
        }
      else
#endif
        {
          ret = s31_sha_final(session, crp->crp_mac);
        }
    }

  PERIPH_RCC_ATOMIC()
    {
      sha_ll_reset_register();
    }

  esp_crypto_sha_aes_lock_release();
  if (!update || ret < 0)
    {
      char bytecount[21];
      char *digits = bytecount + sizeof(bytecount);
      uint64_t count = session->bytes;

      /* Keep the complete counter without passing a 64-bit variadic value
       * through the RV32 kernel logging path. The operation result is still
       * the actual backend return value, not a logging-derived status.
       */

      *--digits = '\0';
      do
        {
          *--digits = '0' + count % 10;
          count /= 10;
        }
      while (count != 0);

      session->finished = true;
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
      if (session->hmac)
        {
          cryptinfo("S31_HMAC_SHA%d_HW bytes=%s status=%d\n",
                    session->digestsize * 8, digits, ret);
          explicit_bzero(session->opad, sizeof(session->opad));
        }
      else
#endif
        {
          cryptinfo("S31_SHA%d_HW bytes=%s status=%d\n",
                    session->digestsize * 8, digits, ret);
        }
      explicit_bzero(session->block, sizeof(session->block));
      explicit_bzero(session->state, sizeof(session->state));
    }

  nxmutex_unlock(&g_sha_lock);
  return ret;
}

void esp32s31_sha_initialize(void)
{
  int algorithms[CRYPTO_ALGORITHM_MAX + 1] = {0};
  int id;
  int ret;

  id = crypto_get_driverid(0);
  if (id < 0)
    {
      syslog(LOG_ERR, "S31 SHA driver allocation failed: %d\n", id);
      return;
    }

  algorithms[CRYPTO_SHA1] = CRYPTO_ALG_FLAG_SUPPORTED;
  algorithms[CRYPTO_SHA2_256] = CRYPTO_ALG_FLAG_SUPPORTED;
  algorithms[CRYPTO_SHA2_512] = CRYPTO_ALG_FLAG_SUPPORTED;
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  algorithms[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
  algorithms[CRYPTO_SHA2_256_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
#endif
  ret = crypto_register(id, algorithms, s31_sha_new, s31_sha_free,
                        s31_sha_process);
  if (ret < 0)
    {
      syslog(LOG_ERR, "S31 SHA registration failed: %d\n", ret);
      return;
    }

  syslog(LOG_INFO, "S31 hardware crypto: SHA1/SHA256/SHA512 PIO\n");
#ifdef CONFIG_ESP32S31_CRYPTO_HMAC
  syslog(LOG_INFO, "S31 hardware crypto: HMAC-SHA1/256 via SHA PIO\n");
#endif
}
