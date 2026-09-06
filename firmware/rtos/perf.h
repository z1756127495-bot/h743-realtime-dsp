#ifndef PERF_H
#define PERF_H

#include "stm32h7xx_hal.h"

/* DWT cycle counter: the only sane way to measure latency / throughput.
 * 480 MHz => 1 cycle = ~2.083 ns. Enable once at startup. */
static inline void perf_cpu_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

#define perf_ticks() (DWT->CYCCNT)
#define perf_cycles_to_ns(c) (((uint64_t)(c)) * 1000u / 480u)  /* ns, 480 MHz */

#endif /* PERF_H */
