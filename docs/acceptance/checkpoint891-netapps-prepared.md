# Network category candidate 891

Full offline build PASS; TARGET NOT RUN. Longrun864 remains on the board.
The paired receipt is build891-netapps-country.sha256, output
openvela-dev/out/esp32s31-netapps. Kernel1021688 bytes, AppFS1490944 bytes.

CountryCode getter now dispatches SIOCGIWCOUNTRY and reads the current ESP
country into the caller's checked buffer. The original setter is unchanged.
Original case4.1.57 is prepared in xts-wifi-lifecycle.py country, including
US and CN readback. Run only after longrun exits and this pair is flashed.

The existing curl CLI requires PIPES and LIBC_LOCALE, now enabled alongside
local zlib and MbedTLS. FTP's example task_create path is unavailable in
BUILD_KERNEL; ftpd_start runs its daemon in its own process, compatible with
the published ftpd_start -4 & command. Argument offsets follow that process
model, and the in-process-global ftpd_stop command is excluded in this mode.
Terminate its actual PID using NSH kill when needed. FLAT behavior is retained.

Original cases4.1.113/114/117 are now build-ready, not passed. SCP is still
unprepared. HTTPS support has compiled, but is not required by these HTTP
case bodies and has no new target evidence. Builds889/890 exposed FTP task
creation and curl missing-libc dependencies;891 is the successful result.
Git diff --check passed for nuttx/apps and runner syntax compiled.
