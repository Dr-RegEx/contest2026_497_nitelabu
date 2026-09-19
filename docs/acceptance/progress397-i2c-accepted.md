# I2C0 control-bus checkpoint

## Commits

- NuttX `44c62a6fe8e`: locked S31 I2C HAL integration, task/ISR exclusion,
  event preservation, bounded recovery and kernel descriptor/payload buffers.
- NuttX `d431245382c`: board I2C0 registration on GPIO50/51, opt-in profiles,
  bounded board test tool and acceptance documentation.
- Apps remains `e498e0ec6` (earlier ioctl errno propagation fix).

The older six Wi-Fi/SMP diagnostic modifications and three untracked profiles
remain separate. They were not swept into either I2C commit. No reference
repository or dependency was changed.

## Root cause and validation

See `progress366-i2c-investigation.md` for rejected hypotheses and trace 376.
The ISR observed flags 0 and length 0 instead of the caller's NOSTOP/length-1
message. In the ADDRENV build, retaining user virtual pointers across sleep
and accessing them from an ISR on another hart/address environment was unsafe.
The fix uses kernel snapshots and copies successful reads back in the caller.
Neither the software transfer timeout nor SCL timeout was increased.

Buffered diagnostic build 377 passed:

- I2C 382: 960 ID reads, 30 resets.
- I2C 385: 3840 ID reads, 15 resets, four NACK/retry sequences.
- I2C 387: 15360 ID reads, 60 resets, four NACK/retry sequences.
- HTTP 381, 383, 386, 390: each 100 samples plus boundary checks passed.
  I2C 382's first attempt did not execute because automatic approval review
  timed out; the permitted single retry succeeded. HTTP 383 was added to
  ensure substantial overlap, rather than assuming the delayed run overlapped.

Clean candidate build 388 (temporary ISR snapshots removed) passed:

- Offline CMake build and boolean/protocol-7 artifact validation.
- Flash 391: matching kernel/AppFS hashes verified, only offsets 0x2000 and
  0x200000 written. Existing complete first-5-MiB backup checked first.
- Demo 392: association, DHCP, gateway ping and HTTP server startup.
- I2C 394: 3840 ID reads, 15 resets, four NACK/retry sequences.
- HTTP 393: 100 samples and method/path/header/idle boundary checks.
- Host 389: all six I2C host regression scripts passed.
- Host 384: existing regressions and dependency F0 passed; the protocol model
  used revision 6e102 because temporary frame statistics are not covered.
- Host 396: IRQ routing/unmap, GPIO banks and board buttons passed.
- `git diff --check` and `tools/nxstyle arch/risc-v/src/common/espressif/esp_i2c.c`
  passed. Existing unrelated board-file style issues were not mass-formatted.

Exact board test command for I2C 394:

```sh
s31-reference/.venv-nuttx/bin/python -u openvela-dev/nuttx/tools/espressif/esp32s31_i2c_smoke.py --port /dev/ttyUSB0 --no-boot --batches 5 --reads 128 --nack-recovery > backups/2026-09-10-scan-stress/logs/i2c394-clean.log 2>&1
```

Full build/flash commands are traced in their logs. Build 388 used
`S31_DEMO_PROFILE=demo-i2c bash backups/2026-09-10-scan-stress/build-demo.sh`
with the absolute `build388-i2c-clean.sha256` receipt path.

The short 385 test had identical before/after kernel usage (98940 bytes).
During later HTTP traffic, transient heap usage was higher. After HTTP 393
completed, serial 395 sampled 99124 bytes twice, five seconds apart, with
1282048 page bytes used. This is bounded evidence, not a general leak proof.

## Limits and next work

Accepted scope is I2C0 control-bus ID/reset/NACK operation under tested SMP/MMU
and Wi-Fi b/g/n demo load. It does not certify physical bus timing, all I2C
flags/lengths, I2C1/LP-I2C, I2S audio, BMI160 xTS, or a full Make I2C build.
Wi-Fi HE acceptance remains unresolved; BLE/Zigbee remain scheduled last.
Next local subsystem is the onboard GPIO60 addressable RGB LED/RMT path,
followed by remaining audio/peripheral/network work. The board remains on
candidate 388 with the demo at http://192.168.1.60:8080/ at this checkpoint.
