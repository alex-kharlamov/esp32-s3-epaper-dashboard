# Captured API fixtures

These fixed public responses were captured on 6 October 2026 for deterministic tests. The tests use the weather fixture's time, not the machine's current date. They are never used by live firmware.

- `weather.json`: Open-Meteo two-day UTC forecast for the repository's generic London coordinates, with current temperature/wind/condition and hourly temperature/rain probability/condition/day flags.
- `tfl-lines.json`: TfL DLR/Jubilee line status; both reported good service.
- `tfl-stations.json`: TfL disruption records for `940GZZDLCGT`, `940GZZLUCGT` and `940GZZDLEIN`. Canning Town Underground had a reduced-escalator-service Information notice; the monitored DLR stations had no returned notices.

Sources: [Open-Meteo](https://open-meteo.com/en/docs), [TfL Unified API](https://tfl.gov.uk/info-for/open-data-users/unified-api). No credentials or private configured coordinates are included.
