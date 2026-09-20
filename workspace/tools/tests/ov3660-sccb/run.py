#!/usr/bin/env python3
"""Compile the production OV3660 SCCB driver with a recording bus substitute."""
from pathlib import Path
import tempfile, subprocess
root = Path(__file__).resolve().parents[3]
source = root / "openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_ov3660.c"
with tempfile.TemporaryDirectory() as temp:
    p = Path(temp)
    def put(name, text):
        file = p / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text)
    put("nuttx/config.h", "#define CONFIG_ESP32S31_CAMERA_DVP 1\n#define CONFIG_ESP32S31_CAMERA_DVP_HRES 640\n#define CONFIG_ESP32S31_CAMERA_DVP_VRES 480\n#define CONFIG_ESP32S31_CAMERA_DVP_XCLK_FREQ 16000000\n")
    put("nuttx/signal.h", "int nxsig_usleep(unsigned int);\n")
    put("nuttx/i2c/i2c_master.h", r'''#include <stdint.h>
struct i2c_master_s { int dummy; };
struct i2c_config_s { uint32_t frequency; uint16_t address; uint8_t addrlen; };
struct i2c_msg_s { uint32_t frequency; uint16_t addr; uint16_t flags; uint8_t *buffer; int length; };
#define I2C_M_READ 1
int i2c_write(struct i2c_master_s *, const struct i2c_config_s *, const uint8_t *, int);
int fake_transfer(struct i2c_master_s *, struct i2c_msg_s *, int);
#define I2C_TRANSFER fake_transfer
''')
    put("test.c", '#include "'+str(source)+'"\n'+r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char registers[65536];
static int writes, reads, fail_write, fail_read;
static unsigned int delay;
static struct i2c_master_s bus;
int nxsig_usleep(unsigned int usec) { delay += usec; return 0; }
int i2c_write(struct i2c_master_s *b, const struct i2c_config_s *c,
              const uint8_t *data, int length)
{
  assert(b == &bus && c->frequency == 100000 && c->address == 0x3c && c->addrlen == 7);
  assert(length == 3);
  if (++writes == fail_write) return -ETIMEDOUT;
  registers[(data[0] << 8) | data[1]] = data[2];
  return 0;
}
int fake_transfer(struct i2c_master_s *b, struct i2c_msg_s *m, int count)
{
  assert(b == &bus && count == 2);
  assert(m[0].frequency == 100000 && m[1].frequency == 100000);
  assert(m[0].addr == 0x3c && m[1].addr == 0x3c);
  assert(m[0].flags == 0 && m[1].flags == I2C_M_READ);
  assert(m[0].length == 2 && m[1].length == 1);
  if (++reads == fail_read) return -EIO;
  *m[1].buffer = registers[(m[0].buffer[0] << 8) | m[0].buffer[1]];
  return 0;
}
static void reset(void)
{
  memset(registers, 0, sizeof(registers));
  registers[0x300a] = 0x36; registers[0x300b] = 0x60;
  reads = writes = fail_write = fail_read = delay = 0;
}
int main(void)
{
  int total;
  reset(); assert(esp32s31_ov3660_initialize(NULL) == -EINVAL);
  assert(esp32s31_ov3660_initialize(&bus) == 0);
  total = writes;
  assert(total > 200 && reads == 9 && delay >= 210000);
  assert(registers[0x3808] == 2 && registers[0x3809] == 0x80);
  assert(registers[0x380a] == 1 && registers[0x380b] == 0xe0);
  assert(registers[0x501f] == 1 && registers[0x4300] == 0x61);
  assert(registers[0x5001] == 0xa3);
  assert(registers[0x303b] == 4 && registers[0x3824] == 2);
  assert(registers[0x3008] == 2);
  reset(); registers[0x300b] = 0x40;
  assert(esp32s31_ov3660_initialize(&bus) == -ENODEV && writes == 0);
  for (int i = 1; i <= 9; ++i) {
    reset(); fail_read = i;
    assert(esp32s31_ov3660_initialize(&bus) == -EIO && reads == i);
  }
  for (int i = 1; i <= total; ++i) {
    reset(); fail_write = i;
    assert(esp32s31_ov3660_initialize(&bus) == -ETIMEDOUT && writes == i);
  }
  printf("PASS: SCCB address/16-bit register framing, VGA/PLL/scaler, PID, all %d write failures and 9 read failures preserved\n", total);
}
''')
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-fno-pie", "-no-pie", "-I"+str(p), str(p / "test.c"), "-o", str(p / "test")], check=True)
    subprocess.run([str(p / "test")], check=True)
