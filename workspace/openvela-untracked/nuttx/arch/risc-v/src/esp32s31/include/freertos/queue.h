/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ESP32S31_FREERTOS_QUEUE_H
#define ESP32S31_FREERTOS_QUEUE_H
#include "FreeRTOS.h"
struct esp32s31_sd_queue_s;
typedef struct esp32s31_sd_queue_s *QueueHandle_t;
QueueHandle_t xQueueCreateWithCaps(UBaseType_t count, UBaseType_t size,
                                 UBaseType_t caps);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t ticks);
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void *item,
                           BaseType_t *woken);
void vQueueDeleteWithCaps(QueueHandle_t queue);
#endif
