# Source recovery checkpoint970 — 2026-09-16

Recovery snapshot, not a build or hardware PASS. Captures the current eight
repository HEADs, working changes against HEAD (including staged changes), and
current contents of the same explicitly selected source files as checkpoint953.
Includes ADC954, USB965, competition967 and audio968 source updates and current
board-only runners. Existing source trees and previous evidence are untouched.

This is NOT a standalone SDK. Keep existing pinned repositories, toolchain,
HAL/IDF dependencies and downloaded third-party sources. Untracked omissions
are listed per repository; these include generated files and NIST data/templates.
Private SSH keys and local credentials are not part of the selected helper set.

Restore into separate clean copies at the recorded HEADs: apply each repository's
working.patch with git apply, then extract its untracked-source archive in that
repository. Nested repositories have their own patches. Do not apply over the
current dirty workspace or discard existing changes. Helpers belong in the
original backups/2026-09-10-scan-stress directory; build-demo.sh and
flash-demo-pair.sh are the frozen competition967 entry points. Paths are local
workspace paths and must be reviewed when moving machines.

For FLAT builds use build-xts-flat.sh with S31_FLAT_PROFILE and a fresh receipt.
For the competition pair use the preserved build-demo.sh; kernel and AppFS must
remain paired. The f0 lock records reference dependency versions. Build-only
images remain 946/947/948/949/950, 954, 965, 967, 968 and 939 as documented.
This snapshot does not assert fresh compilation of every profile.

Current restriction: no Wi-Fi provisioning, DHCP or NTP. Collector6841 retains
the UART for interrupted-host/recovered-board longrun864. Original1.3.14 also
requires no provisioning. Common26/35; category formal PASS0. No new board test.
