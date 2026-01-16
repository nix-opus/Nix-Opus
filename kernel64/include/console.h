/*
 * Nix-Opus 64-bit Kernel
 * Framebuffer Console
 */

#ifndef _KERNEL_CONSOLE_H
#define _KERNEL_CONSOLE_H

#include "types.h"
#include "../../boot/uefi/bootinfo.h"

/* Color definitions (32-bit ARGB) */
#define COLOR_BLACK       0x00000000
#define COLOR_WHITE       0x00FFFFFF
#define COLOR_RED         0x00FF0000
#define COLOR_GREEN       0x0000FF00
#define COLOR_BLUE        0x000000FF
#define COLOR_CYAN        0x0000FFFF
#define COLOR_MAGENTA     0x00FF00FF
#define COLOR_YELLOW      0x00FFFF00
#define COLOR_LIGHT_GRAY  0x00C0C0C0
#define COLOR_DARK_GRAY   0x00808080

/* Default colors */
#define DEFAULT_FG        COLOR_LIGHT_GRAY
#define DEFAULT_BG        COLOR_BLACK

/* Initialize the console with framebuffer info */
void console_init(framebuffer_info_t *fb);

/* Clear the screen */
void console_clear(void);

/* Set foreground and background colors */
void console_set_color(uint32_t fg, uint32_t bg);

/* Print a single character */
void console_putchar(char c);

/* Print a string */
void console_puts(const char *str);

/* Print a string with newline */
void console_println(const char *str);

/* Print a hex number */
void console_print_hex(uint64_t val);

/* Print a decimal number */
void console_print_dec(uint64_t val);

/* Get console dimensions in characters */
void console_get_size(uint32_t *cols, uint32_t *rows);

/* Set cursor position */
void console_set_cursor(uint32_t col, uint32_t row);

/* Get cursor position */
void console_get_cursor(uint32_t *col, uint32_t *row);

/* Draw a pixel directly (for graphics) */
void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Fill a rectangle */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

#endif /* _KERNEL_CONSOLE_H */
