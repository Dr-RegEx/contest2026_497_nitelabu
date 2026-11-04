/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sal_lea_iso.h"
#include "sal_zblue_lea_iso.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Both framework and Host headers declare UUID initializer macros. */
#undef BT_UUID_DECLARE_16
#undef BT_UUID_DECLARE_32
#undef BT_UUID_DECLARE_128
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

NET_BUF_POOL_FIXED_DEFINE(lea_tx_pool, CONFIG_BT_ISO_TX_BUF_COUNT,
    BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
    CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

typedef struct {
    struct bt_bap_stream* stream;
    uint64_t generation;
    uint64_t next_packet;
    uint64_t next_submit;
    uint16_t handle;
    uint16_t max_sdu;
} lea_tx_binding_t;

typedef struct {
    lea_send_iso_data_t packet;
    struct net_buf* buffer;
    uint64_t generation;
    uint64_t number;
    uint16_t handle;
    uint16_t capacity;
} lea_tx_packet_t;

static pthread_mutex_t tx_lock = PTHREAD_MUTEX_INITIALIZER;
static lea_tx_binding_t tx_bindings[CONFIG_BT_ISO_MAX_CHAN];
static uint64_t tx_generation;

static lea_tx_binding_t* find_binding(uint16_t handle)
{
    for (size_t i = 0; i < CONFIG_BT_ISO_MAX_CHAN; i++) {
        if (tx_bindings[i].stream && tx_bindings[i].handle == handle)
            return &tx_bindings[i];
    }
    return NULL;
}

int sal_zblue_lea_tx_bind(struct bt_bap_stream* stream, uint16_t handle, uint16_t max_sdu)
{
    lea_tx_binding_t* available = NULL;
    int ret = 0;

    if (!stream || handle > 0x0eff || !max_sdu || max_sdu > CONFIG_BT_ISO_TX_MTU)
        return -EINVAL;

    pthread_mutex_lock(&tx_lock);
    for (size_t i = 0; i < CONFIG_BT_ISO_MAX_CHAN; i++) {
        lea_tx_binding_t* binding = &tx_bindings[i];
        if (binding->stream && (binding->stream == stream || binding->handle == handle)) {
            ret = -EEXIST;
            goto done;
        }
        if (!binding->stream && !available)
            available = binding;
    }
    if (!available) {
        ret = -ENOMEM;
        goto done;
    }
    if (tx_generation == UINT64_MAX) {
        ret = -EOVERFLOW;
        goto done;
    }
    *available = (lea_tx_binding_t) {
        .stream = stream,
        .handle = handle,
        .max_sdu = max_sdu,
        .generation = ++tx_generation,
    };

done:
    pthread_mutex_unlock(&tx_lock);
    return ret;
}

void sal_zblue_lea_tx_unbind(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&tx_lock);
    for (size_t i = 0; i < CONFIG_BT_ISO_MAX_CHAN; i++) {
        if (tx_bindings[i].stream == stream)
            memset(&tx_bindings[i], 0, sizeof(tx_bindings[i]));
    }
    pthread_mutex_unlock(&tx_lock);
}

void sal_zblue_lea_tx_reset(void)
{
    pthread_mutex_lock(&tx_lock);
    memset(tx_bindings, 0, sizeof(tx_bindings));
    /* Keep the generation counter across service restart. */
    pthread_mutex_unlock(&tx_lock);
}

lea_send_iso_data_t* bt_sal_lea_alloc_send_buffer(uint16_t size, uint16_t handle)
{
    lea_tx_packet_t* packet = NULL;
    lea_tx_binding_t* binding;
    uint64_t number;

    pthread_mutex_lock(&tx_lock);
    binding = find_binding(handle);
    if (!binding || !size || size > binding->max_sdu || binding->next_packet == UINT64_MAX)
        goto done;

    number = binding->next_packet++;
    packet = calloc(1, sizeof(*packet));
    if (!packet)
        goto done;

    packet->buffer = net_buf_alloc(&lea_tx_pool, K_NO_WAIT);
    if (!packet->buffer) {
        free(packet);
        packet = NULL;
        goto done;
    }
    net_buf_reserve(packet->buffer, BT_ISO_CHAN_SEND_RESERVE);
    packet->capacity = size;
    packet->handle = handle;
    packet->generation = binding->generation;
    packet->number = number;
    packet->packet.iso_handle = handle;
    packet->packet.sdu = packet->buffer->data;

done:
    pthread_mutex_unlock(&tx_lock);
    return packet ? &packet->packet : NULL;
}

void bt_sal_lea_free_send_buffer(lea_send_iso_data_t* packet)
{
    lea_tx_packet_t* tx = (lea_tx_packet_t*)packet;

    if (!tx)
        return;
    if (tx->buffer)
        net_buf_unref(tx->buffer);
    free(tx);
}

bt_status_t bt_sal_lea_send_iso_data(lea_send_iso_data_t* packet)
{
    lea_tx_packet_t* tx = (lea_tx_packet_t*)packet;
    lea_tx_binding_t* binding;
    bt_status_t status = BT_STATUS_NOT_READY;
    int ret;

    if (!tx)
        return BT_STATUS_PARM_INVALID;

    pthread_mutex_lock(&tx_lock);
    if (packet->sdu != tx->buffer->data || packet->iso_handle != tx->handle ||
        packet->sdu_length > tx->capacity) {
        status = BT_STATUS_PARM_INVALID;
        goto done;
    }
    binding = find_binding(tx->handle);
    if (!binding || binding->generation != tx->generation ||
        tx->number < binding->next_submit)
        goto done;

    binding->next_submit = tx->number + 1;
    net_buf_add(tx->buffer, packet->sdu_length);
    ret = bt_bap_stream_send(binding->stream, tx->buffer, (uint16_t)tx->number);
    if (ret >= 0) {
        tx->buffer = NULL; /* Host owns the reference, including queued data. */
        status = BT_STATUS_SUCCESS;
    } else if (ret == -ENOMEM || ret == -ENOBUFS) {
        status = BT_STATUS_NOMEM;
    } else if (ret == -EAGAIN || ret == -EBUSY) {
        status = BT_STATUS_BUSY;
    } else if (ret == -ENOTCONN || ret == -EBADMSG) {
        status = BT_STATUS_NOT_READY;
    } else {
        status = BT_STATUS_FAIL;
    }

done:
    pthread_mutex_unlock(&tx_lock);
    bt_sal_lea_free_send_buffer(packet);
    return status;
}

bt_status_t bt_sal_lea_send_sdu(uint16_t handle, const uint8_t* data, uint16_t size)
{
    lea_send_iso_data_t* packet;

    if (!data || !size || size > CONFIG_BT_ISO_TX_MTU)
        return BT_STATUS_PARM_INVALID;

    packet = bt_sal_lea_alloc_send_buffer(size, handle);
    if (!packet)
        return BT_STATUS_NO_RESOURCES;
    memcpy(packet->sdu, data, size);
    packet->sdu_length = size;
    return bt_sal_lea_send_iso_data(packet);
}
