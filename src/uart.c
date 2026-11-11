/*
 * uart.c — USART2 driver for STM32F407, interrupt-driven RX and TX.
 *
 * TWO RING BUFFERS, SYMMETRIC PATTERN
 *
 *   RX:  ISR writes s_rx_buf[s_rx_head], main advances s_rx_tail
 *   TX:  main writes s_tx_buf[s_tx_head], ISR advances s_tx_tail
 *
 * In both directions, one writer and one reader on each index, so the
 * lock-free invariant from the RX-only version holds unchanged.
 *
 * TX GOTCHA: TXE IS ALWAYS SET WHEN THE BUFFER IS EMPTY
 *
 * Unlike RXNE (set only when a byte arrives), TXE means "ready for the
 * next byte" and stays set on an idle transmitter. If TXEIE were left
 * enabled, the ISR would fire continuously, consuming all CPU time in
 * an interrupt loop. The fix: enable TXEIE in uart_putc (when there's
 * actually work), and disable it in the ISR (when the buffer empties).
 * This is the single biggest difference from the RX-only driver.
 *
 * TXE vs TC
 *
 *   TXE set → the data register is empty, we can push the next byte
 *   TC  set → the last byte's stop bit has physically left the wire
 *
 * uart_putc only cares about TXE. uart_flush needs TC, because the
 * shift register may still be holding a byte when DR looks empty.
 */
#include "stm32f407_regs.h"
#include "uart.h"

/* ---- Wiring ----------------------------------------------------- */
#define UART_GPIO           GPIOA
#define UART_RCC_GPIOEN     RCC_AHB1ENR_GPIOAEN
#define UART_RCC_USARTEN    RCC_APB1ENR_USART2EN
#define UART_REG            USART2
#define UART_TX_PIN         2
#define UART_RX_PIN         3
#define UART_AF             7

#define UART_PCLK_HZ        16000000UL

/* ---- Ring buffers ----------------------------------------------- */
#define UART_RX_BUF_SIZE    64U
#define UART_RX_BUF_MASK    (UART_RX_BUF_SIZE - 1U)

/* TX buffer larger than RX because output bursts (printf lines, log
 * dumps) are typically longer than inbound keystrokes. 256 bytes is
 * enough to hold a 200-char line plus the next status burst without
 * blocking main. */
#define UART_TX_BUF_SIZE    256U
#define UART_TX_BUF_MASK    (UART_TX_BUF_SIZE - 1U)

static volatile uint8_t  s_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;       /* ISR writes */
static volatile uint16_t s_rx_tail;       /* main writes */
static volatile uint32_t s_rx_drops;
static volatile uint32_t s_overruns;

static volatile uint8_t  s_tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t s_tx_head;       /* main writes */
static volatile uint16_t s_tx_tail;       /* ISR writes */

int uart_init(uint32_t baud) {
    if (baud == 0) return -1;

    s_rx_head = s_rx_tail = 0;
    s_tx_head = s_tx_tail = 0;
    s_rx_drops = s_overruns = 0;

    RCC->AHB1ENR |= UART_RCC_GPIOEN;
    RCC->APB1ENR |= UART_RCC_USARTEN;

    UART_GPIO->MODER &= ~((3U << (UART_TX_PIN * 2)) | (3U << (UART_RX_PIN * 2)));
    UART_GPIO->MODER |=  ((2U << (UART_TX_PIN * 2)) | (2U << (UART_RX_PIN * 2)));
    UART_GPIO->AFR[0] &= ~((0xFU << (UART_TX_PIN * 4)) | (0xFU << (UART_RX_PIN * 4)));
    UART_GPIO->AFR[0] |=  ((uint32_t)UART_AF << (UART_TX_PIN * 4));
    UART_GPIO->AFR[0] |=  ((uint32_t)UART_AF << (UART_RX_PIN * 4));

    UART_REG->BRR = (UART_PCLK_HZ + baud / 2U) / baud;

    /* Enable TX, RX, RXNEIE, and the peripheral.
     * TXEIE is deliberately NOT set here — see uart_putc. */
    UART_REG->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    /* USART2_IRQn = 38 → ISER1 bit 6. */
    NVIC_ISER1 = (1U << (USART2_IRQN - 32U));

    return 0;
}

void uart_putc(char c) {
    uint16_t next = (s_tx_head + 1U) & UART_TX_BUF_MASK;

    /* Backpressure: if the buffer is full, wait for the ISR to drain.
     * This can only happen if main outpaces 115200 baud, which means
     * a very long string. Blocking here is the correct choice — the
     * alternative (dropping output) would corrupt printf statements
     * mid-line, which is worse than a brief stall. */
    while (next == s_tx_tail) { }

    s_tx_buf[s_tx_head] = (uint8_t)c;
    s_tx_head = next;

    /* Arm the TX interrupt. Idempotent — the ISR disables it again
     * when the buffer drains, so we can safely OR it in every time. */
    UART_REG->CR1 |= USART_CR1_TXEIE;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_flush(void) {
    /* First: wait for the ring buffer to drain (means the ISR has
     * pushed everything into DR). */
    while (s_tx_head != s_tx_tail) { }

    /* Second: wait for the shift register to finish. TXE would be
     * misleading here — it's set as soon as DR is empty, but the last
     * byte's stop bit may still be on the wire. TC is the honest
     * signal. */
    while (!(UART_REG->SR & USART_SR_TC)) { }
}

int uart_getc(void) {
    if (s_rx_tail == s_rx_head) return -1;
    uint8_t b = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1U) & UART_RX_BUF_MASK;
    return (int)b;
}

uint32_t uart_tx_pending(void)    { return (s_tx_head - s_tx_tail) & UART_TX_BUF_MASK; }
uint32_t uart_get_rx_drops(void) { return s_rx_drops; }
uint32_t uart_get_overruns(void) { return s_overruns; }

/*
 * USART2_IRQHandler — services both RX and TX.
 *
 * Read SR once at entry — it captures both RXNE and TXE as they were
 * the instant the ISR was entered, so we handle both cases without a
 * second read. Reading SR also arms the ORE-clear on the subsequent
 * DR read (see the RX-only driver for details).
 */
void USART2_IRQHandler(void) {
    uint32_t sr = UART_REG->SR;

    /* ---- RX path: a byte arrived -------------------------------- */
    if (sr & USART_SR_RXNE) {
        uint8_t b = (uint8_t)UART_REG->DR;   /* clears RXNE + ORE */

        if (sr & USART_SR_ORE) s_overruns++;

        uint16_t next = (s_rx_head + 1U) & UART_RX_BUF_MASK;
        if (next == s_rx_tail) {
            s_rx_drops++;
        } else {
            s_rx_buf[s_rx_head] = b;
            s_rx_head = next;
        }
    }

    /* ---- TX path: ready for the next byte ----------------------
     * The second condition (CR1 & TXEIE) is essential: TXE is set
     * whenever the transmitter is idle, but we only want to act on it
     * when we actually have data queued. Without this check the ISR
     * would fire continuously after every byte, burning 100% CPU. */
    if ((sr & USART_SR_TXE) && (UART_REG->CR1 & USART_CR1_TXEIE)) {
        if (s_tx_tail != s_tx_head) {
            /* Push the next byte. Writing DR clears TXE. */
            UART_REG->DR = s_tx_buf[s_tx_tail];
            s_tx_tail = (s_tx_tail + 1U) & UART_TX_BUF_MASK;
        } else {
            /* Buffer empty — disable the interrupt or we'd spin in
             * this ISR forever. Next uart_putc will re-enable it. */
            UART_REG->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
