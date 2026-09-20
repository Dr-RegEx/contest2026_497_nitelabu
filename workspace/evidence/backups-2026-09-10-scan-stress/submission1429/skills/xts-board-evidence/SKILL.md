---
name: xts-board-evidence
description: Run or review openvela xTS on an existing development board while preserving firmware pairing, serial ownership and original acceptance evidence. Use for interrupted board tests, test handovers or xTS result classification.
---

# Existing-board xTS evidence

Start from the user's selected cases and hardware. This workflow preserves an existing port and its test history; it does not select the project's product scope or authorize network provisioning, reset, flashing or publication beyond the current request.

## Resume before acting

Locate the current case list, active-work record, raw log, script and firmware receipt. Treat the active-work JSON as a locator, not proof of a live process. Poll its session or inspect the actual process and latest output. A timed observation is not a failed board. Do not restart a live long test to restore host logging.

Check the previous acceptance records before adding work. A PASS may belong to a different configuration. Reuse that evidence for its actual scope; do not infer that the currently loaded image contains all previously tested drivers. A subsequent fix justifies the relevant regression, not automatically a full repeat of the old suite.

## Associate evidence with artifacts

Run `scripts/verify_receipt.py RECEIPT --base WORKSPACE` against a SHA-256 receipt. Relative paths resolve from `--base`; absolute paths remain absolute. This reads artifacts only. An OK result proves file identity, not that those files are currently running.

For kernel/AppFS builds verify both files from the same build. Establish the target's loaded pair from the flash/boot evidence before testing. Frozen images remain usable after source edits; a build-only candidate is not target acceptance. Retain config, source revision or patch, original command and hardware conditions alongside results.

## Execute the original case

Read the specific original steps and expected result. Keep its workload, loop count and parameters unless the original allows adjustment or the user explicitly changes the acceptance. Do not invent performance thresholds, continuous logging requirements or extra cases.

Use a single serial owner. On an already running board, opening a serial port may toggle reset lines: reuse a transport known to preserve those lines and take its exclusive lock. Never flash or send commands through another transport while the test owns the UART.

When a test legitimately expects reset, crash, unreachable network or EOF, evaluate that step's expected behavior. Generic words such as `failed` may describe expected EOF; inspect source and protocol state before overruling an existing failure label. Retain both the raw result and any corrected interpretation. Conversely, compiler success, an empty driver-test branch or a normal shell prompt do not establish real hardware exercise.

Use credentials only in the authorized transport with redacted capture. Test logs and submission artifacts must not contain a passphrase. Do not upload private credentials or unselected AI sessions as part of evidence packaging.

## Classify without erasing gaps

Record outcomes separately when needed:

- Original workload and target command result.
- Numeric performance or statistical acceptance, including unavailable outputs.
- Human listening, physical power-cycle or external-fixture acceptance.
- Integration scope: single image, separate profiles or untested coexistence.

A measurement with unspecified acceptance remains a measurement. A missing statistical output is not a passing output. A sensor compilation is not a physical reading. A reset command is not a cold power cycle. Playback reaching its completion event is not listening acceptance when the case requires sound quality.

Archive raw logs without rewriting them. Use a new result identifier for new executions. Add a concise review linking old and new evidence; update the per-case row and count only complete cases once. For duplicate original numbers use the source heading location as identity. A selected milestone percentage must name its denominator and must not replace the full project goal.

## Report the next action

After each completed test or fix, state its result, evidence and material remaining limitation. When a test is live, continue independent preparation that does not change its board, image or serial stream. If physical cooperation is required, prepare the concrete operation and ask for only the missing condition; retain the broader goal.
