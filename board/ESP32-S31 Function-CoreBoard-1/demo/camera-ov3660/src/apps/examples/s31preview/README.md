# ESP32-S31 camera preview

`s31preview` captures real 640×480 RGB565 images from `/dev/video0` and copies
each completed frame to the center of the 800×480 RGB565 `/dev/fb0` drawing
buffer. The 80-pixel left and right margins are black. Every displayed frame
requires a successful `VIDIOC_DQBUF` followed by `FBIO_UPDATE`; there are no
synthetic replacement images.

Enable `CONFIG_EXAMPLES_S31PREVIEW=y` with the existing ESP32-S31 V4L2 camera
and framebuffer drivers. The default stack size is 8192 bytes. Both CMake and
Make discover the application through the normal examples directory rules.

```text
s31preview
s31preview --frames 120 --timeout-ms 5000
s31preview --video /dev/video0 --fb /dev/fb0 --frames 1
```

The default is 60 displayed frames and a 3000 ms capture deadline per frame.
Frame count and timeout must be integers in `1..INT_MAX`. Capture uses
`O_NONBLOCK`, `poll(POLLIN)`, and nonblocking dequeue under one monotonic
deadline; interrupted polls and readiness races do not reset that deadline.
No frame-rate control or artificial inter-frame delay is applied.

The negotiated format must remain 640×480 RGB565. Camera row stride is
honored when reported; the current fixed-format S31 bridge reports zero
`bytesperline`/`sizeimage`, for which packed RGB565 is used. The application
checks buffer identity, error flags, length and `bytesused` before reading.
Framebuffer row padding is supported, but dimensions, RGB565, 16 bpp, one
plane, zero offsets, and sufficient mapped storage are required. Pixels are
copied without byte swapping because both contracts use RGB565, not RGB565X.

Two FIFO USERPTR buffers are requested. The S31 DVP layer allocates its own
DMA-capable buffers and copies completed images into the queued user buffer;
therefore application buffers use ordinary `malloc`, not assumed DMA memory
or the unrelated camera example's 32-byte alignment requirement. Dequeued
buffers remain owned by the application until copying and presentation
finish, then are requeued. Shutdown attempts STREAMOFF and REQBUFS(0), closes
the camera, and only then frees user buffers. In the current capture
framework REQBUFS(0) is a no-op; close is still required to release resources
and drain the bridge's deferred copy. In FLAT builds the framebuffer address is obtained directly from the driver;
it has no MMU mapping to release. In kernel builds mmap/munmap are paired.
The framebuffer descriptor is closed on both success and failure.

Each successful update prints an application frame number, buffer index,
capture timestamp, elapsed monotonic time, `bytesused`, and FNV-1a checksum
over the active camera pixels (excluding row padding). The application frame
number is not a fabricated driver sequence: the current bridge does not
populate V4L2's sequence field. `update=OK` confirms driver completion; optical
appearance must still be checked on the panel. Identical checksums can be
valid for a stationary image and do not by themselves prove stale capture.

Any capture timeout, invalid layout/frame, presentation failure, or cleanup
failure yields a nonzero exit status. The timeout bounds capture readiness;
it does not impose a separate deadline on driver ioctl/close operations or
on the framebuffer driver's own update synchronization.

Implementation was checked against the board's `esp32s31_camera_v4l2.c`, the
DVP copy/ownership implementation, `v4l2_cap.c`, and `s31rgb565`. Korvo + OV3660 board bring-up evidence is recorded in
`backups/2026-09-20-korvo/README.md` at the workspace root; optical confirmation
is recorded separately from successful framebuffer updates.
