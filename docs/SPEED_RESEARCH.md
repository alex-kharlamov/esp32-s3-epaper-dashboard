# Can the clock refresh in two seconds?

The dashboard is already drawn using black and white pixels. Its framebuffer still uses the controller’s native two-bit pixel format. Avoiding red and yellow pixels does not, by itself, select a shorter waveform.

## GxEPD2 comparison

The inspected GxEPD2 snapshot is `de82887e77a78528ea386e68bba2cb0291d2c319`.

- `GxEPD2_1085_GDEM1085T51` is a different 10.85-inch **monochrome** panel using JD79686AB. Its header gives a 700 ms partial-refresh timing. Those settings are not interchangeable with this four-colour JD79665AA panel.
- `GxEPD2_750c_GDEM075F52` uses JD79665AA, but is a different 800 × 480 assembly. Its header explicitly sets `hasFastPartialUpdate=false`; full and partial timings are both 21 seconds. Its optional fast waveform is comparable to the vendor fast mode we already use, rather than a differential black/white mode.
- `GxEPD2x_FastBlackWhiteOnColor` targets selected **three-colour** panels, not this four-colour model.

The exact panel manual documents a partial window and a PLL frame-rate register. It does not provide a ready-to-use two-second black/white waveform. A window limits where source outputs drive the panel; it does not automatically shorten waveform phases or gate scanning.

## Measurement

A one-shot experiment writes volatile register `R30=07` after vendor fast initialization: dynamic frame rate disabled, fixed 120 Hz, the highest listed rate in the exact panel manual. Normal full refreshes remain unchanged. The following clock cycle automatically returns to the vendor dynamic frame rate through reset and initialization. The faster value is now selectable using `DASH_CLOCK_PLL`: `0x07` for fixed 120 Hz, `0x08` for vendor dynamic. The faster setting is the current default after the tester accepted its small coloured residue. The one-shot probe itself is not part of the firmware.

| Mode | Measured waveform BUSY duration |
|---|---:|
| Normal full, before probe | 16.961 s |
| Fast clock window, fixed 120 Hz (`R30=07`) | 5.270 s |
| Vendor dynamic clock window, following probe | 12.090 s |

The fixed-rate experiment substantially shortened the waveform, but did not reach two seconds. These measurements exclude rendering, initialization, SRAM transfer and sleep. The tester reported small coloured residue and accepted that tradeoff. Repeated-use durability remains unvalidated; BUSY releasing alone does not prove a clean image. The tested clock window already contained only black and white pixels.


## Sources

- [Exact panel manual, PLL on printed page 25 and partial window on page 40](https://files.waveshare.com/wiki/10.85inch_e-Paper_HAT%2B_G/10.85inch_e-Paper_G.pdf)
- [GxEPD2 monochrome 10.85-inch driver](https://github.com/ZinggJM/GxEPD2/blob/de82887e77a78528ea386e68bba2cb0291d2c319/src/gdem/GxEPD2_1085_GDEM1085T51.h)
- [GxEPD2 driver with JD79665AA](https://github.com/ZinggJM/GxEPD2/blob/de82887e77a78528ea386e68bba2cb0291d2c319/src/epd4c/GxEPD2_750c_GDEM075F52.h)
- [Fast black/white on selected three-colour panels](https://github.com/ZinggJM/GxEPD2/blob/de82887e77a78528ea386e68bba2cb0291d2c319/examples/GxEPD2x_FastBlackWhiteOnColor/GxEPD2x_FastBlackWhiteOnColor.ino)

## Result

Drawing in black and white did not unlock GxEPD2’s differential refresh on this panel. The documented PLL setting cut the measured waveform from 12.09 to 5.27 seconds. A two-second update has not been demonstrated. Reaching that target would require a shorter waveform qualified for this panel, or a different panel with documented fast black/white partial refresh. The driver does not cut power early, exceed documented PLL rates, or alter persistent waveform memory.
