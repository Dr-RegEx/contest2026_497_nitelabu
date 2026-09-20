/* Generic I2C echo fixture: S31 master, C6 slave. */
#include <string.h>
#include "driver/i2c_slave.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
typedef struct { bool read; size_t len; uint8_t data[64]; } event_t;
static QueueHandle_t queue;
static volatile unsigned dropped;
static bool IRAM_ATTR receive_cb(i2c_slave_dev_handle_t h, const i2c_slave_rx_done_event_data_t *e, void *arg)
{
  event_t ev = {.len=e->length}; BaseType_t wake=pdFALSE;
  if (ev.len > sizeof(ev.data)) { dropped++; return false; }
  memcpy(ev.data,e->buffer,ev.len);
  if(xQueueSendFromISR(queue,&ev,&wake)!=pdTRUE) dropped++;
  return wake==pdTRUE;
}
static bool IRAM_ATTR request_cb(i2c_slave_dev_handle_t h, const i2c_slave_request_event_data_t *e, void *arg)
{
  event_t ev={.read=true}; BaseType_t wake=pdFALSE;
  if(xQueueSendFromISR(queue,&ev,&wake)!=pdTRUE) dropped++;
  return wake==pdTRUE;
}
void app_main(void)
{
  i2c_slave_dev_handle_t h;
  i2c_slave_config_t cfg={.i2c_port=0,.scl_io_num=CONFIG_I2C_SLAVE_SCL_GPIO,
    .sda_io_num=CONFIG_I2C_SLAVE_SDA_GPIO,.clk_source=I2C_CLK_SRC_DEFAULT,
    .send_buf_depth=256,.receive_buf_depth=64,.slave_addr=CONFIG_I2C_SLAVE_ADDRESS,
    .flags.enable_internal_pullup=true};
  queue=xQueueCreate(16,sizeof(event_t)); assert(queue);
  ESP_ERROR_CHECK(i2c_new_slave_device(&cfg,&h));
  i2c_slave_event_callbacks_t cb={.on_receive=receive_cb,.on_request=request_cb};
  ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(h,&cb,NULL));
  uint8_t data[64]={0xa0,0xa1}; size_t len=2; event_t ev;
  ESP_LOGI("fixture","READY addr=0x%02x SCL=%d SDA=%d",cfg.slave_addr,cfg.scl_io_num,cfg.sda_io_num);
  while(xQueueReceive(queue,&ev,portMAX_DELAY)==pdTRUE) {
    if(!ev.read) { len=ev.len; memcpy(data,ev.data,len);
      ESP_LOGI("fixture","RX len=%u dropped=%u",(unsigned)len,dropped);
      ESP_LOG_BUFFER_HEX("fixture",data,len);
    } else {
      uint32_t sent=0;
      ESP_ERROR_CHECK(i2c_slave_write(h,data,len,&sent,1000));
      ESP_ERROR_CHECK(sent==len?ESP_OK:ESP_FAIL);
      ESP_LOGI("fixture","TX len=%u dropped=%u",(unsigned)sent,dropped);
    }
  }
}
