# Wi-Fi interrupt-mask correctness, 2026-09-14

Previous group progressed:600 b/g/n DNS/TCP/UDP3/3 success;603 HE3/3 failure
on sameAP despite DHCP/HE20, restored604/demo606/HTTP607 and sealed607.
Starting N HEAD f10a8f4b209, Apps ae0dfd89c; six diagnostic WIP preserved.

Audit found enable_intr_wrapper/disable_intr_wrapper in shared C6/S31 Wi-Fi
adapter use only ffs(mask)-1. Zero gives -1 to esp_get_irq, which directly
indexes g_cpuint_map. Multibit masks silently omit all except lowestbit.
Locked IDF components/esp_wifi/esp32s31/esp_adapter.c passes complete masks
to esprv_int_enable/disable; HAL wifi_os_adapter.h declares uint32 masks.
S31 ESP_NCPUINTS=32. Fix loops over every setbit, clearing lowestbit each
iteration, leaving zero a no-op. Existing per-interrupt NuttX mapping and
enable state bookkeeping preserved; this does not implement arbitrary
unmapped IRQ support or atomic simultaneous multibit hardware updates.

New tools/test_esp_wifi_irq_mask.py extracts actual wrapper functions;
models valid mapped IRQs and checks zero/all32singlebits/all496pairs/allbits/
alternatingbits/10000 deterministic masks. Lookup and enable/disable each
exactlyonce; no unintended bits. Runs C6 and S31 preprocessor variants with
UBSan, not real multicore hardware. Tests/commands from workspace root:

python3 openvela-dev/nuttx/tools/test_esp_wifi_irq_mask.py
  logs/host608-irq-mask.log exit0, both variants PASS.
python3 openvela-dev/nuttx/tools/test_esp_wifi_irq_mask.py --revision HEAD
  logs/host608-irq-mask-before.log exit1 as expected; oldf10 fails cpuint>=0
  assertion on zero. This is an intended negative test, not a build failure.
tools/nxstyle -r 1036,59 arch/risc-v/src/esp32c6/esp_wifi_adapter.c
  from NuttX, exit0; git diff --check PASS.

Staged only non-diagnostic wrapper changes via irq-mask608-index.patch
(git apply --cached --check before application), plus new test. No staged
content existed before. Actual staged tree addba3e362719604278d74fef8ed226c46f0986f
tested using --revision <tree>, logs/host611-irq-mask-index.log exit0.
No existing Wi-Fi/SMP counters are being committed.

D=this directory. S31_DEMO_PROFILE=demo-rmt bash D/build-demo.sh
D/build609-irq-mask.sha256 > D/logs/build609-irq-mask.log 2>&1
Exit0, first real build error:none, config/ELF protocol7. Existing HAL warnings
remain; full configure/compiler commands logged; source Make config restored.
bash D/flash-demo-pair.sh D/build609-irq-mask.sha256
> D/logs/flash610-irq-mask.log 2>&1: exit0, verifiedbackup/hash/size/ELF guards;
only0x2000 and0x200000 written, no data>=0x500000 or efuses.

S31_EXPECT_PROTOCOL=7 S31_TEST_BSSID=60:ce:41:ab:02:d0
R/.venv-nuttx/bin/python -u D/network-repeat.py network611-irq-mask tcp-udp-dns
(R=workspace/s31-reference). Exit0, BATCH_RESULTS=[0,0,0]. Each round scan,
DHCP/gateway/DNS/TCP4096/UDP256x96bytes and normal ifdown/reset cleanup PASS.
Total UDP768packets; no injected multibit masks on hardware, so board checks
are regression coverage of the existing mask0x2 path, not proof of physical
multivector handling. The latter has actual-wrapper host coverage only.

Commit f65fe96c6d2: risc-v/espressif: handle complete Wi-Fi interrupt masks.
Only two files,16insertions/8deletions adapter and97line test; six previous
diagnostic edits remain unstaged. Stagedtree test excludes those diagnostics.
Demo restart command R/.venv-nuttx/bin/python -u
openvela-dev/nuttx/tools/espressif/esp32s31_demo.py --port /dev/ttyUSB0
--log D/logs/demo612-irq-mask.log: exit0, http://192.168.1.60:8080/.
Windows PowerShell invokes existing Windows cachedPython and
openvela-dev/apps/examples/s31demo/test_http.py 192.168.1.60
--samples 10 --boundaries. logs/http613-irq-mask.log exit0;10samples and
method/path/browser-headers/header-limit/idle-recovery all PASS.
DemoPID12 remains connected, no active build/flash/serial/HTTP process.
checkpoint613.sh seals firmware609, commitbundle f10..f65, diagnosticpatch,
logs/test receipts/scripts plus prior607 checksum link; SHA256SUMS-progress613.
Current PD images match build609-irq-mask.sha256, not older604 receipt.

Important limits:603 observed masks all0x2, so this fix is not established as
HE root cause. set_intr_wrapper still ignores cpu_no, routing helper configures
callinghart; up_enable/disable are localCLIC operations. CPU ownership and
cross-hart synchronization need a separate design/review, not blind SMP-call
insertion that can deadlock under Wi-Fi critical sections. No router/TUN/
firewall/IDF/reference/dependency changes. Board now runs609 resident demo612.
