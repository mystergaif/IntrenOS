#ifndef CONSOLE_H
#define CONSOLE_H

#define MAXIMUM_PAGES 10
#define SCROLL_UP 0
#define SCROLL_DOWN 1

#include "vga.h"

// Declare global color variables as extern
extern uint8 g_fore_color;
extern uint8 g_back_color;

void console_clear(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);

//initialize console
void console_init(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);

void console_putchar(char ch);
void console_newline();
// revert back the printed character and add 0 to it
void console_ungetchar();
// revert back the printed character until n characters
void console_ungetchar_bound(uint8 n);

void console_gotoxy(uint8 x, uint8 y);

void console_putstr(const char *str);
void console_printf(const char *format, ...);

// read string from console, but no backing
void getstr(char *buffer);

// read string from console, and erase or go back util bound occurs
void getstr_bound(char *buffer, uint8 bound);

#endif
