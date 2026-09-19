#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate temporary header-only SYN-ACK counters against known checksums."""

from pathlib import Path
import struct
import subprocess
import tempfile

from test_esp_hr_timer_lifecycle import definition


def checksum(data):
    data += b'\0' if len(data) % 2 else b''
    value = sum(struct.unpack(f'!{len(data) // 2}H', data))
    while value >> 16:
        value = (value & 65535) + (value >> 16)
    return (~value) & 65535


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / 'arch/risc-v/src/common/espressif/esp_wlan.c').read_text()
    ip = bytearray.fromhex('450000280000400040060000c0a8013cc0a8011d')
    tcp = bytearray(struct.pack('!HHIIBBHHH', 5471, 42424, 123, 456,
                                0x50, 0x12, 4096, 0, 0))
    ip[10:12] = struct.pack('!H', checksum(ip))
    tcp[16:18] = struct.pack('!H', checksum(ip[12:20] + b'\0\x06\0\x14' + tcp))
    frame = bytes(12) + b'\x08\x00' + ip + tcp
    fixture = '''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
static uint32_t g_s31_tcp_synack_diag[6];
'''
    for name in ('wlan_tcp_diag_sum', 'wlan_tcp_diag_sum_valid', 'wlan_tcp_synack_diag'):
        fixture += definition(source, name) + '\n'
    fixture += '\nstatic uint8_t frame[] = {' + ','.join(map(str, frame)) + '};\n'
    fixture += r'''
int main(void)
{
  for (size_t n = 0; n < sizeof(frame); n++)
    { assert(!wlan_tcp_synack_diag(frame, n)); }
  assert(wlan_tcp_synack_diag(frame, sizeof(frame)));
  assert(g_s31_tcp_synack_diag[0] == 1);
  assert(!g_s31_tcp_synack_diag[4] && !g_s31_tcp_synack_diag[5]);
  frame[22] ^= 1;
  assert(wlan_tcp_synack_diag(frame, sizeof(frame)));
  assert(g_s31_tcp_synack_diag[4] == 1 && !g_s31_tcp_synack_diag[5]);
  frame[22] ^= 1;
  frame[45] ^= 1;
  assert(wlan_tcp_synack_diag(frame, sizeof(frame)));
  assert(g_s31_tcp_synack_diag[4] == 1 && g_s31_tcp_synack_diag[5] == 1);
  frame[45] ^= 1;
  const size_t offsets[] = {12, 14, 16, 20, 21, 23, 34, 46, 47};
  const uint8_t values[] = {9, 0x4f, 0xff, 0x20, 1, 17, 0, 0xf0, 0x10};
  for (size_t i = 0; i < sizeof(values); i++)
    {
      uint8_t saved = frame[offsets[i]];
      frame[offsets[i]] = values[i];
      assert(!wlan_tcp_synack_diag(frame, sizeof(frame)));
      frame[offsets[i]] = saved;
    }
  assert(g_s31_tcp_synack_diag[0] == 3);
  const uint8_t odd[] = {0x12, 0x34, 0x56};
  assert(wlan_tcp_diag_sum(odd, sizeof(odd), 0) == 0x6834);
  assert(wlan_tcp_diag_sum_valid(0x1fffe));
  puts("TCP_SYNACK_DIAG=PASS checksums/truncation/filter/odd-length");
}
'''
    with tempfile.TemporaryDirectory(prefix='s31-synack-') as directory:
        path = Path(directory)
        (path / 'test.c').write_text(fixture)
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        str(path / 'test.c'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)


if __name__ == '__main__':
    main()
