/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/romfs_stub.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/compiler.h>

weak_data const unsigned char aligned_data(4) romfs_img[] =
{
  0x00
};

weak_data const unsigned int romfs_img_len = 1;
