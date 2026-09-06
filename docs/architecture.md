# Architecture

> Draft — fill in as the firmware lands.

## Data flow

```text
ES8388 ADC (mic / line-in)
      │  I2S (SAI1, master/slave pair, 16-bit)
      ▼
SAI1 RX ──► DMA2_Stream5 double buffer (buf0 <-> buf1)
      │  transfer-complete ISR
      │  1. SCB_InvalidateDCache_by_Addr (targeted, BEFORE read)
      │  2. push whole half-buffer into ring_buffer_t (SPSC)
      ▼
FreeRTOS "processor" task ──► unpack int16 frames ──► FIR filter ──► RMS/peak
      ▼
"streamer" task ──► USART / USB CDC
```

## Tasking model

| Task | Priority | Period | Role |
|------|----------|--------|------|
| Processor | 4 | DMA buffer (~10 ms) | consume ring buffer, filter, compute stats |
| Streamer | 3 | on demand | outbound framing / protocol |
| Monitor | 1 | 1 s | periodic telemetry + performance counters |

## Cache / memory model

- DMA buffers are 32-byte aligned and invalidated via
  `SCB_InvalidateDCache_by_Addr` at the buffer boundary (see `audio/sai_audio.c`).
- The D-Cache line size on Cortex-M7 is 32 bytes; buffer alignment and scaling
  must be multiples of that.
- The naive alternative (whole-cache `SCB_CleanInvalidateDCache()` after the
  callback) is what the ALIENTEK driver does — too late and too coarse.
