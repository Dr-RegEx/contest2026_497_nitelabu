# Peripheral candidates prepared without interrupting longrun864

GPIO886 and BMI160887 are separate FLAT images, both full offline CMake/Ninja
builds PASS. Neither has run on the board. Common xTS remains25/35.

GPIO uses J2 GPIO47 input /dev/gpio0 and GPIO48 output /dev/gpio1. Both pads
boot as inputs. The unchanged xTS selects pin direction dynamically. IRQ
counts are logged when disabled, outside the ISR; level events are masked
once until registration rearms them, preventing an interrupt storm. The
fixture runner requires actual IRQ evidence because original poll checks
accept a timeout. The original falling-edge test also has a setup race;
a missing event remains a failure/incomplete result, with no automatic retry.
885 initially failed Kconfig dependency resolution;886 uses DEV_GPIO as a
prerequisite rather than selecting it through its dependent lower-half option.

BMI160 uses I2C0 GPIO45/46, address0x68, the character interface /dev/accel0.
Missing sensors log unavailable and leave the shell accessible. Read/register
errors propagate; six-axis data and the complete24-bit timestamp are decoded
from15 sensor bytes without reading an uninitialized byte. The SPI helper is
excluded from I2C-only builds. Actual-function host checks pass ASAN/UBSAN,
covering bus errors, decoding, lengths, absent chips and registration failure.
These checks do not substitute for hardware or electrical validation.

User requested September19 submission with time reserved for physical tools.
`peripheral-test-guide.md` gives wiring and guarded commands for September17;
September18 is reserved for fixes/retests and demo evidence. `xts-peripheral.py`
checks explicit wiring confirmation, image receipt, profile and UART vacancy,
resets once, and runs original apps without retries or test-source edits.
Both firmware archives, source checkpoint888 and host/build evidence are
hashed in SHA256SUMS-xts888. The original24h run remains undisturbed.
