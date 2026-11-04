/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SAL_LEA_ISO_H
#define SAL_LEA_ISO_H

#include "lea_audio_common.h"
#include "bt_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocation is nonblocking. The caller owns the packet until send/free.
 * Send consumes every non-NULL packet on both success and failure. A successful
 * return means acceptance by the Host queue, not delivery over the radio.
 * Call allocation once per SDU interval; failed allocations also advance the
 * stream sequence. Send allocated packets in order; stale packets are dropped.
 */
lea_send_iso_data_t* bt_sal_lea_alloc_send_buffer(uint16_t size, uint16_t handle);
void bt_sal_lea_free_send_buffer(lea_send_iso_data_t* packet);
bt_status_t bt_sal_lea_send_iso_data(lea_send_iso_data_t* packet);
bt_status_t bt_sal_lea_send_sdu(uint16_t handle, const uint8_t* data, uint16_t size);

#ifdef __cplusplus
}
#endif
#endif
