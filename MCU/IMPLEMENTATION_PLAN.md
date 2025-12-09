# Piezosensor Signal Processing Implementation Plan

**Project**: USB HID Piezosensor Controller for Taiko Sensors
**Platform**: STM32F103C8T6 (Blue Pill)
**Date**: 2025-11-21
**Status**: Planning Phase

---

## 1. Project Overview

### Current Hardware Configuration
- **MCU**: STM32F103C8T6 (Blue Pill variant)
  - Clock: 72MHz external HSE with PLL
  - Package: LQFP-48
  - Memory: 64KB Flash, 20KB RAM

### Existing Peripheral Configuration
- **ADC1**: 5-channel simultaneous sampling
  - Channels: PA0 (IN0), PA1 (IN1), PA2 (IN2), PA3 (IN3), PA4 (IN4)
  - Sampling Rate: ~10kHz per channel (TIM3 triggered)
  - Resolution: 12-bit
  - DMA: Circular mode, buffer size = 240 samples (5 channels × 48 samples)

- **Timers**:
  - TIM1: Configured for 48kHz but NOT STARTED (Prescaler=47, Period=499)
  - TIM3: Running at 10kHz, triggers ADC conversions (Prescaler=99, Period=10)

- **USB**: HID class device configured but no data transmission
  - Full Speed (12Mbps)
  - No report descriptor implemented (mouse.h exists but incomplete)

- **GPIO**:
  - Inputs: K1 (PA15), K2 (PB3), K3 (PB4) - pull-up buttons
  - Outputs: XLED1-4 (PB12-15), XGOOD (PB5) - status LEDs
  - USB: PA11 (DM), PA12 (DP)

### Current Code State
- ADC DMA running and filling buffer `arrADC1_DMA[240]`
- ADC callbacks (`HAL_ADC_ConvCpltCallback`, `HAL_ADC_ConvHalfCpltCallback`) are **empty placeholders**
- Main loop is empty
- No signal processing implemented
- TIM1 unused despite being configured
- USB HID framework ready but no data transmission

---

## 2. User Requirements

### Sensor Configuration
- **Total Sensors**: 5 piezosensors connected to ADC channels 0-4
- **Active Sensors**: 4 sensors will trigger HID output
- **Redundant Sensor**: 1 sensor (channel 4) processed but NOT sent to HID
- **Future Use**: Redundant channel available for testing/expansion

### Signal Processing Requirements
- **Approach**: Finite State Machine (FSM) implementation
  - User will provide detailed FSM requirements later
  - Must be flexible to accommodate custom state transitions

- **Trigger Detection**: Not yet specified (awaiting FSM details)
  - Options discussed: threshold crossing, peak detection, envelope detection

- **Anti-Jitter**: Adaptive threshold approach
  - Dynamic threshold adjustment based on signal statistics
  - Not simple cooldown timer (user specifically rejected this)

- **Key Mapping**: Custom mapping to be provided
  - Each of the 4 active sensors → specific keyboard key
  - Configuration will be defined in header file

---

## 3. Signal Processing Architecture Design

### 3.1 Data Flow Diagram

```
Piezosensors (PA0-PA4)
    ↓
ADC1 (5-channel simultaneous sampling @ 10kHz)
    ↓
DMA (circular buffer arrADC1_DMA[240])
    ↓
ADC Callback (HAL_ADC_ConvCpltCallback at 5kHz effective rate)
    ↓
Finite State Machine (per-sensor processing)
    ├─ Adaptive Threshold Calculation
    ├─ State Transition Logic
    ├─ Peak Detection
    └─ Trigger Validation
    ↓
HID Report Queue
    ↓
USB HID Keyboard (4 keys active)
    ↓
Host Computer
```

### 3.2 Finite State Machine States (Proposed - Awaiting User Details)

**Recommended FSM States for Piezosensor Processing:**

```c
typedef enum {
    SENSOR_IDLE,           // Waiting for signal, monitoring noise floor
    SENSOR_RISING_EDGE,    // Signal rising above threshold
    SENSOR_PEAK_DETECT,    // Searching for maximum amplitude
    SENSOR_FALLING_EDGE,   // Signal falling back below threshold
    SENSOR_TRIGGERED,      // Valid strike detected
    SENSOR_COOLDOWN        // Holding state to prevent retriggering
} SensorState_t;
```

**State Transitions:**
1. **IDLE → RISING_EDGE**: ADC value exceeds adaptive threshold
2. **RISING_EDGE → PEAK_DETECT**: Continue monitoring for peak
3. **PEAK_DETECT → FALLING_EDGE**: Peak found, signal decreasing
4. **FALLING_EDGE → TRIGGERED**: Signal below threshold, check if valid
5. **TRIGGERED → COOLDOWN**: Queue HID key press
6. **COOLDOWN → IDLE**: Hold for debounce period, release key, return to IDLE

### 3.3 Adaptive Threshold Algorithm

**Concept**: Dynamic threshold based on signal statistics

**Implementation Approach:**
```c
// Pseudo-code
for each sensor:
    // Track noise floor (slow moving average)
    noise_floor = (noise_floor * 0.95) + (current_value * 0.05)

    // Calculate signal variance
    if current_value > noise_floor:
        signal_energy += (current_value - noise_floor)
    else:
        signal_energy *= 0.9; // decay

    // Adaptive threshold
    threshold = noise_floor + K1 + (signal_energy * K2)

    where:
        K1 = base sensitivity (configurable)
        K2 = adaptive factor (0.1 to 0.3 typical)
```

**Benefits:**
- Automatically compensates for temperature drift
- Adapts to different sensor sensitivities
- Reduces false triggers from electrical noise
- No manual calibration needed

---

## 4. Implementation Phases

### Phase 1: Core Architecture Foundation

**Task 1.1: Create Piezosensor FSM Data Structures**
- File: `piezosensor.h`
- Define sensor state enum
- Create sensor state structure:
  ```c
  typedef struct {
      float noise_floor;          // Running average of quiet state
      float threshold;            // Adaptive threshold
      uint16_t peak_value;        // Maximum in current cycle
      uint32_t last_trigger;      // Timestamp of last trigger
      SensorState_t state;        // FSM state
      uint8_t key_code;           // HID key code for this sensor
      bool active;                // Whether sensor triggers HID
  } PiezoSensor_t;
  ```

**Task 1.2: Design Adaptive Threshold Algorithm**
- Function: `update_adaptive_threshold()`
- Track noise floor with EWMA (Exponentially Weighted Moving Average)
- Calculate dynamic threshold based on signal energy
- Configurable sensitivity parameters

### Phase 2: USB HID Keyboard Implementation

**Task 2.1: Create Keyboard HID Report Descriptor**
- File: Modify USB HID class descriptor
- Standard keyboard descriptor (boot protocol compatible)
- Report size: 8 bytes
  - Byte 0: Modifier keys (shift, ctrl, alt, etc.)
  - Byte 1: Reserved
  - Bytes 2-7: Up to 6 simultaneous key presses (6KRO)

**Task 2.2: Implement Keyboard Data Structures**
- File: `Core/Inc/keyboard.h` (replace empty file)
- Define keyboard report structure
- Define USB HID key scan codes
- Implement report transmission function

### Phase 3: Signal Processing Implementation

**Task 3.1: Implement ADC Callback Processing**
- File: `Core/Src/main.c`
- Fill `HAL_ADC_ConvCpltCallback()` function
- Process each of 5 sensor channels through FSM
- Handle 5th sensor as "active but no HID output"

**Task 3.2: Finite State Machine Core**
- File: `Core/Src/piezosensor.c`
- Implement state transition logic
- Peak detection algorithm
- Signal validation (amplitude, timing checks)
- Key press/release queuing

**Task 3.3: Signal Feature Extraction**
- Rise time measurement (time from threshold to peak)
- Peak amplitude extraction
- Signal energy calculation (area under curve)
- Spurious spike rejection

### Phase 4: Configuration and Integration

**Task 4.1: Create Configuration Header**
- File: `Core/Inc/piezo_config.h`
- Sensor-to-key mapping table (sensor 0-3 → HID key codes)
- Sensitivity constants (K1, K2 for adaptive threshold)
- Debounce timing settings
- Minimum/maximum valid signal parameters

**Task 4.2: LED Visualization System**
- Map XLED1-4 (PB12-PB14) to sensors 0-3
- LED states:
  - OFF: Sensor idle
  - ON: Sensor active (signal present)
  - BLINK: Sensor triggered (key press sent)
- XGOOD (PB5): System heartbeat / USB connected indicator

**Task 4.3: Main Loop Integration**
- Process USB HID transmission queue
- Update LED states
- Optional diagnostic tasks

### Phase 5: Debug and Testing

**Task 5.1: UART Debug Output**
- Use USART3 (PB10/PB11 @ 115200 baud)
- Print sensor values on trigger
- Display threshold values
- Show FSM state transitions

**Task 5.2: Test Mode Implementation**
- Use buttons K1-K3 for mode selection
- Modes:
  - Normal operation
  - Threshold adjustment mode
  - Raw data streaming mode
  - Sensor test mode

---

## 5. File Structure Changes

### New Files Created
```
MCU/Core/Inc/
├── piezosensor.h          // FSM structures and function declarations
├── piezo_config.h         // User configuration (mappings, thresholds)

MCU/Core/Src/
├── piezosensor.c          // FSM implementation and adaptive threshold
```

### Files Modified
```
MCU/Core/Inc/
├── keyboard.h             // Replace empty file with HID structures
├── main.h                 // Add includes for new headers

MCU/Core/Src/
├── main.c                 // Implement ADC callbacks and main loop

USB_DEVICE/
├── App/
│   └── usbd_hid.c         // Add keyboard HID report descriptor
```

---

## 6. Key Parameters and Configuration

### Sampling Rate Considerations

**Current Configuration: 10kHz per channel**
- TIM3: 72MHz / (99+1) / (10+1) = 65.45kHz timer
- ADC triggered by TIM3 TRGO
- Effective sampling: ~10kHz per channel
- DMA callbacks: 240 samples / 5 channels / 10kHz = 4.8kHz rate

**Optional: Increase to 48kHz per channel**
- Requires TIM3 configuration change:
  - Prescaler: 14 (instead of 99)
  - Period: 10 (same)
  - New rate: 72MHz / 15 / 11 = 436kHz timer → ~48kHz sampling
- Benefits: Higher resolution for transient capture
- Trade-offs: More CPU load, faster callback rate

**Recommendation**: Start with current 10kHz, optimize processing, then consider increasing if needed.

### Adaptive Threshold Parameters

Suggested starting values (tunable via config):
```c
#define NOISE_FLOOR_ALPHA    0.95f    // EWMA smoothing factor (higher = slower adaptation)
#define BASE_SENSITIVITY     50       // Minimum threshold above noise floor (ADC counts)
#define ADAPTIVE_FACTOR      0.20f    // Signal energy contribution (0.0 to 0.5)
#define PEAK_VALID_MIN       100      // Minimum peak amplitude (ADC counts)
#define COOLDOWN_MS          15       // Minimum time between triggers (ms)
```

### Timing Considerations

**ADC Callback Rate**: ~5kHz (4.8kHz exact)
- Processing budget: ~200μs per callback maximum
- 5 sensors × FSM processing should fit comfortably

**HID Report Rate**: USB Full Speed
- Maximum 1000 reports/second (1ms interval)
- Typical keyboard usage: updates on key change only

**Debouncing/Cooldown**: 10-20ms
- Prevents multiple triggers from sensor ringing
- Adjustable based on sensor mechanical characteristics

---

## 7. USB HID Keyboard Implementation Details

### Report Descriptor

Standard boot keyboard descriptor:
```c
static uint8_t HID_KEYBOARD_ReportDesc[63] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0xE0,  //   Usage Minimum (224)
    0x29, 0xE7,  //   Usage Maximum (231)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input (Data, Variable, Absolute) ; Modifier byte
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x08,  //   Report Size (8)
    0x81, 0x01,  //   Input (Constant) ; Reserved byte
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x65,  //   Logical Maximum(101)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0x00,  //   Usage Minimum (0)
    0x29, 0x65,  //   Usage Maximum (101)
    0x81, 0x00,  //   Input (Data, Array)
    0xC0         // End Collection
};
```

### Key Report Structure

```c
typedef struct {
    uint8_t modifier;  // Modifier keys (bit flags)
    uint8_t reserved;  // Padding
    uint8_t keycode[6]; // Up to 6 keys pressed simultaneously
} HID_KeyboardReport_t;

// Keycode examples (USB HID Usage Tables)
#define KEY_A       0x04
#define KEY_S       0x16
#define KEY_D       0x07
#define KEY_F       0x09
#define KEY_Z       0x1D
#define KEY_X       0x1B
#define KEY_C       0x06
#define KEY_V       0x19
```

---

## 8. Memory Usage Estimates

### RAM Requirements
```
arrADC1_DMA:            240 samples × 2 bytes = 480 bytes
PiezoSensor_t array:     5 sensors × ~40 bytes = 200 bytes
HID Report buffer:       8 bytes
Other globals:           ~100 bytes
-------------------------------------------
Total:                   ~800 bytes (4% of 20KB RAM)
```

### Flash Requirements
```
FSM code:                ~2KB
USB HID additions:       ~1KB
ADC callbacks:           ~1KB
Configuration/data:      ~0.5KB
-------------------------------------------
Total added:             ~4.5KB (7% of 64KB Flash)
```

---

## 9. Testing Strategy

### Unit Tests
1. **Threshold Algorithm Test**
   - Inject known signal patterns
   - Verify threshold adapts correctly
   - Test noise floor tracking

2. **FSM State Transitions**
   - Test each state transition
   - Verify debouncing behavior
   - Test edge cases (rapid triggers)

3. **HID Report Generation**
   - Verify correct key codes sent
   - Test multiple simultaneous keys
   - Check modifier key handling

### Integration Tests
1. **Sensor Response**
   - Strike each sensor individually
   - Verify corresponding key press
   - Check for crosstalk/coupling

2. **Adaptive Threshold**
   - Test with varying strike forces
   - Verify consistent triggering
   - Check adaptation to sensor drift

3. **Rapid Strike Test**
   - Drumming patterns (1-10 strikes/second)
   - Verify all triggers registered
   - Check for missing events

4. **Long Duration Test**
   - Run for 24+ hours
   - Monitor for memory leaks
   - Check threshold drift
   - Verify USB stability

### Debug Tools
- UART terminal output (sensor values, states, thresholds)
- LED visualization of sensor activity
- Optional: USB serial for runtime configuration

---

## 10. Open Questions for User

### Awaiting Detailed Requirements:
1. **FSM State Diagram**: User will provide detailed FSM requirements
   - Exact state definitions
   - Transition conditions
   - Any additional states needed

2. **Key Mapping**: User will specify which keys map to which sensors
   - Exact HID key codes (e.g., A, S, D, F or arrow keys, etc.)
   - Any modifier keys needed

3. **Sensor Characteristics**:
   - Expected voltage range from piezosensors
   - Typical strike duration
   - Ringing/decay characteristics
   - Crosstalk behavior between sensors

4. **Application Requirements**:
   - What is the target application? (game, music, etc.)
   - Expected strike rate (triggers per second per sensor)
   - Latency requirements
   - Multi-strike tolerance

### Architecture Decisions to Confirm:
1. **Processing Location**: ADC callbacks (5kHz rate) - confirm this is acceptable
2. **Sampling Rate**: Keep 10kHz or increase to 48kHz?
3. **5th Sensor**: Confirm it should be processed but not trigger HID
4. **USB Protocol**: Pure keyboard, or combination keyboard/mouse?
5. **LED Indicators**: Confirm mapping and behavior
6. **Button Functions**: What should K1-K3 do? (mode select, test trigger, etc.)

---

## 11. Risk Mitigation

### Risks Identified
1. **CPU Overload**: ADC callback rate may be too high
   - Monitor execution time with scope/logic analyzer
   - Reduce sampling rate if needed
   - Optimize FSM code

2. **USB Halt**: Long processing may block USB interrupts
   - Keep processing short (<100μs)
   - Consider processing in main loop instead
   - Use double-buffering

3. **False Triggers**: Electrical noise or crosstalk
   - Implement signal validation in FSM
   - Adjust adaptive threshold parameters
   - Use shielded cables if needed

4. **Missed Triggers**: FSM too strict
   - Log rejected triggers for analysis
   - Tune threshold and validation parameters
   - Check for sensor degradation

5. **Threshold Drift**: Long-term changes in sensor sensitivity
   - Adaptive threshold should handle this
   - May need periodic re-baselining
   - Consider temperature compensation

---

## 12. Timeline Estimate

### Phase 1: Foundation (2-3 hours)
- FSM data structures
- Stub functions
- Configuration framework

### Phase 2: USB HID (1-2 hours)
- Keyboard report descriptor
- HID transmission functions

### Phase 3: Signal Processing (3-4 hours)
- FSM implementation
- Adaptive threshold algorithm
- ADC callback integration

### Phase 4: Configuration & Integration (2 hours)
- Configuration header
- LED visualization
- Main loop integration

### Phase 5: Testing & Debug (2-3 hours)
- UART debug output
- Testing and tuning
- Documentation

**Total Estimated Time**: 10-14 hours

---

## 13. Next Steps

### Immediate Actions (Once User Approves Plan):
1. Start Phase 1: Create core data structures (piezosensor.h)
2. Implement USB HID keyboard report descriptor
3. Create piezo_config.h with placeholder mappings
4. Implement stub FSM in ADC callbacks

### Awaiting From User:
1. Detailed FSM state diagram or requirements
2. Specific key mappings (which keyboard keys for each sensor)
3. Any sensor characteristics or timing requirements

### After Initial Implementation:
1. User testing with actual piezosensors
2. Parameter tuning based on real sensor data
3. Performance optimization if needed
4. Documentation completion

---

## 14. References

### Datasheets
- STM32F103C8T6 Reference Manual (RM0008)
- STM32F103xx Datasheet
- USB HID Specification v1.11
- USB Device Class Definition for HID v1.11

### STM32 HAL Documentation
- STM32CubeF1 HAL User Manual
- UM1850: Description of STM32F1xx HAL drivers

### USB HID Keycodes
- USB HID Usage Tables v1.12 (Section 10: Keyboard/Keypad Page)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-21
**Prepared by**: Claude Code
**Status**: Planning Phase - Awaiting User Approval
