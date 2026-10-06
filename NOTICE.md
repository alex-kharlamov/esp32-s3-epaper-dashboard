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
`asset-manifest.json` records their hashes. Generated bitmaps in
`DashboardAssets.cpp` derive from those assets.

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

Open-Meteo weather and air-quality APIs provide modelled data. The README/footer
attribute Open-Meteo. Air-quality API data also derives from CAMS; consult
https://open-meteo.com/en/docs/air-quality-api for data attribution and usage terms.
Arduino-ESP32, ArduinoJson, pyserial, esptool and Pillow retain their respective
licenses and are installed separately, not vendored here.

## Images

The dashboard preview is a rendering of sample fixtures using the upstream-derived
assets above. The hardware photograph was supplied by the project owner and shows
its initial four-colour bring-up test. It is not a photograph of the final UI.
