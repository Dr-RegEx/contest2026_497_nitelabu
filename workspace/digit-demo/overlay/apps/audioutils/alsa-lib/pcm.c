/****************************************************************************
 * apps/audioutils/alsa-lib/pcm.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <alsa/error.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>

#include "pcm_local.h"

/****************************************************************************
 * Pre-processor Prototypes
 ****************************************************************************/

#define SND_PCM_CHECK_SETUP(pcm)                                            \
  do                                                                        \
    {                                                                       \
      assert(pcm);                                                          \
      if (snd_pcm_state(pcm) <= SND_PCM_STATE_OPEN)                         \
        {                                                                   \
          SNDERR("PCM not set up");                                         \
          return -EIO;                                                      \
        }                                                                   \
    }                                                                       \
  while (0)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int snd_pcm_open(FAR snd_pcm_t **pcmp, FAR const char *name,
                 snd_pcm_stream_t stream, int mode)
{
  assert(pcmp && name);

  if (strcmp(name, "default") == 0)
    {
      name = stream == SND_PCM_STREAM_PLAYBACK ? "pcm0p" : "pcm0c";
    }

  if (stream == SND_PCM_STREAM_CAPTURE)
    {
      return snd_pcm_hw_open(pcmp, name, stream, mode);
    }

#if defined(CONFIG_AUDIOUTILS_ALSA_LIB_DEVICE_DMIX)
  return snd_pcm_dmix_open(pcmp, name, stream, mode);
#elif defined(CONFIG_AUDIOUTILS_ALSA_LIB_DEVICE_HW)
  return snd_pcm_hw_open(pcmp, name, stream, mode);
#else
  return -EPERM;
#endif
}

int snd_pcm_close(FAR snd_pcm_t *pcm)
{
  int ret;

  assert(pcm);
  snd_pcm_munmap(pcm);
  ret = pcm->ops->close(pcm->ops_arg);

  snd_pcm_free(pcm);
  return ret;
}

FAR const char *snd_pcm_name(FAR snd_pcm_t *pcm)
{
  assert(pcm);
  return pcm->name;
}

snd_pcm_type_t snd_pcm_type(FAR snd_pcm_t *pcm)
{
  assert(pcm);
  return pcm->type;
}

snd_pcm_stream_t snd_pcm_stream(FAR snd_pcm_t *pcm)
{
  assert(pcm);
  return pcm->stream;
}

int snd_pcm_poll_descriptors_count(FAR snd_pcm_t *pcm)
{
  return pcm->ops->poll_descriptors_count(pcm);
}

int snd_pcm_poll_descriptors(FAR snd_pcm_t *pcm, FAR struct pollfd *pfds,
                             unsigned int space)
{
  assert(pcm && pfds);
  return pcm->ops->poll_descriptors(pcm->ops_arg, pfds, space);
}

int snd_pcm_poll_descriptors_revents(FAR snd_pcm_t *pcm,
                                     FAR struct pollfd *pfds,
                                     unsigned int nfds,
                                     FAR unsigned short *revents)
{
  if (pcm->ops->poll_revents)
    {
      return pcm->ops->poll_revents(pcm->ops_arg, pfds, nfds, revents);
    }

  if (nfds == 1)
    {
      *revents = pfds->revents;
      return 0;
    }

  return -EINVAL;
}

int snd_pcm_nonblock(FAR snd_pcm_t *pcm, int nonblock)
{
  assert(pcm);
  if (nonblock)
    {
      pcm->mode |= SND_PCM_NONBLOCK;
    }
  else
    {
      pcm->mode &= ~SND_PCM_NONBLOCK;
    }

  return 0;
}

int snd_pcm_prepare(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->prepare(pcm->ops_arg);
}

int snd_pcm_reset(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->reset(pcm->ops_arg);
}

int snd_pcm_start(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  snd_pcm_dump(pcm, stdout);
  return pcm->ops->start(pcm->ops_arg);
}

int snd_pcm_pause(FAR snd_pcm_t *pcm, int enable)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->pause(pcm->ops_arg, enable);
}

snd_pcm_state_t snd_pcm_state(FAR snd_pcm_t *pcm)
{
  assert(pcm);
  return pcm->ops->state(pcm->ops_arg);
}

int snd_pcm_delay(FAR snd_pcm_t *pcm, FAR snd_pcm_sframes_t *delayp)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->delay(pcm->ops_arg, delayp);
}

int snd_pcm_drop(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->drop(pcm->ops_arg);
}

int snd_pcm_drain(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->drain(pcm->ops_arg);
}

int snd_pcm_resume(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->resume(pcm->ops_arg);
}

snd_pcm_sframes_t snd_pcm_avail_update(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->avail_update(pcm->ops_arg);
}

int snd_pcm_wait(FAR snd_pcm_t *pcm, int timeout)
{
  unsigned short revents = 0;
  struct pollfd *pfd;
  int npfds;
  int ret;

  npfds = snd_pcm_poll_descriptors_count(pcm);
  if (npfds <= 0)
    {
      SNDERR("Invalid snd_pcm_poll_descriptors_count return %d\n", npfds);
      return npfds;
    }

  pfd = alloca(sizeof(*pfd) * npfds);
  ret = snd_pcm_poll_descriptors(pcm, pfd, npfds);
  if (ret <= 0)
    {
      SNDERR("Invalid snd_pcm_poll_descriptors return: %d", ret);
      return ret;
    }

  for (; ; )
    {
      ret = poll(pfd, npfds, timeout);
      if (ret < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -errno;
        }
      else if (ret == 0)
        {
          return 0;
        }

      ret = snd_pcm_poll_descriptors_revents(pcm, pfd, npfds, &revents);
      if (ret < 0)
        {
          return ret;
        }

      if (revents & (POLLERR | POLLNVAL))
        {
          return -EIO;
        }
      else if (revents & (POLLIN | POLLOUT))
        {
          return 1;
        }
    }
}

snd_pcm_sframes_t snd_pcm_writei(FAR snd_pcm_t *pcm, FAR const void *buffer,
                                 snd_pcm_uframes_t size)
{
  SND_PCM_CHECK_SETUP(pcm);
  if (pcm->ops->writei)
    {
      return pcm->ops->writei(pcm->ops_arg, buffer, size);
    }

  return snd_pcm_mmap_writei(pcm->ops_arg, buffer, size);
}

snd_pcm_sframes_t snd_pcm_readi(FAR snd_pcm_t *pcm, FAR void *buffer,
                                 snd_pcm_uframes_t size)
{
  SND_PCM_CHECK_SETUP(pcm);
  if (pcm->ops->readi)
    {
      return pcm->ops->readi(pcm->ops_arg, buffer, size);
    }

  return snd_pcm_mmap_readi(pcm->ops_arg, buffer, size);
}

snd_pcm_sframes_t snd_pcm_writen(FAR snd_pcm_t *pcm, FAR void **bufs,
                                 snd_pcm_uframes_t size)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->writen(pcm->ops_arg, bufs, size);
}

int snd_pcm_recover(FAR snd_pcm_t *pcm, int err, int silent)
{
  err = err < 0 ? err : -err;
  if (err == -EINTR)
    {
      return 0;
    }

  if (err != -EPIPE)
    {
      return err;
    }

  err = snd_pcm_prepare(pcm);
  if (err < 0)
    {
      SNDERR("cannot recovery from %s, prepare failed: %s",
             snd_pcm_stream(pcm) == SND_PCM_STREAM_PLAYBACK ? "underrun"
                                                            : "overrun",
             snd_strerror(err));
    }

  return err;
}

int snd_pcm_set_params(FAR snd_pcm_t *pcm, snd_pcm_format_t format,
                       snd_pcm_access_t access, unsigned int channels,
                       unsigned int rate, int soft_resample,
                       unsigned int latency)
{
  FAR snd_pcm_hw_params_t *params;
  int ret;

  assert(pcm);

  snd_pcm_hw_params_alloca(&params);
  ret = snd_pcm_hw_params_any(pcm, params);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_any return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_format(pcm, params, format);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_format return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_access(pcm, params, access);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_access return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_channels(pcm, params, channels);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_channels return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_rate(pcm, params, rate, 0);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_rate return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_buffer_time(pcm, params, latency, 0);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_buffer_time return: %d", ret);
      return ret;
    }

  ret = snd_pcm_hw_params_set_period_time(pcm, params, latency / 4, 0);
  if (ret < 0)
    {
      SNDERR("Invalid snd_pcm_hw_params_set_period_time return: %d", ret);
      return ret;
    }

  return snd_pcm_hw_params(pcm, params);
}

int snd_pcm_get_params(FAR snd_pcm_t *pcm,
                       FAR snd_pcm_uframes_t *buffer_size,
                       FAR snd_pcm_uframes_t *period_size)
{
  assert(pcm);

  *period_size = pcm->period_size;
  *buffer_size = pcm->period_size * pcm->periods;

  return 0;
}

snd_pcm_sframes_t snd_pcm_bytes_to_frames(FAR snd_pcm_t *pcm, ssize_t bytes)
{
  if (!pcm->frame_bits)
    {
      return -EINVAL;
    }

  return bytes / (pcm->frame_bits / 8);
}

ssize_t snd_pcm_frames_to_bytes(FAR snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
  if (!pcm->frame_bits)
    {
      return -EINVAL;
    }

  return frames * (pcm->frame_bits / 8);
}

long snd_pcm_bytes_to_samples(FAR snd_pcm_t *pcm, ssize_t bytes)
{
  if (!pcm->sample_bits)
    {
      return -EINVAL;
    }

  return bytes / (pcm->sample_bits / 8);
}

ssize_t snd_pcm_samples_to_bytes(FAR snd_pcm_t *pcm, long samples)
{
  if (!pcm->sample_bits)
    {
      return -EINVAL;
    }

  return samples * (pcm->sample_bits / 8);
}

int snd_pcm_set_volume(FAR snd_pcm_t *pcm, unsigned int volume)
{
  if (volume > 100)
    {
      return -EINVAL;
    }

  pcm->volume = volume / 100.0;
  SNDINFO("set volume:%f", pcm->volume);

  return 0;
}

int snd_pcm_set_volume_db(FAR snd_pcm_t *pcm, int db)
{
  if (db > 0)
    {
      return -EINVAL;
    }

  pcm->volume = powf(10.0,  db / 2000.0);
  SNDINFO("set volume:%f", pcm->volume);

  return 0;
}

int snd_pcm_dump(FAR snd_pcm_t *pcm, FAR snd_output_t *out)
{
  UNUSED(out);
  SND_PCM_CHECK_SETUP(pcm);

  /* Dump hardware info */

  SNDINFO("device         : %s\n", pcm->name);
  SNDINFO("stream         : %s\n", snd_pcm_stream_name(pcm->stream));
  SNDINFO("access         : %s\n", snd_pcm_access_name(pcm->access));
  SNDINFO("format         : %s\n", snd_pcm_format_name(pcm->format));
  SNDINFO("channels       : %u\n", pcm->channels);
  SNDINFO("rate           : %u\n", pcm->rate);
  SNDINFO("period         : %u\n", pcm->periods);
  SNDINFO("period_time    : %u\n", pcm->period_time);
  SNDINFO("period_frames  : %ld\n", pcm->period_size);

  /* Dump software info */

  SNDINFO("start_threshold: %ld\n", pcm->start_threshold);

  /* Dump device info */

  if (pcm->ops->dump)
    {
      pcm->ops->dump(pcm->ops_arg, out);
    }

  return 0;
}

FAR const char *snd_pcm_stream_name(snd_pcm_stream_t stream)
{
  switch (stream)
    {
    case SND_PCM_STREAM_PLAYBACK:
      return "PLAYBACK";
    case SND_PCM_STREAM_CAPTURE:
      return "CAPTURE";
    default:
      return NULL;
    }
}

FAR const char *snd_pcm_state_name(snd_pcm_state_t state)
{
  switch (state)
    {
    case SND_PCM_STATE_OPEN:
      return "OPEN";
    case SND_PCM_STATE_SETUP:
      return "SETUP";
    case SND_PCM_STATE_PREPARED:
      return "PREPARED";
    case SND_PCM_STATE_RUNNING:
      return "RUNNING";
    case SND_PCM_STATE_XRUN:
      return "XRUN";
    case SND_PCM_STATE_DRAINING:
      return "DRAINING";
    case SND_PCM_STATE_PAUSED:
      return "PAUSED";
    case SND_PCM_STATE_SUSPENDED:
      return "SUSPENDED";
    case SND_PCM_STATE_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return NULL;
    }
}

int snd_pcm_mmap(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->mmap(pcm->ops_arg);
}

int snd_pcm_munmap(FAR snd_pcm_t *pcm)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->munmap(pcm->ops_arg);
}

int snd_pcm_mmap_begin(FAR snd_pcm_t *pcm,
                       FAR const snd_pcm_channel_area_t **areas,
                       FAR snd_pcm_uframes_t *offset,
                       FAR snd_pcm_uframes_t *frames)
{
  assert(pcm && areas && offset && frames);
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->mmap_begin(pcm->ops_arg, areas, offset, frames);
}

snd_pcm_sframes_t snd_pcm_mmap_commit(FAR snd_pcm_t *pcm,
                                      snd_pcm_uframes_t offset,
                                      snd_pcm_uframes_t frames)
{
  SND_PCM_CHECK_SETUP(pcm);
  return pcm->ops->mmap_commit(pcm->ops_arg, offset, frames);
}

snd_pcm_sframes_t snd_pcm_mmap_writei(FAR snd_pcm_t *pcm,
                                      FAR const void *buffer,
                                      snd_pcm_uframes_t size)
{
  FAR const snd_pcm_channel_area_t *areas;
  FAR const uint8_t *in = buffer;
  snd_pcm_uframes_t xfer = 0;
  snd_pcm_uframes_t offset;
  snd_pcm_uframes_t frames;
  FAR uint8_t *out;
  int ret = 0;
  ssize_t len;

  while (size > 0)
    {
      frames = size;
      ret = snd_pcm_mmap_begin(pcm, &areas, &offset, &frames);
      if (ret < 0)
        {
          break;
        }

      if (frames == 0)
        {
          if (pcm->mode & SND_PCM_NONBLOCK)
            {
              ret = -EAGAIN;
              break;
            }

          ret = snd_pcm_wait(pcm, -1);
          if (ret < 0)
            {
              break;
            }

          continue;
        }

      len = snd_pcm_frames_to_bytes(pcm, frames);
      if (len < 0)
        {
          ret = len;
          break;
        }

      out = (FAR uint8_t *)areas->addr +
            snd_pcm_frames_to_bytes(pcm, offset);

      memcpy(out, in, len);
      in += len;
      size -= frames;
      xfer += frames;

      if (pcm->volume != 1.0)
        {
          snd_pcm_softvol_scale(pcm->format, pcm->volume, out,
                                snd_pcm_bytes_to_samples(pcm, len));
        }

      ret = snd_pcm_mmap_commit(pcm, offset, frames);
      if (ret < 0)
        {
          break;
        }

      if (snd_pcm_state(pcm) == SND_PCM_STATE_PREPARED &&
          pcm->period_size * pcm->appl >= pcm->start_threshold)
        {
          ret = snd_pcm_start(pcm);
          if (ret < 0)
            {
              break;
            }
        }
    }

  return xfer > 0 ? xfer : ret;
}

snd_pcm_sframes_t snd_pcm_mmap_readi(FAR snd_pcm_t *pcm, FAR void *buffer,
                                     snd_pcm_uframes_t size)
{
  FAR const snd_pcm_channel_area_t *areas;
  snd_pcm_uframes_t xfer = 0;
  FAR uint8_t *out = buffer;
  snd_pcm_uframes_t offset;
  snd_pcm_uframes_t frames;
  FAR uint8_t *in;
  int ret = 0;
  ssize_t len;

  if (snd_pcm_state(pcm) == SND_PCM_STATE_PREPARED)
    {
      ret = snd_pcm_start(pcm);
      if (ret < 0)
        {
          return ret;
        }
    }

  while (size > 0)
    {
      frames = size;
      ret = snd_pcm_mmap_begin(pcm, &areas, &offset, &frames);
      if (ret < 0)
        {
          break;
        }

      if (frames == 0)
        {
          if (pcm->mode & SND_PCM_NONBLOCK)
            {
              ret = -EAGAIN;
              break;
            }

          ret = snd_pcm_wait(pcm, -1);
          if (ret < 0)
            {
              break;
            }

          continue;
        }

      len = snd_pcm_frames_to_bytes(pcm, frames);
      if (len < 0)
        {
          ret = len;
          break;
        }

      in = (FAR uint8_t *)areas->addr +
           snd_pcm_frames_to_bytes(pcm, offset);

      memcpy(out, in, len);
      out += len;
      size -= frames;
      xfer += frames;

      if (pcm->volume != 1.0)
        {
          snd_pcm_softvol_scale(pcm->format, pcm->volume, out,
                                snd_pcm_bytes_to_samples(pcm, len));
        }

      ret = snd_pcm_mmap_commit(pcm, offset, frames);
      if (ret < 0)
        {
          break;
        }
    }

  return xfer > 0 ? xfer : ret;
}

int snd_pcm_new(FAR snd_pcm_t **pcmp, snd_pcm_type_t type,
                FAR const char *name, snd_pcm_stream_t stream, int mode)
{
  FAR snd_pcm_t *pcm;
  pcm = calloc(1, sizeof(*pcm));
  if (!pcm)
    {
      return -ENOMEM;
    }

  pcm->type = type;
  if (name)
    {
      pcm->name = strdup(name);
    }

  pcm->stream = stream;
  pcm->mode = mode;
  pcm->ops_arg = pcm;
  pcm->volume = 1.0;
  *pcmp = pcm;
  return 0;
}

int snd_pcm_free(FAR snd_pcm_t *pcm)
{
  assert(pcm);
  free(pcm->name);
  free(pcm);
  return 0;
}
