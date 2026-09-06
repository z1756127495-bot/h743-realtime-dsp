#ifndef SIGNAL_SRC_H
#define SIGNAL_SRC_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "ring_buffer.h"

/* Deterministic, hardware-free sample generator. A low-priority producer task
 * writes one mono 16-bit sine sample into an SPSC ring buffer and signals the
 * DSP task. No codec / PLL / DMA -> guaranteed to run.
 * The measured `[mon] sps=` is the real DSP-pipeline throughput (a benchmark). */
#define SRC_SAMPLE_BYTES  2u

void signal_src_init(SemaphoreHandle_t notify_sem);
ring_buffer_t *signal_src_ring(void);

#endif /* SIGNAL_SRC_H */

