# Compact two-journey colour dashboard

The selected layout uses only the panel's native black, white, yellow and red. The huge fixed-cell clock occupies the right two thirds; weather and date occupy the left third. Temperature uses the 120-pixel font, with the condition in the 35-pixel font (two lines for multiword conditions). Wind sits below the current-weather icon. The clock and transport geometry is unchanged by this weather-pane refinement.

A 44-pixel transport strip replaces the previous header badges, leaving eight full-width forecast slots below.

| Region | Pixels | Refresh |
|---|---|---|
| Clock | x=472…1339, y=8…243 | Only changed adjacent digit groups; fast fixed-120-Hz waveform. The colon remains unchanged. |
| East India / DLR | x=0…679, y=252…295 | Together with weather on the ten-minute normal full refresh. |
| Canning Town / Jubilee | x=680…1359, y=252…295 | Together with weather on the ten-minute normal full refresh. |
| Weather, forecast, date and footer | Remaining regions | Normal full refresh every ten minutes. |

Automatic updates use one shared ten-minute weather/TfL fetch batch and one scheduled full redraw. Between full redraws, only changed clock digits refresh; transport results and freshness changes are held until the next full redraw. The manual repaint diagnostic retains the separately verified normal-colour transport phase before the fast clock phase. Clock and colour waveform/overhead estimates remain separate.

Inactive controllers receive only a four-pixel known-white padding window at y=0. The renderer and host tests ensure those pixels stay white. Every clock window is horizontally aligned to four pixels and excludes the transport strip and weather; coloured pixels never enter the clock area.

## Status and colour

East India combines DLR network service with East India station notices. Canning Town combines Jubilee network service with Canning Town Underground notices; its DLR notices are not used for this indicator.

- White with a black check and `GOOD`: both relevant results are good and fresh.
- Yellow with `NOTICE`: a station notice, such as an escalator fault. This does not imply train delays.
- Red with `ISSUE`: a line disruption or station closure/suspension. The compact generic label avoids calling every suspension or closure a delay.
- White with a question mark and `OLD` or `UNKNOWN`: unavailable or stale information.

Disruption takes priority over notices, then stale/unknown data, then good service. No failed request can produce `GOOD`. The full-line status is not a guarantee about an individual train or route segment.

Sunshine uses yellow; other weather symbols stay black and white. Forecast rain probabilities of 40% or more receive a yellow highlight. The number is still a probability, not a claim that rain is certain or severe. Green is not supported by this panel.

The footer retains the weather cache/source label and the last successful monitored TfL check time at the last full-frame redraw. It remains still during ordinary minute updates; indicator freshness is evaluated independently on every scheduled update.

## Verification and manual diagnostic

`bash tools/test_clock_windows.sh` verifies all 1,440 clock transitions, midnight and NTP corrections. `bash tools/test_live_data.sh` checks all 50 single-indicator state transitions, merged transport windows, monochrome clock pixels, inactive-controller padding, cache recovery and unavailable states.

The USB command `REPAINT TRANSPORT` requests one normal-colour repaint at the next scheduled update. It keeps the real current values and does not fetch or invent transport data. This lets the physical colour-window behavior be checked without waiting for a real disruption. Afterwards the usual changed-digit cadence resumes. Every refresh ends in sleep with panel PWR LOW.

Physical preservation and ghosting require observation; host pixel tests alone cannot establish them.

## Device result — 6 October 2026

Both builds passed and the uploaded image was hash-verified. The board restored its 48-hour weather cache and fetched real feeds. After a normal full redraw, two single-digit minute updates used the 5.270-second fast waveform. A forced repaint of the live white/yellow transport strip used a 16.950-second normal-colour waveform, followed by the separate fast clock phase; total combined cycle was 29.356 seconds, completing 90 ms before its NTP target. The next minute returned to the fast clock-only mode. The user confirmed the layout, unchanged weather/forecast during both update types, and a clean strip.

See [structured verification](compact-colour-validation.json) and [selected serial capture](compact-colour-serial.txt). Individual half-strip changes and red disruption states were verified by host pixel tests rather than fabricated live status changes.
