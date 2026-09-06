# Architecture

> Draft — fill in as the firmware lands.

## Data flow

```text
ICM20608 (SPI+FIFO)
      │  SPI + DMA (single-shot on FIFO watermark)
      ▼
DMA / ISR ──► ring_buffer_t (SPSC, power-of-two) ──► FreeRTOS "processor" task
                                                      │ convert to float frames
                                                      ▼
                                              CMSIS-DSP FIR / stats
                                                      ▼
                                    "streamer" task ──► USART / USB CDC
```

## Tasking model

| Task | Priority | Period | Role |
|------|----------|--------|------|
| Processor | High | set by sample rate | consume FIFO, filter, compute stats |
| Streamer | Medium | lower | outbound framing / protocol |
| Monitor | Low | 1 s | periodic telemetry + performance counters |

## Cache / memory model

- DMA buffers live in a region where cache is either configured write-through or
  flushed via `SCB_CleanDCache_by_Addr` / `SCB_InvalidateDCache_by_Addr` at the
  buffer boundary.
- The D-Cache line size on Cortex-M7 is 32 bytes; buffer alignment and scaling
  must be multiples of that.
- Document the choice of normal vs non-cacheable memory and why.

