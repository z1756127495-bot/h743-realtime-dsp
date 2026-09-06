# Firmware (STM32H743IIT6)

This is the target side. It is designed to be dropped into a **CubeMX/HAL
project** (`STM32H743IIT6`), or referenced inside a Keil `MDK5` project. All
source here is portable C; the only hardware coupling is through the STM32H7 HAL
(SPI / GPIO / DWT), isolated so it is easy to adapt.

## Layout

```text
icm20608/   SPI register driver (init, WHO_AM_I, FIFO read, scale conversion)
rtos/       FreeRTOS tasks + DWT-based perf counter
../../app/  host-tested primitives (ring buffer, FIR) shared with CI
```

## How to wire it up (CubeMX / CubeIDE)

1. New project, device = **STM32H743IIT6**.
2. SPI1 (or SPI4) in full-duplex master mode, speed **<= ~4 MHz**, CPOL/CPHA
   high (Mode 3). Assign `SCK/MISO/MOSI` pins.
3. One GPIO as output for **CS**. Record its port/pin into the `g_imu` handle in
   `board.c`.
4. Enable **FreeRTOS (CMSIS-RTOS v2 or FreeRTOS kernel)** and **CMSIS-DSP**.
5. Add `app/`, `firmware/icm20608/`, `firmware/rtos/` to the project's source
   groups and include paths.
6. In `main()` after `HAL_Init()`:
   - `perf_cpu_init();` (enable DWT cycle counter)
   - `icm_init(&g_imu, &g_imu_cfg);`
   - `app_init();`

`board.c` must define `icm20608_t g_imu` (with the real `hspi1`/CS) and
`const icm_cfg_t g_imu_cfg` (e.g. 1 kHz, gyro ±2000 dps, accel ±4 g, DLPF=42 Hz).

## Next steps (Week 2)

- Move the producer off `vTaskDelay(1)` polling into a **DMA-driven** FIFO read
  triggered by the ICM20608 data-ready / FIFO watermark interrupt, signalled to
  the `acq_task` via a FreeRTOS semaphore.
- Add **cache maintenance** (`SCB_CleanDCache_by_Addr` / `SCB_InvalidateDCache_by_Addr`)
  around any DMA buffer and record the cost. See `../../docs/cache-coherency.md`.
- Replace `stream_send()` printf with USB CDC or Ethernet (lwIP) transport.

