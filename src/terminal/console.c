#include "keyboard.h"
#include "../include/vga.h"
#include "console.h"
#include "string.h"
#include "types.h"

// Declare the global exit flag (defined in kernel.c)
extern volatile int g_exit_program;

// Define VGA constants and colors for console.c
#define VGA_ADDRESS        0xB8000
#define VGA_TOTAL_ITEMS    2200
#define VGA_WIDTH     80
#define VGA_HEIGHT    24


static uint16 *g_vga_buffer;
static uint8 cursor_pos_x = 0, cursor_pos_y = 0;
uint8 g_fore_color, g_back_color;

// clear video buffer array
void console_clear(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color) {
    uint32 i;
    for (i = 0; i < VGA_TOTAL_ITEMS; i++) {
        g_vga_buffer[i] = vga_item_entry(' ', fore_color, back_color); // Use space to clear
    }
    cursor_pos_x = 0;
    cursor_pos_y = 0;
    vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
}

void console_init(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color) {
    g_vga_buffer = (uint16 *)VGA_ADDRESS;
    g_fore_color = fore_color;
    g_back_color = back_color;
    cursor_pos_x = 0;
    cursor_pos_y = 0;
    console_clear(fore_color, back_color);
}

void console_newline() {
    if (cursor_pos_y >= VGA_HEIGHT - 1) { // Scroll up when cursor reaches the last line
        // Copy each line up by one line
        for (int i = 1; i < VGA_HEIGHT; i++) {
            memcpy(g_vga_buffer + (i - 1) * VGA_WIDTH, g_vga_buffer + i * VGA_WIDTH, VGA_WIDTH * sizeof(uint16));
        }
        // Clear the last line
        for (int i = 0; i < VGA_WIDTH; i++) {
            g_vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = vga_item_entry(' ', g_fore_color, g_back_color);
        }
        // Cursor stays on the last line
        cursor_pos_x = 0;
        cursor_pos_y = VGA_HEIGHT - 1;
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
    } else { // Move to the next line
        cursor_pos_x = 0;
        cursor_pos_y++;
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
    }
}

void console_putchar(char ch) {
    if (ch == '\n') {
        console_newline();
        return;
    }
    if (cursor_pos_x >= VGA_WIDTH) {
        console_newline();
    }
    g_vga_buffer[cursor_pos_y * VGA_WIDTH + cursor_pos_x] = vga_item_entry(ch, g_fore_color, g_back_color);
    cursor_pos_x++;
    vga_set_cursor_pos(cursor_pos_x, cursor_pos_y); // Set cursor after incrementing x
}

void console_gotoxy(uint8 x, uint8 y) {
    cursor_pos_x = x;
    cursor_pos_y = y;
            vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
        }

void console_putstr(const char *str) {
    while (*str) {
        console_putchar(*str++);
    }
}

// revert back the printed character and add space to it
void console_ungetchar() {
    if (cursor_pos_x > 0) {
        cursor_pos_x--;
        g_vga_buffer[cursor_pos_y * VGA_WIDTH + cursor_pos_x] = vga_item_entry(' ', g_fore_color, g_back_color);
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
    } else if (cursor_pos_y > 0) {
        cursor_pos_y--;
        cursor_pos_x = VGA_WIDTH - 1;
        g_vga_buffer[cursor_pos_y * VGA_WIDTH + cursor_pos_x] = vga_item_entry(' ', g_fore_color, g_back_color);
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
    }
}

// revert back the printed character until bound occurs
void console_ungetchar_bound(uint8 bound) {
    // Only backspace if the cursor is beyond the bound
    if (cursor_pos_x > bound) {
        cursor_pos_x--;
        g_vga_buffer[cursor_pos_y * VGA_WIDTH + cursor_pos_x] = vga_item_entry(' ', g_fore_color, g_back_color);
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);
    }
    // Note: This function does not handle backspacing across lines,
    // as it's intended for bounded input on a single line (like a command prompt).
}

void console_printf(const char *format, ...) {
    char **arg = (char **)&format;
    int c;
    char buf[32];

    arg++;

    memset(buf, 0, sizeof(buf));
    while ((c = *format++) != 0) {
        if (c != '%')
            console_putchar(c);
        else {
            char *p, *p2;
            int pad0 = 0, pad = 0;

            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }

            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }

            switch (c) {
                case 'd':
                case 'u':
                case 'x':
                    itoa(buf, c, *((int *)arg++));
                    p = buf;
                    goto string;
                    break;

                case 's':
                    p = *arg++;
                    if (!p)
                        p = "(null)";

                string:
                    for (p2 = p; *p2; p2++)
                        ;
                    for (; p2 < p + pad; p2++)
                        console_putchar(pad0 ? '0' : ' ');
                    while (*p)
                        console_putchar(*p++);
                    break;

                default:
                    console_putchar(*((int *)arg++));
                    break;
            }
        }
    }
}

// read string from console, handles backspace and printable chars
void getstr(char *buffer) {
    if (!buffer) return;
    char *current_buffer_ptr = buffer;
    int buffer_len = 0; // Keep track of buffer length

    uint8 initial_cursor_x = cursor_pos_x;
    uint8 initial_cursor_y = cursor_pos_y;

    while(1) {
        // Ensure hardware cursor is at the correct position
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);

        // Add a small delay for blinking effect
        for (volatile int i = 0; i < 1000000; i++);

        // Ensure hardware cursor is at the correct position
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);

        char ch = kb_getchar(); // Read character using kb_getchar()

        if (ch == 0x1B) { // Check for ESC
            *buffer = '\0'; // Clear the buffer
            g_exit_program = 0; // Reset exit flag (in case it was set by handler)
            return; // Exit getstr immediately
        } else if (ch == '\n') {
            *current_buffer_ptr = '\0';
            console_newline(); // Add newline after command
            return ;
        } else if (ch == '\b') {
            // Only backspace if we are past the initial prompt position
            // and there are characters in the buffer to delete
            if (cursor_pos_x > initial_cursor_x || cursor_pos_y > initial_cursor_y) {
                 if (buffer_len > 0) {
                    console_ungetchar();
                    current_buffer_ptr--;
                    *current_buffer_ptr = '\0';
                    buffer_len--;
                 }
            }
        } else if (ch >= 32 && ch <= 126) { // Only accept printable characters
             // Add character to buffer and console
            *current_buffer_ptr = ch;
            console_putchar(ch);
            current_buffer_ptr++;
            buffer_len++;
        }
        // Ignore other control characters for now
    }
}

// read string from console, and erase or go back until bound occurs
void getstr_bound(char *buffer, uint8 bound) {
    if (!buffer) return;
    char *current_buffer_ptr = buffer;
    int buffer_len = 0; // Keep track of buffer length

    uint8 initial_cursor_x = cursor_pos_x;
    uint8 initial_cursor_y = cursor_pos_y; // Should be on the same line as bound

    while(1) {
        // Ensure hardware cursor is at the correct position
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);

        // Add a small delay for blinking effect
        for (volatile int i = 0; i < 1000000; i++);

        // Ensure hardware cursor is at the correct position
        vga_set_cursor_pos(cursor_pos_x, cursor_pos_y);

        char ch = kb_getchar(); // Read character using kb_getchar()

        if (ch == '\n') {
            *current_buffer_ptr = '\0';
            console_newline(); // Add newline after command
            return ;
        } else if (ch == '\b') {
            // Only backspace if the cursor is beyond the bound
            if (cursor_pos_x > bound) {
                 if (buffer_len > 0) {
                    console_ungetchar_bound(bound); // Use the corrected function
                    current_buffer_ptr--;
                    *current_buffer_ptr = '\0';
                    buffer_len--;
                 }
            }
        } else if (ch >= 32 && ch <= 126) { // Only accept printable characters
            // Add character to buffer and console
            *current_buffer_ptr = ch;
            console_putchar(ch);
            current_buffer_ptr++;
            buffer_len++;
        }
        // Ignore other control characters for now
    }
}
