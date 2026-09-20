# Candidate 985: original mediatool audio preparation

Status: host build preparation only. No UART, flash, reset, provisioning,
original-resource playback or listening result is claimed by this checkpoint.
The existing long-run image and frozen 968/979/980 outputs remain unchanged.

Profile: `esp32s31-core-function-board:xts-flat-media`, independent output
`openvela-dev/out/esp32s31-xts-flat-media`. This inherits the default-off 979
48 kHz / S16LE / two-slot duplex transport. Physical ES8311 ADC/DAC is mono.
Original 4.1.128 MP3, 4.1.129 Opus and 4.1.130 AAC are the first target cases.
4.1.131 WAV remains prepared but requires an intact-resource storage solution:
the original approximately 17 MiB WAV exceeds the available 16 MiB PSRAM and
16 MiB Flash individually. Cropping or transcoding is not equivalent evidence.

The native media server, mediatool, policy framework and direct file KV backend
run on one CPU with local AF_LOCAL sockets. IPv4, IPv6, radio and SMP are off.
NETDEV_LATEINIT suppresses a nonexistent early physical network initializer;
no network interface is required for local media IPC. Graph/runtime files are
staged manually under `/data/media`, and `/tmp/media-kv` must exist before
starting mediad. The daemon is not started by the boot script.

The graph routes Music through the existing `asubgraph@Convert`, whose native
`<subgraph>` section contains aresample to 48 kHz / S16LE / stereo transport.
A plain aresample on the top-level route lacks the map_array negotiation API.
The asubgraph map-array count was moved adjacent to its pointer, a mistaken
assignment in EAGAIN handling corrected, and true drain/EOF submission errors
propagated. The existing filter Makefile now includes its amix dependency.
ES8311_PCM_CAPS opt-in supplies actual S16LE and exact two-channel transport
caps to ALSA. Default-off codec preprocessing matches frozen968.

FFmpeg uses native AAC, MP3 float, Opus and PCM S16 decoders; MOV (the original
AAC is MP4), AAC, MP3, Ogg and WAV demuxers; file protocol; abufsrc/asubgraph,
abuffer/abuffersink, aresample and alsasink. Unused double/integer TX backends
are disabled, preserving the float transforms required by AAC and Opus.

The default-off LIB_FFMPEG_AUDIO_TABLEGEN option runs existing host generators
for AAC PS, cube root, sine windows, MPEG audio, shared MPEG audio and PCM
headers in the independent output. Hardcoded tables move immutable data from
internal BSS into Flash rodata. The MP3 generator uses fixed arrays only under
BUILD_TABLES; normal runtime allocation behavior is unchanged. Both generated
shared MPEG arrays have 32828 values, matching `(8191 + 16) * 4`. PCM generator
calls/output follow the existing decoder/encoder conditions. No codec algorithm,
HAL, IDF, linker RAM size or external-BSS mechanism was changed.

Necessary build integration repairs: map NuttX `risc-v` to FFmpeg `riscv`;
respect DEPCONFIG in the board Make.defs for external CMake-hosted Make builds;
compile pcm_dmix only when selected (HW-only ALSA must not require unavailable
Speex); match existing int32_t DSP declarations on RV32 where int32_t is long;
use uint32_t for WAV chunk tags. Existing source changes are preserved.

See the frozen build result for exact image hash/size, BSS and internal SRAM
remainder. A successful link is not a boot or playback guarantee: the remaining
internal SRAM must still support early allocation before PSRAM joins the heap,
and target decoder/task/graph memory must be checked before staging resources.
Follow media-target-sequence-985.md; preserve full original resources, hashes,
initialization, prepare/start/close diagnostics, completion and listening result.
