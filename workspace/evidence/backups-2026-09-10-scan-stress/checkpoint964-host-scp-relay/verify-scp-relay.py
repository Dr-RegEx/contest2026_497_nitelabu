import subprocess,sys,json
from pathlib import Path
root=Path(__file__).resolve().parent
relay=subprocess.Popen([sys.executable,str(root/'host-tcp-relay.py'),'--bind','127.0.0.1','--port','29222','--target','172.31.208.211','--target-port','2222'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
try:
    line=relay.stdout.readline()
    assert 'listening' in line, 'Relay did not start'
    scan=subprocess.run([r'C:\Windows\System32\OpenSSH\ssh-keyscan.exe','-T','5','-p','29222','-t','rsa','127.0.0.1'],capture_output=True,text=True,timeout=20)
    (root/'scp-relay-hostkey.pub').write_text(scan.stdout)
    (root/'scp-relay-keyscan.log').write_text(scan.stderr)
    assert scan.returncode==0 and 'ssh-rsa ' in scan.stdout, 'SSH keyscan failed'
    print(json.dumps({'status':'WINDOWS_RELAY_SSH_HANDSHAKE_PASS','bind':'127.0.0.1:29222','target':'WSL:2222','board_test':False}))
finally:
    relay.terminate()
    relay.communicate(timeout=10)
