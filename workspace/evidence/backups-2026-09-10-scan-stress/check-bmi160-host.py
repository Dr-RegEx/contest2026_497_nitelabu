#!/usr/bin/env python3
"""Exercise the actual BMI160 read/getregs/register functions with a bus shim.

Host-only error/packing evidence; never a substitute for the original target xTS.
"""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
sensors = root / 'openvela-dev/nuttx/drivers/sensors'


def function(text, signature):
    start = text.rindex(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


base = (sensors / 'bmi160_base.c').read_text()
char = (sensors / 'bmi160.c').read_text()
getregs = function(base, 'int bmi160_getregs(')
read = function(char, 'static ssize_t bmi160_read(FAR')
# Skip the I2C/SPI signature choice while retaining the complete actual body.
start = char.index('int bmi160_register(FAR const char *devpath, FAR struct i2c_master_s *dev)')
body = char[char.index('{', start):]
register = 'int bmi160_register(const char *devpath, struct i2c_master_s *dev)\n' + body[:body.rfind('#endif')]
header = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#define FAR
#define CONFIG_SENSORS_BMI160_I2C 1
#define OK 0
#define I2C_M_NOSTOP 1
#define I2C_M_READ 2
#define BMI160_DATA_8 0x0c
#define BMI160_I2C_ADDR 0x68
#define BMI160_I2C_FREQ 400000
#define BMI160_PMU_TRIGGER 0x6c
#define snerr(...) ((void)0)
#define sninfo(...) ((void)0)
struct i2c_master_s { int unused; };
struct i2c_msg_s { int frequency, addr, flags; uint8_t *buffer; int length; };
struct bmi160_dev_s { struct i2c_master_s *i2c; uint8_t addr; int freq; };
struct inode { void *i_private; };
struct file { struct inode *f_inode; };
struct axis { int16_t x, y, z; };
struct accel_gyro_st_s { struct axis gyro, accel; uint32_t sensor_time; };
static int transfer_result, register_result, checkid_result, allocated;
static void *registered;
static const int g_bmi160fops;
static const uint8_t sample[15] = {
  0x34,0x12,0x00,0x80,0xff,0xff,0x01,0x00,0xff,0x7f,0xfe,0xff,
  0x56,0x34,0x12
};
static int transfer(struct i2c_master_s *bus, struct i2c_msg_s *m, int n)
{
  (void)bus;
  assert(n == 2 && m[0].addr == 0x68 && m[1].addr == 0x68);
  assert(m[0].frequency == 400000 && m[0].flags == I2C_M_NOSTOP);
  assert(m[0].length == 1 && *m[0].buffer == BMI160_DATA_8);
  assert(m[1].flags == I2C_M_READ && m[1].length == 15);
  if (transfer_result < 0) return transfer_result;
  memcpy(m[1].buffer, sample, sizeof(sample));
  return transfer_result;
}
#define I2C_TRANSFER transfer
static void *kmm_malloc(size_t n) { allocated++; return malloc(n); }
static void kmm_free(void *p) { allocated--; free(p); }
static int bmi160_checkid(struct bmi160_dev_s *p)
{ (void)p; return checkid_result; }
static void bmi160_putreg8(struct bmi160_dev_s *p, int a, int v)
{ (void)p; (void)a; (void)v; }
static int register_driver(const char *path, const void *ops, int mode, void *p)
{
  (void)ops; (void)mode;
  assert(strcmp(path, "/dev/accel0") == 0);
  if (register_result == 0) registered = p;
  return register_result;
}
'''
main = r'''
int main(void)
{
  struct i2c_master_s bus = {0};
  struct bmi160_dev_s dev = {&bus, 0x68, 400000};
  struct inode inode = {&dev};
  struct file file = {&inode};
  struct accel_gyro_st_s out;
  memset(&out, 0xa5, sizeof(out));
  assert(bmi160_read(&file, (char *)&out, sizeof(out)) == sizeof(out));
  assert(out.gyro.x == 0x1234 && out.gyro.y == -32768 && out.gyro.z == -1);
  assert(out.accel.x == 1 && out.accel.y == 32767 && out.accel.z == -2);
  assert(out.sensor_time == 0x123456);
  assert(bmi160_read(&file, (char *)&out, sizeof(out) - 1) == -EINVAL);
  assert(bmi160_read(&file, (char *)&out, sizeof(out) + 1) == sizeof(out));
  transfer_result = -ENXIO;
  memset(&out, 0xa5, sizeof(out));
  assert(bmi160_read(&file, (char *)&out, sizeof(out)) == -ENXIO);
  assert(out.sensor_time == 0xa5a5a5a5);
  checkid_result = -ENODEV;
  assert(bmi160_register("/dev/accel0", &bus) == -ENODEV && allocated == 0);
  checkid_result = 0; register_result = -EEXIST;
  assert(bmi160_register("/dev/accel0", &bus) == -EEXIST && allocated == 0);
  register_result = 0;
  assert(bmi160_register("/dev/accel0", &bus) == 0 && allocated == 1);
  kmm_free(registered);
  puts("PASS: actual BMI160 functions: signed axes, 24-bit timestamp, length, I2C failure, absent chip, register failure, registration");
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bmi160-host-') as temp:
    temp = pathlib.Path(temp)
    src = temp / 'check.c'
    src.write_text(header + getregs + '\n' + read + '\n' + register + '\n' + main)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(src), '-o', str(temp / 'check')], check=True)
    subprocess.run([str(temp / 'check')], check=True)
