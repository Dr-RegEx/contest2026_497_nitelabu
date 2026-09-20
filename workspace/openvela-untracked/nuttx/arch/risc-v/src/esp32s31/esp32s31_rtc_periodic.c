/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_rtc_periodic.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <nuttx/clock.h>
#include <nuttx/spinlock.h>

#include "esp_hr_timer.h"
#include "esp32s31_rtc_periodic.h"

/* Use the same hardware-backed HR timer service as the ESP RTC alarm path.
 * This supplies awake periodic notifications, not deep-sleep RTC wakeup.
 * The static RTC device owns one lazily allocated timer for its lifetime.
 */

static struct esp_hr_timer_s *g_periodic_timer;
static rtc_wakeup_callback_t g_periodic_cb;
static void *g_periodic_arg;
static spinlock_t g_periodic_lock = SP_UNLOCKED;

static void esp32s31_rtc_periodic_cb(void *arg)
{
  rtc_wakeup_callback_t cb;
  void *priv;
  irqstate_t flags;

  flags = spin_lock_irqsave(&g_periodic_lock);
  cb = g_periodic_cb;
  priv = g_periodic_arg;
  spin_unlock_irqrestore(&g_periodic_lock, flags);
  if (cb != NULL)
    {
      cb(priv, 0);
    }
}

int esp32s31_rtc_setperiodic(struct rtc_lowerhalf_s *lower,
                            const struct lower_setperiodic_s *info)
{
  struct esp_hr_timer_args_s args =
  {
    .callback = esp32s31_rtc_periodic_cb
  };
  irqstate_t flags;
  uint64_t period;
  int ret;

  if (info == NULL || info->id != 0 || info->cb == NULL ||
      info->period.tv_sec < 0 || info->period.tv_nsec < 0 ||
      info->period.tv_nsec >= NSEC_PER_SEC ||
      (uint64_t)info->period.tv_sec >
      (UINT64_MAX - USEC_PER_SEC) / USEC_PER_SEC)
    {
      return -EINVAL;
    }

  period = (uint64_t)info->period.tv_sec * USEC_PER_SEC +
           info->period.tv_nsec / NSEC_PER_USEC;
  if (period == 0)
    {
      return -EINVAL;
    }

  /* RTC upper-half serializes ioctl writers; allocate outside the spinlock. */

  if (g_periodic_timer == NULL)
    {
      ret = esp_hr_timer_create(&args, &g_periodic_timer);
      if (ret < 0)
        {
          return ret;
        }
    }

  flags = spin_lock_irqsave(&g_periodic_lock);
  g_periodic_cb = info->cb;
  g_periodic_arg = info->priv;
  esp_hr_timer_start(g_periodic_timer, period, true);
  spin_unlock_irqrestore(&g_periodic_lock, flags);
  return OK;
}

int esp32s31_rtc_cancelperiodic(struct rtc_lowerhalf_s *lower, int id)
{
  irqstate_t flags;

  if (id != 0)
    {
      return -EINVAL;
    }

  flags = spin_lock_irqsave(&g_periodic_lock);
  g_periodic_cb = NULL;
  g_periodic_arg = NULL;
  if (g_periodic_timer != NULL)
    {
      esp_hr_timer_stop(g_periodic_timer);
    }

  spin_unlock_irqrestore(&g_periodic_lock, flags);
  return OK;
}
