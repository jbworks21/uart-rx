/*
 * uart.h — USART2 driver with interrupt-driven RX and TX.
 *
 * Both directions are now buffered through ring buffers. The main loop
 * never stalls on UART I/O unless a buffer genuinely fills up, which
 * means printf and other output feel instant even though the wire is
 * still 115200 baud.
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Initialize the UART and enable RX + TX interrupts.
 * Returns 0 on success, -1 on invalid baud. */
int uart_init(uint32_t baud);

/* Queue one byte for transmission. Non-blocking if the buffer has
 * room; blocks if the TX buffer is full (backpressure). */
void uart_putc(char c);

/* Queue a string. Same backpressure semantics as uart_putc. */
void uart_puts(const char *s);

/* Wait until the TX ring buffer is empty AND the last byte has
 * physically left the wire (TC set). Use before reset or power-down. */
void uart_flush(void);

/* Pop one byte from the RX ring buffer.
 * Returns the byte, or -1 if the buffer is empty. Never blocks. */
int uart_getc(void);

/* Mini printf. Supports %c %s %d %u %x %%. */
void uart_printf(const char *fmt, ...);

/* Diagnostics */
uint32_t uart_tx_pending(void);    /* bytes queued but not yet sent */
uint32_t uart_get_rx_drops(void);
uint32_t uart_get_overruns(void);

#endif /* UART_H */
