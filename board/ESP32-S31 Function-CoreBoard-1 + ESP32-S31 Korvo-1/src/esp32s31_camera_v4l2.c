/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_camera_v4l2.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_ESP32S31_CAMERA_V4L2

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/video/imgdata.h>
#include <nuttx/video/imgsensor.h>
#include <nuttx/video/v4l2_cap.h>
#include <nuttx/video/video.h>

#include "esp32s31_camera_dvp.h"

struct esp32s31_camera_v4l2_s
{
  struct imgdata_s data;
  struct imgsensor_s sensor;
  imgdata_capture_t capture_cb;
  FAR void *capture_arg;
  FAR uint8_t *buffer;
  uint32_t buffer_size;
  bool registered;
};

static struct esp32s31_camera_v4l2_s g_camera_v4l2;

static const struct imgsensor_ops_s g_camera_sensor_ops;
static const struct imgdata_ops_s g_camera_data_ops;

static const struct v4l2_fmtdesc g_camera_fmts[] =
{
  {
    .pixelformat = V4L2_PIX_FMT_RGB565,
    .description = "RGB565",
  }
};

static const struct v4l2_frmsizeenum g_camera_frmsizes[] =
{
  {
    .type = V4L2_FRMSIZE_TYPE_DISCRETE,
    .discrete =
    {
      .width = CONFIG_ESP32S31_CAMERA_DVP_HRES,
      .height = CONFIG_ESP32S31_CAMERA_DVP_VRES,
    }
  }
};

static bool camera_sensor_is_available(FAR struct imgsensor_s *sensor)
{
  (void)sensor;
  return true;
}

static int camera_sensor_init(FAR struct imgsensor_s *sensor)
{
  (void)sensor;
  return 0;
}

static int camera_sensor_uninit(FAR struct imgsensor_s *sensor)
{
  (void)sensor;
  return 0;
}

static FAR const char *camera_sensor_get_driver_name
  (FAR struct imgsensor_s *sensor)
{
  (void)sensor;
#ifdef CONFIG_ESP32S31_CAMERA_OV3660
  return "ESP32-S31 DVP OV3660";
#else
  return "ESP32-S31 DVP OV2640";
#endif
}

static int camera_sensor_validate_frame_setting
  (FAR struct imgsensor_s *sensor, imgsensor_stream_type_t type,
   uint8_t nr_datafmts, FAR imgsensor_format_t *datafmts,
   FAR imgsensor_interval_t *interval)
{
  (void)sensor;
  (void)type;

  if (nr_datafmts != 1 || datafmts == NULL || interval == NULL ||
      datafmts[0].width != CONFIG_ESP32S31_CAMERA_DVP_HRES ||
      datafmts[0].height != CONFIG_ESP32S31_CAMERA_DVP_VRES ||
      datafmts[0].pixelformat != IMGSENSOR_PIX_FMT_RGB565 ||
      interval->numerator == 0 || interval->denominator == 0)
    {
      return -EINVAL;
    }

  return 0;
}

static int camera_sensor_start_capture
  (FAR struct imgsensor_s *sensor, imgsensor_stream_type_t type,
   uint8_t nr_datafmts, FAR imgsensor_format_t *datafmts,
   FAR imgsensor_interval_t *interval)
{
  int ret;

  ret = camera_sensor_validate_frame_setting(sensor, type, nr_datafmts,
                                             datafmts, interval);
  if (ret < 0)
    {
      return ret;
    }

  return esp32s31_camera_dvp_start();
}

static int camera_sensor_stop_capture(FAR struct imgsensor_s *sensor,
                                      imgsensor_stream_type_t type)
{
  (void)sensor;
  (void)type;
  return esp32s31_camera_dvp_stop();
}

static int camera_sensor_get_frame_interval
  (FAR struct imgsensor_s *sensor, imgsensor_stream_type_t type,
   FAR imgsensor_interval_t *interval)
{
  (void)sensor;
  (void)type;

  if (interval == NULL)
    {
      return -EINVAL;
    }

  /* Neither sensor timing nor software frame skipping is implemented here.
   * Report unsupported rather than the capture framework's 30 fps default.
   */

  return -ENOTTY;
}

static int camera_sensor_get_supported_value
  (FAR struct imgsensor_s *sensor, uint32_t id,
   FAR imgsensor_supported_value_t *value)
{
  (void)sensor;
  (void)id;
  (void)value;
  return -ENOTTY;
}

static int camera_sensor_get_value(FAR struct imgsensor_s *sensor,
                                   uint32_t id, uint32_t size,
                                   FAR imgsensor_value_t *value)
{
  (void)sensor;
  (void)id;
  (void)size;
  (void)value;
  return -ENOTTY;
}

static int camera_sensor_set_value(FAR struct imgsensor_s *sensor,
                                   uint32_t id, uint32_t size,
                                   imgsensor_value_t value)
{
  (void)sensor;
  (void)id;
  (void)size;
  (void)value;
  return -ENOTTY;
}

static int camera_data_init(FAR struct imgdata_s *data)
{
  esp32s31_camera_dvp_set_capture_lock(data->capture_lock);
  return 0;
}

static int camera_data_uninit(FAR struct imgdata_s *data)
{
  (void)data;
  esp32s31_camera_dvp_set_capture_lock(NULL);
  return 0;
}

static int camera_data_set_buf(FAR struct imgdata_s *data,
                               uint8_t nr_datafmts,
                               FAR imgdata_format_t *datafmts,
                               uint8_t *addr, uint32_t size)
{
  size_t frame_size;
  int ret;

  (void)data;
  if (nr_datafmts != 1 || datafmts == NULL || addr == NULL)
    {
      return -EINVAL;
    }

  frame_size = (size_t)CONFIG_ESP32S31_CAMERA_DVP_HRES *
               CONFIG_ESP32S31_CAMERA_DVP_VRES * 2;
  if (datafmts[0].width != CONFIG_ESP32S31_CAMERA_DVP_HRES ||
      datafmts[0].height != CONFIG_ESP32S31_CAMERA_DVP_VRES ||
      datafmts[0].pixelformat != IMGDATA_PIX_FMT_RGB565 ||
      size < frame_size)
    {
      return -EINVAL;
    }

  /* V4L2 rotates buffers from complete_capture without starting the stream
   * again.  Preserve its completion callback for every subsequent frame.
   */

  ret = esp32s31_camera_dvp_set_target(addr, size,
                                      g_camera_v4l2.capture_cb,
                                      g_camera_v4l2.capture_arg);
  if (ret < 0)
    {
      return ret;
    }

  g_camera_v4l2.buffer = addr;
  g_camera_v4l2.buffer_size = size;
  return 0;
}

static int camera_data_validate_frame_setting
  (FAR struct imgdata_s *data, uint8_t nr_datafmts,
   FAR imgdata_format_t *datafmts, FAR imgdata_interval_t *interval)
{
  (void)data;

  if (nr_datafmts != 1 || datafmts == NULL || interval == NULL ||
      datafmts[0].width != CONFIG_ESP32S31_CAMERA_DVP_HRES ||
      datafmts[0].height != CONFIG_ESP32S31_CAMERA_DVP_VRES ||
      datafmts[0].pixelformat != IMGDATA_PIX_FMT_RGB565 ||
      interval->numerator == 0 || interval->denominator == 0)
    {
      return -EINVAL;
    }

  return 0;
}

static int camera_data_start_capture
  (FAR struct imgdata_s *data, uint8_t nr_datafmts,
   FAR imgdata_format_t *datafmts, FAR imgdata_interval_t *interval,
   FAR imgdata_capture_t callback, FAR void *arg)
{
  int ret;

  ret = camera_data_validate_frame_setting(data, nr_datafmts, datafmts,
                                           interval);
  if (ret < 0)
    {
      return ret;
    }

  if (callback == NULL)
    {
      return -EINVAL;
    }

  if (g_camera_v4l2.buffer == NULL || g_camera_v4l2.buffer_size == 0)
    {
      return -ENOBUFS;
    }

  ret = esp32s31_camera_dvp_set_target(g_camera_v4l2.buffer,
                                       g_camera_v4l2.buffer_size,
                                       callback, arg);
  if (ret < 0)
    {
      return ret;
    }

  g_camera_v4l2.capture_cb = callback;
  g_camera_v4l2.capture_arg = arg;
  return 0;
}

static int camera_data_stop_capture(FAR struct imgdata_s *data)
{
  (void)data;
  int ret = esp32s31_camera_dvp_set_target(NULL, 0, NULL, NULL);
  if (ret == 0)
    {
      g_camera_v4l2.capture_cb = NULL;
      g_camera_v4l2.capture_arg = NULL;
    }

  return ret;
}

static const struct imgsensor_ops_s g_camera_sensor_ops =
{
  .is_available           = camera_sensor_is_available,
  .init                   = camera_sensor_init,
  .uninit                 = camera_sensor_uninit,
  .get_driver_name        = camera_sensor_get_driver_name,
  .validate_frame_setting = camera_sensor_validate_frame_setting,
  .start_capture          = camera_sensor_start_capture,
  .stop_capture           = camera_sensor_stop_capture,
  .get_frame_interval     = camera_sensor_get_frame_interval,
  .get_supported_value    = camera_sensor_get_supported_value,
  .get_value              = camera_sensor_get_value,
  .set_value              = camera_sensor_set_value,
  .frame_interval_unsupported = true,
};

static const struct imgdata_ops_s g_camera_data_ops =
{
  .deferred_copy          = true,
  .init                   = camera_data_init,
  .uninit                 = camera_data_uninit,
  .set_buf                = camera_data_set_buf,
  .validate_frame_setting = camera_data_validate_frame_setting,
  .start_capture          = camera_data_start_capture,
  .stop_capture           = camera_data_stop_capture,
};

int esp32s31_camera_v4l2_initialize(void)
{
  FAR struct imgsensor_s *sensors[1];
  int ret;

  if (g_camera_v4l2.registered)
    {
      return -EALREADY;
    }

  g_camera_v4l2.data.ops = &g_camera_data_ops;
  g_camera_v4l2.sensor.ops = &g_camera_sensor_ops;
  g_camera_v4l2.sensor.fmtdescs_num = 1;
  g_camera_v4l2.sensor.fmtdescs = g_camera_fmts;
  g_camera_v4l2.sensor.frmsizes_num = 1;
  g_camera_v4l2.sensor.frmsizes = g_camera_frmsizes;
  g_camera_v4l2.sensor.frmintervals_num = 0;
  g_camera_v4l2.sensor.frmintervals = NULL;
  sensors[0] = &g_camera_v4l2.sensor;

  ret = capture_register("/dev/video0", &g_camera_v4l2.data,
                         sensors, 1);
  if (ret < 0)
    {
      memset(&g_camera_v4l2, 0, sizeof(g_camera_v4l2));
      return ret;
    }

  g_camera_v4l2.registered = true;
  return 0;
}

int esp32s31_camera_v4l2_uninitialize(void)
{
  int ret;

  if (!g_camera_v4l2.registered)
    {
      return 0;
    }

  ret = camera_data_stop_capture(&g_camera_v4l2.data);
  if (ret < 0)
    {
      return ret;
    }

  ret = capture_unregister("/dev/video0");
  if (ret >= 0)
    {
      memset(&g_camera_v4l2, 0, sizeof(g_camera_v4l2));
    }

  return ret;
}

#endif /* CONFIG_ESP32S31_CAMERA_V4L2 */
