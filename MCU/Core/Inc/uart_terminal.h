/*
 * uart_terminal.h
 * UART terminal with idle line detection for command input
 */

#ifndef INC_UART_TERMINAL_H_
#define INC_UART_TERMINAL_H_

#include <stdint.h>
#include <stdbool.h>

/* Configuration */
#define UART_TERMINAL_BUFFER_SIZE   64      // Max command length
#define UART_IDLE_TIMEOUT_MS        50      // Idle time before processing command

/* Terminal input modes */
typedef enum {
    TERMINAL_INPUT_BOTH = 0,         // Accept input from both UART and USB CDC
    TERMINAL_INPUT_USB_ONLY = 1,    // Only accept input from USB CDC
    TERMINAL_INPUT_UART_ONLY = 2,   // Only accept input from UART
} TerminalInputMode_t;
/*
========== CONFIGURATION COMMANDS ==========
 Command        | Description                     | Example
 ---------------|---------------------------------|------------------
 mult0..4       | Sensor multipliers (0.00-4096.00) | mult0=10.50
 th0..4         | Sensor thresholds (0-65535)     | th0=150
 base_th        | Base trigger threshold          | base_th=200
 tour_dur       | Tournament duration (ms)        | tour_dur=5
 cool_down      | Cooling down period (ms)        | cool_down=15
 key_dur        | Key press duration (ms)         | key_dur=20
 th_boost       | Threshold boost (0.00-10.00)    | th_boost=1.50
 th_decay       | Decay factor (0.00-1.00)        | th_decay=0.97

 Query: Add '?' after variable      | Example: base_th?
 Save: Type 'save'                  | Save to flash
 Defaults: Type 'defaults'          | Restore defaults
 Reboot: Type 'reboot' or 'reset'   | Restart device
 Help: Type 'help', 'h', or '?'     | Show this help
 =============================================
 */
/* Function prototypes */
void uart_terminal_init(void);
void uart_terminal_receive_byte(uint8_t byte);
void uart_terminal_process_idle(void);
bool uart_terminal_is_active(void);
void uart_terminal_tick(uint32_t current_ms);

/* Input mode control */
void uart_terminal_set_input_mode(TerminalInputMode_t mode);
TerminalInputMode_t uart_terminal_get_input_mode(void);
/* Helper function to send output via CDC or UART */
void uart_terminal_send(const uint8_t* data, uint16_t len);

#endif /* INC_UART_TERMINAL_H_ */
