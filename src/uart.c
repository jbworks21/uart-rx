/*
 * uart.c — USART2 driver for STM32F407, interrupt-driven RX.
 *
 * THE RING BUFFER
 *
 * Two indices into a shared buffer:
 *
 *     s_buf:  [ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ]
 *                  ^           ^
 *              s_tail       s_head
 *
 *     bytes present = [s_tail, s_head)   (wrapping around the array)
 *
 *   - s_head is written only by the ISR
 *   - s_tail is written only by main (via uart_getc)
 *   - Empty when s_head == s_tail
 *   - Full when (s_head + 1) & MASK == s_tail
 *
 * The buffer holds SIZE-1 bytes at most. One slot stays empty so we
 * can distinguish "empty" from "full" with just the two indices —
 * classic ring buffer trick. SIZE is a power of two so wrapping is a
 * mask instead of a modulo.
 *
 * WHY THIS IS LOCK-FREE
 *
 * Single producer (ISR), single consumer (main), and each variable
 * has exactly one writer. On a single-core Cortex-M:
 *
 *   - ISR writes s_buf[s_head] then advances s_head
 *   - Main reads s_buf[s_tail] then advances s_tail
 *   - Neither writer ever touches the other's variable or index
 *   - uint16_t loads/stores are single instructions (atomic)
 *   - volatile prevents the compiler from reordering or caching
 *
 * The worst outcome of any race is a *spurious drop* (the ISR thinks
 * the buffer is full when main was about to drain it). Never
 * corruption, never a torn byte, never an overwrite of a live entry.
 * That's the invariant worth memorizing.
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

/* Peripheral clock after reset: HSI, 16 MHz. Update if you enable PLL. */
#define UART_PCLK_HZ        16000000UL

/* ---- Ring buffer -------------------------------------------------
 * 64 bytes: enough for ~5.5 ms of continuous input at 115200 baud.
 * Humans type at ~10 cps (100 ms/byte), so typing never fills it.
 * Pasting a large blob will overflow and drops will climb — that's
 * the visible signal that the buffer is undersized for the workload.
 * Double to 128 for a more forgiving paste experience. */
#define UART_RX_BUF_SIZE    64U
#define UART_RX_BUF_MASK    (UART_RX_BUF_SIZE - 1U)

static volatile uint8_t  s_buf[UART_RX_BUF_SIZE];
static volatile uint16_t s_head;      /* written by ISR only */
static volatile uint16_t s_tail;      /* written by main only */
static volatile uint32_t s_drops;     /* buffer full during push */
static volatile uint32_t s_overruns;  /* hardware ORE fired */

int uart_init(uint32_t baud) {
    if (baud == 0) return -1;

    /* Reset the ring buffer state so a re-init starts clean. */
    s_head = s_tail = 0;
    s_drops = s_overruns = 0;

    /* Step 1: clocks */
    RCC->AHB1ENR |= UART_RCC_GPIOEN;
    RCC->APB1ENR |= UART_RCC_USARTEN;

    /* Step 2: pins to AF7 */
    UART_GPIO->MODER &= ~((3U << (UART_TX_PIN * 2)) | (3U << (UART_RX_PIN * 2)));
    UART_GPIO->MODER |=  ((2U << (UART_TX_PIN * 2)) | (2U << (UART_RX_PIN * 2)));
    UART_GPIO->AFR[0] &= ~((0xFU << (UART_TX_PIN * 4)) | (0xFU << (UART_RX_PIN * 4)));
    UART_GPIO->AFR[0] |=  ((uint32_t)UART_AF << (UART_TX_PIN * 4));
    UART_GPIO->AFR[0] |=  ((uint32_t)UART_AF << (UART_RX_PIN * 4));

    /* Step 3: baud rate. Same formula as the polling driver. */
    UART_REG->BRR = (UART_PCLK_HZ + baud / 2U) / baud;

    /* Step 4: enable TX, RX, the RX interrupt, and the peripheral.
     * RXNEIE = 1 means "raise the USART2 IRQ whenever a byte arrives."
     * This is the peripheral-side enable; the NVIC-side comes next. */
    UART_REG->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    /* Step 5: NVIC enable for USART2. This is the step polling drivers
     * never needed and the step people most often forget.
     *
     *   USART2_IRQn = 38 → 38 / 32 = 1 → NVIC_ISER1
     *                      38 % 32 = 6 → bit 6
     *   So: NVIC_ISER1 = (1 << 6)
     *
     * If DIER/CR1 has the flag set and this line is missing, the
     * peripheral will assert its IRQ line but the CPU will never see
     * it. Silent failure. */
    NVIC_ISER1 = (1U << (USART2_IRQN - 32U));

    return 0;
}

void uart_putc(char c) {
    while (!(UART_REG->SR & USART_SR_TXE)) { }
    UART_REG->DR = (uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

int uart_getc(void) {
    /* Empty check. Both indices are volatile; the compiler reloads
     * them on every call, so this sees whatever the ISR has done. */
    if (s_tail == s_head) return -1;

    uint8_t b = s_buf[s_tail];
    s_tail = (s_tail + 1U) & UART_RX_BUF_MASK;
    return (int)b;
}

uint32_t uart_get_drops(void)    { return s_drops; }
uint32_t uart_get_overruns(void) { return s_overruns; }

/*
 * USART2_IRQHandler — fires whenever RXNE or ORE becomes set.
 *
 * The template's startup.c declared this as a weak alias to
 * Default_Handler. This strong definition overrides it at link time,
 * and the vector table's slot 54 (16 + 38) now points here. Verified
 * with:  llvm-nm firmware.elf | grep USART2_IRQHandler
 *       → should print a `T` at a distinct address, not a `W`.
 *
 * WHY READ SR ONCE, THEN DR?
 *
 *   - Reading SR captures the state at entry into the ISR.
 *   - Reading DR clears RXNE, AND clears ORE *because* SR was read
 *     immediately before. That SR-then-DR sequence is the documented
 *     way to clear ORE on STM32F4.
 *   - Reading DR twice would push the same byte twice. Reading it
 *     never would leave the flag set and re-trigger immediately.
 *   - The `if (sr & ORE)` check happens AFTER the DR read, so the
 *     valid byte still gets pushed before we account for the loss.
 *     If we returned early on ORE we'd lose the byte that was
 *     legitimately buffered.
 *
 * WHY IT'S SHORT
 *
 * Every instruction in this function delays every other interrupt in
 * the system and delays the main loop's return to user code. The ISR
 * does exactly three things: grab the byte, push it if there's room,
 * bump a counter. Anything else (parsing, formatting, printing)
 * belongs in main, driven by uart_getc().
 */
void USART2_IRQHandler(void) {
    uint32_t sr = UART_REG->SR;

    if (sr & USART_SR_RXNE) {
        /* Single DR read clears both RXNE and (because SR was read
         * above) ORE. The byte is valid — push it below. */
        uint8_t b = (uint8_t)UART_REG->DR;

        if (sr & USART_SR_ORE) {
            /* Hardware lost one or more bytes before this one arrived.
             * Track it so the demo can surface it. Should stay at zero
             * for this project — ORE only fires if the ISR is starved,
             * and USART2 is the only enabled interrupt. */
            s_overruns++;
        }

        uint16_t next = (s_head + 1U) & UART_RX_BUF_MASK;
        if (next == s_tail) {
            /* Buffer full — main isn't draining fast enough. Drop the
             * byte and count it. This is the software backpressure
             * path; the alternative (block in the ISR until space
             * appears) would deadlock because main can't run while the
             * ISR is spinning. Drop-and-count is the right choice. */
            s_drops++;
        } else {
            s_buf[s_head] = b;
            s_head = next;
        }
    }
}
