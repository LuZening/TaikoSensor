# Integer Overflow Risk Analysis

## ADC Input Constraint
- **Max ADC value**: 4095 (12-bit ADC)

## 1. Ring Buffer Moving Average (ring_buffer_get_average)
```c
sum += sensor->envelope_ring_buffer[idx];  // Line 89
return (sum * 10) / sensor->ring_buffer_count;
```
- **Max samples**: 100 (100ms window at 1ms/sample)
- **Max sum**: 4095 * 100 = 409,500
- **Max sum * 10**: 4,095,000
- **Variable type**: uint32_t (max 4,294,967,295)
- **✓ SAFE**: 4,095,000 << 4,294,967,295

## 2. Envelope Extraction Top-N Sum
```c
sum += top_values[i];  // Line 214
```
- **ENVELOPE_TOP_N**: 3
- **Max sum**: 4095 * 3 = 12,285
- **Variable type**: uint32_t
- **✓ SAFE**: Well within uint32_t range

## 3. Sensor Multiplier Scaling
```c
uint32_t scaled_envelope = (uint32_t)envelope *
                           g_sensor_config.sensor_multiplier[sensor];  // Line 220
```
- **envelope max**: 4095
- **multiplier max**: 40960 (4096.0 * 10, from parse_fixed_point limit)
- **Product max**: 4095 * 40960 = 167,731,200
- **Variable type**: uint32_t
- **✓ SAFE**: 167,731,200 < 4,294,967,295

## 4. Threshold Multiplier Adjustment
```c
uint32_t adjusted_threshold = ((uint32_t)g_sensor_config.sensor_threshold[i] *
                               g_sensors[i].threshold_multiplier) / 10;  // Lines 331-332
```
- **threshold max**: 65535 (uint16_t)
- **multiplier max**: 15 (initial value, decays to 10)
- **Product max**: 65535 * 15 = 983,025
- **Division**: / 10
- **Variable type**: uint32_t
- **✓ SAFE**: Well within range

## 5. Tournament RMS Accumulator
```c
g_sensors[sensor_id].rms_accumulator += envelope;  // Line 366
```
- **Accumulator type**: uint32_t rms_accumulator
- **Max tournament duration**: 20ms = 20 samples
- **Max envelope**: 4095
- **Max accumulator**: 4095 * 20 = 81,900
- **✓ SAFE**: Far below uint32_t limit

```c
g_sensors[sensor_id].rms_value = (g_sensors[sensor_id].rms_accumulator * 10) /
                                 g_sensors[sensor_id].rms_sample_count;  // Line 374
```
- **Accumulator * 10 max**: 81,900 * 10 = 819,000
- **✓ SAFE**

## 6. Bundle Trigger Threshold
```c
uint32_t bundle_threshold = (g_tournament.best_rms * 8) / 10;  // Line 408
```
- **best_rms max**: rms_value max = (4095 * 10) = 40,950 (worst case: 4095 avg * 10)
- **Product max**: 40,950 * 8 = 327,600
- **Division**: / 10
- **Variable type**: uint32_t
- **✓ SAFE**

## 7. Tournament Best RMS Tracking
```c
if (g_sensors[sensor_id].rms_value > g_tournament.best_rms) {  // Line 378
    g_tournament.best_rms = g_sensors[sensor_id].rms_value;
```
- **rms_value max**: 40,950 (as calculated above)
- **best_rms type**: uint32_t
- **✓ SAFE**

## 8. Threshold Decay Update
```c
uint32_t decrease = (time_since_trigger * 5) / THRESHOLD_RECOVERY_T_MS;  // Line 599
g_sensors[i].threshold_multiplier = 15 - decrease;  // Line 603
```
- **time_since_trigger max**: 20ms (THRESHOLD_RECOVERY_T_MS)
- **time_since_trigger * 5 max**: 20 * 5 = 100
- **Division**: / 20 = 5 max
- **Result**: decrease max = 5, multiplier = 15 - 5 = 10
- **✓ SAFE**

## Summary

| Operation | Max Value | Variable Type | Status |
|-----------|-----------|---------------|--------|
| Moving average sum | 4,095,000 | uint32_t | ✓ SAFE |
| Envelope top-N sum | 12,285 | uint32_t | ✓ SAFE |
| Multiplier product | 167,731,200 | uint32_t | ✓ SAFE |
| Threshold adjustment | 983,025 | uint32_t | ✓ SAFE |
| RMS accumulator | 81,900 | uint32_t | ✓ SAFE |
| Bundle threshold | 40,950 | uint32_t | ✓ SAFE |

**CONCLUSION: All integer operations are SAFE from overflow.**

## Recommendations

1. **rms_accumulator type**: The accumulator is currently uint32_t which is more than sufficient. It could be uint16_t (max 65,535) but it's fine to keep uint32_t for clarity and margin.

2. **Comments update**: The comment on line 42 says "Sum of squares" but should say "Sum of envelope values" since we removed the square operation during float-to-int conversion.

3. **assertions (optional)**: If you want extra safety during development, you could add:
```c
assert(g_sensors[sensor_id].rms_accumulator < UINT32_MAX);
```
But this is not necessary as the analysis proves safety.
