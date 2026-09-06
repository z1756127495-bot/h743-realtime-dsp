# DMA / D-Cache coherency on STM32H743

> Draft — the centrepiece of the project narrative. Explain *why* a naive DMA
> pipeline shows corrupted or stale data, how you diagnosed it, and how it was
> fixed, with measured before/after numbers.

## The bug

The Cortex-M7 D-cache is **not coherent with buses** (AHB/AXI peripherals). If the
CPU writes data that is still in cache and the DMA then reads it, the DMA sees
stale data. If the DMA writes and the CPU then reads it, the CPU reads stale data
unless the cache line is invalidated.

## The diagnosis

Describe the symptoms (e.g. "first frames are fine, then periodic garbage"),
the tools used (DWT, debugger, a known pattern fill), and the reasoning chain.

## The fix

- Align and size buffers to the 32-byte cache line.
- `SCB_CleanDCache_by_Addr` before a DMA read, `SCB_InvalidateDCache_by_Addr`
  after a DMA write.
- Consider `MPU`-based non-cacheable or write-through regions for DMA buffers.
- Measure the maintenance cost and discuss the trade-off.

