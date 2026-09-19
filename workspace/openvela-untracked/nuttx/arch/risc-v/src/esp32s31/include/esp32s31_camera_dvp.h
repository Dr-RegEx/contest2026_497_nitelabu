/****************************************************************************
 * arch/risc-v/src/esp32s31/include/esp32s31_camera_dvp.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_CAMERA_DVP_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_CAMERA_DVP_H

#include <sys/time.h>
#include <stddef.h>
#include <stdint.h>

#include <nuttx/compiler.h>
#include <nuttx/mutex.h>

typedef int (*esp32s31_camera_dvp_capture_t)(uint8_t result,
                                              uint32_t size,
                                              FAR const struct timeval *ts,
                                              FAR void *arg);

void esp32s31_camera_dvp_set_capture_lock(FAR mutex_t *lock);
int esp32s31_camera_dvp_initialize(void);
int esp32s31_camera_dvp_start(void);
int esp32s31_camera_dvp_stop(void);
void esp32s31_camera_dvp_uninitialize(void);

/* Connect the DVP frame queue to the NuttX image-data lower half.  The
 * controller keeps its DMA-safe internal frame and copies completed frames
 * to the caller-owned V4L2 buffer.  A NULL callback disables completion
 * notification and is useful while tearing down a stream.
 */

int esp32s31_camera_dvp_set_target(FAR uint8_t *buffer, size_t size,
                                   esp32s31_camera_dvp_capture_t callback,
                                   FAR void *arg);

#endif
