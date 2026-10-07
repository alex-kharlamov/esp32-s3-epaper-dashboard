# Independent clock, weather cache and London transport

The renderer remains on the ESP32. A background FreeRTOS task fetches weather and TfL data; it publishes small snapshots under a mutex. The foreground display loop performs no HTTP requests or flash writes, so a slow API does not block the NTP-aligned clock schedule.

## Startup and offline weather

The clock no longer requires a successful weather request. After the existing 180-second panel cooldown, it can render valid time with `WEATHER UNAVAILABLE` and empty forecast slots. When a cold boot has no valid time, it renders `TIME WAITING` rather than guessing the time. During a network outage the already-running system clock continues; accurate time after a complete power loss still requires NTP or a separate battery-backed RTC.

Each successful weather fetch saves a pointer-free snapshot in the `dashboardwx` NVS namespace. It includes the fetch/condition timestamps, configured coordinates, temperature, wind, weather code and up to 48 UTC hourly forecasts. It never contains Wi-Fi credentials. A version, size, checksum and value/location checks reject incompatible, damaged or wrong-location data. NVS blob writes preserve the previous committed value if a write fails.

On restart the last good snapshot is loaded before networking starts. The footer labels restored data `CACHED` with its original download date/time. Current weather can remain visible while offline; the forecast selects upcoming UTC hours at rendering time. Hours outside the saved range show placeholders, so yesterday's forecast is never shifted into today's future slots. Weather still refreshes every ten minutes, with a 30-second retry interval after a failed request. No synthetic live weather fallback is used.

## TfL status indicators

The two indicators are **East India / DLR** and **Canning Town / Jubilee**. Each combines line-wide service with the relevant station notices; Canning Town uses its Underground stop record, while East India is DLR-only. A disruption takes precedence over a notice, then stale/unknown data, then good service. Only a successful good line result plus a successful clear station result can yield `GOOD`.

| Label | Meaning |
|---|---|
| `GOOD` | A fresh successful TfL response reports good line service, or no current station notices. |
| `NOTICE` (yellow) | A current station notice, including lift/escalator faults; it does not imply train delays. |
| `ISSUE` (red) | A current line disruption or station closure/suspension. |
| `OLD` | Previously obtained data is unavailable, a subsequent request failed, Wi-Fi is disconnected, or the result is over thirteen minutes old (ten-minute polling plus a three-minute allowance). |
| `?` | No usable result has been obtained yet. |

Future and expired disruption periods are excluded. Multiple line statuses are combined conservatively: an active disruption takes precedence over a simultaneous good-service entry. Missing/malformed API data cannot yield `OK`.

TfL and weather are fetched together every ten minutes and displayed together on the normal full refresh. The intervening minute updates change only clock digits; transport indicators remain as shown at the last full redraw. Failed weather requests can still retry after 30 seconds without triggering additional TfL requests. A failed or stale transport result is shown at the next full refresh. The USB repaint diagnostic is explicitly manual.

The line badges describe the full DLR/Jubilee network, not a route-specific guarantee for the two stations. Station `OK` means no current notices were returned for those monitored stop records. It does not guarantee an individual train arrival.

Endpoints:

- `https://api.tfl.gov.uk/Line/dlr,jubilee/Status?detail=true`
- `https://api.tfl.gov.uk/StopPoint/940GZZDLCGT,940GZZLUCGT,940GZZDLEIN/Disruption?includeRouteBlockedStops=false`

Data and attribution: [TfL Unified API](https://tfl.gov.uk/info-for/open-data-users/unified-api), [official API schema](https://api.tfl.gov.uk/swagger/docs/v1), and [Open-Meteo](https://open-meteo.com/en/docs). Public unauthenticated endpoint access was tested during implementation; a rejection is shown as unavailable/stale, not good service.

## Checks

Run `bash tools/test_live_data.sh` after setup. It uses captured public fixtures and AddressSanitizer/UndefinedBehaviorSanitizer for cache corruption/version/location checks, real-response parsing, offline/expired rendering, future/expired station notices, failure freshness, and dirty-region pixel coverage for all 50 combined-indicator transitions. `tools/test_clock_windows.sh` continues to check all 1,440 clock minute transitions and NTP corrections. Fixtures are fixed test data, not runtime transport information.

`DASH_TRANSPORT_INTERVAL_MS` and `DASH_TRANSPORT_STALE_SECONDS` are defined in `Configuration.h`. The private deployed sketch retains its original weather coordinates; public defaults remain generic London.

## Earlier v12 device verification

The combined Field Journal firmware was built and flashed on 6 October 2026. The device fetched real weather, saved it, and restored 48 forecast hours before Wi-Fi connected after an explicit restart. Live TfL requests succeeded for both lines and all monitored stations. Two consecutive one-window clock updates completed 33 ms and 18 ms after their NTP minute boundaries. The startup full refresh completed normally; it was not a minute-alignment measurement.

[Recorded results](offline-transport-validation.json) and [selected serial evidence](offline-transport-serial.log) distinguish device observations from host tests. Cache recovery was observed on a connected-board restart; offline rendering and transport header transitions were verified with host fixtures rather than a forced hardware outage.

## v21 provider independence and detail

Weather, TfL lines, TfL stations and NTP keep independent health/retry state. A weather failure cannot make a successful TfL check stale. Detailed severity and bounded reason text are retained; upcoming periods within seven days supply planned notices. Station/line state is cached with checksum/version and configured station IDs, but restored checks are always old until confirmed by the API. Current weather source time expires after two hours even when the download was recent. Ten-minute batches and minute clock-only windows remain the default.
