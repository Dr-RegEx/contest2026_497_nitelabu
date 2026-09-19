/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/serial/uart_bth4.h>
#include <nuttx/wireless/bluetooth/bt_driver.h>
#include <nuttx/wireless/bluetooth/bt_uart.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#ifdef CONFIG_BUILD_KERNEL
#  include "esp32s31_ble_dispatch.h"
#endif

#include "esp_bt.h"
#include "esp_hci_driver.h"
#ifdef CONFIG_BT_ISO
#  include "esp_hci_internal.h"
#  include "btdm_osal.h"
#endif
#include "esp_random.h"
#include "esp_mac.h"
#include "tinycrypt/ecc.h"

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static bool g_open;
static bool g_registered;
static struct bt_driver_s g_driver;

/* ISO_Data_Load_Length occupies 14 bits; the HCI ISO header is four bytes.
 * The H4 packet indicator is not included in the NuttX send length.
 */

#define S31_ISO_HEADER_SIZE 4
#define S31_ISO_LENGTH_MASK 0x3fff
#define S31_ISO_PACKET_MAX (S31_ISO_HEADER_SIZE + S31_ISO_LENGTH_MASK)

static int s31_random(uint8_t *dest, unsigned int size)
{
  esp_fill_random(dest, size);
  return 1;
}

static int s31_receive(uint8_t *data, uint16_t len)
{
  enum bt_buf_type_e type;
  int ret;

  if (data == NULL || len < 1 || g_driver.receive == NULL)
    {
      return -EINVAL;
    }

  if (data[0] == H4_EVT)
    {
      type = BT_EVT;
    }
  else if (data[0] == H4_ACL)
    {
      type = BT_ACL_IN;
    }
  else if (data[0] == H4_ISO)
    {
      if (len < H4_HEADER_SIZE + S31_ISO_HEADER_SIZE ||
          (data[4] & 0xc0) != 0 ||
          len != H4_HEADER_SIZE + S31_ISO_HEADER_SIZE +
                 (data[3] | data[4] << 8))
        {
          return -EINVAL;
        }

      type = BT_ISO_IN;
    }
  else
    {
      return -ENOTSUP;
    }

  if (type == BT_EVT && len >= 7 && data[1] == 0x0e)
    {
      syslog(LOG_INFO, "S31 BLE: complete opcode=%04x status=%u len=%u\n",
             data[4] | data[5] << 8, data[6], len);
    }

  if (type == BT_EVT && len >= 7 && data[1] == 0x05)
    {
      syslog(LOG_INFO,
             "S31 BLE: disconnect status=%02x handle=%04x reason=%02x\n",
             data[3], data[4] | data[5] << 8, data[6]);
    }

  ret = g_driver.receive(&g_driver, type, data + 1, len - 1);
  return ret < 0 ? ret : 0;
}

static const esp_vhci_host_callback_t g_callbacks =
{
  .notify_host_recv = s31_receive
};

static int s31_open_core(struct bt_driver_s *dev)
{
  esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  uint8_t mac[6];
  int ret;

  nxmutex_lock(&g_lock);
  if (g_open)
    {
      nxmutex_unlock(&g_lock);
      return -EBUSY;
    }

  /* The IDF ABI uses priorities 0..24. HAL platform/os.h has native NuttX
   * priorities; the OSAL performs the translation, so supply the IDF value.
   */

  syslog(LOG_INFO, "S31 BLE: controller open\n");
  ret = esp_read_mac(mac, ESP_MAC_BT);
  if (ret != ESP_OK)
    {
      syslog(LOG_ERR, "S31 BLE: eFuse Bluetooth MAC failed: %d\n", ret);
      nxmutex_unlock(&g_lock);
      return -EIO;
    }

  syslog(LOG_INFO, "S31 BLE: public MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  config.btdm.task_prio = 23;
  config.ble.controller_task_prio = 23;
  uECC_set_rng(s31_random);
  ret = esp_bt_controller_init(&config);
  if (ret != ESP_OK)
    {
      syslog(LOG_ERR, "BLE controller init failed: %d\n", ret);
      nxmutex_unlock(&g_lock);
      return -EIO;
    }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret == ESP_OK)
    {
      ret = esp_vhci_host_register_callback(&g_callbacks);
    }

  if (ret != ESP_OK)
    {
      if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
        {
          esp_bt_controller_disable();
        }

      esp_bt_controller_deinit();
      nxmutex_unlock(&g_lock);
      return -EIO;
    }

  syslog(LOG_INFO, "S31 BLE: controller ready, VHCI registered\n");
  g_open = true;
  nxmutex_unlock(&g_lock);
  return OK;
}

static int s31_send_core(struct bt_driver_s *dev, enum bt_buf_type_e type,
                    void *data, size_t len)
{
  const uint8_t *payload = data;
  uint8_t *packet;
  uint8_t h4;
  int ret;

  if (data == NULL)
    {
      return -EINVAL;
    }

  if (type == BT_CMD && len >= 3 && len == (size_t)payload[2] + 3)
    {
      h4 = H4_CMD;
    }
  else if (type == BT_ACL_OUT && len >= 4 && len <= 259 &&
           len == (size_t)(payload[2] | payload[3] << 8) + 4)
    {
      h4 = H4_ACL;
    }
  else if (type == BT_ISO_OUT && len >= S31_ISO_HEADER_SIZE &&
           len <= S31_ISO_PACKET_MAX && (payload[3] & 0xc0) == 0 &&
           len == (size_t)(payload[2] | payload[3] << 8) +
                  S31_ISO_HEADER_SIZE)
    {
      h4 = H4_ISO;
    }
  else
    {
      return -EINVAL;
    }

#ifdef CONFIG_BT_ISO
  if (type == BT_ISO_OUT)
    {
      /* The locked controller takes ownership of a raw ISO header+payload,
       * including on rejection.  VHCI's generic H4 path neither strips its
       * indicator nor copies this packet.  Use the controller OS allocator
       * paired with ext_funcs_ro._free and transfer it exactly once.
       */

      packet = btdm_osal_malloc(len, BTDM_OSAL_MALLOC_F_INTERNAL);
      if (packet == NULL)
        {
          return -ENOMEM;
        }

      memcpy(packet, data, len);
      nxmutex_lock(&g_lock);
      if (!g_open)
        {
          btdm_osal_free(packet);
          ret = -ENODEV;
        }
      else
        {
          ret = ble_hci_trans_hs_iso_tx(packet, len, NULL);
          ret = ret == 0 ? (int)len : (ret < 0 ? ret : -EIO);
        }

      nxmutex_unlock(&g_lock);
      return ret;
    }
#else
  if (type == BT_ISO_OUT)
    {
      return -ENOTSUP;
    }
#endif

  packet = kmm_malloc(len + 1);
  if (packet == NULL)
    {
      return -ENOMEM;
    }

  if (type == BT_CMD)
    {
      syslog(LOG_INFO, "S31 BLE: TX opcode=%04x len=%u\n",
             payload[0] | payload[1] << 8, (unsigned int)len);
    }

  packet[0] = h4;
  memcpy(packet + 1, data, len);
  nxmutex_lock(&g_lock);
  if (!g_open)
    {
      ret = -ENODEV;
    }
  else
    {
      /* The public void VHCI send API discards transport errors. Use the
       * same pinned transport entry directly and preserve its result.
       */

      ret = hci_driver_vhci_ops.hci_driver_tx(h4, packet, len + 1,
                                             HCI_DRIVER_DIR_H2C);
      ret = ret == 0 ? (int)len : (ret < 0 ? ret : -EIO);
    }

  if (type == BT_CMD)
    {
      syslog(LOG_INFO, "S31 BLE: TX opcode=%04x result=%d\n",
             payload[0] | payload[1] << 8, ret);
    }

  nxmutex_unlock(&g_lock);
  kmm_free(packet);
  return ret;
}

static void s31_close_core(struct bt_driver_s *dev)
{
  int ret;

  nxmutex_lock(&g_lock);
  if (g_open)
    {
      ret = esp_bt_controller_disable();
      if (ret == ESP_OK)
        {
          ret = esp_bt_controller_deinit();
        }

      if (ret != ESP_OK)
        {
          syslog(LOG_ERR, "BLE controller close failed: %d\n", ret);
        }

      g_open = false;
    }

  nxmutex_unlock(&g_lock);
}

#ifdef CONFIG_BUILD_KERNEL
struct s31_send_request_s
{
  struct bt_driver_s *dev;
  enum bt_buf_type_e type;
  size_t length;
  uint8_t data[];
};

static int s31_open_request(void *arg)
{
  return s31_open_core(arg);
}

static int s31_close_request(void *arg)
{
  s31_close_core(arg);
  return 0;
}

static int s31_send_request(void *arg)
{
  struct s31_send_request_s *request = arg;
  return s31_send_core(request->dev, request->type,
                       request->data, request->length);
}

static int s31_open(struct bt_driver_s *dev)
{
  return esp32s31_ble_dispatch(s31_open_request, dev);
}

static void s31_close(struct bt_driver_s *dev)
{
  int ret = esp32s31_ble_dispatch(s31_close_request, dev);
  if (ret < 0)
    {
      syslog(LOG_ERR, "S31 BLE: close dispatch failed: %d\n", ret);
    }
}

static int s31_send(struct bt_driver_s *dev, enum bt_buf_type_e type,
                    void *data, size_t len)
{
  struct s31_send_request_s *request;
  int ret;

  if (data == NULL ||
      len > (type == BT_ISO_OUT ? S31_ISO_PACKET_MAX : 259))
    {
      return -EINVAL;
    }

  /* The worker cannot dereference an application's address environment. */

  request = kmm_malloc(sizeof(*request) + len);
  if (request == NULL)
    {
      return -ENOMEM;
    }

  request->dev = dev;
  request->type = type;
  request->length = len;
  memcpy(request->data, data, len);
  ret = esp32s31_ble_dispatch(s31_send_request, request);
  kmm_free(request);
  return ret;
}
#else
#  define s31_open  s31_open_core
#  define s31_send  s31_send_core
#  define s31_close s31_close_core
#endif

int esp32s31_ble_initialize(void)
{
  int ret;

  if (g_registered)
    {
      return OK;
    }

#ifdef CONFIG_BUILD_KERNEL
  ret = esp32s31_ble_dispatch_init();
  if (ret < 0)
    {
      return ret;
    }
#endif

  g_driver.head_reserve = H4_HEADER_SIZE;
  g_driver.open = s31_open;
  g_driver.send = s31_send;
  g_driver.close = s31_close;
  ret = uart_bth4_register("/dev/ttyHCI0", &g_driver);
  if (ret == OK)
    {
      g_registered = true;
    }

  return ret;
}
