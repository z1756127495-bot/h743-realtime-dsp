#include "ring_buffer.h"

void rb_init(ring_buffer_t *rb, uint8_t *storage, size_t size_pow2)
{
    rb->buf  = storage;
    rb->mask = size_pow2 - 1;
    rb->head = 0;
    rb->tail = 0;
}

size_t rb_used(const ring_buffer_t *rb)
{
    return rb->head - rb->tail;
}

size_t rb_free(const ring_buffer_t *rb)
{
    return (rb->mask + 1) - rb_used(rb);
}

int rb_write(ring_buffer_t *rb, const uint8_t *data, size_t len)
{
    size_t free_slots = rb_free(rb);
    if (len > free_slots) {
        len = free_slots;
    }
    for (size_t i = 0; i < len; i++) {
        rb->buf[(rb->head + i) & rb->mask] = data[i];
    }
    rb->head += len;
    return (int)len;
}

int rb_read(ring_buffer_t *rb, uint8_t *data, size_t maxlen)
{
    size_t used = rb_used(rb);
    if (maxlen > used) {
        maxlen = used;
    }
    for (size_t i = 0; i < maxlen; i++) {
        data[i] = rb->buf[(rb->tail + i) & rb->mask];
    }
    rb->tail += maxlen;
    return (int)maxlen;
}

