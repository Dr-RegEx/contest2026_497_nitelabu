# Korvo SMP display and MEDIA integration

This profile extends `korvo-smp-media` with the standard RGB565 framebuffer,
GT1151 input device, and `s31fbcheck` application. It uses the same embedded
application ROMFS and separate `/etc` ROMFS as the MEDIA profile.

The driver reserves three page-aligned PSRAM buffers (2,310,144 bytes total).
Applications can map the staging frame with shared read/write access. They must
finish writing pixels before `FBIO_UPDATE` and keep the frame unchanged until
the call returns. Partial unmap, private mappings, and read-only mappings are
not supported. Process exit releases mappings but retains driver-owned frames.

`s31fbcheck paint SEED CPU unmap|exit` checks synthetic pixels through two
mappings and standard file I/O. A separate `verify` invocation can check them
from the other CPU. `s31mediacheck` exercises the local MEDIA service. This
configuration is an integration step toward SystemServer; it does not enable
XMS, LVGL, camera capture, or wireless services.
