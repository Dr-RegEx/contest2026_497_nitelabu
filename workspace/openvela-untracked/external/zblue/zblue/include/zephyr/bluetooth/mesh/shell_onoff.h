/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_MESH_SHELL_ONOFF_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_MESH_SHELL_ONOFF_H_

#include <zephyr/bluetooth/mesh.h>

extern const struct bt_mesh_model_op bt_mesh_shell_onoff_ops[];
extern const struct bt_mesh_model_cb bt_mesh_shell_onoff_cb;

#define BT_MESH_SHELL_GEN_ONOFF_CLI_MODEL \
	BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, \
			bt_mesh_shell_onoff_ops, NULL, NULL, &bt_mesh_shell_onoff_cb)

#endif
