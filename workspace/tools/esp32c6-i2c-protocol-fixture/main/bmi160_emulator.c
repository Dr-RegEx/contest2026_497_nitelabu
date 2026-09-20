/*
 * Generic I2C protocol fixture for the ESP32-C6 DevKitM.
 *
 * The S31 remains the I2C master. The C6 responds at address 0x28 with a
 * deterministic 32-byte read payload followed by its NuttX crc32part value,
 * and records the master's write transactions for independent verification.
 */

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "driver/i2c_slave.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifndef CONFIG_I2C_SLAVE_SCL_GPIO
#  define CONFIG_I2C_SLAVE_SCL_GPIO 4
#endif
#ifndef CONFIG_I2C_SLAVE_SDA_GPIO
#  define CONFIG_I2C_SLAVE_SDA_GPIO 5
#endif
#ifndef CONFIG_I2C_SLAVE_ADDRESS
#  define CONFIG_I2C_SLAVE_ADDRESS 0x68
#endif

#define TAG "i2c_protocol_fixture"
#define PAYLOAD_SIZE 32
#define RESPONSE_SIZE (PAYLOAD_SIZE + sizeof(uint32_t))

enum fixture_event_e
{
  FIXTURE_RX
};

struct fixture_ctx_s
{
  i2c_slave_dev_handle_t handle;
  QueueHandle_t events;
  uint8_t rx_buffer[64];
  uint32_t rx_length;
  uint32_t rx_transactions;
  uint32_t write_bytes;
  bool response_queued;
};

static uint32_t fixture_crc32part(const uint8_t *src, size_t len,
                                  uint32_t crc)
{
  while (len-- > 0)
    {
      crc ^= *src++;
      for (unsigned int bit = 0; bit < 8; bit++)
        {
          crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
        }
    }

  return crc;
}

static void fixture_fill_response(uint8_t *response)
{
  uint32_t crc;

  for (uint32_t i = 0; i < PAYLOAD_SIZE; i++)
    {
      response[i] = (uint8_t)(0xa0u + i);
    }

  crc = fixture_crc32part(response, PAYLOAD_SIZE, UINT32_MAX);
  memcpy(response + PAYLOAD_SIZE, &crc, sizeof(crc));
}

static void fixture_queue_initial_response(struct fixture_ctx_s *ctx)
{
  uint8_t response[RESPONSE_SIZE];
  uint32_t written;

  fixture_fill_response(response);
  ESP_ERROR_CHECK(i2c_slave_write(ctx->handle, response, sizeof(response),
                                  &written, 1000));
  ESP_LOGI(TAG, "TX response addr=0x%02x len=%" PRIu32 " payload=%u crc=0x%08"
           PRIx32, CONFIG_I2C_SLAVE_ADDRESS, written, PAYLOAD_SIZE,
           fixture_crc32part(response, PAYLOAD_SIZE, UINT32_MAX));
}

static bool IRAM_ATTR fixture_rx_callback(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_rx_done_event_data_t *event, void *arg)
{
  struct fixture_ctx_s *ctx = (struct fixture_ctx_s *)arg;
  BaseType_t task_woken = pdFALSE;
  uint32_t length = event->length;

  if (length > sizeof(ctx->rx_buffer))
    {
      length = sizeof(ctx->rx_buffer);
    }

  ctx->rx_length = length;
  ctx->rx_transactions++;
  ctx->write_bytes += length;
  memcpy(ctx->rx_buffer, event->buffer, length);
  xQueueSendFromISR(ctx->events, &(enum fixture_event_e){FIXTURE_RX},
                    &task_woken);
  return task_woken;
}

static bool IRAM_ATTR fixture_request_callback(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_request_event_data_t *event, void *arg)
{
  /* The response is queued by the receive task after the length transaction. */
  return false;
}

static void fixture_task(void *arg)
{
  struct fixture_ctx_s *ctx = (struct fixture_ctx_s *)arg;
  enum fixture_event_e event;

  while (xQueueReceive(ctx->events, &event, portMAX_DELAY) == pdTRUE)
    {
      if (event == FIXTURE_RX)
        {
          ESP_LOGI(TAG, "RX txn=%" PRIu32 " len=%" PRIu32
                   " first=0x%02x last=0x%02x total=%" PRIu32,
                   ctx->rx_transactions, ctx->rx_length,
                   ctx->rx_length ? ctx->rx_buffer[0] : 0,
                   ctx->rx_length ? ctx->rx_buffer[ctx->rx_length - 1] : 0,
                   ctx->write_bytes);
          if (!ctx->response_queued && ctx->rx_length == sizeof(uint32_t) &&
              ctx->rx_buffer[0] == PAYLOAD_SIZE &&
              ctx->rx_buffer[1] == 0 && ctx->rx_buffer[2] == 0 &&
              ctx->rx_buffer[3] == 0)
            {
              fixture_queue_initial_response(ctx);
              ctx->response_queued = true;
            }
        }
    }
}

void app_main(void)
{
  static struct fixture_ctx_s ctx;
  static const i2c_slave_config_t config = {
    .i2c_port = 0,
    .scl_io_num = CONFIG_I2C_SLAVE_SCL_GPIO,
    .sda_io_num = CONFIG_I2C_SLAVE_SDA_GPIO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .send_buf_depth = 32,
    .receive_buf_depth = 16,
    .slave_addr = CONFIG_I2C_SLAVE_ADDRESS,
    .flags.enable_internal_pullup = true,
  };
  static const i2c_slave_event_callbacks_t callbacks = {
    .on_request = fixture_request_callback,
    .on_receive = fixture_rx_callback,
  };

  ctx.events = xQueueCreate(8, sizeof(enum fixture_event_e));
  ESP_ERROR_CHECK(ctx.events ? ESP_OK : ESP_ERR_NO_MEM);
  ESP_ERROR_CHECK(i2c_new_slave_device(&config, &ctx.handle));
  ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(ctx.handle, &callbacks,
                                                     &ctx));
  xTaskCreate(fixture_task, "i2c_protocol_fixture", 3072, &ctx, 10, NULL);

  ESP_LOGI(TAG, "I2C slave addr=0x%02x SCL=%d SDA=%d", CONFIG_I2C_SLAVE_ADDRESS,
           CONFIG_I2C_SLAVE_SCL_GPIO, CONFIG_I2C_SLAVE_SDA_GPIO);
}
