from pathlib import Path
import hashlib,json,re
root=Path('/home/regex/work/esp32s31-openvela');d=Path(__file__).resolve().parent
p=root/'openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_xts_media_volume.c'
h=root/'openvela-dev/nuttx/include/nuttx/mtd/mtd.h';src=p.read_text();header=h.read_text()
parts=[]
for name in ['mtd_dev_s','mtd_geometry_s','rammtd_config_s']:
 match=re.search(r'struct '+name+r'\s*\{.*?\n\};',header,re.S);assert match;parts.append(match[0])
for line in header.splitlines():
 if re.match(r'#define MTD_(ERASE|BREAD|BWRITE|READ|WRITE|IOCTL)\(',line):parts.append(line)
(d/'actual-mtd.h').write_text('\n'.join(parts)+'\n')
(d/'actual-wrapper.c').write_text(re.sub(r'^#include[^\n]*\n','',src,flags=re.M))
(d/'wrapper-source-hashes.json').write_text(json.dumps({str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [p,h]},indent=2)+'\n')
