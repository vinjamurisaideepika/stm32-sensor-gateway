/**
 * @file    drv_adc_sensors.c
 * @brief   ADC1 driver for the STM32F446RE.
 *
 * Channels used (all on ADC1):
 *   IN0  (PA0 / Arduino A0) : diode forward voltage  -> temperature
 *   IN1  (PA1 / Arduino A1) : resistor voltage divider
 *   VREFINT                 : internal 1.21 V reference -> real VDDA
 *   TEMPSENSOR              : internal die temperature sensor
 *
 * The ADC is configured in CubeMX for single conversion (no scan). We select
 * one channel, convert, read, and move on. Simple and easy to debug.
 */
#include "app/drv_adc_sensors.h"

/* ---- Factory calibration values (STM32F446 reference manual / datasheet) */
#define GW_VREFINT_CAL ((const uint16_t *)0x1FFF7A2AU) /* raw @ VDDA=3.3V  */
#define GW_TS_CAL1     ((const uint16_t *)0x1FFF7A2CU) /* raw @ 30 C       */
#define GW_TS_CAL2     ((const uint16_t *)0x1FFF7A2EU) /* raw @ 110 C      */
#define CAL_VDDA_MV      3300
#define GW_TS_CAL1_TEMP_C   30
#define GW_TS_CAL2_TEMP_C   110

/* A silicon diode's forward voltage falls ~2 mV for every +1 C. */
#define DIODE_TEMPCO_UV_PER_C 2000

#define ADC_FULL_SCALE 4095U
#define ADC_TIMEOUT_MS 10U
#define OVERSAMPLE     8U /* average 8 samples to reduce noise */

static ADC_HandleTypeDef *s_hadc;

/* Diode calibration point: at s_diode_ref_cdeg the diode read s_diode_ref_mv */
static int32_t s_diode_ref_mv   = -1; /* -1 = not yet calibrated */
static int32_t s_diode_ref_cdeg = 2500;

/* Convert one channel and return the averaged raw value (0..4095). */
static HAL_StatusTypeDef read_channel(uint32_t channel, uint32_t sample_time,
                                      uint16_t *raw_out)
{
    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = channel;
    cfg.Rank         = 1;
    cfg.SamplingTime = sample_time;
    if (HAL_ADC_ConfigChannel(s_hadc, &cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    uint32_t sum = 0;
    for (uint32_t i = 0; i < OVERSAMPLE; i++) {
        if (HAL_ADC_Start(s_hadc) != HAL_OK) {
            return HAL_ERROR;
        }
        if (HAL_ADC_PollForConversion(s_hadc, ADC_TIMEOUT_MS) != HAL_OK) {
            HAL_ADC_Stop(s_hadc);
            return HAL_TIMEOUT;
        }
        sum += HAL_ADC_GetValue(s_hadc);
        HAL_ADC_Stop(s_hadc);
    }
    *raw_out = (uint16_t)(sum / OVERSAMPLE);
    return HAL_OK;
}

static uint16_t raw_to_mv(uint16_t raw, uint16_t vdda_mv)
{
    return (uint16_t)(((uint32_t)raw * vdda_mv) / ADC_FULL_SCALE);
}

HAL_StatusTypeDef adc_sensors_init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
    s_diode_ref_mv = -1;
    return (hadc != NULL) ? HAL_OK : HAL_ERROR;
}

void adc_sensors_calibrate_diode(int32_t room_temp_cdeg)
{
    adc_readings_t r;
    if (adc_sensors_read(&r) == HAL_OK) {
        s_diode_ref_mv   = r.diode_mv;
        s_diode_ref_cdeg = room_temp_cdeg;
    }
}

HAL_StatusTypeDef adc_sensors_read(adc_readings_t *out)
{
    uint16_t raw_vref, raw_ts, raw_diode, raw_div;

    /* Internal channels need a long sampling time (>= 10 us). */
    if (read_channel(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_480CYCLES, &raw_vref) != HAL_OK ||
        read_channel(ADC_CHANNEL_TEMPSENSOR, ADC_SAMPLETIME_480CYCLES, &raw_ts) != HAL_OK ||
        read_channel(ADC_CHANNEL_0, ADC_SAMPLETIME_144CYCLES, &raw_diode) != HAL_OK ||
        read_channel(ADC_CHANNEL_1, ADC_SAMPLETIME_144CYCLES, &raw_div) != HAL_OK) {
        return HAL_ERROR;
    }
    if (raw_vref == 0U) {
        return HAL_ERROR;
    }

    /* 1) Real VDDA: VREFINT is a fixed voltage, so if VDDA sags the raw
     *    value rises. VDDA = 3.3 V * VREFINT_CAL / raw_vref               */
    out->vdda_mv = (uint16_t)((CAL_VDDA_MV * (uint32_t)(*GW_VREFINT_CAL)) / raw_vref);

    /* 2) Internal temperature: linear interpolation between the two
     *    factory calibration points. Rescale raw to the 3.3 V calibration. */
    int32_t ts_scaled = (int32_t)(((uint32_t)raw_ts * out->vdda_mv) / CAL_VDDA_MV);
    int32_t cal1 = (int32_t)(*GW_TS_CAL1);
    int32_t cal2 = (int32_t)(*GW_TS_CAL2);
    out->mcu_temp_cdeg = GW_TS_CAL1_TEMP_C * 100 +
        ((ts_scaled - cal1) * (GW_TS_CAL2_TEMP_C - GW_TS_CAL1_TEMP_C) * 100) / (cal2 - cal1);

    /* 3) Diode and divider voltages */
    out->diode_mv   = raw_to_mv(raw_diode, out->vdda_mv);
    out->divider_mv = raw_to_mv(raw_div, out->vdda_mv);

    /* 4) Diode temperature: T = T_ref + (V_ref - V) / 2 mV per C.
     *    Until the user calibrates, assume the first reading is 25.00 C. */
    if (s_diode_ref_mv < 0) {
        s_diode_ref_mv = out->diode_mv;
    }
    int32_t delta_uv = (s_diode_ref_mv - (int32_t)out->diode_mv) * 1000;
    out->diode_temp_cdeg = s_diode_ref_cdeg + (delta_uv * 100) / DIODE_TEMPCO_UV_PER_C;

    return HAL_OK;
}
