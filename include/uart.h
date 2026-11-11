/*
 * uart.h — Public API of the USART2 driver with interrupt-driven RX.
 *
 * Compared to the polling driver from earlier: TX is still blocking
 * (polling TXE), but RX is now interrupt-driven. The ISR buffers every
 * incoming byte into a ring buffer; uart_getc() pops from that buffer
 * without ever blocking. The main loop can spend arbitrary time on
 * other work and no bytes are lost — up to the buffer's capacity.
 *
 * The API is the same as before. That's the point of the driver
 * boundary: nothing that calls uart_getc() needs to know whether the
 * underlying driver polls or uses interrupts.
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>

/*
 * Initialize the UART and enable the RX interrupt.
 *
 * baud: standard rates. Assumes HCLK = 16 MHz (reset default HSI).
 *
 * Returns 0 on success, -1 on invalid baud (0).
 */
int uart_init(uint32_t baud);

/* Send one byte. Blocks until the transmitter is ready. */
void uart_putc(char c);

/* Send a null-terminated string. */
void uart_puts(const char *s);

/*
 * Pop one byte from the RX ring buffer.
 * Returns the byte (0-255), or -1 if the buffer is empty.
 *
 * Never blocks. Never reads the hardware directly — the ISR already
 * took the byte from DR and stored it here.
 */
int uart_getc(void);

/*
 * Counters, useful for diagnosing the driver under load.
 *
 * drops    — bytes the ISR received but couldn't buffer because the
 *            ring buffer was full. Rises when main doesn't drain fast
 *            enough. Fix by draining faster or enlarging the buffer.
 *
 * overruns — times the hardware ORE bit fired, meaning bytes were
 *            lost before the ISR even saw them. Should be zero unless
 *            the ISR is being starved by a higher-priority interrupt.
 */
uint32_t uart_get_drops(void);
uint32_t uart_get_overruns(void);

#endif /* UART_H */
