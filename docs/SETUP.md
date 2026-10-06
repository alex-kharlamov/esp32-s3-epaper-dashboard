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
| `BUSY timeout` or never asserted | Check GPIO4 and display power/wiring against the README. The driver stops rather than repeatedly refreshing. |
| Other areas change during a clock update | Set `DASH_CLOCK_WINDOW_ENABLED=false` and rebuild for fast full-screen minute updates; inspect the exact panel revision and wiring. |
| UI assets not found | Use the included generated `DashboardAssets.cpp`; regenerate only when intentionally editing fonts/icons. |

NTP time synchronization is automatic every 15 minutes. Opening a monitor can reset the board and restart its three-minute cooldown. `NTP_SYNC` confirms a server response; `CLOCK_COMPLETE` includes `offset_ms`, the difference between completion and the intended minute boundary. The first full image is immediate after cooldown and is not a boundary-alignment measurement.

Send `STATUS` followed by newline over serial to report Wi-Fi/time/weather availability without credentials. Logs remain local and are ignored by Git. When sharing diagnostics, remove SSIDs, addresses and other personal information.
