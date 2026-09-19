#!/usr/bin/env python3
"""Production board startup: XCLK precedes SCCB, failures unwind controller."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[3]
source = root / 'openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_camera.c'
with tempfile.TemporaryDirectory() as temp:
    p = Path(temp)
    def put(name, text):
        f = p / name; f.parent.mkdir(parents=True, exist_ok=True); f.write_text(text)
    put('nuttx/config.h', '''#define CONFIG_ESP32S31_CAMERA_DVP 1
#define CONFIG_ESP32S31_CAMERA_V4L2 1
#define CONFIG_ESPRESSIF_I2C0 1
#define CONFIG_I2C_DRIVER 1
#define CONFIG_ESPRESSIF_I2C0_SCLPIN 1
#define CONFIG_ESPRESSIF_I2C0_SDAPIN 0
''')
    put('nuttx/i2c/i2c_master.h', '#include <stddef.h>\nstruct i2c_master_s {int value;};\n')
    put('espressif/esp_i2c.h','struct i2c_master_s *esp_i2cbus_initialize(int);\n')
    put('nuttx/video/ov2640.h','int ov2640_initialize(struct i2c_master_s *);\n')
    put('esp32s31_camera_dvp.h', 'int esp32s31_camera_dvp_initialize(void);\nvoid esp32s31_camera_dvp_uninitialize(void);\n')
    put('test.c', '#include "'+str(source)+'"\n'+r'''
#include <assert.h>
#include <stdio.h>
static int stage, fail, cleaned;
static struct i2c_master_s bus;
struct i2c_master_s *esp_i2cbus_initialize(int port)
{ assert(port == 0 && stage++ == 0); return fail == 1 ? NULL : &bus; }
int esp32s31_camera_dvp_initialize(void)
{ assert(stage++ == 1); return fail == 2 ? -ENOMEM : 0; }
int esp32s31_ov3660_initialize(struct i2c_master_s *b)
{ assert(b == &bus && stage++ == 2); return fail == 3 ? -ENODEV : 0; }
int ov2640_initialize(struct i2c_master_s *b)
{ return esp32s31_ov3660_initialize(b); }
int esp32s31_camera_v4l2_initialize(void)
{ assert(stage++ == 3); return fail == 4 ? -EIO : 0; }
void esp32s31_camera_dvp_uninitialize(void) { ++cleaned; }
int main(void)
{
  int errors[] = {0, -ENODEV, -ENOMEM, -ENODEV, -EIO};
  for (fail = 0; fail < 5; fail++) {
    stage = cleaned = 0;
    assert(esp32s31_camera_initialize() == errors[fail]);
    assert(cleaned == (fail >= 3));
    if (!fail) assert(stage == 4);
  }
  puts("PASS: production startup XCLK before SCCB; no registration after probe failure; DVP cleanup on sensor/registration failure");
}
''')
    for sensor in ('OV3660', 'OV2640'):
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(p), '-DCONFIG_ESP32S31_CAMERA_'+sensor+'=1', str(p/'test.c'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
