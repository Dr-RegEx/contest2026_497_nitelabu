/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SAL_LEA_COMMON_H
#define SAL_LEA_COMMON_H
#include "sal_lea_iso.h"
#ifdef __cplusplus
extern "C" {
#endif
bt_status_t bt_sal_lea_init(void);
bt_status_t bt_sal_lea_cleanup(void);
bt_status_t bt_sal_lea_disconnect(bt_address_t* addr);
bool bt_sal_lea_is_source_stream(uint32_t stream_id);
#ifdef __cplusplus
}
#endif
#endif
