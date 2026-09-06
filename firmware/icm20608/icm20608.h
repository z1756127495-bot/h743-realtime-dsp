#ifndef ICM20608_H
#define ICM20608_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

/* ICM20608 SPI sample-rate / DLPF settings */
typedef enum {
    ICM_GYRO_FS_250  = 0,   /* 131 LSB/dps   */
    ICM_GYRO_FS_500  = 1,   /* 65.5 LSB/dps  */
    ICM_GYRO_FS_1000 = 2,   /* 32.8 LSB/dps  */
    ICM_GYRO_FS_2000 = 3,   /* 16.4 LSB/dps  */
} icm_gyro_fs_t;

typedef enum {
    ICM_ACCEL_FS_2g  = 0,   /* 16384 LSB/g */
    ICM_ACCEL_FS_4g  = 1,   /*  8192 LSB/g */
    ICM_ACCEL_FS_8g  = 2,   /*  4096 LSB/g */
    ICM_ACCEL_FS_16g = 3,   /*  2048 LSB/g */
} icm_accel_fs_t;

typedef struct {
    uint8_t  gyro_fs_sel;    /* icm_gyro_fs_t */
    uint8_t  accel_fs_sel;   /* icm_accel_fs_t */
    uint16_t sample_rate_hz; /* e.g. 1000 with DLPF on */
    uint8_t  dlpf_cfg;       /* 0..7, into CONFIG / ACCEL_CONFIG2 */
} icm_cfg_t;

typedef struct {
    SPI_HandleTypeDef *hspi;    /* SPI handle from CubeMX (e.g. hspi1) */
    GPIO_TypeDef      *cs_port; /* CS GPIO port */
    uint16_t           cs_pin;  /* CS GPIO pin  */
    float              gyro_lsb;   /* dps per LSB, derived from fs */
    float              accel_lsb;  /* g per LSB, derived from fs */
} icm20608_t;

typedef struct {
    float acc_x, acc_y, acc_z;
    float gyro_x, gyro_y, gyro_z;
    float temp_c;
} imu_data_t;

/* --- API --------------------------------------------------------------- */
HAL_StatusTypeDef icm_init(icm20608_t *s, const icm_cfg_t *cfg);
HAL_StatusTypeDef icm_whoami(icm20608_t *s, uint8_t *id);
HAL_StatusTypeDef icm_get_imu(icm20608_t *s, imu_data_t *out);
HAL_StatusTypeDef icm_fifo_available(icm20608_t *s, uint16_t *bytes);
HAL_StatusTypeDef icm_fifo_read(icm20608_t *s, uint8_t *buf, uint16_t len);
void icm_convert(imu_data_t *out, const int16_t *raw, const icm20608_t *s);

#endif /* ICM20608_H */

