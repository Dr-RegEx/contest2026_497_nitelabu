#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise bounded, header-only ARP diagnostic classification."""

from pathlib import Path
import subprocess
import tempfile

from test_esp_hr_timer_lifecycle import definition


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / 'arch/risc-v/src/common/espressif/esp_wlan.c').read_text()
    fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WLAN_BUF_SIZE 1518
static uint32_t g_s31_arp_diag[3][10];
static uint32_t g_s31_tx_done_diag[4];
static uint32_t g_s31_tx_shape_diag[5];
static uint32_t g_s31_arp_request_diag[6];
'''
    fixture += definition(source, 'wlan_arp_diag')
    fixture += '\n' + definition(source, 'wlan_arp_request_diag')
    fixture += '\n' + definition(source, 'wlan_tx_shape_diag')
    fixture += '\n' + definition(source, 'wlan_arp_tx_done_diag')
    fixture += r'''
int main(void)
{
  uint8_t frame[42] = {0};
  const uint8_t peer[4] = {192, 168, 1, 29};
  assert(!wlan_arp_diag(NULL, 42, 0));
  assert(!wlan_arp_diag(frame, 42, 3));
  assert(!wlan_arp_diag(frame, 42, 0));
  frame[12] = 8; frame[13] = 6;
  for (size_t n = 0; n < 14; n++)
    { assert(!wlan_arp_diag(frame, n, 0)); }
  for (size_t n = 14; n < 42; n++)
    { assert(wlan_arp_diag(frame, n, 0)); }
  assert(g_s31_arp_diag[0][0] == 28 && g_s31_arp_diag[0][1] == 28);
  memset(g_s31_arp_diag, 0, sizeof(g_s31_arp_diag));
  frame[15] = 1; frame[16] = 8; frame[18] = 6; frame[19] = 4;
  frame[21] = 1;
  memcpy(frame + 38, peer, 4);
  assert(wlan_arp_diag(frame, sizeof(frame), 0));
  assert(g_s31_arp_diag[0][0] == 1 && !g_s31_arp_diag[0][1]);
  assert(g_s31_arp_diag[0][2] == 1 && !g_s31_arp_diag[0][3]);
  assert(g_s31_arp_diag[0][4] == 1 && !g_s31_arp_diag[0][5]);
  assert(g_s31_arp_diag[0][6] == 42 && !g_s31_arp_diag[0][7]);
  frame[21] = 2;
  memcpy(frame + 28, peer, 4);
  assert(wlan_arp_diag(frame, sizeof(frame), 1));
  assert(g_s31_arp_diag[1][3] == 1 && g_s31_arp_diag[1][5] == 1);
  frame[22] = 1;
  assert(wlan_arp_diag(frame, sizeof(frame), 1));
  assert(g_s31_arp_diag[1][7] == 1);
  frame[18] = 5;
  assert(wlan_arp_diag(frame, sizeof(frame), 1));
  assert(g_s31_arp_diag[1][1] == 1 && g_s31_arp_diag[1][3] == 2);
  frame[18] = 6;
  frame[21] = 1;
  memset(frame + 28, 0, 4);
  uint16_t len = sizeof(frame);
  wlan_arp_tx_done_diag(frame, &len, true);
  wlan_arp_tx_done_diag(frame, &len, false);
  assert(g_s31_arp_diag[2][4] == 2 && g_s31_arp_diag[2][5] == 0);
  assert(g_s31_arp_diag[2][8] == 1 && g_s31_arp_diag[2][9] == 1);
  wlan_arp_tx_done_diag(NULL, &len, true);
  wlan_arp_tx_done_diag(frame, NULL, true);
  len = 13;
  wlan_arp_tx_done_diag(frame, &len, true);
  len = WLAN_BUF_SIZE + 1;
  wlan_arp_tx_done_diag(frame, &len, true);
  len = sizeof(frame);
  frame[13] = 0;
  wlan_arp_tx_done_diag(frame, &len, true);
  assert(g_s31_tx_done_diag[0] == 7 && g_s31_tx_done_diag[1] == 1);
  assert(g_s31_tx_done_diag[2] == 4 && g_s31_tx_done_diag[3] == 1);
  uint8_t shaped[101] = {0};
  const uint8_t snap_arp[16] =
    {0xaa, 0xaa, 3, 0, 0, 0, 8, 6, 0, 1, 8, 0, 6, 4, 0, 1};
  const unsigned offsets[] = {0, 24, 26, 32, 64};
  for (unsigned i = 0; i < 5; i++)
    {
      memset(shaped, 0, sizeof(shaped));
      memcpy(shaped + offsets[i], snap_arp, sizeof(snap_arp));
      wlan_tx_shape_diag(shaped, offsets[i] + 36, false);
      assert(g_s31_tx_shape_diag[1] == i + 1);
      assert(g_s31_tx_shape_diag[2] == i + 1);
      assert(g_s31_tx_shape_diag[3] == offsets[i]);
    }
  memset(shaped, 0, sizeof(shaped));
  memcpy(shaped + 65, snap_arp, sizeof(snap_arp));
  wlan_tx_shape_diag(shaped, sizeof(shaped), true);
  assert(g_s31_tx_shape_diag[0] == 5);
  for (size_t n = 0; n < 16; n++)
    {
      uint8_t *short_frame = n ? malloc(n) : NULL;
      if (n)
        {
          assert(short_frame);
          memset(short_frame, 0, n);
          memcpy(short_frame, snap_arp, n < 16 ? n : 16);
        }
      wlan_tx_shape_diag(short_frame, n, true);
      free(short_frame);
    }
  assert(g_s31_tx_shape_diag[1] == 5);
  memset(shaped, 0, sizeof(shaped));
  memcpy(shaped, snap_arp, sizeof(snap_arp));
  wlan_tx_shape_diag(shaped, 16, true);
  assert(g_s31_tx_shape_diag[1] == 6 && g_s31_tx_shape_diag[2] == 5);
  shaped[15] = 3;
  wlan_tx_shape_diag(shaped, 36, true);
  shaped[7] = 0;
  wlan_tx_shape_diag(shaped, 36, true);
  assert(g_s31_tx_shape_diag[1] == 6);
  uint8_t request[42] = {0};
  const uint8_t mac[6] = {2, 3, 4, 5, 6, 7};
  const uint8_t ip[4] = {192, 168, 1, 60};
  memset(request, 255, 6);
  memcpy(request + 6, mac, 6);
  request[12] = 8; request[13] = 6; request[21] = 1;
  memcpy(request + 22, mac, 6);
  memcpy(request + 28, ip, 4);
  memcpy(request + 38, peer, 4);
  wlan_arp_request_diag(NULL, 42, mac, ip);
  wlan_arp_request_diag(request, 41, mac, ip);
  wlan_arp_request_diag(request, 42, NULL, ip);
  wlan_arp_request_diag(request, 42, mac, NULL);
  assert(g_s31_arp_request_diag[0] == 0);
  wlan_arp_request_diag(request, 42, mac, ip);
  assert(g_s31_arp_request_diag[0] == 1);
  for (unsigned i = 1; i < 6; i++) assert(g_s31_arp_request_diag[i] == 0);
  const unsigned mutations[] = {0, 6, 22, 28, 32};
  for (unsigned i = 0; i < 5; i++)
    {
      request[mutations[i]] ^= 1;
      wlan_arp_request_diag(request, 42, mac, ip);
      request[mutations[i]] ^= 1;
      assert(g_s31_arp_request_diag[i + 1] == 1);
    }
  assert(g_s31_arp_request_diag[0] == 6);
  request[21] = 2;
  wlan_arp_request_diag(request, 42, mac, ip);
  request[21] = 1; request[41] ^= 1;
  wlan_arp_request_diag(request, 42, mac, ip);
  assert(g_s31_arp_request_diag[0] == 6);
  puts("ARP_DIAG=PASS bounds/TX-done/SNAP/live-interface-request-invariants");
}
'''
    with tempfile.TemporaryDirectory(prefix='s31-arp-diag-') as directory:
        path = Path(directory)
        (path / 'test.c').write_text(fixture)
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        str(path / 'test.c'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)


if __name__ == '__main__':
    main()
