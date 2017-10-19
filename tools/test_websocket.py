#import sys
#import telnetlib
#
#tn = telnetlib.Telnet('localhost', 80)
#tn.write('GET /websocket\n')
#tn.write('Connection: Upgrade\n')
#tn.write('Upgrade: websocket\n')
#tn.write('Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\n')
#tn.write('\n')
#
#while 1:
#    x = tn.read_some()
#    if not x:
#        break
#    sys.stdout.write(x)
#    sys.stdout.flush()

import websocket
ws = websocket.WebSocket()
client = ws.connect("ws://localhost/websocket", http_proxy_host="proxy_host_name", http_proxy_port=3128)
print client.read()
