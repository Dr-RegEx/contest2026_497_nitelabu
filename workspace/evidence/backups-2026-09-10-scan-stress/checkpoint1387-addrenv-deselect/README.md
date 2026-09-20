# Kernel address environment switch repair

1387补齐ARCH_HAVE_ADDRENV_DESELECT内核线程切换分支，保持硬件页表与g_addrenv同步。配置与1379完全相同；配对构建/烧录/启动/重新配网通过。原UDP TX1391在此镜像运行，未验收：首秒17.2KiB后零流量。此前PHY MMU故障截至观察尚未复现，但不以此宣布所有网络故障修复。
