/**
 * @file    drv_vsensor_spi.h
 * @brief   Driver for the "virtual accelerometer" on SPI (emulated by ESP32-S3).
 *
 * Protocol: the STM32 (master) pulls CS low and clocks 9 bytes.
 *   MOSI: 0x01 (READ_FRAME command) followed by 8 x 0x00
 *   MISO: a 9-byte frame prepared in advance by the ESP32:
 *
 *   Byte | Content
 *   -----+-------------------------------------------
 *   0    | header, always 0xA5
 *   1    | sequence number (increments every frame)
 *   2-3  | accel X, int16 big-endian, milli-g
 *   4-5  | accel Y
 *   6-7  | accel Z
 *   8    | checksum = XOR of bytes 0..7
 */
#ifndef APP_DRV_VSENSOR_SPI_H
#define APP_DRV_VSENSOR_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define VSENSOR_SPI_FRAME_LEN 9u
#define VSENSOR_SPI_HEADER    0xA5u
#define VSENSOR_SPI_CMD_READ  0x01u

typedef struct {
    uint8_t seq;
    int16_t ax_mg, ay_mg, az_mg;
} vsensor_spi_data_t;

void              vsensor_spi_init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
HAL_StatusTypeDef vsensor_spi_read(vsensor_spi_data_t *out);

#endif /* APP_DRV_VSENSOR_SPI_H */
