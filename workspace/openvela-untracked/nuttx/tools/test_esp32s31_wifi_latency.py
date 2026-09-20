#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check temporary monitor-to-Wi-Fi callback cycle accounting, not timing."""

from pathlib import Path
import subprocess
import tempfile

from test_esp_hr_timer_lifecycle import definition


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / "arch/risc-v/src/esp32c6/esp_wifi_adapter.c").read_text()
    assembly = (root / "arch/risc-v/src/esp32s31/esp32s31_supervisor.S").read_text()
    start = assembly.index(".Lmachine_forward_interrupt:\n")
    direct = assembly.index(".Lmachine_forward_direct:\n", start)
    assert start < assembly.index("  esp32s31_monlat_start\n", start) < direct
    assert "esp32s31_monlat_replay" not in assembly
    fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define IRAM_ATTR
#define CONFIG_ESPRESSIF_CPU_FREQ_MHZ 320
static volatile uint32_t g_esp32s31_machine_monlat[2][16];
'''
    fixture += definition(source, "s31_wifi_latency_update")
    fixture += r'''
int main(void)
{
  s31_wifi_latency_update(0, 1234);
  assert(g_esp32s31_machine_monlat[0][3] == 0);
  const uint32_t delays[] = {10, 10240, 10241, 32000, 32001, 320000, 320001};
  for (unsigned cpu = 0; cpu < 2; cpu++) {
    uint32_t sum = 0;
    g_esp32s31_machine_monlat[cpu][8] = cpu;
    for (unsigned i = 0; i < sizeof(delays) / sizeof(delays[0]); i++) {
      uint32_t start = cpu ? UINT32_MAX - 7 : 100;
      g_esp32s31_machine_monlat[cpu][0] = start;
      s31_wifi_latency_update(cpu, start + delays[i]);
      sum += delays[i];
      assert(g_esp32s31_machine_monlat[cpu][0] == 0);
      assert(g_esp32s31_machine_monlat[cpu][1] == delays[i]);
      assert(g_esp32s31_machine_monlat[cpu][2] == delays[i]);
      assert(g_esp32s31_machine_monlat[cpu][3] == i + 1);
      assert(g_esp32s31_machine_monlat[cpu][7] == sum);
      s31_wifi_latency_update(cpu, 9999);
      assert(g_esp32s31_machine_monlat[cpu][3] == i + 1);
    }
    assert(g_esp32s31_machine_monlat[cpu][4] == 5);
    assert(g_esp32s31_machine_monlat[cpu][5] == 3);
    assert(g_esp32s31_machine_monlat[cpu][6] == 1);
    g_esp32s31_machine_monlat[cpu][0] = 100;
    g_esp32s31_machine_monlat[cpu][7] = UINT32_MAX;
    s31_wifi_latency_update(cpu, 110);
    assert(g_esp32s31_machine_monlat[cpu][7] == 9);
    assert(g_esp32s31_machine_monlat[cpu][2] == 320001);
    assert(g_esp32s31_machine_monlat[cpu][11 + cpu] == 8);
    assert(g_esp32s31_machine_monlat[cpu][9 + cpu] == 320001);
    g_esp32s31_machine_monlat[cpu][8] = 1 - cpu;
    g_esp32s31_machine_monlat[cpu][0] = 100;
    s31_wifi_latency_update(cpu, 110);
    assert(g_esp32s31_machine_monlat[cpu][12 - cpu] == 1);
    assert(g_esp32s31_machine_monlat[cpu][10 - cpu] == 10);
  }
  puts("S31_WIFI_LATENCY_MODEL=PASS per-hart/thresholds/wrap/consume-once");
}
'''
    with tempfile.TemporaryDirectory(prefix="s31-wifi-latency-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(fixture)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all",
                        str(path / "test.c"), "-o", str(path / "test")],
                       check=True)
        subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    main()
