#!/usr/bin/env python3
"""Generate the packaged 8 s, 16 kHz stereo synthetic tone deterministically.

Integer triangle waves, 500/1000 Hz, peak 768/32768 with a 16 ms fade.
This asset contains no recorded sound and requires no writable media at runtime.
"""
from pathlib import Path
import struct
import wave


def main():
    frames = 16000 * 8
    pcm = bytearray()
    for frame in range(frames):
        gain = min(frame, frames - frame - 1, 256)
        left = (abs(frame % 32 - 16) - 8) * 96 * gain // 256
        right = (abs(frame % 16 - 8) - 4) * 192 * gain // 256
        pcm.extend(struct.pack('<hh', left, right))
    with wave.open(str(Path(__file__).with_name('demo-tone.wav')), 'wb') as output:
        output.setparams((2, 2, 16000, frames, 'NONE', 'not compressed'))
        output.writeframes(pcm)


if __name__ == '__main__':
    main()
