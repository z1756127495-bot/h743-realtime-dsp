#ifndef SAI_AUDIO_H
#define SAI_AUDIO_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "ring_buffer.h"

/* Audio capture: ES8388 codec over SAI1 (I2S) with DMA double buffer.
 * 48 kHz, 16-bit, stereo => 192 KB/s raw audio stream. */
#define AUDIO_FS_HZ        48000u
#define AUDIO_BUF_BYTES    8192u   /* per DMA half-buffer (4096 x 16-bit words) */
#define AUDIO_FRAME_BYTES  4u      /* 16-bit stereo, L then R */

void audio_init(SemaphoreHandle_t notify_sem);
void audio_start(void);
void audio_stop(void);

/* SPSC ring buffer: producer = SAI RX DMA ISR, consumer = FreeRTOS task */
ring_buffer_t *audio_rb_get(void);

/* number of completed DMA buffers (for the monitor task) */
uint32_t audio_rx_count(void);

#endif /* SAI_AUDIO_H */
