#!/usr/bin/env python3
"""Capture UART; optional explicit reset/RUN. Bridge opening may toggle modem lines."""
import argparse,time,serial,sys
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('port');p.add_argument('--seconds',type=int,default=150)
p.add_argument('--refresh-count',type=int,default=1,help='Stop after this many live refreshes; zero captures for the full duration')
p.add_argument('--run',action='store_true',help='Send RUN; only after boot cooldown is complete')
p.add_argument('--reset',action='store_true',help='Pulse EN via RTS with BOOT/DTR released')
a=p.parse_args()
s=serial.Serial(port=None,baudrate=115200,timeout=0.2)
s.dtr=False;s.rts=False;s.port=a.port;s.open()
if a.reset:
 s.dtr=False;s.rts=True;time.sleep(0.15);s.rts=False
if a.run:s.write(b'RUN\n')
end=time.monotonic()+a.seconds
log=Path(__file__).parent/'serial.log'
completed=0;pending=b''
with log.open('ab') as f:
 while time.monotonic()<end:
  try:
   b=s.read(4096)
  except serial.SerialException as e:
   print(f"\nSerial disconnected: {e}",file=sys.stderr);break
  if b:
   f.write(b);f.flush();sys.stdout.buffer.write(b);sys.stdout.buffer.flush()
   pending+=b
   stopped=False
   while b'\n' in pending:
    line,pending=pending.split(b'\n',1)
    if b'LIVE_REFRESH_DONE:' in line:completed+=1
    if line.startswith(b'DONE:') or b'Stopped:' in line:stopped=True
   if stopped or (a.refresh_count>0 and completed>=a.refresh_count):break
s.close()
