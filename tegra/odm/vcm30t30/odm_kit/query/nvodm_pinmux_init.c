/*
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
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


#define PINMUX_AUX_SDMMC4_DAT0_0_IO_RESET_DISABLE   PINMUX_AUX_SDMMC4_DAT0_0_IO_RESET_NORMAL
#define PINMUX_AUX_SDMMC4_DAT1_0_IO_RESET_DISABLE   PINMUX_AUX_SDMMC4_DAT0_0_IO_RESET_NORMAL
#define PINMUX_AUX_SDMMC4_DAT2_0_IO_RESET_DISABLE   PINMUX_AUX_SDMMC4_DAT0_0_IO_RESET_NORMAL
#define PINMUX_AUX_SDMMC4_DAT3_0_IO_RESET_DISABLE   PINMUX_AUX_SDMMC4_DAT0_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_PCLK_0_IO_RESET_DISABLE       PINMUX_AUX_VI_PCLK_0_IO_RESET_NORMAL
#define PINMUX_AUX_DDC_SCL_0_OD_ENABLE              NV_PINMUX_E_OD_ENABLE
#define PINMUX_AUX_DDC_SDA_0_OD_ENABLE              NV_PINMUX_E_OD_ENABLE
#define PINMUX_AUX_VI_D2_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D3_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D4_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D5_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D6_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D7_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D8_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_VI_D9_0_IO_RESET_DISABLE         PINMUX_AUX_VI_D2_0_IO_RESET_NORMAL
#define PINMUX_AUX_GPIO_PV0_0_PM_RSVD               NV_PINMUX_PM_RSVD
#define PINMUX_AUX_GPIO_PV1_0_PM_RSVD               NV_PINMUX_PM_RSVD
#define PINMUX_AUX_HDMI_INT_0_PM_RSVD0              NV_PINMUX_PM_RSVD
#define PINMUX_AUX_VI_D0_0_PM_SAFE                  PINMUX_AUX_VI_D0_0_PM_DEFAULT
#define PINMUX_AUX_SPDIF_OUT_0_PM_SAFE              PINMUX_AUX_SPDIF_OUT_0_PM_DEFAULT

static NvPingroupConfig vcm30t30_pinmux_avp[] = {
    /* UART1 pinmux */
    DEFAULT_PINMUX(ULPI_DATA0,      UARTA,          NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(ULPI_DATA1,      UARTA,          NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DATA2,      UARTA,          NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DATA3,      UARTA,          NORMAL,         NORMAL,         DISABLE),
    /* UART2 pinmux */
    DEFAULT_PINMUX(UART2_RXD,       IRDA,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(UART2_TXD,       IRDA,           NORMAL,         NORMAL,         DISABLE),
    /* UART4 pinmux */
    DEFAULT_PINMUX(GMI_A16,         UARTD,          NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_A17,         UARTD,          NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_A18,         UARTD,          NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_A19,         UARTD,          NORMAL,         NORMAL,         DISABLE),
    /* I2C1 pinmux */
    I2C_PINMUX(GEN1_I2C_SCL,        I2C1,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
    I2C_PINMUX(GEN1_I2C_SDA,        I2C1,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
    /* PowerI2C pinmux */
    I2C_PINMUX(PWR_I2C_SCL,         I2CPWR,         NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
    I2C_PINMUX(PWR_I2C_SDA,         I2CPWR,         NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
};

#if AVP_PINMUX == 0
static NvPingroupConfig vcm30t30_pinmux_common[] = {
    /* SDMMC1 pinmux */
    DEFAULT_PINMUX(SDMMC1_CLK,      SDMMC1,         NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC1_CMD,      SDMMC1,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT3,     SDMMC1,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT2,     SDMMC1,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT1,     SDMMC1,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC1_DAT0,     SDMMC1,         PULL_UP,        NORMAL,         ENABLE),

    /* SDMMC2 pinmux */
    DEFAULT_PINMUX(KB_ROW10,        SDMMC2,         NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW11,        SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW12,        SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW13,        SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW14,        SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW15,        SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW6,         SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW7,         SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW8,         SDMMC2,         PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(KB_ROW9,         SDMMC2,         PULL_UP,        NORMAL,         ENABLE),

    /* SDMMC4 pinmux */
    DEFAULT_PINMUX(CAM_MCLK,        POPSDMMC4,      NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PCC1,       POPSDMMC4,      NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PBB0,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    I2C_PINMUX(CAM_I2C_SCL,         POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE,          DISABLE,        DISABLE),
    I2C_PINMUX(CAM_I2C_SDA,         POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE,          DISABLE,        DISABLE),
    DEFAULT_PINMUX(GPIO_PBB3,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PBB4,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PBB5,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PBB6,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PBB7,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),
    DEFAULT_PINMUX(GPIO_PCC2,       POPSDMMC4,      PULL_UP,        NORMAL,         ENABLE),

    /* UART5 pinmux */
    LVPAD_PINMUX(SDMMC4_DAT0,       UARTE,          NORMAL,         NORMAL,         DISABLE, DISABLE,        DISABLE),
    LVPAD_PINMUX(SDMMC4_DAT1,       UARTE,          NORMAL,         NORMAL,         ENABLE,  DISABLE,        DISABLE),
    LVPAD_PINMUX(SDMMC4_DAT2,       UARTE,          NORMAL,         NORMAL,         ENABLE,  DISABLE,        DISABLE),
    LVPAD_PINMUX(SDMMC4_DAT3,       UARTE,          NORMAL,         NORMAL,         DISABLE, DISABLE,        DISABLE),

    /* I2C2 pinmux */
    I2C_PINMUX(GEN2_I2C_SCL,        I2C2,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
    I2C_PINMUX(GEN2_I2C_SDA,        I2C2,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),

    /* I2C4 pinmux */
    I2C_PINMUX(DDC_SCL,             I2C4,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),
    I2C_PINMUX(DDC_SDA,             I2C4,           NORMAL,         NORMAL,         ENABLE,          DISABLE,        ENABLE),

    /* SPI1 pinmux */
    DEFAULT_PINMUX(ULPI_CLK,        SPI1,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DIR,        SPI1,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_NXT,        SPI1,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_STP,        SPI1,           NORMAL,         NORMAL,         ENABLE),

    /* SPI2 pinmux */
    DEFAULT_PINMUX(ULPI_DATA4,      SPI2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DATA5,      SPI2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DATA6,      SPI2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(ULPI_DATA7,      SPI2,           NORMAL,         NORMAL,         ENABLE),

    /* SPI3 pinmux  */
    DEFAULT_PINMUX(SDMMC3_CLK,      SPI3,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT0,     SPI3,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT1,     SPI3,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT2,     SPI3,           NORMAL,         NORMAL,         ENABLE),

    /* SPDIF pinmux */
    DEFAULT_PINMUX(SPDIF_IN,        SPDIF,          NORMAL,         NORMAL,         ENABLE),

    /* DAP1 */
    DEFAULT_PINMUX(DAP1_FS,         I2S0,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP1_DIN,        I2S0,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP1_DOUT,       I2S0,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP1_SCLK,       I2S0,           NORMAL,         NORMAL,         ENABLE),

    /* DAP2 */
    DEFAULT_PINMUX(DAP3_FS,         I2S2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP3_DIN,        I2S2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP3_DOUT,       I2S2,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(DAP3_SCLK,       I2S2,           NORMAL,         NORMAL,         ENABLE),

    /* DAP3 */
    DEFAULT_PINMUX(SDMMC4_DAT4,     I2S4,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT5,     I2S4,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT6,     I2S4,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(SDMMC4_DAT7,     I2S4,           NORMAL,         NORMAL,         ENABLE),

    /* NOR pinmux */
    DEFAULT_PINMUX(GMI_AD0,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD1,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD2,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD3,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD4,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD5,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD6,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD7,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD8,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD9,         GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD10,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD11,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD12,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD13,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD14,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_AD15,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_ADV_N,       NAND,           NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_CLK,         NAND,           NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_CS0_N,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_OE_N,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_RST_N,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_WAIT,        GMI,            NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(GMI_WP_N,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GMI_WR_N,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP2_FS,         GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP2_DIN,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP2_DOUT,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP2_SCLK,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(SPI1_MOSI,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(SPI2_CS0_N,      GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(SPI2_SCK,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(SPI2_MOSI,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(SPI2_MISO,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP4_FS,         GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP4_DIN,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP4_DOUT,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(DAP4_SCLK,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU0,    GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU1,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU2,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU3,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU4,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU5,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(GPIO_PU6,        GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART2_RTS_N,     GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART2_CTS_N,     GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART3_TXD,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART3_RXD,       GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART3_CTS_N,     GMI,            NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(UART3_RTS_N,     GMI,            NORMAL,         NORMAL,         DISABLE),

    /* DISPLAY pinmux */
    DEFAULT_PINMUX(LCD_CS1_N,       DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D0,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D1,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D2,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D3,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D4,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D5,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D6,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D7,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D8,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D9,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D10,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D11,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D12,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D13,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D14,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D15,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D16,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D17,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D18,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D19,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D20,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D21,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D22,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_D23,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_DC0,         DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_DE,          DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_HSYNC,       DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_PCLK,        DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_VSYNC,       DISPLAYA,       NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(LCD_WR_N,        DISPLAYA,       NORMAL,         NORMAL,         ENABLE),

    DEFAULT_PINMUX(PEX_WAKE_N,      PCIE,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(PEX_L1_PRSNT_N,  PCIE,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(PEX_L1_RST_N,    PCIE,           NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(PEX_L1_CLKREQ_N, PCIE,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(PEX_L2_PRSNT_N,  PCIE,           NORMAL,         NORMAL,         ENABLE),
    DEFAULT_PINMUX(PEX_L2_RST_N,    PCIE,           NORMAL,         NORMAL,         DISABLE),
    DEFAULT_PINMUX(PEX_L2_CLKREQ_N, PCIE,           NORMAL,         NORMAL,         ENABLE),

    DEFAULT_PINMUX(CLK1_OUT,    EXTPERIPH1,     PULL_DOWN,      NORMAL, ENABLE),

    VI_PINMUX(VI_D2,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D3,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D4,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D5,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D6,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D7,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D8,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_D9,                VI,             NORMAL,         NORMAL,         ENABLE,          DISABLE,        DISABLE),
    VI_PINMUX(VI_PCLK,              VI,             PULL_UP,        TRISTATE,       ENABLE,          DISABLE,        DISABLE),

    /* pin config for gpios */
    DEFAULT_PINMUX(VI_D0,           SAFE,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(CLK1_REQ,        RSVD2,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_SCK,         SPI5,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_DC1,         RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT5,     SDMMC3, NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SPDIF_OUT,       SAFE,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SPI1_SCK,        GMI,    NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SPI1_CS0_N,      GMI,    NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SPI1_MISO,       RSVD3,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SPI2_CS2_N,      SPI2,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(GPIO_PV0,        RSVD,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(GPIO_PV1,        RSVD,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(CRT_HSYNC,       RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(CRT_VSYNC,       RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_CS0_N,       RSVD,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_M1,          RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_PWR0,        SPI5,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_PWR1,        RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_PWR2,        SPI5,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_SDIN,        RSVD,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(LCD_SDOUT,       SPI5,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(GPIO_PV2,        RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(GPIO_PV3,        RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT7,     SDMMC3, NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC4_CLK,      NAND,   NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC3_CMD,      SDMMC3, NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT3,     RSVD0,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(VI_D1,           RSVD1,  NORMAL, NORMAL, ENABLE),
    DEFAULT_PINMUX(SDMMC3_DAT4,     SDMMC3, NORMAL, TRISTATE,       ENABLE),
    DEFAULT_PINMUX(SPI2_CS1_N,      SPI2,   NORMAL, TRISTATE,       ENABLE),
    DEFAULT_PINMUX(HDMI_INT,        RSVD0,  NORMAL, TRISTATE,       ENABLE),
};

// Building for T30 Dalmore
static NvPinDrivePingroupConfig vcm30t30_drive_pinmux[] = {
    /* ATC1 CFG */
    SET_DRIVE(AT1,  ENABLE, ENABLE, DIV_1, 0, 0, SLOWEST, SLOWEST),
    /* ATC2 CFG */
    SET_DRIVE(AT2,  ENABLE, ENABLE, DIV_1, 0, 0, SLOWEST, SLOWEST),
    /* ATC3 CFG */
    SET_DRIVE(AT3,  ENABLE, ENABLE, DIV_1, 0, 0, SLOWEST, SLOWEST),
    /* ATC4 CFG */
    SET_DRIVE(AT4,  DISABLE, DISABLE, DIV_1, 0, 0, SLOWEST, SLOWEST),

    /* All I2C pins are driven to maximum drive strength */
    /* GEN1 I2C */
    SET_DRIVE(DBG,  DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

    /* GEN2 I2C */
    SET_DRIVE(AT5,  DISABLE, ENABLE, DIV_1, 12, 30, FASTEST, FASTEST),

    /* DDC I2C */
    SET_DRIVE(DDC,  DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

    /* PWR_I2C */
    SET_DRIVE(AO1,  DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

    /* SDMMC4 */
    SET_DRIVE(GME,  DISABLE, ENABLE, DIV_1, 22, 18, SLOWEST, SLOWEST),
    SET_DRIVE(GMF,  DISABLE, ENABLE, DIV_1,  0,  0, SLOWEST, SLOWEST),
    SET_DRIVE(GMG,  DISABLE, ENABLE, DIV_1, 15,  6, SLOWEST, SLOWEST),
    SET_DRIVE(GMH,  DISABLE, ENABLE, DIV_1, 12,  6, SLOWEST, SLOWEST),

    /* LCD */
    SET_DRIVE(LCD1, DISABLE, ENABLE, DIV_1, 31,  31, FASTEST, FASTEST),
    SET_DRIVE(LCD2, DISABLE, ENABLE, DIV_1,  2,   2, FASTEST, FASTEST),

    /* DAP2 */
    SET_DRIVE(DAP2, ENABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* DAP4 */
    SET_DRIVE(DAP4, ENABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* DBG */
    SET_DRIVE(DBG,  ENABLE, ENABLE, DIV_1, 20,  0, SLOWEST, SLOWEST),
    /* SPI */
    SET_DRIVE(SPI,  ENABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* UAA */
    SET_DRIVE(UAA,  DISABLE, DISABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* UART2 */
    SET_DRIVE(UART2,        ENABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* UART3 */
    SET_DRIVE(UART3,        ENABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* GME */
    SET_DRIVE(GME,  DISABLE, ENABLE, DIV_1, 1,  4, SLOWEST, SLOWEST),
    /* GMF */
    SET_DRIVE(GMF,  DISABLE, ENABLE, DIV_1, 0,  0, SLOWEST, SLOWEST),
    /* GMG */
    SET_DRIVE(GMG,  DISABLE, ENABLE, DIV_1, 3,  0, SLOWEST, SLOWEST),
    /* GMH */
    SET_DRIVE(GMH,  DISABLE, ENABLE, DIV_1, 0,  12, SLOWEST, SLOWEST),

    /* I2S/TDM */
    SET_DRIVE(DAP1, ENABLE, ENABLE, DIV_1, 20,  20, SLOWEST, SLOWEST),
    SET_DRIVE(DAP3, ENABLE, ENABLE, DIV_1, 20,  20, SLOWEST, SLOWEST),

    /* SPI */
    SET_DRIVE(UAD,          DISABLE, ENABLE, DIV_1, 4, 1, SLOWEST, SLOWEST),
    SET_DRIVE(UAB,          DISABLE, ENABLE, DIV_1, 4, 1, SLOWEST, SLOWEST),
    SET_DRIVE(SDIO3,        DISABLE, ENABLE, DIV_8, 4, 1, FASTEST, FASTEST),
};
#endif // Ends if AVP_PINMUX==0

NvError NvOdmPinmuxInit(NvU32 BoardId)
{
    NvError err = 0;

    err = NvPinmuxConfigTable(vcm30t30_pinmux_avp, NV_ARRAY_SIZE(vcm30t30_pinmux_avp));
    NV_CHECK_PINMUX_ERROR(err);

#if AVP_PINMUX == 0
    err = NvPinmuxConfigTable(vcm30t30_pinmux_common, NV_ARRAY_SIZE(vcm30t30_pinmux_common));
    NV_CHECK_PINMUX_ERROR(err);

    err = NvPinmuxDriveConfigTable(vcm30t30_drive_pinmux,
       NV_ARRAY_SIZE(vcm30t30_drive_pinmux));
    NV_CHECK_PINMUX_ERROR(err);
#endif

fail:
    return err;
}

NvError NvOdmSdmmc3UartPinmuxInit(void)
{
    return NvSuccess;
}
