#include "signal_src.h"
#include "task.h"

#include <math.h>
#include <stdint.h>

static ring_buffer_t     g_rb;
static uint8_t           g_rb_storage[2048u];      /* power of two; 1024 samples */
static SemaphoreHandle_t g_sem;

static void src_task(void *arg)
{
    double phase = 0.0;
    const int batch = 48;            /* samples per 1ms tick -> ~48 kHz nominal */
    (void)arg;
    for (;;) {
        for (int k = 0; k < batch; k++) {
            int16_t samp = (int16_t)(sin(phase) * 20000.0);
            phase += 0.05;
            uint8_t b[2] = { (uint8_t)(samp & 0xff), (uint8_t)(((uint16_t)samp >> 8) & 0xff) };
            if (rb_free(&g_rb) >= SRC_SAMPLE_BYTES) {
                rb_write(&g_rb, b, SRC_SAMPLE_BYTES);
            }
        }
        if (g_sem) {
            xSemaphoreGive(g_sem);     /* wake the DSP task */
        }
        vTaskDelay(pdMS_TO_TICKS(1));  /* pace: gives the monitor task CPU time */
    }
}

void signal_src_init(SemaphoreHandle_t notify_sem)
{
    g_sem = notify_sem;
    rb_init(&g_rb, g_rb_storage, sizeof(g_rb_storage));
    xTaskCreate(src_task, "src", 256, NULL, 3, NULL);  /* lower priority than DSP */
}

ring_buffer_t *signal_src_ring(void) { return &g_rb; }
