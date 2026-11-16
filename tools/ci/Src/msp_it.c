/**
 * CI stand-in for stm32f4xx_hal_msp.c and stm32f4xx_it.c (CubeMX generates
 * both). The MSP functions route pins to peripherals and enable clocks;
 * the IRQ handlers forward interrupts into the HAL.
 */
#include "main.h"

extern ADC_HandleTypeDef  hadc1;
extern I2C_HandleTypeDef  hi2c1;
extern SPI_HandleTypeDef  hspi2;
extern TIM_HandleTypeDef  htim6;
extern UART_HandleTypeDef huart2;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin       = GPIO_PIN_2 | GPIO_PIN_3; /* PA2 TX, PA3 RX -> ST-LINK VCP */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &g);
        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin  = GPIO_PIN_0 | GPIO_PIN_1; /* PA0 = IN0 diode, PA1 = IN1 divider */
        g.Mode = GPIO_MODE_ANALOG;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin       = GPIO_PIN_8 | GPIO_PIN_9; /* PB8 SCL (D15), PB9 SDA (D14) */
        g.Mode      = GPIO_MODE_AF_OD;
        g.Pull      = GPIO_NOPULL;               /* external 4.7k pull-ups */
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &g);
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; /* SCK MISO MOSI */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *h)
{
    if (h->Instance == TIM6) {
        __HAL_RCC_TIM6_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
}

/* ------------------------------------------------------ IRQ handlers */
void NMI_Handler(void)        { while (1) { } }
void HardFault_Handler(void)  { while (1) { } }
void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }
void SysTick_Handler(void)    { HAL_IncTick(); }

void USART2_IRQHandler(void)    { HAL_UART_IRQHandler(&huart2); }
void TIM6_DAC_IRQHandler(void)  { HAL_TIM_IRQHandler(&htim6); }
void EXTI15_10_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(B1_Pin); }
