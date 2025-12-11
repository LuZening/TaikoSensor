/*
 * sensor_config.h
 * Sensor configuration management (multipliers, thresholds)
 * Stores settings in flash EEPROM emulation
 */

#ifndef INC_SENSOR_CONFIG_H_
#define INC_SENSOR_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/* Flash storage configuration - use HAL definition for page size */
#define FLASH_CONFIG_ADDRESS    (0x0801FC00)  // Last 1KB of 64KB flash (for 64KB device)
#define FLASH_CONFIG_MAGIC      0x434F4E46  // "CONF" magic word

extern const uint16_t SENSOR_MULTIPLIERS_DEFAULT[5];

/* Configuration structure - must be packed and aligned */
typedef struct __attribute__((packed)) {
    uint32_t magic;                     // Magic word for validity check
    uint16_t version;                   // Configuration version
    uint16_t sensor_multiplier[5];      // ADC pre-multipliers for 5 sensors (0-40960, fixed point *10)
    uint16_t sensor_threshold[5];       // Trigger thresholds for 5 sensors
    uint16_t trigger_threshold_base;    // Base trigger threshold (ADC counts)
    uint16_t tournament_duration_ms;    // Tournament duration in milliseconds
    uint16_t cooling_down_ms;           // Cooling down period in milliseconds
    uint16_t key_press_duration_ms;     // Key press duration in milliseconds
    float threshold_multiplier_boost;   // Threshold multiplier boost factor
    float threshold_decay_factor_float; // Threshold decay factor per ms
    uint16_t checksum;                  // Simple checksum for integrity
} SensorConfig_t;

/* Global configuration instance */
extern SensorConfig_t g_sensor_config;

/* Function prototypes */
void sensor_config_init(void);
void sensor_config_set_defaults(void);
bool sensor_config_load_from_flash(void);
bool sensor_config_save_to_flash(void);
uint16_t sensor_config_calculate_checksum(const SensorConfig_t *config);

/* Command processing */
void sensor_config_process_command(const char *cmd_buffer, uint16_t length);
void sensor_config_send_ok(void);
void sensor_config_send_string(const char *str);
void sensor_config_send_float(const char *prefix, float value);
void sensor_config_send_uint16(const char *prefix, uint16_t value);

/* Status functions */
bool sensor_config_load_failed(void);

#endif /* INC_SENSOR_CONFIG_H_ */
