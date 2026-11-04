/* SPDX-License-Identifier: Apache-2.0 */
/* Korvo SUB3 GT1151: I2C polling because TP_INT is not routed to the SoC.
 * Expose standard NuttX touch samples; never reset the shared camera bus. */
#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/touchscreen.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include "esp_i2c.h"

/* Reports are serialized as one point per sample. */
static struct touch_lowerhalf_s g_touch = { .maxpoint = 1 };
static struct work_s g_work;
static struct i2c_master_s *g_i2c;
static uint8_t g_addr;
static struct touch_point_s g_last[5];
static uint8_t g_count;

static int readreg(uint16_t reg, uint8_t *data, int size)
{
  uint8_t address[2] = {reg >> 8, reg & 255};
  struct i2c_msg_s msg[2] = {
    {.frequency=100000,.addr=g_addr,.buffer=address,.length=2},
    {.frequency=100000,.addr=g_addr,.flags=I2C_M_READ,.buffer=data,.length=size}
  };
  return I2C_TRANSFER(g_i2c, msg, 2);
}

static void report(struct touch_point_s *point, uint8_t event)
{
  struct touch_sample_s sample;
  memset(&sample, 0, sizeof(sample));
  sample.npoints = 1;
  sample.point[0] = *point;
  sample.point[0].flags = event | TOUCH_ID_VALID | TOUCH_POS_VALID;
  touch_event(g_touch.priv, &sample);
}

static void sample_touch(void *arg)
{
  uint8_t data[43], status, ack[3] = {0x81,0x4e,0};
  struct touch_point_s next[5];
  struct i2c_msg_s msg = {.frequency=100000,.addr=g_addr,.buffer=ack,.length=3};
  int count, i, j;
  uint8_t sum = 0;
  (void)arg;
  if (readreg(0x814e, &status, 1) < 0 || !(status & 0x80)) goto again;
  count = status & 15;
  if (count > 5) { I2C_TRANSFER(g_i2c, &msg, 1); goto again; }
  if (count && readreg(0x814e, data, count * 8 + 3) < 0) goto again;
  if (I2C_TRANSFER(g_i2c, &msg, 1) < 0) goto again;
  if (count) {
    for (i=0; i<count*8+3; i++) sum += data[i];
    if (sum != 0 || (data[0] & 15) != count) goto again;
  }
  memset(next, 0, sizeof(next));
  for (i=0; i<count; i++) {
    uint8_t *p = &data[1+i*8];
    next[i].id = p[0] & 15;
    next[i].x = p[1] | p[2] << 8;
    next[i].y = p[3] | p[4] << 8;
    if (next[i].x >= 800 || next[i].y >= 480) goto again;
  }
  for (i=0; i<g_count; i++) {
    for (j=0; j<count && next[j].id != g_last[i].id; j++);
    if (j == count) report(&g_last[i], TOUCH_UP);
  }
  for (i=0; i<count; i++) {
    for (j=0; j<g_count && g_last[j].id != next[i].id; j++);
    if (j == g_count || next[i].x != g_last[j].x ||
        next[i].y != g_last[j].y)
      report(&next[i], j == g_count ? TOUCH_DOWN : TOUCH_MOVE);
  }
  memcpy(g_last, next, sizeof(next));
  g_count = count;
again:
  work_queue(LPWORK, &g_work, sample_touch, NULL, MSEC2TICK(10));
}

int esp32s31_touch_initialize(void)
{
  uint8_t id[5] = {0};
  int ret;
  g_i2c = esp_i2cbus_initialize(0);
  if (!g_i2c) return -ENODEV;
  g_addr = 0x5d;
  ret = readreg(0x8140, id, 4);
  if (ret < 0) { g_addr = 0x14; ret = readreg(0x8140, id, 4); }
  if (ret < 0) return ret;
  syslog(LOG_INFO, "Korvo touch: addr=0x%02x product=%s\n", g_addr, id);
  if (memcmp(id, "115", 3) != 0 && memcmp(id, "911", 3) != 0) return -ENODEV;
  ret = touch_register(&g_touch, "/dev/input0", 32);
  if (ret < 0) return ret;
  return work_queue(LPWORK, &g_work, sample_touch, NULL, MSEC2TICK(10));
}
