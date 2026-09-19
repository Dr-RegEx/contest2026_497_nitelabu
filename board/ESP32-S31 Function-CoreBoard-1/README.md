已通过测试例程见 [demo/README.md](demo/README.md)，按用例编号整理源码、构建配置、运行步骤和验收依据。

复刻仓构建请先阅读 [根目录 README](../../README.md)。本目录为实际板级源码副本，以下是原移植说明与历史状态。

# ESP32-S31 Function-CoreBoard-1

This port uses the S31 preview ABI from these existing, locked dependencies:

- ESP-IDF: `14f663f003eb8fd9a688c301a412a9540d29dacf`.
- Espressif HAL: `290edc31b50decca660c1a11ce3506fd9b2e1e27`, including its
  matching PHY, Wi-Fi, coexistence and mbedTLS/TF-PSA submodules and patches.
- Toolchain: `riscv32-esp-elf-`.

Do not substitute another chip's binary libraries or reset/repatch the locked
reference checkout. Set `ESP_HAL_3RDPARTY_LOCAL` to the verified local snapshot.
The Make build copies it into its own chip directory without fetching or
resetting submodules. Make's S31 wireless source list is separate from the
older common `Wireless.mk`; its parity check is:

```sh
python3 tools/test_esp32s31_wireless_make.py --hal "$ESP_HAL_3RDPARTY_LOCAL"
```

## Configurations

- `esp32s31-core-function-board` / `nsh`: minimal flat-build NSH.
- `esp32s31-core-function-board:smp`: SMP bring-up configuration.
- `esp32s31-core-function-board:production`: S-mode kernel, two CPUs,
  external user-mode applications, read-only ROMFS seed, writable LittleFS
  and Wi-Fi station support.
- `esp32s31-core-function-board:demo`: the same kernel facilities, explicit
  b/g/n station compatibility mode, DHCP setup, TCP/UDP examples and a
  read-only LAN status page. Bluetooth and Zigbee are not enabled.

Confirm `CONFIG_ARCH_CHIP_ESP32S31=y` and `CONFIG_ESPRESSIF_ESP32S31=y` after
configuration. The old Espressif chip-series choice must not select C3.

Use separate build worktrees for Make and CMake. A Make configuration in the
source tree can shadow the generated CMake configuration; do not switch a
live worktree or discard its contents merely to run another build method.

For a kernel-mode Make build, building the kernel is not enough to prepare
AppFS. Run `make export` in NuttX, then from the matching Apps worktree run
`tools/mkimport.sh -z -x <absolute-path-to-nuttx-export.tar.gz>` followed by
`make import`. The import tool replaces generated import directories: use an
isolated build tree and preserve any earlier generated artifacts first.
Package that tree's `apps/bin` with `esp32s31_prepare_appfs.py` and `genromfs`.
Check required application files and their SHA256 sidecars before flashing;
a successful build alone does not prove that every enabled app was registered.
The kernel and user applications must use matching source/configuration and
the same export package. Keep all dependency acquisition offline against the
existing verified local repositories.

### Competition xTS profiles

The following dedicated profiles keep the original xTS applications and
assertions. Status here is as of2026-09-15; the workspace's
`backups/2026-09-10-scan-stress/xts-current-status.md` records subsequent
hardware results and exact paired-image receipts.

| Profile | Purpose | Hardware evidence |
| --- | --- | --- |
| `xts-flat-rtc` | M-mode FLAT, Octal PSRAM heap, original mm/RAM-block/RTC tests | Build837, tests839–841 PASS |
| `xts-flat-flash` | Original block test on fixed1MiB scratch at0xc00000 | Build865 only; target pending |
| `xts-flat-wdt` | Original four watchdog modes, RTC-system reset and CLIC priority7 fatal IRQ | Build868 only; target pending |
| `demo-rmt-xts-standby` | SMP/MMU kernel KASAN and resource monitor | Build860;12h/24h run864 in progress |
| `demo-rmt-xts-ecc` | Combined AES modes, SHA/HMAC and ECC/ECDSA candidate with the LAN demo | Build883 and host checks only; target pending |

FLAT requires `ESPRESSIF_SPIRAM_USE_8LINE_MODE` for this module. Its heap
registration uses the actual HAL-reported free PSRAM interval. The watchdog
profile is single-core M-mode: normal critical sections preserve fatal IRQ
level7 using threshold0xdf; capture callbacks use level1. This is not an SMP
watchdog claim. Flash scratch registration does not write or erase; perform
the existing two-read blank backup guard before the destructive block test.

The standby profile uses internal filesystem allocation. Kernel KASAN hooks
execute from IRAM, and BUILD_KERNEL process-private heaps do not register
unmapped private shadows in the global kernel checker. User-process sanitizer
coverage and the large PSRAM filesystem heap with KASAN are not established.
Keep the board, power and UART uninterrupted during the timed run.

The combined Crypto profile uses ordinary volatile keys. AES192 and unsupported
algorithms retain software fallbacks; HMAC-SHA1/256 uses SHA inner/outer hashes,
not the eFuse-keyed HMAC peripheral. ECC point multiplication assists P256
keygen/sign, with scalar arithmetic in software; ECDSA verification uses its
hardware engine. Original apps and hardware completion records must both pass
before claiming target coverage. No eFuse provisioning is performed.

### Basic LAN demo

Build the `demo` profile with the existing CMake workflow. The matching Apps
tree must include `examples/s31demo`. Back up any newly affected AppFS sectors
before flashing the larger image; keep the kernel and AppFS from the same
build and do not overwrite the writable partition at 0x500000.

With the USB-UART console connected and the paired images installed:

```sh
python3 tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0 --log demo-start.log
```

The tool prompts privately for an authorized WPA2 SSID/password, resets the
board, obtains DHCP, checks the gateway and starts `s31demo`. It leaves the
board connected and prints `http://<board-ip>:8080/`. Open that address from
the trusted test LAN; `/status.json` provides uptime, IPv4 and request count.
Credentials are not persisted. A reset requires setup again. Stop the demo
with NSH `kill <pid>` or reset the board.

For manual startup after association/DHCP, use `prlimit -s 8192 s31demo &`.
The explicit stack limit avoids the ELF loader's 2 KiB fallback when AppFS
stripping removes `nx_stacksize`; the interactive tool applies this limit too.
The S31 Make/CMake toolchain now retains the loader's `nx_stacksize`,
`nx_heapsize`, `nx_priority` and identity metadata during stripping. Validate
the installed executable against its unstripped ELF before relying on those
defaults:

```sh
python3 tools/test_esp32s31_elf_metadata.py --elf <build>/bin/s31demo \
  --reference <build>/bin_debug/s31demo --check-make
```

Validate the page separately from a LAN client with the Apps tree's
`python3 examples/s31demo/test_http.py <board-ip>`.

The page is unauthenticated and read-only, with no external assets, uploads,
commands or filesystem writes. Do not expose it to the Internet. Successful
demo setup is not full HE, Bluetooth, Zigbee, driver or xTS acceptance.

### CPU1 interrupt-routing regression

The `demo-cpu1-init` diagnostic profile includes `demo` and changes the
default task affinity to CPU1. The Wi-Fi worker remains explicitly bound
to CPU0. This exercises RTC/high-resolution timer initialization on CPU1:
the interrupt matrix must target the same hart whose local CLIC is being
configured. Normal `demo` keeps the default affinity covering both CPUs.

Host checks use the actual IRQ helper bodies:

```sh
python3 tools/test_esp32s31_irq_route.py
python3 tools/test_esp32s31_irq_unmap.py
```

These models check routing and disconnect encoding, not remote-hart
enable/disable synchronization. Real CPU1 initialization, scan, DHCP,
gateway traffic and LAN HTTP must also be verified with the diagnostic
kernel and its matching AppFS. A pass does not establish HE acceptance.

### Wi-Fi event groups (kernel builds)

S31 kernel builds implement the five vendor event-group callbacks with
24 application bits, full pre-clear snapshots, any/all waits and bounded
or indefinite tick waits. Group deletion wakes pending waiters with zero;
the pre-stack-release task hook removes waiters belonging to a stopped
task. An owner must prevent new calls after deleting the group.

```sh
python3 tools/test_esp32s31_wifi_event.py
python3 tools/test_esp32s31_wifi_task.py
python3 tools/test_esp32s31_event_smoke.py
```

The first test uses the actual event implementation and stack-release wrapper
with pthread-backed OS primitives and sanitizers. It covers set/timeout/delete
races, multiple waiters and stopped tasks. It is not a substitute for targeted
board task-deletion/SMP stress or evidence that the HE data-path failure is
fixed. Other chips and non-kernel builds retain their existing callbacks.

The `demo-rmt-event` diagnostic profile runs eight bounded kernel boot tests:
set/wake, partial timeout, group deletion and forced blocked-task deletion,
in both CPU0-to-CPU1 and CPU1-to-CPU0 directions. It checks actual worker CPU
and event waiter registration, and restores the bring-up thread's affinity.
Use the matching diagnostic kernel/AppFS, then:

```sh
python3 tools/espressif/esp32s31_event_smoke.py --port /dev/ttyUSB0 --boots 3
```

Three resets/24 cases passed on Function-CoreBoard-1. This is bounded
cross-core functional coverage, not long-term event/allocator stress.
Normal demo/production leave `CONFIG_ESP32S31_WIFI_EVENT_TEST` disabled.
Restore the ordinary demo image pair after diagnostic testing.

### I2C0 and the board codec control bus

The `demo-i2c` profile adds the interrupt-driven I2C0 controller and the
`i2c` application. GPIO50 is SCL and GPIO51 is SDA; `/dev/i2c0` is registered
at board bring-up. The normal `demo` profile does not enable this bus.

The [board schematic](https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf)
pulls the ES8311 CE pin low. The
[codec interface specification](https://dl.espressif.com/dl/schematics/Audio_ES8311.pdf)
therefore selects seven-bit address `0x18`. Read its ID registers without
changing audio settings or enabling the GPIO57-controlled amplifier:

```sh
i2c get -b0 -a18 -rfd -w8 -f100000
i2c get -b0 -a18 -rfe -w8 -f400000
```

Expected values are `83` and `11`, respectively. Register reads use an
address write followed by repeated START and a final read NACK. The S31
recovery operation (`i2c reset`) serializes against transfers and bounds
its recovery polling without masking global interrupts for that wait.

Host regression checks:

```sh
python3 tools/test_esp_i2c_frequency.py
python3 tools/test_esp_i2c_commands.py
python3 tools/test_esp32s31_i2c_reset.py
python3 tools/test_esp_i2c_spurious_irq.py
python3 tools/test_esp_i2c_transfer_lock.py
python3 tools/test_esp_i2c_kernel_buffers.py
```

The S31 task and ISR share a device spinlock, released before waiting for
completion or polling bus recovery. Failed interrupt-driven transfers stop
the controller and drain the completion semaphore before returning. Command
rearming preserves newly latched peripheral events instead of clearing them.
With `ARCH_ADDRENV`, descriptors and payloads are copied to shared kernel
allocations before starting the transfer. Read results are copied back by
the issuing task, never through user pointers in an ISR on another hart.
Without this, concurrent HTTP exposed corrupted descriptor flags/lengths
and intermittent STOP-stage timeouts despite the device lock. The diagnostic
`demo-i2c-cpu1-init` profile is an initialization-affinity comparison, not
a production workaround or a substitute for the default dual-hart test.

The buffered diagnostic firmware passed 15,360 ID reads, 60 controller
recoveries and four NACK/retry sequences under overlapping LAN HTTP load
with the default dual-hart CPU set.

After removing the temporary ISR snapshots, the candidate passed another
3,840 reads, 15 controller recoveries, four NACK/retry sequences and a
100-request HTTP run including request-boundary checks. Run board checks (omit
`--no-boot` for a boot check; omit `--nack-recovery` if external devices could
occupy address `0x19`):

```sh
python3 tools/espressif/esp32s31_i2c_smoke.py --port /dev/ttyUSB0 --no-boot --batches 5 --reads 128 --nack-recovery
```

Successful codec ID reads do not validate I2S audio, physical SCL timing,
all I2C transfer modes, I2C1/LP-I2C or the BMI160-based xTS I2C/SPI test.

### On-board RGB LED

The `demo-rmt` profile extends `demo-i2c` with RMT TX channel 0 and the
single WS2812 LED on GPIO60, registered as `/dev/leds0`. The matching Apps
tree must provide `examples/s31led`. Run `s31led 1` to send low-brightness
red, green, blue and off; the optional cycle count is bounded to 1..10.
The application also opens two descriptors and closes one before writing,
exercising the shared driver's buffer lifetime.

```sh
python3 tools/test_esp32s31_rmt_compat.py
python3 tools/test_esp_ws2812_lifecycle.py
python3 tools/test_esp_ws2812_write.py
python3 tools/test_esp_rmt_completion.py
python3 tools/test_esp_rmt_write.py
python3 tools/test_esp_rmt_acquire.py
python3 tools/test_esp_rmt_initialize.py
python3 tools/test_esp_rmt_install.py
python3 tools/test_esp_rmt_rx_start.py
python3 tools/test_esp_rmt_tx_lifecycle.py
python3 tools/test_esp_rmt_tx_concurrency.py
python3 tools/espressif/esp32s31_led_smoke.py --port /dev/ttyUSB0 --cycles 1
```

Use `--no-boot` to preserve an already running network demo. A failed
serial smoke test resets the board to recover from a blocked transmitter.
The serial PASS means writes completed and the console remained responsive;
actual colors and waveform timing require observation or measurement.
This profile does not establish RMT RX, DMA, long-stream refill, loop mode,
error cancellation or complete cross-hart synchronization acceptance.

On 2026-09-14, an observer confirmed the red, green, blue, off sequence
for ten cycles at brightness 8, including the final off state. The matching
serial test completed 40 frames and passed the timer/console checks.
This accepts the basic on-board color sequence only; waveform timing,
colorimetry and brightness linearity remain unmeasured.

The offline Make kernel/export/import/AppFS workflow has also passed real
board boot, 40 LED frames, ten BOOT electrical interrupt edges, association,
DHCP, gateway ping, 3,840 I2C ID reads with 15 resets and NACK recovery, and
100 LAN HTTP requests with boundary checks. I2C and HTTP were overlapped;
an additional 40 LED frames passed while the network demo remained running.
These are bounded regressions, not a long-duration or optical certification.

### BOOT button and GPIO interrupts

The [official board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html)
assigns BOOT to GPIO61. The board configures it as an active-low input with a
pull-up, never an output. Enable `ARCH_BUTTONS`, `ESPRESSIF_GPIO_IRQ`,
`ARCH_IRQBUTTONS`, `INPUT`, `INPUT_BUTTONS` and `INPUT_BUTTONS_LOWER` to expose
`/dev/buttons`; the `demo` profile includes these and the `buttons` example.
Run `buttons &` to monitor state changes. Holding BOOT during reset enters
the ROM downloader, so release it before normal boot.

The S31 GPIO HAL routes pin interrupts through GPIO_INTR0. Both 32-bit
status banks are acknowledged and dispatched without shifting away the
original pin positions. GPIO61 electrical assertion/release was exercised
through the board's documented USB-UART auto-download circuit with EN held
high: five cycles/ten edges produced poll notifications and the expected
states without reset. This does not test the manual switch contacts or
every header pin. Host regressions:

```sh
python3 tools/test_esp_gpio_irq_banks.py
python3 tools/test_esp32s31_buttons.py
```

The current Apps button example must include its kernel-ELF entry and
current-state read fixes. The upper half returns a state on every read;
draining it as if it were a pipe would loop indefinitely.

### Wi-Fi protocol compatibility

The S31 non-lazy FPU context path must save CLEAN as well as DIRTY banks:
its destination is a new exception stack frame, not the persistent FPU
area used by lazy switching. Skipping a CLEAN save can restore uninitialized
registers and FCSR later. A captured ROM illegal instruction was a dynamic
rounding floating-point conversion with FCSR=0xc8 (reserved FRM=6).
The S31 fix preserves the existing INITIAL/OFF, restore, and lazy policies;
it does not change other chips or claim complete FPU initialization coverage.

The bounded host regression is `python3 tools/test_esp32s31_fpu_context.py`.
It interprets the actual assembly's control flow, not floating-point hardware.
Both production and minimal Make builds and the production CMake build passed.
On-board b/g/n checks passed three DHCP/DNS/TCP/UDP rounds and a separate
three-round, 174-packet gateway ping check without the ROM fault. These are
bounded regressions, not a long-duration reliability or HE acceptance result.

The S31 sdkconfig bridge enables IRAM, RX_IRAM and EXTRA_IRAM optimizations.
The linker therefore collects their RX/sleep shared and extra sections in
internal SRAM as required by the locked HAL's `esp_wifi/linker.lf`; sleep-only
sections remain in flash. Check a Wi-Fi build with
`python3 tools/test_esp32s31_wifi_iram.py --map <build>/nuttx.map`.
Both Make and CMake maps and three b/g/n network rounds passed. Three HE
rounds still lost gateway packets after DHCP, so placement is not an HE fix.

S31 kernel-mode Wi-Fi lock deletion and coexistence frees must use
`kmm_free` to match their internal allocations. Plain `free` follows the
current task's heap and is not safe for these objects from user-task ioctl
contexts. `python3 tools/test_esp32s31_wifi_heap_callbacks.py` checks the
actual callback tables and isolates the S31 kernel change from other profiles.
This pairing correction is not evidence of the HE failure's cause.

`CONFIG_ESPRESSIF_WIFI_STA_11AX` defaults to `y`, allowing the S31 station
to use 802.11b/g/n/ax. Disabling it restricts only the station to b/g/n;
it does not disable AMPDU or change the SoftAP protocol. Use it for explicit
interoperability comparisons, not as evidence that 802.11ax has passed.

On the current test network, three independent b/g/n connections passed DHCP
and all 24 gateway pings, while restoring b/g/n/ax reproduced gateway loss
in three connections despite successful DHCP. The HE-path cause is still
unresolved; this does not establish a general router or silicon defect.

An on-board isolation test on 2026-09-11 used the locally locked ESP-IDF
`14f663f003eb8fd9a688c301a412a9540d29dacf`. Three diagnostic variants used
the original IDF binaries, the locked HAL PHY binary, and the locked HAL
PHY plus Wi-Fi binaries respectively. Each variant passed three HE20 and
three HT20 connections, DHCP, eight gateway pings per connection, and an
exact 4096-byte TCP echo checked by the peer. No dependency repositories
were modified. Temporary IDF flash sectors were restored from two matching
backups, followed by a full 5 MiB byte-for-byte comparison and an NSH boot
check; the writable partition at 0x500000 was not accessed.

These results show that the same board, test APs, and locked PHY/Wi-Fi
binaries can carry HE traffic under IDF. They do not validate every HAL
subsystem or prove that NuttX's OS interface and startup are equivalent.
Disabling software coexistence for Wi-Fi-only operation did not fix the
NuttX HE failure and did not pass all b/g/n regressions; that candidate was
withdrawn. Omitting the explicit active/passive scans also reproduced the
HE failure. Keep investigating the NuttX configuration, startup and OS
interface differences rather than replacing the locked dependencies.

The S31 IPv4 station publishes its assigned address to the vendor driver
after association, matching ESP-IDF's GOT_IP notification. With this older
netdev interface, a deferred worker waits for a nonzero address and stops
after notification succeeds; disconnection/interface-down cancels the poll.
It does not acquire an address or replace the DHCP client. For DHCP tests,
use `CONFIG_NETINIT_IPADDR=0x00000000` in the isolated build so an old static
default is not mistaken for a lease. This integration is not an HE fix.

Refresh an existing build's Kconfig before relying on a newly added option.
CMake reconfiguration alone does not expand new defaults in an existing
`.config`. Preserve that file and use the matching Kconfig environment;
`resetconfig` replaces it with the board defaults and loses local settings.

## Make production applications

In this openvela version, a kernel-mode `make` builds the kernel and user
libraries, **not** the user applications. A successful `nuttx.bin` alone is
not a complete production installation. With the existing toolchain, esptool,
Kconfig tools, genromfs and all manifest dependencies available locally:

```sh
# Run in an isolated, already populated NuttX worktree.
tools/configure.sh -l esp32s31-core-function-board:production
make -j8
make -j8 export

# Select the exact export package just produced; do not use a stale SDK.
# This checkout currently reports version 0.0.0.
cd ../apps
bash tools/mkimport.sh -z -x ../nuttx/nuttx-export-0.0.0.tar.gz
make -j8 import
cd ../nuttx

python3 tools/espressif/esp32s31_prepare_appfs.py \
  --source ../apps/bin --output appfs-root --max-size 524288
genromfs -f appfs.img -d appfs-root -V S31AppFS
```

The import command replaces generated SDK contents under `apps/import`.
The preparation script replaces its generated `appfs-root` directory. Keep
user files out of both paths and preserve any previous artifacts before
repeating those operations. Neither step should access the network. If a
dependency is absent, resolve it against the project manifest before proceeding.

Production currently supplies `init`, `sh`, `wapi`, `ping` and `renew`.
Check each ELF against `CONFIG_ELF_APP_MAX_FILESIZE`; the preparation script's
`--max-size` argument is also used to construct deliberate oversized inputs
when `--e0-corpus` is selected and is not a general file-size validator.

## Flash layout and verification

With the current production configuration:

| Region | Offset | Limit |
| --- | --- | --- |
| Simple Boot kernel (`nuttx.bin`) | `0x2000` | End before `0x200000` |
| Read-only application seed (`appfs.img`) | `0x200000` | `0x300000` bytes |
| Writable LittleFS | `0x500000` | `0x700000` bytes; preserve existing data |

The ROM-visible SHA-256 digest must be added by
`tools/espressif/esp32s31_simple_boot_digest.py`; both build methods invoke it.
Use the USB-UART connection and verify the board identity before flashing.
Write only the kernel and seed regions, never erase the whole flash to work
around a boot problem. Keep a known-good kernel/AppFS pair for recovery.

The production smoke check does not need network credentials:

```sh
python3 tools/espressif/esp32s31_production_smoke.py \
  --port /dev/ttyUSB0 --baud 115200 --boots 3 --storage --radio-cycles 3
```

It checks SMP/NSH startup, application mounts, timers and radio lifecycle.
`--storage` creates a unique temporary file, verifies it across reboot, then
removes it. A smoke or scan pass is not WPA2/DHCP/network or xTS acceptance.
