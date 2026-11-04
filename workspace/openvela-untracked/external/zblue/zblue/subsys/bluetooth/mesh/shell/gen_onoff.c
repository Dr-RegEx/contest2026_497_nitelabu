/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/shell.h>
#include <zephyr/bluetooth/mesh/shell_onoff.h>
#include "mesh/msg.h"
#include "utils.h"

#define OP_GET BT_MESH_MODEL_OP_2(0x82, 0x01)
#define OP_SET BT_MESH_MODEL_OP_2(0x82, 0x02)
#define OP_STATUS BT_MESH_MODEL_OP_2(0x82, 0x04)

/* One command model instance, serialized by the interactive shell. */
static const struct bt_mesh_model *client;
static struct bt_mesh_msg_ack_ctx ack;
static uint8_t transaction_id;

struct onoff_status {
	uint16_t app_idx;
	uint8_t present;
	uint8_t target;
	uint8_t remaining;
};

static int onoff_status(const struct bt_mesh_model *model,
			struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf)
{
	struct onoff_status *response;
	uint8_t present, target, remaining;

	ARG_UNUSED(model);
	if (buf->len != 1 && buf->len != 3) {
		return -EMSGSIZE;
	}

	present = buf->data[0];
	target = buf->len == 3 ? buf->data[1] : present;
	remaining = buf->len == 3 ? buf->data[2] : 0;
	if (present > 1 || target > 1) {
		return -EINVAL;
	}

	if (bt_mesh_msg_ack_ctx_match(&ack, OP_STATUS, ctx->addr,
				     (void **)&response) && response->app_idx == ctx->app_idx) {
		response->present = present;
		response->target = target;
		response->remaining = remaining;
		bt_mesh_msg_ack_ctx_rx(&ack);
	}

	return 0;
}

const struct bt_mesh_model_op bt_mesh_shell_onoff_ops[] = {
	{ OP_STATUS, BT_MESH_LEN_MIN(1), onoff_status },
	BT_MESH_MODEL_OP_END,
};

static int onoff_init(const struct bt_mesh_model *model)
{
	if (client && client != model) {
		return -EALREADY;
	}
	client = model;
	bt_mesh_msg_ack_ctx_init(&ack);
	return 0;
}

static void onoff_reset(const struct bt_mesh_model *model)
{
	ARG_UNUSED(model);
	bt_mesh_msg_ack_ctx_reset(&ack);
	transaction_id = 0;
}

const struct bt_mesh_model_cb bt_mesh_shell_onoff_cb = {
	.init = onoff_init,
	.reset = onoff_reset,
};

static int request(const struct shell *sh, bool set, uint8_t value)
{
	struct bt_mesh_msg_ctx ctx = BT_MESH_MSG_CTX_INIT_APP(
		bt_mesh_shell_target_ctx.app_idx, bt_mesh_shell_target_ctx.dst);
	struct onoff_status status = { .app_idx = ctx.app_idx };
	struct bt_mesh_msg_rsp_ctx response = {
		.ack = &ack, .op = OP_STATUS, .user_data = &status, .timeout = 5000,
	};
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_SET, 2);
	int err;

	if (!client || !BT_MESH_ADDR_IS_UNICAST(ctx.addr) || value > 1) {
		return -EINVAL;
	}
	bt_mesh_model_msg_init(&msg, set ? OP_SET : OP_GET);
	if (set) {
		net_buf_simple_add_u8(&msg, value);
		net_buf_simple_add_u8(&msg, transaction_id++);
	}

	err = bt_mesh_msg_ackd_send(client, &ctx, &msg, &response);
	if (err) {
		shell_error(sh, "OnOff request failed (err %d)", err);
		return err;
	}
	shell_print(sh, "OnOff status src=0x%04x present=%u target=%u remaining=0x%02x",
		    ctx.addr, status.present, status.target, status.remaining);
	return 0;
}

static int cmd_get(const struct shell *sh, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return request(sh, false, 0);
}

static int cmd_set(const struct shell *sh, size_t argc, char *argv[])
{
	unsigned long value;
	int err = 0;

	if (argc != 2) {
		return -EINVAL;
	}
	value = shell_strtoul(argv[1], 0, &err);
	if (err || value > 1) {
		return -EINVAL;
	}
	return request(sh, true, value);
}

SHELL_STATIC_SUBCMD_SET_CREATE(onoff_commands,
	SHELL_CMD_ARG(get, NULL, NULL, cmd_get, 1, 0),
	SHELL_CMD_ARG(set, NULL, "<0|1>", cmd_set, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((mesh, models), onoff, &onoff_commands,
		 "Generic OnOff Client", bt_mesh_shell_mdl_cmds_help, 1, 1);
