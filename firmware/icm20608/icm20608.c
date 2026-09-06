#include "icm20608.h"

/* --- Register map ------------------------------------------------------- */
#define ICM_WHO_AM_I        0x75
#define ICM_PWR_MGMT_1      0x6B
#define ICM_PWR_MGMT_2      0x6C
#define ICM_SMPLRT_DIV      0x19
#define ICM_CONFIG          0x1A
#define ICM_GYRO_CONFIG     0x1B
#define ICM_ACCEL_CONFIG    0x1C
#define ICM_ACCEL_CONFIG2   0x1D
#define ICM_FIFO_EN         0x23
#define ICM_INT_ENABLE      0x38
#define ICM_INT_STATUS      0x3A
#define ICM_USER_CTRL       0x6A
#define ICM_ACCEL_XOUT_H    0x3B
#define ICM_FIFO_COUNT_H    0x72
#define ICM_FIFO_COUNT_L    0x73
#define ICM_FIFO_R_W        0x74

#define ICM_WHOAMI_EXPECT   0xAF

/* USER_CTRL bits */
#define UCTRL_FIFO_RST      0x01
#define UCTRL_FIFO_EN       0x40
/* FIFO_EN bits */
#define FIFO_GYRO_EN        0x80
#define FIFO_ACCEL_EN       0x40

/* --- Low-level SPI access ---------------------------------------------- */
static void cs_low(icm20608_t *s)  { HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_RESET); }
static void cs_high(icm20608_t *s) { HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_SET); }

/* Read `len` bytes starting at register `reg` (auto sets RW=1 bit). */
static HAL_StatusTypeDef read_regs(icm20608_t *s, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint8_t addr = reg | 0x80;
    cs_low(s);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(s->hspi, &addr, 1, HAL_MAX_DELAY);
    if (st == HAL_OK) {
        st = HAL_SPI_Receive(s->hspi, data, len, HAL_MAX_DELAY);
    }
    cs_high(s);
    return st;
}

/* Write `len` bytes to register `reg` (RW=0). */
static HAL_StatusTypeDef write_regs(icm20608_t *s, uint8_t reg, const uint8_t *data, uint16_t len)
{
    uint8_t addr = reg & 0x7F;
    cs_low(s);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(s->hspi, &addr, 1, HAL_MAX_DELAY);
    if (st == HAL_OK) {
        st = HAL_SPI_Transmit(s->hspi, (uint8_t *)data, len, HAL_MAX_DELAY);
    }
    cs_high(s);
    return st;
}

static HAL_StatusTypeDef write_reg(icm20608_t *s, uint8_t reg, uint8_t val)
{
    return write_regs(s, reg, &val, 1);
}

static float gyro_lsb_for(uint8_t sel)
{
    switch (sel) {
        case ICM_GYRO_FS_500:  return 65.5f;
        case ICM_GYRO_FS_1000: return 32.8f;
        case ICM_GYRO_FS_2000: return 16.4f;
        default:               return 131.0f;
    }
}

static float accel_lsb_for(uint8_t sel)
{
    switch (sel) {
        case ICM_ACCEL_FS_4g:  return 8192.0f;
        case ICM_ACCEL_FS_8g:  return 4096.0f;
        case ICM_ACCEL_FS_16g: return 2048.0f;
        default:               return 16384.0f;
    }
}

/* --- Public API --------------------------------------------------------- */
HAL_StatusTypeDef icm_init(icm20608_t *s, const icm_cfg_t *cfg)
{
    uint8_t v;

    s->gyro_lsb  = gyro_lsb_for(cfg->gyro_fs_sel);
    s->accel_lsb = accel_lsb_for(cfg->accel_fs_sel);

    /* software reset, wait for it to settle */
    write_reg(s, ICM_PWR_MGMT_1, 0x80);
    HAL_Delay(100);

    /* wake up, clock = internal PLL */
    write_reg(s, ICM_PWR_MGMT_1, 0x01);
    write_reg(s, ICM_PWR_MGMT_2, 0x00);          /* all axes enabled */

    /* sample rate = 1000 / (1 + SMPLRT_DIV) when DLPF is enabled */
    uint8_t div = (uint8_t)((1000u / (cfg->sample_rate_hz ? cfg->sample_rate_hz : 1u)) - 1u);
    write_reg(s, ICM_SMPLRT_DIV, div);

    write_reg(s, ICM_CONFIG, cfg->dlpf_cfg & 0x07);
    write_reg(s, ICM_ACCEL_CONFIG2, cfg->dlpf_cfg & 0x07);  /* accel DLPF */
    write_reg(s, ICM_GYRO_CONFIG,  (uint8_t)((cfg->gyro_fs_sel) << 3));
    write_reg(s, ICM_ACCEL_CONFIG, (uint8_t)((cfg->accel_fs_sel) << 3));

    /* reset FIFO, then route accel+gyro into it */
    write_reg(s, ICM_USER_CTRL, UCTRL_FIFO_RST);
    HAL_Delay(1);
    write_reg(s, ICM_FIFO_EN, FIFO_GYRO_EN | FIFO_ACCEL_EN);
    write_reg(s, ICM_USER_CTRL, UCTRL_FIFO_EN);

    /* read back who-am-i as a self check */
    (void)icm_whoami(s, &v);
    return v == ICM_WHOAMI_EXPECT ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef icm_whoami(icm20608_t *s, uint8_t *id)
{
    return read_regs(s, ICM_WHO_AM_I, id, 1);
}

HAL_StatusTypeDef icm_fifo_available(icm20608_t *s, uint16_t *bytes)
{
    uint8_t b[2];
    HAL_StatusTypeDef st = read_regs(s, ICM_FIFO_COUNT_H, b, 2);
    *bytes = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    return st;
}

HAL_StatusTypeDef icm_fifo_read(icm20608_t *s, uint8_t *buf, uint16_t len)
{
    return read_regs(s, ICM_FIFO_R_W, buf, len);
}

HAL_StatusTypeDef icm_get_imu(icm20608_t *s, imu_data_t *out)
{
    uint8_t b[14];
    int16_t raw[6];
    HAL_StatusTypeDef st = read_regs(s, ICM_ACCEL_XOUT_H, b, 14);
    if (st != HAL_OK) {
        return st;
    }
    /* 14-byte burst: accel(6) + temp(2) + gyro(6) */
    raw[0] = (int16_t)((uint16_t)((uint16_t)b[0]  << 8) | b[1]);   /* acc x */
    raw[1] = (int16_t)((uint16_t)((uint16_t)b[2]  << 8) | b[3]);   /* acc y */
    raw[2] = (int16_t)((uint16_t)((uint16_t)b[4]  << 8) | b[5]);   /* acc z */
    raw[3] = (int16_t)((uint16_t)((uint16_t)b[8]  << 8) | b[9]);   /* gyro x */
    raw[4] = (int16_t)((uint16_t)((uint16_t)b[10] << 8) | b[11]);  /* gyro y */
    raw[5] = (int16_t)((uint16_t)((uint16_t)b[12] << 8) | b[13]);  /* gyro z */

    icm_convert(out, raw, s);
    /* temp is the 7th/8th byte of the 14-byte read (registers 0x41/0x42) */
    out->temp_c = ((int16_t)((uint16_t)((uint16_t)b[6] << 8) | b[7])) / 326.8f + 25.0f;
    return HAL_OK;
}

void icm_convert(imu_data_t *out, const int16_t *raw, const icm20608_t *s)
{
    out->acc_x  = raw[0] / s->accel_lsb;
    out->acc_y  = raw[1] / s->accel_lsb;
    out->acc_z  = raw[2] / s->accel_lsb;
    out->gyro_x = raw[3] / s->gyro_lsb;
    out->gyro_y = raw[4] / s->gyro_lsb;
    out->gyro_z = raw[5] / s->gyro_lsb;
}
