/**
 * @file    app.c
 * @brief   Application logic: scheduling, sensor health, output and commands.
 *
 * Architecture (three layers):
 *
 *   +-----------------------------------------------------------+
 *   | Application   app.c  (scheduler, health, commands)        |
 *   +-----------------------------------------------------------+
 *   | Drivers       drv_adc_sensors  drv_vsensor_i2c  drv_vsensor_spi |
 *   | Comms         uart_console (printf + command input)       |
 *   +-----------------------------------------------------------+
 *   | STM32 HAL + CubeMX init code (main.c)                     |
 *   +-----------------------------------------------------------+
 *
 * Interrupts used:
 *   TIM6 update (10 Hz)  -> sets a "tick" flag, the main loop does the work
 *   EXTI15_10 (B1, PC13) -> blue user button toggles TEXT/CSV output
 *   USART2 RX            -> pushes received bytes into a ring buffer
 * ISRs only set flags or copy a byte. All real work happens in app_loop().
 */
#include "app/app.h"
#include "app/drv_adc_sensors.h"
#include "app/drv_vsensor_i2c.h"
#include "app/drv_vsensor_spi.h"
#include "app/uart_console.h"
#include "main.h" /* CubeMX pin labels: LD2_Pin, B1_Pin, SPI2_CS_Pin ... */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Handles created by CubeMX in main.c */
extern ADC_HandleTypeDef  hadc1;
extern I2C_HandleTypeDef  hi2c1;
extern SPI_HandleTypeDef  hspi2;
extern TIM_HandleTypeDef  htim6;
extern UART_HandleTypeDef huart2;

#define FW_VERSION         "1.0.0"
#define TICK_HZ            10u    /* TIM6 frequency                      */
#define RETRY_PERIOD_TICKS 20u    /* retry an offline sensor every 2 s   */
#define BUTTON_DEBOUNCE_MS 200u

/* ---------------------------------------------------------------- types */
typedef enum { SENSOR_ONLINE, SENSOR_OFFLINE } sensor_state_t;

typedef struct {
    const char    *name;
    sensor_state_t state;
    uint32_t       ok_count;
    uint32_t       err_count;
    uint32_t       retry_in;   /* ticks until next retry when offline */
} sensor_health_t;

typedef enum { OUT_TEXT, OUT_CSV } output_format_t;

/* ------------------------------------------------------------- state */
static volatile uint32_t s_ticks;          /* incremented by TIM6 ISR  */
static volatile bool     s_button_pressed; /* set by EXTI ISR          */

static uint32_t        s_last_tick_handled;
static uint32_t        s_sample_period_ticks = 10; /* 1 s by default   */
static uint32_t        s_ticks_since_sample;
static bool            s_streaming = true;
static output_format_t s_format    = OUT_TEXT;

static sensor_health_t s_adc = { "adc", SENSOR_ONLINE, 0, 0, 0 };
static sensor_health_t s_i2c = { "i2c", SENSOR_OFFLINE, 0, 0, 0 };
static sensor_health_t s_spi = { "spi", SENSOR_OFFLINE, 0, 0, 0 };

static adc_readings_t     s_adc_data;
static vsensor_i2c_data_t s_i2c_data;
static vsensor_spi_data_t s_spi_data;

/* ---------------------------------------------------- small helpers */
/* Print a centi-value (e.g. 2534) as "25.34" without using float printf. */
static void print_centi(int32_t v)
{
    const char *sign = (v < 0) ? "-" : "";
    int32_t a = labs(v);
    printf("%s%ld.%02ld", sign, (long)(a / 100), (long)(a % 100));
}

static void health_update(sensor_health_t *h, HAL_StatusTypeDef st)
{
    if (st == HAL_OK) {
        if (h->state == SENSOR_OFFLINE) {
            printf("[info] %s sensor ONLINE\r\n", h->name);
        }
        h->state = SENSOR_ONLINE;
        h->ok_count++;
    } else {
        h->err_count++;
        if (h->state == SENSOR_ONLINE) {
            printf("[warn] %s sensor OFFLINE (status %d)\r\n", h->name, (int)st);
        }
        h->state    = SENSOR_OFFLINE;
        h->retry_in = RETRY_PERIOD_TICKS;
    }
}

/* Offline sensors are only retried every RETRY_PERIOD_TICKS so a missing
 * device does not slow down the whole loop with timeouts. */
static bool health_should_try(sensor_health_t *h, uint32_t elapsed)
{
    if (h->state == SENSOR_ONLINE) {
        return true;
    }
    if (h->retry_in > elapsed) {
        h->retry_in -= elapsed;
        return false;
    }
    return true;
}

/* Parse "23", "23.5" or "-4.25" into centi-degrees. Returns false on error. */
static bool parse_centi(const char *s, int32_t *out)
{
    int sign = 1;
    int32_t whole = 0, frac = 0, frac_digits = 0;
    if (*s == '-') { sign = -1; s++; }
    if (*s < '0' || *s > '9') return false;
    while (*s >= '0' && *s <= '9') whole = whole * 10 + (*s++ - '0');
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && frac_digits < 2) {
            frac = frac * 10 + (*s++ - '0');
            frac_digits++;
        }
        if (frac_digits == 1) frac *= 10;
    }
    *out = sign * (whole * 100 + frac);
    return true;
}

/* -------------------------------------------------------- sampling */
static void sample_all(uint32_t elapsed_ticks)
{
    health_update(&s_adc, adc_sensors_read(&s_adc_data));

    if (health_should_try(&s_i2c, elapsed_ticks)) {
        health_update(&s_i2c, vsensor_i2c_read(&s_i2c_data));
    }
    if (health_should_try(&s_spi, elapsed_ticks)) {
        health_update(&s_spi, vsensor_spi_read(&s_spi_data));
    }
}

static void print_csv_header(void)
{
    printf("time_ms,vdda_mv,mcu_c,diode_mv,diode_c,divider_mv,"
           "i2c_t_c,i2c_rh_pct,i2c_cnt,spi_seq,ax_mg,ay_mg,az_mg\r\n");
}

static void print_sample(void)
{
    if (s_format == OUT_CSV) {
        printf("%lu,%u,", (unsigned long)HAL_GetTick(), s_adc_data.vdda_mv);
        print_centi(s_adc_data.mcu_temp_cdeg);
        printf(",%u,", s_adc_data.diode_mv);
        print_centi(s_adc_data.diode_temp_cdeg);
        printf(",%u,", s_adc_data.divider_mv);
        if (s_i2c.state == SENSOR_ONLINE) {
            print_centi(s_i2c_data.temp_cdeg);
            printf(",");
            print_centi(s_i2c_data.humidity_cpct);
            printf(",%u,", s_i2c_data.counter);
        } else {
            printf(",,,");
        }
        if (s_spi.state == SENSOR_ONLINE) {
            printf("%u,%d,%d,%d\r\n", s_spi_data.seq,
                   s_spi_data.ax_mg, s_spi_data.ay_mg, s_spi_data.az_mg);
        } else {
            printf(",,,\r\n");
        }
        return;
    }

    /* Human-readable text */
    printf("[%7lu ms] ADC vdda=%umV mcu=", (unsigned long)HAL_GetTick(), s_adc_data.vdda_mv);
    print_centi(s_adc_data.mcu_temp_cdeg);
    printf("C diode=%umV/", s_adc_data.diode_mv);
    print_centi(s_adc_data.diode_temp_cdeg);
    printf("C div=%umV", s_adc_data.divider_mv);

    if (s_i2c.state == SENSOR_ONLINE) {
        printf(" | I2C T=");
        print_centi(s_i2c_data.temp_cdeg);
        printf("C RH=");
        print_centi(s_i2c_data.humidity_cpct);
        printf("%% #%u", s_i2c_data.counter);
    } else {
        printf(" | I2C offline");
    }

    if (s_spi.state == SENSOR_ONLINE) {
        printf(" | SPI a=(%d,%d,%d)mg #%u\r\n",
               s_spi_data.ax_mg, s_spi_data.ay_mg, s_spi_data.az_mg, s_spi_data.seq);
    } else {
        printf(" | SPI offline\r\n");
    }
}

/* -------------------------------------------------------- commands */
static void print_health(const sensor_health_t *h)
{
    printf("  %-4s %-8s ok=%lu err=%lu\r\n", h->name,
           h->state == SENSOR_ONLINE ? "ONLINE" : "OFFLINE",
           (unsigned long)h->ok_count, (unsigned long)h->err_count);
}

static void cmd_help(void)
{
    printf("Commands:\r\n"
           "  help            this list\r\n"
           "  status          sensor health and settings\r\n"
           "  scan            scan the I2C bus\r\n"
           "  rate <ms>       sample period, 100..10000 ms\r\n"
           "  cal <temp>      diode calibration, e.g. 'cal 22.5'\r\n"
           "  stream on|off   start/stop periodic output\r\n"
           "  csv | text      output format (button B1 toggles too)\r\n"
           "  read            take one sample now\r\n");
}

static void handle_command(char *line)
{
    char *cmd = strtok(line, " ");
    char *arg = strtok(NULL, " ");
    if (cmd == NULL) {
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "status") == 0) {
        printf("FW %s, uptime %lu ms, period %lu ms, streaming %s, format %s\r\n",
               FW_VERSION, (unsigned long)HAL_GetTick(),
               (unsigned long)(s_sample_period_ticks * 1000u / TICK_HZ),
               s_streaming ? "on" : "off", s_format == OUT_CSV ? "csv" : "text");
        print_health(&s_adc);
        print_health(&s_i2c);
        print_health(&s_spi);
    } else if (strcmp(cmd, "scan") == 0) {
        i2c_bus_scan(&hi2c1);
    } else if (strcmp(cmd, "rate") == 0 && arg != NULL) {
        long ms = strtol(arg, NULL, 10);
        if (ms >= 100 && ms <= 10000) {
            s_sample_period_ticks = (uint32_t)ms * TICK_HZ / 1000u;
            printf("period = %ld ms\r\n", ms);
        } else {
            printf("error: rate must be 100..10000 ms\r\n");
        }
    } else if (strcmp(cmd, "cal") == 0 && arg != NULL) {
        int32_t cdeg;
        if (parse_centi(arg, &cdeg)) {
            adc_sensors_calibrate_diode(cdeg);
            printf("diode calibrated to ");
            print_centi(cdeg);
            printf(" C\r\n");
        } else {
            printf("error: usage 'cal 22.5'\r\n");
        }
    } else if (strcmp(cmd, "stream") == 0 && arg != NULL) {
        s_streaming = (strcmp(arg, "on") == 0);
        printf("streaming %s\r\n", s_streaming ? "on" : "off");
    } else if (strcmp(cmd, "csv") == 0) {
        s_format = OUT_CSV;
        print_csv_header();
    } else if (strcmp(cmd, "text") == 0) {
        s_format = OUT_TEXT;
    } else if (strcmp(cmd, "read") == 0) {
        sample_all(RETRY_PERIOD_TICKS);
        print_sample();
    } else {
        printf("unknown command '%s' - type 'help'\r\n", cmd);
    }
}

/* ------------------------------------------------------ LED pattern */
static void update_led(uint32_t ticks)
{
    /* Healthy: 1 Hz blink. Any sensor offline: fast 5 Hz blink. */
    bool degraded = (s_i2c.state == SENSOR_OFFLINE) || (s_spi.state == SENSOR_OFFLINE) ||
                    (s_adc.state == SENSOR_OFFLINE);
    uint32_t half_period = degraded ? 1u : 5u; /* in 100 ms ticks */
    bool on = ((ticks / half_period) % 2u) == 0u;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* =========================================================== public */
void app_init(void)
{
    uart_console_init(&huart2);
    printf("\r\n=== STM32 Embedded Sensor Gateway v%s (NUCLEO-F446RE) ===\r\n", FW_VERSION);
    printf("SYSCLK = %lu MHz\r\n", (unsigned long)(HAL_RCC_GetSysClockFreq() / 1000000u));

    adc_sensors_init(&hadc1);
    vsensor_spi_init(&hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin);

    HAL_StatusTypeDef st = vsensor_i2c_init(&hi2c1);
    printf("I2C sensor @0x%02X: %s\r\n", VSENSOR_I2C_ADDR_7BIT,
           st == HAL_OK ? "found (WHO_AM_I ok)" : "NOT found");
    health_update(&s_i2c, st);

    vsensor_spi_data_t probe;
    health_update(&s_spi, vsensor_spi_read(&probe));

    cmd_help();
    HAL_TIM_Base_Start_IT(&htim6); /* start the 10 Hz scheduler tick */
}

void app_loop(void)
{
    char line[CONSOLE_LINE_MAX];
    if (uart_console_get_line(line, sizeof line)) {
        handle_command(line);
    }

    if (s_button_pressed) {
        s_button_pressed = false;
        s_format = (s_format == OUT_TEXT) ? OUT_CSV : OUT_TEXT;
        printf("[button] format -> %s\r\n", s_format == OUT_CSV ? "csv" : "text");
        if (s_format == OUT_CSV) {
            print_csv_header();
        }
    }

    uint32_t now = s_ticks; /* single 32-bit read is atomic on Cortex-M4 */
    uint32_t elapsed = now - s_last_tick_handled;
    if (elapsed == 0u) {
        return;
    }
    s_last_tick_handled = now;
    update_led(now);

    s_ticks_since_sample += elapsed;
    if (s_ticks_since_sample >= s_sample_period_ticks) {
        sample_all(s_ticks_since_sample);
        s_ticks_since_sample = 0;
        if (s_streaming) {
            print_sample();
        }
    }
}

/* ============================================ HAL interrupt callbacks
 * HAL defines these as weak functions; defining them here overrides them. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        s_ticks++;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_press_ms;
    if (GPIO_Pin == B1_Pin) {
        uint32_t now = HAL_GetTick();
        if (now - last_press_ms > BUTTON_DEBOUNCE_MS) { /* simple debounce */
            s_button_pressed = true;
            last_press_ms = now;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_console_rx_isr(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uart_console_error_isr(huart);
}
