# Timer differential audit 641–642

Previous turn only restated existing results (no progress). This group adds
actual-build preprocessing evidence; no firmware changes or hardware reset.
NuttX HEAD 8fb1462ee68 and existing six tracked diagnostic changes preserved.
git diff --check passed. No reference changes or dependency downloads.

Command: python3 backups/2026-09-10-scan-stress/audit-timer641.py
Output: logs/host641-timer-audit.log. Extraction completed with PASS marker.
Each compiler subprocess uses check=True, recorded compile_commands.json,
removes output/dependency flags, and emits only preprocessor stdout. NuttX
explicitly preincludes build635 config rather than source-tree Make config.
Full commands, preprocessed hashes and ELF hashes are recorded. This is not
a recompile/link, hardware time measurement, or proof of HE correctness.

Actual PHY/systimer translation units agree between current635 and IDF173:
- PLL tracking period 1000 ms, converted to 1000000 us by PHY source.
- systimer_ticks_to_us divides by16; systimer_us_to_ticks multiplies by16.
- counter0/alarm2; fixed divider enabled in HR/IDF systimer implementation.
- OS tick NuttX10000us, IDF100Hz, both10ms.
This does not support a wrong timer unit/divider configuration hypothesis.

Source differences requiring bounded interpretation:
- IDF esp_timer_start_periodic rejects an armed timer; current PHY wrapper
  restarts it and returns success. IDF stop of inactive timer returns invalid
  state; wrapper returns success. PHY common code inspected uses create/start
  and stop/delete, so no demonstrated triggering call sequence or HE cause.
- IDF init_timer_task explicitly pins CPU0. NuttX HR init uses kthread_create
  without explicit affinity. sched/task/task_setup.c inherits creator affinity
  (tcb->affinity = rtcb->affinity), so actual HR CPU eligibility must be checked
  before treating it as a difference; sdkconfig affinity0 alone does not bind
  the NuttX HR worker. Next useful check: actual worker affinity/callback CPU,
  then controlled diagnostic if different, not blind priority/clock changes.

Command: python3 openvela-dev/nuttx/tools/test_esp_hr_timer_lifecycle.py
Output: logs/host642-timer-lifecycle.log. Exit0:
ESP_HR_TIMER_LIFECYCLE=PASS periodic/delete/restart/cancel.
This exercises actual worker/stop functions with host mocks, not real SMP/ISR.

Read-only search errors: hal_backports/include/sdkconfig.h does not exist;
correct file is esp32s31/include/sdkconfig.h. sched/sched/sched_setup.c does
not exist; inheritance found using rg in sched/task/task_setup.c. No source
conclusions rely on those missing paths.

No build, flash, serial or network session launched. Last verified board635
demo638/HTTP639 remain the handoff, not freshly tested connectivity this group.
No code commit warranted for an audit-only result. Full HE and other subsystem
acceptance remain open. Prior recoverable firmware/archive progress632-640
is unchanged; this note, audit script and logs are separately checksummed.
