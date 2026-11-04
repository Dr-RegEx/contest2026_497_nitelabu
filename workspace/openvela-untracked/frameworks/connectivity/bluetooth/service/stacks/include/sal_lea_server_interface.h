/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SAL_LEA_SERVER_INTERFACE_H
#define SAL_LEA_SERVER_INTERFACE_H
#include "sal_lea_common.h"
#ifdef __cplusplus
extern "C" {
#endif
bt_status_t bt_sal_lea_server_start_announce(int8_t adv_id, uint8_t type,
    uint8_t* data, uint16_t size, uint8_t* metadata, uint16_t metadata_size);
bt_status_t bt_sal_lea_server_stop_announce(int8_t adv_id);
bt_status_t bt_sal_lea_server_request_disable(bt_address_t* addr, uint8_t ase_id);
#ifdef __cplusplus
}
#endif
#endif
