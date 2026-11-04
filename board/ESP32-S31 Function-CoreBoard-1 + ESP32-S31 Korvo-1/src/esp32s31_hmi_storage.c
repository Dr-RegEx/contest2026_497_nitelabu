/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_hmi_storage.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>

#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>

#include "espressif/esp_spiflash.h"
#include "espressif/esp_spiflash_mtd.h"
#include "esp32s31-core-function-board.h"

#define HMI_DATA_DEVICE "/dev/hmidata"
#define HMI_DATA_MOUNT  "/data"

/* Check the entire partition, not just the header or first erase block.
 * Existing/corrupt volumes must not be erased by an automatic recovery path.
 */

static int hmi_data_is_blank(FAR struct mtd_dev_s *mtd, size_t size)
{
  uint8_t buffer[256];
  size_t offset;
  size_t i;
  ssize_t result;

  for (offset = 0; offset < size; offset += sizeof(buffer))
    {
      size_t count = size - offset;
      if (count > sizeof(buffer))
        {
          count = sizeof(buffer);
        }

      result = MTD_READ(mtd, offset, count, buffer);
      if (result != (ssize_t)count)
        {
          return result < 0 ? (int)result : -EIO;
        }

      for (i = 0; i < count; i++)
        {
          if (buffer[i] != 0xff)
            {
              return 0;
            }
        }
    }

  return 1;
}

int esp32s31_hmi_storage_initialize(void)
{
  struct mtd_geometry_s geometry;
  FAR struct mtd_dev_s *raw;
  FAR struct mtd_dev_s *data;
  uint64_t capacity;
  uint32_t offset = CONFIG_ESP32S31_HMI_DATA_OFFSET;
  uint32_t size = CONFIG_ESP32S31_HMI_DATA_SIZE;
  int ret;

  ret = esp_spiflash_init();
  if (ret < 0)
    {
      return ret;
    }

  raw = esp_spiflash_mtd();
  if (raw == NULL)
    {
      return -ENODEV;
    }

  ret = MTD_IOCTL(raw, MTDIOC_GEOMETRY, (unsigned long)&geometry);
  if (ret < 0)
    {
      return ret;
    }

  capacity = (uint64_t)geometry.erasesize * geometry.neraseblocks;
  if (geometry.erasesize == 0 || size == 0 ||
      offset < 0x500000 || offset % geometry.erasesize != 0 ||
      size % geometry.erasesize != 0 || (uint64_t)offset + size > capacity)
    {
      return -EINVAL;
    }

  data = esp_spiflash_alloc_mtdpart(offset, size);
  if (data == NULL)
    {
      return -ENODEV;
    }

  ret = register_mtddriver(HMI_DATA_DEVICE, data, 0600, NULL);
  if (ret < 0)
    {
      return ret;
    }

  ret = hmi_data_is_blank(data, size);
  if (ret < 0)
    {
      return ret;
    }

  if (ret == 1)
    {
      syslog(LOG_INFO, "HMI data entire partition blank; formatting once\n");
      ret = nx_mount(HMI_DATA_DEVICE, HMI_DATA_MOUNT,
                     "littlefs", 0, "forceformat");
      if (ret < 0)
        {
          return ret;
        }

      /* Startup owns the final mount and gates service startup on success. */

      ret = nx_umount2(HMI_DATA_MOUNT, 0);
      if (ret < 0)
        {
          return ret;
        }
    }

  syslog(LOG_INFO, "HMI data MTD ready: offset=%#lx size=%#lx\n",
         (unsigned long)offset, (unsigned long)size);
  return 0;
}
