/*
 * piezosensor.h
 * Piezosensor data structures for FSM implementation
 */

#ifndef INC_PIEZOSENSOR_H_
#define INC_PIEZOSENSOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "piezo_config.h"
#include "debug_output.h"


/* Trigger FSM States */
typedef enum {
    TRIGGER_STATE_WAIT_FOR_TRIGGER = 0,
    TRIGGER_STATE_TOURNAMENT,
    TRIGGER_STATE_FINAL
} TriggerFSMState_t;

/* Cooling Down FSM States (per sensor) */
typedef enum {
    COOLDOWN_STATE_ACTIVE = 0,
    COOLDOWN_STATE_COOLING
} CooldownFSMState_t;

/* Sensor Data Structure */
typedef struct {
    /* Cooling Down FSM State */
    CooldownFSMState_t cooldown_state;

    /* Ring Buffer for Envelope History */
    uint16_t envelope_ring_buffer[ENVELOPE_RING_BUFFER_SIZE];  // History of envelope values
    uint16_t ring_buffer_head;         // Index for newest value
    uint16_t ring_buffer_tail;         // Index for oldest value
    uint16_t ring_buffer_count;        // Number of values currently in buffer

    /* Moving Average (calculated from ring buffer) - Fixed point * 10 */
    uint32_t moving_average_10x;           // Current moving average value * 10

    /* Tournament statistics - Fixed point * 10 for RMS */
    uint16_t trigger_value; // the ADC value when triggered
    uint32_t rms_accumulator;          // Sum of envelope values during tournament
    uint16_t rms_sample_count;         // Number of samples in tournament
    uint32_t rms_value;                // Final RMS value * 10

    /* Key mapping */
    uint8_t hid_keycode;               // USB HID key code for this sensor
    uint16_t drumcontroller_keycode;
    /* LED indicator */
    uint16_t led_pin;                  // LED pin for this sensor
    GPIO_TypeDef* led_port;            // LED GPIO port
    uint32_t led_on_time_ms;           // When LED was turned on (for 20ms pulse)

    /* Last trigger time (for threshold recovery) */
    uint32_t last_trigger_ms;          // Timestamp of last trigger (ms)

    /* Time when envelope first dropped below MA (for forced cooling period) */
    uint32_t ma_hit_time_ms;           // Timestamp when envelope < MA



    /* A bonus for the sensor who fires the trigger for tournament*/
    uint32_t tournament_bonus_multiplier;

    /* Key press state tracking */
    bool key_pressed;                  // True if HID key is currently pressed
    uint32_t key_press_scheduled_time_ms;  // When key should be pressed (0 = not scheduled)
    uint32_t key_release_scheduled_time_ms; // When key should be released

    /* Last DMA sample for differentiation (only used if enabled) */
    uint16_t last_dma_sample;          // Previous RAW ADC value from DMA

    /* Threshold multiplier - raised on trigger, decayed over time */
    float threshold_multiplier;        // Multiplier (e.g., 1.5, 1.0)
    float adjusted_threshold;
} PiezoSensor_t;

/* Trigger FSM Structure (shared by all sensors) */
typedef struct {
    TriggerFSMState_t state;
    uint32_t tournament_start_ms;
    uint8_t winner_sensor_id;       // ID of winning sensor (0-3)
    bool bundle_trigger;            // Whether bundle sensors triggered together
    uint16_t current_threshold;     // Current threshold
    uint16_t tournament_duration_ms;    // Tournament duration in milliseconds
    uint16_t cooling_down_ms;           // Cooling down period in milliseconds
    uint16_t key_press_duration_ms;     // Key press duration in milliseconds
    float threshold_multiplier_boost;   // Threshold multiplier boost factor
    float threshold_decay_factor_float; // Threshold decay factor per ms
} TriggerFSM_t;

/* Tournament Data Structure (stores data during tournament) */
typedef struct {
    uint8_t contender_sensors[4];  // Which sensors are in contention
    uint8_t num_contenders;        // Number of active contenders
    uint8_t winner_idx;            // Index of winner in contenders array
    uint32_t best_rms;             // Best RMS value found (*10 fixed-point)
} TournamentData_t;

/* Global Variables (extern declarations) */
extern PiezoSensor_t g_sensors[SENSORS_ACTIVE];
extern TriggerFSM_t g_trigger_fsm;
extern uint16_t g_adc_dma_buffer[DMA_BUFFER_SAMPLES];
extern volatile uint32_t g_system_ms;
/* Function Prototypes */

/* Initialization */
void piezosensor_init(void);

/* Envelope Extraction (called from ADC callback) */
void extract_envelope_from_samples(volatile uint16_t *samples, uint16_t sample_offset);

/* FSM Processing (called from main loop or at 1ms intervals) */
void trigger_fsm_process(void);
void cooldown_fsm_process(void);

/* Tournament calculation */
void tournament_calculate_rms(uint16_t *samples, uint16_t sample_offset);
void tournament_find_winner(void);
void tournament_trigger_hid(void);

/* HID transmission */
void hid_clear_report(void);
void hid_process_afterglow(void);
uint8_t hid_send_buffered_reports(void);
void schedule_key_press(uint8_t sensor_id);
/* Utility functions */
uint32_t get_current_ms(void);

/* LED control */
void led_set_sensor_active(uint8_t sensor_id);
void led_set_sensor_triggered(uint8_t sensor_id);
void led_set_sensor_idle(uint8_t sensor_id);
void led_update_all(void);
void led_check_turn_off(uint32_t current_ms);

/* Debug output (if enabled) */
void debug_print_trigger(uint8_t sensor_id, uint32_t rms_value);

#endif /* INC_PIEZOSENSOR_H_ */
