/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_adc_buttons.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <nuttx/clock.h>
#include <nuttx/input/buttons.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/wqueue.h>
#include <arch/board/board.h>

#include "esp32s31_adc.h"

#define KEYS_ALL ((1u << NUM_BUTTONS) - 1)
#define KEYS_RAW_MAX 131071
#define KEYS_WINDOW CONFIG_ESP32S31_KORVO_KEYS_RAW_WINDOW
#define KEYS_HYST CONFIG_ESP32S31_KORVO_KEYS_RAW_HYSTERESIS

struct s31_keys_s
{
  struct btn_lowerhalf_s lower;
  struct adc_dev_s *adc;
  struct work_s work;
  mutex_t registration;
  mutex_t control;
  spinlock_t lock;
  btn_handler_t handler;
  void *arg;
  btn_buttonset_t watch;
  btn_buttonset_t state;
  btn_buttonset_t candidate;
  int32_t raw;
  uint8_t stable;
  bool active;
  bool ready;
  bool sampled;
  bool fault;
  bool registered;
#ifdef CONFIG_ESP32S31_KORVO_KEYS_TRACE
  clock_t traced;
#endif
};

static btn_buttonset_t keys_supported(const struct btn_lowerhalf_s *lower);
static btn_buttonset_t keys_buttons(const struct btn_lowerhalf_s *lower);
static void keys_enable(const struct btn_lowerhalf_s *lower,
                        btn_buttonset_t press, btn_buttonset_t release,
                        btn_handler_t handler, void *arg);
static void keys_poll(void *arg);

static struct s31_keys_s g_keys =
{
  .lower =
  {
    .bl_supported = keys_supported,
    .bl_buttons = keys_buttons,
    .bl_enable = keys_enable
  },
  .registration = NXMUTEX_INITIALIZER,
  .control = NXMUTEX_INITIALIZER,
  .lock = SP_UNLOCKED
};

/* Bits follow board.h; the schematic calls PLAY "MODE". These raw centers
 * are configurable, and must be measured on the actual board before release.
 */

static const int32_t g_centers[NUM_BUTTONS] =
{
  CONFIG_ESP32S31_KORVO_KEYS_RAW_PLAY,
  CONFIG_ESP32S31_KORVO_KEYS_RAW_SET,
  CONFIG_ESP32S31_KORVO_KEYS_RAW_VOLDOWN,
  CONFIG_ESP32S31_KORVO_KEYS_RAW_VOLUP
};

static int keys_receive(struct adc_dev_s *dev, uint8_t channel, int32_t raw)
{
  (void)dev;
  if (channel != 0 || raw < 0 || raw > KEYS_RAW_MAX)
    {
      return -EIO;
    }

  /* This ADC client's conversion callback is synchronous in ao_ioctl. */

  g_keys.raw = raw;
  g_keys.sampled = true;
  return OK;
}

static const struct adc_callback_s g_callback =
{
  .au_receive = keys_receive
};

static int32_t keys_distance(int32_t a, int32_t b)
{
  return a > b ? a - b : b - a;
}

static btn_buttonset_t keys_decode(int32_t raw, btn_buttonset_t held)
{
  int i;

  for (i = 0; i < NUM_BUTTONS; i++)
    {
      int32_t window = KEYS_WINDOW + ((held & (1u << i)) ? KEYS_HYST : 0);
      if (keys_distance(raw, g_centers[i]) <= window)
        {
          return 1u << i;
        }
    }

  /* Idle, gaps and unknown levels are released. A resistor ladder cannot
   * reliably identify simultaneous key combinations; only one bit is valid.
   */

  return 0;
}

static void keys_poweroff(void)
{
  if (g_keys.ready)
    {
      g_keys.adc->ad_ops->ao_rxint(g_keys.adc, false);
      g_keys.adc->ad_ops->ao_shutdown(g_keys.adc);
      g_keys.ready = false;
    }
}

static int keys_sample(void)
{
  int ret;

  if (!g_keys.ready)
    {
      ret = g_keys.adc->ad_ops->ao_setup(g_keys.adc);
      if (ret < 0)
        {
          return ret;
        }

      g_keys.ready = true;
      g_keys.adc->ad_ops->ao_rxint(g_keys.adc, true);
    }

  g_keys.sampled = false;
  ret = g_keys.adc->ad_ops->ao_ioctl(g_keys.adc, ANIOC_TRIGGER, 0);
  return ret < 0 ? ret : (g_keys.sampled ? OK : -EIO);
}

static void keys_poll(void *arg)
{
  btn_handler_t handler = NULL;
  void *handler_arg = NULL;
  btn_buttonset_t next;
  btn_buttonset_t changed;
  irqstate_t flags;
  int ret;
  int queue_error = OK;
  bool queued = false;

  (void)arg;
  flags = spin_lock_irqsave(&g_keys.lock);
  if (!g_keys.active)
    {
      spin_unlock_irqrestore(&g_keys.lock, flags);
      return;
    }

  spin_unlock_irqrestore(&g_keys.lock, flags);
  ret = keys_sample();
  if (ret < 0 && !g_keys.fault)
    {
      syslog(LOG_ERR, "Korvo ADC keys: sample failed %d\n", ret);
    }
  else if (ret == OK && g_keys.fault)
    {
      syslog(LOG_INFO, "Korvo ADC keys: sampling recovered\n");
    }

  g_keys.fault = ret < 0;

#ifdef CONFIG_ESP32S31_KORVO_KEYS_TRACE
  if (ret == OK && clock_systime_ticks() - g_keys.traced >= MSEC2TICK(200))
    {
      g_keys.traced = clock_systime_ticks();
      syslog(LOG_INFO, "Korvo ADC keys: raw=%ld\n", (long)g_keys.raw);
    }
#endif

  flags = spin_lock_irqsave(&g_keys.lock);
  if (g_keys.active)
    {
      /* Consecutive failed samples also debounce a release. A conversion
       * timeout must not become a zero-voltage key or leave a key stuck.
       */

      next = ret < 0 ? 0 : keys_decode(g_keys.raw, g_keys.state);
      if (next != g_keys.candidate)
        {
          g_keys.candidate = next;
          g_keys.stable = 0;
        }

      if (g_keys.stable < CONFIG_ESP32S31_KORVO_KEYS_DEBOUNCE)
        {
          g_keys.stable++;
        }

      if (g_keys.stable == CONFIG_ESP32S31_KORVO_KEYS_DEBOUNCE)
        {
          changed = next ^ g_keys.state;
          g_keys.state = next;
          if ((changed & g_keys.watch) != 0)
            {
              handler = g_keys.handler;
              handler_arg = g_keys.arg;
            }
        }
    }

  spin_unlock_irqrestore(&g_keys.lock, flags);

  /* Always report both edges for a watched button. The upper half tracks
   * both edges and applies each client's press/release notification masks.
   */

  if (handler != NULL)
    {
      handler(&g_keys.lower, handler_arg);
    }

  flags = spin_lock_irqsave(&g_keys.lock);
  if (g_keys.active)
    {
      ret = work_queue(LPWORK, &g_keys.work, keys_poll, NULL,
                       MSEC2TICK(CONFIG_ESP32S31_KORVO_KEYS_PERIOD_MS));
      queued = ret == OK;
      if (!queued)
        {
          queue_error = ret;
          g_keys.active = false;
          handler = (g_keys.state & g_keys.watch) != 0 ?
                    g_keys.handler : NULL;
          handler_arg = g_keys.arg;
          g_keys.state = 0;
        }
    }

  spin_unlock_irqrestore(&g_keys.lock, flags);
  if (!queued)
    {
      keys_poweroff();
      if (queue_error < 0)
        {
          /* Update the upper half's last sample before stopping. Otherwise
           * the next press after a retry could be mistaken for a held key.
           */

          if (handler != NULL)
            {
              handler(&g_keys.lower, handler_arg);
            }

          syslog(LOG_ERR, "Korvo ADC keys: poll queue failed %d\n",
                 queue_error);
        }
    }
}

static btn_buttonset_t keys_supported(const struct btn_lowerhalf_s *lower)
{
  (void)lower;
  return KEYS_ALL;
}

static btn_buttonset_t keys_buttons(const struct btn_lowerhalf_s *lower)
{
  btn_buttonset_t state;
  irqstate_t flags;

  (void)lower;
  flags = spin_lock_irqsave(&g_keys.lock);
  state = g_keys.state;
  spin_unlock_irqrestore(&g_keys.lock, flags);
  return state;
}

static void keys_enable(const struct btn_lowerhalf_s *lower,
                        btn_buttonset_t press, btn_buttonset_t release,
                        btn_handler_t handler, void *arg)
{
  btn_buttonset_t watch = (press | release) & KEYS_ALL;
  irqstate_t flags;
  bool active;
  int ret;

  (void)lower;
  ret = nxmutex_lock(&g_keys.control);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Korvo ADC keys: control lock failed %d\n", ret);
      return;
    }

  flags = spin_lock_irqsave(&g_keys.lock);
  active = g_keys.active;
  if (watch == 0 || handler == NULL)
    {
      g_keys.active = false;
      g_keys.handler = NULL;
      g_keys.arg = NULL;
    }

  spin_unlock_irqrestore(&g_keys.lock, flags);
  if (watch == 0 || handler == NULL)
    {
      /* Set inactive before cancellation so an in-flight conversion cannot
       * rearm work. Join it before releasing the exclusive ADC client.
       */

      work_cancel_sync(LPWORK, &g_keys.work);
      keys_poweroff();
      flags = spin_lock_irqsave(&g_keys.lock);
      g_keys.state = 0;
      g_keys.candidate = 0;
      g_keys.stable = 0;
      spin_unlock_irqrestore(&g_keys.lock, flags);
    }
  else
    {
      if (!active)
        {
          /* Also retire a worker that failed to requeue before a new open. */

          work_cancel_sync(LPWORK, &g_keys.work);
          keys_poweroff();
          g_keys.fault = false;
        }

      flags = spin_lock_irqsave(&g_keys.lock);
      g_keys.watch = watch;
      g_keys.handler = handler;
      g_keys.arg = arg;
      g_keys.active = true;
      if (!active)
        {
          g_keys.state = 0;
          g_keys.candidate = 0;
          g_keys.stable = 0;
          ret = work_queue(LPWORK, &g_keys.work, keys_poll, NULL, 0);
          if (ret < 0)
            {
              g_keys.active = false;
            }
        }

      spin_unlock_irqrestore(&g_keys.lock, flags);
      if (ret < 0)
        {
          syslog(LOG_ERR, "Korvo ADC keys: start queue failed %d\n", ret);
        }
    }

  nxmutex_unlock(&g_keys.control);
}

int esp32s31_korvo_buttons_initialize(void)
{
  int i;
  int j;
  int ret = nxmutex_lock(&g_keys.registration);

  if (ret < 0)
    {
      return ret;
    }

  if (g_keys.registered)
    {
      nxmutex_unlock(&g_keys.registration);
      return OK;
    }

  for (i = 0; i < NUM_BUTTONS; i++)
    {
      int32_t margin = KEYS_WINDOW + KEYS_HYST;
      if (g_centers[i] < margin || g_centers[i] > KEYS_RAW_MAX - margin)
        {
          nxmutex_unlock(&g_keys.registration);
          return -EINVAL;
        }

      for (j = 0; j < i; j++)
        {
          if (keys_distance(g_centers[i], g_centers[j]) <= 2 * margin)
            {
              nxmutex_unlock(&g_keys.registration);
              return -EINVAL;
            }
        }
    }

  g_keys.adc = esp32s31_adc_initialize();
  ret = g_keys.adc->ad_ops->ao_bind(g_keys.adc, &g_callback);
  if (ret == OK)
    {
      /* Registration has its own lock: btn_register calls bl_enable(false),
       * which takes control. Concurrent bringup retries must not register a
       * second upper half or replace an active ADC callback.
       */

      ret = btn_register("/dev/buttons", &g_keys.lower);
    }

  g_keys.registered = ret == OK;
  nxmutex_unlock(&g_keys.registration);
  return ret;
}
