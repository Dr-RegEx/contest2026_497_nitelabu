/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_flash.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <nuttx/drivers/drivers.h>
#include <nuttx/fs/fs.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/mtd/mtd.h>

#include "espressif/esp_spiflash.h"
#include "espressif/esp_spiflash_mtd.h"
#include "esp32s31-core-function-board.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The competition image reserves 0x000000..0xbfffff for firmware and AppFS.
 * Default tests use a separate fixed 1 MiB range. The opt-in large FS
 * profile preserves that region and uses the following 3 MiB instead.
 */

#ifdef CONFIG_ESP32S31_XTS_FLASH_LARGE
#  ifdef CONFIG_ESP32S31_XTS_MEDIA_VOLUME
#    error "Large filesystem scratch overlaps the WAV volume Flash"
#  endif
#  define S31_XTS_FLASH_OFFSET  0xd00000u
#  define S31_XTS_FLASH_SIZE    0x300000u
#else
#  define S31_XTS_FLASH_OFFSET  0xc00000u
#  define S31_XTS_FLASH_SIZE    0x100000u
#endif
#define S31_XTS_FLASH_DEVICE  "/dev/xtsflash"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32s31_xts_flash_initialize(void)
{
  struct mtd_geometry_s geometry;
  struct mtd_dev_s *mtd;
  int ret;

  ret = esp_spiflash_init();
  if (ret < 0)
    {
      return ret;
    }

  mtd = esp_spiflash_alloc_mtdpart(S31_XTS_FLASH_OFFSET, S31_XTS_FLASH_SIZE);
  if (mtd == NULL)
    {
      return -ENODEV;
    }

  ret = MTD_IOCTL(mtd, MTDIOC_GEOMETRY, (unsigned long)&geometry);
  if (ret < 0)
    {
      return ret;
    }

  if ((uint64_t)geometry.erasesize * geometry.neraseblocks !=
      S31_XTS_FLASH_SIZE)
    {
      return -EINVAL;
    }

  /* No erase, write, formatting or filesystem mount at registration. */

  ret = register_mtddriver(S31_XTS_FLASH_DEVICE, mtd, 0600, NULL);
  if (ret == OK)
    {
      syslog(LOG_INFO, "xTS Flash scratch: %s offset=%#x size=%#x\n",
             S31_XTS_FLASH_DEVICE, S31_XTS_FLASH_OFFSET, S31_XTS_FLASH_SIZE);
    }

#ifdef CONFIG_ESP32S31_XTS_FLASH_RAW
  if (ret == OK)
    {
      ret = ftl_initialize_by_path("/dev/xtsblock", mtd, O_RDWR);
      if (ret == OK)
        {
          ret = bchdev_register("/dev/xtsblock", "/dev/xtsraw", O_RDWR);
          if (ret < 0)
            {
              unregister_blockdriver("/dev/xtsblock");
            }
          else
            {
              syslog(LOG_INFO, "xTS raw Flash: /dev/xtsraw size=%#x\n",
                     S31_XTS_FLASH_SIZE);
            }
        }
    }
#endif

  return ret;
}
