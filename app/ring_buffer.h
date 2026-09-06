#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>

/*
 * Single-producer / single-consumer (SPSC) ring buffer over a power-of-two
 * buffer. In the firmware this runs between the DMA/ISR producer and the
 * FreeRTOS processing task (the consumer). The lock-free contract relies on
 * one writer and one reader only.
 */
typedef struct {
    uint8_t        *buf;   /* storage, length must be a power of two */
    size_t          mask;  /* (length - 1) */
    volatile size_t head;  /* written by the producer */
    volatile size_t tail;  /* read by the consumer */
} ring_buffer_t;

void   rb_init(ring_buffer_t *rb, uint8_t *storage, size_t size_pow2);
size_t rb_used(const ring_buffer_t *rb);
size_t rb_free(const ring_buffer_t *rb);
int    rb_write(ring_buffer_t *rb, const uint8_t *data, size_t len);
int    rb_read(ring_buffer_t *rb, uint8_t *data, size_t maxlen);

#endif /* RING_BUFFER_H */

