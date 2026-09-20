# PWM category candidate 898

Full offline build PASS, kernel327784 bytes. TARGET NOT RUN. Original case
4.2.20 requires cmocka_driver_pwm and an actual waveform artifact. Original
application default is100Hz/50percent/5seconds, one cmocka case. It does not
measure the waveform; API PASS alone cannot close the published case.

The S31 common-driver CMake entry already existed, but the old esp_ledc.c
uses a different HAL ABI. A small S31 lower-half now uses pinned S31 LEDC LL
interfaces directly, group0/timer0/channel0 and GPIO48 J2.14. ESPRESSIF_LEDC
is disabled in this separate FLAT profile; shared old driver/HAL files are
unchanged. No pulsecount/multichannel scope. Frequencies outside representable
18-bit8.8-divider bounds fail, duty is validated as16.16 in[0,1], unsupported
ioctl returns ENOTTY. XTAL frequency is queried, not assumed. Resolution is
chosen up to14bits. Driver registration starts no output; setup holds low,
start sets timer/duty/gamma RAM, stop gates output low, close releases clocks
and leaves the pad input. GPIO48 is exclusive to this separate fixture image.

Clock/reset/memory/channel-power and gamma RAM sequencing was checked against
pinned IDF esp_driver_ledc and S31 esp_hal_ledc/hal/ledc_ll.h. No CPUfreq/clock
constant from another ESP32 was reused. Build897 caught a missing critical
section declaration include;898 is the successful build. Diff check passed.
Actual waveform frequency/duty/stopping and board startup still await target.

Physical capture: connect scope/logic-analyzer signal to GPIO48 J2.14 and
its ground to boardGND;3.3V signal. Remove the separate GPIO47/48 loopback wire
for clarity. Trigger on rising edge before original five-second run. Retain
frequency/duty measurements, waveform image and raw UART under same test tag.
Use xts-peripheral.py pwm --receipt build898-flat-pwm.sha256 --wiring-confirmed
only after receipt-checked flashing and longrun864 completion. That runner
reports PWM API PASS with waveform acceptance PENDING, not category PASS.
