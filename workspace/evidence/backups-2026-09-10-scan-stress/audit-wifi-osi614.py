"""Read actual ELF32 Wi-Fi OSI tables with each build's preprocessed ABI.

No source/build writes or third-party Python dependency. This verifies table
layout and link targets, not runtime callback correctness or HE acceptance.
"""
import hashlib
import json
from pathlib import Path
import re
import shlex
import struct
import subprocess

ROOT = Path('/home/regex/work/esp32s31-openvela')


def elf_table(path):
    data = path.read_bytes()
    assert data[:7] == b'\x7fELF\x01\x01\x01', 'expected ELF32 little endian'
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    assert header[2] == 243, 'expected RISC-V'
    shoff, shsize, count = header[6], header[11], header[12]
    assert shsize == 40 and shoff + shsize * count <= len(data)
    sections = [struct.unpack_from('<10I', data, shoff + index * shsize)
                for index in range(count)]
    symbols = []
    for section in sections:
        if section[1] != 2:
            continue
        assert section[9] == 16 and section[5] % 16 == 0
        strings_section = sections[section[6]]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        for offset in range(section[4], section[4] + section[5], 16):
            name, value, size, info, other, index = struct.unpack_from('<IIIBBH', data, offset)
            name = strings[name:strings.index(b'\0', name)].decode()
            symbols.append((name, value, size, info & 15, index))
    targets = [entry for entry in symbols if entry[0] == 'g_wifi_osi_funcs']
    assert len(targets) == 1
    name, address, size, kind, index = targets[0]
    section = sections[index]
    offset = section[4] + address - section[3]
    assert kind == 1 and size % 4 == 0
    assert section[4] <= offset and offset + size <= section[4] + section[5]
    assert offset + size <= len(data)
    words = struct.unpack_from('<' + 'I' * (size // 4), data, offset)
    functions = {}
    for name, value, _, kind, _ in symbols:
        if kind == 2:
            functions.setdefault(value, []).append(name)
    print('ELF_SHA256=' + hashlib.sha256(data).hexdigest())
    return words, functions


def fields(build, suffix, nuttx):
    entries = json.loads((build / 'compile_commands.json').read_text())
    matches = [entry for entry in entries if entry['file'].endswith(suffix)]
    assert len(matches) == 1
    entry = matches[0]
    original = iter(shlex.split(entry['command']))
    command = []
    for arg in original:
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            next(original)
        elif arg not in ('-c', '-MD', '-MMD', '-MP'):
            command.append(arg)
    command.extend(('-E', '-P'))
    if nuttx:
        command[1:1] = ['-include', str(build / 'include/nuttx/config.h')]
    print('PREPROCESS_COMMAND=' + shlex.join(command), flush=True)
    result = subprocess.run(command, cwd=entry['directory'], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            check=True)
    if result.stderr:
        print(result.stderr)
    match = re.search(r'typedef struct wifi_osi_funcs_t\s*\{(.*?)\}\s*wifi_osi_funcs_t;',
                      result.stdout, re.S)
    assert match, 'preprocessed ABI missing'
    names = []
    for declaration in match[1].split(';'):
        if not declaration.strip():
            continue
        name = re.search(r'\(\s*\*\s*(_\w+)\s*\)', declaration)
        if name is None:
            name = re.fullmatch(r'\s*int32_t\s+(_version|_magic)\s*', declaration)
        assert name, declaration
        names.append(name[1])
    assert names[0] == '_version' and names[-1] == '_magic'
    return names


def main():
    previous = None
    for label, relative, suffix, binary in (
        ('nuttx-current', 'openvela-dev/out/esp32s31-cmake-demo',
         '/esp32c6/esp_wifi_adapter.c', 'nuttx'),
        ('idf173', 'diagnostics/idf-baseline/build-hal-wifi',
         '/esp32s31/esp_adapter.c', 's31_wifi_baseline.elf'),
    ):
        print('PROFILE=' + label, flush=True)
        build = ROOT / relative
        names = fields(build, suffix, label == 'nuttx-current')
        words, functions = elf_table(build / binary)
        assert len(names) == len(words), (len(names), len(words))
        assert words[0] == 9 and words[-1] == 0xdeadbeaf
        if previous is not None:
            assert names == previous, 'ABI field ordering differs'
        previous = names
        for index, (name, value) in enumerate(zip(names, words)):
            if index not in (0, len(names) - 1):
                assert value == 0 or value in functions, (name, hex(value))
            target = ('NULL' if value == 0 else
                      '|'.join(functions.get(value, ['NONFUNCTION_OR_CONSTANT'])))
            print(f'{index * 4:03x} {name} {value:08x} {target}')
        print(f'OSI_TABLE_LAYOUT=PASS fields={len(names)} bytes={len(words) * 4}')


if __name__ == '__main__':
    main()
