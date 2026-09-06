# Performance methodology

> Draft — record real measurements here. Without numbers, the project is a demo.

## Instruments

- **DWT->CYCCNT** for per-operation cycle counts (480 MHz => 1 cycle = ~2.08 ns).
- **GPIO toggling + logic analyzer** for hardware-level ISR latency.
- **FreeRTOS tick + a ping-pong task** for context-switch latency and jitter.

## Metrics to record

- Sustained sample rate achieved (Hz) and how it was chosen.
- DMA transfer throughput (bytes/s) with and without cache maintenance.
- Cache clean/invalidate cost per buffer (cycles).
- ISR entry-to-first-instruction latency (ns).
- FreeRTOS context-switch latency (ns) and worst-case jitter.
- CPU load per task (from the monitor task).

## Reproducibility

State the exact test: input signal, sample rate, buffer size, cache config, clocks.

