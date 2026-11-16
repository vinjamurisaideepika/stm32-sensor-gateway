/**
 * @file    uart_console.c
 * @brief   Interrupt-driven UART console (communication layer).
 */
#include "app/uart_console.h"
#include "app/ring_buffer.h"
#include <string.h>

static UART_HandleTypeDef *s_huart;
static ring_buffer_t       s_rx_rb;
static uint8_t             s_rx_byte;           /* 1-byte buffer for HAL IT */
static char                s_line[CONSOLE_LINE_MAX];
static size_t              s_line_len;

void uart_console_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    rb_init(&s_rx_rb);
    s_line_len = 0;
    /* Arm the receive interrupt for the first byte. */
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

void uart_console_rx_isr(UART_HandleTypeDef *huart)
{
    if (huart != s_huart) {
        return;
    }
    (void)rb_push(&s_rx_rb, s_rx_byte);       /* keep the ISR short   */
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1); /* re-arm for next byte */
}

void uart_console_error_isr(UART_HandleTypeDef *huart)
{
    if (huart != s_huart) {
        return;
    }
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
}

bool uart_console_get_line(char *line, size_t maxlen)
{
    uint8_t c;
    while (rb_pop(&s_rx_rb, &c)) {
        if (c == '\r' || c == '\n') {
            if (s_line_len == 0) {
                continue; /* ignore empty lines / CRLF pairs */
            }
            s_line[s_line_len] = '\0';
            strncpy(line, s_line, maxlen - 1);
            line[maxlen - 1] = '\0';
            s_line_len = 0;
            HAL_UART_Transmit(s_huart, (uint8_t *)"\r\n", 2, 10);
            return true;
        }
        if ((c == 0x08 || c == 0x7F) && s_line_len > 0) { /* backspace */
            s_line_len--;
            HAL_UART_Transmit(s_huart, (uint8_t *)"\b \b", 3, 10);
            continue;
        }
        if (c >= 0x20 && c < 0x7F && s_line_len < CONSOLE_LINE_MAX - 1) {
            s_line[s_line_len++] = (char)c;
            HAL_UART_Transmit(s_huart, &c, 1, 10); /* local echo */
        }
    }
    return false;
}

/* ------------------------------------------------------------------------
 * printf() retargeting. newlib's printf ends up calling _write(); the
 * STM32CubeIDE syscalls.c defines _write() as a weak function, so this
 * strong definition replaces it.
 * ------------------------------------------------------------------------ */
int _write(int file, char *ptr, int len)
{
    (void)file;
    if (s_huart == NULL) {
        return len;
    }
    HAL_UART_Transmit(s_huart, (uint8_t *)ptr, (uint16_t)len, 100);
    return len;
}
