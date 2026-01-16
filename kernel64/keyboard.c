/*
 * Nix-Opus 64-bit Kernel
 * PS/2 Keyboard Driver (polling mode for simplicity)
 */

#include "include/types.h"
#include "include/console.h"

#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64

/* US keyboard scancode to ASCII */
static const char scancode_to_ascii[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,  ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0
};

static const char scancode_to_ascii_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,  ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  0
};

#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LCTRL        0x1D
#define SC_CAPS_LOCK    0x3A
#define SC_RELEASE      0x80

static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool caps_lock = false;

/*
 * Initialize keyboard
 */
void keyboard_init(void) {
    /* Flush keyboard buffer */
    while (inb(KBD_STATUS_PORT) & 0x01) {
        inb(KBD_DATA_PORT);
    }
}

/*
 * Check if a key is available
 */
bool keyboard_has_key(void) {
    return (inb(KBD_STATUS_PORT) & 0x01) != 0;
}

/*
 * Get a character (polling, blocking)
 */
char keyboard_getchar(void) {
    while (1) {
        /* Wait for key */
        while (!(inb(KBD_STATUS_PORT) & 0x01)) {
            hlt();
        }

        uint8_t scancode = inb(KBD_DATA_PORT);

        /* Handle key release */
        if (scancode & SC_RELEASE) {
            scancode &= ~SC_RELEASE;
            if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
                shift_pressed = false;
            } else if (scancode == SC_LCTRL) {
                ctrl_pressed = false;
            }
            continue;
        }

        /* Handle modifier keys */
        if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
            shift_pressed = true;
            continue;
        }
        if (scancode == SC_LCTRL) {
            ctrl_pressed = true;
            continue;
        }
        if (scancode == SC_CAPS_LOCK) {
            caps_lock = !caps_lock;
            continue;
        }

        /* Convert to ASCII */
        char c;
        if (shift_pressed) {
            c = scancode_to_ascii_shift[scancode & 0x7F];
        } else {
            c = scancode_to_ascii[scancode & 0x7F];
        }

        /* Handle caps lock */
        if (caps_lock) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            else if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }

        /* Handle Ctrl+C */
        if (ctrl_pressed && (c == 'c' || c == 'C')) {
            return 3;  /* ETX */
        }

        if (c != 0) {
            return c;
        }
    }
}

/*
 * Get a character (non-blocking)
 */
char keyboard_getchar_nonblock(void) {
    if (!keyboard_has_key()) {
        return 0;
    }

    uint8_t scancode = inb(KBD_DATA_PORT);

    /* Handle key release */
    if (scancode & SC_RELEASE) {
        scancode &= ~SC_RELEASE;
        if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
            shift_pressed = false;
        } else if (scancode == SC_LCTRL) {
            ctrl_pressed = false;
        }
        return 0;
    }

    /* Handle modifier keys */
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = true;
        return 0;
    }
    if (scancode == SC_LCTRL) {
        ctrl_pressed = true;
        return 0;
    }
    if (scancode == SC_CAPS_LOCK) {
        caps_lock = !caps_lock;
        return 0;
    }

    /* Convert to ASCII */
    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode & 0x7F];
    } else {
        c = scancode_to_ascii[scancode & 0x7F];
    }

    if (caps_lock) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        else if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }

    return c;
}

/*
 * Read a line with echo
 */
int keyboard_readline(char *buf, size_t max_len) {
    size_t pos = 0;

    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n' || c == '\r') {
            buf[pos] = '\0';
            console_putchar('\n');
            return pos;
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                console_putchar('\b');
                console_putchar(' ');
                console_putchar('\b');
            }
        } else if (c == 3) {
            /* Ctrl+C */
            buf[0] = '\0';
            console_putchar('^');
            console_putchar('C');
            console_putchar('\n');
            return -1;
        } else if (c >= 32 && c < 127) {
            buf[pos++] = c;
            console_putchar(c);
        }
    }

    buf[pos] = '\0';
    return pos;
}
