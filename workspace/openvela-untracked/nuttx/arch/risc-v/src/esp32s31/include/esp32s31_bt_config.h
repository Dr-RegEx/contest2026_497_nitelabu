/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_BT_CONFIG_H
#define __ARCH_RISCV_SRC_ESP32S31_BT_CONFIG_H

/* Pinned controller ABI configuration: BLE-only, awake, VHCI, no IDF host.
 * Numeric defaults follow porting_btdm Kconfig; these are not FreeRTOS APIs.
 */

#define CONFIG_BT_CONTROLLER_ENABLED 1
#define CONFIG_BTDM_CTRL_MODE_BLE_ONLY 1
#define CONFIG_BT_CTRL_BLE_ENABLE 1
#define CONFIG_BT_DUAL_MODE_ARCH 1
#define CONFIG_BT_CTRL_HCI_INTERFACE_USE_RAM 1
#define CONFIG_BT_CTRL_TASK_STACK_SIZE 4096
#define CONFIG_BT_CTRL_PINNED_TO_CORE 0
#define CONFIG_BT_CTRL_HCI_CMD_NUM 5
#define CONFIG_BT_CTRL_LP_CLK_SRC_MAIN_XTAL 1
/* Match the host advertising capacity for the original two-advertiser cases. */
#ifdef CONFIG_BT_EXT_ADV_MAX_ADV_SET
#  define CONFIG_BT_LE_MAX_EXT_ADV_INSTANCES CONFIG_BT_EXT_ADV_MAX_ADV_SET
#endif
#define CONFIG_BT_LE_LL_RESOLV_LIST_SIZE 4
#define CONFIG_BT_LE_LL_DUP_SCAN_LIST_COUNT 20
#define CONFIG_BT_LE_LL_SCA 60
#define CONFIG_BT_LE_COEX_PHY_CODED_TX_RX_TLIM_EFF 0
#define CONFIG_BT_LE_MSYS_1_BLOCK_COUNT 12
#define CONFIG_BT_LE_MSYS_1_BLOCK_SIZE 256
#define CONFIG_BT_LE_MSYS_2_BLOCK_COUNT 24
#define CONFIG_BT_LE_MSYS_2_BLOCK_SIZE 320
#define CONFIG_BT_LE_MSYS_BUF_FROM_HEAP 1
#define CONFIG_BT_LE_MSYS_INIT_IN_CONTROLLER 1
#define CONFIG_BT_LE_SM_SC 1
#define CONFIG_BT_LE_USE_ESP_TIMER 1

/* Enable the controller ISO stack only for an ISO-enabled NuttX host.
 * The pinned controller allows at most two groups and six streams of each
 * kind. Use standard HCI completed-packet flow control: IDF's optional NSFC
 * requires a private host API which ZBlue does not call.
 */

#ifdef CONFIG_BT_ISO
#  define CONFIG_BT_LE_ISO_SUPPORT 1
#  define CONFIG_BT_LE_ISO_BUF_COUNT 12
#  define CONFIG_BT_LE_ISO_BUF_SIZE 251
#  if CONFIG_BT_ISO_MAX_CHAN < 1 || CONFIG_BT_ISO_MAX_CHAN > 6
#    error "S31 controller ISO channel capacity must be between 1 and 6"
#  endif
#  define CONFIG_BT_LE_ISO_BIS CONFIG_BT_ISO_MAX_CHAN
#  define CONFIG_BT_LE_ISO_CIS CONFIG_BT_ISO_MAX_CHAN
#  define CONFIG_BT_LE_ISO_BIS_PER_BIG CONFIG_BT_ISO_MAX_CHAN
#  define CONFIG_BT_LE_ISO_CIS_PER_CIG CONFIG_BT_ISO_MAX_CHAN
#  ifdef CONFIG_BT_ISO_MAX_BIG
#    define CONFIG_BT_LE_ISO_BIG CONFIG_BT_ISO_MAX_BIG
#  else
#    define CONFIG_BT_LE_ISO_BIG 1
#  endif
#  ifdef CONFIG_BT_ISO_MAX_CIG
#    define CONFIG_BT_LE_ISO_CIG CONFIG_BT_ISO_MAX_CIG
#  else
#    define CONFIG_BT_LE_ISO_CIG 1
#  endif
#  if CONFIG_BT_LE_ISO_BIG < 1 || CONFIG_BT_LE_ISO_BIG > 2 || \
      CONFIG_BT_LE_ISO_CIG < 1 || CONFIG_BT_LE_ISO_CIG > 2
#    error "S31 controller ISO group capacity must be between 1 and 2"
#  endif
#endif

#endif
