/*
 * piezosensor.c
 * Piezosensor FSM implementation with ring buffer envelope history
 */

#include "piezosensor.h"
#include "piezo_config.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include "usbd_hid.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "drumcontroller.h"
#include "usbd_composite_desc.h"
#include "uart_terminal.h"
#include "usbd_composite_wrapper.h"
#include "usbd_drumcontroller_wrapper.h"

/* Forward declarations of static functions */
static void ring_buffer_add(PiezoSensor_t *sensor, uint16_t value);
static uint32_t ring_buffer_get_average(PiezoSensor_t *sensor);
static uint16_t ring_buffer_get_latest(PiezoSensor_t *sensor);
static void wait_for_trigger_state(void);
static void tournament_state(void);
static void final_state(void);

/* Global Variables */
PiezoSensor_t g_sensors[SENSORS_ACTIVE] = {0};
TriggerFSM_t g_trigger_fsm = {0};
TournamentData_t g_tournament = {0};

/* Extern variables from main.c */
extern volatile uint16_t arrADC1_DMA[DMA_BUFFER_SAMPLES];
extern USBD_HandleTypeDef hUsbDeviceFS;
extern UART_HandleTypeDef huart3;  // For debug output

/* Global system time */
volatile uint32_t g_system_ms = 0;

/* HID report buffer */
volatile uint8_t g_hid_report[8] = {0};
volatile USB_JoystickReport_Input_t joystick_report = {.Button = 0, .Hat = JOYSTICK_HAT_NEUTRAL, .LX = JOYSTICK_ANALOG_NEUTRAL, .LY = JOYSTICK_ANALOG_NEUTRAL, .RX = JOYSTICK_ANALOG_NEUTRAL, .RY = JOYSTICK_ANALOG_NEUTRAL, .VendorSpec = 0};

/* Debug state for max value tracking */
volatile DebugState_t g_debug_state = {0};

/*================= RING BUFFER FUNCTIONS =================*/

/**
 * @brief Add a new envelope value to the ring buffer
 * @param sensor Pointer to the sensor structure
 * @param value New envelope value to add
 *
 * @note This function is called from ISR context (ADC DMA callback).
 *      Runs every 1ms. No critical section - accepts dirty reads from main loop.
 */
static void ring_buffer_add(PiezoSensor_t *sensor, uint16_t value)
{
    /* Add value at head position */
    sensor->envelope_ring_buffer[sensor->ring_buffer_head] = value;

    /* Increment head */
    sensor->ring_buffer_head++;
    if (sensor->ring_buffer_head >= ENVELOPE_RING_BUFFER_SIZE) {
        sensor->ring_buffer_head = 0;
    }

    /* If buffer is full, increment tail to maintain window size */
    if (sensor->ring_buffer_count >= ENVELOPE_RING_BUFFER_SIZE) {
        sensor->ring_buffer_tail++;
        if (sensor->ring_buffer_tail >= ENVELOPE_RING_BUFFER_SIZE) {
            sensor->ring_buffer_tail = 0;
        }
        /* Count stays the same when buffer is full (fixed-size window) */
    } else {
        sensor->ring_buffer_count++;
    }
}

/**
 * @brief Calculate moving average from ring buffer
 * @param sensor Pointer to the sensor structure
 * @return Average value * 10 (fixed-point with 1 decimal place)
 * @note Accepts dirty reads - main loop may read slightly stale data from ISR updates
 */
static uint32_t ring_buffer_get_average(PiezoSensor_t *sensor)
{
    if (sensor->ring_buffer_count == 0) {
        return 0;
    }

    uint32_t sum = 0;
    for (uint16_t i = 0; i < sensor->ring_buffer_count; i++) {
        uint16_t idx = (sensor->ring_buffer_tail + i) & (ENVELOPE_RING_BUFFER_MOD_MASK);
        sum += sensor->envelope_ring_buffer[idx];
    }

    /* Return as fixed-point * 10 (multiply by 10 early to preserve precision) */
    return (sum * 10) / sensor->ring_buffer_count;
}

/**
 * @brief Get the most recent envelope value from ring buffer
 * @param sensor Pointer to the sensor structure
 * @return Latest envelope value or 0 if buffer empty
 * @note Accepts dirty reads - may return slightly stale data if ISR updates simultaneously
 */
static uint16_t ring_buffer_get_latest(PiezoSensor_t *sensor)
{
    if (sensor->ring_buffer_count == 0) {
        return 0;
    }

    /* Latest is one position before head (accounting for wrap) */
    uint16_t latest_idx = (sensor->ring_buffer_head == 0) ?
                          (ENVELOPE_RING_BUFFER_SIZE - 1) :
                          (sensor->ring_buffer_head - 1);

    return sensor->envelope_ring_buffer[latest_idx];
}

/*================= INITIALIZATION =================*/

#include "sensor_config.h"

/* Extern configuration */
extern SensorConfig_t g_sensor_config;

void piezosensor_init(void)
{
    /* Initialize all sensors */
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        g_sensors[i].cooldown_state = COOLDOWN_STATE_ACTIVE;
        g_sensors[i].moving_average_10x = 0;  // 0 in fixed-point (*10)

        /* Initialize ring buffer */
        memset(g_sensors[i].envelope_ring_buffer, 0,
               sizeof(g_sensors[i].envelope_ring_buffer));
        g_sensors[i].ring_buffer_head = 0;
        g_sensors[i].ring_buffer_tail = 0;
        g_sensors[i].ring_buffer_count = 0;

        g_sensors[i].rms_accumulator = 0;       // Sum of envelope values (not squared now)
        g_sensors[i].rms_sample_count = 0;
        g_sensors[i].rms_value = 0;             // 0 in fixed-point (*10)
        g_sensors[i].threshold_multiplier = 1.0f; // 1.0 in float
        g_sensors[i].adjusted_threshold = g_sensor_config.trigger_threshold_base;
        g_sensors[i].last_trigger_ms = 0;
        g_sensors[i].ma_hit_time_ms = 0;

        /* Set keycodes */
        if (i == 0) g_sensors[i].hid_keycode = SENSOR_0_KEYCODE;
        else if (i == 1) g_sensors[i].hid_keycode = SENSOR_1_KEYCODE;
        else if (i == 2) g_sensors[i].hid_keycode = SENSOR_2_KEYCODE;
        else if (i == 3) g_sensors[i].hid_keycode = SENSOR_3_KEYCODE;
        if (i == 0) g_sensors[i].drumcontroller_keycode = SENSOR_0_DRUMCONTROLLER_KEYCODE;
        else if (i == 1) g_sensors[i].drumcontroller_keycode = SENSOR_1_DRUMCONTROLLER_KEYCODE;
        else if (i == 2) g_sensors[i].drumcontroller_keycode = SENSOR_2_DRUMCONTROLLER_KEYCODE;
        else if (i == 3) g_sensors[i].drumcontroller_keycode = SENSOR_3_DRUMCONTROLLER_KEYCODE;
        /* Set LED pins */
        if (i == 0) {
            g_sensors[i].led_pin = LED_SENSOR_0;
            g_sensors[i].led_port = LED_SENSOR_0_PORT;
        } else if (i == 1) {
            g_sensors[i].led_pin = LED_SENSOR_1;
            g_sensors[i].led_port = LED_SENSOR_1_PORT;
        } else if (i == 2) {
            g_sensors[i].led_pin = LED_SENSOR_2;
            g_sensors[i].led_port = LED_SENSOR_2_PORT;
        } else if (i == 3) {
            g_sensors[i].led_pin = LED_SENSOR_3;
            g_sensors[i].led_port = LED_SENSOR_3_PORT;
        }

        /* Turn off LEDs initially (active low: 1 = OFF) */
        HAL_GPIO_WritePin(g_sensors[i].led_port, g_sensors[i].led_pin, GPIO_PIN_SET);

        /* Initialize LED timestamp */
        g_sensors[i].led_on_time_ms = 0;

        /* Initialize key press state */
        g_sensors[i].key_pressed = false;
        g_sensors[i].key_press_scheduled_time_ms = 0;
        g_sensors[i].key_release_scheduled_time_ms = 0;

        /* Initialize last DMA sample for differentiation */
        g_sensors[i].last_dma_sample = 0;
    }

    /* Initialize trigger FSM */
    g_trigger_fsm.state = TRIGGER_STATE_WAIT_FOR_TRIGGER;
    g_trigger_fsm.current_threshold = g_sensor_config.sensor_threshold[0];  // Use sensor 0 as default
    g_trigger_fsm.winner_sensor_id = 0;
    g_trigger_fsm.bundle_trigger = false;

    /* Initialize tournament data */
    memset(&g_tournament, 0, sizeof(g_tournament));
}

/*================= ENVELOPE EXTRACTION =================*/

void extract_envelope_from_samples(volatile uint16_t *samples, uint16_t sample_offset)
{
    /* Process each sensor channel */
    for (int sensor = 0; sensor < SENSOR_TOTAL_CHANNELS; sensor++) {
        uint32_t top_values[2] = {0};

        /* Find top N values in the 48 samples for this sensor */
        for (int i = 0; i < SAMPLES_PER_MS; i++) {
            uint16_t idx = sample_offset + sensor + (i * SENSOR_TOTAL_CHANNELS);
            uint16_t sample = samples[idx];  // RAW ADC value from DMA

#if ENABLE_1ST_ORDER_DIFFERENTIATION
            /* Apply 1st order differentiation on RAW ADC: diff = |current - previous|
             * Use absolute value to capture both press (positive) and release (negative) events
             */
            uint16_t diff_sample;
            if (i == 0) {
                /* First sample in this batch: use stored last sample from previous DMA callback */
                diff_sample = (sample >= g_sensors[sensor].last_dma_sample) ?
                             (sample - g_sensors[sensor].last_dma_sample) :
                             (g_sensors[sensor].last_dma_sample - sample);
            } else {
                /* Subsequent samples: use previous raw DMA sample in current batch */
                uint16_t prev_idx = idx - SENSOR_TOTAL_CHANNELS;  // Previous sample in DMA buffer
                uint16_t prev_sample = samples[prev_idx];  // RAW ADC value
                diff_sample = (sample >= prev_sample) ?
                             (sample - prev_sample) : (prev_sample - sample);
            }

            /* Store differentiated value (absolute difference) for envelope extraction */
            sample = diff_sample;
#endif

            /* Store last raw DMA sample for next batch iteration */
            if (i == SAMPLES_PER_MS - 1) {
                g_sensors[sensor].last_dma_sample = samples[idx];
            }

            /* Insert sample (raw or differentiated) into top values array */
            for (int j = 0; j < 2; j++) {
                if (sample > top_values[j]) {
                    /* Shift down */
                    for (int k = 2 - 1; k > j; k--) {
                        top_values[k] = top_values[k - 1];
                    }
                    top_values[j] = sample;
                    break;
                }
            }
        }

        /* Calculate envelope as average of top N values */
        uint32_t sum = 0;
        for (int i = 0; i < 2; i++) {
            sum += top_values[i];
        }
        uint16_t envelope = sum >> 1;

        /* Apply sensor multiplier (fixed-point: multiplier is value * 10) */
        if (sensor < SENSOR_TOTAL_CHANNELS) {
            uint32_t scaled_envelope = (uint32_t)envelope * g_sensor_config.sensor_multiplier[sensor];

            /* Divide by 10 to get actual scaled value */
//            scaled_envelope = scaled_envelope / 10;
            scaled_envelope = scaled_envelope >> 6; // use /64 to replace /100, for efficiency
            /* Clamp to ADC max */
            if (scaled_envelope > 65535) {
                scaled_envelope = 65535;
            }
            envelope = (uint16_t)scaled_envelope;
        }

        /* Add to ring buffer for active sensors */
        if (sensor < SENSORS_ACTIVE) {
            ring_buffer_add(&g_sensors[sensor], envelope);
            /* Update moving average from ring buffer */
            g_sensors[sensor].moving_average_10x = ring_buffer_get_average(&g_sensors[sensor]);

            /* Track max value for debug output */
            debug_update_max(sensor, envelope);
        }
    }
}

/*================= COOLING DOWN FSM =================*/

void cooldown_fsm_process(void)
{
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        PiezoSensor_t *sensor = &g_sensors[i];

        /* State transitions */
        switch (sensor->cooldown_state) {
            case COOLDOWN_STATE_ACTIVE:
                /* Stay active - no transition needed */
                break;

            case COOLDOWN_STATE_COOLING:
            {
                /* Get latest envelope value */
                uint16_t latest_envelope = ring_buffer_get_latest(sensor);

                /* Check if envelope has dropped below moving average (multiply envelope by 10 for fixed-point comparison) */
                if ((uint32_t)latest_envelope * 10 < sensor->moving_average_10x) {
                    /* First time hitting MA? Record timestamp */
                    if (sensor->ma_hit_time_ms == 0) {
                        sensor->ma_hit_time_ms = g_system_ms;
                    }

                    /* Check if cooling down period has passed since hitting MA */
                    uint32_t time_since_ma_hit = g_system_ms - sensor->ma_hit_time_ms;
                    if (time_since_ma_hit >= g_trigger_fsm.cooling_down_ms) {
                        /* Additional check: envelope < MA * ratio */
                        /* COOLDOWN_THRESHOLD_RATIO is 0.9, so we multiply by 9 and divide by 10 */
                        if ((uint32_t)latest_envelope * 10 < sensor->moving_average_10x) {
                            /* Key is released by afterglow system, not here.
                             * Just mark sensor as ready (afterglow will expire key independently) */

                            sensor->cooldown_state = COOLDOWN_STATE_ACTIVE;

                            /* Reset threshold multiplier to 1.0 */
                            sensor->threshold_multiplier = 1.0f;
                            /* Reset adjusted_threshold */
                            sensor->adjusted_threshold = g_sensor_config.trigger_threshold_base;

                            /* Reset MA hit timestamp */
                            sensor->ma_hit_time_ms = 0;
                        }
                    }
                } else {
                    /* Envelope is back above MA, reset the timestamp */
                    sensor->ma_hit_time_ms = 0;
                }
                break;
            }
        }
    }
}

/*================= TRIGGER FSM =================*/

void trigger_fsm_process(void)
{
    switch (g_trigger_fsm.state) {
        case TRIGGER_STATE_WAIT_FOR_TRIGGER:
            wait_for_trigger_state();
            break;

        case TRIGGER_STATE_TOURNAMENT:
            tournament_state();
            break;

        case TRIGGER_STATE_FINAL:
            final_state();
            break;
    }
}

static void wait_for_trigger_state(void)
{
	/* get the max ring buffer value among all sensors */
	uint16_t max_envelope = 0;
	for(int i = 0; i< SENSORS_ACTIVE; i++)
	{
		/* Get latest envelope value */
		uint16_t latest_envelope = ring_buffer_get_latest(&g_sensors[i]);
		if(latest_envelope > max_envelope)
			max_envelope = latest_envelope;
	}

    /* Check each active sensor that is not in cooldown */

	for (int i = 0; i < SENSORS_ACTIVE; i++) {
        if (g_sensors[i].cooldown_state != COOLDOWN_STATE_ACTIVE) {
            continue;
        }


        /* Get latest envelope value */
        uint16_t latest_envelope = ring_buffer_get_latest(&g_sensors[i]);
		g_sensors[i].trigger_value = latest_envelope;

        /* Check if envelope exceeds sensor-specific threshold (using float threshold_multiplier) */

        // the trigger sensor's value cannot be less than 75% of the max value among all sensors
        if ((latest_envelope > g_sensors[i].adjusted_threshold) && (latest_envelope > ((max_envelope * 7) >> 3) )) {
        	/* give some bounus in tournament for the first trigger */
        	g_sensors[i].tournament_bonus_multiplier = (uint32_t)(g_sensor_config.threshold_multiplier_boost * 10);
            /* Trigger detected! Start tournament */
            g_trigger_fsm.state = TRIGGER_STATE_TOURNAMENT;
            g_trigger_fsm.tournament_start_ms = g_system_ms;

            /* Reset tournament data */
            g_tournament.num_contenders = 0;
            g_tournament.winner_idx = 0;
            g_tournament.best_rms = 0;  // Fixed-point *10

            /* Add all active sensors to tournament and boost the threshold */
            for (int j = 0; j < SENSORS_ACTIVE; j++) {
//                if (g_sensors[j].cooldown_state == COOLDOWN_STATE_ACTIVE)
            	if(1) // add all sensors to be eligible for tournament, no matter cooling down
                {
                    g_tournament.contender_sensors[g_tournament.num_contenders++] = j;
                    g_sensors[j].rms_accumulator = 0;  // Reset accumulator
                    g_sensors[j].rms_sample_count = 0;
                    g_sensors[j].adjusted_threshold = (float)max_envelope * g_trigger_fsm.threshold_multiplier_boost;
                }
            }

            return;
        }
    }
}

static void tournament_state(void)
{
    /* Calculate average envelope for each contender during tournament */
    for (int i = 0; i < g_tournament.num_contenders; i++) {
        uint8_t sensor_id = g_tournament.contender_sensors[i];
        if(g_sensors[sensor_id].rms_sample_count == 0)
        {
        	g_sensors[sensor_id].rms_accumulator = g_sensors[sensor_id].trigger_value;
        	g_sensors[sensor_id].rms_sample_count = 1;
        }
        /* Get latest envelope and add to accumulator */
        uint16_t envelope = ring_buffer_get_latest(&g_sensors[sensor_id]);
        g_sensors[sensor_id].rms_accumulator += envelope;
        g_sensors[sensor_id].rms_sample_count++;
    }

    /* Check if tournament duration has elapsed */
    uint32_t elapsed_ms = g_system_ms - g_trigger_fsm.tournament_start_ms;
    if (elapsed_ms >= g_trigger_fsm.tournament_duration_ms) {
        /* Tournament complete - calculate average and find winner */
        for (int i = 0; i < g_tournament.num_contenders; i++) {
            uint8_t sensor_id = g_tournament.contender_sensors[i];

            if (g_sensors[sensor_id].rms_sample_count > 0) {
                /* Calculate average * 10 (fixed-point) using the accumulator, tournament_bonus_multiplier starts from 10 */
                g_sensors[sensor_id].rms_value = (g_sensors[sensor_id].rms_accumulator * g_sensors[sensor_id].tournament_bonus_multiplier ) /
                                               g_sensors[sensor_id].rms_sample_count;
//                g_sensors[sensor_id].rms_value += g_sensors[sensor_id].tournament_bonus_multiplier >> 1;

                /* Track winner (both values are *10 fixed-point) */
                if (g_sensors[sensor_id].rms_value > g_tournament.best_rms) {
                    g_tournament.best_rms = g_sensors[sensor_id].rms_value;
                    g_tournament.winner_idx = i;
                }
            }
        }

        g_trigger_fsm.state = TRIGGER_STATE_FINAL;
    }
}

static void final_state(void)
{
    uint8_t winner_id = g_tournament.contender_sensors[g_tournament.winner_idx];

    /* Check for bundle triggering */
    g_trigger_fsm.bundle_trigger = false;
    uint8_t secondary_sensor = 255;

    if (winner_id == BUNDLE1_SENSOR_A && g_tournament.num_contenders > 1) {
        /* Check if sensor B is in tournament */
        for (int i = 0; i < g_tournament.num_contenders; i++) {
            if (g_tournament.contender_sensors[i] == BUNDLE1_SENSOR_B) {
                uint32_t secondary_rms = g_sensors[BUNDLE1_SENSOR_B].rms_value;
                /* BUNDLE_TRIGGER_RATIO is 3/4 */
                uint32_t bundle_threshold = (g_tournament.best_rms * 3) >> 2;
                if (secondary_rms >= bundle_threshold) {
                    g_trigger_fsm.bundle_trigger = true;
                    secondary_sensor = BUNDLE1_SENSOR_B;
                }
                break;
            }
        }
    } else if (winner_id == BUNDLE1_SENSOR_B && g_tournament.num_contenders > 1) {
        /* Winner is B, check A */
        for (int i = 0; i < g_tournament.num_contenders; i++) {
            if (g_tournament.contender_sensors[i] == BUNDLE1_SENSOR_A) {
                uint32_t secondary_rms = g_sensors[BUNDLE1_SENSOR_A].rms_value;
                uint32_t bundle_threshold = (g_tournament.best_rms * 3) >> 2;
                if (secondary_rms >= bundle_threshold) {
                    g_trigger_fsm.bundle_trigger = true;
                    secondary_sensor = BUNDLE1_SENSOR_A;
                }
                break;
            }
        }
    } else if (winner_id == BUNDLE2_SENSOR_A && g_tournament.num_contenders > 1) {
        /* Check if sensor B is in tournament */
        for (int i = 0; i < g_tournament.num_contenders; i++) {
            if (g_tournament.contender_sensors[i] == BUNDLE2_SENSOR_B) {
                uint32_t secondary_rms = g_sensors[BUNDLE2_SENSOR_B].rms_value;
                uint32_t bundle_threshold = (g_tournament.best_rms * 3) >> 2;
                if (secondary_rms >= bundle_threshold) {
                    g_trigger_fsm.bundle_trigger = true;
                    secondary_sensor = BUNDLE2_SENSOR_B;
                }
                break;
            }
        }
    } else if (winner_id == BUNDLE2_SENSOR_B && g_tournament.num_contenders > 1) {
        /* Winner is B, check A */
        for (int i = 0; i < g_tournament.num_contenders; i++) {
            if (g_tournament.contender_sensors[i] == BUNDLE2_SENSOR_A) {
                uint32_t secondary_rms = g_sensors[BUNDLE2_SENSOR_A].rms_value;
                uint32_t bundle_threshold = (g_tournament.best_rms * 3) >> 2;
                if (secondary_rms >= bundle_threshold) {
                    g_trigger_fsm.bundle_trigger = true;
                    secondary_sensor = BUNDLE2_SENSOR_A;
                }
                break;
            }
        }
    }

    /* Schedule key press(es) - HID report will be sent at end of ISR (buffered) */
    if (g_trigger_fsm.bundle_trigger && secondary_sensor != 255) {
        /* Schedule both keys (both will be sent in unified report at end of ISR) */
        schedule_key_press(winner_id);
        schedule_key_press(secondary_sensor);

        /* Trigger LED and debug for actual winner(s) */
        led_set_sensor_triggered(winner_id);
        debug_print_trigger(winner_id, g_sensors[winner_id].rms_value);
        led_set_sensor_triggered(secondary_sensor);
        debug_print_trigger(secondary_sensor, g_sensors[secondary_sensor].rms_value);
    } else {
        /* Schedule single key press (will be sent in unified report at end of ISR) */
        schedule_key_press(winner_id);

        /* Trigger LED and debug for winner */
        led_set_sensor_triggered(winner_id);
        debug_print_trigger(winner_id, g_sensors[winner_id].rms_value);
    }

    /* Apply cooldown to ALL sensors simultaneously */
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        g_sensors[i].cooldown_state = COOLDOWN_STATE_COOLING;
        g_sensors[i].last_trigger_ms = g_system_ms;
        g_sensors[i].ma_hit_time_ms = 0;
    }

    /* clear other sensor states */
    for(int i = 0; i < 4; ++i)
    {
    	g_sensors[i].tournament_bonus_multiplier = 10;
    }
    /* Transition back to WAIT_FOR_TRIGGER */
    g_trigger_fsm.state = TRIGGER_STATE_WAIT_FOR_TRIGGER;
}

/*================= HID TRANSMISSION =================*/

void hid_clear_report(void)
{
    /* Clear all key slots in HID report (keep modifiers at 0) */
    for (int i = 0; i < 8; i++) {
        g_hid_report[i] = 0;
    }
}

/*================= KEY SCHEDULING & BUFFERING =================*/

/**
 * @brief Schedule a key press for a sensor
 * @param sensor_id Sensor ID (0-3)
 *
 * Sets up a fresh key press period based on configured duration.
 * Always resets timestamps from current time (no extension logic).
 */
void schedule_key_press(uint8_t sensor_id)
{
    if (sensor_id >= SENSORS_ACTIVE) return;

    /* Schedule key press from current time */
    g_sensors[sensor_id].key_press_scheduled_time_ms = g_system_ms;
    g_sensors[sensor_id].key_release_scheduled_time_ms = g_system_ms + g_trigger_fsm.key_press_duration_ms;
    g_sensors[sensor_id].key_pressed = true;
}

/**
 * @brief Send buffered HID report with all active keys
 * @return Number of keys sent in report
 *
 * Builds HID report from all sensors with key_pressed = true,
 * then sends single unified report via USB.
 */
uint8_t hid_send_buffered_reports(void)
{
    /* Check USB device mode and handle accordingly */

    if (g_USB_device_type == DEVICE_TYPE_DRUMCONTROLLER)
    {
        /* Add all active buttons to the joystick report */
        for (int i = 0; i < SENSORS_ACTIVE; i++)
        {
            if (g_sensors[i].key_pressed)
            {
				/* Use bitwise OR to combine all active drum controller button codes */
            	if((joystick_report.Button & g_sensors[i].drumcontroller_keycode) == 0)
            	{
					joystick_report.Button |= g_sensors[i].drumcontroller_keycode;
					g_HID_report_modified = 1;
            	}
            }
        }
		/* Send joystick report */
        if(g_HID_report_modified)
        {
			USBD_DrumController_SendReport(&hUsbDeviceFS, (uint8_t*)&joystick_report, sizeof(USB_JoystickReport_Input_t));
			g_HID_report_modified = 0;
        }
    }
    else
    {
        /* Keyboard Mode - Build HID Keyboard Report */
        uint8_t num_keys = 0;
        /* Add all active keys to report */
        for (int i = 0; i < SENSORS_ACTIVE; i++) {
            if (g_sensors[i].key_pressed) {
                /* Check if this keycode is already in the report to avoid duplicates */
                bool keycode_exists = false;
                for (int slot = 2; slot < 8; slot++) {
                    if (g_hid_report[slot] == g_sensors[i].hid_keycode) {
                        keycode_exists = true;
                        break;
                    }
                }

                /* Only add the keycode if it's not already in the report */
                if (!keycode_exists) {
                    /* Find next empty slot (start at index 2, after modifier/reserved bytes) */
                    for (int slot = 2; slot < 8; slot++) {
                        if (g_hid_report[slot] == 0) {
                            g_hid_report[slot] = g_sensors[i].hid_keycode;
                            num_keys++;
                            g_HID_report_modified = 1;
                            break;
                        }
                    }
                    /* Stop if all 6 key slots filled */
                    if (num_keys >= 6) break;
                }
            }
        }

        /* Send keyboard report, only transmit changes */
        if(g_HID_report_modified)
        {
        	USBD_HID_SendReport(&hUsbDeviceFS, g_hid_report, 8);
        	g_HID_report_modified = 0;
        }

        /* For keyboard mode, return number of keys added to report */
        return num_keys;
    }

    /* Default return for the drum controller case */
    return 0;
}

/**
 * @brief Process afterglow and expire keys that have reached release time
 *
 * Checks each sensor's key_release_scheduled_time_ms against current
 * system time. If release time reached, marks key as not pressed and
 * clears timestamp.
 */
void hid_process_afterglow(void)
{
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        if (g_sensors[i].key_pressed && g_sensors[i].key_release_scheduled_time_ms > 0) {
            /* Check if release time has been reached */
            if (g_system_ms >= g_sensors[i].key_release_scheduled_time_ms) {
                /* Expire the key */
                g_sensors[i].key_pressed = false;
                g_sensors[i].key_press_scheduled_time_ms = 0;
                g_sensors[i].key_release_scheduled_time_ms = 0;

                /* Remove from HID report (will be rebuilt next cycle) */
                if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
                {
					for (int slot = 2; slot < 8; slot++) {
						if (g_hid_report[slot] == g_sensors[i].hid_keycode) {
							g_hid_report[slot] = 0;
							g_HID_report_modified = 1; // to notify HID report transmission
							break;
						}
					}
                }
                else if(g_USB_device_type == DEVICE_TYPE_DRUMCONTROLLER)
                {
                	if((joystick_report.Button & g_sensors[i].drumcontroller_keycode) != 0)
                	{
                		joystick_report.Button &= (~g_sensors[i].drumcontroller_keycode);
                		g_HID_report_modified = 1;// to notify HID report transmission
                	}
                }
            }
        }
    }
}

/*================= LED CONTROL =================*/

void led_set_sensor_active(uint8_t sensor_id)
{
    if (sensor_id >= SENSORS_ACTIVE) return;
    /* LEDs are low active: 0 = ON, 1 = OFF */
    HAL_GPIO_WritePin(g_sensors[sensor_id].led_port, g_sensors[sensor_id].led_pin, GPIO_PIN_RESET);
}

void led_set_sensor_idle(uint8_t sensor_id)
{
    if (sensor_id >= SENSORS_ACTIVE) return;
    /* LEDs are low active: 0 = ON, 1 = OFF */
    HAL_GPIO_WritePin(g_sensors[sensor_id].led_port, g_sensors[sensor_id].led_pin, GPIO_PIN_SET);
}

void led_set_sensor_triggered(uint8_t sensor_id)
{
    /* Turn on LED (active low) and record timestamp */
    if (sensor_id >= SENSORS_ACTIVE) return;
    HAL_GPIO_WritePin(g_sensors[sensor_id].led_port, g_sensors[sensor_id].led_pin, GPIO_PIN_RESET);
    g_sensors[sensor_id].led_on_time_ms = g_system_ms;
}

void led_update_all(void)
{
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        if (g_sensors[i].cooldown_state == COOLDOWN_STATE_ACTIVE) {
            uint16_t latest_envelope = ring_buffer_get_latest(&g_sensors[i]);
            /* Compare with moving_average (which is *10, so multiply envelope by 10) */
            if ((uint32_t)latest_envelope * 10 > g_sensors[i].moving_average_10x) {
                led_set_sensor_active(i);
            } else {
                led_set_sensor_idle(i);
            }
        }
    }
}

/*================= THRESHOLD RECOVERY =================*/

void decay_adjusted_threshold(void)
{
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        float exceed = g_sensors[i].adjusted_threshold - g_sensor_config.trigger_threshold_base;
        if (exceed > 0) {
                /* Apply decay: multiplier decays using decay factor */
                g_sensors[i].adjusted_threshold -= (1.f - g_trigger_fsm.threshold_decay_factor_float) * exceed;
                /* Clamp to minimum BASE threshold */
                if (g_sensors[i].adjusted_threshold < g_sensor_config.trigger_threshold_base) {
                    g_sensors[i].adjusted_threshold = g_sensor_config.trigger_threshold_base;
            }
        }
    }
}

/*================= UTILITY FUNCTIONS =================*/

void increment_system_ms(void)
{
    g_system_ms++;
}

/*================= LED TURN-OFF CHECK =================*/

void led_check_turn_off(uint32_t current_ms)
{
    for (int i = 0; i < SENSORS_ACTIVE; i++) {
        /* Check if LED is on (led_on_time_ms > 0) and 20ms has passed */
        if (g_sensors[i].led_on_time_ms > 0) {
            if ((current_ms - g_sensors[i].led_on_time_ms) >= 20) {
                /* Turn off LED (active low) */
                HAL_GPIO_WritePin(g_sensors[i].led_port, g_sensors[i].led_pin, GPIO_PIN_SET);
                g_sensors[i].led_on_time_ms = 0;  /* Mark as off */
            }
        }
    }
}

/*================= DEBUG OUTPUT =================*/

void debug_print_trigger(uint8_t sensor_id, uint32_t rms_value)
{
#if UART_DEBUG_ENABLED && UART_DEBUG_TRIGGER
    char buffer[64];
    /* rms_value and moving_average are fixed-point *10, divide by 10 for display */
    int len = snprintf(buffer, sizeof(buffer),
                      "TRIG: Sensor %d, RMS: %d.%d, MA: %d.%d\r\n",
                      sensor_id,
                      (int)(rms_value / 10),
                      (int)(rms_value % 10),
                      (int)(g_sensors[sensor_id].moving_average_10x / 10),
                      (int)(g_sensors[sensor_id].moving_average_10x % 10));
    HAL_UART_Transmit(&huart3, (uint8_t *)buffer, len, 10);
#endif
}

/**
 * @brief Initialize debug state
 */
void debug_init(void)
{
    // Manually clear the volatile debug state
    for (int i = 0; i < 5; i++) {
        g_debug_state.max_values[i] = 0;
    }
    g_debug_state.last_output_ms = 0;
}

/**
 * @brief Update max value for a sensor
 * @param sensor_id Sensor ID (0-4)
 * @param value Current envelope value
 */
void debug_update_max(uint8_t sensor_id, uint16_t value)
{
    if (sensor_id < 5) {
        if (value > g_debug_state.max_values[sensor_id]) {
            g_debug_state.max_values[sensor_id] = value;
        }
    }
}

/**
 * @brief Output max values every 100ms if needed
 * @param current_ms Current system time in ms
 */


void debug_output_if_needed(uint32_t current_ms)
{
#if UART_DEBUG_ENABLED
    /* Don't output debug info if terminal mode is active */
    if (uart_terminal_is_active()) {
        return;
    }

    if ((current_ms - g_debug_state.last_output_ms) >= 100) {
        /* Build output string */
        static char buffer[128];
        int len = snprintf(buffer, sizeof(buffer),
                          "ADC_MAX: [S0:%4u S1:%4u S2:%4u S3:%4u S4:%4u]\r\n",
                          g_debug_state.max_values[0],
                          g_debug_state.max_values[1],
                          g_debug_state.max_values[2],
                          g_debug_state.max_values[3],
                          g_debug_state.max_values[4]);

        /* Route output through CDC if connected, otherwise UART */
        uart_terminal_send((uint8_t *)buffer, len);

        /* Reset max values for next period */
        for (int i = 0; i < 5; i++) {
            g_debug_state.max_values[i] = 0;
        }

        /* Update last output time */
        g_debug_state.last_output_ms = current_ms;
    }
#endif
}
