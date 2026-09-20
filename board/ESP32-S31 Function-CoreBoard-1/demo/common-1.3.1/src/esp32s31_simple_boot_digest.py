#!/usr/bin/env python3
#
# SPDX-License-Identifier: Apache-2.0
#
"""Add the ROM-visible digest to an ESP32-S31 simple-boot image.

esptool's --ram-only-header layout puts the XIP segment records immediately
after the checksum of the ROM-visible RAM segments and therefore suppresses
the normal image digest.  ESP32-S31 ROM nevertheless verifies a digest at
that position.  Insert it while shortening the following alignment segment,
so every XIP segment remains at its original flash address.
"""

import argparse
import hashlib
import struct
from pathlib import Path


IMAGE_HEADER_SIZE = 24
SEGMENT_HEADER_SIZE = 8
HASH_APPENDED_OFFSET = 23
DIGEST_SIZE = 32


def visible_image_end(image: bytearray) -> int:
    """Return the byte after the aligned checksum of visible RAM segments."""

    if len(image) < IMAGE_HEADER_SIZE or image[0] != 0xE9:
        raise ValueError("not an ESP image")

    offset = IMAGE_HEADER_SIZE
    for _ in range(image[1]):
        if offset + SEGMENT_HEADER_SIZE > len(image):
            raise ValueError("truncated segment header")
        _, data_len = struct.unpack_from("<II", image, offset)
        offset += SEGMENT_HEADER_SIZE + data_len
        if offset > len(image):
            raise ValueError("truncated segment data")

    # The checksum occupies the final byte of a 16-byte block.  If segment
    # data already ends on a 16-byte boundary, esptool emits a complete new
    # padding/checksum block rather than placing the checksum before it.

    return (offset + 16) & ~15


def add_digest(path: Path) -> None:
    image = bytearray(path.read_bytes())
    digest_offset = visible_image_end(image)

    if image[HASH_APPENDED_OFFSET] == 1:
        stored = image[digest_offset:digest_offset + DIGEST_SIZE]
        calculated = hashlib.sha256(image[:digest_offset]).digest()
        if stored != calculated:
            raise ValueError("existing image digest is invalid")
        return

    hidden_header = digest_offset
    if hidden_header + SEGMENT_HEADER_SIZE > len(image):
        raise ValueError("missing hidden-segment alignment record")

    load_addr, padding_len = struct.unpack_from("<II", image, hidden_header)
    if load_addr != 0 or padding_len < DIGEST_SIZE:
        raise ValueError("first hidden record is not a usable padding segment")

    padding_start = hidden_header + SEGMENT_HEADER_SIZE
    padding_end = padding_start + padding_len
    if padding_end > len(image):
        raise ValueError("truncated hidden padding segment")

    removed = image[padding_end - DIGEST_SIZE:padding_end]
    if any(byte not in (0x00, 0xFF) for byte in removed):
        raise ValueError("hidden padding tail contains non-padding data")

    image[HASH_APPENDED_OFFSET] = 1
    digest = hashlib.sha256(image[:digest_offset]).digest()
    shortened_header = struct.pack("<II", load_addr,
                                   padding_len - DIGEST_SIZE)
    output = (image[:digest_offset] + digest + shortened_header +
              image[padding_start:padding_end - DIGEST_SIZE] +
              image[padding_end:])

    if len(output) != len(image):
        raise ValueError("image size changed while inserting digest")

    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(output)
    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    add_digest(args.image)


if __name__ == "__main__":
    main()
