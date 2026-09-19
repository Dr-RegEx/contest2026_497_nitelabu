# Host network fixtures (961/962)

These are host preparations, not category PASS evidence. Do not open UART or
flash while longrun 864 owns the board. Use candidate 904 or paired demo 911
after the longrun timing audit and queued common tests.

## Locations

- Windows directory: `C:\Users\tttgu\Documents\Codex\s31-host-tools-2026-09-16`
- Native Python: `C:\msys64\ucrt64\bin\python.exe`
- Native iperf: `iperf2.exe` in the Windows directory; version 2.1.8.
- ADB: `platform-tools\adb.exe`; version 37.0.1.
- Linux evidence directory: this document's directory, abbreviated D below.
- Linux SCP interpreter: `D/host-ssh-venv/bin/python`.

Read the current Windows WLAN IPv4 and WSL eth0 IPv4 immediately before
execution. At preparation they were 192.168.1.29 and 172.31.208.211, respectively;
these are observations, not fixed configuration. No firewall rules or system
services were installed. LAN reachability remains to be checked with the board.

## Original unicast throughput

Keep both endpoints' complete output and original 300-second duration.
Use the original case-specific flags; the port assignments below keep the
four runs distinguishable. UDP offered load 40M is not a promised result.

| Original case | Board role | PC command |
|---|---|---|
| 5.1.22 TCP RX | `iperf2 -s -p 5001` | `iperf2.exe -c BOARD_IP -p 5001 -t 300 -i 1` |
| 5.1.23 TCP TX | `iperf2 -c PC_IP -p 5002 -t 300 -i 1` | `iperf2.exe -s -B PC_IP -p 5002 -i 1` |
| 5.1.24 UDP RX | `iperf2 -s -u -p 5003` | `iperf2.exe -c BOARD_IP -u -p 5003 -b 40M -t 300 -i 1` |
| 5.1.25 UDP TX | `iperf2 -c PC_IP -u -p 5004 -b 40M -t 300 -i 1` | `iperf2.exe -s -u -B PC_IP -p 5004 -i 1` |

961 passed one-second TCP/UDP Windows loopback only. Multicast is disabled in
this host build. Record actual throughput, loss, router, distance and RF setup.
Stop each owned server after its run.

## HTTP / FTP

Serve only `http-fixture`, using native Python's `-m http.server 8000
--bind PC_IP --directory <Windows directory>\http-fixture`.
Use `/xts-transfer-64KiB.bin` for the original HTTP file-download case and verify
65536 bytes against host-network-64KiB-fixture.json. The original web-content case
still uses its specified public website; this fixture does not replace it.
For FTP, start the board's original `ftpd_start -4 &` and run PC get/put as
specified in the original case. WSL clients can connect outbound to board LAN.

FTP source audit: the compiled example registers `ftp` with an empty password
and HOME defaults to `/`. Use `ftp`, not Python's default anonymous login:
the latter supplies a nonempty password which this server rejects. Passive
mode lets the WSL client initiate both connections through NAT. The candidate
already enables FTPD and TCP backlog; no firmware change is needed for this.

After the HTTP fixture exists at `/data/xts-transfer-64KiB.bin`, check space for
one additional 64 KiB file and ensure `/data/xts-ftp-upload.bin` is unused. Run
the following from D, replacing BOARD_IP with the observed address. Preserve
the complete client output and board log. This is the original login/list/get/
put/quit flow plus a content comparison, not additional acceptance coverage.

```python
from ftplib import FTP
from pathlib import Path
import hashlib

fixture = Path("host-scp-fixture/xts-transfer-64KiB.bin")
expected = "7daca2095d0438260fa849183dfc67faa459fdf4936e1bc91eec6b281b27e4c2"
assert hashlib.sha256(fixture.read_bytes()).hexdigest() == expected
ftp = FTP(timeout=60)
print(ftp.connect("BOARD_IP", 21))
print(ftp.login("ftp", ""))
ftp.set_pasv(True)
print(ftp.cwd("/data"))
ftp.retrlines("LIST")
with Path("ftp-board-download.bin").open("xb") as output:
    print(ftp.retrbinary("RETR xts-transfer-64KiB.bin", output.write))
with fixture.open("rb") as source:
    print(ftp.storbinary("STOR xts-ftp-upload.bin", source))
with Path("ftp-upload-readback.bin").open("xb") as output:
    print(ftp.retrbinary("RETR xts-ftp-upload.bin", output.write))
print(ftp.quit())
for name in ("ftp-board-download.bin", "ftp-upload-readback.bin"):
    data = Path(name).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    print(name, len(data), digest)
    assert len(data) == 65536 and digest == expected
```

The snippet is prepared only, not executed against the board. Choose fresh
host output names for a repeat run; retain failed outputs before retrying.
Remove only this run's board transfer files after collecting evidence. Check
board logs for faults before marking original case 4.1.117 passed.

## Original WAPI persistence and reconnect

The original904/911 profiles do not enable WIRELESS_WAPI_INITCONF, so they
cannot qualify4.1.28/30/29. The updated competition profile adds the existing
cJSON dependency and WAPI save_config/reconnect commands. Its configured path
is `/apps/s31-wapi-xts.conf`, on the existing writable volume. This is a single
configuration file, not a disposable filesystem: never format `/apps`.

After the new paired image passes boot and association, check that this path
does not already contain user data before saving. Run these original cases in
order, with the authenticated WPA2 association established privately:

1. 4.1.28: `wapi save_config wlan0`, then inspect the saved JSON. Compare its
   SSID/auth/cipher/PSK with the actual association, without copying secrets
   into UART evidence or archives. The published path is explicitly variable;
   record this board's actual path. Do not accept a zero shell status if the
   file is absent, truncated or contains incorrect settings.
2. 4.1.30: `wapi save_config wlan0`, `wapi reconnect wlan0`, `renew wlan0`,
   `ifconfig`, and ping the actual DHCP gateway. Retain connection/DHCP/ping
   output and reject command errors. No reboot is required for this case.
3. 4.1.29: save configuration, `reboot`, then `ifup wlan0`,
   `wapi reconnect wlan0`, `renew wlan0`, and ping the DHCP gateway. Preserve
   the boot log and prove the saved configuration survived; do not re-enter
   credentials after reboot or reflash between save and reconnect.

Do not print the file with the ordinary raw UART logger enabled. Capture its
contents only in memory for comparison, then persist redacted field checks.
Only this run's explicitly created configuration may be removed after evidence
is complete. These cases remain TARGET NOT RUN; enabling the commands is not
persistence or reconnect acceptance.

## Original SCP 4.1.115 / 4.1.116

The board's original scp command has no port flag, so the PC endpoint uses 22.
Start the dedicated Linux peer using the current WSL address:

```sh
D/host-ssh-venv/bin/python D/host-scp-peer.py --bind WSL_IP --port 2222 --root D/host-scp-fixture --private D/host-ssh-private
```

Start native Windows Python with `host-tcp-relay.py --bind PC_IP --port 22
--target WSL_IP --target-port 2222`. The relay forwards TCP bytes to the real
SSH server; it does not implement or replace SCP. Only the dedicated fixture
root is exposed. No interactive remote shell or OS account is created.

Use username `xts`. The generated password and host key remain in the private
0700 directory, outside all archives. Feed the password locally to the board's
interactive prompt without recording it. Compare the host key before accepting
the first connection. The original helper asks separately whether to save the
key on disk: press Enter at that second prompt, as the published case does,
because the default home directory is the pseudo-filesystem root. The first
trust prompt still requires a matching fingerprint and explicit yes. Do not print credentials in logs or put them on a command
line. The board-visible absolute path `/` is the fixture root.

```text
scp xts@PC_IP:/xts-transfer-64KiB.bin /data/xts-transfer-64KiB.bin
scp /data/xts-transfer-64KiB.bin xts@PC_IP:/from-board.bin
```

Use actual `/data`, not the original example's `/dev/data` typo. Retain original
transfer diagnostics, compare file sizes in both directions, and compare hashes
on the PC where available. Host-only legacy SCP upload/download of 1MiB passed
with identical content; Windows relay and target interoperability are pending.
Stop the owned relay/peer after execution.

## USB ADB

The package was downloaded from the official Android platform-tools page and
its ZIP CRC/version checked. No target enumeration has occurred. Follow
`usb-adb-target-sequence.md` and verify J4 VBUS isolation before connecting;
J1/J3 are not the native ADB device port. Windows driver binding is unverified.

964 host validation: Windows loopback TCP relay to the WSL peer completed
a real OpenSSH RSA host-key handshake, with the public key matched to the
dedicated peer. Both temporary processes stopped. This establishes the host
forwarding path only; board/LAN endpoint reachability and SCP remain pending.

Target capacity:904/911 use FS_HEAPSIZE=0, so the host-only1MiB verification
file is not the planned RAM target payload. The original HTTP/FTP/SCP cases
do not prescribe a fixed size. Use the disclosed64KiB fixture, check `free`
and mounts first, and use a fresh disposable /data tmpfs only when memory
allows. Do not overwrite existing /data mounts or user files. Preserve
transfer results before removing this task's temporary files. No performance
claim or large-file qualification follows from these functional transfers.
