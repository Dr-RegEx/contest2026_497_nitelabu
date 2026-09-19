"""Upload then read back the existing media runtime files; no reset."""
from pathlib import Path
import hashlib
import subprocess
R=Path('/home/regex/work/esp32s31-openvela')
D=R/'backups/2026-09-10-scan-stress'
P=R/'s31-reference/.venv-nuttx/bin/python'
S=R/'openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/xts-flat-media/media'
for name in ('graph.conf','criteria.txt','settings.pfw'):
 for flag, value, direction in (('--upload',str(S/name),'upload'),('--download',name,'readback')):
  out=D/('media1231-'+name+'-'+direction)
  subprocess.run([str(P),str(D/'audio-file-transfer.py'),flag,value,'--receipt',str(D/'build1229-media.sha256'),'--output',str(out)],check=True)
 received=list((D/('media1231-'+name+'-readback')).rglob(name))
 assert len(received)==1 and hashlib.sha256(received[0].read_bytes()).digest()==hashlib.sha256((S/name).read_bytes()).digest()
 print('READBACK_SHA256_PASS '+name,flush=True)
