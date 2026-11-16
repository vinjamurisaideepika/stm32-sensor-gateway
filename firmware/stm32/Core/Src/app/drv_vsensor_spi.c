/**
 * @file    drv_vsensor_spi.c
 * @brief   SPI driver for the ESP32-emulated accelerometer.
 *
 * Chip select is driven manually as a GPIO ("software NSS"), which is how
 * most real projects do it: it lets you share one SPI bus between devices.
 */
#include "app/drv_vsensor_spi.h"

#define SPI_TIMEOUT_MS 10U

static SPI_HandleTypeDef *s_hspi;
static GPIO_TypeDef      *s_cs_port;
static uint16_t           s_cs_pin;

static inline void cs_low(void)  { HAL_GPIO_WritePin(s_cs_port, s_cs_pin, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(s_cs_port, s_cs_pin, GPIO_PIN_SET); }

void vsensor_spi_init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    s_hspi    = hspi;
    s_cs_port = cs_port;
    s_cs_pin  = cs_pin;
    cs_high(); /* idle: device not selected */
}

HAL_StatusTypeDef vsensor_spi_read(vsensor_spi_data_t *out)
{
    uint8_t tx[VSENSOR_SPI_FRAME_LEN] = { VSENSOR_SPI_CMD_READ };
    uint8_t rx[VSENSOR_SPI_FRAME_LEN] = { 0 };

    cs_low();
    /* Give the ESP32 slave a moment to notice CS before the first clock. */
    for (volatile int i = 0; i < 200; i++) { }
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(s_hspi, tx, rx, sizeof tx, SPI_TIMEOUT_MS);
    cs_high();
    if (st != HAL_OK) {
        return st;
    }

    /* Validate the frame before trusting any of it. */
    if (rx[0] != VSENSOR_SPI_HEADER) {
        return HAL_ERROR;
    }
    uint8_t xor_sum = 0;
    for (uint32_t i = 0; i < VSENSOR_SPI_FRAME_LEN - 1u; i++) {
        xor_sum ^= rx[i];
    }
    if (xor_sum != rx[VSENSOR_SPI_FRAME_LEN - 1u]) {
        return HAL_ERROR;
    }

    out->seq   = rx[1];
    out->ax_mg = (int16_t)((rx[2] << 8) | rx[3]);
    out->ay_mg = (int16_t)((rx[4] << 8) | rx[5]);
    out->az_mg = (int16_t)((rx[6] << 8) | rx[7]);
    return HAL_OK;
}
