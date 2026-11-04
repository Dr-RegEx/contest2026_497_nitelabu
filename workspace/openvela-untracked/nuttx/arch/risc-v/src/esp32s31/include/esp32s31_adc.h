/****************************************************************************
 * arch/risc-v/src/esp32s31/include/esp32s31_adc.h
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_ADC_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_ADC_H

struct adc_dev_s;

/* One configured, exclusive ADC1 client: Function raw fixture or Korvo keys.
 * This getter does not power the ADC or start a conversion. The key lower
 * half owns this device directly; it is not simultaneously exposed as adc0.
 */

struct adc_dev_s *esp32s31_adc_initialize(void);

#endif /* __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_ADC_H */
