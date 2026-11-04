# Korvo Wi-Fi and BLE coexistence

This KERNEL/SMP profile enables the maintained Wi-Fi station and Bluetooth
service together. It inherits the existing coexistence integration, selects
the Korvo board, and leaves association under explicit shell control.

Use `bttool` for real advertising or scanning. Its existing `!` command
prefix calls the standard system shell, allowing WAPI commands to run while
the Bluetooth instance remains active. A separate owned BLE device must
observe the advertisements or provide the scan target.

WAPI configuration persistence and automatic network association are disabled.
The kernel and AppFS are one ABI pair. AppFS starts at 2 MiB and reserves
12 MiB; the final 2 MiB holds the temporary writable filesystem. Back up and
restore all affected Flash, and verify the original whole-device digest.

The profile is a coexistence test configuration. Building it does not qualify
the original 100-round scan and disconnect/reconnect workloads. It does not
enable the complete graphical SystemServer product.
