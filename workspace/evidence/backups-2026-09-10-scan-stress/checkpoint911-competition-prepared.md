# Unified competition demo candidate911

Full offline build and packaging PASS; TARGET NOT RUN. Kernel1173468 bytes,
AppFS2356224 bytes, both within existing2MiB/3MiB slots. No repartition.
Profile demo-rmt-netapps-competition inherits904 and adds883 hardware Crypto
options with the ordinary software fallback, without original Crypto test
applications. Includes original curl/FTP/SCP/iperf2, existing Wi-Fi HTTP RGB
button I2C storage demo and SMP/MMU. CountryCode getter is included.

This does not add audio, BLE, USB ADB, OTA or a production watchdog, nor does
it turn the separate FLAT GPIO/PWM/BMI fixtures into a unified SMP driver.
Those remain explicit gaps. Hardware Crypto acceptance still first requires
883's original eight test programs and hardware evidence; this demo has no
new API self-test clone. Network acceptance can run original tools on911
once boot and association are verified. Do not count compiled features as PASS.

Build910 compiled/linked but its new packaging check used the wrong app name
iperf; the existing original utility is iperf2. No app rename or source change
was needed. Corrected helper completed911 and wrote the paired receipt.
Known810/860/883/904 outputs and backups remain unchanged. Flash911 only after
864 completion and queued common validation, using S31_DEMO_PROFILE set to
demo-rmt-netapps-competition and build911-competition.sha256. No flash occurred.
