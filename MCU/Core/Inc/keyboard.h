/*
 * keyboard.h
 * USB HID Keyboard Report Structure Definitions
 */

#ifndef INC_KEYBOARD_H_
#define INC_KEYBOARD_H_

#include <stdint.h>

/* USB HID Keyboard Report Structure (Standard Boot Protocol) */
typedef struct {
    uint8_t modifier;    // Modifier keys (bit flags: Ctrl, Shift, Alt, GUI)
    uint8_t reserved;    // Reserved byte (always 0)
    uint8_t keycode[6];  // Up to 6 simultaneous key presses (6KRO)
} HID_KeyboardReport_t;

/* USB HID Usage Table Keyboard/Keypad Page (0x07) */
/* Modifier bits */
#define KEY_MOD_LCTRL   0x01
#define KEY_MOD_LSHIFT  0x02
#define KEY_MOD_LALT    0x04
#define KEY_MOD_LGUI    0x08
#define KEY_MOD_RCTRL   0x10
#define KEY_MOD_RSHIFT  0x20
#define KEY_MOD_RALT    0x40
#define KEY_MOD_RGUI    0x80

/* Keycodes as defined in USB HID Usage Tables v1.12 */
/* Keyboard a-z */
#define KEY_A       0x04
#define KEY_B       0x05
#define KEY_C       0x06
#define KEY_D       0x07
#define KEY_E       0x08
#define KEY_F       0x09
#define KEY_G       0x0A
#define KEY_H       0x0B
#define KEY_I       0x0C
#define KEY_J       0x0D
#define KEY_K       0x0E
#define KEY_L       0x0F
#define KEY_M       0x10
#define KEY_N       0x11
#define KEY_O       0x12
#define KEY_P       0x13
#define KEY_Q       0x14
#define KEY_R       0x15
#define KEY_S       0x16
#define KEY_T       0x17
#define KEY_U       0x18
#define KEY_V       0x19
#define KEY_W       0x1A
#define KEY_X       0x1B
#define KEY_Y       0x1C
#define KEY_Z       0x1D

/* Keyboard 1-9, 0 */
#define KEY_1       0x1E
#define KEY_2       0x1F
#define KEY_3       0x20
#define KEY_4       0x21
#define KEY_5       0x22
#define KEY_6       0x23
#define KEY_7       0x24
#define KEY_8       0x25
#define KEY_9       0x26
#define KEY_0       0x27

/* Return, Esc, etc. */
#define KEY_ENTER   0x28
#define KEY_ESC     0x29
#define KEY_BACKSPACE 0x2A
#define KEY_TAB     0x2B
#define KEY_SPACE   0x2C
#define KEY_MINUS   0x2D
#define KEY_EQUAL   0x2E

/* Arrow Keys */
#define KEY_UP      0x52
#define KEY_DOWN    0x51
#define KEY_LEFT    0x50
#define KEY_RIGHT   0x4F

/* Keypad */
#define KEY_KP_1    0x59
#define KEY_KP_2    0x5A
#define KEY_KP_3    0x5B
#define KEY_KP_4    0x5C
#define KEY_KP_5    0x5D
#define KEY_KP_6    0x5E
#define KEY_KP_7    0x5F
#define KEY_KP_8    0x60
#define KEY_KP_9    0x61
#define KEY_KP_0    0x62

/* Function Keys */
#define KEY_F1      0x3A
#define KEY_F2      0x3B
#define KEY_F3      0x3C
#define KEY_F4      0x3D
#define KEY_F5      0x3E
#define KEY_F6      0x3F
#define KEY_F7      0x40
#define KEY_F8      0x41
#define KEY_F9      0x42
#define KEY_F10     0x43
#define KEY_F11     0x44
#define KEY_F12     0x45

/* Currently pressed keys buffer (extern in piezosensor.c) */
extern uint8_t g_hid_report[8];

#endif /* INC_KEYBOARD_H_ */
