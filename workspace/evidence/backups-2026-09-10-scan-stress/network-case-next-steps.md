# Published network cases after longrun864

These are category-specific cases, outside the common35-heading denominator.
No new network target tests ran during864. Prior scan-stress33 has only10
up/down cycles and20 mixed active/passive scans; it is not evidence for either
published100-operation case. The current combined883 demo already has WAPI,
Wi-Fi b/g/n, DHCP, ping and HTTP. No new firmware features are needed below.

| Priority | Published case | Exact workload | Target scheduling |
|---|---|---|---|
|1|5.1.41|ifup wlan0; ifdown wlan0,100 cycles|After combined Crypto verification, before associated demo|
|2|5.1.42|ifup wlan0; wapi scan wlan0 repeated100 times, unassociated|Same pair, fresh boot; retain AP lists and changes|

Both use a receipt-checked SMP/MMU demo pair. Require no command errors,
crashes or unexpected resets. Scan acceptance additionally requires actual AP
results and visible variation in returned entries; empty headers alone are
not RF acceptance. Keep the original100 count and do not retry failed cycles.
Time allowance roughly10–30 minutes depending on scan time and UART output;
this is an estimate, not measured performance. Exact runner timeout is30s per
command, so100 scans can take longer if the radio is slow.

Source: openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md
lines1372 and1392. Prior evidence: logs/scan-stress33-redacted.log and the
existing scan-stress.py. New runner xts-wifi-lifecycle.py is prepared only.

Throughput cases5.1.22/23/24/25 require300s streams and environment/version
records. September15 follow-up located the actual existing external/iperf2
repository (ignored by ordinary rg traversal). Its UTILS_IPERF2 application
supports the published iperf2 CLI, including UDP -b40M. The earlier audit only
found the simplified netutils/iperf; that search was incomplete. Prepare the
existing original tool without downloading or renaming the simplified app.
No throughput target PASS is claimed. Country4.1.57 getter and curl/FTP891
are build-ready; SCP candidate packaging is being completed separately.

Prepared commands, only after longrun864 exits and the chosen pair is flashed:

```sh
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-wifi-lifecycle.py ifcycle --receipt backups/2026-09-10-scan-stress/build883-ecc-point.sha256
s31-reference/.venv-nuttx/bin/python -u backups/2026-09-10-scan-stress/xts-wifi-lifecycle.py scan --receipt backups/2026-09-10-scan-stress/build883-ecc-point.sha256
```

Use fresh numbered raw logs for both. The script validates both image hashes,
radio/kernel configuration and UART vacancy; one initial reset per case,
no association and no automatic retries. Python syntax check passed; no target
execution performed. Keep AP list evidence local with the other raw logs.
