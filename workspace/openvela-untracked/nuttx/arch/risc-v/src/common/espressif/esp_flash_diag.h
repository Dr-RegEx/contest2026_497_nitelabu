/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ESP_FLASH_DIAG_H
#define __ESP_FLASH_DIAG_H
#include <stdint.h>
struct esp_flash_diag_s
{
  uint64_t read_calls, read_bytes;
  uint64_t write_calls, write_bytes;
  uint64_t erase_calls, erase_bytes;
  uint64_t guards, body_sum, body_max, span_sum, span_max;
  uint64_t kind_body_max[4], kind_span_max[4];
  uint64_t kind_body_sum[4], kind_span_sum[4];
  uint64_t read_transfers, program_transfers, erased_sectors;
  uint32_t span_migrations;
  uint32_t staging_checks, staging_invalid, staging_min, staging_max;
  uint32_t rom_page_size, staging_bytes, configured_mhz;
};
struct esp_timer_diag_s
{
  uint64_t hw_us, monotonic_us, handled_ticks, pending_ticks;
  uint32_t valid, retries, sequence;
};
#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
void esp32s31_flash_diag_snapshot(struct esp_flash_diag_s *snapshot);
uint64_t esp32s31_timer_diag_time_us(void);
void esp32s31_timer_diag_snapshot(struct esp_timer_diag_s *snapshot);
#endif
#ifdef CONFIG_ESP32S31_KORVO_HMI_NETDIAG
void esp32s31_timer_stall_dump(void);
#endif
#endif
