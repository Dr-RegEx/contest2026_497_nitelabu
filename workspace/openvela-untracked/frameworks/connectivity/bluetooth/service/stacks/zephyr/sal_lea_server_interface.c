/* SPDX-License-Identifier: Apache-2.0 */
#define LOG_TAG "sal_lea_server"
#include "sal_lea_server_interface.h"
#include "sal_zblue_lea_iso.h"
#include "lea_server_service.h"
#include "sal_zephyr_interface.h"
#include "utils/log.h"
#include <errno.h>
#include <pthread.h>
#include <string.h>

#undef BT_UUID_DECLARE_16
#undef BT_UUID_DECLARE_32
#undef BT_UUID_DECLARE_128
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/pacs.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>

#define LEA_STREAM_COUNT (CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT)
#define LEA_PAC_COUNT 8
#define LEA_ADV_FIELDS 16

/* This backend owns the unicast ASCS registration. Client and broadcast
 * service backends have separate lifecycle work and are not implemented here.
 */
typedef struct {
    struct bt_bap_stream bap;
    lea_audio_stream_t audio;
    uint8_t ase_id;
    bool allocated;
} lea_stream_t;

static pthread_mutex_t lea_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static lea_stream_t streams[LEA_STREAM_COUNT];
static uint32_t next_stream_id;
static bool active, ascs_registered, callbacks_registered, conn_registered;
static struct bt_audio_codec_cap codec_caps[LEA_PAC_COUNT];
static struct bt_pacs_cap pac_caps[LEA_PAC_COUNT];
static enum bt_audio_dir pac_dirs[LEA_PAC_COUNT];
static size_t registered_pacs;
static lea_pacs_info_t pacs_info;
static lea_ascs_info_t ascs_info;
static struct bt_le_ext_adv* announcement;
static int8_t announcement_id = -1;
static struct bt_bap_stream_ops stream_ops;
static const struct bt_bap_qos_cfg_pref qos_pref =
    BT_BAP_QOS_CFG_PREF(true, BT_GAP_LE_PHY_2M, 2, 40, 10000, 40000, 10000, 40000);

static bt_status_t status_from_errno(int err)
{
    if (!err) return BT_STATUS_SUCCESS;
    if (err == -ENOMEM || err == -ENOBUFS) return BT_STATUS_NOMEM;
    if (err == -EBUSY || err == -EALREADY) return BT_STATUS_BUSY;
    if (err == -EINVAL || err == -EMSGSIZE) return BT_STATUS_PARM_INVALID;
    if (err == -ENOTCONN) return BT_STATUS_NOT_READY;
    return BT_STATUS_FAIL;
}

static lea_stream_t* from_stream(struct bt_bap_stream* bap)
{
    for (size_t i = 0; i < LEA_STREAM_COUNT; i++)
        if (&streams[i].bap == bap) return &streams[i];
    return NULL;
}

static int convert_codec(const struct bt_audio_codec_cfg* cfg, enum bt_audio_dir dir,
    lea_codec_config_t* result, uint8_t* channel_count)
{
    enum bt_audio_location allocation;
    int frequency, duration, octets, blocks;
    unsigned channels;

    if (!cfg || cfg->id != BT_HCI_CODING_FORMAT_LC3 || cfg->cid || cfg->vid)
        return -EINVAL;
    frequency = bt_audio_codec_cfg_get_freq(cfg);
    duration = bt_audio_codec_cfg_get_frame_dur(cfg);
    octets = bt_audio_codec_cfg_get_octets_per_frame(cfg);
    blocks = bt_audio_codec_cfg_get_frame_blocks_per_sdu(cfg, true);
    if (frequency < 1 || frequency > 13 || duration < 0 || duration > 1 ||
        octets <= 0 || blocks <= 0 || blocks > 255 ||
        bt_audio_codec_cfg_get_chan_allocation(cfg, &allocation, true) < 0)
        return -EINVAL;
    channels = allocation ? __builtin_popcount(allocation) : 1;
    if (channels > 8) return -EINVAL;
    for (size_t i = 0; i < pacs_info.pac_number; i++) {
        const lea_pac_info_t* pac = &pacs_info.pac_list[i];
        const lea_codec_cap_t* cap = &pac->codec_pac;
        bool source = pac->pac_type == ADPT_LEA_PAC_TYPE_SOURCE_PAC;
        if (source != (dir == BT_AUDIO_DIR_SOURCE) ||
            !(cap->frequencies & (1u << (frequency - 1))) ||
            !(cap->durations & (1u << duration)) ||
            !(cap->channels & (1u << (channels - 1))) ||
            octets < cap->frame_octets_min || octets > cap->frame_octets_max ||
            blocks > cap->max_frames)
            continue;
        *result = (lea_codec_config_t) {
            .codec_id = { .format = cfg->id, .company_id = cfg->cid, .codec_id = cfg->vid },
            .mask = 0x1f, .frequency = frequency, .duration = duration,
            .allocation = allocation, .octets = octets, .blocks = blocks,
        };
        *channel_count = channels;
        return 0;
    }
    return -ENOTSUP;
}

static int reject(struct bt_bap_ascs_rsp* rsp, int err)
{
    *rsp = BT_BAP_ASCS_RSP(err == -ENOMEM ? BT_BAP_ASCS_RSP_CODE_NO_MEM :
        BT_BAP_ASCS_RSP_CODE_CONF_UNSUPPORTED, BT_BAP_ASCS_REASON_NONE);
    return err;
}

static int server_config(struct bt_conn* conn, const struct bt_bap_ep* ep,
    enum bt_audio_dir dir, const struct bt_audio_codec_cfg* cfg,
    struct bt_bap_stream** stream, struct bt_bap_qos_cfg_pref* pref,
    struct bt_bap_ascs_rsp* rsp)
{
    lea_codec_config_t codec;
    struct bt_bap_ep_info info;
    bt_address_t addr;
    uint8_t channels;
    lea_stream_t* slot = NULL;
    int err = -ENOMEM;

    pthread_mutex_lock(&lea_lock);
    if (!active || (dir != BT_AUDIO_DIR_SINK && dir != BT_AUDIO_DIR_SOURCE) ||
        get_le_addr_from_conn(conn, &addr) != BT_STATUS_SUCCESS ||
        bt_bap_ep_get_info(ep, &info) || convert_codec(cfg, dir, &codec, &channels)) {
        err = -EINVAL;
        goto done;
    }
    size_t first = dir == BT_AUDIO_DIR_SINK ? 0 : CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT;
    size_t count = dir == BT_AUDIO_DIR_SINK ? ascs_info.sink_ase_number : ascs_info.source_ase_number;
    for (size_t i = first; i < first + count; i++) {
        /* ASCS clears ep after released() returns; don't reuse it sooner. */
        if (!streams[i].allocated && !streams[i].bap.ep) { slot = &streams[i]; break; }
    }
    if (!slot || next_stream_id == UINT32_MAX) goto done;
    slot->audio = (lea_audio_stream_t) {
        .stream_id = ++next_stream_id, .is_source = dir == BT_AUDIO_DIR_SOURCE,
        .iso_handle = 0x100 + (slot - streams), .codec_cfg = codec,
        .channal_num = channels, .addr = addr,
    };
    slot->ase_id = info.id;
    slot->allocated = true;
    slot->bap.ops = &stream_ops;
    if (!lea_server_add_stream(slot->audio.stream_id, &addr)) {
        slot->allocated = false;
        goto done;
    }
    lea_server_on_connection_state_changed(&addr, PROFILE_STATE_CONNECTED);
    lea_server_on_stream_added(&addr, slot->audio.stream_id);
    *stream = &slot->bap;
    *pref = qos_pref;
    err = 0;
done:
    pthread_mutex_unlock(&lea_lock);
    return err ? reject(rsp, err) : 0;
}

static int server_reconfig(struct bt_bap_stream* stream, enum bt_audio_dir dir,
    const struct bt_audio_codec_cfg* cfg, struct bt_bap_qos_cfg_pref* pref,
    struct bt_bap_ascs_rsp* rsp)
{
    int err = -EINVAL;
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated && !slot->audio.started &&
        slot->audio.is_source == (dir == BT_AUDIO_DIR_SOURCE)) {
        err = convert_codec(cfg, dir, &slot->audio.codec_cfg, &slot->audio.channal_num);
        if (!err) *pref = qos_pref;
    }
    pthread_mutex_unlock(&lea_lock);
    return err ? reject(rsp, err) : 0;
}

static int server_qos(struct bt_bap_stream* stream, const struct bt_bap_qos_cfg* qos,
    struct bt_bap_ascs_rsp* rsp)
{
    int err = -EINVAL;
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated && qos) {
        lea_codec_config_t* codec = &slot->audio.codec_cfg;
        uint32_t size = codec->octets * slot->audio.channal_num * codec->blocks;
        uint32_t interval = (codec->duration ? 10000 : 7500) * codec->blocks;
        unsigned mtu = slot->audio.is_source ? CONFIG_BT_ISO_TX_MTU : CONFIG_BT_ISO_RX_MTU;
        if (qos->interval == interval && size <= qos->sdu && qos->sdu <= mtu) {
            slot->audio.sdu_size = size;
            slot->audio.max_sdu = qos->sdu;
            err = 0;
        }
    }
    pthread_mutex_unlock(&lea_lock);
    return err ? reject(rsp, err) : 0;
}

static int server_metadata(struct bt_bap_stream* stream, const uint8_t* meta, size_t size,
    struct bt_bap_ascs_rsp* rsp)
{
    int err = -EINVAL;
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (!active || !slot || !slot->allocated || (size && !meta)) goto done;
    for (size_t pos = 0; pos < size;) {
        uint8_t length = meta[pos++];
        if (!length || length > size - pos) goto done;
        if (meta[pos] == BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT) {
            if (length != 3) goto done;
            uint16_t context = sys_get_le16(&meta[pos + 1]);
            uint16_t available = slot->audio.is_source ? pacs_info.available_ctx.source : pacs_info.available_ctx.sink;
            if (!context || (context & ~available)) goto done;
        }
        pos += length;
    }
    err = 0;
done:
    pthread_mutex_unlock(&lea_lock);
    return err ? reject(rsp, err) : 0;
}

static int server_control(struct bt_bap_stream* stream, struct bt_bap_ascs_rsp* rsp)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    int err = active && slot && slot->allocated ? 0 : -EINVAL;
    pthread_mutex_unlock(&lea_lock);
    return err ? reject(rsp, err) : 0;
}

static void stream_state(struct bt_bap_stream* stream, uint8_t state)
{
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated)
        lea_server_on_ascs_event(&slot->audio.addr, slot->ase_id, state,
            slot->audio.is_source ? ADPT_LEA_PAC_TYPE_SOURCE_PAC : ADPT_LEA_PAC_TYPE_SINK_PAC);
}

static void stream_configured(struct bt_bap_stream* stream, const struct bt_bap_qos_cfg_pref* pref)
{
    (void)pref;
    pthread_mutex_lock(&lea_lock);
    stream_state(stream, ADPT_LEA_ASE_STATE_CODEC_CONFIG);
    pthread_mutex_unlock(&lea_lock);
}

static void stream_qos_set(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    stream_state(stream, ADPT_LEA_ASE_STATE_QOS_CONFIG);
    pthread_mutex_unlock(&lea_lock);
}

static void stream_enabled(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    stream_state(stream, ADPT_LEA_ASE_STATE_ENABLING);
    if (active && slot && slot->allocated && !slot->audio.is_source) {
        int err = bt_bap_stream_start(stream);
        if (err) BT_LOGE("sink start failed: %d", err);
    }
    pthread_mutex_unlock(&lea_lock);
}

static void stream_started(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated) {
        int err = slot->audio.is_source ? sal_zblue_lea_tx_bind(stream,
            slot->audio.iso_handle, slot->audio.max_sdu) : 0;
        if (err) {
            BT_LOGE("stream TX bind failed: %d", err);
            bt_bap_stream_disable(stream);
        } else {
            slot->audio.started = true;
            stream_state(stream, ADPT_LEA_ASE_STATE_STREAMING);
            lea_server_on_stream_started(&slot->audio);
        }
    }
    pthread_mutex_unlock(&lea_lock);
}

static void stream_stopped(struct bt_bap_stream* stream, uint8_t reason)
{
    (void)reason;
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    sal_zblue_lea_tx_unbind(stream);
    if (active && slot && slot->allocated) {
        slot->audio.started = false;
        lea_server_on_stream_stopped(slot->audio.stream_id);
    }
    pthread_mutex_unlock(&lea_lock);
}

static void stream_disabled(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    sal_zblue_lea_tx_unbind(stream);
    stream_state(stream, ADPT_LEA_ASE_STATE_DISABLING);
    pthread_mutex_unlock(&lea_lock);
}

static void stream_released(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    sal_zblue_lea_tx_unbind(stream);
    if (slot && slot->allocated) {
        if (active) lea_server_on_stream_removed(&slot->audio.addr, slot->audio.stream_id);
        slot->allocated = false;
        slot->audio.started = false;
    }
    pthread_mutex_unlock(&lea_lock);
}

static void stream_metadata_updated(struct bt_bap_stream* stream)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated) lea_server_on_metedata_updated(slot->audio.stream_id);
    pthread_mutex_unlock(&lea_lock);
}

static void stream_recv(struct bt_bap_stream* stream, const struct bt_iso_recv_info* info,
    struct net_buf* buffer)
{
    pthread_mutex_lock(&lea_lock);
    lea_stream_t* slot = from_stream(stream);
    if (active && slot && slot->allocated && slot->audio.started && info && buffer &&
        (info->flags & BT_ISO_FLAGS_VALID) && buffer->len <= slot->audio.max_sdu)
        lea_server_on_stream_recv(slot->audio.stream_id, info->ts, info->seq_num,
            buffer->data, buffer->len);
    /* The callback borrows the Host buffer. The framework copies the SDU. */
    pthread_mutex_unlock(&lea_lock);
}

static struct bt_bap_stream_ops stream_ops = {
    .configured = stream_configured, .qos_set = stream_qos_set, .enabled = stream_enabled,
    .started = stream_started, .stopped = stream_stopped, .disabled = stream_disabled,
    .released = stream_released, .metadata_updated = stream_metadata_updated, .recv = stream_recv,
};
static const struct bt_bap_unicast_server_cb server_ops = {
    .config = server_config, .reconfig = server_reconfig, .qos = server_qos,
    .enable = server_metadata, .metadata = server_metadata, .start = server_control,
    .disable = server_control, .stop = server_control, .release = server_control,
};

static void disconnected(struct bt_conn* conn, uint8_t reason)
{
    bt_address_t addr;
    (void)reason;
    pthread_mutex_lock(&lea_lock);
    if (active && get_le_addr_from_conn(conn, &addr) == BT_STATUS_SUCCESS)
        lea_server_on_connection_state_changed(&addr, PROFILE_STATE_DISCONNECTED);
    pthread_mutex_unlock(&lea_lock);
}
static struct bt_conn_cb conn_ops = { .disconnected = disconnected };

static int register_pac(const lea_pac_info_t* pac, size_t index)
{
    struct bt_audio_codec_cap* codec = &codec_caps[index];
    const lea_codec_cap_t* cap = &pac->codec_pac;
    struct bt_audio_codec_octets_per_codec_frame octets = {
        .min = cap->frame_octets_min, .max = cap->frame_octets_max,
    };
    uint16_t context = 0;
    if (pac->codec_id.format != ADPT_LEA_FORMAT_LC3 || pac->codec_id.company_id ||
        pac->codec_id.codec_id || (pac->md_number && !pac->md_value) ||
        (pac->pac_type != ADPT_LEA_PAC_TYPE_SOURCE_PAC && pac->pac_type != ADPT_LEA_PAC_TYPE_SINK_PAC))
        return -EINVAL;
    for (size_t i = 0; i < pac->md_number; i++) {
        if (pac->md_value[i].type != ADPT_LEA_METADATA_PREFERRED_AUDIO_CONTEXTS ||
            pac->md_value[i].preferred_contexts > UINT16_MAX) return -ENOTSUP;
        context |= pac->md_value[i].preferred_contexts;
    }
    memset(codec, 0, sizeof(*codec));
    codec->id = BT_HCI_CODING_FORMAT_LC3;
    codec->path_id = BT_ISO_DATA_PATH_HCI;
    if (bt_audio_codec_cap_set_freq(codec, cap->frequencies) < 0 ||
        bt_audio_codec_cap_set_frame_dur(codec, cap->durations) < 0 ||
        bt_audio_codec_cap_set_supported_audio_chan_counts(codec, cap->channels) < 0 ||
        bt_audio_codec_cap_set_octets_per_frame(codec, &octets) < 0 ||
        bt_audio_codec_cap_set_max_codec_frames_per_sdu(codec, cap->max_frames) < 0 ||
        bt_audio_codec_cap_meta_set_pref_context(codec, context) < 0) return -EINVAL;
    memset(&pac_caps[index], 0, sizeof(pac_caps[index]));
    pac_caps[index].codec_cap = codec;
    pac_dirs[index] = pac->pac_type == ADPT_LEA_PAC_TYPE_SOURCE_PAC ? BT_AUDIO_DIR_SOURCE : BT_AUDIO_DIR_SINK;
    return bt_pacs_cap_register(pac_dirs[index], &pac_caps[index]);
}

/* Caller holds lea_lock. Keep ownership flags if an unregister fails, so a
 * later cleanup can retry rather than reusing live registered storage.
 */
static int unregister_server(void)
{
    int err;
    active = false;
    sal_zblue_lea_tx_reset();
    if (callbacks_registered) {
        err = bt_bap_unicast_server_unregister_cb(&server_ops);
        if (err) return err;
        callbacks_registered = false;
    }
    if (ascs_registered) {
        /* An unavailable service must not continue advertising usable contexts. */
        if (ascs_info.sink_ase_number) {
            err = bt_pacs_set_available_contexts(BT_AUDIO_DIR_SINK, 0);
            if (err) return err;
        }
        if (ascs_info.source_ase_number) {
            err = bt_pacs_set_available_contexts(BT_AUDIO_DIR_SOURCE, 0);
            if (err) return err;
        }
        err = bt_bap_unicast_server_unregister();
        if (err) return err;
        ascs_registered = false;
    }
    while (registered_pacs) {
        size_t i = registered_pacs - 1;
        err = bt_pacs_cap_unregister(pac_dirs[i], &pac_caps[i]);
        if (err) return err;
        registered_pacs--;
    }
    if (conn_registered) {
        bt_conn_cb_unregister(&conn_ops);
        conn_registered = false;
    }
    return 0;
}

bt_status_t bt_sal_lea_init(void)
{
    int err = -EINVAL;
    pthread_mutex_lock(&lea_lock);
    if (active) { pthread_mutex_unlock(&lea_lock); return BT_STATUS_SUCCESS; }
    if (ascs_registered || callbacks_registered || registered_pacs || conn_registered) {
        pthread_mutex_unlock(&lea_lock); return BT_STATUS_BUSY;
    }
    memset(&pacs_info, 0, sizeof(pacs_info));
    memset(&ascs_info, 0, sizeof(ascs_info));
    if (!bt_is_ready() || !lea_server_on_pacs_info_request(&pacs_info) ||
        !lea_server_on_ascs_info_request(&ascs_info) || !pacs_info.pac_list ||
        !pacs_info.pac_number || pacs_info.pac_number > LEA_PAC_COUNT ||
        ascs_info.sink_ase_number > CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT ||
        ascs_info.source_ase_number > CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT) goto done;
    struct bt_bap_unicast_server_register_param param = {
        ascs_info.sink_ase_number, ascs_info.source_ase_number,
    };
    err = bt_bap_unicast_server_register(&param);
    if (err) goto done;
    ascs_registered = true;
    for (size_t i = 0; i < pacs_info.pac_number; i++) {
        err = register_pac(&pacs_info.pac_list[i], i);
        if (err) goto fail;
        registered_pacs++;
    }
    for (unsigned i = 0; i < 2; i++) {
        enum bt_audio_dir dir = i ? BT_AUDIO_DIR_SOURCE : BT_AUDIO_DIR_SINK;
        unsigned count = i ? ascs_info.source_ase_number : ascs_info.sink_ase_number;
        if (!count) continue;
        err = bt_pacs_set_location(dir, i ? pacs_info.source_location : pacs_info.sink_location);
        if (!err) err = bt_pacs_set_supported_contexts(dir, i ? pacs_info.supported_ctx.source : pacs_info.supported_ctx.sink);
        if (!err) err = bt_pacs_set_available_contexts(dir, i ? pacs_info.available_ctx.source : pacs_info.available_ctx.sink);
        if (err) goto fail;
    }
    err = bt_bap_unicast_server_register_cb(&server_ops);
    if (err) goto fail;
    callbacks_registered = true;
    bt_conn_cb_register(&conn_ops);
    conn_registered = true;
    active = true;
    lea_server_on_stack_state_changed(LEA_SERVER_STATE_ENABLED);
    goto done;
fail:
    if (unregister_server()) BT_LOGE("registration rollback requires retry");
done:
    pthread_mutex_unlock(&lea_lock);
    return status_from_errno(err);
}

bt_status_t bt_sal_lea_cleanup(void)
{
    pthread_mutex_lock(&lea_lock);
    for (size_t i = 0; i < LEA_STREAM_COUNT; i++) {
        if (streams[i].allocated || streams[i].bap.ep) {
            pthread_mutex_unlock(&lea_lock); return BT_STATUS_BUSY;
        }
    }
    if (announcement) {
        int err = bt_le_ext_adv_stop(announcement);
        if (!err) err = bt_le_ext_adv_delete(announcement);
        if (err) { pthread_mutex_unlock(&lea_lock); return status_from_errno(err); }
        announcement = NULL; announcement_id = -1;
    }
    int err = unregister_server();
    pthread_mutex_unlock(&lea_lock);
    return status_from_errno(err);
}

bool bt_sal_lea_is_source_stream(uint32_t id)
{
    bool source = false;
    pthread_mutex_lock(&lea_lock);
    for (size_t i = 0; i < LEA_STREAM_COUNT; i++)
        if (streams[i].allocated && streams[i].audio.stream_id == id) source = streams[i].audio.is_source;
    pthread_mutex_unlock(&lea_lock);
    return source;
}

struct disconnect_request {
    bt_address_t* addr;
    int result;
};

static void disconnect_matching(struct bt_conn* conn, void* data)
{
    struct disconnect_request* request = data;
    bt_address_t addr;
    if (request->result == -ENOTCONN &&
        get_le_addr_from_conn(conn, &addr) == BT_STATUS_SUCCESS &&
        !memcmp(&addr, request->addr, sizeof(addr)))
        request->result = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

bt_status_t bt_sal_lea_disconnect(bt_address_t* addr)
{
    if (!addr) return BT_STATUS_PARM_INVALID;
    struct disconnect_request request = { addr, -ENOTCONN };
    /* foreach retains each connection while the callback uses it. */
    bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_matching, &request);
    return status_from_errno(request.result);
}

bt_status_t bt_sal_lea_server_request_disable(bt_address_t* addr, uint8_t ase_id)
{
    int err = -ENOTCONN;
    if (!addr) return BT_STATUS_PARM_INVALID;
    pthread_mutex_lock(&lea_lock);
    for (size_t i = 0; i < LEA_STREAM_COUNT; i++) {
        if (streams[i].allocated && streams[i].ase_id == ase_id &&
            !memcmp(addr, &streams[i].audio.addr, sizeof(*addr))) {
            err = bt_bap_stream_disable(&streams[i].bap); break;
        }
    }
    pthread_mutex_unlock(&lea_lock);
    return status_from_errno(err);
}

bt_status_t bt_sal_lea_server_start_announce(int8_t id, uint8_t type,
    uint8_t* data, uint16_t size, uint8_t* metadata, uint16_t metadata_size)
{
    struct bt_data fields[LEA_ADV_FIELDS];
    uint8_t service_data[255];
    size_t count = 1;
    int err;
    if (id < 0 || id > 15 || type > 1 || (size && !data) ||
        (metadata_size && !metadata) || metadata_size > sizeof(service_data) - 8)
        return BT_STATUS_PARM_INVALID;
    for (size_t pos = 0; pos < size;) {
        uint8_t len = data[pos++];
        if (!len || len > size - pos || count == LEA_ADV_FIELDS) return BT_STATUS_PARM_INVALID;
        fields[count++] = (struct bt_data) { .type = data[pos], .data_len = len - 1, .data = &data[pos + 1] };
        pos += len;
    }
    pthread_mutex_lock(&lea_lock);
    if (!active || announcement) {
        pthread_mutex_unlock(&lea_lock); return active ? BT_STATUS_BUSY : BT_STATUS_NOT_READY;
    }
    sys_put_le16(BT_UUID_ASCS_VAL, service_data);
    service_data[2] = type;
    sys_put_le16(pacs_info.available_ctx.sink, &service_data[3]);
    sys_put_le16(pacs_info.available_ctx.source, &service_data[5]);
    service_data[7] = metadata_size;
    if (metadata_size) memcpy(service_data + 8, metadata, metadata_size);
    fields[0] = (struct bt_data) { .type = BT_DATA_SVC_DATA16, .data_len = 8 + metadata_size, .data = service_data };
    struct bt_le_adv_param param = {
        .id = BT_ID_DEFAULT, .sid = id, .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONN,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2, .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    };
    err = bt_le_ext_adv_create(&param, NULL, &announcement);
    if (!err) {
        announcement_id = id;
        err = bt_le_ext_adv_set_data(announcement, fields, count, NULL, 0);
        if (!err) err = bt_le_ext_adv_start(announcement, BT_LE_EXT_ADV_START_DEFAULT);
        if (err && !bt_le_ext_adv_delete(announcement)) { announcement = NULL; announcement_id = -1; }
    }
    pthread_mutex_unlock(&lea_lock);
    return status_from_errno(err);
}

bt_status_t bt_sal_lea_server_stop_announce(int8_t id)
{
    int err;
    pthread_mutex_lock(&lea_lock);
    if (!announcement || announcement_id != id) {
        pthread_mutex_unlock(&lea_lock); return BT_STATUS_NOT_READY;
    }
    err = bt_le_ext_adv_stop(announcement);
    if (!err) err = bt_le_ext_adv_delete(announcement);
    if (!err) { announcement = NULL; announcement_id = -1; }
    pthread_mutex_unlock(&lea_lock);
    return status_from_errno(err);
}
