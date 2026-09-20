from pathlib import Path
import json,shlex,subprocess
root=Path.cwd();b=root/'openvela-dev/out/esp32s31-xts-flat-sdmmc-1775';entries=json.loads((b/'compile_commands.json').read_text());e=next(e for e in entries if e['file'].endswith('/esp32s31_sdmmc.c'));hal=root/'s31-reference/deps/esp-hal-3rdparty/components'
failed = False
for name in ['src/sd_host_sdmmc.c','src/sd_trans_sdmmc.c','legacy/src/sdmmc_host.c','legacy/src/sdmmc_transaction.c']:
 c=shlex.split(e['command']);c[c.index('-o')+1]='/tmp/'+Path(name).stem+'.o';c[c.index(e['file'])]=str(hal/'upper_hal_sdmmc'/name);c+=['-include',str(b/'include/nuttx/config.h'),'-Wall','-Werror','-include',str(root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31/include/esp32s31_sdmmc_idf_compat.h'),*['-I'+str(hal/d) for d in ['upper_hal_sd_intf/include','upper_hal_sdmmc/src','upper_hal_sdmmc/include','upper_hal_sdmmc/legacy/include','esp_hal_sd/include','esp_hal_sd/esp32s31/include','soc/esp32s31/include']]];r=subprocess.run(c,cwd=e['directory']);print(name,':',r.returncode);failed |= r.returncode != 0

c=shlex.split(e['command'])
c[c.index('-o')+1]='/tmp/sdmmc_os.o'
c[c.index(e['file'])]=str(root/'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_sdmmc_os.c')
c+=['-include',str(b/'include/nuttx/config.h'),'-Wall','-Werror']
r=subprocess.run(c,cwd=e['directory']);print('NuttX SDMMC synchronization:',r.returncode);failed |= r.returncode != 0
raise SystemExit(1 if failed else 0)
