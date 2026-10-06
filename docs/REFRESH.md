# Full, fast and clock-window updates

## Measured behaviour

| Mode | Waveform BUSY duration | What changes |
|---|---:|---|
| Normal full | 16.97 seconds | Whole dashboard |
| Vendor fast full | 12.10 seconds | Whole dashboard |
| Vendor dynamic clock window | 12.09 seconds | Clock rectangle only, confirmed visually |
| Fixed 120 Hz clock window (current default) | 5.27 seconds | Clock rectangle only; small coloured residue observed and accepted |

These measure waveform BUSY duration, not complete cycle time. Rendering takes about 0.21 seconds; initialization, frame transfer and sleep add time. Consecutive minute cycles started about 60.2 seconds apart.

## Controller sequence

The panel uses two JD79665AA controllers. Each receives 81,600 bytes of a packed two-bit frame.

Normal full updates use Waveshare's unmodified initialization sequence. Fast mode adds the vendor sequence `E0=03`, `E6=5C`, `A5=00`, then waits for BUSY. It uses the panel's existing waveform rather than a custom LUT.

The controller manual's printed page 40 documents `R83h` (PTL). Its nine parameters contain horizontal and vertical start/end addresses, `PTH_EN` and `PMODE`. For a clock-window update:

1. Initialize the vendor fast mode. For the faster clock setting, send `R30=07` afterward to disable dynamic frame rate and select the documented 120 Hz rate. `DASH_CLOCK_PLL=0x08` restores vendor dynamic mode.
2. Load both complete controller SRAM images with the current clock and cached weather.
3. Compare the clock with the last successfully displayed `HH:MM`. Generate a rectangle for each changed digit, merging adjacent changed digits. Send `0x83` with `PMODE=1`, `PTH_EN=0` so source outputs follow horizontal bounds.
4. Trigger the refresh with `0x12:0x00`, wait for BUSY to assert and release, then sleep and set PWR LOW.

The allowed clock region is **x=472…1339, y=40…323**. Actual windows are computed from the LED font advances and the same fitting calculation as the renderer. Horizontal boundaries are rounded outward to four pixels as required by the controller.

Each window is split at x=680 into local controller coordinates. Refresh commands still go to both controllers because BUSY is shared. If one controller has no changed digit, it receives a minimal **four-pixel white padding window** at y=40 above the clock glyphs, rather than a window containing an unchanged digit. Firmware checks that this padding is white before driving it.

The hardware supports one window per controller per waveform. Separate changed groups, such as hour digits and minute digits with an unchanged colon between them, therefore use separate passes. Most minutes need one pass; an hour rollover can need two, increasing duration. This narrows the affected area; it does not promise a shorter waveform.

The previous clock string is committed only after refresh and sleep complete successfully. Initial and periodic full refreshes reset that reference; reboot starts with a full refresh. An unchanged time skips the minute update while leaving the full-refresh deadline intact. Forward/backward time corrections are compared with the actual last displayed time, so the logic does not assume time always increments normally.

A partial update here is **optical region selection**. SPI still transfers the full image, since each sleep/reset cycle discards controller RAM. This avoids assuming retained memory or erasing unchanged content. The manual states that gate scanning continues outside the window; physical testing, not the existence of a register alone, established that the surrounding image stayed still on this assembly.

The vendor `DisplayPart()` helper is not used. It fills the area outside its input with white and triggers the regular refresh, rather than preserving the dashboard with `0x83`.

## Scheduling

The first update after a 180-second boot cooldown is normal full-screen. Subsequent clock updates follow **NTP wall-clock minute boundaries**, not a repeating delay measured from boot.

The scheduler prepares the upcoming minute’s digits and starts before its boundary, estimating the complete render, initialization, transfer, waveform and sleep duration. It learns the full-cycle duration and the per-pass clock waveform/overhead from successful cycles. Separate changed digit groups increase the lead time. The goal is for the image to settle near the minute boundary, rather than beginning its several-second refresh afterward. During the transition it may show incomplete or upcoming digits.

Periodic full updates target ten minutes after the last full image timestamp, with a monotonic fallback following a backward time correction. The first image uses its estimated completion time; it is not delayed to wait for a minute boundary. Late wakes and significant forward/backward NTP corrections are handled using current wall time, with no catch-up burst.

NTP resynchronizes every **15 minutes** using the existing three servers. `NTP_SYNC` reports actual synchronization callbacks. `CLOCK_SCHEDULE` and `CLOCK_COMPLETE` report the target, actual start/finish timestamps and completion offset in milliseconds. Completion includes sleep and PWR LOW, so the physical image may settle slightly earlier. This is measured timing compensation, not a guarantee of exact visual synchronization at every temperature.

Weather requests are attempted early in a minute, away from the normal clock deadline; the extra synchronous fetch immediately before a full refresh was removed. Cached data is displayed until the next full refresh. Slow network requests, reconnects or firmware stalls can still delay a cycle; if that happens, the scheduler catches up to wall time. Failed requests retain cached weather.

## What has been established

- Arduino compilation and upload hash verification passed.
- One normal full refresh and two consecutive clock-window updates completed.
- The user confirmed only the clock changed; weather, date and forecasts stayed still.
- A fixed-120-Hz clock waveform completed in 5.270 seconds. The user observed small coloured residue and accepted it. The following vendor dynamic clock cycle took 12.090 seconds. See [speed research](SPEED_RESEARCH.md) and [the measurement record](speed-validation.json).
- The ten-minute full-repeat timer is configured and inspected in code, but was not observed in the short capture.
- Long-term ghosting, ageing and operation across all temperatures/panel revisions are not validated.

Waveshare does not advertise partial refresh for this panel. Its general guidance recommends at least 180 seconds between refreshes, except supported partial-refresh products. This firmware's minute window mode should be treated as a prototype based on the documented controller and a specific successful physical test. Set `DASH_CLOCK_WINDOW_ENABLED=false` for the previously measured fast full-screen mode; increase intervals if following the general vendor cadence.

Sources: [exact panel manual, printed page 40](https://files.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_G/10.85inch_e-Paper_G.pdf#page=43), [vendor driver](https://github.com/waveshareteam/e-Paper/tree/master/E-paper_Separate_Program/10.85inch_e-Paper_G/ESP32), [panel specifications](https://www.waveshare.com/10.85inch-e-paper-hat-plus-g.htm), [usage guidance](https://www.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_%28G%29_Manual).

## Digit-window verification

Run `./tools/test_clock_windows.sh`. It renders every one of the 1,440 daily minute transitions and checks that each changed pixel falls inside an aligned window, including midnight. It also checks unchanged time, forward/backward corrections, single-digit changes, minute carry and separate hour/minute groups, with the colon excluded. These software checks do not replace physical confirmation of preservation and ghosting.

On the device, `12:02 → 12:03` produced one group: controller S received x=456…651 (global x=1136…1331), y=40…323, while controller M received only its four white padding pixels. The waveform completed in 5.270 seconds. See [the capture summary](digit-window-validation.json). Visual confirmation and a physical two-pass rollover capture are separate from the software tests.

## NTP alignment capture

An actual `NTP_SYNC` callback was observed after upload. The next two minute transitions, `12:13 → 12:14` and `12:14 → 12:15`, completed **47 ms** and **14 ms** after their respective NTP wall-clock boundaries. Complete clock cycles measured **8.958 s** and **8.959 s**, including the 5.27-second waveform plus rendering, initialization, transfer and sleep. The scheduler started those cycles with 8.928 s and 8.958 s of lead time. See [the measurement record](time-sync-validation.json).

These are cycle-end timestamps on the device, not external optical measurements or a guarantee of NTP server accuracy. The 15-minute resync interval is configured; the short capture observed the initial sync only.
