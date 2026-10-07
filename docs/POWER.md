# Battery power management (v21)

Firmware is implemented and flashed. Builds and host tests pass. Device tests verified minute updates, a one-second light-sleep probe and a 90-second deep-sleep timer wake with dashboard restoration. A bounded battery-profile run also passed; see [validation status](power/validation-status.json). Electrical consumption and a full overnight run remain unmeasured.

A 3000 mAh single 3.7 V cell stores 11.1 Wh nominally. This model reserves 15% for conversion losses: 9.435 Wh usable. No current meter was available; electrical power and battery lifetime below are estimates, not measured results.

## Baseline first

Before these changes, the CPU ran at 240 MHz, Wi-Fi remained associated with modem sleep disabled, BUSY used polling, and night mode only paused application updates. The panel already slept with PWR LOW after refresh. Assuming 0.4–0.7 W for the whole powered system gives **13.5–23.6 hours**, with **18.9 hours** at 0.5 W. This assumption is not a baseline ammeter reading.

## Implemented

- Wi-Fi modem sleep during an active batch, then Wi-Fi and SNTP off between batches. Weather, TfL and NTP share a ten-minute session. Intentional radio-off does not falsely label good cached data offline.
- Normal providers share a single ten-minute batch deadline so differences in request completion do not split Wi-Fi sessions. Failed providers retry independently, backing off from 30 seconds to ten minutes. Association and HTTPS timeouts bound individual operations; the 45-second session budget prevents starting more requests when exhausted. It is a soft budget: an in-flight operation can exceed it.
- CPU uses 80 MHz at idle and 240 MHz for render, hardware SPI transfers and networking. A reference count handles overlapping render/network work. Lowering transfer CPU speed would slow clock refresh, so transfer speed is preserved.
- Explicit light sleep between clock deadlines and while the display is BUSY, with GPIO4 HIGH wake and a timer fallback. RAM/PSRAM and pin levels are retained. Networking owns a sleep lock so an active request is not suspended by display sleep.
- The display sleeps and its signal/power pins are driven LOW after transfer to avoid phantom power through signal leads.
- Real overnight deep sleep after the squirrel image; wake at 06:55 for NTP before the 07:00 dashboard. No weather/TfL fetching during quiet hours. RTC memory retains the panel completion time so timer wake honours the remaining 180-second cooldown. A normal cold boot still waits 180 seconds.
- Fault handling stops network work, switches radio off, lowers CPU frequency and waits without spinning.
- `POWER STATUS` reports radio-session duration, low-CPU time, chip-sleep duration, BUSY sleep and sleep errors. Radio session includes association/CPU work; it is not exact RF transmit time.

The installed Arduino SDK does not enable automatic dynamic-frequency/tickless power management. This implementation therefore uses coordinated explicit sleep instead of claiming `esp_pm_configure()` enables unsupported automatic sleep.

## USB and battery profiles

An observed native USB Serial/JTAG host is latched until reboot and suppresses light/deep sleep to keep the console available. A charger or battery without a USB data host uses the battery profile automatically when booted without a data host. Reboot after unplugging a debug host to clear the latch. Override `DASH_POWER_USB_DEBUG=false` for deployment that must sleep even with a host. Never interpret USB debug current as the battery profile's current.

Serial diagnostics: `POWER STATUS`, `POWER LIGHT TEST` (one-second bounded sleep probe), `POWER DEEP TEST` (draw squirrel, deep sleep for 90 seconds, timer wake, retained cooldown, full restore). USB can disconnect during these tests; reconnect without asserting DTR/RTS. `POWER BATTERY TEST` forces the battery profile for 120 seconds, finishing after any active display cycle. It saves sleep duration, errors, clock cycles and worst completion offset to NVS so `POWER STATUS` or the next boot can report the result even if USB logging disappears. `POWER DEBUG` ends it early and marks it incomplete. Normal production operation does not write diagnostics to flash every minute. Console output has zero blocking timeout so an absent host cannot stall the clock.

For passive capture on macOS/Linux, use `python3 tools/power_monitor.py /dev/cu.usbmodem2101 --seconds 700 --command "POWER BATTERY TEST" --after-refresh 1`. It avoids DTR/RTS reset; USB sleep can still require reattachment or a deliberate reset. Standard serial tools may reset this board on opening the port. The worker is notified from monotonic elapsed time after manual sleep, avoiding dependence on RTOS tick advancement while both CPUs are asleep.

## Lifetime model

Run `python3 tools/estimate_battery.py`; assumptions and results are saved in [power/battery-estimate.json](power/battery-estimate.json).

The model keeps the requested one-minute clock and ten-minute full weather/TfL schedule. Per ten daytime minutes it uses nine 1.66-second clock/render overheads plus one 1.62-second full/render overhead, nine 5.3-second clock BUSY periods and one 17-second full BUSY period. An extra hour-rollover BUSY pass is included conservatively; cold boot and the nightly redraw add small unmodelled overhead. Radio-session assumptions are 10.275/12.255/30 seconds per batch, using observed successful sessions for the first two scenarios. Added radio power is counted conservatively even when it overlaps render work. Whole-board overhead assumptions are 10/25/50 mW; LEDs, regulators and HAT leakage can dominate real sleep consumption.

| Scenario | Average power estimate | 3000 mAh runtime estimate |
|---|---:|---:|
| Baseline central | 500 mW | 18.9 hours |
| Improved conservative | 85.4 mW | 4.6 days |
| Improved central | 43.9 mW | 9.0 days |
| Improved optimistic | 23.7 mW | 16.6 days |

The central model predicts about **91% lower average power** and **11× runtime**. Treat **4.6–16.6 days** as scenarios, not a confidence interval or verified battery test. Long Wi-Fi outages, converter quiescent current, board LEDs and cell cutoff can reduce it substantially. With the same central assumptions but 100–150 mW of board overhead, runtime drops to **3.3–2.3 days**. Measuring the complete board matters more than chip-only datasheet sleep current.

## Device evidence

Measured on the ESP32, without a current sensor:

- Idle CPU: 80 MHz; active work: 240 MHz.
- One-second light-sleep probe: 991 ms asleep, one successful sleep, zero reported errors.
- Deep-sleep probe: 90-second timer wake, retained remaining panel cooldown about 90 seconds, full dashboard restoration and subsequent clock updates.
- Successful radio sessions: 10.275, 10.770, 12.913 and 15.063 seconds. Mean 12.255 seconds, about 2% of a ten-minute interval. This includes association and request processing, not just RF transmit time.
- v20 clock waveform: 5.270 seconds; single-window total: 8.980 seconds. v21 hardware SPI: the waveform remains 5.270 seconds but total falls to 6.933 seconds (23% faster). Full total falls from 20.659 to 18.600 seconds; measured waveform 16.971 seconds. These are USB debug timings, not battery-current measurements.
- Steady clock completion offsets after prediction adapted: 2–6 ms in short captures. This is refresh completion timing against the device NTP clock, not an independent clock accuracy measurement.
- User confirmed the physical clock kept advancing during the two-minute battery-profile run, with weather and transport unchanged. Recovered v20 counters: 120.168 seconds elapsed, 112.552 seconds in light sleep (93.7%), including 10.557 seconds during BUSY; 117 sleeps, zero errors, two clock cycles, worst completion offset 202 ms. USB required a physical reconnect, but the saved summary survived.

## Hardware work still requiring hardware

No regulator/LED was removed and no battery circuit was added. A lower-quiescent-current regulator/boost converter, disconnectable LEDs, a switched HAT supply, and an external low-power RTC can reduce overhead further. These require identifying and modifying the board or adding parts; software cannot implement them. A protected cell and suitable regulated supply are needed; the 3.7 V cell is an energy-model assumption, not permission to wire it directly to a 3V3/USB pin. Validate current at the battery terminals and supply conversion efficiency before relying on runtime.

## Sources and practical limits

[ESP32-S3 datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf) specifies chip-only low-power consumption; PSRAM and board losses must be added. [Espressif sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html) describes explicit sleep, Wi-Fi shutdown, GPIO wake and pad retention. [Exact-panel specification](https://files.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_G/10.85inch_e-Paper_G.pdf) supplies the 60 mW typical operating assumption; this is not a measurement of our HAT or experimental waveform. RTC drift through deep sleep is corrected by early-morning NTP when the network is available. A seven-hour physical overnight test remains necessary.

The v21 SPI change reduces modelled CPU work per ten minutes from about 37 seconds to 17 seconds. Keeping the same electrical assumptions, central runtime rises from 7.89 to 8.96 days (about 14%). Optional RTC/fuel-gauge modules and Home Assistant requests add unmeasured load; they are disabled in this scenario. This is a model update using measured durations, not a measured energy saving.
