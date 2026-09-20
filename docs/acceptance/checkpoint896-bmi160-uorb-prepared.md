# BMI160 uORB candidate 896

Full offline build PASS; TARGET NOT RUN. Kernel350556 bytes. Separate
xts-flat-bmi160-uorb profile, original listener and both registered topics.
Shared board Kconfig/CMake/Make/header/bringup and helper profile guards are
integrated. Character887 remains separate. See bmi160-uorb-preparation.md
for driver fixes, source-only checks, wiring, original commands and limits.

The original4.1.119 sequence is implemented in xts-peripheral.py bmi160_uorb:
25/50/100Hz, ten complete rounds, total10 messages each invocation and both
topics must have positive receipt counts. Actual topic data is uncalibrated
raw axis counts as in the previous driver; unused event fields are initialized,
not measured. The exact listener summary prints independently of UORB_INFO.
The profile's NSH80-character line capacity accommodates both topic names.

No board reset, flash or fixture test took place. Longrun864 stayed active.
Physical run requires the documented BMI160 wiring and receipt-checked image.
