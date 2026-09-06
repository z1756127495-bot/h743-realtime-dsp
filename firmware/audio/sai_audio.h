#ifndef SAI_AUDIO_H
#define SAI_AUDIO_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

/* ES8388 codec over SAI1 (I2S), DMA double buffer. The ISR stays minimal:
 * it only marks a buffer ready and signals the processor task. Cache
 * maintenance + DSP are done in task context (best practice). */
#define AUDIO_FS_HZ        48000u
#define AUDIO_BUF_BYTES    8192u   /* per DMA half-buffer (4096 x 16-bit words) */
#define AUDIO_FRAME_BYTES  4u      /* 16-bit stereo, L then R */

void audio_init(SemaphoreHandle_t notify_sem);
void audio_start(void);
void audio_stop(void);

/* index (0/1) of the most recently completed DMA half-buffer */
uint8_t  audio_ready_idx(void);
/* pointer to DMA half-buffer `idx` */
uint8_t *audio_dma_buf(uint8_t idx);
/* number of completed buffers (for telemetry) */
uint32_t audio_rx_count(void);

#endif /* SAI_AUDIO_H */

