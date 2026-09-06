#include "sai_audio.h"
#include "perf.h"

#include "BSP/ES8388/es8388.h"
#include "BSP/SAI/sai.h"
#include "MALLOC/malloc.h"

/* DMA buffers live in AXI SRAM (SRAMIN), allocated by the ALIENTEK heap so the
 * SAI DMA can reach them. DTCM (0x2000_0000) is NOT DMA-accessible. */
static uint8_t *g_buf0;
static uint8_t *g_buf1;

static ring_buffer_t     g_rb;
static uint8_t          *g_rb_storage;
static SemaphoreHandle_t g_sem;
static volatile uint32_t g_rx_count;
static volatile uint32_t g_dropped;

static void audio_rx_cb(void);

void audio_init(SemaphoreHandle_t notify_sem)
{
    g_sem = notify_sem;

    /* allocate from SRAMIN (AXI SRAM), 32-byte aligned for line maintenance */
    g_buf0 = (uint8_t *)(((uint32_t)mymalloc(SRAMIN, AUDIO_BUF_BYTES + 32) + 31u) & ~31u);
    g_buf1 = (uint8_t *)(((uint32_t)mymalloc(SRAMIN, AUDIO_BUF_BYTES + 32) + 31u) & ~31u);
    g_rb_storage = (uint8_t *)mymalloc(SRAMIN, 16384u);   /* SPSC ring, keeps .bss small */
    rb_init(&g_rb, g_rb_storage, 16384u);

    /* --- ES8388 codec (I2C control, addr 0x10) ------------------------ */
    es8388_init();
    es8388_adda_cfg(0, 1);       /* DAC off, ADC on  */
    es8388_input_cfg(0);         /* MIC input channel */
    es8388_mic_gain(8);
    es8388_sai_cfg(0, 3);        /* codec SAI format = standard, 16-bit data */

    /* --- SAI1 (I2S) master/slave pair, 16-bit -------------------------- */
    sai1_saia_init(0, 1, 4);     /* Block A / master, 16-bit */
    sai1_saib_init(3, 1, 4);     /* Block B / slave receiver, 16-bit */
    sai1_samplerate_set(AUDIO_FS_HZ);

    /* --- DMA receive, double buffered (buf0 <-> buf1) ------------------ */
    sai1_rx_dma_init(g_buf0, g_buf1, AUDIO_BUF_BYTES / 2, 1);  /* 16-bit words */
    sai1_rx_callback = audio_rx_cb;

    printf("[app] dmabuf0=%p dmabuf1=%p\n", (void *)g_buf0, (void *)g_buf1);

    /* FreeRTOS ISR-safe API (xSemaphoreGiveFromISR) requires the SAI DMA IRQs
     * at a numeric priority >= configMAX_SYSCALL_INTERRUPT_PRIORITY (=5 here).
     * ALIENTEK's sai.c leaves them at 0/1, which is invalid -> assert. */
    HAL_NVIC_SetPriority(SAI1_TX_DMASx_IRQ, 6, 0);
    HAL_NVIC_SetPriority(SAI1_RX_DMASx_IRQ, 6, 0);
}

void audio_start(void) { sai1_rec_start(); }
void audio_stop(void)  { sai1_rec_stop(); }

ring_buffer_t *audio_rb_get(void) { return &g_rb; }
uint32_t audio_rx_count(void) { return g_rx_count; }

/*
 * SAI RX DMA transfer-complete ISR -- the producer of the SPSC ring buffer.
 *
 * CACHE-COHERENCY POINT: the ALIENTEK BSP invalidates the whole D-Cache
 * *after* this callback. That is (a) too coarse (full cache, kills perf) and
 * (b) too late (the buffer was written by DMA while stale lines may sit in
 * cache). We do a targeted `SCB_InvalidateDCache_by_Addr` on *this* buffer
 * BEFORE any CPU read. This is the classic Cortex-M7 DMA/DCache lesson.
 */
static void audio_rx_cb(void)
{
    static uint8_t flip = 0;
    uint8_t *done_buf = flip ? g_buf1 : g_buf0;
    flip = !flip;

    /* invalidate the 32-byte lines covering this DMA buffer */
    SCB_InvalidateDCache_by_Addr((uint32_t *)done_buf, AUDIO_BUF_BYTES);

    /* push a whole half-buffer into the SPSC ring (keeps frame alignment) */
    if (rb_free(&g_rb) >= AUDIO_BUF_BYTES) {
        rb_write(&g_rb, done_buf, AUDIO_BUF_BYTES);
    } else {
        g_dropped++;
    }

    g_rx_count++;
    if (g_sem) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(g_sem, &woken);
    }
}
