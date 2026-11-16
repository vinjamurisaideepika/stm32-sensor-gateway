/**
 * @file    drv_vsensor_i2c.h
 * @brief   Driver for the "virtual environment sensor" on I2C.
 *
 * The sensor is emulated by the ESP32-S3 (see firmware/esp32s3_sensor_emulator).
 * It behaves like a typical real I2C sensor with a register map:
 *
 *   Reg  | Name      | Description
 *   -----+-----------+--------------------------------------------------
 *   0x00 | TEMP_H    | temperature, int16 big-endian, centi-degrees C
 *   0x01 | TEMP_L    |
 *   0x02 | HUM_H     | relative humidity, uint16 big-endian, centi-percent
 *   0x03 | HUM_L     |
 *   0x04 | COUNTER   | increments every new measurement (0..255)
 *   0x0F | WHO_AM_I  | always 0xA5
 */
#ifndef APP_DRV_VSENSOR_I2C_H
#define APP_DRV_VSENSOR_I2C_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define VSENSOR_I2C_ADDR_7BIT 0x48u
#define VSENSOR_REG_DATA      0x00u
#define VSENSOR_REG_WHO_AM_I  0x0Fu
#define VSENSOR_WHO_AM_I_VAL  0xA5u

typedef struct {
    int16_t  temp_cdeg;
    uint16_t humidity_cpct;
    uint8_t  counter;
} vsensor_i2c_data_t;

HAL_StatusTypeDef vsensor_i2c_init(I2C_HandleTypeDef *hi2c); /* checks WHO_AM_I */
HAL_StatusTypeDef vsensor_i2c_read(vsensor_i2c_data_t *out);

/** Scan the bus and print every address that ACKs (diagnostic command). */
void i2c_bus_scan(I2C_HandleTypeDef *hi2c);

#endif /* APP_DRV_VSENSOR_I2C_H */
