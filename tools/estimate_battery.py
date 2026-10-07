#!/usr/bin/env python3
"""Battery scenarios: measured durations combined with ASSUMED electrical powers."""
import json
from pathlib import Path
usable_wh = 3 * 3.7 * .85
clock_cycle = 6.933
clock_busy = 5.270
full_cycle = 18.600
full_busy = 16.981
radio_samples = [10.275, 10.770, 12.913, 15.063]
central_radio = sum(radio_samples) / len(radio_samples)
# One extra hour/minute waveform per hour; normal colour refresh replaces
# that clock pass at ten-minute boundaries, so including this is conservative.
transfer_seconds = 9 * (clock_cycle - clock_busy) + full_cycle - full_busy
panel_seconds = 9 * clock_busy + full_busy + clock_busy / 6
night_hours = 7 - 5 / 60

def estimate(overhead, radio_seconds, transfer_w, radio_w, sleep_w, deep_w):
    day = (transfer_seconds * transfer_w + (600 - transfer_seconds) * sleep_w
           + panel_seconds * .060 + radio_seconds * radio_w) / 600 + overhead
    night = deep_w + overhead
    average = (day * (24 - night_hours) + night * night_hours) / 24
    return {'day_W': round(day, 4), 'night_W': round(night, 4),
            'average_W': round(average, 4), 'lifetime_hours': round(usable_wh / average, 1),
            'lifetime_days': round(usable_wh / average / 24, 2)}

report = {
    'type': 'ESTIMATE—not electrical measurement',
    'firmware': 'v21 hardware SPI; electrical assumptions unchanged from v20',
    'battery_Wh': 11.1, 'usable_Wh': usable_wh,
    'baseline': {'assumed_load_W': [.4, .5, .7],
                 'lifetime_hours': [round(usable_wh / p, 1) for p in [.4, .5, .7]]},
    'measured_durations': {'clock_cycle_seconds': clock_cycle,
        'clock_BUSY_seconds': clock_busy, 'full_cycle_seconds': full_cycle,
        'full_BUSY_seconds': full_busy, 'radio_session_samples_seconds': radio_samples,
        'radio_session_mean_seconds': central_radio,
        'single_light_probe_ms': 991, 'deep_timer_test_seconds': 90},
    'improved_battery_profile': {
        'optimistic': estimate(.010, min(radio_samples), .200, .250, .003, .0001),
        'central': estimate(.025, central_radio, .250, .350, .006, .0002),
        'conservative': estimate(.050, 30, .300, .500, .010, .0003)},
    'board_overhead_sensitivity_central_powers': {
        str(w) + '_W': estimate(w, central_radio, .250, .350, .006, .0002)
        for w in [.010, .025, .050, .100, .150]},
    'assumptions': {'day_hours': 24 - night_hours, 'night_hours': night_hours,
        'CPU_work_seconds_per_600': transfer_seconds,
        'panel_waveform_seconds_per_600': panel_seconds,
        'radio_seconds_per_600': '10.275 / observed mean / 30',
        'transfer_W': '0.200 / 0.250 / 0.300',
        'additional_radio_W': '0.250 / 0.350 / 0.500',
        'light_sleep_W': '0.003 / 0.006 / 0.010',
        'deep_sleep_W': '0.0001 / 0.0002 / 0.0003',
        'board_overhead_W': '0.010 / 0.025 / 0.050; additional sensitivity to 0.150',
        'radio_load': 'Added conservatively, including when CPU work overlaps; not exact RF airtime',
        'cold_boot_and_midnight_redraw': 'Small additional daily overhead excluded',
        'unmodelled': 'battery ageing/cutoff, conversion topology, LED/regulator actual current, Wi-Fi failures'},
}
path = Path(__file__).resolve().parents[1] / 'docs/power/battery-estimate.json'
path.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
