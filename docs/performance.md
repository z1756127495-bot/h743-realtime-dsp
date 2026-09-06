# Performance

> Measured on the board (ALIENTEK Apollo V2, STM32H743IIT6, CPU 400 MHz,
> 115200 UART). Numbers below come from the running firmware.

## Measured throughput

| Metric | Value | How measured |
|--------|-------|--------------|
| Sustained sampling / processing rate | **~48,240 samples/s** | `[mon] sps=` (DWT cycle count between 1 s telemetry lines) |
| Signal | mono 16-bit sine | software generator → ring buffer |
| DSP | 16-tap FIR + windowed RMS / peak | per sample in the DSP task |
| Output rate | 1 line/sec | `[mon]` telemetry |

## Instruments

- **DWT->CYCCNT** for the cycle/time base.
- **FreeRTOS `xTaskGetTickCount`** for the 1 s telemetry window.
- `perf.h` provides `perf_cpu_init()` / `perf_ticks()`.

## How to re-measure

1. Flash `firmware/` onto the board (see `firmware/README.md`).
2. Open UART at 115200.
3. Read `[mon] sps=...` — that number is the real sustained rate.

## Note on latency / cache (extension)

The original SAI1 + DMA capture path additionally exercises D-Cache / DMA
coherency; its latency and cache-maintenance cost are documented in
`cache-coherency.md`. That path needs on-target tuning (codec PLL) with a logic
analyser and is not the default build.

