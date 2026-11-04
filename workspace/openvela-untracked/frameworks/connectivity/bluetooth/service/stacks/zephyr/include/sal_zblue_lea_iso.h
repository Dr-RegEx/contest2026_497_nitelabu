/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SAL_ZBLUE_LEA_ISO_H
#define SAL_ZBLUE_LEA_ISO_H

#include <stdint.h>
struct bt_bap_stream;

#ifdef __cplusplus
extern "C" {
#endif

/* Bind on stream start and unbind on stop/release. BAP stream storage must
 * remain valid until unbind returns. Each binding has a fresh generation:
 * outstanding packets cannot be sent after stop, even if the handle is reused.
 * Unbind/reset waits for a concurrent Host submission, not controller drain.
 * The profile still owns BAP stop/release and controller-queued packets.
 */
int sal_zblue_lea_tx_bind(struct bt_bap_stream* stream, uint16_t handle, uint16_t max_sdu);
void sal_zblue_lea_tx_unbind(struct bt_bap_stream* stream);
void sal_zblue_lea_tx_reset(void);

#ifdef __cplusplus
}
#endif
#endif
