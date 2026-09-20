# Candidate980: offline competition presentation, BUILD ONLY

Independent profile demo-rmt-netapps-competition-offline inherits967 and keeps
NETINIT_NETLOCAL with automatic DHCP disabled. Existing967 and810 outputs and
receipts are untouched. Kernel1173468 bytes, AppFS2348032 bytes, one ABI pair.

The existing s31demo application adds --status [interface]. It reports real
monotonic uptime, interface flags/address and device-node presence. A datagram
socket is used only for read-only ioctls; no listener, association, DHCP, or
network traffic. Device presence is not functional acceptance. Query failures
return nonzero and S31_DEMO_STATUS=ERROR. Existing HTTP mode remains available
only when authorized; absence of --status still selects that original mode.

The existing host esp32s31_demo.py adds --offline --receipt. It checks pair
hashes and no-automatic-network config, rejects busy tty, uses existing raw
115200 without termios/DTR/RTS changes, and does not reset. It runs status,
existing RGB cycle, two ES8311 ID reads, free/ps/df. It creates a fresh host
transcript and stops on failures. Optical, BOOT button and audio reviews stay
explicitly pending. The receipt identifies expected files, not independent
cryptographic attestation of running firmware; retain the actual flash/boot
record for the matching pair. Never run while864 owns UART.

After actual longrun evidence review and board release:

```sh
S31_DEMO_PROFILE=demo-rmt-netapps-competition-offline bash backups/2026-09-10-scan-stress/flash-demo-pair.sh "$PWD/backups/2026-09-10-scan-stress/build980-competition-offline.sha256"
s31-reference/.venv-nuttx/bin/python openvela-dev/nuttx/tools/espressif/esp32s31_demo.py --offline --port /dev/ttyUSB0 --receipt backups/2026-09-10-scan-stress/build980-competition-offline.sha256 --log backups/2026-09-10-scan-stress/logs/demo980-offline-first.log
```

Allow normal boot to reach NSH and release any logger before the second command.
The tty must already be raw115200. Observe RGB during execution, then use the
existing buttons command in NSH and press/release BOOT, retaining that actual
record separately. No external sensor or loopback jumper is required.

Validation: clean independent3790-step build, F0 dependency lock, source/shell
checks. Host actual C executable tested --status on loopback and a missing
interface; strace showed no listener or traffic calls. Python success and bad
codec-ID paths exercised with simulated UART responses and traps on credential,
reset and association entry points. These are host checks, not board results.
Original xTS source/workloads were not changed by980. Audio/BLE/USB still require
separate profiles: their FLAT/single-core/memory restrictions do not disappear
when this demo builds. Original M4 requirement is still not satisfied by980.
