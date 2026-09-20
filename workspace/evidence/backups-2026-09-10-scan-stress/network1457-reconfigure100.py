"""Original 100-round runner; keep SSID out of process command line."""
import getpass, importlib.util, sys
from pathlib import Path
D=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('original',D/'network-reconfigure100-original.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
ssid=getpass.getpass('Test SSID (hidden): ')
sys.argv=[str(__file__),'--ssid',ssid,'--gateway','192.168.1.1','--receipt',str(D/'build1455-wifi-state-origin.sha256'),'--result',str(D/'network1457-reconfigure100-result.json')]
raise SystemExit(m.main())
