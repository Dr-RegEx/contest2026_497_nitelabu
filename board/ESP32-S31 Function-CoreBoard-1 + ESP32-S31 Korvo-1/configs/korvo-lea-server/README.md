# Korvo LE Audio and MEDIA service configuration

This single-core FLAT configuration combines the ZBlue unicast server backend,
Bluetooth audio transport, LC3 and the board MEDIA graph. Controller memory and
task stacks use internal SRAM; the separate application heap uses PSRAM. Wi-Fi
and automatic radio procedures are disabled in this profile.

From the integration workspace, build into a new output directory:

```sh
python3 tools/tests/korvo-adc-keys/build.py --workspace-source \
  --profile korvo-lea-server --output /tmp/korvo-lea-build --jobs 4
```

The profile uses the normal maintained sources and `/etc/media-lea/` graph.
KVDB must be available before Bluetooth service initialization. MEDIA and
Bluetooth service startup remain explicit. The build includes no synthetic
audio injection command or private PCM measurement extension.

Applications can use `bt_lea_server_receive_control()` to pause, resume, mute
or unmute the shared local receive pipeline. In an initialized Bluetooth tool
session, the corresponding commands are `leas receive pause`, `resume`,
`mute` and `unmute`. A successful submission only queues the operation; its
callback (or the tool's completion message) reports the actual result. Keep
the Bluetooth event loop and callback context alive until completion.

Pause and mute are independent. Clearing one does not clear the other; packets
received while either is set are discarded. A fully stopped stream cannot be
resumed this way. Commands belong to the current service generation and are
rejected after a service restart. This API controls the shared local software
receiver; it does not send a remote media-player command or control one ASE.

Local synthetic receive, pause/resume and task shutdown have been exercised
in an earlier instrumented image. That evidence does not qualify this newer
control API on hardware, wireless interoperability or acoustic output. Remote
media control, additional LE Audio service roles, actual CIS/BIG peers and SMP
product coexistence remain integration work.

Bluetooth and MEDIA must be rebuilt together when updating their shared
audio control protocol. This profile does not promise compatibility with a
mixture of independently built service versions.
