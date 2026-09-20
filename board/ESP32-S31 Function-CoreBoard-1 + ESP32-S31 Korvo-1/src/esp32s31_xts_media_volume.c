/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 *
 * Dedicated original-WAV volume: 14 MiB volatile PSRAM followed by the fixed
 * Flash interval [0xd00000, 0x1000000). Registration never writes Flash.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/drivers/drivers.h>
#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mtd/mtd.h>

#include "esp_memory_utils.h"
#include "espressif/esp_spiflash.h"
#include "espressif/esp_spiflash_mtd.h"
#include "esp32s31-core-function-board.h"

#define S31_VOLUME_RAM_SIZE      (14u * 1024u * 1024u)
#define S31_VOLUME_FLASH_SIZE    (3u * 1024u * 1024u)
#define S31_VOLUME_FLASH_OFFSET  0xd00000u
#define S31_VOLUME_SIZE          (S31_VOLUME_RAM_SIZE + S31_VOLUME_FLASH_SIZE)
#define S31_VOLUME_BLOCK         64u
#define S31_VOLUME_ERASE         4096u

struct s31_media_volume_s
{
  struct mtd_dev_s mtd;
  struct mtd_dev_s *part[2];
};

static struct s31_media_volume_s g_volume;
static bool g_attempted;
static int g_result = -ENODEV;

/* Validate the entire request before dispatching any part. Subtract before
 * comparing so even SIZE_MAX counts cannot wrap or cause a partial write.
 */

static bool s31_volume_range(off_t start, size_t count, size_t unit)
{
  uint64_t limit = S31_VOLUME_SIZE / unit;

  return start >= 0 && (uint64_t)start <= limit &&
         (uint64_t)count <= limit - (uint64_t)start;
}

static int s31_volume_erase(struct mtd_dev_s *dev, off_t start,
                            size_t count)
{
  struct s31_media_volume_s *priv = (struct s31_media_volume_s *)dev;
  const size_t split = S31_VOLUME_RAM_SIZE / S31_VOLUME_ERASE;
  size_t take;
  int part;
  int ret;

  if (!s31_volume_range(start, count, S31_VOLUME_ERASE))
    {
      return -EINVAL;
    }

  while (count > 0)
    {
      part = (size_t)start >= split;
      take = part ? count : (count < split - start ? count : split - start);
      ret = MTD_ERASE(priv->part[part], part ? start - split : start, take);
      if (ret < 0)
        {
          return ret;
        }

      /* RAMMTD returns OK, while this exact Flash partition returns the
       * erased-block count. Do not accept a short positive Flash erase.
       */

      if ((!part && ret != OK) || (part && (size_t)ret != take))
        {
          return -EIO;
        }

      start += take;
      count -= take;
    }

  return OK;
}

static ssize_t s31_volume_transfer(struct mtd_dev_s *dev, off_t start,
                                   size_t count, uint8_t *readbuf,
                                   const uint8_t *writebuf, bool writing,
                                   bool bytes)
{
  struct s31_media_volume_s *priv = (struct s31_media_volume_s *)dev;
  size_t unit = bytes ? 1 : S31_VOLUME_BLOCK;
  size_t split = S31_VOLUME_RAM_SIZE / unit;
  size_t done = 0;
  size_t take;
  off_t local;
  ssize_t ret;
  int part;

  if (!s31_volume_range(start, count, unit) ||
      (count > 0 && (writing ? writebuf == NULL : readbuf == NULL)))
    {
      return -EINVAL;
    }

  while (done < count)
    {
      part = (size_t)start >= split;
      take = count - done;
      if (!part && take > split - start)
        {
          take = split - start;
        }

      local = part ? start - split : start;
      if (bytes)
        {
          if (writing)
            {
#ifdef CONFIG_MTD_BYTE_WRITE
              ret = MTD_WRITE(priv->part[part], local, take,
                              writebuf + done);
#else
              ret = -ENOSYS;
#endif
            }
          else
            {
              ret = MTD_READ(priv->part[part], local, take, readbuf + done);
            }
        }
      else if (writing)
        {
          ret = MTD_BWRITE(priv->part[part], local, take,
                           writebuf + done * unit);
        }
      else
        {
          ret = MTD_BREAD(priv->part[part], local, take, readbuf + done * unit);
        }

      if (ret < 0)
        {
          return done > 0 ? (ssize_t)done : ret;
        }

      if ((size_t)ret > take)
        {
          return done > 0 ? (ssize_t)done : -EIO;
        }

      done += ret;
      start += ret;
      if ((size_t)ret < take)
        {
          break;
        }
    }

  return done;
}

static ssize_t s31_volume_bread(struct mtd_dev_s *dev, off_t start,
                                size_t count, uint8_t *buffer)
{
  return s31_volume_transfer(dev, start, count, buffer, NULL, false, false);
}

static ssize_t s31_volume_bwrite(struct mtd_dev_s *dev, off_t start,
                                 size_t count, const uint8_t *buffer)
{
  return s31_volume_transfer(dev, start, count, NULL, buffer, true, false);
}

static ssize_t s31_volume_read(struct mtd_dev_s *dev, off_t start,
                               size_t count, uint8_t *buffer)
{
  return s31_volume_transfer(dev, start, count, buffer, NULL, false, true);
}

#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t s31_volume_write(struct mtd_dev_s *dev, off_t start,
                                size_t count, const uint8_t *buffer)
{
  return s31_volume_transfer(dev, start, count, NULL, buffer, true, true);
}
#endif

static int s31_volume_ioctl(struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
  struct mtd_geometry_s *geo = (struct mtd_geometry_s *)arg;

  if (cmd == MTDIOC_GEOMETRY)
    {
      if (geo == NULL)
        {
          return -EINVAL;
        }

      memset(geo, 0, sizeof(*geo));
      geo->blocksize = S31_VOLUME_BLOCK;
      geo->erasesize = S31_VOLUME_ERASE;
      geo->neraseblocks = S31_VOLUME_SIZE / S31_VOLUME_ERASE;
      return OK;
    }

  /* Never forward BULKERASE, XIP or other parent-device controls. */

  return -ENOTTY;
}

int esp32s31_xts_media_volume_initialize(void)
{
  struct rammtd_config_s config = {0};
  struct mtd_geometry_s geo;
  uint8_t *ram;
  int ret;

  if (g_attempted)
    {
      return g_result;
    }

  g_attempted = true;
  ram = kumm_memalign(S31_VOLUME_BLOCK, S31_VOLUME_RAM_SIZE);
  if (ram == NULL)
    {
      syslog(LOG_ERR, "xTS WAV volume: contiguous 14 MiB PSRAM unavailable\n");
      g_result = -ENOMEM;
      return g_result;
    }

  if (!esp_ptr_external_ram(ram) ||
      !esp_ptr_external_ram(ram + S31_VOLUME_RAM_SIZE - 1))
    {
      kumm_free(ram);
      g_result = -EFAULT;
      return g_result;
    }

  /* Initialize volatile media only; the Flash range is never erased here. */

  memset(ram, 0xff, S31_VOLUME_RAM_SIZE);
  config.start = ram;
  config.size = S31_VOLUME_RAM_SIZE;
  config.blocksize = S31_VOLUME_BLOCK;
  config.erasesize = S31_VOLUME_ERASE;
  config.erase_state = 0xff;
  strlcpy(config.name, "wavram", sizeof(config.name));
  g_volume.part[0] = rammtd_initialize_with_config(&config);
  if (g_volume.part[0] == NULL)
    {
      kumm_free(ram);
      g_result = -ENOMEM;
      return g_result;
    }

  ret = esp_spiflash_init();
  if (ret < 0)
    {
      goto fail_unregistered;
    }

  g_volume.part[1] = esp_spiflash_alloc_mtdpart(S31_VOLUME_FLASH_OFFSET,
                                             S31_VOLUME_FLASH_SIZE);
  if (g_volume.part[1] == NULL)
    {
      ret = -ENODEV;
      goto fail_unregistered;
    }

  ret = MTD_IOCTL(g_volume.part[1], MTDIOC_GEOMETRY, (unsigned long)&geo);
  if (ret < 0)
    {
      goto fail_unregistered;
    }

  if (geo.blocksize != S31_VOLUME_BLOCK || geo.erasesize != S31_VOLUME_ERASE ||
      geo.neraseblocks != S31_VOLUME_FLASH_SIZE / S31_VOLUME_ERASE)
    {
      ret = -EINVAL;
      goto fail_unregistered;
    }

  g_volume.mtd.erase = s31_volume_erase;
  g_volume.mtd.bread = s31_volume_bread;
  g_volume.mtd.bwrite = s31_volume_bwrite;
  g_volume.mtd.read = s31_volume_read;
#ifdef CONFIG_MTD_BYTE_WRITE
  g_volume.mtd.write = s31_volume_write;
#endif
  g_volume.mtd.ioctl = s31_volume_ioctl;
  g_volume.mtd.name = "wavvol";

  ret = register_mtddriver("/dev/wavvol", &g_volume.mtd, 0600, NULL);
  if (ret < 0)
    {
      goto fail_unregistered;
    }

  g_result = ret;
  if (ret == OK)
    {
      syslog(LOG_INFO, "xTS WAV volume: /dev/wavvol RAM=%p+%u "
             "Flash=%#x+%#x total=%u; not mounted or formatted\n",
             ram, S31_VOLUME_RAM_SIZE, S31_VOLUME_FLASH_OFFSET,
             S31_VOLUME_FLASH_SIZE, S31_VOLUME_SIZE);
    }

  return ret;

fail_unregistered:
  rammtd_uninitialize(g_volume.part[0]);
  g_volume.part[0] = NULL;
  kumm_free(ram);
  g_result = ret;
  return ret;
}
