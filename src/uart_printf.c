/*
 * uart_printf.c — Minimal printf built on uart_putc.
 *
 * Now that uart_putc buffers into the TX ring instead of blocking on
 * the hardware, this whole function is essentially a memcpy sequence —
 * formatting and buffer push in microseconds, wire time is later.
 *
 * Supports %c %s %d %u %x %%. No width, no precision, no floats.
 */
#include <stdarg.h>
#include "uart.h"

static void print_uint(uint32_t value, int base) {
    char buf[32];
    int i = 0;
    if (value == 0) { uart_putc('0'); return; }
    while (value > 0) {
        int digit = value % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }
    while (i--) uart_putc(buf[i]);
}

static void print_int(int value) {
    if (value < 0) {
        uart_putc('-');
        value = -(unsigned)value;    /* safe for INT_MIN */
    }
    print_uint((uint32_t)value, 10);
}

void uart_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { uart_putc(*p); continue; }
        p++;
        switch (*p) {
            case 'c': uart_putc((char)va_arg(ap, int)); break;
            case 's': uart_puts(va_arg(ap, const char *)); break;
            case 'd': print_int(va_arg(ap, int)); break;
            case 'u': print_uint(va_arg(ap, unsigned int), 10); break;
            case 'x': print_uint(va_arg(ap, unsigned int), 16); break;
            case '%': uart_putc('%'); break;
            default:  uart_putc('%'); uart_putc(*p); break;
        }
    }
    va_end(ap);
}
