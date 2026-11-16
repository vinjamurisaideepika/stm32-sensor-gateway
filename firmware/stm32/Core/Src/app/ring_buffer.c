/**
 * @file    ring_buffer.c
 * @brief   SPSC byte FIFO used between the UART ISR and the main loop.
 */
#include "app/ring_buffer.h"

#define RB_MASK (RING_BUFFER_SIZE - 1u)

void rb_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte)
{
    uint16_t next = (uint16_t)((rb->head + 1u) & RB_MASK);
    if (next == rb->tail) {
        return false; /* full: drop the byte */
    }
    rb->data[rb->head] = byte;
    rb->head = next;
    return true;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *byte)
{
    if (rb->tail == rb->head) {
        return false; /* empty */
    }
    *byte = rb->data[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1u) & RB_MASK);
    return true;
}
