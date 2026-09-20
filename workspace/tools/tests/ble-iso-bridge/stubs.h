/* Host-only API boundary stubs. Test the production bridge, not controller RF. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#define OK 0
#define ESP_OK 0
#define ESP_MAC_BT 0
#define ESP_BT_MODE_BLE 0
#define ESP_BT_CONTROLLER_STATUS_ENABLED 1
#define H4_CMD 1
#define H4_ACL 2
#define H4_EVT 4
#define H4_ISO 5
#define H4_HEADER_SIZE 1
#define HCI_DRIVER_DIR_H2C 0
typedef int mutex_t;
#define NXMUTEX_INITIALIZER 0
static int nxmutex_lock(mutex_t *m) { return 0; }
static int nxmutex_unlock(mutex_t *m) { return 0; }
static void *kmm_malloc(size_t n) { return malloc(n); }
static void kmm_free(void *p) { free(p); }
enum bt_buf_type_e { BT_CMD, BT_EVT, BT_ACL_OUT, BT_ACL_IN, BT_ISO_OUT, BT_ISO_IN };
struct bt_driver_s {
  int head_reserve;
  int (*open)(struct bt_driver_s *);
  int (*send)(struct bt_driver_s *, enum bt_buf_type_e, void *, size_t);
  void (*close)(struct bt_driver_s *);
  int (*receive)(struct bt_driver_s *, enum bt_buf_type_e, void *, size_t);
};
typedef struct { int (*notify_host_recv)(uint8_t *, uint16_t); } esp_vhci_host_callback_t;
typedef struct { struct { int task_prio; } btdm; struct { int controller_task_prio; } ble; } esp_bt_controller_config_t;
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() { { 0 }, { 0 } }
static void esp_fill_random(void *p, size_t n) { memset(p, 0, n); }
static int esp_read_mac(uint8_t *p, int type) { memset(p, 0, 6); return 0; }
static void uECC_set_rng(int (*fn)(uint8_t *, unsigned int)) { }
static int esp_bt_controller_init(esp_bt_controller_config_t *p) { return 0; }
static int esp_bt_controller_enable(int mode) { return 0; }
static int esp_bt_controller_disable(void) { return 0; }
static int esp_bt_controller_deinit(void) { return 0; }
static int esp_bt_controller_get_status(void) { return 1; }
static int esp_vhci_host_register_callback(const esp_vhci_host_callback_t *cb) { return 0; }
static int uart_bth4_register(const char *p, struct bt_driver_s *d) { return 0; }
static int esp32s31_ble_dispatch_init(void) { return 0; }
static int esp32s31_ble_dispatch(int (*fn)(void *), void *arg) { return fn(arg); }
static uint8_t captured[17000];
static size_t captured_len;
static int captured_type, tx_result, rx_result, tx_calls, rx_calls;
static int host_tx(int type, uint8_t *data, uint32_t len, int dir)
{
  assert(dir == HCI_DRIVER_DIR_H2C);
  assert(len <= sizeof(captured));
  captured_type = type; captured_len = len;
  memcpy(captured, data, len); tx_calls++;
  return tx_result;
}
static struct { int (*hci_driver_tx)(int, uint8_t *, uint32_t, int); }
  hci_driver_vhci_ops = { host_tx };
static int host_rx(struct bt_driver_s *d, enum bt_buf_type_e type, void *data, size_t len)
{
  assert(len <= sizeof(captured));
  captured_type = type; captured_len = len;
  memcpy(captured, data, len); rx_calls++;
  return rx_result;
}

#define CONFIG_BT_ISO 1
#define BTDM_OSAL_MALLOC_F_INTERNAL 0
static void *btdm_osal_malloc(size_t n, int flags) { (void)flags; return malloc(n); }
static void btdm_osal_free(void *p) { free(p); }
static int ble_hci_trans_hs_iso_tx(uint8_t *p, uint16_t len, void *arg)
{
  (void)arg; captured_type=H4_ISO; captured_len=len;
  memcpy(captured,p,len); tx_calls++;
  /* Locked controller owns the raw buffer on both success and rejection. */
  free(p);
  return tx_result;
}
