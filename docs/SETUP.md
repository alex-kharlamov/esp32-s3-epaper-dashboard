# Build, wire and run

## Scripted setup: macOS / Linux

`./scripts/setup.sh` installs a contained toolchain into ignored `.tools/`:

- Arduino CLI 1.5.2-rc.1.
- Espressif Arduino core 3.3.12.
- ArduinoJson 7.4.2.
- Python environment with pyserial 3.5, esptool 5.3.1 and Pillow 11.3.0.

On Apple Silicon, it rebuilds the official Arduino ctags release because the bundled Intel binary did not run on the tested host. This requires Xcode Command Line Tools (`xcode-select --install`). The shim changes an old macro that conflicts with current macOS SDK headers.

The scripts were verified on Apple Silicon. Linux branches are provided but have not been tested on a physical Linux host.

Run `scripts/build.sh`, then `scripts/upload.sh YOUR_PORT`. Serial ports vary: `/dev/cu.usbmodem…` on macOS, often `/dev/ttyACM0` on Linux. Use `.tools/bin/arduino-cli board list --config-file .tools/arduino-cli.yaml` to list candidates. Linux may require serial-port group permissions according to your distribution.

## Arduino IDE

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to Additional Boards Manager URLs.
3. Install **esp32 by Espressif Systems 3.3.12** and **ArduinoJson 7.4.2**.
4. Open `ESP32S3_Dashboard/ESP32S3_Dashboard.ino`.
5. Select **ESP32S3 Dev Module** and these settings:

| Option | Value |
|---|---|
| Flash size | 16 MB |
| Flash mode | QIO |
| PSRAM | OPI PSRAM |
| USB CDC on boot | Enabled |
| USB mode | Hardware CDC and JTAG |
| Partition scheme | 16M Flash (3MB APP / 9.9MB FATFS), corresponding to `app3M_fat9M_16MB` |
| Upload speed | 460800 |
| Erase all flash before upload | Disabled |

Select the native USB-OTG port and upload. The IDE route is documented for portability; the tested build used Arduino CLI.

For local Wi-Fi setup, install Python 3 and `pyserial`, then run:

```bash
python -m pip install pyserial==3.5
python tools/wifi_setup.py YOUR_PORT
```

## Location and clock

Change `DASH_LATITUDE`, `DASH_LONGITUDE` and `DASH_LOCATION_LABEL` in `Configuration.h`. Use numeric coordinates, not a postcode. The public defaults are central London.

`DASH_TIMEZONE` uses a **POSIX timezone rule**, not an IANA string such as `Europe/London`. The default `GMT0BST,M3.5.0/1,M10.5.0` handles UK winter/summer time. Supply an appropriate POSIX rule for your location.

`DASH_CLOCK_PLL=0x07` selects the tested fixed 120 Hz mode (5.27 seconds waveform, with small coloured residue). Set it to `0x08` for the original, cleaner vendor dynamic mode (about 12.1 seconds). Only these two values are accepted. Normal ten-minute full updates keep the vendor frame-rate settings.

Digit-window geometry matches this 1360 × 480 layout and LED font. Changing the clock layout requires updating `clockRow()` and `changedClockWindows()` together, then rerunning `tools/test_clock_windows.sh`.

## Wi-Fi setup

Use `scripts/wifi.sh YOUR_PORT`, or invoke `tools/wifi_setup.py` directly with Python and pyserial. The helper waits for application readiness because opening native USB can reset the board. It accepts only the exact successful-save acknowledgement.

The ESP32 first tries credentials from local setup, otherwise the system's saved Wi-Fi configuration. Credentials are persisted in NVS, survive normal firmware uploads, and are not embedded in source. To replace them, rerun the setup prompt. A complete flash erase removes them.

Only one process should own the serial port. Close the Arduino serial monitor before using the helper. Do not paste passwords into issues or commit them to files.

## Troubleshooting

| Symptom | Check |
|---|---|
| Boot remains in download mode | Use native USB-OTG, release BOOT, and press/release the board's reset button. |
| No image for the first three minutes | The boot cooldown keeps PWR LOW for 180 seconds. |
| `WIFI ... reason=201` | No matching AP was found; verify the exact SSID and 2.4 GHz availability. |
| Repeated association / authentication timeouts | Verify credentials; test a nearby compatible hotspot. The 8.5 dBm TX limit helped on the tested board. |
| Clock / data waiting | Internet access and NTP are needed before certificate-verified HTTPS fetches. |
| `BUSY timeout` or never asserted | Check GPIO4 and display power/wiring against the README. Recoverable display faults get two bounded restart attempts; repeated failures park with the panel powered off and USB recovery available. |
| Other areas change during a clock update | Set `DASH_CLOCK_WINDOW_ENABLED=false` and rebuild for fast full-screen minute updates; inspect the exact panel revision and wiring. |
| UI assets not found | Use the included generated `DashboardAssets.cpp`; regenerate only when intentionally editing fonts/icons. |

NTP time synchronization shares the normal ten-minute data batch; failed synchronization has its own retry backoff. Opening a monitor can reset the board and restart its three-minute cooldown. `NTP_SYNC` confirms a server response; `CLOCK_COMPLETE` includes `offset_ms`, the difference between completion and the intended minute boundary. The first full image is immediate after cooldown and is not a boundary-alignment measurement.

Send `STATUS` followed by newline over serial to report Wi-Fi/time/weather availability without credentials. Logs remain local and are ignored by Git. When sharing diagnostics, remove SSIDs, addresses and other personal information.

## Runtime configuration and maintenance (v21)

Boot defaults come from Configuration.h. Later validated settings are stored in NVS; saving restarts the device and preserves Wi-Fi credentials. Settings records have a version/checksum and invalid records fall back to compiled defaults. Labels use printable ASCII; timezone uses a POSIX rule (an IANA name such as Europe/London is rejected).

macOS/Linux USB setup avoids opening DTR/RTS:

```bash
python3 tools/configure.py /dev/cu.usbmodem2101 --show
python3 tools/configure.py /dev/cu.usbmodem2101 --diagnostics
python3 tools/configure.py /dev/cu.usbmodem2101 --file /path/to/private-settings.json
python3 tools/configure.py /dev/cu.usbmodem2101 --portal
python3 tools/configure.py /dev/cu.usbmodem2101 --close
```

The portal waits for safe display/radio handover, creates Epaper-Setup with a new random password printed in the local USB console, and serves http://192.168.4.1. Join that network on a phone/computer. Login is admin with the same password. The WPA2-protected temporary network is local only, limited to one client; it is not an internet service. Forms require a per-session token. It closes after ten minutes or CONFIG CLOSE. Display/network updates pause during maintenance, then resume. Keep temporary passwords and private settings out of public captures.

The phone page edits settings JSON, saves Wi-Fi without exposing saved passwords, shows diagnostics and accepts application firmware uploads. Secret HA tokens are write-only: the settings page only reports whether a token exists. To remove a token, explicitly save an empty ha_token. With no ha_token property the saved token is preserved.

Example settings (add only the fields you want to change):

```json
{
  "location": "London",
  "timezone": "GMT0BST,M3.5.0/1,M10.5.0",
  "quiet_start": 0,
  "quiet_end": 7,
  "fahrenheit": false,
  "wind_mph": false,
  "dlr_station": "940GZZDLEIN",
  "jubilee_station": "940GZZLUCGT",
  "rtc_enabled": false,
  "gauge_enabled": false
}
```

Latitude and longitude are numeric settings. Quiet hours can cross midnight; matching start/end disables quiet mode. The illustration displays the configured wake hour. Station IDs must match TfL's supported station codes. Different stations use generic selected-station labels; DLR/Jubilee line IDs remain the two supported routes.

### Offline time and battery adapters

Optional I2C uses GPIO17 SDA and GPIO18 SCL at 100 kHz, with 3.3 V logic and common ground. Ensure these pins are unused and select suitable breakout modules before attaching hardware. Display wiring stays the same.

- **DS3231, address 0x68:** enable rtc_enabled. It stores UTC; the configured timezone controls display. Oscillator-stop or invalid calendar values are rejected. A valid battery-backed clock initializes system time before networking; successful NTP later updates it. Without hardware, a cold offline boot still cannot invent current time. Use a backup cell/module compatible with its charging circuit.
- **MAX17048, address 0x36, single cell:** enable gauge_enabled. Voltage and the gauge's estimated state-of-charge appear in diagnostics; percentage appears on full dashboard redraws, with CHARGE at 15% or below. Read failures/old samples are unavailable. Percentage is the gauge's model estimate, not a current measurement or elapsed-time guess. Do not enable this for MAX17049/two-cell hardware. The existing board still needs a suitable battery supply/charger; this adapter does not add that circuit.

Both adapters are off by default. Register decoding is host-tested; no modules were attached during implementation. Battery current and runtime remain unmeasured. The MAX17048 driver retains default gauge compensation; a battery-temperature sensor/calibration is needed for improved accuracy across temperature extremes.

### Optional Home Assistant

Save ha_url (HTTPS), ha_token (long-lived token), and optional ha_temperature / ha_note entity IDs. The URL must use a certificate trusted by the built-in CA bundle; self-signed certificates are not bypassed. Tokens stay on the device and are not returned by diagnostics/configuration. NVS is not encrypted in this development build.

ha_temperature is a numeric sensor in C or F and is converted to selected display units. ha_note is a short sensor/template state, for example a next appointment or indoor-air-quality message. Home Assistant is queried through its REST API during ordinary batched sessions; it does not keep a persistent MQTT connection. Unavailable/expired results are omitted and cannot invalidate weather/TfL. No HA server is required for the core dashboard.

### OTA firmware and rollback

```bash
./scripts/build.sh
python3 tools/package_firmware.py
```

The release folder contains the **application-only** .bin, manifest and SHA256SUMS. Open the temporary portal, choose that .bin and paste its checksum. Upload is authenticated and streaming; invalid checksum/oversize/invalid application images are rejected before activation. Do not upload a merged flash image, bootloader, partitions or NVS image. No partition change is required: two 3 MB OTA slots already exist.

The bundled ESP32 3.3.12 SDK/bootloader enables rollback. Firmware overrides verifyRollbackLater so Arduino does not accept the image immediately. The first successful dashboard draw confirms it; a reset/crash before confirmation lets the bootloader select the previous image. A recorded application fault also requests rollback. The portal offers explicit restore of the known previous slot. SHA-256 detects corruption; it is not a publisher signature. Trust the firmware source and keep the temporary portal password private.

### Diagnostics and recovery

DIAGNOSTICS reports provider success/source timestamps, retry state, NTP sync time, heap, batch version, fault history, optional hardware availability and OTA state. POWER STATUS supplies sleep/radio counters. FAULT RESET clears the consecutive-fault record and restarts. A persistent hardware fault allows at most two automatic attempts with cooldown; then fix wiring/power before manual reset. This protects against repeated rapid panel resets. A normal boot still observes the existing three-minute panel cooldown.
