/*
 * stm32f407_regs.h — Register map for the STM32F407.
 *
 * Superset of every peripheral used across all projects so far.
 * When you add a new peripheral (I2C, SPI, DMA, ...), add its struct
 * HERE, in the template — then all existing and future projects
 * benefit, and header drift becomes impossible.
 */
#ifndef STM32F407_REGS_H
#define STM32F407_REGS_H

#include <stdint.h>

/* ================= Base addresses ================= */
#define PERIPH_BASE         0x40000000UL
#define APB1PERIPH_BASE     (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE     (PERIPH_BASE + 0x00020000UL)

#define RCC_BASE            (AHB1PERIPH_BASE + 0x3800UL)
#define GPIOA_BASE          (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE          (AHB1PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE          (AHB1PERIPH_BASE + 0x0800UL)
#define GPIOD_BASE          (AHB1PERIPH_BASE + 0x0C00UL)
#define USART2_BASE         (APB1PERIPH_BASE + 0x4400UL)
#define TIM4_BASE           (APB1PERIPH_BASE + 0x0800UL)
#define ADC1_BASE           (APB2PERIPH_BASE + 0x2000UL)
#define ADC123_COMMON_BASE  (APB2PERIPH_BASE + 0x2300UL)

/* ARM core System Control Space */
#define SCS_BASE            0xE000E000UL
#define SYSTICK_BASE        (SCS_BASE + 0x0010UL)
#define NVIC_BASE           0xE000E100UL

/* ================= Struct definitions ================= */

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_TypeDef;

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER;
    volatile uint32_t CNT, PSC, ARR, RCR, CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t BDTR, DCR, DMAR;
} TIM_TypeDef;

typedef struct {
    volatile uint32_t SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4;
    volatile uint32_t HTR, LTR, SQR1, SQR2, SQR3, JSQR;
    volatile uint32_t JDR1, JDR2, JDR3, JDR4, DR;
} ADC_TypeDef;

typedef struct {
    volatile uint32_t CSR, CCR, CDR;
} ADC_Common_TypeDef;

typedef struct {
    volatile uint32_t CSR, RVR, CVR, CALIB;
} SysTick_TypeDef;

typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR;
    volatile uint32_t AHB1RSTR, AHB2RSTR, AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR, APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR, AHB2ENR, AHB3ENR;
    uint32_t RESERVED2;
    volatile uint32_t APB1ENR, APB2ENR;
} RCC_TypeDef;

/* ================= Typed pointers ================= */
#define RCC            ((RCC_TypeDef *)RCC_BASE)
#define GPIOA          ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB          ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC          ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD          ((GPIO_TypeDef *)GPIOD_BASE)
#define USART2         ((USART_TypeDef *)USART2_BASE)
#define TIM4           ((TIM_TypeDef *)TIM4_BASE)
#define ADC1           ((ADC_TypeDef *)ADC1_BASE)
#define ADC123_COMMON  ((ADC_Common_TypeDef *)ADC123_COMMON_BASE)
#define SYSTICK        ((SysTick_TypeDef *)SYSTICK_BASE)

/* ================= NVIC ================= */
#define NVIC_ISER0     (*(volatile uint32_t *)(NVIC_BASE + 0x000))
#define NVIC_ISER1     (*(volatile uint32_t *)(NVIC_BASE + 0x004))
#define NVIC_ISER2     (*(volatile uint32_t *)(NVIC_BASE + 0x008))
#define NVIC_ICER0     (*(volatile uint32_t *)(NVIC_BASE + 0x080))
#define NVIC_ICER1     (*(volatile uint32_t *)(NVIC_BASE + 0x084))
#define NVIC_ICER2     (*(volatile uint32_t *)(NVIC_BASE + 0x088))
#define NVIC_ISPR0     (*(volatile uint32_t *)(NVIC_BASE + 0x100))
#define NVIC_ICPR0     (*(volatile uint32_t *)(NVIC_BASE + 0x180))
#define NVIC_IPR       ((volatile uint8_t  *)(NVIC_BASE + 0x300))

/* ================= RCC enables ================= */
#define RCC_AHB1ENR_GPIOAEN   (1U << 0)
#define RCC_AHB1ENR_GPIOBEN   (1U << 1)
#define RCC_AHB1ENR_GPIOCEN   (1U << 2)
#define RCC_AHB1ENR_GPIODEN   (1U << 3)
#define RCC_APB1ENR_TIM4EN    (1U << 2)
#define RCC_APB1ENR_USART2EN  (1U << 17)
#define RCC_APB2ENR_ADC1EN    (1U << 8)

/* ================= GPIO ================= */
#define GPIO_MODER_INPUT      0U
#define GPIO_MODER_OUTPUT     1U
#define GPIO_MODER_AF         2U
#define GPIO_MODER_ANALOG     3U

/* ================= USART ================= */
#define USART_SR_TXE          (1U << 7)
#define USART_SR_TC           (1U << 6)
#define USART_SR_RXNE         (1U << 5)
#define USART_SR_ORE          (1U << 3)
#define USART_SR_IDLE         (1U << 4)
#define USART_CR1_UE          (1U << 13)
#define USART_CR1_TXEIE       (1U << 7)
#define USART_CR1_RXNEIE      (1U << 5)
#define USART_CR1_TE          (1U << 3)
#define USART_CR1_RE          (1U << 2)
#define USART2_IRQN           38U

/* ================= TIM ================= */
#define TIM_CR1_CEN           (1U << 0)
#define TIM_CR1_ARPE          (1U << 7)
#define TIM_CCMR1_OC1PE       (1U << 3)
#define TIM_CCMR1_OC1M_PWM1   (6U << 4)
#define TIM_CCMR1_OC1M_PWM2   (7U << 4)
#define TIM_CCMR2_OC3PE       (1U << 3)
#define TIM_CCMR2_OC3M_PWM1   (6U << 4)
#define TIM_CCER_CC1E         (1U << 0)
#define TIM_CCER_CC2E         (1U << 4)
#define TIM_CCER_CC3E         (1U << 8)
#define TIM_CCER_CC4E         (1U << 12)
#define TIM_EGR_UG            (1U << 0)
#define TIM_DIER_UIE          (1U << 0)
#define TIM_SR_UIF            (1U << 0)
#define TIM4_IRQN             30U

/* ================= ADC ================= */
#define ADC_CR2_ADON          (1U << 0)
#define ADC_CR2_SWSTART       (1U << 30)
#define ADC_CR2_ADCAL         (1U << 31)
#define ADC_SR_EOC            (1U << 1)
#define ADC_CCR_ADCPRE_DIV2   (0U << 16)
#define ADC_CCR_ADCPRE_DIV4   (1U << 16)
#define ADC_CCR_ADCPRE_DIV6   (2U << 16)
#define ADC_CCR_ADCPRE_DIV8   (3U << 16)

/* ================= SysTick ================= */
#define SYSTICK_CSR_ENABLE    (1U << 0)
#define SYSTICK_CSR_TICKINT   (1U << 1)
#define SYSTICK_CSR_CLKSOURCE (1U << 2)

#endif /* STM32F407_REGS_H */
