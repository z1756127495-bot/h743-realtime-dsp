#include "app_tasks.h"
#include "ring_buffer.h"
#include "dsp_fir.h"
#include "perf.h"
#include "icm20608.h"

#include <stdio.h>
#include <string.h>

#define RAW_RB_SIZE       2048u       /* power of two */
#define FILTER_TAPS        16u
/* --- static storage ------------------------------------------------------ */
static uint8_t     g_rb_storage[RAW_RB_SIZE];
static ring_buffer_t g_rb;
static QueueHandle_t g_frame_q;

static float g_coeffs[FILTER_TAPS];
static float g_hist[FILTER_TAPS];

/* --- metrics (read by the monitor task) ---------------------------------- */
typedef struct {
    volatile uint32_t fifo_samples;     /* produced */
    volatile uint32_t processed;        /* consumed by DSP */
    volatile uint32_t fifo_overflows;   /* read when FIFO was empty -> missed data */
    volatile uint32_t max_fifo_count;   /* watermark for sizing decisions */
} acq_metrics_t;
static volatile acq_metrics_t g_m;

/* --- task prototypes ----------------------------------------------------- */
static void acq_task(void *arg);
static void process_task(void *arg);
static void stream_task(void *arg);
static void monitor_task(void *arg);

/* Weak default: stream over UART via printf. Replace with USB CDC / Ethernet. */
__weak void stream_send(const dsp_frame_t *f)
{
    printf("SEQ=%lu acc_filt=%.4f acc_rms=%.4f gyro_rms=%.4f temp=%.1f\n",
           (unsigned long)f->seq, (double)f->acc_x_filt,
           (double)f->acc_rms, (double)f->gyro_rms, (double)f->temp_c);
}

void app_init(void)
{
    rb_init(&g_rb, g_rb_storage, RAW_RB_SIZE);
    g_frame_q = xQueueCreate(8, sizeof(dsp_frame_t));
    if (g_frame_q == NULL) {
        return;
    }

    /* A simple low-pass FIR, normalized so DC gain = 1 */
    for (uint32_t i = 0; i < FILTER_TAPS; i++) {
        g_coeffs[i] = 1.0f / FILTER_TAPS;
    }

    xTaskCreate(acq_task,    "acq",    1024, NULL, 5, NULL);
    xTaskCreate(process_task, "proc",  1024, NULL, 4, NULL);
    xTaskCreate(stream_task,  "stream", 768, NULL, 3, NULL);
    xTaskCreate(monitor_task, "mon",    512, NULL, 1, NULL);
}

/* --- producer: ICM20608 FIFO -> 12-byte frame -> SPSC ring buffer -------- */
static void acq_task(void *arg)
{
    uint8_t raw[IMU_SAMPLE_BYTES];
    (void)arg;

    for (;;) {
        uint16_t avail = 0;
        icm_fifo_available(&g_imu, &avail);

        if (avail >= IMU_SAMPLE_BYTES) {
            if (avail > g_m.max_fifo_count) {
                g_m.max_fifo_count = avail;
            }
            if (icm_fifo_read(&g_imu, raw, IMU_SAMPLE_BYTES) == HAL_OK) {
                /* only write a whole frame; never a partial (keeps alignment) */
                if (rb_free(&g_rb) >= IMU_SAMPLE_BYTES) {
                    rb_write(&g_rb, raw, IMU_SAMPLE_BYTES);
                    g_m.fifo_samples++;
                }
            }
        }
        /* In the DMA/ISR design this vTaskDelay is replaced by blocking on a
         * semaphore signalled by the FIFO data-ready interrupt. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* --- DSP: consume frames, filter, push processed frames ------------------ */
static void process_task(void *arg)
{
    uint8_t raw[IMU_SAMPLE_BYTES];
    fir_t fir;
    (void)arg;

    fir_init(&fir, g_hist, g_coeffs, FILTER_TAPS);

    for (;;) {
        if (rb_read(&g_rb, raw, IMU_SAMPLE_BYTES) == IMU_SAMPLE_BYTES) {
            int16_t a[6];
            for (int i = 0; i < 6; i++) {
                a[i] = (int16_t)((uint16_t)((uint16_t)raw[2 * i] << 8) | raw[2 * i + 1]);
            }

            float ax = a[0] / g_imu.accel_lsb;      /* g */
            float ax_f = fir_process(&fir, ax);

            dsp_frame_t f;
            f.acc_x_filt = ax_f;
            f.acc_rms    = ax_f * ax_f;             /* placeholder; accumulates over a window */
            f.gyro_rms   = (float)a[3] / g_imu.gyro_lsb;
            f.temp_c     = 0.0f;
            f.seq        = g_m.fifo_samples;

            if (xQueueSend(g_frame_q, &f, 0) == pdPASS) {
                g_m.processed++;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

/* --- streamer: push processed frames out -------------------------------- */
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

/* --- monitor: periodic telemetry + perf counters ------------------------- */
static void monitor_task(void *arg)
{
    uint32_t prev = g_m.fifo_samples;
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t produced = g_m.fifo_samples - prev;
        prev = g_m.fifo_samples;
        printf("[mon] fps=%lu overflows=%lu fifo_max=%u cpu_ticks=%u\n",
               (unsigned long)produced, (unsigned long)g_m.fifo_overflows,
               (unsigned)g_m.max_fifo_count, (unsigned)perf_ticks());
    }
}
