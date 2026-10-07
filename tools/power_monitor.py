#!/usr/bin/env python3
"""POSIX native-USB diagnostics; never changes DTR/RTS; reconnects after sleep."""
import argparse
import os
from pathlib import Path
import sys
import termios
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('port')
parser.add_argument('--seconds', type=int, default=360)
parser.add_argument('--command', default='', help='One diagnostic command, sent once only')
parser.add_argument('--after-refresh', type=int, default=0, help='Send command after this many completed redraws')
parser.add_argument('--log', type=Path, default=Path(__file__).resolve().parents[1] / 'build/power-serial.log')
args = parser.parse_args()
args.log.parent.mkdir(parents=True, exist_ok=True)
fd = None
sent = False
fingerprint = None
completed = 0
pending = b''
deadline = time.monotonic() + args.seconds

def device_fingerprint():
    stat = os.stat(args.port)
    return stat.st_ino, stat.st_rdev, stat.st_ctime_ns

def close_port():
    global fd
    if fd is not None:
        os.close(fd)
        fd = None

try:
    with args.log.open('ab') as log:
        while time.monotonic() < deadline:
            try:
                current = device_fingerprint()
                if fd is not None and current != fingerprint:
                    close_port()
                if fd is None:
                    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                    attrs = termios.tcgetattr(fd)
                    attrs[0] = attrs[1] = attrs[3] = 0
                    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
                    attrs[4] = attrs[5] = termios.B115200
                    attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
                    termios.tcsetattr(fd, termios.TCSANOW, attrs)
                    fingerprint = device_fingerprint()
                data = os.read(fd, 4096)
                if data:
                    log.write(data)
                    log.flush()
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                    pending += data
                    while b'\n' in pending:
                        line, pending = pending.split(b'\n', 1)
                        if b'LIVE_REFRESH_DONE:' in line:
                            completed += 1
                if args.command and not sent and completed >= args.after_refresh:
                    packet = (args.command + '\n').encode()
                    # Keep diagnostics small so a single nonblocking write is atomic.
                    if len(packet) > 256:
                        raise ValueError('Diagnostic command must be at most 255 bytes')
                    count = os.write(fd, packet)
                    if count != len(packet):
                        raise RuntimeError('Partial diagnostic write; command will not be resent')
                    sent = True
            except BlockingIOError:
                pass
            except OSError:
                close_port()
                time.sleep(0.25)
            time.sleep(0.02)
except KeyboardInterrupt:
    pass
finally:
    close_port()
