/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "pinmux.h"
#include "pinmux_soc.h"
#include "nvcommon.h"
#include "nvodm_pinmux_init.h"

#define PINMUX_AUX_GPIO_W3_AUD_0_PM_RSVD                0
#define PINMUX_AUX_GPIO_PV1_0_PM_RSVD                   0
#define PINMUX_AUX_SDMMC1_WP_N_0_PM_RSVD                0
#define PINMUX_AUX_GPIO_PU4_0_PM_RSVD                   0
#define PINMUX_AUX_GPIO_PU5_0_PM_RSVD                   0
#define PINMUX_AUX_GPIO_PU6_0_PM_RSVD                   0
#define PINMUX_AUX_GPIO_X7_AUD_0_PM_RSVD                0
#define PINMUX_AUX_GPIO_PI5_0_PM_KBC                    0

#if AVP_PINMUX == 0

// Building for T124 Loki
static NvPinDrivePingroupConfig loki_drive_pinmux[] = {
    /* DEFAULT_DRIVE(<pin_group>), */
    /* SDMMC1 */
    SET_DRIVE(SDIO1, DISABLE, DISABLE, DIV_1, 36, 20, SLOW, SLOW),

    /* SDMMC4 */
    SET_DRIVE_GMA(GMA, DISABLE, DISABLE, 1, 2, 1, FASTEST, FASTEST),
};

static NvPingroupConfig loki_pinmux_common[] = {

    /* EXTPERIPH1 pinmux */
    DEFAULT_PINMUX(DAP_MCLK1,     EXTPERIPH1,  NORMAL,    NORMAL,   DISABLE),

    /* I2S1 pinmux */
    DEFAULT_PINMUX(DAP2_DIN,      I2S1,        NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(DAP2_DOUT,     I2S1,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(DAP2_FS,       I2S1,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(DAP2_SCLK,     I2S1,        NORMAL,    NORMAL,   DISABLE),

    /* CLDVFS pinmux */
    DEFAULT_PINMUX(DVFS_PWM,      CLDVFS,      NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(DVFS_CLK,      CLDVFS,      NORMAL,    NORMAL,   DISABLE),

    /* SPI1 pinmux */
    DEFAULT_PINMUX(ULPI_CLK,      SPI1,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(ULPI_DIR,      SPI1,        NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(ULPI_NXT,      SPI1,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(ULPI_STP,      SPI1,        NORMAL,    NORMAL,   DISABLE),

    /* I2C2 pinmux */
    I2C_PINMUX(GEN2_I2C_SCL, I2C2, NORMAL, NORMAL, ENABLE, DISABLE, ENABLE),
    I2C_PINMUX(GEN2_I2C_SDA, I2C2, NORMAL, NORMAL, ENABLE, DISABLE, ENABLE),

    /* SPI4 pinmux */
    DEFAULT_PINMUX(GPIO_PG4,      SPI4,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PG5,      SPI4,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PG6,      SPI4,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PG7,      SPI4,        NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(GPIO_PI3,      SPI4,        NORMAL,    NORMAL,   DISABLE),

    /* EXTPERIPH2 pinmux */
    DEFAULT_PINMUX(CLK2_OUT,      EXTPERIPH2,  NORMAL,    NORMAL,   DISABLE),

    /* SDMMC1 pinmux */
    DEFAULT_PINMUX(SDMMC1_CLK,    SDMMC1,      NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC1_CMD,    SDMMC1,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT0,   SDMMC1,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT1,   SDMMC1,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT2,   SDMMC1,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT3,   SDMMC1,      PULL_UP,   NORMAL,   ENABLE),

    /* SDMMC3 pinmux */
    DEFAULT_PINMUX(SDMMC3_CLK,    SDMMC3,      NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_CMD,    SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT0,   SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT1,   SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT2,   SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT3,   SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_CLK_LB_OUT, SDMMC3,  PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_CLK_LB_IN, SDMMC3,   PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL4,       SDMMC3,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_CD_N,   SDMMC3,      PULL_UP,   NORMAL,   ENABLE),

    /* SDMMC4 pinmux */
    DEFAULT_PINMUX(SDMMC4_CLK,    SDMMC4,      NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_CMD,    SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT0,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT1,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT2,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT3,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT4,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT5,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT6,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT7,   SDMMC4,      PULL_UP,   NORMAL,   ENABLE),

    /* PWM2 pinmux */
    DEFAULT_PINMUX(KB_COL3,       PWM2,        NORMAL,    NORMAL,   DISABLE),

    /* DISPLAYA_ALT pinmux */
    DEFAULT_PINMUX(KB_ROW6,       DISPLAYA_ALT, PULL_DOWN, NORMAL,   ENABLE),

    /* RTCK pinmux */
    DEFAULT_PINMUX(JTAG_RTCK,     RTCK,        PULL_UP,   NORMAL,    DISABLE),

    /* CLK pinmux */
    DEFAULT_PINMUX(CLK_32K_IN,    CLK,         NORMAL,    NORMAL,   ENABLE),

    /* PWRON pinmux */
    DEFAULT_PINMUX(CORE_PWR_REQ,  PWRON,       NORMAL,    NORMAL,   DISABLE),

    /* CPU pinmux */
    DEFAULT_PINMUX(CPU_PWR_REQ,   CPU,         NORMAL,    NORMAL,   DISABLE),

    /* PMI pinmux */
    DEFAULT_PINMUX(PWR_INT_N,     PMI,         PULL_UP,   NORMAL,   ENABLE),

    /* RESET_OUT_N pinmux */
    DEFAULT_PINMUX(RESET_OUT_N,   RESET_OUT_N, NORMAL,    NORMAL,   DISABLE),

    /* I2S3 pinmux */
    DEFAULT_PINMUX(DAP4_DIN,      I2S3,        NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(DAP4_DOUT,     I2S3,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(DAP4_FS,       I2S3,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(DAP4_SCLK,     I2S3,        NORMAL,    NORMAL,   DISABLE),

    /* PWM0 pinmux */
    DEFAULT_PINMUX(GPIO_PU3,      PWM0,        PULL_UP,    NORMAL,   DISABLE),

    DEFAULT_PINMUX(GPIO_PU4,      PWM1,    NORMAL, NORMAL,   ENABLE),

    /* UARTB pinmux */
    DEFAULT_PINMUX(UART2_CTS_N,   UARTB,       NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(UART2_RTS_N,   UARTB,       NORMAL,    NORMAL,   DISABLE),

    /* IRDA pinmux */
    DEFAULT_PINMUX(UART2_RXD,     IRDA,        NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(UART2_TXD,     IRDA,        NORMAL,    NORMAL,   DISABLE),

    /* UARTC pinmux */
    DEFAULT_PINMUX(UART3_CTS_N,   UARTC,       NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(UART3_RTS_N,   UARTC,       NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(UART3_RXD,     UARTC,       NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(UART3_TXD,     UARTC,       NORMAL,    NORMAL,   DISABLE),

    /* CEC pinmux */
    CEC_PINMUX(HDMI_CEC, CEC, NORMAL, NORMAL, ENABLE, DEFAULT, ENABLE),

    /* I2C4 pinmux */
    DDC_PINMUX(DDC_SCL, I2C4, NORMAL, NORMAL, ENABLE, DEFAULT, HIGH),
    DDC_PINMUX(DDC_SDA, I2C4, NORMAL, NORMAL, ENABLE, DEFAULT, HIGH),

    /* USB pinmux */
    USB_PINMUX(USB_VBUS_EN0, USB, NORMAL, NORMAL, ENABLE, DEFAULT, ENABLE),
    USB_PINMUX(USB_VBUS_EN1, USB, NORMAL, NORMAL, ENABLE, DEFAULT, ENABLE),

    /* SOC pinmux */
    DEFAULT_PINMUX(GPIO_PK0,      SOC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(GPIO_PJ2,      SOC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_ROW15,      SOC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(CLK_32K_OUT,   SOC,         PULL_UP,   NORMAL,   ENABLE),

    /* GPIO settings */
    DEFAULT_PINMUX(GPIO_PH0,      PWM0,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PH1,      PWM1,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PH2,      PWM2,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PH3,      PWM3,        NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PI5,      KBC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL6,       KBC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL7,       KBC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL0,       KBC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL1,       KBC,         NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_COL2,       KBC,         PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(KB_ROW0,       KBC,         NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(KB_ROW1,       KBC,         NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(KB_ROW2,       KBC,         NORMAL,    NORMAL,   DISABLE),

    DEFAULT_PINMUX(GPIO_PJ0,      USB,         PULL_UP,   NORMAL,   ENABLE),
};

static NvPingroupConfig loki_sdmmc3_uart_pinmux[] = {
    DEFAULT_PINMUX(SDMMC3_CMD,    UARTA,       NORMAL,    NORMAL,   ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT1,   UARTA,       NORMAL,    NORMAL,   DISABLE),
};

static NvPingroupConfig unused_pins_lowpower[] = {
DEFAULT_PINMUX(DAP1_DIN,      I2S0,        NORMAL,    NORMAL,   ENABLE)
};
#endif

static NvPingroupConfig loki_pinmux_avp[] = {
    /* UART D : DEBUG */
    /* UARTD pinmux */
    DEFAULT_PINMUX(GPIO_PJ7,      UARTD,       NORMAL,    NORMAL,   DISABLE),
    DEFAULT_PINMUX(GPIO_PB0,      UARTD,       PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(GPIO_PB1,      UARTD,       PULL_UP,   NORMAL,   ENABLE),
    DEFAULT_PINMUX(GPIO_PK7,      UARTD,       NORMAL,    NORMAL,   DISABLE),

    /* I2C1 pinmux */

    I2C_PINMUX(GEN1_I2C_SCL, I2C1, NORMAL, NORMAL, ENABLE, DISABLE, DISABLE),
    I2C_PINMUX(GEN1_I2C_SDA, I2C1, NORMAL, NORMAL, ENABLE, DISABLE, DISABLE),

    /* I2CPWR pinmux */
    I2C_PINMUX(PWR_I2C_SCL, I2CPWR, NORMAL, NORMAL, ENABLE, DISABLE, DISABLE),
    I2C_PINMUX(PWR_I2C_SDA, I2CPWR, NORMAL, NORMAL, ENABLE, DISABLE, DISABLE),
};

NvError NvOdmPinmuxInit(NvU32 BoardId)
{

    NvError err = 0;

    err = NvPinmuxConfigTable(loki_pinmux_avp, NV_ARRAY_SIZE(loki_pinmux_avp));
    NV_CHECK_PINMUX_ERROR(err);

#if AVP_PINMUX == 0
    err = NvPinmuxConfigTable(loki_pinmux_common,
                              NV_ARRAY_SIZE(loki_pinmux_common));
    NV_CHECK_PINMUX_ERROR(err);

    err = NvPinmuxDriveConfigTable(loki_drive_pinmux,
       NV_ARRAY_SIZE(loki_drive_pinmux));
    NV_CHECK_PINMUX_ERROR(err);

    err = NvPinmuxConfigTable(unused_pins_lowpower,
    NV_ARRAY_SIZE(unused_pins_lowpower));
    NV_CHECK_PINMUX_ERROR(err);
#endif

fail:
    return err;
}

NvError NvOdmSdmmc3UartPinmuxInit(void)
{
    NvError err = 0;

#if AVP_PINMUX == 0
    err = NvPinmuxConfigTable(loki_sdmmc3_uart_pinmux,
                              NV_ARRAY_SIZE(loki_sdmmc3_uart_pinmux));
    NV_CHECK_PINMUX_ERROR(err);
#endif

fail:
    return err;
}
