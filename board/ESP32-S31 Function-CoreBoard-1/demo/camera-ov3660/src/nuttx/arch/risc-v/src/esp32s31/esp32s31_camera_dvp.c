/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_camera_dvp.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mutex.h>
#include <nuttx/sched.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>

#include <nuttx/clock.h>
#include <nuttx/wqueue.h>
#include <nuttx/spinlock.h>

#include "esp32s31_camera_dvp.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_heap_caps.h"

struct esp32s31_camera_dvp_s
{
  esp_cam_ctlr_handle_t controller;
  void *frame[2];
  size_t frame_size;
  enum { FRAME_FREE, FRAME_DMA, FRAME_COPY } frame_state[2];
  int copy_index;
  bool copy_busy;
  bool copy_running;
  pid_t copy_pid;
  uint32_t generation;
  uint32_t copy_generation;
  FAR mutex_t *capture_lock;
  uint8_t *target;
  size_t target_size;
  esp32s31_camera_dvp_capture_t capture_cb;
  void *capture_arg;
  size_t received;
  struct work_s copy_work;
  bool enabled;
  bool started;
};

static struct esp32s31_camera_dvp_s g_camera;
static spinlock_t g_camera_lock = SP_UNLOCKED;

static bool IRAM_ATTR esp32s31_camera_get_buffer
  (esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *transaction,
   void *arg)
{
  struct esp32s31_camera_dvp_s *camera = arg;

  irqstate_t flags;
  int i;

  (void)handle;
  transaction->buffer = NULL;
  transaction->buflen = 0;
  flags = spin_lock_irqsave(&g_camera_lock);
  for (i = 0; i < 2; i++)
    {
      if (camera->frame_state[i] == FRAME_FREE)
        {
          camera->frame_state[i] = FRAME_DMA;
          transaction->buffer = camera->frame[i];
          transaction->buflen = camera->frame_size;
          break;
        }
    }

  spin_unlock_irqrestore(&g_camera_lock, flags);

  /* The controller requests the next frame BEFORE reporting completion of
   * the previous one.  If both buffers are owned, use its private backup
   * buffer; never return a frame still being copied by HPWORK.
   */

  return false;
}

static void esp32s31_camera_copy_work(FAR void *arg)
{
  struct esp32s31_camera_dvp_s *camera = arg;
  struct timespec ts;
  struct timeval tv;
  irqstate_t flags;
  esp32s31_camera_dvp_capture_t callback = NULL;
  void *callback_arg = NULL;
  uint8_t *target = NULL;
  mutex_t *capture_lock = camera->capture_lock;
  bool locked = false;
  size_t copied;
  int index;

  /* STREAMOFF/REQBUFS/CLOSE own this task mutex while invalidating buffers.
   * Never block here: close may hold it while draining this work queue. */
  if (capture_lock != NULL)
    {
      locked = nxmutex_trylock(capture_lock) == 0;
    }

  flags = spin_lock_irqsave(&g_camera_lock);
  index = camera->copy_index;
  copied = camera->received;
  if ((capture_lock == NULL || locked) &&
      camera->copy_generation == camera->generation &&
      camera->target != NULL && camera->capture_cb != NULL &&
      copied == camera->frame_size && copied <= camera->target_size)
    {
      target = camera->target;
      callback = camera->capture_cb;
      callback_arg = camera->capture_arg;
      camera->copy_running = true;
      camera->copy_pid = nxsched_gettid();
    }

  spin_unlock_irqrestore(&g_camera_lock, flags);
  if (target != NULL)
    {
#ifdef CONFIG_ESP32S31_CAMERA_OV3660
      /* OV3660 format register 0x4300=0x61 sends RGB565 MSB first.
       * Our V4L2 contract is RGB565 (little-endian), not RGB565X.
       * Normalize once here so every camera consumer sees that contract. */

      const uint8_t *source = camera->frame[index];
      size_t pixel;
      for (pixel = 0; pixel < copied; pixel += 2)
        {
          target[pixel] = source[pixel + 1];
          target[pixel + 1] = source[pixel];
        }
#else
      memcpy(target, camera->frame[index], copied);
#endif
      clock_systime_timespec(&ts);
      TIMESPEC_TO_TIMEVAL(&tv, &ts);
      callback(0, copied, &tv, callback_arg);
    }

  flags = spin_lock_irqsave(&g_camera_lock);
  camera->copy_running = false;
  camera->frame_state[index] = FRAME_FREE;
  camera->copy_busy = false;
  spin_unlock_irqrestore(&g_camera_lock, flags);
  if (locked)
    {
      nxmutex_unlock(capture_lock);
    }
}

static bool IRAM_ATTR esp32s31_camera_frame_done
  (esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *transaction,
   void *arg)
{
  struct esp32s31_camera_dvp_s *camera = arg;
  irqstate_t flags;
  int index;
  int ret;

  (void)handle;
  flags = spin_lock_irqsave(&g_camera_lock);
  for (index = 0; index < 2; index++)
    {
      if (transaction->buffer == camera->frame[index])
        {
          break;
        }
    }

  if (index == 2 || camera->frame_state[index] != FRAME_DMA)
    {
      spin_unlock_irqrestore(&g_camera_lock, flags);
      return false;
    }

  if (camera->copy_busy || camera->target == NULL ||
      camera->capture_cb == NULL ||
      transaction->received_size != camera->frame_size)
    {
      camera->frame_state[index] = FRAME_FREE;
      spin_unlock_irqrestore(&g_camera_lock, flags);
      return false;
    }

  camera->frame_state[index] = FRAME_COPY;
  camera->copy_index = index;
  camera->copy_generation = camera->generation;
  camera->received = transaction->received_size;
  camera->copy_busy = true;
  ret = work_queue(HPWORK, &camera->copy_work,
                   esp32s31_camera_copy_work, camera, 0);
  if (ret < 0)
    {
      camera->frame_state[index] = FRAME_FREE;
      camera->copy_busy = false;
    }

  spin_unlock_irqrestore(&g_camera_lock, flags);
  return ret == 0;
}

static int esp32s31_camera_result(esp_err_t result)
{
  return result == ESP_OK ? 0 : -EIO;
}

void esp32s31_camera_dvp_uninitialize(void)
{
  /* Stop DMA callbacks before draining deferred copies.  Otherwise an ISR
   * can enqueue new work after cancellation and access the freed frame.
   */

  if (g_camera.controller != NULL && g_camera.started)
    {
      if (esp_cam_ctlr_stop(g_camera.controller) != ESP_OK)
        {
          syslog(LOG_ERR, "Camera stop failed; retaining active buffers\n");
          return;
        }

      g_camera.started = false;
    }

  work_cancel_sync(HPWORK, &g_camera.copy_work);

  if (g_camera.controller != NULL)
    {
      if (g_camera.enabled)
        {
          if (esp_cam_ctlr_disable(g_camera.controller) != ESP_OK)
            {
              syslog(LOG_ERR, "Camera disable failed; retaining buffers\n");
              return;
            }

          g_camera.enabled = false;
        }

      if (esp_cam_ctlr_del(g_camera.controller) != ESP_OK)
        {
          syslog(LOG_ERR, "Camera delete failed; retaining buffers\n");
          return;
        }
    }

  for (int i = 0; i < 2; i++)
    {
      if (g_camera.frame[i] != NULL)
        {
          heap_caps_free(g_camera.frame[i]);
        }
    }

  g_camera = (struct esp32s31_camera_dvp_s){0};
}

void esp32s31_camera_dvp_set_capture_lock(FAR mutex_t *lock)
{
  /* Called during data init/uninit, with the upper mutex held.  Detach only
   * after all queued workers have stopped using its address. */
  if (lock == NULL)
    {
      int cancelled;
      irqstate_t flags = spin_lock_irqsave(&g_camera_lock);

      /* Also close admission when HAL stop failed: frame_done queues work
       * under this same lock, so none can appear after the drain below.
       */

      g_camera.generation++;
      g_camera.target = NULL;
      g_camera.target_size = 0;
      g_camera.capture_cb = NULL;
      g_camera.capture_arg = NULL;
      spin_unlock_irqrestore(&g_camera_lock, flags);
      cancelled = work_cancel_sync(HPWORK, &g_camera.copy_work);
      flags = spin_lock_irqsave(&g_camera_lock);
      if (cancelled == 0 && g_camera.copy_busy)
        {
          g_camera.frame_state[g_camera.copy_index] = FRAME_FREE;
          g_camera.copy_busy = false;
        }

      spin_unlock_irqrestore(&g_camera_lock, flags);
    }

  g_camera.capture_lock = lock;
}

int esp32s31_camera_dvp_set_target(FAR uint8_t *buffer, size_t size,
                                   esp32s31_camera_dvp_capture_t callback,
                                   FAR void *arg)
{
  irqstate_t flags;
  size_t frame_size;

  if (g_camera.controller == NULL || g_camera.frame[0] == NULL)
    {
      return -ENODEV;
    }

  frame_size = (size_t)CONFIG_ESP32S31_CAMERA_DVP_HRES *
               CONFIG_ESP32S31_CAMERA_DVP_VRES * 2;
  if ((buffer == NULL || size < frame_size) && callback != NULL)
    {
      return -EINVAL;
    }

  flags = spin_lock_irqsave(&g_camera_lock);
  if (g_camera.copy_running && g_camera.copy_pid != nxsched_gettid())
    {
      spin_unlock_irqrestore(&g_camera_lock, flags);
      return -EBUSY;
    }

  g_camera.generation++;
  if (buffer == NULL || size < frame_size)
    {
      g_camera.target = NULL;
      g_camera.target_size = 0;
      g_camera.capture_cb = NULL;
      g_camera.capture_arg = NULL;
    }
  else
    {
      g_camera.target = buffer;
      g_camera.target_size = size;
      g_camera.capture_cb = callback;
      g_camera.capture_arg = arg;
    }

  spin_unlock_irqrestore(&g_camera_lock, flags);
  return 0;
}

int esp32s31_camera_dvp_initialize(void)
{
  static const esp_cam_ctlr_dvp_pin_config_t pins =
    {
      .data_width = CAM_CTLR_DATA_WIDTH_8,
      .data_io =
        {
          CONFIG_ESP32S31_CAMERA_DVP_D0,
          CONFIG_ESP32S31_CAMERA_DVP_D1,
          CONFIG_ESP32S31_CAMERA_DVP_D2,
          CONFIG_ESP32S31_CAMERA_DVP_D3,
          CONFIG_ESP32S31_CAMERA_DVP_D4,
          CONFIG_ESP32S31_CAMERA_DVP_D5,
          CONFIG_ESP32S31_CAMERA_DVP_D6,
          CONFIG_ESP32S31_CAMERA_DVP_D7,
        },
      .vsync_io = CONFIG_ESP32S31_CAMERA_DVP_VSYNC,
      .de_io = CONFIG_ESP32S31_CAMERA_DVP_DE,
      .pclk_io = CONFIG_ESP32S31_CAMERA_DVP_PCLK,
      .xclk_io = CONFIG_ESP32S31_CAMERA_DVP_XCLK,
    };
  const esp_cam_ctlr_dvp_config_t config =
    {
      .ctlr_id = 0,
      .clk_src = CAM_CLK_SRC_DEFAULT,
      .h_res = CONFIG_ESP32S31_CAMERA_DVP_HRES,
      .v_res = CONFIG_ESP32S31_CAMERA_DVP_VRES,
      .input_data_color_type = CAM_CTLR_COLOR_RGB565,
      .output_data_color_type = CAM_CTLR_COLOR_RGB565,
      .cam_data_width = CAM_CTLR_DATA_WIDTH_8,
      .dma_burst_size = 64,
      .xclk_freq = CONFIG_ESP32S31_CAMERA_DVP_XCLK_FREQ,
      .pin = &pins,
      .bk_buffer_dis = 0,
    };
  const esp_cam_ctlr_evt_cbs_t callbacks =
    {
      .on_get_new_trans = esp32s31_camera_get_buffer,
      .on_trans_finished = esp32s31_camera_frame_done,
    };
  size_t frame_size;
  esp_err_t result;

  if (g_camera.controller != NULL)
    {
      return -EALREADY;
    }

  frame_size = (size_t)CONFIG_ESP32S31_CAMERA_DVP_HRES *
               CONFIG_ESP32S31_CAMERA_DVP_VRES * 2;
  result = esp_cam_new_dvp_ctlr(&config, &g_camera.controller);
  if (result != ESP_OK)
    {
      return esp32s31_camera_result(result);
    }

  g_camera.frame_size = frame_size;
  for (int i = 0; i < 2; i++)
    {
      g_camera.frame[i] = esp_cam_ctlr_alloc_buffer(g_camera.controller,
                            frame_size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
      if (g_camera.frame[i] == NULL)
        {
          esp32s31_camera_dvp_uninitialize();
          return -ENOMEM;
        }
    }

  result = esp_cam_ctlr_register_event_callbacks(g_camera.controller,
                                                  &callbacks, &g_camera);
  if (result != ESP_OK)
    {
      esp32s31_camera_dvp_uninitialize();
      return esp32s31_camera_result(result);
    }

  result = esp_cam_ctlr_enable(g_camera.controller);
  if (result != ESP_OK)
    {
      esp32s31_camera_dvp_uninitialize();
      return esp32s31_camera_result(result);
    }

  g_camera.enabled = true;
  syslog(LOG_INFO, "S31 DVP controller prepared: %ux%u RGB565, %zu bytes\n",
         CONFIG_ESP32S31_CAMERA_DVP_HRES,
         CONFIG_ESP32S31_CAMERA_DVP_VRES, frame_size);
  return 0;
}

int esp32s31_camera_dvp_start(void)
{
  esp_err_t result;

  if (g_camera.controller == NULL || !g_camera.enabled)
    {
      return -ENODEV;
    }

  if (g_camera.started)
    {
      return -EALREADY;
    }

  result = esp_cam_ctlr_start(g_camera.controller);
  if (result == ESP_OK)
    {
      g_camera.started = true;
    }

  return esp32s31_camera_result(result);
}

int esp32s31_camera_dvp_stop(void)
{
  esp_err_t result;
  irqstate_t flags;
  int cancelled;
  int i;

  if (g_camera.controller == NULL || !g_camera.started)
    {
      return -ENODEV;
    }

  result = esp_cam_ctlr_stop(g_camera.controller);
  if (result == ESP_OK)
    {
      g_camera.started = false;
      /* V4L2 calls stop while holding its capture spinlock, including from
       * our completion callback.  Do not wait for a worker which may need
       * that lock.  A running copy retains its buffer until it finishes.
       */

      cancelled = work_cancel(HPWORK, &g_camera.copy_work);
      flags = spin_lock_irqsave(&g_camera_lock);
      g_camera.generation++;
      for (i = 0; i < 2; i++)
        {
          if (g_camera.frame_state[i] == FRAME_DMA || cancelled == 0)
            {
              g_camera.frame_state[i] = FRAME_FREE;
            }
        }

      /* Only cancelled queued work relinquishes COPY here. */

      if (cancelled == 0)
        {
          g_camera.copy_busy = false;
        }

      spin_unlock_irqrestore(&g_camera_lock, flags);
    }

  return esp32s31_camera_result(result);
}
