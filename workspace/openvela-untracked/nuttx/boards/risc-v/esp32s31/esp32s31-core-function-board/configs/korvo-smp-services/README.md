# Korvo SMP system services

This profile combines independent SystemServer/AM/PM/WM processes, the native HOME, Device and Widgets applications, and the MEDIA playback service with the standard RGB565 framebuffer and ES8389 audio path. It retains the QuickJS/Feature foundation libraries; a complete Quickapp RPK runtime is not enabled.

Build the profile `esp32s31-core-function-board:korvo-smp-services`. The kernel and external application ROMFS are one ABI pair and must come from the same build. The 16 MiB layout reserves 2 MiB for boot/kernel, 12 MiB for applications and 2 MiB for LittleFS data. Inspect actual image sizes before flashing. There is no OTA A/B slot.

Boot stops at NSH. After verifying `/system/bin`, `/data` and `/etc`, run `sh /etc/init.d/start-system` to request the four services; then verify their readiness. No network provisioning, recording or automatic playback occurs during boot. Wireless drivers are disabled in this service-integration profile. Filesystem mounts and services must be qualified on the target; successful compilation alone is not runtime acceptance.
