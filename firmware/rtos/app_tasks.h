#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* One processed audio frame handed to the streamer. */
typedef struct {
    float    filt;      /* FIR-filtered mono sample (Left channel) */
    float    rms;       /* RMS over the release window */
    float    peak;      /* peak |sample| over the window */
    uint32_t seq;       /* sample counter (continuity check) */
} dsp_frame_t;

extern SemaphoreHandle_t g_audio_sem;  /* given by SAI RX ISR, taken by processor task */

void app_init(void);
void stream_send(const dsp_frame_t *f);   /* weak default -> printf(UART) */

#endif /* APP_TASKS_H */

