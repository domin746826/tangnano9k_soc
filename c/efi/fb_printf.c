#include <stdarg.h>
#include "soc.h"

// ai slop ahead....
static void fb_print_uint(uint32_t val, uint32_t base, int width, char pad_char, int upper) {
    char buf[12];
    int i = 0;

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            uint32_t rem = val % base;
            val /= base;
            if (rem < 10) {
                buf[i++] = '0' + rem;
            } else {
                buf[i++] = (upper ? 'A' : 'a') + (rem - 10);
            }
        }
    }

    while (i < width && i < 11) {
        buf[i++] = pad_char;
    }

    // Odwracamy bufor do wypisania przez fb_print
    char out[12];
    int j = 0;
    while (i > 0) {
        out[j++] = buf[--i];
    }
    out[j] = '\0';
    fb_print(out);
}

void fb_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char ch_buf[2] = {0, 0};

    while (*fmt) {
        if (*fmt != '%') {
            ch_buf[0] = *fmt++;
            fb_print(ch_buf);
            continue;
        }

        fmt++; // pomiń '%'
        if (*fmt == '\0') break;

        // Obsługa opcjonalnego wypełnienia, np. %08x lub %4u
        char pad_char = ' ';
        int width = 0;
        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                fb_print((char *)(s ? s : "(null)"));
                break;
            }
            case 'd': {
                int32_t v = va_arg(args, int32_t);
                if (v < 0) {
                    fb_print("-");
                    fb_print_uint((uint32_t)(-v), 10, width, pad_char, 0);
                } else {
                    fb_print_uint((uint32_t)v, 10, width, pad_char, 0);
                }
                break;
            }
            case 'u': {
                uint32_t v = va_arg(args, uint32_t);
                fb_print_uint(v, 10, width, pad_char, 0);
                break;
            }
            case 'x': {
                uint32_t v = va_arg(args, uint32_t);
                fb_print_uint(v, 16, width, pad_char, 0);
                break;
            }
            case 'X':
            case 'p': {
                uint32_t v = va_arg(args, uint32_t);
                fb_print_uint(v, 16, width, pad_char, 1);
                break;
            }
            case 'c': {
                ch_buf[0] = (char)va_arg(args, int);
                fb_print(ch_buf);
                break;
            }
            case '%': {
                fb_print("%");
                break;
            }
            default: {
                ch_buf[0] = *fmt;
                fb_print(ch_buf);
                break;
            }
        }
        fmt++;
    }

    va_end(args);
}