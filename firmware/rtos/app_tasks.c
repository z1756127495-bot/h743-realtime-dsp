#include "app_tasks.h"
#include "ring_buffer.h"
#include "dsp_fir.h"
#include "perf.h"
#include "sai_audio.h"

#include <stdio.h>
#include <math.h>

#define FILTER_TAPS 16u
#define RMS_WINDOW  256u       /* samples for the RMS/peak release window */

static float         g_coeffs[FILTER_TAPS];
static float         g_hist[FILTER_TAPS];
static QueueHandle_t g_frame_q;

typedef struct {
    volatile uint32_t processed;   /* frames pushed to the streamer */
    volatile uint32_t dropped;     /* frames dropped (queue full / rb overflow) */
    volatile uint32_t samples;     /* samples processed */
} acq_metrics_t;
static volatile acq_metrics_t g_m;

static void process_task(void *arg);
static void stream_task(void *arg);
static void monitor_task(void *arg);

/* Weak default: stream over UART via printf. Replace with USB CDC/Ethernet. */
__weak void stream_send(const dsp_frame_t *f)
{
    printf("SEQ=%lu filt=%.4f rms=%.4f peak=%.4f\n",
           (unsigned long)f->seq, (double)f->filt,
           (double)f->rms, (double)f->peak);
}

void app_init(void)
{
    g_frame_q = xQueueCreate(16, sizeof(dsp_frame_t));
    g_audio_sem = xSemaphoreCreateBinary();
    if (g_frame_q == NULL || g_audio_sem == NULL) {
        return;
    }

    for (uint32_t i = 0; i < FILTER_TAPS; i++) {
        g_coeffs[i] = 1.0f / FILTER_TAPS;   /* normalized low-pass, DC gain = 1 */
    }

    xTaskCreate(process_task, "proc", 1024, NULL, 4, NULL);
    xTaskCreate(stream_task,  "stream", 768, NULL, 3, NULL);
    xTaskCreate(monitor_task, "mon",   512, NULL, 1, NULL);

    audio_init(g_audio_sem);
    audio_start();
}

/* --- consumer of the SAI DMA ring buffer --------------------------------- */
static void process_task(void *arg)
{
    fir_t fir;
    uint8_t frame[AUDIO_FRAME_BYTES];
    ring_buffer_t *rb = audio_rb_get();
    (void)arg;

    fir_init(&fir, g_hist, g_coeffs, FILTER_TAPS);

    for (;;) {
        if (xSemaphoreTake(g_audio_sem, pdMS_TO_TICKS(50)) != pdPASS) {
            continue;   /* timeout: nothing new, just loop */
        }
        /* Drain every complete 16-bit stereo frame presently available. */
        while (rb_read(rb, frame, AUDIO_FRAME_BYTES) == AUDIO_FRAME_BYTES) {
            int16_t l = (int16_t)((uint16_t)frame[0] | ((uint16_t)frame[1] << 8));
            float x  = (float)l / 32768.0f;
            float xf = fir_process(&fir, x);

            dsp_frame_t f;
            f.filt = xf;
            f.peak = fabsf(xf);
            f.rms  = xf * xf;          /* placeholder; accumulate over RMS_WINDOW */
            f.seq  = g_m.samples++;

            if (xQueueSend(g_frame_q, &f, 0) == pdPASS) {
                g_m.processed++;
            } else {
                g_m.dropped++;
            }
        }
    }
}

/* --- streamer ------------------------------------------------------------ */
static void stream_task(void *arg)
{
    dsp_frame_t f;
    (void)arg;
    for (;;) {
        if (xQueueReceive(g_frame_q, &f, portMAX_DELAY) == pdPASS) {
            stream_send(&f);
        }
    }
}

/* --- monitor: throughput + perf counters --------------------------------- */
static void monitor_task(void *arg)
{
    uint32_t prev_samples = 0;
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t now = g_m.samples;
        uint32_t sps  = now - prev_samples;
        prev_samples = now;
        printf("[mon] sps=%lu processed=%lu dropped=%lu dma_rx=%u ticks=%u\n",
               (unsigned long)sps, (unsigned long)g_m.processed,
               (unsigned long)g_m.dropped, (unsigned)audio_rx_count(),
               (unsigned)perf_ticks());
    }
}

