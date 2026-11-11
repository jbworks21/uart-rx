/*
 * app/main.c — Command-driven demo of the fully-buffered UART.
 *
 * Commands (single keypress):
 *   'x' — burst: queue 5 status lines in one shot, demonstrate that
 *         main returns immediately while the ISR drips them out
 *   's' — stats: tx_pending, tx_drops (there are none — TX blocks
 *         instead), rx_drops, overruns
 *   any other key — echo it back
 *
 * What to notice:
 *   - Press 'x' ten times in a row, fast. All 50 lines come out,
 *     smoothly, in order. Main never stalled on UART during the
 *     presses — each keypress was handled in <1 ms.
 *   - Right after 'x' press, 's' shows tx_pending > 0. A second
 *     later, back to 0 — the ISR drained it in the background.
 *   - No line is ever truncated mid-character. Every printf is atomic
 *     at the queue level.
 */
#include "uart.h"

int main(void) {
    if (uart_init(115200) != 0) while (1) { }

    uart_puts("\r\n=== UART fully-buffered demo ===\r\n");
    uart_puts("'x' = burst 5 lines, 's' = stats, other = echo\r\n\r\n");

    uint32_t burst_count = 0;

    while (1) {
        int c = uart_getc();
        if (c < 0) continue;

        if (c == 'x') {
            burst_count++;
            for (int i = 0; i < 5; i++) {
                uart_printf("burst %u line %d of 5 — this line is ~40 chars long\r\n",
                            burst_count, i + 1);
            }
            /* At this point, up to 5 lines (~200 bytes) are queued in
             * the TX ring buffer. main returns to the loop instantly.
             * The ISR pushes them out over the next ~17 ms at 115200. */
        } else if (c == 's') {
            uart_printf("tx_pending=%u rx_drops=%u overruns=%u\r\n",
                        uart_tx_pending(),
                        uart_get_rx_drops(),
                        uart_get_overruns());
        } else {
            uart_putc((char)c);
        }
    }
}
