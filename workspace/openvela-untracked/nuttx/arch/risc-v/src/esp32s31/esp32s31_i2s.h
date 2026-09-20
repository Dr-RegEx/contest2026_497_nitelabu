/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_i2s.h
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_ESP32S31_I2S_H
#define __ARCH_RISCV_SRC_ESP32S31_ESP32S31_I2S_H

#include <nuttx/audio/i2s.h>

/* I2S0, Philips 16-bit mono/stereo, half duplex.  The board supplies PA
 * control; it must remain inactive except during a playback transfer.
 */

struct i2s_dev_s *esp32s31_i2s_initialize(int port);
#ifdef CONFIG_ESP32S31_I2S_DUPLEX
struct i2s_dev_s *esp32s31_i2s_duplex_initialize(bool rx);
#endif
void esp32s31_audio_pa(bool enable);

#endif
