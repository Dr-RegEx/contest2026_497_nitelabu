# S31 basic LAN demo

A small, read-only HTTP application for the ESP32-S31 bring-up demo.
It serves `/` and `/status.json` on TCP port 8080, showing the board's IPv4
address, monotonic uptime and request count. The page refreshes every five
seconds and loads no external resources. No credentials or files are served.

For an offline console snapshot, without provisioning or starting HTTP:

```text
s31demo --status
```

An optional interface argument selects the queried interface; the default is
`wlan0`. This mode reports monotonic uptime, interface UP state, IPv4 address,
and the presence of the RGB, button, I2C, PCM and HCI device nodes. It uses a
datagram socket only for read-only interface ioctls, sends no traffic, opens no
devices and exits immediately. `STATE=PRESENT` means the device node exists,
not that playback, capture, Bluetooth or any xTS case passed. Absent nodes are
reported as `STATE=ABSENT`. Successful queries end with `S31_DEMO_STATUS=OK`;
query errors are reported explicitly with a nonzero exit status. This mode
does not start DHCP, associate Wi-Fi or change the interface state.

Build the NuttX board's `demo` configuration and flash its matching kernel
and AppFS together. This configuration uses the validated b/g/n station path;
it is not an HE, Bluetooth, Zigbee, performance or long-duration certification.

After associating with Wi-Fi and obtaining DHCP, start it from NSH:

```text
prlimit -s 8192 s31demo &
```

Open `http://<board-ip>:8080/` from a device on the same trusted LAN.
The explicit stack limit is required when AppFS stripping removes the ELF's
`nx_stacksize` symbol; otherwise the loader falls back to a 2 KiB stack.
Stop the reported process with `kill <pid>` or reset the board. The application
does not configure Wi-Fi or save credentials. The host-side
`nuttx/tools/espressif/esp32s31_demo.py` can perform interactive, redacted setup
and leave the demo running.

This is an unauthenticated, single-client demonstration, not an Internet-facing
web server. Only GET is accepted; headers are capped at 2047 bytes and socket
reads/writes have five-second timeouts. Oversized or malformed requests close
the connection. There are no upload, shell-command or storage-write endpoints.

Host regression: `python3 examples/s31demo/test_host.py` from the Apps tree.
It compiles the actual C source and tests HTML/JSON, repeated requests, method
and path rejection, and the header bound. Real board validation is separate.

LAN validation: `python3 examples/s31demo/test_http.py <board-ip>`.
This checks the HTML and ten successive JSON responses using a direct HTTP
client, without reading HTTP proxy environment settings.
Add `--samples 100 --boundaries` to also check a longer sequence, rejected
methods/paths, browser-sized headers, oversized headers and recovery after an
idle partial request. The timeout check occupies this single-client server
for approximately five seconds; use it only on an authorized test board.
