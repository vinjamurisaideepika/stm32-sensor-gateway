/**
 * @file    uart_console.h
 * @brief   Communication layer: interrupt-driven UART console over the
 *          ST-LINK virtual COM port (USART2, 115200 8N1).
 *
 * - printf() is redirected to USART2 (see _write() in uart_console.c).
 * - Received characters are collected by the RX interrupt into a ring
 *   buffer; uart_console_get_line() assembles them into command lines.
 */
#ifndef APP_UART_CONSOLE_H
#define APP_UART_CONSOLE_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stddef.h>

#define CONSOLE_LINE_MAX 48u

void uart_console_init(UART_HandleTypeDef *huart);

/** Returns true when a full line (terminated by Enter) is available. */
bool uart_console_get_line(char *line, size_t maxlen);

/** Call from HAL_UART_RxCpltCallback(). */
void uart_console_rx_isr(UART_HandleTypeDef *huart);

/** Call from HAL_UART_ErrorCallback() (e.g. overrun) to re-arm reception. */
void uart_console_error_isr(UART_HandleTypeDef *huart);

#endif /* APP_UART_CONSOLE_H */
