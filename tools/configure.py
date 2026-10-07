#!/usr/bin/env python3
"""Configure/diagnose via POSIX USB without DTR/RTS reset. Secret values are not echoed."""
import argparse
import json
import os
from pathlib import Path
import termios
import time
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('port')
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument('--file', type=Path, help='Local settings JSON (keep HA tokens private)')
group.add_argument('--show', action='store_true')
group.add_argument('--diagnostics', action='store_true')
group.add_argument('--portal', action='store_true')
group.add_argument('--close', action='store_true')
args = parser.parse_args()
if args.file:
    settings = json.loads(args.file.read_text())
    if not isinstance(settings, dict):
        raise SystemExit('Settings must be a JSON object')
    command = 'CONFIG ' + json.dumps(settings, separators=(',', ':'), ensure_ascii=True)
else:
    command = 'CONFIG SHOW' if args.show else 'DIAGNOSTICS' if args.diagnostics else 'CONFIG PORTAL' if args.portal else 'CONFIG CLOSE'
packet = (command + '\n').encode()
if len(packet) > 2048:
    raise SystemExit('Settings exceed the 2048-byte USB command limit')
fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    attrs = termios.tcgetattr(fd)
    attrs[0] = attrs[1] = attrs[3] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[4] = attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    deadline = time.monotonic() + 55
    pos = 0
    pending = b''
    while time.monotonic() < deadline:
        try:
            if pos < len(packet):
                pos += os.write(fd, packet[pos:])
            data = os.read(fd, 4096)
            pending += data
            while b'\n' in pending:
                line, pending = pending.split(b'\n', 1)
                text = line.decode(errors='replace').strip()
                if args.portal and text.startswith('PORTAL_READY:'):
                    print(text)
                    raise SystemExit(0)
                if text.startswith(('CONFIG_SAVED:', 'CONFIG_FAILED:', 'PORTAL_CLOSE_REQUESTED')):
                    print(text)
                    raise SystemExit(1 if text.startswith('CONFIG_FAILED:') else 0)
                if text.startswith('{'):
                    result = json.loads(text)
                    print(json.dumps(result, indent=2))
                    raise SystemExit(0)
        except BlockingIOError:
            pass
        time.sleep(.02)
    raise SystemExit('No response; the panel may be busy, USB may need reconnecting, or maintenance handover exceeded 55 seconds')
finally:
    os.close(fd)
