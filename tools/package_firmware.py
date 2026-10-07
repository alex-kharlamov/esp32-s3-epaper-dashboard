#!/usr/bin/env python3
"""Package application-only OTA image; never includes Wi-Fi/NVS/settings."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[1]
source = root / 'build/firmware/ESP32S3_Dashboard.ino.bin'
assert source.is_file(), 'Build the public firmware first'
assert source.stat().st_size <= 0x300000, 'Application exceeds OTA slot'
out = root / 'build/release'
out.mkdir(parents=True, exist_ok=True)
image = out / 'dashboard-esp32s3-v21.bin'
shutil.copyfile(source, image)
digest = hashlib.sha256(image.read_bytes()).hexdigest()
(out / 'SHA256SUMS').write_text(f'{digest}  {image.name}\n')
(out / 'manifest.json').write_text(json.dumps({'version': 'v21', 'target': 'ESP32-S3 N16R8 / 10.85 G', 'file': image.name, 'bytes': image.stat().st_size, 'sha256': digest}, indent=2) + '\n')
print(f'Packaged {image.name}; SHA-256 {digest}')
