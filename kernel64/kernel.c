/*
 * Nix-Opus 64-bit Kernel
 * Main entry point
 */

#include "include/types.h"
#include "include/console.h"
#include "../boot/uefi/bootinfo.h"

/* Serial port for debugging */
#define SERIAL_PORT 0x3F8

static void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);    /* Disable interrupts */
    outb(SERIAL_PORT + 3, 0x80);    /* Enable DLAB */
    outb(SERIAL_PORT + 0, 0x03);    /* 38400 baud */
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);    /* 8N1 */
    outb(SERIAL_PORT + 2, 0xC7);    /* Enable FIFO */
    outb(SERIAL_PORT + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static void serial_putchar(char c) {
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putchar('\r');
        serial_putchar(*s++);
    }
}

static void serial_puthex(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    char buf[17];
    buf[16] = 0;
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    serial_puts(buf);
}

/* Forward declarations */
void keyboard_init(void);
bool keyboard_has_key(void);
char keyboard_getchar(void);
char keyboard_getchar_nonblock(void);
int keyboard_readline(char *buf, size_t max_len);

void shell_init(void);
void shell_run(void);

/* String functions */
static size_t strlen(const char *str) {
    size_t len = 0;
    while (*str++) len++;
    return len;
}

static void *memset(void *dst, int c, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = c;
    return dst;
}

static void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

/* Global boot info pointer */
static boot_info_t *g_boot_info;

/*
 * Print a banner
 */
static void print_banner(void) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_println("========================================");
    console_println("         Nix-Opus 64-bit Kernel         ");
    console_println("           UEFI + GOP Edition           ");
    console_println("========================================");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_putchar('\n');
}

/*
 * Print boot info
 */
static void print_boot_info(void) {
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_puts("[INFO] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Framebuffer: ");
    console_print_dec(g_boot_info->framebuffer.width);
    console_puts("x");
    console_print_dec(g_boot_info->framebuffer.height);
    console_puts(" @ ");
    console_print_hex(g_boot_info->framebuffer.base);
    console_putchar('\n');

    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_puts("[INFO] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Kernel loaded at: ");
    console_print_hex(g_boot_info->kernel_physical);
    console_puts(" (");
    console_print_dec(g_boot_info->kernel_size);
    console_puts(" bytes)\n");

    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_puts("[INFO] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Memory regions: ");
    console_print_dec(g_boot_info->memory_map_entries);
    console_putchar('\n');

    /* Print memory map summary */
    uint64_t total_usable = 0;
    for (uint32_t i = 0; i < g_boot_info->memory_map_entries; i++) {
        if (g_boot_info->memory_map[i].type == MEMORY_USABLE) {
            total_usable += g_boot_info->memory_map[i].length;
        }
    }

    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_puts("[INFO] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Usable memory: ");
    console_print_dec(total_usable / (1024 * 1024));
    console_puts(" MB\n");

    console_putchar('\n');
}

/*
 * Initialize interrupts (basic IDT setup)
 */
static void init_interrupts(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("[INIT] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Setting up interrupts...\n");

    /* For now, just keep interrupts disabled */
    /* Full IDT setup would go here */

    console_puts("       Interrupts configured\n");
}

/*
 * Kernel main entry point
 */
void kernel_main(boot_info_t *boot_info) {
    /* Initialize serial for debugging */
    serial_init();
    serial_puts("\n[KERNEL] Nix-Opus kernel starting...\n");

    /* Verify boot info */
    if (boot_info == NULL) {
        serial_puts("[KERNEL] ERROR: boot_info is NULL!\n");
        while (1) hlt();
    }

    serial_puts("[KERNEL] boot_info at: 0x");
    serial_puthex((uint64_t)boot_info);
    serial_puts("\n");

    if (boot_info->magic != BOOTINFO_MAGIC) {
        serial_puts("[KERNEL] ERROR: Invalid boot_info magic!\n");
        serial_puts("[KERNEL] Expected: 0x");
        serial_puthex(BOOTINFO_MAGIC);
        serial_puts(" Got: 0x");
        serial_puthex(boot_info->magic);
        serial_puts("\n");
        while (1) hlt();
    }

    serial_puts("[KERNEL] Boot info validated\n");
    serial_puts("[KERNEL] Framebuffer: 0x");
    serial_puthex(boot_info->framebuffer.base);
    serial_puts(" ");
    serial_puthex(boot_info->framebuffer.width);
    serial_puts("x");
    serial_puthex(boot_info->framebuffer.height);
    serial_puts("\n");

    g_boot_info = boot_info;

    serial_puts("[KERNEL] Initializing console...\n");
    /* Initialize console with framebuffer */
    console_init(&boot_info->framebuffer);
    serial_puts("[KERNEL] Console initialized\n");

    /* Print banner */
    serial_puts("[KERNEL] Printing banner...\n");
    print_banner();

    /* Print boot info */
    serial_puts("[KERNEL] Printing boot info...\n");
    print_boot_info();

    /* Initialize subsystems */
    serial_puts("[KERNEL] Initializing subsystems...\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("[INIT] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Initializing kernel subsystems...\n");

    init_interrupts();

    serial_puts("[KERNEL] Initializing keyboard...\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("[INIT] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Initializing keyboard...\n");
    keyboard_init();
    console_puts("       Keyboard ready\n");

    serial_puts("[KERNEL] Initializing shell...\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("[INIT] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Initializing shell...\n");
    shell_init();
    console_puts("       Shell ready\n");

    /* Enable interrupts */
    serial_puts("[KERNEL] Enabling interrupts...\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("[INIT] ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts("Enabling interrupts...\n");
    sti();

    /* Print ready message */
    console_putchar('\n');
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_println("========================================");
    console_println("     Nix-Opus is ready!                 ");
    console_println("========================================");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);

    /* Run shell */
    serial_puts("[KERNEL] Starting shell...\n");
    shell_run();

    /* Should never reach here */
    while (1) hlt();
}
