# ES8311 PCM capabilities: actual-source host check for 985

Both builds passed with AddressSanitizer and UndefinedBehaviorSanitizer.
No target firmware build, UART or network access was performed.

The harness extracts es8311_getcaps and ALSA snd_pcm_hw_query_format,
snd_pcm_hw_query_channel, snd_pcm_hw_query_rate verbatim. Audio capability
structs (including codec members), ES8311 lower config, constants, ioctl macro
and ALSA stream enum come from actual project headers. Only ioctl dispatch and
minimal enclosing device wrappers are host stand-ins. The production source
files are not modified. source-manifest.json records all extraction inputs.

PCM_CAPS disabled, SHARED_DUPLEX enabled: format query reproduces -EPERM;
channel range remains historical 1..2; rate 48000. PCM_CAPS enabled: both
capture and playback return exactly S16_LE, channel range 2..2, and 48000.
The opposite direction reports no channels/rates. Unsupported MP3 returns
AUDIO_SUBFMT_END; the enabled PCM subformat list terminates after S16_LE.
Actual ALSA headers map SND_PCM_FORMAT_S16_LE through SNDRV_PCM_FORMAT_S16_LE
to AUDIO_SUBFMT_PCM_S16_LE, so the reported identifier matches ALSA's enum.

Compile each with:

```
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter caps-source-check.c -o /tmp/s31-media-caps-off
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter -DCONFIG_ES8311_PCM_CAPS=1 caps-source-check.c -o /tmp/s31-media-caps-on
```

The unchanged ALSA query's `return i == 0 ? -EPERM : i` emits a host signedness
warning in both variants; the actual returned error was asserted successfully.
See off.log and on.log for results. This proves capability negotiation logic,
not codec/I2S playback, capture, or xTS hardware acceptance.
