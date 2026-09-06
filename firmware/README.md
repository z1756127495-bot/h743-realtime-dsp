# Firmware (STM32H743IIT6) — real-time audio DSP pipeline

Target side. Designed to be dropped into an **ALIENTEK Apollo V2 H743 example
project** (CubeMX/HAL + Keil MDK5). The board's onboard **ES8388 audio codec**
is the high-rate source: audio in → I2S(SAI1) → DMA double buffer → ring buffer
→ FreeRTOS DSP task → stream out.

> Why audio instead of the IMU: the onboard six-axis (SH3001 / QMI8658A) is on
> **I2C**, which caps at ~50 KB/s and cannot sustain the high data rate needed to
> demonstrate DMA + D-Cache coherency. The audio codec gives a genuine
> 192 KB/s @ 48 kHz stream and a real DSP problem.

## Layout

```text
audio/      ES8388 + SAI1 DMA-capture module (double buffer + cache handling)
rtos/       FreeRTOS tasks + DWT-based perf counter
../../app/  host-tested primitives (ring buffer, FIR) shared with CI
```

## Files you must add from the ALIENTEK example

The `audio/` module calls the proven ALIENTEK BSP driver, so copy these into the
project (they are already in the Apollo example):

```text
Drivers/BSP/ES8388/es8388.c/.h     (codec, I2C addr 0x10)
Drivers/BSP/SAI/sai.c/.h           (SAI1 + DMA, pins PE2/PE3/PE4/PE5/PE6 AF6)
Drivers/BSP/IIC/myiic.c/.h         (codec control bus)
```

## Wiring in `main()`

```c
perf_cpu_init();          // enable DWT cycle counter
app_init();               // creates tasks + initialises & starts SAI audio
```

No extra pin configuration is required — the ALIENTEK BSP already hard-codes the
board's SAI1 pin mapping.

## The cache-coherency point (read this for interviews)

`audio/sai_audio.c` does a **targeted** `SCB_InvalidateDCache_by_Addr` on the DMA
buffer in the RX ISR, before the CPU reads it. The ALIENTEK driver instead calls
`SCB_CleanInvalidateDCache()` **after** the callback — i.e. too late and too
coarse. This is the exact M7 DMA/D-Cache trap, and it is the project's technical
highlight. See `../../docs/cache-coherency.md`.

## Week-2 to-dos

- Confirm measured throughput (`[mon] sps=` should be ~48000) and DMA IRQ rate.
- Replace `stream_send()` printf with USB CDC or Ethernet (lwIP) transport.
- Add the DMA→cache maintenance cost measurement into `docs/performance.md`.

