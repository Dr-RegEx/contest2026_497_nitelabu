/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_PCM_H
#define __BOARDS_RISCV_ESP32S31_PCM_H

#include <nuttx/audio/audio.h>
#include <nuttx/audio/i2s.h>
#include "esp32s31_es8389.h"

/* Board-lifetime endpoints and one persistent worker, FLAT qualification.
 * No codec register programming or real capture occurs at registration.
 */

int s31_pcm_initialize(struct s31_es8389_s *codec,
                       struct i2s_dev_s *playback,
                       struct i2s_dev_s *capture);
struct audio_lowerhalf_s *s31_pcm_endpoint(bool capture);

#endif
