# Architecture

> Draft — fill in as the firmware lands.

## Data flow

```text
signal_src (priority 3)                       DSP task (priority 4)
   generate 16-bit sine samples  ──►  ring_buffer_t (SPSC, lock-free)
   batch of 48 per 1 ms tick          │
   xSemaphoreGive ────────────────────►  unpack int16 ──► FIR ──► RMS/peak
                                              │  latest summary
                                              ▼
                              monitor task (priority 1, 1 Hz) ──► UART telemetry
```

## Tasking model

| Task | Priority | Period | Role |
|------|----------|--------|------|
| Processor | 4 | per batch | consume ring buffer, filter, compute stats |
| Monitor | 1 | 1 s | periodic telemetry + performance counters |

## Cache / memory model

- The default source is a software generator, so no DMA / cache maintenance is
  needed. The SPSC ring buffer is lock-free and power-of-two, so head/tail
  arithmetic wraps with a mask (no modulo).
- For the DMA capture extension (`firmware/audio/`), buffers must be 32-byte
  aligned and invalidated via `SCB_InvalidateDCache_by_Addr` before a CPU read
  (Cortex-M7 line size = 32 bytes). See `cache-coherency.md`.
