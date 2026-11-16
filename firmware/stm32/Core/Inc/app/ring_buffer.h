/**
 * @file    ring_buffer.h
 * @brief   Small lock-free single-producer / single-consumer byte FIFO.
 *
 * The UART receive interrupt (producer) pushes bytes, the main loop
 * (consumer) pops them. Because only one side writes `head` and only the
 * other side writes `tail`, no interrupt locking is required.
 */
#ifndef APP_RING_BUFFER_H
#define APP_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 128u /* must be a power of two */

typedef struct {
    volatile uint8_t  data[RING_BUFFER_SIZE];
    volatile uint16_t head; /* written by producer (ISR)      */
    volatile uint16_t tail; /* written by consumer (main loop) */
} ring_buffer_t;

void rb_init(ring_buffer_t *rb);
bool rb_push(ring_buffer_t *rb, uint8_t byte); /* false if full  */
bool rb_pop(ring_buffer_t *rb, uint8_t *byte); /* false if empty */

#endif /* APP_RING_BUFFER_H */
