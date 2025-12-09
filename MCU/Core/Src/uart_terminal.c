/*
 * uart_terminal.c
 * UART terminal with idle line detection for command input
 */

#include "uart_terminal.h"
#include "sensor_config.h"
#include "stm32f1xx_hal.h"
#include "string.h"
#include "main.h"
#include "usbd_cdc_if.h"
#include "usb_cdc_wrapper.h"

/* Extern variables */
extern UART_HandleTypeDef huart3;

/* External CDC enqueue function for async transmission */
extern bool usb_cdc_enqueue_tx(const uint8_t* data, uint16_t len);

/* Function to check if CDC is connected */
static bool is_cdc_connected(void)
{
    extern USBD_HandleTypeDef hUsbDeviceFS;
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
}

/* Terminal state */
typedef struct {
    char buffer[UART_TERMINAL_BUFFER_SIZE];
    uint16_t buffer_index;
    uint32_t last_char_time_ms;
    bool terminal_active;
    bool idle_detected;
    TerminalInputMode_t input_mode;
} TerminalState_t;

static TerminalState_t g_terminal = {0};

/* Forward declaration */
static void uart_terminal_process_command(void);

/*================= INITIALIZATION =================*/

void uart_terminal_init(void)
{
    memset(&g_terminal, 0, sizeof(g_terminal));
    g_terminal.terminal_active = false;
    g_terminal.input_mode = TERMINAL_INPUT_BOTH;  /* Default: accept from both */
}

/*================= INPUT MODE CONTROL =================*/

void uart_terminal_set_input_mode(TerminalInputMode_t mode)
{
    g_terminal.input_mode = mode;
}

TerminalInputMode_t uart_terminal_get_input_mode(void)
{
    return g_terminal.input_mode;
}

/*================= BYTE RECEPTION =================*/

void uart_terminal_receive_byte(uint8_t byte)
{
    /* First byte received - switch from debug mode to terminal mode if not already active */
    if (!g_terminal.terminal_active) {
        g_terminal.terminal_active = true;
    }

    /* Reset idle timer */
    g_terminal.last_char_time_ms = HAL_GetTick();
    g_terminal.idle_detected = false;


    /* Check for newline or carriage return - process command immediately */
    if (byte == '\n' || byte == '\r') {
        g_terminal.buffer[g_terminal.buffer_index] = '\0';
        uart_terminal_process_command();
        g_terminal.buffer_index = 0;
        /* Echo newline */
        uint8_t newline[] = "\r\n";
        uart_terminal_send(newline, 2);
        return;
    }

    /* Add character to buffer if space available */
    if (g_terminal.buffer_index < UART_TERMINAL_BUFFER_SIZE - 1) {
        g_terminal.buffer[g_terminal.buffer_index++] = byte;
        /* No echo in naive command mode */
    }
}

/* Helper function to send data via CDC or UART */
void uart_terminal_send(const uint8_t* data, uint16_t len)
{
    if (is_cdc_connected())
    {
        /* Use async CDC enqueue - non-blocking */
        usb_cdc_enqueue_tx(data, len);
    }
    else
    {
        /* Fall back to UART */
        HAL_UART_Transmit(&huart3, (uint8_t*)data, len, 100);
    }
}

/*================= IDLE LINE DETECTION =================*/

void uart_terminal_tick(uint32_t current_ms)
{
    /* Check for idle line timeout */
    if (g_terminal.buffer_index > 0 && !g_terminal.idle_detected) {
        uint32_t time_since_last_char = current_ms - g_terminal.last_char_time_ms;

        if (time_since_last_char >= UART_IDLE_TIMEOUT_MS) {
            /* Line has been idle - process command */
            g_terminal.idle_detected = true;
            g_terminal.buffer[g_terminal.buffer_index] = '\0';
            uart_terminal_process_command();
            g_terminal.buffer_index = 0;
        }
    }
}

void uart_terminal_process_idle(void)
{
    uart_terminal_tick(HAL_GetTick());
}

/*================= COMMAND PROCESSING =================*/

static void uart_terminal_process_command(void)
{
    /* Skip empty commands */
    if (g_terminal.buffer_index == 0) {
        return;
    }

    /* Process command through sensor config module */
    sensor_config_process_command(g_terminal.buffer, g_terminal.buffer_index);
}

/*================= STATUS CHECK =================*/

bool uart_terminal_is_active(void)
{
    return g_terminal.terminal_active;
}
