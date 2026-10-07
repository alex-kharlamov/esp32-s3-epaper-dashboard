# Firmware checkpoint — 7 October 2026

v21 implements the ten improvements and has been flashed with hash verification. It booted normally on USB-OTG, restored caches, and obtained fresh weather/TfL/NTP in one 10.373-second startup session. The v21 implementation, power work and validation evidence are included in the repository; use Git history for publication status.

Public source is in `outputs/esp32-s3-epaper-dashboard`; private deployment is in `outputs/ESP32S3_Dashboard/ESP32S3_Dashboard`. Only Configuration.h differs, preserving private coordinates. Never publish deployment logs, credentials, settings exports or temporary portal passwords.

Public/private builds pass: 1,561,095 bytes program, 55,436 bytes globals. Host regression suites pass, including cache migration, independent retries, shared normal batch deadlines, HTTP bounds, settings, adapter decoding and TfL explanation priority. See [all improvements](../IMPROVEMENTS.md), [setup](../SETUP.md) and [validation status](validation-status.json).

Hardware SPI measured 6.933-second single-window cycles versus 8.980 seconds previously. Waveform remains 5.270 seconds. Full cycle measured 18.608 seconds, finishing 13 ms after target. Repeated minute completions were 3–8 ms after the device NTP boundary. These are software timing observations, not independently measured time accuracy or electrical current.

Temporary setup AP opened and closed with safe radio/display handover; password was withheld from saved validation evidence. No actual phone HTTP client, OTA upload/rollback, Home Assistant server, RTC or fuel gauge has been tested. RTC/gauge and HA remain disabled by default. First SPI prototype fault was detected and corrected; two recorded faults remain until 30 healthy minutes clear the consecutive counter.

The two-minute v20 battery-profile test passed (120.168 seconds, 112.552 seconds light sleep, 117 sleeps, zero errors, two cycles); current firmware inherits its power policy. Full overnight sleep, physical module tests, battery current and actual runtime remain outstanding. A 3000 mAh 3.7 V cell is only the energy-model assumption.

Current lifetime model central estimate: 8.96 days (previous v20 7.89 days), using measured SPI durations and unchanged assumed electrical powers. Scenarios 4.61–16.56 days are not confidence bounds. Optional modules/integrations add unmeasured load.

Native USB is `/dev/cu.usbmodem2101`. Passive POSIX capture avoids DTR/RTS; ordinary pyserial attachment may reset the board. Cold boot waits 180 seconds. Do not repeatedly reflash just to inspect status. Main source, tests, package/checksum and documentation are saved locally.

Final board capture completed: normal batch requested all four providers together. A transient association failure was retried after backoff; weather/TfL/NTP then succeeded in one 8.779-second session. Minute clock remained accurate through the outage (final steady offsets 4–7 ms). Final diagnostics confirmed all four providers fresh, radio/maintenance off and no new faults. No serial monitor or setup AP remains active. Sanitized evidence is saved beside this checkpoint.
