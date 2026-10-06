# Full, fast and clock-window updates

## Measured behaviour

| Mode | Waveform BUSY duration | What changes |
|---|---:|---|
| Normal full | 16.97 seconds | Whole dashboard |
| Vendor fast full | 12.10 seconds | Whole dashboard |
| Fast clock window | 12.09 seconds | Clock rectangle only, confirmed visually |

These measure waveform BUSY duration, not complete cycle time. Rendering takes about 0.21 seconds; initialization, frame transfer and sleep add time. Consecutive minute cycles started about 60.2 seconds apart.

## Controller sequence

The panel uses two JD79665AA controllers. Each receives 81,600 bytes of a packed two-bit frame.

Normal full updates use Waveshare's unmodified initialization sequence. Fast mode adds the vendor sequence `E0=03`, `E6=5C`, `A5=00`, then waits for BUSY. It uses the panel's existing waveform rather than a custom LUT.

The controller manual's printed page 40 documents `R83h` (PTL). Its nine parameters contain horizontal and vertical start/end addresses, `PTH_EN` and `PMODE`. For a clock-window update:

1. Initialize the vendor fast mode.
2. Load both complete controller SRAM images with the current clock and cached weather.
3. Send `0x83` to each controller, enabling `PMODE=1`, with `PTH_EN=0` so source outputs follow horizontal bounds.
4. Trigger the refresh with `0x12:0x00`, wait for BUSY to assert and release, then sleep and set PWR LOW.

The global rectangle is **x=472…1339, y=40…323**. Controller M receives x=472…679; controller S receives x=0…659. Horizontal boundaries are aligned to four pixels as required by the hardware.

A partial update here is **optical region selection**. SPI still transfers the full image, since each sleep/reset cycle discards controller RAM. This avoids assuming retained memory or erasing unchanged content. The manual states that gate scanning continues outside the window; physical testing, not the existence of a register alone, established that the surrounding image stayed still on this assembly.

The vendor `DisplayPart()` helper is not used. It fills the area outside its input with white and triggers the regular refresh, rather than preserving the dashboard with `0x83`.

## Scheduling

The first update after a 180-second boot cooldown is normal full-screen. Clock cycles are due 60 seconds from the previous cycle start. When 600 seconds have elapsed since the previous full-cycle start, a fresh weather fetch and normal full refresh replace that minute's window update.

A failed weather fetch keeps cached data. Network calls can delay a cycle; updates never overlap, and there is no catch-up burst. Date and forecast labels change only with full updates.

## What has been established

- Arduino compilation and upload hash verification passed.
- One normal full refresh and two consecutive clock-window updates completed.
- The user confirmed only the clock changed; weather, date and forecasts stayed still.
- The ten-minute full-repeat timer is configured and inspected in code, but was not observed in the short capture.
- Long-term ghosting, ageing and operation across all temperatures/panel revisions are not validated.

Waveshare does not advertise partial refresh for this panel. Its general guidance recommends at least 180 seconds between refreshes, except supported partial-refresh products. This firmware's minute window mode should be treated as a prototype based on the documented controller and a specific successful physical test. Set `DASH_CLOCK_WINDOW_ENABLED=false` for the previously measured fast full-screen mode; increase intervals if following the general vendor cadence.

Sources: [exact panel manual, printed page 40](https://files.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_G/10.85inch_e-Paper_G.pdf#page=43), [vendor driver](https://github.com/waveshareteam/e-Paper/tree/master/E-paper_Separate_Program/10.85inch_e-Paper_G/ESP32), [panel specifications](https://www.waveshare.com/10.85inch-e-paper-hat-plus-g.htm), [usage guidance](https://www.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_%28G%29_Manual).
