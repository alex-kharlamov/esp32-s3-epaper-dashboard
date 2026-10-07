# Ten improvements in v21

The existing layout, changed-digit minute refresh, ten-minute full refresh and midnight–07:00 squirrel screen remain the defaults. The implementation runs on the ESP32 without a rendering server.

| Improvement | Result | Validation |
|---|---|---|
| Independent data health | Weather, TfL lines, stations, NTP and optional Home Assistant keep separate timestamps and retry backoff. A failure does not invalidate unrelated data. | Failure/caching tests; live diagnostics. Board recovered from a transient Wi-Fi failure and fetched weather/TfL/NTP together on retry. |
| Useful transport explanations | Both existing indicators can include a short disruption reason or upcoming notice, including station accessibility information. | Real TfL fixtures, future/expired notices and rendering tests. |
| Offline clock continuity | An optional DS3231 can supply UTC after a cold offline boot; NTP updates it when available. An oscillator-stop flag prevents invented time. | Adapter decoding tests; physical RTC still required. |
| Faster display transfer | Hardware SPI sends buffered rows at 4 MHz. Changed clock digits still use their own refresh windows. | Board: single-window total 6.933 seconds, previously 8.980 seconds. |
| More useful weather | Footer includes freshness, feels-like temperature, gusts and the next forecast hour with rain probability above 50%. Units can be changed. | Weather/cache and renderer tests; live weather requests. |
| Runtime setup | USB commands and a temporary password-protected phone setup portal configure location, units, quiet hours, station IDs and integrations. Settings persist without rebuilding. | Validation tests; firmware builds. See setup instructions. |
| Recovery and diagnostics | Persistent fault reason/count, bounded restart attempts, provider diagnostics and heap reporting; a fatal fault leaves USB recovery available. | The initial SPI initialization failure was recorded and corrected during board validation. |
| Battery visibility | An optional MAX17048 provides measured voltage and its estimated state of charge, with a low-battery footer warning. No voltage-only percentage is invented. | Register-decoding tests; physical gauge still required. |
| Updates and rollback | Temporary setup portal accepts an application image with a required SHA-256; dual application slots and deferred acceptance protect failed first boots. | Firmware builds and linked rollback override verified; an actual phone upload/rollback test remains outstanding. |
| Home Assistant | Optional HTTPS REST entities add an indoor temperature and a short note. They share the normal data batch and have independent failure handling. | Request/parsing paths compile; a configured server is still required for end-to-end validation. |

## Using the new features

Follow [runtime setup](SETUP.md#runtime-configuration-and-maintenance-v21). Existing Wi-Fi credentials and weather cache are retained. An older weather cache is migrated after checksum/shape validation. TfL cache is restored with an OLD state until a fresh request succeeds.

RTC, fuel gauge and Home Assistant are disabled by default. The firmware cannot create a battery charger, add physical modules or measure current without equipment. Use the [power model](POWER.md) as a scenario estimate, not a measured battery runtime.

Setup temporarily pauses display/network work to give configuration and firmware uploads exclusive access. Close setup or allow its ten-minute expiry to resume the dashboard. Saving settings restarts the board; the panel's 180-second cold-boot cooldown still applies.

## Verification limits

Host checks cover clock transitions, quiet hours/DST, provider failures, cache corruption/migration, bounded HTTP bodies, settings, adapter registers, TfL fixtures, colour-window containment and renderer guards. Public and private firmware builds pass (1,561,095 bytes program; 55,436 bytes globals). The temporary setup AP handover and close were exercised on the board, without changing its saved settings. GitHub CI is included but has not run until these local changes are published.

Physical RTC/fuel-gauge operation, Home Assistant, a phone-mediated OTA and rollback, a complete overnight run, long-term ghosting and battery current remain unverified. The experimental partial waveform is not vendor-qualified for this panel.
