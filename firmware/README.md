# Firmware — real-time DSP pipeline (STM32H743IIT6, FreeRTOS)

Default target path for the ALIENTEK Apollo V2 H743. The pipeline runs
out-of-the-box with a **deterministic software signal generator** (no codec/DMA
clock to tune), so it reliably demonstrates the RTOS + DSP + measurement stack.

## Pipeline

```text
signal_src task (prio 3)   →  SPSC ring buffer  →  DSP task (prio 4: FIR + RMS/peak)
        │ 48 samples / 1 ms tick                    │
        └── xSemaphoreGive ─────────────────────────┘
monitor task (prio 1, 1 Hz) → UART telemetry:  [mon] sps=N rms=... peak=...
```

## Files

```text
signal_src/   software 16-bit sine generator (producer task)
rtos/         app_tasks: DSP task + monitor;  perf.h : DWT cycle counter
app/          host-tested SPSC ring buffer + FIR
audio/        (extension) ES8388 + SAI1 + DMA capture; needs on-target tuning
```

## Build / run

On the ALIENTEK HAL+FreeRTOS Keil project: copy `app/`, `signal_src/`,
`rtos/audio*/` into a source group, add the include path, and in `main()`:

```c
perf_cpu_init();
app_init();          // creates tasks + starts the generator
```

Open UART 115200: once per second you get `[mon] sps=... rms=... peak=...`.
`sps` is the real measured throughput (~48,240 samples/s at 400 MHz).

## Extension: live audio via the codec (SAI1 + DMA)

`firmware/audio/` contains the ES8388 codec + SAI1 + DMA capture that exercises
**D-Cache / DMA coherency**. It is not the default build because the codec's PLL
clock (PLL2) must be tuned on-target (oscilloscope / logic analyser). The design
and cache-coherency analysis are in `../docs/cache-coherency.md`.

