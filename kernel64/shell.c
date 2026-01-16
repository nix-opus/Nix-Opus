/*
 * Nix-Opus 64-bit Kernel
 * Simple Shell
 */

#include "include/types.h"
#include "include/console.h"
#include "../boot/uefi/bootinfo.h"

/* External functions */
extern int keyboard_readline(char *buf, size_t max_len);
extern boot_info_t *g_boot_info;

/* String functions */
static size_t strlen(const char *str) {
    size_t len = 0;
    while (*str++) len++;
    return len;
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

#define SHELL_MAX_CMD_LEN 256
#define SHELL_MAX_ARGS    16

/* Command structure */
typedef struct {
    const char *name;
    const char *help;
    int (*func)(int argc, char **argv);
} shell_cmd_t;

/* Forward declarations */
static int cmd_help(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_mem(int argc, char **argv);
static int cmd_uname(int argc, char **argv);
static int cmd_reboot(int argc, char **argv);

/* Command table */
static const shell_cmd_t commands[] = {
    { "help",   "Show available commands",   cmd_help   },
    { "clear",  "Clear the screen",          cmd_clear  },
    { "echo",   "Print text",                cmd_echo   },
    { "mem",    "Show memory info",          cmd_mem    },
    { "uname",  "Show system info",          cmd_uname  },
    { "reboot", "Reboot the system",         cmd_reboot },
    { NULL, NULL, NULL }
};

/*
 * Print prompt
 */
static void print_prompt(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("nix");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(":");
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_puts("/");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts("$ ");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
}

/*
 * Parse command line
 */
static int parse_cmdline(char *cmdline, char **argv, int max_args) {
    int argc = 0;
    char *p = cmdline;

    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }

    return argc;
}

/*
 * Help command
 */
static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;

    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_println("Nix-Opus Shell Commands:");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);

    for (int i = 0; commands[i].name != NULL; i++) {
        console_puts("  ");
        console_set_color(COLOR_CYAN, COLOR_BLACK);
        console_puts(commands[i].name);
        console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);

        size_t len = strlen(commands[i].name);
        for (size_t j = len; j < 10; j++) {
            console_putchar(' ');
        }

        console_puts("- ");
        console_println(commands[i].help);
    }

    return 0;
}

/*
 * Clear command
 */
static int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
    return 0;
}

/*
 * Echo command
 */
static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) console_putchar(' ');
        console_puts(argv[i]);
    }
    console_putchar('\n');
    return 0;
}

/*
 * Memory info command
 */
static int cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Note: We'd need the actual boot_info pointer passed to kernel_main */
    /* For now, print a placeholder */
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_println("Memory Information:");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_println("  (Memory map not yet accessible from shell)");

    return 0;
}

/*
 * Uname command
 */
static int cmd_uname(int argc, char **argv) {
    (void)argc; (void)argv;

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("Nix-Opus");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_puts(" v0.3 (x86_64) - UEFI Edition\n");

    return 0;
}

/*
 * Reboot command
 */
static int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;

    console_println("Rebooting...");

    /* Try keyboard controller reset */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);

    /* If that didn't work, triple fault */
    cli();
    uint64_t zero = 0;
    __asm__ volatile("lidt %0" : : "m"(zero));
    __asm__ volatile("int $0");

    while (1) hlt();
    return 0;
}

/*
 * Execute command
 */
static int shell_execute(const char *cmdline) {
    char cmd_copy[SHELL_MAX_CMD_LEN];
    char *argv[SHELL_MAX_ARGS];
    int argc;

    strncpy(cmd_copy, cmdline, SHELL_MAX_CMD_LEN - 1);
    cmd_copy[SHELL_MAX_CMD_LEN - 1] = '\0';

    argc = parse_cmdline(cmd_copy, argv, SHELL_MAX_ARGS);
    if (argc == 0) return 0;

    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].func(argc, argv);
        }
    }

    console_puts(argv[0]);
    console_println(": command not found");
    return 127;
}

/*
 * Initialize shell
 */
void shell_init(void) {
    /* Nothing to initialize for now */
}

/*
 * Run shell
 */
void shell_run(void) {
    char cmdline[SHELL_MAX_CMD_LEN];

    console_putchar('\n');
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_println("Welcome to Nix-Opus Shell!");
    console_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    console_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();

        int len = keyboard_readline(cmdline, SHELL_MAX_CMD_LEN);
        if (len < 0) continue;  /* Ctrl+C */

        if (len > 0) {
            shell_execute(cmdline);
        }
    }
}
