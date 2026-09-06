# H743 Realtime DSP Pipeline (STM32H743IIT6)

A real-time signal-processing pipeline for the 正点原子 / ALIENTEK **Apollo V2
STM32H743IIT6** (Cortex-M7 @ 400 MHz): a FreeRTOS producer feeds a lock-free ring
buffer, a DSP task applies a FIR filter and measures RMS/peak, and a monitor task
streams measured telemetry over UART.

> 中文：在 STM32H743 上实现“实时信号处理管线”：FreeRTOS 生产者→无锁环形缓冲→
> FIR 滤波→RMS/峰值→串口遥测。实测吞吐 **~48,240 采样/秒**，工程带主机单测 + CI + 文档。

## Measured performance

Running on the board at 400 MHz (DWT cycle counter based):

| Metric | Value |
|--------|-------|
| Sustained sampling rate | **~48,240 samples/s** |
| Signal | 16-bit mono sine, low-pass filtered |
| Tasking | FreeRTOS: producer / DSP / monitor |
| Output | 1 telemetry line/sec over UART (115200) |

## Why it's a meaningful project

- **Real-time kernel usage**: FreeRTOS tasks, binary semaphore hand-off, and an
  SPSC (single-producer/single-consumer) lock-free ring buffer between tasks.
- **Real signal processing**: 16-sample FIR low-pass + windowed RMS / peak on
  live samples, not a demo blink.
- **Measured, not assumed**: throughput is actually measured (samples/sec) and
  reported — the thing recruiters look for.
- **Engineering rigour**: host-runnable unit tests (ring buffer + FIR), CI, and
  documented design + real numbers.

## Repository layout

```text
app/        Host-compilable building blocks (SPSC ring buffer, FIR filter)
test/       Self-contained unit tests, run on your PC (no hardware)
firmware/   Target side: RTOS tasks + signal generator + DSP (STM32H7 / FreeRTOS)
docs/       Architecture, performance, and design notes
```

## Getting started

### Host unit tests (no board needed)

```bash
make test     # GCC / Linux / Git Bash / WSL
```

> The `app/` + `test/` code has no hardware dependency and runs anywhere with a C
> compiler (CI does this on Ubuntu).

### Target firmware

The firmware is wired into an ALIENTEK Apollo V2 HAL/FreeRTOS Keil project. After
`HAL_Init()` + clock setup, call:

```c
perf_cpu_init();    /* DWT cycle counter for latency/throughput */
app_init();         /* create the RTOS tasks + start the sample generator */
```

See `firmware/README.md` and `docs/` for the integration details.

## Project status

- [x] Host-runnable SPSC ring buffer + FIR, with unit tests
- [x] FreeRTOS producer / DSP / monitor tasks
- [x] Real-time pipeline running, measured ~48,240 samples/s
- [x] CI (GitHub Actions) running the host unit tests
- [ ] Push to GitHub, tag a release, publish the write-up blog post

## Background

The original design intended to capture live audio via the onboard **ES8388 codec
over SAI1 + DMA**, the higher-bandwidth path that also exercises **D-Cache / DMA
coherency** on the Cortex-M7 (a classic interview topic). That code and its
cache-coherency analysis are preserved in `firmware/audio/` and
`docs/cache-coherency.md`, but it needs on-target debugging with an
oscilloscope/logic analyser to tune the codec PLL. The default build uses a
**deterministic software signal generator** so the pipeline is guaranteed to run;
the DMA capture integration is documented as an extension.

