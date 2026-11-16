/**
 * @file    drv_adc_sensors.h
 * @brief   ADC driver: internal temperature sensor, VREFINT, a diode used as
 *          a temperature sensor (PA0) and a resistor voltage divider (PA1).
 *
 * All results are fixed-point integers (no floats needed in printf):
 *   - voltages in millivolts (mV)
 *   - temperatures in centi-degrees Celsius (2534 = 25.34 C)
 */
#ifndef APP_DRV_ADC_SENSORS_H
#define APP_DRV_ADC_SENSORS_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
    uint16_t vdda_mv;         /* real supply voltage, measured via VREFINT   */
    int32_t  mcu_temp_cdeg;   /* internal die temperature sensor             */
    uint16_t diode_mv;        /* forward voltage of the 1N4148 on PA0        */
    int32_t  diode_temp_cdeg; /* temperature estimated from diode_mv         */
    uint16_t divider_mv;      /* voltage-divider output on PA1               */
} adc_readings_t;

HAL_StatusTypeDef adc_sensors_init(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef adc_sensors_read(adc_readings_t *out);

/** Calibrate the diode: "right now the room is at temp_cdeg". */
void adc_sensors_calibrate_diode(int32_t room_temp_cdeg);

#endif /* APP_DRV_ADC_SENSORS_H */
