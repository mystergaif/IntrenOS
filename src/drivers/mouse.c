/**
 * PS2 mouse setup
 * see https://wiki.osdev.org/PS/2_Mouse
 */

#include "mouse.h"
#include "8259_pic.h"
#include "console.h"
#include "idt.h"
#include "io_ports.h"
#include "isr.h"
#include "string.h"
#include "types.h"
#include "vga.h"

// VGA dimensions
#define VGA_WIDTH 80
#define VGA_HEIGHT 24

// VGA colors
#define COLOR_BLACK 0
#define COLOR_WHITE 15

int g_mouse_x_pos = 0, g_mouse_y_pos = 0;
MOUSE_STATUS g_status;

// Add smoothing variables
static int last_x = 0;
static int last_y = 0;
static const float smoothing_factor = 0.3; // Adjust this value between 0 and 1 for different smoothing

int mouse_getx() {
    return g_mouse_x_pos;
}

int mouse_gety() {
    return g_mouse_y_pos;
}

void mouse_wait(BOOL type) {
    uint32 time_out = 100000;
    if (type == FALSE) {
        // suspend until status is 1
        while (time_out--) {
            if ((inportb(PS2_CMD_PORT) & 1) == 1) {
                return;
            }
        }
        return;
    } else {
        while (time_out--) {
            if ((inportb(PS2_CMD_PORT) & 2) == 0) {
                return;
            }
        }
    }
}

void mouse_write(uint8 data) {
    // sending write command
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0xD4);
    mouse_wait(TRUE);
    // finally write data to port
    outportb(MOUSE_DATA_PORT, data);
}

uint8 mouse_read() {
    mouse_wait(FALSE);
    return inportb(MOUSE_DATA_PORT);
}

void get_mouse_status(char status_byte, MOUSE_STATUS *status) {
    memset(status, 0, sizeof(MOUSE_STATUS));
    if (status_byte & 0x01)
        status->left_button = 1;
    if (status_byte & 0x02)
        status->right_button = 1;
    if (status_byte & 0x04)
        status->middle_button = 1;
    if (status_byte & 0x08)
        status->always_1 = 1;
    if (status_byte & 0x10)
        status->x_sign = 1;
    if (status_byte & 0x20)
        status->y_sign = 1;
    if (status_byte & 0x40)
        status->x_overflow = 1;
    if (status_byte & 0x80)
        status->y_overflow = 1;
}

void print_mouse_info() {
    console_gotoxy(0, 0);
    console_printf("Mouse Demo X: %d, Y: %d\n", g_mouse_x_pos, g_mouse_y_pos);
    if (g_status.left_button) {
        console_printf("Left button clicked");
    }
    if (g_status.right_button) {
        console_printf("Right button clicked");
    }
    if (g_status.middle_button) {
        console_printf("Middle button clicked");
    }
}

void mouse_handler(REGISTERS *r) {
    static uint8 mouse_cycle = 0;
    static char mouse_byte[3];
    uint8 status;

    // Check if there's data to read
    status = inportb(PS2_CMD_PORT);
    if (!(status & 1)) {
        isr_end_interrupt(IRQ_BASE + 12);
        return;
    }

    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = mouse_read();
            if (!(mouse_byte[0] & 0x08)) {
                // Invalid first byte
                mouse_cycle = 0;
                isr_end_interrupt(IRQ_BASE + 12);
                return;
            }
            get_mouse_status(mouse_byte[0], &g_status);
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = mouse_read();
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = mouse_read();
            
            // Apply smoothing to movement
            int new_x = g_mouse_x_pos + mouse_byte[1];
            int new_y = g_mouse_y_pos - mouse_byte[2];
            
            // Smooth the movement
            g_mouse_x_pos = (int)(last_x + (new_x - last_x) * smoothing_factor);
            g_mouse_y_pos = (int)(last_y + (new_y - last_y) * smoothing_factor);
            
            // Update last positions
            last_x = g_mouse_x_pos;
            last_y = g_mouse_y_pos;

            if (g_mouse_x_pos < 0)
                g_mouse_x_pos = 0;
            if (g_mouse_y_pos < 0)
                g_mouse_y_pos = 0;
            if (g_mouse_x_pos > VGA_WIDTH)
                g_mouse_x_pos = VGA_WIDTH - 1;
            if (g_mouse_y_pos > VGA_HEIGHT)
                g_mouse_y_pos = VGA_HEIGHT - 1;

            console_clear(COLOR_WHITE, COLOR_BLACK);
            console_gotoxy(g_mouse_x_pos, g_mouse_y_pos);
            console_putchar('X');
            print_mouse_info();
            mouse_cycle = 0;
            break;
    }
    
    // Acknowledge the interrupt
    isr_end_interrupt(IRQ_BASE + 12);
}

/**
 * available rates 10, 20, 40, 60, 80, 100, 200
 */
void set_mouse_rate(uint8 rate) {
    uint8 status;

    outportb(MOUSE_DATA_PORT, MOUSE_CMD_SAMPLE_RATE);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        console_printf("error: failed to send mouse sample rate command\n");
        return;
    }
    outportb(MOUSE_DATA_PORT, rate);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        console_printf("error: failed to send mouse sample rate data\n");
        return;
    }
}

void mouse_init() {
    uint8 status;

    g_mouse_x_pos = 5;
    g_mouse_y_pos = 2;

    console_printf("initializing mouse...\n");

    // Wait for keyboard controller to be ready
    do {
        status = inportb(PS2_CMD_PORT);
    } while (status & 2);

    // Disable mouse interface
    outportb(PS2_CMD_PORT, 0xA7);
    
    // Wait for command to complete
    do {
        status = inportb(PS2_CMD_PORT);
    } while (status & 2);

    // Clear output buffer
    do {
        status = inportb(PS2_CMD_PORT);
        if (status & 1) {
            inportb(MOUSE_DATA_PORT);
        }
    } while (status & 1);

    // Enable mouse interface
    outportb(PS2_CMD_PORT, 0xA8);

    // Wait for command to complete
    do {
        status = inportb(PS2_CMD_PORT);
    } while (status & 2);

    // Use default settings
    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        console_printf("error: failed to set default mouse settings\n");
        return;
    }

    // Set mouse sample rate to 40 (slower than default 100)
    set_mouse_rate(40);

    // Enable the interrupt
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0x20);
    mouse_wait(FALSE);
    status = inportb(MOUSE_DATA_PORT);
    status |= 2;  // Enable IRQ12
    mouse_wait(TRUE);
    outportb(PS2_CMD_PORT, 0x60);
    mouse_wait(TRUE);
    outportb(MOUSE_DATA_PORT, status);

    // Enable packet streaming
    mouse_write(MOUSE_CMD_ENABLE_PACKET_STREAMING);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        console_printf("error: failed to enable mouse packet streaming\n");
        return;
    }

    // Set mouse handler
    isr_register_interrupt_handler(IRQ_BASE + 12, mouse_handler);
}

