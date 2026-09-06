#include "sai_audio.h"
#include "perf.h"

#include "BSP/ES8388/es8388.h"
#include "BSP/SAI/sai.h"
#include "MALLOC/malloc.h"

static uint8_t           *g_buf[2];          /* DMA half-buffers (AXI SRAM) */
static SemaphoreHandle_t  g_sem;
static volatile uint32_t  g_rx_count;
static volatile uint8_t   g_ready_idx;

static void audio_rx_cb(void);

static uint8_t *align_ptr(uint8_t *p)
{
    return (uint8_t *)(((uint32_t)p + 31u) & ~31u);
}

void audio_init(SemaphoreHandle_t notify_sem)
{
    g_sem = notify_sem;
    g_buf[0] = align_ptr(mymalloc(SRAMIN, AUDIO_BUF_BYTES + 32));
    g_buf[1] = align_ptr(mymalloc(SRAMIN, AUDIO_BUF_BYTES + 32));

    /* --- ES8388 codec (I2C control, addr 0x10) ------------------------ */
    es8388_init();
    es8388_adda_cfg(0, 1);       /* DAC off, ADC on  */
    es8388_input_cfg(0);         /* MIC input channel */
    es8388_mic_gain(8);
    es8388_sai_cfg(0, 3);        /* codec SAI format = standard, 16-bit data */
    es8388_hpvol_set(25);        /* codec headphone on (matches ALIENTEK recorder) */
    es8388_spkvol_set(25);       /* codec speaker on  (matches ALIENTEK recorder) */

    /* --- SAI1 (I2S) master/slave pair, 16-bit -------------------------- */
    sai1_saia_init(0, 1, 4);     /* Block A / master, 16-bit */
    sai1_saib_init(3, 1, 4);     /* Block B / slave receiver, 16-bit */
    sai1_samplerate_set(AUDIO_FS_HZ);

    /* --- DMA receive, double buffered (buf0 <-> buf1) ------------------ */
    sai1_rx_dma_init(g_buf[0], g_buf[1], AUDIO_BUF_BYTES / 2, 1);  /* 16-bit words */
    sai1_rx_callback = audio_rx_cb;

    /* FreeRTOS fromISR API needs these IRQs at numeric priority >= 5 */
    HAL_NVIC_SetPriority(SAI1_TX_DMASx_IRQ, 6, 0);
    HAL_NVIC_SetPriority(SAI1_RX_DMASx_IRQ, 6, 0);
}

void audio_start(void) { sai1_rec_start(); }
void audio_stop(void)  { sai1_rec_stop(); }

uint8_t  audio_ready_idx(void) { return g_ready_idx; }
uint8_t *audio_dma_buf(uint8_t idx) { return g_buf[idx & 1u]; }
uint32_t audio_rx_count(void) { return g_rx_count; }

/* Minimal ISR: mark which buffer just completed and wake the DSP task.
 * No cache maintenance, no copies here -- the task does the heavy work. */
static void audio_rx_cb(void)
{
    static uint8_t idx = 0;
    g_ready_idx = idx;
    idx = (uint8_t)((idx + 1) & 1u);
    g_rx_count++;
    if (g_sem) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(g_sem, &woken);
    }
}
