# Startup diagnosis — 7 October 2026

User reconnected board and reports no display updates after Type-C power-on. Diagnosis is incomplete; awaiting single-cable USB-OTG/power-cycle check.

Observed native device `/dev/cu.usbmodem2101`. Several serial captures report `rst:0x15 (USB_UART_CHIP_RESET),boot:0x20 (DOWNLOAD(USB/UART0))` and `waiting for download`. The dashboard cannot run in that mode. Normal firmware contains a 180-second cold-boot panel cooldown; this is a separate delay, not an explanation for ROM download mode.

Attempted ordinary software reset, user RET press, supported esptool flash-id/reset, and documented `--after watchdog-reset` recovery. No successful application boot or redraw has yet been captured. Serial attachment may itself affect native USB boot controls; causal attribution remains unconfirmed. A passive-flow-control monitor was inconclusive and must not be treated as validated. A raw termios monitor leaving DTR/RTS untouched also captured no application output. No firmware was modified/reflashed during this diagnosis.

Read GPIO input register via esptool: `0x6000403c = 0x3c000008`. Bit 0 is zero, but input-enable/mux state was not validated, so this is **not sufficient evidence** to assert a physical GPIO0 short.

Espressif documents an S3 manual-download/native-USB latch requiring `--after watchdog-reset`: https://docs.espressif.com/projects/esptool/en/latest/esp32s3/troubleshooting.html . This recovery was attempted without successful boot confirmation.

Required next check: only one power/data cable in USB-OTG, nothing in USB-TTL, BOOT untouched, then full disconnect/reconnect power and RET if needed. Observe reset/boot log without unnecessary serial control changes. Confirm `SPI_FAST_FLASH_BOOT`, v18 banner, successful NTP and first redraw after cooldown, then minute updates. If download mode persists with one cable, physically inspect BOOT/GPIO0 and board reset circuit. If firmware boots but remains silent/not rendering, resume main-loop/power-lock diagnostics from RESUME.md.

All diagnostic monitors were closed. Private logs: ../ESP32S3_Dashboard/serial.log, power-validation.log and startup-validation.log. Temporary raw_monitor.py/passive_monitor.py are exploratory tools, not public validated utilities. No task-completion claim or battery/sleep validation is warranted yet.

## Single-cable full power-cycle follow-up

After the user completed the full disconnect/reconnect check, capture reports SPI_FAST_FLASH_BOOT and v18 startup. Saved Wi-Fi connects, current weather and both TfL requests succeed, and NTP reports epoch 1791387411.669036. Radio session finishes after 10,275 ms. The initial screen and subsequent clock update are still being observed through the 180-second cold-boot delay. The exact earlier download-mode trigger is not proven; this recovery establishes that normal application boot is possible following the single-cable power cycle.

## First redraw confirmed

First capture completed normal-colour redraw at displayed 16:40: cycle 20,659 ms, waveform 16,981 ms, first frame offset +1,653 ms, no display error, CPU returned to 80 MHz. Reopening the standard pyserial monitor caused another `USB_UART_CHIP_RESET`/normal SPI boot, restarting the cooldown. A concurrent raw POSIX capture leaving modem controls untouched remained attached after the normal monitor exited and successfully captured the subsequent full redraw (16:44): cycle 20,658 ms, waveform 16,981 ms. Minute-window verification follows. A cold first frame can finish late relative to its target if the cooldown expires just before a minute boundary; ordinary predictive minute scheduling has not yet been checked in this follow-up.

## Automatic minute update confirmed

16:44 -> 16:45 selected exactly one clock window; waveform 5,270 ms, total cycle 8,979 ms, settled 212 ms after the NTP target. Weather/transport were not scheduled for refresh in that cycle. CPU returned to 80 MHz. No source/firmware changes were necessary for this recovery. A single-cable full power cycle restored normal boot; exact earlier download-mode trigger remains unproven. At power-on allow 180 seconds cooldown plus about 21 seconds full refresh. Subsequent clock updates are automatic every minute, full/weather/TfL batches every ten minutes. Actual night and chip light/deep-sleep verification remain outstanding.
