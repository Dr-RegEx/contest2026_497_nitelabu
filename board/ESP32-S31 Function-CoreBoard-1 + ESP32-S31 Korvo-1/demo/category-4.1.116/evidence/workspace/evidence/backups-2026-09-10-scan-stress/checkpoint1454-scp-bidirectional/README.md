# SCP original cases 1453 / 1454 PASS

Firmware: paired1444, flashed1444, boot1445, saved-network reconnect1446. 4.1.115 download and4.1.116 upload each exited0 with65536byte source and destination; roundtrip byte comparison and SHA256 match. Standard interactive scp, verified peer fingerprint, password authentication. Host is Windows192.168.1.29:22 relay to dedicated WSL asyncssh server172.31.208.211:2222, no remote shell enabled. Board /data uses existing tmpfs mounted1447. No private credentials included.

Repair: enable Mbed TLS pthread config for libssh default thread callbacks; include vendor builtin threading.c in both ESP32-S31 wireless source lists. Retains1427 seed-error/init-error handling. No bypass of ssh_init, entropy or mutexes. CMake full build validated; Make source list updated but Make build not run.

Failures retained:1440 link missing threading implementation;1447 peer ARP failure;1450 extra LF consumed by interactive host-key prompt;1451 script expected trailing space after Password: and did not send password.1452 empty newline recovered timed-out prompt without reset.1453 uses single command CR and matches Password: without mandatory trailing space. Expected automatic public-key fallback is followed by successful password authentication and is not transfer failure.

Progress: common30/35, selected category38/53, total68/88=77.3%. Original1428 snapshot retained.
