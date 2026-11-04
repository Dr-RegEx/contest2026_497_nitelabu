/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 *
 * Korvo ES8389 fixed-format standard audio lower half. The I2S engine owns
 * DMA and its own APB reference. This layer owns a separate reference from
 * successful enqueue until the upper callback has returned. Static request
 * slots survive STOP timeouts and late I2S callbacks.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/clock.h>
#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
#  include <nuttx/addrenv.h>
#  include <nuttx/sched.h>
#  include <debug.h>
#endif
#include <errno.h>
#include <string.h>
#include "esp32s31_pcm.h"

#if defined(CONFIG_BUILD_KERNEL) && !defined(CONFIG_AUDIO_PROCESS_OWNERSHIP)
#  error "Korvo PCM kernel buffers require AUDIO_PROCESS_OWNERSHIP"
#endif

#define PCM_REQUESTS 8
#define PCM_BUFFER_SIZE 4096

enum pcm_state_e
{
  PCM_FREE, PCM_PENDING, PCM_INFLIGHT, PCM_DONE
};

struct pcm_dev_s;

struct pcm_request_s
{
  struct pcm_dev_s *owner;
  struct ap_buffer_s *apb;
  size_t bytes;               /* Immutable submitted payload length */
  uint64_t sequence;
  enum pcm_state_e state;
  int result;
  bool cancelled;
};

struct pcm_dev_s
{
  struct audio_lowerhalf_s dev;
  struct i2s_dev_s *i2s;
  struct s31_es8389_s *codec;
  mutex_t lock;                /* API/worker state and I2S submissions */
  spinlock_t callback_lock;    /* Request state publication by I2S */
  sem_t stopped;
  struct pcm_request_s request[PCM_REQUESTS];
  uint64_t sequence;
  unsigned int count;
  uint8_t direction;
  bool reserved;
  bool configured;
  bool running;
  bool bus_started;
  bool stopping;
  bool stream;                /* Exactly one COMPLETE per successful START */
  bool sealed;                /* FINAL accepted: no following buffer */
  int result;
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  struct addrenv_s *addrenv;   /* Reserved process, including empty streams */
#endif
};

static struct pcm_dev_s g_pcm[2];
static bool g_ready;

static bool pcm_capture(struct pcm_dev_s *priv)
{
  return priv->direction == S31_ES8389_CAPTURE;
}

/* Called with the API lock. A failed bus STOP retains all in-flight slots.
 * A codec fault disables PA in the backend. Stream errors are reported by
 * DEQUEUE/COMPLETE/STOP; they must not prevent safe subsequent cleanup.
 */

static int pcm_halt(struct pcm_dev_s *priv)
{
  int ret;
  int codec_ret;

  priv->stopping = true;
  priv->running = false;
  codec_ret = s31_es8389_disable(priv->codec, priv->direction);
  ret = I2S_IOCTL(priv->i2s, AUDIOIOC_STOP, 0);
  if (ret == OK)
    {
      priv->bus_started = false;
    }

  if (priv->result == OK)
    {
      priv->result = ret < 0 ? ret : codec_ret;
    }

  return ret;
}

static void pcm_callback(struct i2s_dev_s *i2s, struct ap_buffer_s *apb,
                         void *arg, int result)
{
  struct pcm_request_s *req = arg;
  struct pcm_dev_s *priv = req->owner;
  irqstate_t flags;

  (void)i2s;
  (void)apb;
  flags = spin_lock_irqsave(&priv->callback_lock);
  req->result = result;
  req->state = PCM_DONE;
  spin_unlock_irqrestore(&priv->callback_lock, flags);
}

/* The persistent worker alone returns APBs and publishes STOP completion.
 * It never calls the upper half while holding either lower-half lock.
 * Polling also makes a lost event notification unable to strand a stream.
 */

static void pcm_service(struct pcm_dev_s *priv)
{
  struct pcm_request_s *req;
  struct ap_buffer_s *apb;
  unsigned int i;
  irqstate_t flags;
  bool final;
  bool cancelled;
  int result;
  int ret;
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  struct addrenv_s *oldenv = NULL;
#endif

  nxmutex_lock(&priv->lock);
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  if (!priv->reserved ||
      (!priv->running && !priv->stopping && priv->count == 0))
    {
      nxmutex_unlock(&priv->lock);
      return;
    }

  /* The upper half permits one process per device. Keep its mapping active
   * for APB access, I2S submission and both DEQUEUE/COMPLETE callbacks.
   * addrenv_select takes a temporary reference in addition to the session.
   */

  ret = addrenv_select(priv->addrenv, &oldenv);
  if (ret < 0)
    {
      DEBUGVERIFY(addrenv_restore(oldenv));
      priv->result = ret;
      if (!priv->stopping)
        {
          pcm_halt(priv);
        }

      /* Do not touch or free an unmapped APB. STOP remains bounded; retain
       * the session and retry from the persistent worker on a later poll.
       */

      nxmutex_unlock(&priv->lock);
      return;
    }
#endif
  if (priv->running && s31_es8389_health(priv->codec) < 0)
    {
      priv->result = -EIO;
      pcm_halt(priv);
    }

  for (;;)
    {
      req = NULL;
      flags = spin_lock_irqsave(&priv->callback_lock);
      for (i = 0; i < PCM_REQUESTS; i++)
        {
          if (priv->request[i].state == PCM_DONE &&
              (req == NULL || priv->request[i].sequence < req->sequence))
            {
              req = &priv->request[i];
            }
        }

      if (req == NULL)
        {
          spin_unlock_irqrestore(&priv->callback_lock, flags);
          break;
        }

      apb = req->apb;
      result = req->result;
      cancelled = req->cancelled;
      final = !cancelled && (apb->flags & AUDIO_APB_FINAL) != 0;
      req->apb = NULL;
      req->state = PCM_FREE;
      spin_unlock_irqrestore(&priv->callback_lock, flags);
      if (result < 0 && !cancelled &&
          !(priv->stopping && result == -ECANCELED))
        {
          priv->result = result;
          if (!priv->stopping)
            {
              pcm_halt(priv);
            }
        }

      if (final && !priv->stopping)
        {
          pcm_halt(priv);
        }

      nxmutex_unlock(&priv->lock);
      if (priv->dev.upper != NULL)
        {
          priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE,
                          apb, result);
        }

      apb_free(apb);
      nxmutex_lock(&priv->lock);
      /* Count callbacks still in progress, while making the slot reusable
       * for callback-driven requeue. Even a cancelled pre-START buffer must
       * finish its callback before STOP/release may report a drained device.
       */

      priv->count--;
    }

  /* Accept pre-START buffers, but submit only while running. STOP and this
   * loop share the API lock so no new submission can follow cancellation.
   */

  for (;;)
    {
      req = NULL;
      flags = spin_lock_irqsave(&priv->callback_lock);
      for (i = 0; i < PCM_REQUESTS; i++)
        {
          if (priv->request[i].state == PCM_PENDING &&
              (req == NULL || priv->request[i].sequence < req->sequence))
            {
              req = &priv->request[i];
            }
        }

      if (req == NULL || (!priv->running && !priv->stopping))
        {
          spin_unlock_irqrestore(&priv->callback_lock, flags);
          break;
        }

      if (priv->stopping)
        {
          req->result = -ECANCELED;
          req->state = PCM_DONE;
          spin_unlock_irqrestore(&priv->callback_lock, flags);
          continue;
        }

      req->state = PCM_INFLIGHT;
      spin_unlock_irqrestore(&priv->callback_lock, flags);
      ret = pcm_capture(priv) ?
        I2S_RECEIVE(priv->i2s, req->apb, pcm_callback, req, 0) :
        I2S_SEND(priv->i2s, req->apb, pcm_callback, req, 0);
      if (ret < 0)
        {
          flags = spin_lock_irqsave(&priv->callback_lock);
          req->result = ret;
          req->state = PCM_DONE;
          spin_unlock_irqrestore(&priv->callback_lock, flags);
          priv->result = ret;
          pcm_halt(priv);
        }
    }

  if (priv->stopping && !priv->bus_started && priv->count == 0)
    {
      bool notify = priv->stream;

      result = priv->result;
      priv->stream = false;
      nxmutex_unlock(&priv->lock);
      if (notify && priv->dev.upper != NULL)
        {
          priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE,
                          NULL, result);
        }

      nxmutex_lock(&priv->lock);
      priv->stopping = false;
      nxsem_get_value(&priv->stopped, &ret);
      if (ret <= 0)
        {
          nxsem_post(&priv->stopped);
        }
    }

#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  /* Keep the session reference until the temporary selection is restored.
   * addrenv_restore only gives its reference; it does not destroy the last
   * environment. Concurrent release must not drop the session first.
   */

  DEBUGVERIFY(addrenv_restore(oldenv));
#endif
  nxmutex_unlock(&priv->lock);
}

static int pcm_worker(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  for (;;)
    {
      pcm_service(&g_pcm[0]);
      pcm_service(&g_pcm[1]);
      nxsig_usleep(5000);
    }

  return OK;
}

static int pcm_getcaps(struct audio_lowerhalf_s *dev, int type,
                       struct audio_caps_s *caps)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  uint8_t direction = pcm_capture(priv) ? AUDIO_TYPE_INPUT : AUDIO_TYPE_OUTPUT;

  (void)type;
  if (caps == NULL || caps->ac_len < sizeof(*caps))
    {
      return -EINVAL;
    }

  caps->ac_format.hw = 0;
  caps->ac_controls.w = 0;
  caps->ac_channels = 0;
  if (caps->ac_type == AUDIO_TYPE_QUERY)
    {
      caps->ac_channels = 2;
      if (caps->ac_subtype == AUDIO_TYPE_QUERY)
        {
          caps->ac_format.hw = 1 << (AUDIO_FMT_PCM - 1);
          caps->ac_controls.b[0] = direction |
            (pcm_capture(priv) ? 0 : AUDIO_TYPE_FEATURE);
        }
      else if (caps->ac_subtype == AUDIO_FMT_PCM)
        {
          caps->ac_controls.b[0] = AUDIO_SUBFMT_PCM_S16_LE;
          caps->ac_controls.b[1] = AUDIO_SUBFMT_END;
        }
      else
        {
          caps->ac_controls.b[0] = AUDIO_SUBFMT_END;
        }
    }
  else if (caps->ac_type == direction)
    {
      caps->ac_channels = 0x22; /* Exactly two slots, not range 1..2 */
      caps->ac_controls.hw[0] = AUDIO_SAMP_RATE_16K;
    }
  else if (caps->ac_type == AUDIO_TYPE_FEATURE && !pcm_capture(priv) &&
           caps->ac_subtype == AUDIO_FU_UNDEF)
    {
#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
      caps->ac_controls.b[0] |= AUDIO_FU_VOLUME;
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_MUTE
      caps->ac_controls.b[0] |= AUDIO_FU_MUTE;
#endif
    }

  return caps->ac_len;
}

static int pcm_configure(struct audio_lowerhalf_s *dev,
                         const struct audio_caps_s *caps)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  int ret;

  if (caps == NULL || caps->ac_len < sizeof(*caps))
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);
  if (!priv->reserved)
    {
      ret = -EPERM;
    }
  else if (caps->ac_type == AUDIO_TYPE_FEATURE && !pcm_capture(priv))
    {
      switch (caps->ac_format.hw)
        {
#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
          case AUDIO_FU_VOLUME:
            ret = s31_es8389_volume(priv->codec, caps->ac_controls.hw[0]);
            break;
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_MUTE
          case AUDIO_FU_MUTE:
            ret = s31_es8389_mute(priv->codec, caps->ac_controls.hw[0] != 0);
            break;
#endif
          default:
            ret = -ENOTTY;
            break;
        }
    }
  else if (priv->running || priv->stopping || priv->count != 0)
    {
      ret = -EBUSY;
    }
  else if (caps->ac_type != (pcm_capture(priv) ? AUDIO_TYPE_INPUT :
                                               AUDIO_TYPE_OUTPUT) ||
           caps->ac_channels != 2 ||
           caps->ac_subtype != AUDIO_FMT_PCM ||
           (caps->ac_format.b[0] != 0 &&
            caps->ac_format.b[0] != AUDIO_SUBFMT_PCM_S16_LE) ||
           (caps->ac_chmap != 0 && caps->ac_chmap != 3) ||
           caps->ac_controls.hw[0] != S31_ES8389_RATE ||
           caps->ac_controls.b[3] != 0 || caps->ac_controls.b[2] != 16)
    {
      ret = -EINVAL;
    }
  else
    {
      priv->configured = true;
      ret = OK;
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_start(struct audio_lowerhalf_s *dev)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  int ret;

  nxmutex_lock(&priv->lock);
  if (!priv->reserved || !priv->configured)
    {
      ret = -EPERM;
    }
  else if (priv->running || priv->stopping)
    {
      ret = -EBUSY;
    }
  else
    {
      priv->result = OK;
      if (priv->count == 0)
        {
          priv->sealed = false;
        }

      ret = s31_es8389_prepare(priv->codec);
      if (ret == OK)
        {
          /* START failures may be partial: STOP must still quiesce the bus. */

          priv->bus_started = true;
          ret = I2S_IOCTL(priv->i2s, AUDIOIOC_START, 0);
          if (ret == OK)
            {
              ret = s31_es8389_enable(priv->codec, priv->direction);
            }

          if (ret < 0)
            {
              priv->result = ret;
              pcm_halt(priv);
            }
          else
            {
              priv->running = true;
              priv->stream = true;
            }
        }
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_stop(struct audio_lowerhalf_s *dev)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  clock_t start = clock_systime_ticks();
  clock_t limit = MSEC2TICK(CONFIG_ESP32S31_KORVO_PCM_STOP_MS) + 1;
  clock_t elapsed;
  int ret;

  nxmutex_lock(&priv->lock);
  if (!priv->running && !priv->stopping && priv->count == 0)
    {
      ret = priv->result;
      /* Report a completed stream error once. Keeping it forever makes a
       * later STOP/configure recovery fail even though no buffer is owned.
       */

      priv->result = OK;
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  ret = pcm_halt(priv);
  nxmutex_unlock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  for (;;)
    {
      nxmutex_lock(&priv->lock);
      if (!priv->stopping)
        {
          ret = priv->result;
          priv->result = OK;
          nxmutex_unlock(&priv->lock);
          return ret;
        }

      nxmutex_unlock(&priv->lock);
      elapsed = clock_systime_ticks() - start;
      if (elapsed >= limit)
        {
          return -ETIMEDOUT;
        }

      nxsem_tickwait_uninterruptible(&priv->stopped, limit - elapsed);
    }
}

static int pcm_reserve(struct audio_lowerhalf_s *dev)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  int ret = OK;

  nxmutex_lock(&priv->lock);
  if (priv->reserved || priv->stopping || priv->running || priv->count != 0)
    {
      ret = -EBUSY;
    }
  else
    {
      priv->reserved = true;
      priv->configured = false;
      priv->sealed = false;
      priv->result = OK;
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
      priv->addrenv = this_task()->addrenv_curr;
      addrenv_take(priv->addrenv);
#endif
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_release(struct audio_lowerhalf_s *dev)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  int ret = pcm_stop(dev);
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  struct addrenv_s *owner = NULL;
#endif

  nxmutex_lock(&priv->lock);
  if (!priv->stopping && !priv->running && priv->count == 0)
    {
      priv->reserved = false;
      priv->configured = false;
      priv->sealed = false;
      priv->result = OK;
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
      owner = priv->addrenv;
      priv->addrenv = NULL;
#endif
      ret = OK;
    }

  nxmutex_unlock(&priv->lock);
#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  addrenv_drop(owner, false);
#endif
  return ret;
}

static int pcm_enqueue(struct audio_lowerhalf_s *dev, struct ap_buffer_s *apb)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  struct pcm_request_s *req = NULL;
  size_t bytes;
  irqstate_t flags;
  unsigned int i;
  int ret = OK;

  if (apb == NULL || apb->samp == NULL || apb->curbyte > apb->nbytes ||
      apb->nbytes > apb->nmaxbytes)
    {
      return -EINVAL;
    }

  bytes = pcm_capture(priv) ? apb->nmaxbytes : apb->nbytes - apb->curbyte;
  if (bytes > 32768 || bytes % 512 != 0 ||
      (bytes == 0 && (pcm_capture(priv) || !(apb->flags & AUDIO_APB_FINAL))))
    {
      return -EINVAL;
    }

  /* The standard upper half may enqueue shared periods while holding its
   * spinlock. Never wait for a mutex (including the APB reference mutex) in
   * this entry point. A rejected buffer remains entirely caller-owned.
   */

  ret = nxmutex_trylock(&priv->lock);
  if (ret < 0)
    {
      return -EAGAIN;
    }

#ifdef CONFIG_AUDIO_PROCESS_OWNERSHIP
  if (priv->reserved && priv->addrenv != this_task()->addrenv_curr)
    {
      nxmutex_unlock(&priv->lock);
      return -EXDEV;
    }
#endif

  flags = spin_lock_irqsave(&priv->callback_lock);
  if (!priv->reserved || !priv->configured)
    {
      ret = -EPERM;
    }
  else if (priv->stopping || priv->sealed)
    {
      ret = -ESHUTDOWN;
    }
  else
    {
      for (i = 0; i < PCM_REQUESTS; i++)
        {
          if (priv->request[i].state != PCM_FREE &&
              priv->request[i].apb == apb)
            {
              ret = -EBUSY;
              break;
            }

          if (priv->request[i].state == PCM_FREE)
            {
              req = &priv->request[i];
            }
        }

      if (ret == OK && req == NULL)
        {
          ret = -ENOMEM;
        }
    }

  spin_unlock_irqrestore(&priv->callback_lock, flags);
  if (ret == OK)
    {
      ret = nxmutex_trylock(&apb->lock);
      if (ret < 0)
        {
          nxmutex_unlock(&priv->lock);
          return -EAGAIN;
        }

      /* Same reference operation as apb_reference(), using a nonwaiting
       * acquisition because this API is also called from upper callbacks.
       */

      if (apb->crefs == 0 || apb->crefs == UINT16_MAX)
        {
          nxmutex_unlock(&apb->lock);
          nxmutex_unlock(&priv->lock);
          return -EINVAL;
        }

      apb->crefs++;
      nxmutex_unlock(&apb->lock);
      flags = spin_lock_irqsave(&priv->callback_lock);
      req->apb = apb;
      req->bytes = bytes;
      req->sequence = priv->sequence++;
      req->result = OK;
      req->cancelled = false;
      req->state = PCM_PENDING;
      priv->count++;
      priv->sealed = (apb->flags & AUDIO_APB_FINAL) != 0;
      spin_unlock_irqrestore(&priv->callback_lock, flags);
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_cancel(struct audio_lowerhalf_s *dev, struct ap_buffer_s *apb)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  unsigned int i;
  irqstate_t flags;
  int ret = -ENOENT;

  nxmutex_lock(&priv->lock);
  flags = spin_lock_irqsave(&priv->callback_lock);
  for (i = 0; i < PCM_REQUESTS; i++)
    {
      struct pcm_request_s *req = &priv->request[i];

      if (req->state != PCM_FREE && req->apb == apb)
        {
          ret = -ENOTSUP; /* Bus has endpoint STOP, not single-request cancel */
          if (req->state == PCM_PENDING)
            {
              req->result = -ECANCELED;
              req->cancelled = true;
              req->state = PCM_DONE;
              if (apb->flags & AUDIO_APB_FINAL)
                {
                  priv->sealed = false;
                }
              ret = OK;
            }

          break;
        }
    }

  spin_unlock_irqrestore(&priv->callback_lock, flags);
  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_ioctl(struct audio_lowerhalf_s *dev, int cmd, unsigned long arg)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;

  if (cmd == AUDIOIOC_GETLATENCY)
    {
      long *frames = (long *)arg;
      size_t bytes = 0;
      irqstate_t flags;
      unsigned int i;

      if (frames == NULL)
        {
          return -EINVAL;
        }

      *frames = 0;
      if (pcm_capture(priv))
        {
          return -ENOTSUP;
        }

      /* This is a period-resolution playback estimate in PCM frames.
       * Count accepted pending/in-flight payloads exactly once; DONE has
       * already left I2S. The bus does not expose sub-period/FIFO progress.
       * Do not read curbyte: the I2S worker owns and updates it before the
       * callback, outside callback_lock.
       */

      flags = spin_lock_irqsave(&priv->callback_lock);
      for (i = 0; i < PCM_REQUESTS; i++)
        {
          if (priv->request[i].state == PCM_PENDING ||
              priv->request[i].state == PCM_INFLIGHT)
            {
              bytes += priv->request[i].bytes;
            }
        }

      spin_unlock_irqrestore(&priv->callback_lock, flags);
      *frames = bytes / (2 * sizeof(int16_t)); /* 16 kHz, stereo S16 */
      return OK;
    }

  if (cmd == AUDIOIOC_GETBUFFERINFO)
    {
      struct ap_buffer_info_s *info = (struct ap_buffer_info_s *)arg;

      if (info == NULL)
        {
          return -EINVAL;
        }

      info->buffer_size = PCM_BUFFER_SIZE;
      info->nbuffers = 4;
      return OK;
    }

  return -ENOTTY;
}

#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
static int pcm_pause_resume(struct audio_lowerhalf_s *dev, int command)
{
  struct pcm_dev_s *priv = (struct pcm_dev_s *)dev;
  int ret;

  nxmutex_lock(&priv->lock);
  ret = priv->running && !priv->stopping ?
        I2S_IOCTL(priv->i2s, command, 0) : -EPIPE;
  nxmutex_unlock(&priv->lock);
  return ret;
}

static int pcm_pause(struct audio_lowerhalf_s *dev)
{
  return pcm_pause_resume(dev, AUDIOIOC_PAUSE);
}

static int pcm_resume(struct audio_lowerhalf_s *dev)
{
  return pcm_pause_resume(dev, AUDIOIOC_RESUME);
}
#endif

static const struct audio_ops_s g_pcm_ops =
{
  .getcaps = pcm_getcaps,
  .configure = pcm_configure,
  .shutdown = pcm_release,
  .start = pcm_start,
  .stop = pcm_stop,
#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
  .pause = pcm_pause,
  .resume = pcm_resume,
#endif
  .enqueuebuffer = pcm_enqueue,
  .cancelbuffer = pcm_cancel,
  .ioctl = pcm_ioctl,
  .reserve = pcm_reserve,
  .release = pcm_release,
};

int s31_pcm_initialize(struct s31_es8389_s *codec,
                       struct i2s_dev_s *playback, struct i2s_dev_s *capture)
{
  unsigned int i;
  unsigned int j;
  int ret;

  /* The board serializes initialization. Once exposed, never reset slots
   * that a callback may still own. The worker and devices live with the BSP.
   */

  if (g_ready)
    {
      return OK;
    }

  if (codec == NULL || playback == NULL || capture == NULL)
    {
      return -EINVAL;
    }

  memset(g_pcm, 0, sizeof(g_pcm));
  for (i = 0; i < 2; i++)
    {
      struct pcm_dev_s *priv = &g_pcm[i];

      priv->dev.ops = &g_pcm_ops;
      priv->codec = codec;
      priv->i2s = i == 0 ? playback : capture;
      priv->direction = i == 0 ? S31_ES8389_PLAYBACK : S31_ES8389_CAPTURE;
      nxmutex_init(&priv->lock);
      nxsem_init(&priv->stopped, 0, 0);
      nxsem_set_protocol(&priv->stopped, SEM_PRIO_NONE);
      for (j = 0; j < PCM_REQUESTS; j++)
        {
          priv->request[j].owner = priv;
        }
    }

  ret = kthread_create("s31_pcm", 235, 4096, pcm_worker, NULL);
  if (ret < 0)
    {
      for (i = 0; i < 2; i++)
        {
          nxsem_destroy(&g_pcm[i].stopped);
          nxmutex_destroy(&g_pcm[i].lock);
        }

      return ret;
    }

  g_ready = true;
  return OK;
}

struct audio_lowerhalf_s *s31_pcm_endpoint(bool capture)
{
  return g_ready ? &g_pcm[capture].dev : NULL;
}

