/*
 * sensor_config.c
 * Sensor configuration management implementation
 */

#include "sensor_config.h"
#include "stm32f1xx_hal.h"
#include "string.h"
#include "stdio.h"
#include "piezo_config.h"
#include "piezosensor.h"
#include "main.h"
#include "uart_terminal.h"

/* Extern variables */
extern UART_HandleTypeDef huart3;
extern PiezoSensor_t g_sensors[SENSORS_ACTIVE];
extern TriggerFSM_t g_trigger_fsm;


/* Global configuration instance with defaults */
const uint16_t SENSOR_MULTIPLIERS_DEFAULT[5] = {8, 12, 12, 8, 8};
SensorConfig_t g_sensor_config = {0};

/* Terminal mode state */
static bool g_terminal_mode_active = false;
static uint8_t g_uart_rx_buffer[1];  // Single byte receive buffer

/* Private function prototypes */
static void uart_start_receive(void);
static bool parse_fixed_point(const char *str, uint16_t *value);
static bool parse_uint16(const char *str, uint16_t *value);
static void handle_set_command(const char *var_name, const char *value_str);
static void handle_query_command(const char *var_name);
static bool parse_simple_float(const char *str, float *result);
static void fixed_point_to_string(char *buffer, uint16_t fixed_value);

/*================= INITIALIZATION =================*/

void sensor_config_init(void)
{
    /* Try to load configuration from flash */
    if (!sensor_config_load_from_flash()) {
        /* Load failed - use defaults */
        sensor_config_set_defaults();
    }

    /* Apply loaded configuration to runtime variables */
    g_trigger_fsm.current_threshold = g_sensor_config.sensor_threshold[0];
    /* Note: Thresholds are per-sensor but we use a global threshold currently */
    /* Multipliers will be applied during envelope extraction */

    /* Apply runtime parameters */
    g_trigger_fsm.tournament_duration_ms = g_sensor_config.tournament_duration_ms;
    g_trigger_fsm.cooling_down_ms = g_sensor_config.cooling_down_ms;
    g_trigger_fsm.key_press_duration_ms = g_sensor_config.key_press_duration_ms;
    g_trigger_fsm.threshold_multiplier_boost = g_sensor_config.threshold_multiplier_boost;
    g_trigger_fsm.threshold_decay_factor_float = g_sensor_config.threshold_decay_factor_float;

    /* Start UART reception for command input */
    uart_start_receive();
}

void sensor_config_set_defaults(void)
{
    g_sensor_config.magic = FLASH_CONFIG_MAGIC;
    g_sensor_config.version = 2;  // Increment version for new structure

    /* Set default multipliers to 1.0 (no pre-amplification), stored as 10 (1.0 * 10) */
    for (int i = 0; i < 5; i++) {
        g_sensor_config.sensor_multiplier[i] = SENSOR_MULTIPLIERS_DEFAULT[i];  // 10 = 1.0 in fixed-point (*10)
    }

    /* Set default thresholds */
    for (int i = 0; i < 5; i++) {
        g_sensor_config.sensor_threshold[i] = TRIGGER_THRESHOLD_BASE;
    }

    /* Set default runtime parameters */
    g_sensor_config.trigger_threshold_base = TRIGGER_THRESHOLD_BASE;
    g_sensor_config.tournament_duration_ms = TOURNAMENT_DURATION_MS;
    g_sensor_config.cooling_down_ms = COOLING_DOWN_MS;
    g_sensor_config.key_press_duration_ms = KEY_PRESS_DURATION_MS;
    g_sensor_config.threshold_multiplier_boost = THRESHOLD_MULTIPLIER_BOOST;
    g_sensor_config.threshold_decay_factor_float = THRESHOLD_DECAY_FACTOR_FLOAT;

    g_sensor_config.checksum = sensor_config_calculate_checksum(&g_sensor_config);
}

/* Helper function to convert fixed-point *10 value to display string */
void fixed_point_to_string(char *buffer, uint16_t fixed_value)
{
    uint16_t integer_part = fixed_value / 10;
    uint16_t decimal_part = fixed_value % 10;

    if (decimal_part == 0) {
        snprintf(buffer, 16, "%u", integer_part);
    } else {
        snprintf(buffer, 16, "%u.%u", integer_part, decimal_part);
    }
}

/*================= FLASH OPERATIONS =================*/

bool sensor_config_load_from_flash(void)
{
    /* Read configuration from flash */
    SensorConfig_t *flash_config = (SensorConfig_t *)FLASH_CONFIG_ADDRESS;

    /* Check magic word */
    if (flash_config->magic != FLASH_CONFIG_MAGIC) {
        return false;
    }

    /* Check version - support both version 1 and 2 */
    if (flash_config->version != 1 && flash_config->version != 2) {
        return false;  // Version mismatch
    }

    /* Verify checksum */
    uint16_t stored_checksum = flash_config->checksum;
    uint16_t calculated_checksum = sensor_config_calculate_checksum(flash_config);
    if (stored_checksum != calculated_checksum) {
        return false;
    }

    /* Load configuration */
    if (flash_config->version == 1) {
        /* Load old version - copy only the fields that exist in version 1 */
        memcpy(&g_sensor_config, flash_config, offsetof(SensorConfig_t, trigger_threshold_base));
        g_sensor_config.version = 1;

        /* Set defaults for new parameters */
        g_sensor_config.trigger_threshold_base = TRIGGER_THRESHOLD_BASE;
        g_sensor_config.tournament_duration_ms = TOURNAMENT_DURATION_MS;
        g_sensor_config.cooling_down_ms = COOLING_DOWN_MS;
        g_sensor_config.key_press_duration_ms = KEY_PRESS_DURATION_MS;
        g_sensor_config.threshold_multiplier_boost = THRESHOLD_MULTIPLIER_BOOST;
        g_sensor_config.threshold_decay_factor_float = THRESHOLD_DECAY_FACTOR_FLOAT;
    } else {
        /* Load full version 2 structure */
        memcpy(&g_sensor_config, flash_config, sizeof(SensorConfig_t));
    }

    /* Apply loaded configuration to runtime variables */
    g_trigger_fsm.current_threshold = g_sensor_config.sensor_threshold[0];
    /* Note: Thresholds are per-sensor but we use a global threshold currently */
    /* Multipliers will be applied during envelope extraction */

    return true;
}

bool sensor_config_save_to_flash(void)
{
    HAL_StatusTypeDef status;
    uint32_t primask_bit;

    /* Update checksum before saving */
    g_sensor_config.checksum = sensor_config_calculate_checksum(&g_sensor_config);

    /* Disable interrupts before flash operations (CRITICAL SECTION) */
    primask_bit = __get_PRIMASK();
    __disable_irq();

    /* Unlock flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        __set_PRIMASK(primask_bit);  // Restore interrupt state
        return false;
    }

    /* Clear error flags */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    /* Erase the page containing our configuration */
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_CONFIG_ADDRESS;
    erase_init.NbPages = 1;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        __set_PRIMASK(primask_bit);  // Restore interrupt state
        return false;
    }

    /* Write configuration to flash word by word (use memcpy to avoid unaligned access) */
    uint32_t num_words = (sizeof(SensorConfig_t) + 3) / 4;  // Round up to word boundary
    uint32_t word_buffer;

    for (uint32_t i = 0; i < num_words; i++) {
        /* Copy 4 bytes at a time to aligned buffer */
        memcpy(&word_buffer, ((uint8_t *)&g_sensor_config) + (i * 4), 4);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   FLASH_CONFIG_ADDRESS + (i * 4),
                                   word_buffer);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            __set_PRIMASK(primask_bit);  // Restore interrupt state
            return false;
        }
    }

    /* Lock flash */
    status = HAL_FLASH_Lock();

    /* Re-enable interrupts (CRITICAL SECTION END) */
    __enable_irq();
    __set_PRIMASK(primask_bit);

    if (status != HAL_OK) {
        return false;
    }

    return true;
}

uint16_t sensor_config_calculate_checksum(const SensorConfig_t *config)
{
    /* Simple additive checksum of all data except checksum field */
    uint16_t checksum = 0;

    /* Calculate checksum for all bytes except the checksum field itself */
    const uint8_t *data = (const uint8_t *)config;
    for (uint32_t i = 0; i < sizeof(SensorConfig_t) - 2; i++) {
        checksum += data[i];
    }

    return checksum;
}

/*================= UART RECEIVE =================*/

static void uart_start_receive(void)
{
    /* Start receiving single bytes with interrupt */
    HAL_UART_Receive_IT(&huart3, g_uart_rx_buffer, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        /* Forward byte to terminal processor */
        uart_terminal_receive_byte(g_uart_rx_buffer[0]);

        /* Restart receive for next byte */
        uart_start_receive();
    }
}

/* Optional: Add error callback for debugging */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        /* Could add error handling here if needed */
    }
}

/*================= COMMAND PROCESSING =================*/

void sensor_config_process_command(const char *cmd_buffer, uint16_t length)
{
    /* If this is the first command, switch to terminal mode */
    if (!g_terminal_mode_active) {
        g_terminal_mode_active = true;
    }

    /* Make sure command is null-terminated for string functions */
    char cmd[64];
    uint16_t cmd_len = (length < 63) ? length : 63;
    memcpy(cmd, cmd_buffer, cmd_len);
    cmd[cmd_len] = '\0';

    /* Trim whitespace */
    while (cmd_len > 0 && (cmd[cmd_len - 1] == '\r' || cmd[cmd_len - 1] == '\n')) {
        cmd[--cmd_len] = '\0';
    }

    /* Parse command */
    if (cmd_len == 0) {
        return;  // Empty command
    }

    /* Check for save command */
    if (strcmp(cmd, "save") == 0) {
        if (sensor_config_save_to_flash()) {
            sensor_config_send_ok();
        } else {
            sensor_config_send_string("ERROR: Save failed\r\n");
        }
        return;
    }

    /* Check for help command */
    if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        sensor_config_send_string("\r\n========================================\r\n");
        sensor_config_send_string("  TAIKO SENSOR CONFIGURATION TERMINAL\r\n");
        sensor_config_send_string("========================================\r\n\r\n");
        sensor_config_send_string("COMMAND FORMAT:\r\n");
        sensor_config_send_string("  <variable>=<value>  Set a value (no spaces)\r\n");
        sensor_config_send_string("  <variable>?         Query current value\r\n");
        sensor_config_send_string("  <command>           Execute action\r\n\r\n");
        sensor_config_send_string("=============== SET COMMANDS ===============\r\n");
        sensor_config_send_string("Per-Sensor Settings (X = 0-4):\r\n");
        sensor_config_send_string("  multX=VAL    Sensor X multiplier (0.1-4096.0)  [eg: mult0=0.8]\r\n");
        sensor_config_send_string("  thX=VAL      Sensor X threshold (0-65535)    [eg: th0=150]\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("Global Trigger Settings:\r\n");
        sensor_config_send_string("  base_th=VAL  Base trigger threshold (0-65535)  [eg: base_th=150]\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("Timing Parameters (1-100 ms):\r\n");
        sensor_config_send_string("  tour_dur=VAL Tournament duration  [eg: tour_dur=4]\r\n");
        sensor_config_send_string("  cool_down=VAL Cooldown period     [eg: cool_down=15]\r\n");
        sensor_config_send_string("  key_dur=VAL  Key press duration   [eg: key_dur=20]\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("Threshold Dynamics:\r\n");
        sensor_config_send_string("  th_boost=VAL Threshold boost (0.1-10.0)  [eg: th_boost=1.5]\r\n");
        sensor_config_send_string("  th_decay=VAL Threshold decay (0.0-1.0)  [eg: th_decay=0.97]\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("============ QUERY COMMANDS =============\r\n");
        sensor_config_send_string("Use ? to view any value (e.g., th0?, base_th?)\r\n\r\n");
        sensor_config_send_string("============ ACTION COMMANDS =============\r\n");
        sensor_config_send_string("  save         Save all settings to flash\r\n");
        sensor_config_send_string("  defaults     Restore default settings\r\n");
        sensor_config_send_string("  reboot       Restart the device\r\n");
        sensor_config_send_string("  help or ?    Show this help message\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("============ USAGE EXAMPLES ============\r\n");
        sensor_config_send_string("  SETTINGS:\r\n");
        sensor_config_send_string("    mult0=1.5      Set sensor 0 to 1.5x multiplier\r\n");
        sensor_config_send_string("    th0=500        Set sensor 0 threshold to 500\r\n");
        sensor_config_send_string("    base_th=200    Set base threshold to 200\r\n");
        sensor_config_send_string("    th_boost=2.0   Set threshold boost to 2.0\r\n");
        sensor_config_send_string("    tour_dur=10    Set tournament duration to 10ms\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("  QUERIES:\r\n");
        sensor_config_send_string("    th0?           Show sensor 0 threshold\r\n");
        sensor_config_send_string("    base_th?       Show base threshold\r\n");
        sensor_config_send_string("    th_boost?      Show threshold boost value\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("  ACTIONS:\r\n");
        sensor_config_send_string("    save           Store all settings\r\n");
        sensor_config_send_string("    defaults       Restore factory defaults\r\n");
        sensor_config_send_string("    reboot         Restart device\r\n");
        sensor_config_send_string("    help           Show this help\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("Press ? or type help at any time\r\n");
        sensor_config_send_string("========================================\r\n\r\n");
        return;
    }

    /* Look for '=' or '?' */
    char *equals_pos = strchr(cmd, '=');
    char *question_pos = strchr(cmd, '?');

    if (equals_pos != NULL) {
        /* Set command: VAR=VALUE */
        *equals_pos = '\0';
        const char *var_name = cmd;
        const char *value_str = equals_pos + 1;

        handle_set_command(var_name, value_str);
    } else if (question_pos != NULL) {
        /* Query command: VAR? */
        *question_pos = '\0';
        const char *var_name = cmd;

        handle_query_command(var_name);
    } else {
        /* Unknown command */
        sensor_config_send_string("ERROR: Unknown command\r\n");
    }
}

/*================= COMMAND HANDLERS =================*/

static void handle_set_command(const char *var_name, const char *value_str)
{
    uint16_t uint_val;
    float float_val;

    /* Handle multiplier commands: mult0, mult1, mult2, mult3, mult4 */
    if (strlen(var_name) == 5 && strncmp(var_name, "mult", 4) == 0) {
        int sensor_idx = var_name[4] - '0';
        if (sensor_idx >= 0 && sensor_idx < 5) {
            uint16_t fixed_value;
            if (parse_fixed_point(value_str, &fixed_value)) {
                g_sensor_config.sensor_multiplier[sensor_idx] = fixed_value;
                sensor_config_send_ok();
                return;
            }
        }
    }

    /* Handle threshold commands: th0, th1, th2, th3, th4 */
    if (strlen(var_name) == 3 && strncmp(var_name, "th", 2) == 0) {
        int sensor_idx = var_name[2] - '0';
        if (sensor_idx >= 0 && sensor_idx < 5) {
            if (parse_uint16(value_str, &uint_val)) {
                g_sensor_config.sensor_threshold[sensor_idx] = uint_val;

                /* Update global threshold if setting sensor 0 */
                if (sensor_idx == 0) {
                    g_trigger_fsm.current_threshold = uint_val;
                }

                sensor_config_send_ok();
                return;
            }
        }
    }

    /* Handle base threshold: base_th */
    if (strcmp(var_name, "base_th") == 0) {
        if (parse_uint16(value_str, &uint_val)) {
            g_sensor_config.trigger_threshold_base = uint_val;
            /* Sync base threshold to all sensor thresholds */
            for (int i = 0; i < 5; i++) {
                g_sensor_config.sensor_threshold[i] = uint_val;
            }
            /* Also update global threshold if sensor 0 is used */
            g_trigger_fsm.current_threshold = uint_val;
            sensor_config_send_ok();
            return;
        }
    }

    /* Handle tournament duration: tour_dur */
    if (strcmp(var_name, "tour_dur") == 0) {
        if (parse_uint16(value_str, &uint_val)) {
            g_sensor_config.tournament_duration_ms = uint_val;
            g_trigger_fsm.tournament_duration_ms = uint_val;
            sensor_config_send_ok();
            return;
        }
    }

    /* Handle cooling down: cool_down */
    if (strcmp(var_name, "cool_down") == 0) {
        if (parse_uint16(value_str, &uint_val)) {
            g_sensor_config.cooling_down_ms = uint_val;
            g_trigger_fsm.cooling_down_ms = uint_val;
            sensor_config_send_ok();
            return;
        }
    }

    /* Handle key press duration: key_dur */
    if (strcmp(var_name, "key_dur") == 0) {
        if (parse_uint16(value_str, &uint_val)) {
            g_sensor_config.key_press_duration_ms = uint_val;
            g_trigger_fsm.key_press_duration_ms = uint_val;
            sensor_config_send_ok();
            return;
        }
    }

    /* Handle threshold multiplier boost: th_boost */
    if (strcmp(var_name, "th_boost") == 0) {
        if (parse_simple_float(value_str, &float_val)) {
            if (float_val > 0.0f && float_val <= 10.0f) {
                g_sensor_config.threshold_multiplier_boost = float_val;
                g_trigger_fsm.threshold_multiplier_boost = float_val;
                sensor_config_send_ok();
                return;
            }
        }
    }

    /* Handle threshold decay factor: th_decay */
    if (strcmp(var_name, "th_decay") == 0) {
        if (parse_simple_float(value_str, &float_val)) {
            if (float_val > 0.0f && float_val < 1.0f) {
                g_sensor_config.threshold_decay_factor_float = float_val;
                g_trigger_fsm.threshold_decay_factor_float = float_val;
                sensor_config_send_ok();
                return;
            }
        }
    }

    /* Unknown variable */
    sensor_config_send_string("ERROR: Invalid variable\r\n");
}

static void handle_query_command(const char *var_name)
{
    char buffer[64];

    /* Handle help command */
    if (strcmp(var_name, "help") == 0 || strcmp(var_name, "h") == 0) {
        sensor_config_send_string("\r\n========== CONFIGURATION COMMANDS ==========\r\n");
        sensor_config_send_string("Command        | Description                     | Example\r\n");
        sensor_config_send_string("---------------|---------------------------------|------------------\r\n");
        sensor_config_send_string("mult0..4       | Sensor multipliers (0.1-4096)   | set mult0 10.5\r\n");
        sensor_config_send_string("th0..4         | Sensor thresholds (0-65535)     | set th0 150\r\n");
        sensor_config_send_string("base_th        | Base trigger threshold          | set base_th 200\r\n");
        sensor_config_send_string("tour_dur       | Tournament duration (ms)        | set tour_dur 5\r\n");
        sensor_config_send_string("cool_down      | Cooling down period (ms)        | set cool_down 15\r\n");
        sensor_config_send_string("key_dur        | Key press duration (ms)         | set key_dur 20\r\n");
        sensor_config_send_string("th_boost       | Threshold boost (0.1-10.0)      | set th_boost 1.5\r\n");
        sensor_config_send_string("th_decay       | Decay factor (0.0-1.0)          | set th_decay 0.97\r\n");
        sensor_config_send_string("\r\n");
        sensor_config_send_string("Query: Add '?' to any variable | Example: set base_th?\r\n");
        sensor_config_send_string("Save: save                       | Save to flash\r\n");
        sensor_config_send_string("Defaults: defaults               | Restore defaults\r\n");
        sensor_config_send_string("Reboot: reboot                   | Restart device\r\n");
        sensor_config_send_string("Help: help or h                  | Show this help\r\n");
        sensor_config_send_string("=============================================\r\n\r\n");
        return;
    }

    /* Handle multiplier queries: mult0?, mult1?, etc. */
    if (strlen(var_name) == 5 && strncmp(var_name, "mult", 4) == 0) {
        int sensor_idx = var_name[4] - '0';
        if (sensor_idx >= 0 && sensor_idx < 5) {
            /* Send multiplier as fixed-point value with decimal formatting */
            char value_str[16];
            fixed_point_to_string(value_str, g_sensor_config.sensor_multiplier[sensor_idx]);
            int len = snprintf(buffer, sizeof(buffer), "mult%u=%s\r\n", sensor_idx, value_str);
            uart_terminal_send((const uint8_t *)buffer, len);
            return;
        }
    }

    /* Handle threshold queries: th0?, th1?, etc. */
    if (strlen(var_name) == 3 && strncmp(var_name, "th", 2) == 0) {
        int sensor_idx = var_name[2] - '0';
        if (sensor_idx >= 0 && sensor_idx < 5) {
            char buffer[32];
            int len = snprintf(buffer, sizeof(buffer), "th%d=%u\r\n", sensor_idx, g_sensor_config.sensor_threshold[sensor_idx]);
            uart_terminal_send((const uint8_t *)buffer, len);
            return;
        }
    }

    /* Handle base threshold query: base_th? */
    if (strcmp(var_name, "base_th") == 0) {
        sensor_config_send_uint16("base_th", g_sensor_config.trigger_threshold_base);
        return;
    }

    /* Handle tournament duration query: tour_dur? */
    if (strcmp(var_name, "tour_dur") == 0) {
        sensor_config_send_uint16("tour_dur", g_sensor_config.tournament_duration_ms);
        return;
    }

    /* Handle cooling down query: cool_down? */
    if (strcmp(var_name, "cool_down") == 0) {
        sensor_config_send_uint16("cool_down", g_sensor_config.cooling_down_ms);
        return;
    }

    /* Handle key press duration query: key_dur? */
    if (strcmp(var_name, "key_dur") == 0) {
        sensor_config_send_uint16("key_dur", g_sensor_config.key_press_duration_ms);
        return;
    }

    /* Handle threshold multiplier boost query: th_boost? */
    if (strcmp(var_name, "th_boost") == 0) {
        sensor_config_send_float("th_boost", g_sensor_config.threshold_multiplier_boost);
        return;
    }

    /* Handle threshold decay factor query: th_decay? */
    if (strcmp(var_name, "th_decay") == 0) {
        sensor_config_send_float("th_decay", g_sensor_config.threshold_decay_factor_float);
        return;
    }

    /* Unknown variable */
    sensor_config_send_string("ERROR: Invalid variable\r\n");
}

/*================= PARSING UTILITIES =================*/

/* Simple string to float converter for embedded systems */
static bool parse_simple_float(const char *str, float *result)
{
    int32_t integer_part = 0;
    int32_t decimal_part = 0;
    bool after_decimal = false;
    bool has_decimal = false;
    int digit_count = 0;

    /* Skip leading spaces */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Parse digits */
    while (*str != '\0' && digit_count < 10) {
        if (*str >= '0' && *str <= '9') {
            if (after_decimal) {
                /* Only take first decimal digit */
                if (!has_decimal) {
                    decimal_part = *str - '0';
                    has_decimal = true;
                }
            } else {
                integer_part = integer_part * 10 + (*str - '0');
            }
            digit_count++;
        } else if (*str == '.') {
            if (after_decimal) {
                return false; /* Multiple decimal points */
            }
            after_decimal = true;
        } else {
            /* Invalid character */
            return false;
        }
        str++;
    }

    /* Check range (0.0 to 99.9) */
    if (integer_part > 99) {
        return false;
    }

    /* Combine into float */
    *result = (float)integer_part + ((float)decimal_part / 10.0f);

    return true;
}

static bool parse_fixed_point(const char *str, uint16_t *value)
{
    /* Parse number with 1 decimal place max, return as fixed-point *10 */
    int32_t integer_part = 0;
    int32_t decimal_part = 0;
    bool negative = false;
    bool after_decimal = false;

    /* Skip leading spaces */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Check for sign */
    if (*str == '-') {
        negative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Parse digits */
    while (*str != '\0') {
        if (*str >= '0' && *str <= '9') {
            if (after_decimal) {
                decimal_part = decimal_part * 10 + (*str - '0');
            } else {
                integer_part = integer_part * 10 + (*str - '0');
            }
        } else if (*str == '.' && !after_decimal) {
            after_decimal = true;
        } else {
            /* Invalid character */
            return false;
        }
        str++;
    }

    /* Calculate fixed-point value (multiply by 10) */
    int32_t result = integer_part * 10 + decimal_part;

    /* Clamp decimal part to 1 digit (tenths) */
    if (decimal_part >= 10) {
        decimal_part = 9;
        result = integer_part * 10 + decimal_part;
    }

    if (negative) {
        result = -result;
    }

    /* Check range (allow up to 4096.0 = 40960 in fixed-point) */
    if (result < 0 || result > 40960) {
        return false;
    }

    *value = (uint16_t)result;
    return true;
}

static bool parse_uint16(const char *str, uint16_t *value)
{
    /* Parse unsigned integer */
    uint32_t result = 0;

    /* Skip leading spaces */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /* Parse digits */
    while (*str != '\0') {
        if (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            if (result > 65535) {
                return false;  /* Overflow */
            }
        } else {
            /* Invalid character */
            return false;
        }
        str++;
    }

    *value = (uint16_t)result;
    return true;
}

/*================= UART OUTPUT =================*/

void sensor_config_send_ok(void)
{
    sensor_config_send_string("OK\r\n");
}

void sensor_config_send_string(const char *str)
{
    uart_terminal_send((const uint8_t *)str, strlen(str));
}

void sensor_config_send_float(const char *prefix, float value)
{
    char buffer[32];
    int len;

    /* Format float with 1 decimal place max */
    int32_t int_part = (int32_t)value;
    float frac = value - (float)int_part;

    if (frac < 0.01f) {
        /* No decimal part */
        len = snprintf(buffer, sizeof(buffer), "%s=%ld\r\n", prefix, (long)int_part);
    } else {
        /* One decimal place */
        int32_t frac_part = (int32_t)(frac * 10.0f + 0.5f);  // Round to 1 digit
        if (frac_part >= 10) {
            frac_part = 9;  // Clamp to single digit
        }
        len = snprintf(buffer, sizeof(buffer), "%s=%ld.%ld\r\n", prefix,
                       (long)int_part, (long)frac_part);
    }

    uart_terminal_send((const uint8_t *)buffer, len);
}

void sensor_config_send_uint16(const char *prefix, uint16_t value)
{
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%s=%u\r\n", prefix, value);
    uart_terminal_send((const uint8_t *)buffer, len);
}

/*================= ACCESSOR FUNCTIONS =================*/

bool sensor_config_is_terminal_mode(void)
{
    return g_terminal_mode_active;
}

bool sensor_config_save_pending(void)
{
    /* Save to flash is manually triggered by "save" command,
     * not automatic, so always return false here. */
    return false;
}
