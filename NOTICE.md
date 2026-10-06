# Credits and component licensing

## Original integration

The live-data client, clock/timezone integration, scheduling, configuration,
USB Wi-Fi setup, scripts, documentation and new controller-window implementation
are covered by the scoped MIT license in LICENSE.

## Dashboard design and assets

The UI was ported and adapted from
https://github.com/czuryk/Waveshare-ePaper-10.85-dashboard
at commit `ceced5eb8a0c17c58d73064a687f924bc3e3b461`.

The C++ renderer is inspired by and adapts the upstream dashboard's rendering
primitives and visual design. The fonts `Aldrich-Regular.ttc`,
`advanced_led_board-7.ttc`, and the icon BMP files were copied from that source.
`asset-manifest.json` records their hashes. The older font files remain as
historical source assets; the active renderer now uses Oxanium instead.
Current icon bitmaps in `DashboardAssets.cpp` still derive from upstream icons.

The selected field-journal layout was developed from an AI-generated design
concept. `Oxanium-Variable.ttf` was obtained from the Google Fonts Oxanium
directory (https://github.com/google/fonts/tree/main/ofl/oxanium). The font is
Copyright 2019 The Oxanium Project Authors, distributed under the SIL Open Font
License 1.1, included at `assets/Oxanium-OFL.txt`. Generated glyph subsets in
`DashboardAssets.cpp` derive from that font and retain its licensing terms.

That snapshot did not include a top-level license. This repository does not
claim to grant redistribution or relicensing rights for upstream-derived UI
material, fonts, icons or their generated bitmaps. Their presence and attribution
are not a substitute for the original rights holder's permission. Check the
original source and asset-specific terms before redistribution in another project.

## Display driver

`EPD_10in85g.*`, `DEV_Config.*` and `Debug.h` derive from Waveshare's 10.85-inch
four-colour ESP32 example. Their original permission notices are preserved.
The regional-update additions use register descriptions in the exact panel manual.

Source:
https://github.com/waveshareteam/e-Paper/tree/master/E-paper_Separate_Program/10.85inch_e-Paper_G/ESP32

## Services and other dependencies

Open-Meteo provides weather data; the README/footer attribute Open-Meteo.
The current layout does not request or display air-quality data.
Arduino-ESP32, ArduinoJson, pyserial, esptool and Pillow retain their respective
licenses and are installed separately, not vendored here.

## Images

The dashboard preview is a rendering of sample fixtures using the upstream-derived
assets above. The hardware photograph was supplied by the project owner and shows
its initial four-colour bring-up test. It is not a photograph of the final UI.

The sleeping-squirrel illustration in `assets/sleep-squirrel-source.png` was
generated for this project with the built-in image-generation tool on 2026-10-06.
Its prompt is preserved in `assets/sleep-artwork-prompt.txt`; `SleepArtwork.cpp`
is the corresponding native-palette bitmap compiled by `tools/generate_sleep_asset.py`.
It does not use the upstream dashboard's weather-icon assets.
