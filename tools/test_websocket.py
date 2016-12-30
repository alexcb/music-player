import sys
import telnetlib

tn = telnetlib.Telnet('raspberrypi.local', 80)
tn.write('GET /websocket\n')
tn.write('Connection: Upgrade\n')
tn.write('Upgrade: websocket\n')
tn.write('\n')

while 1:
    x = tn.read_some()
    if not x:
        break
    sys.stdout.write(x)
    sys.stdout.flush()
