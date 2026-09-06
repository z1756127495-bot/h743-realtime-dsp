# H743 Realtime DSP Pipeline

Real-time sensor acquisition pipeline for the 正点原子 / ALIENTEK Apollo V2
**STM32H743IIT6** board (Cortex-M7 @ 480 MHz), structured so the tricky parts are
proven, measured, and documented rather than "blinking LEDs".

> 中文摘要：在 STM32H743 上搭建“高速传感器采集 → DMA/FIFO → FreeRTOS 处理 → 串口/USB
> 输出”的实时管线，重点解决 Cortex-M7 的 **D-Cache 与 DMA 一致性**、确定性调度与延迟/抖动
> 测量。所有关键模块都带可在 PC 上运行的单测，并有 CI。

## What problem does this solve?

Continuously moving high-rate data from a peripheral into a real-time processing
pipeline without losing samples or missing deadlines is hard on a Cortex-M7 for
well-known reasons:

- **DMA / D-Cache coherency** — the DMA is not cache-coherent; stale data appears
  unless you correctly clean/invalidate the cache. This is the classic M7 trap and
  an interview favorite.
- **Determinism** — interrupt and context-switch latency must be bounded and
  measured, not assumed.
- **Throughput** — you have to actually sustain a sample rate and prove it.

This repository makes those engineering decisions explicit and measurable.

## Repository layout

```text
app/        Host-compilable building blocks (ring buffer, FIR filter)
test/       Self-contained unit tests, runs on your PC
docs/       Architecture, performance methodology, cache-coherency deep dive
firmware/   Target-side ES8388/SAI DMA capture + FreeRTOS task framework (H743)
```

## Project status

- [x] Host-runnable primitives + unit tests + CI (this scaffold)
- [x] ES8388/SAI1 DMA double-buffer capture + FreeRTOS task framework
- [ ] CubeMX/Keil firmware project wired to the board
- [ ] Confirm the ALIENTEK BSP is wired in; measure real throughput / IRQ rate
- [ ] FreeRTOS producer / processor / streamer tasks (task skeleton present)
- [ ] DSC / latency / jitter measurement report
- [ ] Cache-coherency write-up (the bug, the diagnosis, the fix)

## Getting started today

The `app/` and `test/` code has **no hardware dependency** and runs anywhere.

```bash
make test          # Linux / Git Bash / WSL (needs gcc)
```

> On Windows, run it from a Git Bash or WSL shell (PowerShell's default `rm` in the
> Makefile `clean` target is not the same tool). If you have no compiler yet,
> install `gcc` (mingw-w64) or use WSL.

## Roadmap (4 weeks)

| Week | Goal |
|------|------|
| 1    | CubeMX/Keil HAL project boots; DWT cycle counter; git repo; README skeleton |
| 2    | ICM20608 SPI + FIFO + DMA; deliberately reproduce the cache bug; fix it; measure DMA throughput |
| 3    | FreeRTOS producer/processor/streamer tasks; measure interrupt + context-switch latency and jitter; CMSIS-DSP FIR |
| 4    | USB/UART output; performance report with real numbers; docs; CI; a tagged release |
