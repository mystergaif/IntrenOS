#include "keyboard.h"

#include "console.h"
#include "idt.h"
#include "io_ports.h"
#include "isr.h"
#include "types.h"
#include "string.h"

// Declare the global exit flag (defined in kernel.c)
extern volatile int g_exit_program;

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define SCAN_CODE_KEY_CAPS_LOCK 0x3A
#define SCAN_CODE_KEY_ENTER 0x1C
#define SCAN_CODE_KEY_TAB 0x0F
#define SCAN_CODE_KEY_LEFT_SHIFT 0x2A

static BOOL g_caps_lock = FALSE;
static BOOL g_shift_pressed = FALSE;
char g_ch = 0;

// see scan codes defined in keyboard.h for index
char g_scan_code_chars[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};


char alternate_chars(char ch) {
    switch(ch) {
        case '`': return '~';
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '\"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return ch;
    }
}

void keyboard_handler(REGISTERS *r) {
    uint8 status;
    int scancode;
    BOOL first_scancode = TRUE;

    // Read status byte and scancodes until buffer is empty
    while ((status = inportb(KEYBOARD_STATUS_PORT)) & 0x01) {
        scancode = inportb(KEYBOARD_DATA_PORT);

        // Check for ESC key (scancode 0x01)
        if (scancode == 0x01) {
            g_exit_program = 1;
            g_ch = 0x1B; // Signal ESC by setting g_ch
            // Acknowledge the interrupt
            outportb(0x20, 0x20);
            return; // Do not process as a regular character
        }

        if (first_scancode) {
            g_ch = 0; // Reset g_ch for the new key press

            if (scancode & 0x80) {
                // key release
                if (scancode == (SCAN_CODE_KEY_LEFT_SHIFT | 0x80)) {
                    g_shift_pressed = FALSE;
                }
            } else {
                // key down
                switch(scancode) {
                    case SCAN_CODE_KEY_CAPS_LOCK:
                        g_caps_lock = !g_caps_lock;
                        break;

                    case SCAN_CODE_KEY_ENTER:
                        g_ch = '\n';
                        break;

                    case SCAN_CODE_KEY_TAB:
                        g_ch = '\t';
                        break;

                    case SCAN_CODE_KEY_LEFT_SHIFT:
                        g_shift_pressed = TRUE;
                        break;

                    default:
                        if (scancode < 128) {
                            g_ch = g_scan_code_chars[scancode];
                            if (g_ch) {
                                if (g_caps_lock) {
                                    if (g_shift_pressed) {
                                        g_ch = alternate_chars(g_ch);
                                    } else {
                                        g_ch = upper(g_ch);
                                    }
                                } else if (g_shift_pressed) {
                                    if (isalpha(g_ch)) {
                                        g_ch = upper(g_ch);
                                    } else {
                                        g_ch = alternate_chars(g_ch);
                                    }
                                }
                            }
                        }
                        break;
                }
            }
            first_scancode = FALSE; // Processed the first scancode
        }
    }
    // Send EOI to Master PIC
    outportb(0x20, 0x20);
}

void keyboard_init() {
    isr_register_interrupt_handler(IRQ_BASE + 1, keyboard_handler);
}

// a blocking character read
char kb_getchar() {
    char c;

    while(g_ch == 0);
    c = g_ch;
    g_ch = 0;
    return c;
}



