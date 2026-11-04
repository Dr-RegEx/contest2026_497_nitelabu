/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_es8389.h
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_ES8389_H
#define __BOARDS_RISCV_ESP32S31_ES8389_H

#include <stdbool.h>
#include <stdint.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/mutex.h>

/* Korvo qualification uses no external MCLK: 16 kHz, two Philips 16-bit
 * slots. Physical MCLK wiring differs between the guide and official BSP;
 * this BCLK-derived format does not depend on that unresolved PCB detail.
 * 0x20 in esp_codec_dev is an eight-bit address: NuttX uses 0x10.
 * This control backend owns the physical codec, not an audio session.
 */

#define S31_ES8389_ADDRESS 0x10
#define S31_ES8389_RATE 16000
#define S31_ES8389_BCLK (S31_ES8389_RATE * 32)
#define S31_ES8389_PLAYBACK 1
#define S31_ES8389_CAPTURE 2

struct s31_es8389_s
{
  mutex_t lock;
  struct i2c_master_s *i2c;
  void (*pa)(bool enable);
  uint8_t active;
  uint16_t volume;
  bool ready;
  bool muted;
  bool faulted;
};

/* Initialize lock with NXMUTEX_INITIALIZER, volume in 0..1000 (<=0 dB).
 * PA must be configured as an output and initially disabled by the board.
 * prepare must precede bus START; enable follows START, with clocks running.
 * disable precedes bus STOP. A failed operation preserves active ownership
 * until disable, blocks further enable and requires a fresh prepare.
 */

int s31_es8389_prepare(struct s31_es8389_s *codec);
int s31_es8389_enable(struct s31_es8389_s *codec, uint8_t direction);
int s31_es8389_disable(struct s31_es8389_s *codec, uint8_t direction);
int s31_es8389_volume(struct s31_es8389_s *codec, uint16_t volume);
int s31_es8389_mute(struct s31_es8389_s *codec, bool mute);
int s31_es8389_health(struct s31_es8389_s *codec);

#endif
