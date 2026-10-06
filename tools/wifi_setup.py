#!/usr/bin/env python3
"""Set ESP32 Wi-Fi locally; never print, log, or save the password on this computer."""
import argparse,getpass,json,time
import serial
p=argparse.ArgumentParser();p.add_argument('port');a=p.parse_args()
ssid=input('Wi-Fi SSID: ');password=getpass.getpass('Wi-Fi password: ')
if not 0<len(ssid.encode())<=32 or len(password.encode())>63:
    raise SystemExit('Invalid Wi-Fi credential lengths')
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.dtr=False;s.rts=False;s.timeout=0.5;s.open()
try:
    # Opening native USB may reset the ESP32. Wait for the application, then
    # submit the credentials; sending immediately can lose them during reset.
    print('Waiting for firmware readiness...')
    deadline=time.monotonic()+35;probe=0;ready=False
    while time.monotonic()<deadline:
        if time.monotonic()>=probe:
            s.write(b'STATUS\n');s.flush();probe=time.monotonic()+2
        line=s.readline().decode(errors='replace').strip()
        if line.startswith('LIVE status:') or line.startswith('LIVE:'):
            ready=True;break
    if not ready: raise SystemExit('Firmware did not acknowledge readiness; credentials were not sent.')
    s.write(('WIFI '+json.dumps({'ssid':ssid,'password':password})+'\n').encode());s.flush();password=''
    deadline=time.monotonic()+25;saved=False
    while time.monotonic()<deadline:
        line=s.readline().decode(errors='replace').strip()
        if line=='WIFI configuration saved; connecting.':
            print(line);saved=True;break
        if line.startswith('WIFI configuration'):
            raise SystemExit(line)
    if not saved: raise SystemExit('No save acknowledgement; Wi-Fi configuration is unconfirmed.')
finally:
    password='';s.close()
