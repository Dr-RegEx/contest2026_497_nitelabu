/* SPDX-License-Identifier: Apache-2.0 */
/* Intentionally included twice: NuttX has no linker-sorted shell sections. */
#if defined(CONFIG_BT_MESH_SHELL_CFG_CLI)
MESH_SHELL_MODEL(cfg)
#endif
#if defined(CONFIG_BT_MESH_SHELL_HEALTH_CLI)
MESH_SHELL_MODEL(health)
#endif
#if defined(CONFIG_BT_MESH_SHELL_SAR_CFG_CLI)
MESH_SHELL_MODEL(sar)
#endif
#if defined(CONFIG_BT_MESH_SHELL_RPR_CLI)
MESH_SHELL_MODEL(rpr)
#endif
#if defined(CONFIG_BT_MESH_SHELL_LARGE_COMP_DATA_CLI)
MESH_SHELL_MODEL(lcd)
#endif
#if defined(CONFIG_BT_MESH_SHELL_OP_AGG_CLI)
MESH_SHELL_MODEL(opagg)
#endif
#if defined(CONFIG_BT_MESH_SHELL_PRIV_BEACON_CLI)
MESH_SHELL_MODEL(prb)
#endif
#if defined(CONFIG_BT_MESH_OD_PRIV_PROXY_CLI)
MESH_SHELL_MODEL(od_priv_proxy)
#endif
#if defined(CONFIG_BT_MESH_SOL_PDU_RPL_CLI)
MESH_SHELL_MODEL(sol_pdu_rpl)
#endif
#if defined(CONFIG_BT_MESH_SHELL_BRG_CFG_CLI)
MESH_SHELL_MODEL(brg)
#endif
#if defined(CONFIG_BT_MESH_SHELL_DFD_SRV)
MESH_SHELL_MODEL(dfd)
#endif
#if defined(CONFIG_BT_MESH_SHELL_DFU_SLOT) || defined(CONFIG_BT_MESH_SHELL_DFU_METADATA) || defined(CONFIG_BT_MESH_SHELL_DFU_CLI) || defined(CONFIG_BT_MESH_SHELL_DFU_SRV)
MESH_SHELL_MODEL(dfu)
#endif
#if defined(CONFIG_BT_MESH_SHELL_BLOB_CLI) || defined(CONFIG_BT_MESH_SHELL_BLOB_SRV) || defined(CONFIG_BT_MESH_SHELL_BLOB_IO_FLASH)
MESH_SHELL_MODEL(blob)
#endif
