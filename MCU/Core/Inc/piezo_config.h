/*
 * piezo_config.h
 * User configuration for piezosensor trigger system
 */

#ifndef INC_PIEZO_CONFIG_H_
#define INC_PIEZO_CONFIG_H_

#include <stdint.h>
#include "main.h"
#include "keyboard.h"
/*====================== USER CONFIGURATION ======================*/

/* Sampling Configuration */
#define ADC_SAMPLING_RATE_HZ    48000   // ADC sampling rate in Hz
#define SAMPLES_PER_MS          (ADC_SAMPLING_RATE_HZ / 1000)      // Samples per channel per millisecond
#define SENSORS_ACTIVE          4       // Number of sensors that trigger HID
#define SENSOR_TOTAL_CHANNELS   5       // Total ADC channels (including redundant)

/* DMA Buffer Configuration */
#define DMA_SAMPLES_PER_HALF    (SAMPLES_PER_MS * SENSOR_TOTAL_CHANNELS)  // 48 * 5 = 240
#define DMA_BUFFER_SAMPLES         (DMA_SAMPLES_PER_HALF * 2)         // Double buffer

/* Trigger Configuration */
#define TRIGGER_THRESHOLD_BASE  150     // Base trigger threshold (ADC counts), 200mV = 248 @ 3.3Vref
#define TOURNAMENT_DURATION_MS  4       // Tournament duration in milliseconds
#define TOURNAMENT_BONUS_MULTIPLIER_DEFAULT 20
#define COOLING_DOWN_MS 15

/* Bundle Configuration (sensors that can trigger together) */
#define BUNDLE1_SENSOR_A        0       // First sensor in bundle 1
#define BUNDLE1_SENSOR_B        3       // Second sensor in bundle 1 (sensor 4)
#define BUNDLE2_SENSOR_A        1       // First sensor in bundle 2 (sensor 2)
#define BUNDLE2_SENSOR_B        2       // Second sensor in bundle 2 (sensor 3)

/* Cooling Down Configuration */
#define MOVING_AVG_WINDOW_MS    20     // Moving average window length (ms)

/* HID Key Press Duration (afterglow) */
#define KEY_PRESS_DURATION_MS   20       // Key press duration in milliseconds

/* Envelope Processing Options */
#define ENABLE_1ST_ORDER_DIFFERENTIATION  0  // 1 = enable, 0 = disable

/* Threshold Recovery Configuration */
#define THRESHOLD_MULTIPLIER_BOOST	1.5f
#define THRESHOLD_DECAY_FACTOR_FLOAT   0.97f     // Decay factor per ms (0.995 = multiply by 0.995 each ms)

/* Ring Buffer Configuration for Envelope History */
#define ENVELOPE_RING_BUFFER_SIZE_POW_OF_2 8U
#define ENVELOPE_RING_BUFFER_SIZE  (1U << (ENVELOPE_RING_BUFFER_SIZE_POW_OF_2))    // Number of envelope samples to keep (200ms history)
#define ENVELOPE_RING_BUFFER_MOD_MASK ((ENVELOPE_RING_BUFFER_SIZE) - 1U)
/* Key Mapping - USB HID Keyboard Codes (Usage Tables v1.12)
 * Common keycodes:
 * 0x04 = A, 0x05 = B, 0x06 = C, 0x07 = D, 0x08 = E, 0x09 = F
 * 0x1D = Z, 0x1E = 1, 0x1F = 2, 0x20 = 3
 * Arrow keys: 0x50 = Right, 0x51 = Left, 0x52 = Down, 0x4F = Right (Note: check usage tables)
 */
#define SENSOR_0_KEYCODE        KEY_K     // Right Rim
#define SENSOR_1_KEYCODE        KEY_J    // Right Center
#define SENSOR_2_KEYCODE        KEY_F    // Left Center
#define SENSOR_3_KEYCODE        KEY_D    // Left Rim
// Sensor 4 is redundant and does not trigger HID

/* LED Mapping */
#define LED_SENSOR_0            XLED1_Pin
#define LED_SENSOR_1            XLED2_Pin
#define LED_SENSOR_2            XLED3_Pin
#define LED_SENSOR_3            XLED4_Pin
#define LED_SENSOR_0_PORT       XLED1_GPIO_Port
#define LED_SENSOR_1_PORT       XLED2_GPIO_Port
#define LED_SENSOR_2_PORT       XLED3_GPIO_Port
#define LED_SENSOR_3_PORT       XLED4_GPIO_Port

/* Debug Configuration */
#define UART_DEBUG_ENABLED      0       // 1 = enable UART debug output
#define UART_DEBUG_TRIGGER      0       // Print on trigger events
#define UART_DEBUG_THRESHOLD    0       // Print threshold values

/*==================== END USER CONFIGURATION ====================*/

#endif /* INC_PIEZO_CONFIG_H_ */
