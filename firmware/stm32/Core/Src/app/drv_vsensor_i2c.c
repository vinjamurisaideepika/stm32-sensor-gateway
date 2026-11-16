/**
 * @file    drv_vsensor_i2c.c
 * @brief   I2C driver for the ESP32-emulated environment sensor.
 *
 * HAL_I2C_Mem_Read() performs the classic sensor transaction:
 *   START, addr+W, register, REPEATED START, addr+R, data..., NACK, STOP
 * Capture it with the logic analyser to see every one of those pieces.
 */
#include "app/drv_vsensor_i2c.h"
#include <stdio.h>

#define I2C_TIMEOUT_MS 20U
/* HAL wants the 7-bit address shifted left by one (R/W bit position). */
#define DEV_ADDR ((uint16_t)(VSENSOR_I2C_ADDR_7BIT << 1))

static I2C_HandleTypeDef *s_hi2c;

HAL_StatusTypeDef vsensor_i2c_init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;

    uint8_t who = 0;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c, DEV_ADDR, VSENSOR_REG_WHO_AM_I,
                                            I2C_MEMADD_SIZE_8BIT, &who, 1, I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        return st;
    }
    return (who == VSENSOR_WHO_AM_I_VAL) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef vsensor_i2c_read(vsensor_i2c_data_t *out)
{
    uint8_t buf[5];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c, DEV_ADDR, VSENSOR_REG_DATA,
                                            I2C_MEMADD_SIZE_8BIT, buf, sizeof buf,
                                            I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        return st;
    }
    out->temp_cdeg     = (int16_t)((buf[0] << 8) | buf[1]);
    out->humidity_cpct = (uint16_t)((buf[2] << 8) | buf[3]);
    out->counter       = buf[4];

    /* Plausibility check: reject physically impossible values. */
    if (out->temp_cdeg < -4000 || out->temp_cdeg > 12500 || out->humidity_cpct > 10000) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

void i2c_bus_scan(I2C_HandleTypeDef *hi2c)
{
    printf("I2C scan:");
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 1, 5) == HAL_OK) {
            printf(" 0x%02X", addr);
            found++;
        }
    }
    printf(found ? "\r\n" : " no devices found\r\n");
}
