#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* One FIFO packet from the ICM20608 = accel(x,y,z) + gyro(x,y,z) = 12 bytes */
#define IMU_SAMPLE_BYTES 12u

/* Processed frame handed from the DSP task to the streamer task. */
typedef struct {
    float acc_x_filt;
    float acc_rms;      /* RMS of the filtered axis over the release window */
    float gyro_rms;
    float temp_c;
    uint32_t seq;       /* sample counter, for continuity checks */
} dsp_frame_t;

/* Owned by board.c / main.c (the CubeMX HAL project) */
extern icm20608_t g_imu;      /* defined in board.c */
extern const icm_cfg_t g_imu_cfg;

void app_init(void);          /* create ring buffer, queue, and the 4 tasks */
void stream_send(const dsp_frame_t *f);  /* weak default -> printf(UART) */

#endif /* APP_TASKS_H */

