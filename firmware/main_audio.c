/* Entry point for the ALIENTEK Apollo V2 STM32H743IIT6 audio-DSP pipeline.
 *
 * Drop-in replacement for the ALIENTEK FreeRTOS "porting" example's User/main.c.
 * It removes every LCD / LTDC dependency and runs the SAI1 audio capture +
 * FreeRTOS DSP tasks, streaming telemetry over the onboard UART (printf).
 */
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/MPU/mpu.h"
#include "./MALLOC/malloc.h"
#include "FreeRTOS.h"
#include "task.h"

#include "perf.h"
#include "app_tasks.h"

int main(void)
{
    sys_cache_enable();                 /* L1 D-Cache ON (essential for SAI DMA path) */
    HAL_Init();
    sys_stm32_clock_init(160, 5, 2, 4); /* CPU @ 400 MHz */
    delay_init(400);
    usart_init(115200);                 /* printf -> UART1 (CH340 USB-serial) */
    mpu_memory_protection();
    led_init();
    key_init();

    /* ALIENTEK heap regions; audio DMA buffers come from SRAMIN (AXI SRAM) */
    my_mem_init(SRAMIN);
    my_mem_init(SRAM12);
    my_mem_init(SRAM4);
    my_mem_init(SRAMDTCM);
    my_mem_init(SRAMITCM);

    perf_cpu_init();                    /* DWT cycle counter (latency/throughput) */
    app_init();                         /* create tasks + start ES8388/SAI capture */

    vTaskStartScheduler();              /* never returns */

    for (;;) { }
}
