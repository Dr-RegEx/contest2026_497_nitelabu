#!/usr/bin/env python3
from pathlib import Path
import json
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='s31-usb-pio-') as tmp:
 exe=Path(tmp)/'host'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31'),str(Path(__file__).with_name('test.c')),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 out=root/'openvela-dev/out/esp32s31-xts-flat-usb-adb'
 entries=json.loads((out/'compile_commands.json').read_text())
 e=next(e for e in entries if e['file'].endswith('/esp32s31_usbdev.c'))
 for source in ['esp32s31_usbhost.c','esp32s31_usbhost_hcd_skeleton.c']:
  cmd=shlex.split(e['command'])
  cmd[cmd.index('-o')+1]=str(Path(tmp)/(source+'.o'))
  cmd[-1]=str(root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31'/source)
  cmd[1:1]=['-include',str(out/'include/nuttx/config.h')]
  cmd+=['-DCONFIG_USBHOST=1','-DCONFIG_ESP32S31_USBHOST=1','-DCONFIG_ESP32S31_USBHOST_HCD_SKELETON=1','-Werror','-Wno-error=undef']
  for name,extra in [('base',[]),('hub_async',['-DCONFIG_USBHOST_HUB=1','-DCONFIG_USBHOST_ASYNCH=1'])]:
   subprocess.run(cmd+extra,cwd=e['directory'],check=True)
   print(f'S31 {source} strict cross-compile {name}: PASS')
