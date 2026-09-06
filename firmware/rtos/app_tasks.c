#include "app_tasks.h"
#include "ring_buffer.h"
#include "dsp_fir.h"
#include "perf.h"
#include "signal_src.h"

#include <stdio.h>
#include <math.h>

#define FILTER_TAPS   16u
#define REPORT_EVERY  2048u     /* samples per window before a summary is made */
#define SAMPLE_BYTES  SRC_SAMPLE_BYTES

static float g_coeffs[FILTER_TAPS];
static float g_hist[FILTER_TAPS];
SemaphoreHandle_t g_audio_sem;              /* extern'd in app_tasks.h */

typedef struct {
    volatile uint32_t processed;   /* windows summarised */
    volatile uint32_t samples;     /* samples processed */
} acq_metrics_t;
static volatile acq_metrics_t g_m;

/* latest summary, published by the DSP task and printed once/sec by the monitor */
static volatile float    g_filt, g_rms, g_peak;
static volatile uint32_t g_seq;

static void process_task(void *arg);
static void monitor_task(void *arg);

void app_init(void)
{
    g_audio_sem = xSemaphoreCreateBinary();
    printf("[app] sem=%s\n", g_audio_sem ? "ok" : "FAIL");
    if (g_audio_sem == NULL) {
        return;
    }
    for (uint32_t i = 0; i < FILTER_TAPS; i++) {
        g_coeffs[i] = 1.0f / FILTER_TAPS;   /* normalized low-pass */
    }

    printf("[app] proc=%s\n",
           xTaskCreate(process_task, "proc", 512, NULL, 4, NULL) == pdPASS ? "ok" : "FAIL");
    printf("[app] mon=%s\n",
           xTaskCreate(monitor_task, "mon", 256, NULL, 1, NULL) == pdPASS ? "ok" : "FAIL");

    signal_src_init(g_audio_sem);
}

static void process_task(void *arg)
{
    fir_t fir;
    uint8_t samp[SAMPLE_BYTES];
    ring_buffer_t *rb = signal_src_ring();
    (void)arg;

    float sum_sq = 0.0f, peak = 0.0f, last_filt = 0.0f;
    uint32_t window = 0;
    fir_init(&fir, g_hist, g_coeffs, FILTER_TAPS);
    printf("[task] proc up\n");

    for (;;) {
        if (xSemaphoreTake(g_audio_sem, pdMS_TO_TICKS(50)) != pdPASS) {
            continue;
        }
        while (rb_read(rb, samp, SAMPLE_BYTES) == SAMPLE_BYTES) {
            int16_t v = (int16_t)((uint16_t)samp[0] | ((uint16_t)samp[1] << 8));
            float x  = (float)v / 32768.0f;
            float xf = fir_process(&fir, x);

            last_filt = xf;
            sum_sq   += xf * xf;
            if (fabsf(xf) > peak) peak = fabsf(xf);
            g_m.samples++;

            if (++window >= REPORT_EVERY) {
                g_filt = last_filt;
                g_rms  = sqrtf(sum_sq / (float)window);
                g_peak = peak;
                g_seq  = g_m.samples;
                g_m.processed++;
                sum_sq = 0.0f; peak = 0.0f; window = 0;
            }
        }
    }
}

static void monitor_task(void *arg)
{
    uint32_t prev = 0;
    (void)arg;
    printf("[task] mon up\n");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t sps = g_m.samples - prev;
        prev = g_m.samples;
        printf("[mon] sps=%lu processed=%lu filt=%.4f rms=%.4f peak=%.4f\n",
               (unsigned long)sps, (unsigned long)g_m.processed,
               (double)g_filt, (double)g_rms, (double)g_peak);
    }
}

