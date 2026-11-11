/*
 * app/main.c — Echo demo with per-100-byte status report.
 *
 * Type a character in the serial terminal: it comes back immediately.
 * Every 100 bytes received, print a status line with total, drops,
 * and overruns. This makes three things visible at a glance:
 *
 *   1. The RX ISR fires at all (bytes come back)
 *   2. The main loop is draining the buffer (drops stays low)
 *   3. Nothing else in the system is starving the ISR (overruns = 0)
 *
 * Type slowly → drops stays at 0 forever. Main has plenty of time
 * between bytes to drain and echo each one.
 *
 * Paste a large block → drops climbs. That's the ring buffer
 * overflowing because main can't echo 1000 chars as fast as the ISR
 * can receive them at 115200. Not a bug; the visible signal that a
 * 64-byte buffer isn't sized for bulk input. Increase UART_RX_BUF_SIZE
 * in src/uart.c to 256 or 1024 if you want to absorb larger pastes.
 */
#include "uart.h"

static void print_u32(uint32_t v) {
    char buf[10];
    int i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10U)); v /= 10U; }
    while (i--) uart_putc(buf[i]);
}

int main(void) {
    if (uart_init(115200) != 0) {
        /* Can't print an error — the console we'd use is broken. */
        while (1) { }
    }

    uart_puts("\r\n=== UART RX interrupt demo ===\r\n");
    uart_puts("Type anything — it echoes back.\r\n");
    uart_puts("Status every 100 bytes: total, drops, overruns.\r\n\r\n");

    uint32_t total = 0;
    while (1) {
        int c = uart_getc();
        if (c < 0) continue;

        uart_putc((char)c);
        total++;

        if ((total % 100U) == 0) {
            uart_puts("\r\n[bytes=");
            print_u32(total);
            uart_puts(" drops=");
            print_u32(uart_get_drops());
            uart_puts(" overruns=");
            print_u32(uart_get_overruns());
            uart_puts("]\r\n");
        }
    }
}
