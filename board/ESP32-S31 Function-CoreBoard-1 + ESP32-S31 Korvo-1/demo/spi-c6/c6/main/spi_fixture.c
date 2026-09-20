/* Full-duplex SPI: each 16-byte transaction returns the preceding RX data. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/spi_slave.h"
#include "esp_log.h"
#include "esp_check.h"
void app_main(void)
{
  spi_bus_config_t bus={.mosi_io_num=5,.miso_io_num=6,.sclk_io_num=4,
    .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=16};
  spi_slave_interface_config_t cfg={.spics_io_num=7,.mode=0,.queue_size=1};
  ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST,&bus,&cfg,SPI_DMA_DISABLED));
  uint8_t tx[16],rx[16];
  for(int i=0;i<16;i++) tx[i]=0xa0+i;
  ESP_LOGI("spi_fixture","READY mode=0 CLK=4 MOSI=5 MISO=6 CS=7; 16-byte frames");
  unsigned count=0;
  for (;;) {
    memset(rx,0,sizeof(rx));
    spi_slave_transaction_t t={.length=128,.tx_buffer=tx,.rx_buffer=rx};
    ESP_ERROR_CHECK(spi_slave_transmit(SPI2_HOST,&t,portMAX_DELAY));
    ESP_LOGI("spi_fixture","RX txn=%u bits=%u",++count,(unsigned)t.trans_len);
    ESP_LOG_BUFFER_HEX("spi_fixture",rx,sizeof(rx));
    if(t.trans_len==128) memcpy(tx,rx,sizeof(tx));
    else ESP_LOGE("spi_fixture","Invalid frame length");
  }
}
