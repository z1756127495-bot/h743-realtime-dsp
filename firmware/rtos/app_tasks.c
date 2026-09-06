#include "app_tasks.h"
#include "ring_buffer.h"
#include "dsp_fir.h"
#include "perf.h"
#include "sai_audio.h"

#include <stdio.h>
#include <math.h>

#define FILTER_TAPS 16u
#define REPORT_EVERY 2048u     /* samples per RMS/peak summary (≈23 lines/s @48k) */

static float         g_coeffs[FILTER_TAPS];
static float         g_hist[FILTER_TAPS];
static QueueHandle_t g_frame_q;
SemaphoreHandle_t    g_audio_sem;    /* defined here, extern'd in app_tasks.h */

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
    printf("[app] queue=%s\n", g_frame_q ? "ok" : "FAIL");
    g_audio_sem = xSemaphoreCreateBinary();
    printf("[app] sem=%s\n", g_audio_sem ? "ok" : "FAIL");
    if (g_frame_q == NULL || g_audio_sem == NULL) {
        return;
    }

    for (uint32_t i = 0; i < FILTER_TAPS; i++) {
        g_coeffs[i] = 1.0f / FILTER_TAPS;   /* normalized low-pass, DC gain = 1 */
    }

    printf("[app] proc=%s\n",
           xTaskCreate(process_task, "proc", 512, NULL, 4, NULL) == pdPASS ? "ok" : "FAIL");
    printf("[app] stream=%s\n",
           xTaskCreate(stream_task, "stream", 384, NULL, 3, NULL) == pdPASS ? "ok" : "FAIL");
    printf("[app] mon=%s\n",
           xTaskCreate(monitor_task, "mon", 256, NULL, 1, NULL) == pdPASS ? "ok" : "FAIL");

    printf("[app] audio init\n");
    audio_init(g_audio_sem);
    printf("[app] audio start\n");
    audio_start();
}

/* --- consumer of the SAI DMA ring buffer --------------------------------- */
static void process_task(void *arg)
{
    fir_t fir;
    uint8_t frame[AUDIO_FRAME_BYTES];
    ring_buffer_t *rb = audio_rb_get();
    (void)arg;

    float sum_sq = 0.0f;         /* window accumulator for RMS */
    float peak   = 0.0f;         /* window peak |sample| */
    uint32_t window = 0;         /* samples in current window */
    float last_filt = 0.0f;

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

            last_filt = xf;
            sum_sq   += xf * xf;
            if (fabsf(xf) > peak) peak = fabsf(xf);
            g_m.samples++;

            /* emit one readable summary per window instead of per sample */
            if (++window >= REPORT_EVERY) {
                dsp_frame_t f;
                f.filt = last_filt;
                f.rms  = sqrtf(sum_sq / (float)window);
                f.peak = peak;
                f.seq  = g_m.samples;

                if (xQueueSend(g_frame_q, &f, 0) == pdPASS) {
                    g_m.processed++;
                } else {
                    g_m.dropped++;
                }
                sum_sq = 0.0f;
                peak   = 0.0f;
                window = 0;
            }
        }
    }
}

/* --- streamer ------------------------------------------------------------ */
static void stream_task(void *arg)
{
    dsp_frame_t f;
    (void)arg;
    printf("[task] stream up\n");
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
    printf("[task] mon up\n");
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
