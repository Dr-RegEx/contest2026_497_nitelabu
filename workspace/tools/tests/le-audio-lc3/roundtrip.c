/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <lc3.h>

int main(void)
{
  enum { rate = 16000, frame_us = 10000, samples = 160, bytes = 40,
         frames = 100, count = samples * frames };
  int16_t *input = calloc(count, sizeof(*input));
  int16_t *output = calloc(count, sizeof(*output));
  uint8_t packet[bytes];
  void *emem = malloc(lc3_encoder_size(frame_us, rate));
  void *dmem = malloc(lc3_decoder_size(frame_us, rate));
  assert(input && output && emem && dmem);
  lc3_encoder_t enc = lc3_setup_encoder(frame_us, rate, 0, emem);
  lc3_decoder_t dec = lc3_setup_decoder(frame_us, rate, 0, dmem);
  assert(enc && dec);
  assert(lc3_frame_samples(frame_us, rate) == samples);
  assert(lc3_frame_bytes(frame_us, 32000) == bytes);
  for (int i = 0; i < count; i++)
    input[i] = (int16_t)(10000 * sin(2 * 3.141592653589793 * 440 * i / rate));
  for (int f = 0; f < frames; f++)
    {
      assert(lc3_encode(enc, LC3_PCM_FORMAT_S16, input + f * samples,
                        1, bytes, packet) == 0);
      assert(lc3_decode(dec, packet, bytes, LC3_PCM_FORMAT_S16,
                        output + f * samples, 1) == 0);
    }
  int delay = lc3_delay_samples(frame_us, rate);
  assert(delay >= 0 && delay < count / 2);
  double signal = 0, noise = 0;
  for (int i = samples * 5 + delay; i < count; i++)
    {
      double expected = input[i - delay];
      double diff = expected - output[i];
      signal += expected * expected;
      noise += diff * diff;
    }
  double snr = 10 * log10(signal / noise);
  /* This is a local corruption check, not an xTS/audio quality threshold. */
  assert(isfinite(snr) && snr > 10);
  printf("PASS: 100 LC3 frames, 16kHz mono, 10ms, 40 bytes/frame; "
         "delay=%d samples, aligned tone SNR=%.2f dB\n", delay, snr);
  free(dmem); free(emem); free(output); free(input);
  return 0;
}
