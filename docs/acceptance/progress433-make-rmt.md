# LED write hardening and offline Make parity

Main NuttX HEAD `ab5a407affe`; Apps HEAD `ae0dfd89c`.

New commits since checkpoint413:

- NuttX `21ebb146256`: WS2812 encoded RGB/RGBW offsets, bounds, zero-length
  resend, failure propagation and position preservation for retry.
- NuttX `ab5a407affe`: board Make GPIO HAL header paths for button builds.
- Apps `2acc034dd`: nettest Make host command-line parser linkage.
- Apps `ae0dfd89c`: s31led Make registration and disabled/y/m host regression.

## CMake and current board

Host414 negative test fails the old implementation's encoded-offset assertion;
new actual-function test passes RGB/RGBW, offsets, bounds, mutex failure,
mapping failure, lower-driver error/short write and retry position checks.
Build415 CMake PASS; flash416 matched hashes PASS; LED417 40 frames and
console/timer PASS; demo418 association/DHCP/gateway/startup PASS;
HTTP419 100 requests and boundary checks PASS. Board still runs candidate415,
LAN demo at http://192.168.1.60:8080/ and LED off before Make comparison.
User is remote: optical colors remain unverified. User requested advance
notice and repeat at a subsequent restart; announcement/repeat were made for
417 but no optical result has been supplied. Do not infer optical acceptance.

## Isolated Make build

Old out/esp32s31-make-production-verify worktrees were inspected and preserved
with their existing modifications. New out/esp32s31-make-rmt420/{nuttx,apps}
worktrees use existing local Git objects, not new clones or repository init:

```sh
git -C openvela-dev/nuttx worktree add --detach /home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-make-rmt420/nuttx 21ebb146256
git -C openvela-dev/apps worktree add --detach /home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-make-rmt420/apps f5284268f
```

Existing local clean mbedTLS `9fdfb2e5a49c5c650ec7ce0b87c4d1b6aed3ad4c`
and LittleFS `9d31e6d` repositories also supply detached worktrees at their
usual nested paths. HAL is copied by the existing offline Make hook from
the locked local reference. git clone/fetch/pull/reset/clean/checkout/submodule,
curl and wget are disabled by the existing offline-bin wrappers. No dependency
was downloaded or original/reference source modified. F0 host423 PASS.

Commands use build-rmt-make420.sh; exact underlying commands appear in logs:

- config420: `configure` -> correct S31, kernel/SMP, I2C50/51, RMT/s31led PASS.
- build421: default make -> first real error undefined `nettest_cmdline`,
  then `g_nettestserver_ipv4` in HOST tcpclient. Added nettest_cmdline.c to
  HOST_SRCS. Existing test_cmdline.py passed (host422).
- build422: first error board esp_gpio.h cannot find hal/gpio_types.h.
  Board.mk does not inherit arch private include paths. Added only needed
  GPIO/soc/common HAL include paths under CONFIG_ARCH_BUTTONS.
- build424: kernel Make build PASS, S31 image/digest generated.
- export425: `export` PASS.
- apps426: `apps` failed because fresh Apps import/scripts/Make.defs absent.
  This is missing export-import preparation, not a source API defect.
- import427: from new Apps worktree,
  `bash tools/mkimport.sh -d -z -x ../nuttx/nuttx-export-0.0.0.tar.gz` PASS.
  Only newly generated import directories were prepared by the standard tool.
- apps428: import compilation PASS; appfs429 packaging PASS, but independent
  artifact check found s31led absent. Added missing examples/s31led/Make.defs.
- apps430 and appfs431: PASS, now s31led present. Host431 verifies actual
  Make registration for disabled/built-in/module plus bounded app control flow.
- verify432: booleans1562/protocol7 PASS; kernel/AppFS size boundaries PASS;
  required app SHA256 sidecars PASS; s31led stack4096/demo8192 metadata preserved;
  Make stripping policy PASS; esptool image-info PASS. Receipt build432.

The new Make worktrees contain only the matching board Make.defs and Apps
nettest Makefile/s31led Make.defs changes. They do NOT contain the main tree's
older six Wi-Fi/SMP diagnostic edits. Their detached HEADs remain unchanged;
those fixes are committed on the main branches and archived as worktree patches.

## Remaining scope

Make hardware comparison is next, not yet passed at this checkpoint.
RMT low-level wait/cancellation/error wakeups and full SMP synchronization
remain incomplete; current accepted hardware scope is one low-brightness LED.
Wi-Fi HE, full stability, audio/I2S, remaining peripherals and formal xTS
remain incomplete. BLE/Zigbee are still scheduled last, not excluded.
