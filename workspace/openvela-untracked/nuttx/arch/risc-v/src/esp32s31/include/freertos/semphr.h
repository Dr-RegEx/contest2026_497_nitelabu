/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ESP32S31_FREERTOS_SEMPHR_H
#define ESP32S31_FREERTOS_SEMPHR_H
#include "FreeRTOS.h"
#include "queue.h"
struct esp32s31_sd_sem_s;
typedef struct esp32s31_sd_sem_s *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinaryWithCaps(UBaseType_t caps);
SemaphoreHandle_t xSemaphoreCreateMutexWithCaps(UBaseType_t caps);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *woken);
void vSemaphoreDeleteWithCaps(SemaphoreHandle_t sem);
#endif
