"""Read-only S31 ET_EXEC layout check against current libelf_loadfile.

This is a build regression check, not an xTS acceptance case.  The loader
packs allocated sections separately into text/data, rounds each size to four
bytes, and then respects the next section's alignment.  Linked addresses
must match.  Data starts one reserved 4096-byte page above 0x61400000.
"""
import argparse
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('elf', type=Path)
args = parser.parse_args()
data = args.elf.read_bytes()
header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
assert header[0][:6] == b'\x7fELF\x01\x01'
assert header[1] == 2 and header[2] == 243, 'expected RISC-V ET_EXEC'
assert header[11] == 40
sections = [struct.unpack_from('<10I', data, header[6] + i * header[11])
            for i in range(header[12])]
names_section = sections[header[13]]
names = data[names_section[4]:names_section[4] + names_section[5]]
addresses = {'text': 0x61000000, 'data': 0x61401000}
mismatches = 0
allocated = 0
for section in sections:
    name, kind, flags, address, offset, size, link, info, align, entry = section
    if not flags & 2 or not size:
        continue
    allocated += 1
    name = names[name:names.index(b'\0', name)].decode()
    pool = 'data' if flags & 1 else 'text'
    align = max(align, 1)
    addresses[pool] = (addresses[pool] + align - 1) & -align
    if addresses[pool] != address:
        print(f'MISMATCH {name} linked={address:#x} '
              f'loaded={addresses[pool]:#x}')
        mismatches += 1
    addresses[pool] += (size + 3) & ~3
print(f'ELF_LAYOUT file={args.elf} allocated={allocated} '
      f'mismatches={mismatches}')
raise SystemExit(bool(mismatches))
