/*
 * debug_output.h
 * UART debug output for sensor monitoring
 */

#ifndef INC_DEBUG_OUTPUT_H_
#define INC_DEBUG_OUTPUT_H_

#include <stdint.h>

/* Max value tracking for each sensor */
typedef struct {
    uint16_t max_values[5];      // Max ADC value for each sensor in current 100ms window
    uint32_t last_output_ms;     // Last time we output to UART
} DebugState_t;

/* Global debug state (defined in piezosensor.c) */
extern DebugState_t g_debug_state;

/* Function prototypes */
void debug_init(void);
void debug_update_max(uint8_t sensor_id, uint16_t value);
void debug_output_if_needed(uint32_t current_ms);

#endif /* INC_DEBUG_OUTPUT_H_ */
