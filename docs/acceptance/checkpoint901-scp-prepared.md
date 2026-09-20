# Network SCP candidate 901

Full offline build and packaging PASS; TARGET NOT RUN. AppFS2124800 bytes
fits the existing3MiB seed partition. Paired build901-netapps-scp.sha256,
output esp32s31-netapps-ssh. This retains891 CountryCode/curl/FTP and adds
original SCP for category4.1.115/116. No SSH server was started on host/board.

The existing libssh CMake assigned common connection/authentication/knownhost
helpers to different applications, leaving unresolved symbols in separate
BUILD_KERNEL ELF executables. Helpers now belong to the libssh static library
once, preserving their original implementation. UTILS_SSH_SCP_ONLY defaults
off; this constrained profile enables it to omit unrelated ssh/sshd apps.
Make and CMake both honor it. The full utilities build900 linked, but its
3377152-byte AppFS exceeded capacity and was not accepted.901 packages only
SCP plus the existing network demo and fits without partition changes.

Original host verification and credential prompts remain. Actual PC-to-board
and board-to-PC SCP transfer, remote authentication, file-size verification
and crash-free UART evidence are still pending. An existing permitted test
server/account is needed when executing. Do not log authentication secrets.

The original4.1.115 sample /dev/data path differs from its /data wording;
use the actual mounted disposable /data destination and record that mapping.
No transfer to user production files is necessary; use fresh test filenames.
