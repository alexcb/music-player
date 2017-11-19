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
# https://pypi.python.org/pypi/websocket-client version 0.44

# callback style app
#  def on_message(ws, message):
#      print message
#  
#  def on_open(ws):
#      print 'opened'
#  
#  def on_close(ws):
#      print 'closed'
#  
#  ws = websocket.WebSocketApp("ws://localhost/websocket", on_message=on_message, on_open=on_open, on_close=on_close)
#  ws.run_forever()

ws = websocket.WebSocket()
ws.connect("ws://localhost/websocket")
while 1:
    print ws.recv()
