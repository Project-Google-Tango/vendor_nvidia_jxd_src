/*
 * Copyright 2010 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** @file
 * <b>NVIDIA Tegra ODM Kit:
 *        Pin configurations for NVIDIA T30 processors</b>
 * 
 * @b Description: Defines the names and configurable settings for pin electrical
 *                 attributes, such as drive strength and slew.
 */

// This is an auto-generated file.  Do not edit.

#ifndef INCLUDED_NVODM_QUERY_PINS_T30_H
#define INCLUDED_NVODM_QUERY_PINS_T30_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * This specifies the list of pin configuration registers supported by
 * T30-compatible products.  This should be used to generate the pin
 * pin-attribute query array.
 * @see NvOdmQueryPinAttributes.
 * @ingroup nvodm_pins
 * @{
 */

typedef enum
{

    /// Pin configuration registers for NVIDIA T30 products
    NvOdmPinRegister_T30_PadCtrl_AOCFG1 = 0x21000868UL,
    NvOdmPinRegister_T30_PadCtrl_AOCFG2 = 0x2100086CUL,
    NvOdmPinRegister_T30_PadCtrl_ATCFG1 = 0x21000870UL,
    NvOdmPinRegister_T30_PadCtrl_ATCFG2 = 0x21000874UL,
    NvOdmPinRegister_T30_PadCtrl_ATCFG3 = 0x21000878UL,
    NvOdmPinRegister_T30_PadCtrl_ATCFG4 = 0x2100087CUL,
    NvOdmPinRegister_T30_PadCtrl_ATCFG5 = 0x21000880UL,
    NvOdmPinRegister_T30_PadCtrl_CDEV1CFG = 0x21000884UL,
    NvOdmPinRegister_T30_PadCtrl_CDEV2CFG = 0x21000888UL,
    NvOdmPinRegister_T30_PadCtrl_CSUSCFG = 0x2100088CUL,
    NvOdmPinRegister_T30_PadCtrl_DAP1CFG = 0x21000890UL,
    NvOdmPinRegister_T30_PadCtrl_DAP2CFG = 0x21000894UL,
    NvOdmPinRegister_T30_PadCtrl_DAP3CFG = 0x21000898UL,
    NvOdmPinRegister_T30_PadCtrl_DAP4CFG = 0x2100089CUL,
    NvOdmPinRegister_T30_PadCtrl_DBGCFG = 0x210008A0UL,
    NvOdmPinRegister_T30_PadCtrl_LCDCFG1 = 0x210008A4UL,
    NvOdmPinRegister_T30_PadCtrl_LCDCFG2 = 0x210008A8UL,
    NvOdmPinRegister_T30_PadCtrl_SDIO2CFG = 0x210008ACUL,
    NvOdmPinRegister_T30_PadCtrl_SDIO3CFG = 0x210008B0UL,
    NvOdmPinRegister_T30_PadCtrl_SPICFG = 0x210008B4UL,
    NvOdmPinRegister_T30_PadCtrl_UAACFG = 0x210008B8UL,
    NvOdmPinRegister_T30_PadCtrl_UABCFG = 0x210008BCUL,
    NvOdmPinRegister_T30_PadCtrl_UART2CFG = 0x210008C0UL,
    NvOdmPinRegister_T30_PadCtrl_UART3CFG = 0x210008C4UL,
    NvOdmPinRegister_T30_PadCtrl_VICFG1 = 0x210008C8UL,
    NvOdmPinRegister_T30_PadCtrl_VIVTTGEN = 0x210008CCUL,
    NvOdmPinRegister_T30_PadCtrl_SDIO1CFG = 0x210008ECUL,
    NvOdmPinRegister_T30_PadCtrl_CRTCFG = 0x210008F8UL,
    NvOdmPinRegister_T30_PadCtrl_DDCCFG = 0x210008FCUL,
    NvOdmPinRegister_T30_PadCtrl_GMACFG = 0x21000900UL,
    NvOdmPinRegister_T30_PadCtrl_GMBCFG = 0x21000904UL,
    NvOdmPinRegister_T30_PadCtrl_GMCCFG = 0x21000908UL,
    NvOdmPinRegister_T30_PadCtrl_GMDCFG = 0x2100090CUL,
    NvOdmPinRegister_T30_PadCtrl_GMECFG = 0x21000910UL,
    NvOdmPinRegister_T30_PadCtrl_GMFCFG = 0x21000914UL,
    NvOdmPinRegister_T30_PadCtrl_GMGCFG = 0x21000918UL,
    NvOdmPinRegister_T30_PadCtrl_GMHCFG = 0x2100091CUL,
    NvOdmPinRegister_T30_PadCtrl_OWRCFG = 0x21000920UL,
    NvOdmPinRegister_T30_PadCtrl_UADCFG = 0x21000924UL,
    NvOdmPinRegister_T30_PadCtrl_GPVCFG = 0x21000928UL,
    NvOdmPinRegister_T30_PadCtrl_DEV3CFG = 0x2100092CUL,
    NvOdmPinRegister_T30_PadCtrl_SDMMC4_VTTGEN = 0x21000930UL,
    NvOdmPinRegister_T30_PadCtrl_NONDDR_LPDDR_ = 0x21000934UL,
    NvOdmPinRegister_T30_PadCtrl_CECCFG = 0x21000938UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA0 = 0x21003000UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA1 = 0x21003004UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA2 = 0x21003008UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA3 = 0x2100300CUL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA4 = 0x21003010UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA5 = 0x21003014UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA6 = 0x21003018UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DATA7 = 0x2100301CUL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_CLK = 0x21003020UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_DIR = 0x21003024UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_NXT = 0x21003028UL,
    NvOdmPinRegister_T30_PullUpDown_ULPI_STP = 0x2100302CUL,
    NvOdmPinRegister_T30_PullUpDown_DAP3_FS = 0x21003030UL,
    NvOdmPinRegister_T30_PullUpDown_DAP3_DIN = 0x21003034UL,
    NvOdmPinRegister_T30_PullUpDown_DAP3_DOUT = 0x21003038UL,
    NvOdmPinRegister_T30_PullUpDown_DAP3_SCLK = 0x2100303CUL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PV0 = 0x21003040UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PV1 = 0x21003044UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_CLK = 0x21003048UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_CMD = 0x2100304CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_DAT3 = 0x21003050UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_DAT2 = 0x21003054UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_DAT1 = 0x21003058UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC1_DAT0 = 0x2100305CUL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PV2 = 0x21003060UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PV3 = 0x21003064UL,
    NvOdmPinRegister_T30_PullUpDown_CLK2_OUT = 0x21003068UL,
    NvOdmPinRegister_T30_PullUpDown_CLK2_REQ = 0x2100306CUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_PWR1 = 0x21003070UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_PWR2 = 0x21003074UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_SDIN = 0x21003078UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_SDOUT = 0x2100307CUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_WR_N = 0x21003080UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_CS0_N = 0x21003084UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_DC0 = 0x21003088UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_SCK = 0x2100308CUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_PWR0 = 0x21003090UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_PCLK = 0x21003094UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_DE = 0x21003098UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_HSYNC = 0x2100309CUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_VSYNC = 0x210030A0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D0 = 0x210030A4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D1 = 0x210030A8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D2 = 0x210030ACUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D3 = 0x210030B0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D4 = 0x210030B4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D5 = 0x210030B8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D6 = 0x210030BCUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D7 = 0x210030C0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D8 = 0x210030C4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D9 = 0x210030C8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D10 = 0x210030CCUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D11 = 0x210030D0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D12 = 0x210030D4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D13 = 0x210030D8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D14 = 0x210030DCUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D15 = 0x210030E0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D16 = 0x210030E4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D17 = 0x210030E8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D18 = 0x210030ECUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D19 = 0x210030F0UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D20 = 0x210030F4UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D21 = 0x210030F8UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D22 = 0x210030FCUL,
    NvOdmPinRegister_T30_PullUpDown_LCD_D23 = 0x21003100UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_CS1_N = 0x21003104UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_M1 = 0x21003108UL,
    NvOdmPinRegister_T30_PullUpDown_LCD_DC1 = 0x2100310CUL,
    NvOdmPinRegister_T30_PullUpDown_HDMI_INT = 0x21003110UL,
    NvOdmPinRegister_T30_PullUpDown_DDC_SCL = 0x21003114UL,
    NvOdmPinRegister_T30_PullUpDown_DDC_SDA = 0x21003118UL,
    NvOdmPinRegister_T30_PullUpDown_CRT_HSYNC = 0x2100311CUL,
    NvOdmPinRegister_T30_PullUpDown_CRT_VSYNC = 0x21003120UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D0 = 0x21003124UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D1 = 0x21003128UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D2 = 0x2100312CUL,
    NvOdmPinRegister_T30_PullUpDown_VI_D3 = 0x21003130UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D4 = 0x21003134UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D5 = 0x21003138UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D6 = 0x2100313CUL,
    NvOdmPinRegister_T30_PullUpDown_VI_D7 = 0x21003140UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D8 = 0x21003144UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D9 = 0x21003148UL,
    NvOdmPinRegister_T30_PullUpDown_VI_D10 = 0x2100314CUL,
    NvOdmPinRegister_T30_PullUpDown_VI_D11 = 0x21003150UL,
    NvOdmPinRegister_T30_PullUpDown_VI_PCLK = 0x21003154UL,
    NvOdmPinRegister_T30_PullUpDown_VI_MCLK = 0x21003158UL,
    NvOdmPinRegister_T30_PullUpDown_VI_VSYNC = 0x2100315CUL,
    NvOdmPinRegister_T30_PullUpDown_VI_HSYNC = 0x21003160UL,
    NvOdmPinRegister_T30_PullUpDown_UART2_RXD = 0x21003164UL,
    NvOdmPinRegister_T30_PullUpDown_UART2_TXD = 0x21003168UL,
    NvOdmPinRegister_T30_PullUpDown_UART2_RTS_N = 0x2100316CUL,
    NvOdmPinRegister_T30_PullUpDown_UART2_CTS_N = 0x21003170UL,
    NvOdmPinRegister_T30_PullUpDown_UART3_TXD = 0x21003174UL,
    NvOdmPinRegister_T30_PullUpDown_UART3_RXD = 0x21003178UL,
    NvOdmPinRegister_T30_PullUpDown_UART3_CTS_N = 0x2100317CUL,
    NvOdmPinRegister_T30_PullUpDown_UART3_RTS_N = 0x21003180UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU0 = 0x21003184UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU1 = 0x21003188UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU2 = 0x2100318CUL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU3 = 0x21003190UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU4 = 0x21003194UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU5 = 0x21003198UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PU6 = 0x2100319CUL,
    NvOdmPinRegister_T30_PullUpDown_GEN1_I2C_SDA = 0x210031A0UL,
    NvOdmPinRegister_T30_PullUpDown_GEN1_I2C_SCL = 0x210031A4UL,
    NvOdmPinRegister_T30_PullUpDown_DAP4_FS = 0x210031A8UL,
    NvOdmPinRegister_T30_PullUpDown_DAP4_DIN = 0x210031ACUL,
    NvOdmPinRegister_T30_PullUpDown_DAP4_DOUT = 0x210031B0UL,
    NvOdmPinRegister_T30_PullUpDown_DAP4_SCLK = 0x210031B4UL,
    NvOdmPinRegister_T30_PullUpDown_CLK3_OUT = 0x210031B8UL,
    NvOdmPinRegister_T30_PullUpDown_CLK3_REQ = 0x210031BCUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_WP_N = 0x210031C0UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_IORDY = 0x210031C4UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_WAIT = 0x210031C8UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_ADV_N = 0x210031CCUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CLK = 0x210031D0UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS0_N = 0x210031D4UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS1_N = 0x210031D8UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS2_N = 0x210031DCUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS3_N = 0x210031E0UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS4_N = 0x210031E4UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS6_N = 0x210031E8UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_CS7_N = 0x210031ECUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD0 = 0x210031F0UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD1 = 0x210031F4UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD2 = 0x210031F8UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD3 = 0x210031FCUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD4 = 0x21003200UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD5 = 0x21003204UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD6 = 0x21003208UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD7 = 0x2100320CUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD8 = 0x21003210UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD9 = 0x21003214UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD10 = 0x21003218UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD11 = 0x2100321CUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD12 = 0x21003220UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD13 = 0x21003224UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD14 = 0x21003228UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_AD15 = 0x2100322CUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_A16 = 0x21003230UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_A17 = 0x21003234UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_A18 = 0x21003238UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_A19 = 0x2100323CUL,
    NvOdmPinRegister_T30_PullUpDown_GMI_WR_N = 0x21003240UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_OE_N = 0x21003244UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_DQS = 0x21003248UL,
    NvOdmPinRegister_T30_PullUpDown_GMI_RST_N = 0x2100324CUL,
    NvOdmPinRegister_T30_PullUpDown_GEN2_I2C_SCL = 0x21003250UL,
    NvOdmPinRegister_T30_PullUpDown_GEN2_I2C_SDA = 0x21003254UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_CLK = 0x21003258UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_CMD = 0x2100325CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT0 = 0x21003260UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT1 = 0x21003264UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT2 = 0x21003268UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT3 = 0x2100326CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT4 = 0x21003270UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT5 = 0x21003274UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT6 = 0x21003278UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_DAT7 = 0x2100327CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC4_RST_N = 0x21003280UL,
    NvOdmPinRegister_T30_PullUpDown_CAM_MCLK = 0x21003284UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PCC1 = 0x21003288UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB0 = 0x2100328CUL,
    NvOdmPinRegister_T30_PullUpDown_CAM_I2C_SCL = 0x21003290UL,
    NvOdmPinRegister_T30_PullUpDown_CAM_I2C_SDA = 0x21003294UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB3 = 0x21003298UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB4 = 0x2100329CUL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB5 = 0x210032A0UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB6 = 0x210032A4UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PBB7 = 0x210032A8UL,
    NvOdmPinRegister_T30_PullUpDown_GPIO_PCC2 = 0x210032ACUL,
    NvOdmPinRegister_T30_PullUpDown_JTAG_RTCK = 0x210032B0UL,
    NvOdmPinRegister_T30_PullUpDown_PWR_I2C_SCL = 0x210032B4UL,
    NvOdmPinRegister_T30_PullUpDown_PWR_I2C_SDA = 0x210032B8UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW0 = 0x210032BCUL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW1 = 0x210032C0UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW2 = 0x210032C4UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW3 = 0x210032C8UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW4 = 0x210032CCUL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW5 = 0x210032D0UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW6 = 0x210032D4UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW7 = 0x210032D8UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW8 = 0x210032DCUL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW9 = 0x210032E0UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW10 = 0x210032E4UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW11 = 0x210032E8UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW12 = 0x210032ECUL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW13 = 0x210032F0UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW14 = 0x210032F4UL,
    NvOdmPinRegister_T30_PullUpDown_KB_ROW15 = 0x210032F8UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL0 = 0x210032FCUL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL1 = 0x21003300UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL2 = 0x21003304UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL3 = 0x21003308UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL4 = 0x2100330CUL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL5 = 0x21003310UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL6 = 0x21003314UL,
    NvOdmPinRegister_T30_PullUpDown_KB_COL7 = 0x21003318UL,
    NvOdmPinRegister_T30_PullUpDown_CLK_32K_OUT = 0x2100331CUL,
    NvOdmPinRegister_T30_PullUpDown_SYS_CLK_REQ = 0x21003320UL,
    NvOdmPinRegister_T30_PullUpDown_CORE_PWR_REQ = 0x21003324UL,
    NvOdmPinRegister_T30_PullUpDown_CPU_PWR_REQ = 0x21003328UL,
    NvOdmPinRegister_T30_PullUpDown_PWR_INT_N = 0x2100332CUL,
    NvOdmPinRegister_T30_PullUpDown_CLK_32K_IN = 0x21003330UL,
    NvOdmPinRegister_T30_PullUpDown_OWR = 0x21003334UL,
    NvOdmPinRegister_T30_PullUpDown_DAP1_FS = 0x21003338UL,
    NvOdmPinRegister_T30_PullUpDown_DAP1_DIN = 0x2100333CUL,
    NvOdmPinRegister_T30_PullUpDown_DAP1_DOUT = 0x21003340UL,
    NvOdmPinRegister_T30_PullUpDown_DAP1_SCLK = 0x21003344UL,
    NvOdmPinRegister_T30_PullUpDown_CLK1_REQ = 0x21003348UL,
    NvOdmPinRegister_T30_PullUpDown_CLK1_OUT = 0x2100334CUL,
    NvOdmPinRegister_T30_PullUpDown_SPDIF_IN = 0x21003350UL,
    NvOdmPinRegister_T30_PullUpDown_SPDIF_OUT = 0x21003354UL,
    NvOdmPinRegister_T30_PullUpDown_DAP2_FS = 0x21003358UL,
    NvOdmPinRegister_T30_PullUpDown_DAP2_DIN = 0x2100335CUL,
    NvOdmPinRegister_T30_PullUpDown_DAP2_DOUT = 0x21003360UL,
    NvOdmPinRegister_T30_PullUpDown_DAP2_SCLK = 0x21003364UL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_MOSI = 0x21003368UL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_MISO = 0x2100336CUL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_CS0_N = 0x21003370UL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_SCK = 0x21003374UL,
    NvOdmPinRegister_T30_PullUpDown_SPI1_MOSI = 0x21003378UL,
    NvOdmPinRegister_T30_PullUpDown_SPI1_SCK = 0x2100337CUL,
    NvOdmPinRegister_T30_PullUpDown_SPI1_CS0_N = 0x21003380UL,
    NvOdmPinRegister_T30_PullUpDown_SPI1_MISO = 0x21003384UL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_CS1_N = 0x21003388UL,
    NvOdmPinRegister_T30_PullUpDown_SPI2_CS2_N = 0x2100338CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_CLK = 0x21003390UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_CMD = 0x21003394UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT0 = 0x21003398UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT1 = 0x2100339CUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT2 = 0x210033A0UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT3 = 0x210033A4UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT4 = 0x210033A8UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT5 = 0x210033ACUL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT6 = 0x210033B0UL,
    NvOdmPinRegister_T30_PullUpDown_SDMMC3_DAT7 = 0x210033B4UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L0_PRSNT_N = 0x210033B8UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L0_RST_N = 0x210033BCUL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L0_CLKREQ_N = 0x210033C0UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_WAKE_N = 0x210033C4UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L1_PRSNT_N = 0x210033C8UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L1_RST_N = 0x210033CCUL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L1_CLKREQ_N = 0x210033D0UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L2_PRSNT_N = 0x210033D4UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L2_RST_N = 0x210033D8UL,
    NvOdmPinRegister_T30_PullUpDown_PEX_L2_CLKREQ_N = 0x210033DCUL,
    NvOdmPinRegister_T30_PullUpDown_HDMI_CEC = 0x210033E0UL,

    NvOdmPinRegister_Force32 = 0x7fffffffUL,
} NvOdmPinRegister;

/*
 * C pre-processor macros are provided below to help ODMs specify
 * pin electrical attributes in a more readable and maintainable fashion
 * than hardcoding hexadecimal numbers directly.  Please refer to the
 * Electrical, Thermal and Mechanical data sheet for your product for more
 * detailed information regarding the effects these values have
 */

/**
 * Use this macro to program the PadCtrl_AOCFG1, PadCtrl_AOCFG2, 
 * PadCtrl_CDEV1CFG, PadCtrl_CDEV2CFG, PadCtrl_DAP1CFG, PadCtrl_DAP2CFG, 
 * PadCtrl_DAP3CFG, PadCtrl_DAP4CFG, PadCtrl_DBGCFG, PadCtrl_LCDCFG1, 
 * PadCtrl_LCDCFG2, PadCtrl_SPICFG, PadCtrl_UAACFG, PadCtrl_UABCFG, 
 * PadCtrl_UART2CFG, PadCtrl_UART3CFG, PadCtrl_CRTCFG, PadCtrl_DDCCFG, 
 * PadCtrl_OWRCFG, PadCtrl_UADCFG, PadCtrl_GPVCFG, PadCtrl_DEV3CFG and 
 * PadCtrl_CECCFG registers.
 *
 * @param HSM_EN : Enable high-speed mode (0 = disable).  Valid Range 0 - 1
 * @param SCHMT_EN : Schmitt trigger enable (0 = disable).  Valid Range 0 - 1
 * @param LPMD : Low-power current/impedance selection (0 = 400 ohm, 1 = 200 ohm, 2 = 100 ohm, 3 = 50 ohm).  Valid Range 0 - 3
 * @param CAL_DRVDN : Pull-down drive strength.  Valid Range 0 - 31
 * @param CAL_DRVUP : Pull-up drive strength.  Valid Range 0 - 31
 * @param CAL_DRVDN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 3
 * @param CAL_DRVUP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 3
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(HSM_EN, SCHMT_EN, LPMD, CAL_DRVDN, CAL_DRVUP, CAL_DRVDN_SLWR, CAL_DRVUP_SLWF) \
    ((((HSM_EN)&1UL) << 2) | (((SCHMT_EN)&1UL) << 3) | (((LPMD)&3UL) << 4) | \
     (((CAL_DRVDN)&31UL) << 12) | (((CAL_DRVUP)&31UL) << 20) | \
     (((CAL_DRVDN_SLWR)&3UL) << 28) | (((CAL_DRVUP_SLWF)&3UL) << 30))

/**
 * Use this macro to program the PadCtrl_ATCFG1 and PadCtrl_ATCFG2 registers.
 *
 * @param HSM_EN : Enable high-speed mode (0 = disable).  Valid Range 0 - 1
 * @param SCHMT_EN : Schmitt trigger enable (0 = disable).  Valid Range 0 - 1
 * @param LPMD : Low-power current/impedance selection (0 = 400 ohm, 1 = 200 ohm, 2 = 100 ohm, 3 = 50 ohm).  Valid Range 0 - 3
 * @param CAL_DRVDN : Pull-down drive strength.  Valid Range 0 - 31
 * @param CAL_DRVUP : Pull-up drive strength.  Valid Range 0 - 31
 * @param CAL_DRVDN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 15
 * @param CAL_DRVUP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 15
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_ATCFG1(HSM_EN, SCHMT_EN, LPMD, CAL_DRVDN, CAL_DRVUP, CAL_DRVDN_SLWR, CAL_DRVUP_SLWF) \
    ((((HSM_EN)&1UL) << 2) | (((SCHMT_EN)&1UL) << 3) | (((LPMD)&3UL) << 4) | \
     (((CAL_DRVDN)&31UL) << 14) | (((CAL_DRVUP)&31UL) << 19) | \
     (((CAL_DRVDN_SLWR)&15UL) << 24) | (((CAL_DRVUP_SLWF)&15UL) << 28))

/**
 * Use this macro to program the PadCtrl_ATCFG3, PadCtrl_ATCFG4, 
 * PadCtrl_ATCFG5, PadCtrl_GMECFG, PadCtrl_GMFCFG, PadCtrl_GMGCFG and 
 * PadCtrl_GMHCFG registers.
 *
 * @param HSM_EN : Enable high-speed mode (0 = disable).  Valid Range 0 - 1
 * @param SCHMT_EN : Schmitt trigger enable (0 = disable).  Valid Range 0 - 1
 * @param LPMD : Low-power current/impedance selection (0 = 400 ohm, 1 = 200 ohm, 2 = 100 ohm, 3 = 50 ohm).  Valid Range 0 - 3
 * @param CAL_DRVDN : Pull-down drive strength.  Valid Range 0 - 31
 * @param CAL_DRVUP : Pull-up drive strength.  Valid Range 0 - 31
 * @param CAL_DRVDN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 3
 * @param CAL_DRVUP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 3
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(HSM_EN, SCHMT_EN, LPMD, CAL_DRVDN, CAL_DRVUP, CAL_DRVDN_SLWR, CAL_DRVUP_SLWF) \
    ((((HSM_EN)&1UL) << 2) | (((SCHMT_EN)&1UL) << 3) | (((LPMD)&3UL) << 4) | \
     (((CAL_DRVDN)&31UL) << 14) | (((CAL_DRVUP)&31UL) << 19) | \
     (((CAL_DRVDN_SLWR)&3UL) << 28) | (((CAL_DRVUP_SLWF)&3UL) << 30))

/**
 * Use this macro to program the PadCtrl_CSUSCFG register.
 *
 * @param DN : Pull-down drive strength.  Valid Range 0 - 31
 * @param UP : Pull-up drive strength.  Valid Range 0 - 31
 * @param DN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 15
 * @param UP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 15
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_CSUSCFG(DN, UP, DN_SLWR, UP_SLWF) \
    ((((DN)&31UL) << 12) | (((UP)&31UL) << 19) | (((DN_SLWR)&15UL) << 24) | \
     (((UP_SLWF)&15UL) << 28))

/**
 * Use this macro to program the PadCtrl_SDIO2CFG, PadCtrl_SDIO3CFG and 
 * PadCtrl_SDIO1CFG registers.
 *
 * @param HSM_EN : Enable high-speed mode (0 = disable).  Valid Range 0 - 1
 * @param SCHMT_EN : Schmitt trigger enable (0 = disable).  Valid Range 0 - 1
 * @param CAL_DRVDN : Pull-down drive strength.  Valid Range 0 - 127
 * @param CAL_DRVUP : Pull-up drive strength.  Valid Range 0 - 127
 * @param CAL_DRVDN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 3
 * @param CAL_DRVUP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 3
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_SDIO2CFG(HSM_EN, SCHMT_EN, CAL_DRVDN, CAL_DRVUP, CAL_DRVDN_SLWR, CAL_DRVUP_SLWF) \
    ((((HSM_EN)&1UL) << 2) | (((SCHMT_EN)&1UL) << 3) | \
     (((CAL_DRVDN)&127UL) << 12) | (((CAL_DRVUP)&127UL) << 20) | \
     (((CAL_DRVDN_SLWR)&3UL) << 28) | (((CAL_DRVUP_SLWF)&3UL) << 30))

/**
 * Use this macro to program the PadCtrl_VICFG1, PadCtrl_GMACFG, 
 * PadCtrl_GMBCFG, PadCtrl_GMCCFG and PadCtrl_GMDCFG registers.
 *
 * @param DN : Pull-down drive strength.  Valid Range 0 - 31
 * @param UP : Pull-up drive strength.  Valid Range 0 - 31
 * @param DN_SLWR : Pull-up slew control (0 = max).  Valid Range 0 - 15
 * @param UP_SLWF : Pull-down slew control (0 = max).  Valid Range 0 - 15
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_VICFG1(DN, UP, DN_SLWR, UP_SLWF) \
    ((((DN)&31UL) << 14) | (((UP)&31UL) << 19) | (((DN_SLWR)&15UL) << 24) | \
     (((UP_SLWF)&15UL) << 28))

/**
 * Use this macro to program the PadCtrl_VIVTTGEN register.
 *
 * @param SHORT : .  Valid Range 0 - 1
 * @param SHORT_PWRGND : .  Valid Range 0 - 1
 * @param E_PWRD : .  Valid Range 0 - 1
 * @param VCLAMP_LEVEL : .  Valid Range 0 - 7
 * @param VAUXP_LEVEL : .  Valid Range 0 - 7
 * @param DRVDN : Pull-down drive strength.  Valid Range 0 - 7
 * @param DRVUP : Pull-up drive strength.  Valid Range 0 - 7
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_VIVTTGEN(SHORT, SHORT_PWRGND, E_PWRD, VCLAMP_LEVEL, VAUXP_LEVEL, DRVDN, DRVUP) \
    ((((SHORT)&1UL) << 0) | (((SHORT_PWRGND)&1UL) << 1) | (((E_PWRD)&1UL) << 5) | \
     (((VCLAMP_LEVEL)&7UL) << 8) | (((VAUXP_LEVEL)&7UL) << 12) | \
     (((DRVDN)&7UL) << 16) | (((DRVUP)&7UL) << 24))

/**
 * Use this macro to program the PadCtrl_SDMMC4_VTTGEN register.
 *
 * @param SHORT : .  Valid Range 0 - 1
 * @param SHORT_PWRGND : .  Valid Range 0 - 1
 * @param E_DDR3 : .  Valid Range 0 - 1
 * @param VCLAMP_LEVEL : .  Valid Range 0 - 7
 * @param VAUXP_LEVEL : .  Valid Range 0 - 7
 * @param DRVDN : Pull-down drive strength.  Valid Range 0 - 7
 * @param DRVUP : Pull-up drive strength.  Valid Range 0 - 7
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_SDMMC4_VTTGEN(SHORT, SHORT_PWRGND, E_DDR3, VCLAMP_LEVEL, VAUXP_LEVEL, DRVDN, DRVUP) \
    ((((SHORT)&1UL) << 0) | (((SHORT_PWRGND)&1UL) << 1) | (((E_DDR3)&1UL) << 2) | \
     (((VCLAMP_LEVEL)&7UL) << 8) | (((VAUXP_LEVEL)&7UL) << 12) | \
     (((DRVDN)&7UL) << 16) | (((DRVUP)&7UL) << 24))

/**
 * Use this macro to program the PadCtrl_NONDDR_LPDDR_ register.
 *
 * @param DVI_E_GPIO : .  Valid Range 0 - 1
 * @param DVI_E_BYPASS : .  Valid Range 0 - 1
 * @param GMI_E_GPIO : .  Valid Range 0 - 1
 * @param GMI_E_BYPASS : .  Valid Range 0 - 1
 */

#define NVODM_QUERY_PIN_T30_PADCTRL_NONDDR_LPDDR_(DVI_E_GPIO, DVI_E_BYPASS, GMI_E_GPIO, GMI_E_BYPASS) \
    ((((DVI_E_GPIO)&1UL) << 0) | (((DVI_E_BYPASS)&1UL) << 1) | \
     (((GMI_E_GPIO)&1UL) << 8) | (((GMI_E_BYPASS)&1UL) << 9))

/**
 * Use this macro to program the PullUpDown_ULPI_DATA0, 
 * PullUpDown_ULPI_DATA1, PullUpDown_ULPI_DATA2, PullUpDown_ULPI_DATA3, 
 * PullUpDown_ULPI_DATA4, PullUpDown_ULPI_DATA5, PullUpDown_ULPI_DATA6, 
 * PullUpDown_ULPI_DATA7, PullUpDown_ULPI_CLK, PullUpDown_ULPI_DIR, 
 * PullUpDown_ULPI_NXT, PullUpDown_ULPI_STP, PullUpDown_DAP3_FS, 
 * PullUpDown_DAP3_DIN, PullUpDown_DAP3_DOUT, PullUpDown_DAP3_SCLK, 
 * PullUpDown_GPIO_PV0, PullUpDown_GPIO_PV1, PullUpDown_SDMMC1_CLK, 
 * PullUpDown_SDMMC1_CMD, PullUpDown_SDMMC1_DAT3, PullUpDown_SDMMC1_DAT2, 
 * PullUpDown_SDMMC1_DAT1, PullUpDown_SDMMC1_DAT0, PullUpDown_GPIO_PV2, 
 * PullUpDown_GPIO_PV3, PullUpDown_CLK2_OUT, PullUpDown_CLK2_REQ, 
 * PullUpDown_LCD_PWR1, PullUpDown_LCD_PWR2, PullUpDown_LCD_SDIN, 
 * PullUpDown_LCD_SDOUT, PullUpDown_LCD_WR_N, PullUpDown_LCD_CS0_N, 
 * PullUpDown_LCD_DC0, PullUpDown_LCD_SCK, PullUpDown_LCD_PWR0, 
 * PullUpDown_LCD_PCLK, PullUpDown_LCD_DE, PullUpDown_LCD_HSYNC, 
 * PullUpDown_LCD_VSYNC, PullUpDown_LCD_D0, PullUpDown_LCD_D1, 
 * PullUpDown_LCD_D2, PullUpDown_LCD_D3, PullUpDown_LCD_D4, PullUpDown_LCD_D5, 
 * PullUpDown_LCD_D6, PullUpDown_LCD_D7, PullUpDown_LCD_D8, PullUpDown_LCD_D9, 
 * PullUpDown_LCD_D10, PullUpDown_LCD_D11, PullUpDown_LCD_D12, 
 * PullUpDown_LCD_D13, PullUpDown_LCD_D14, PullUpDown_LCD_D15, 
 * PullUpDown_LCD_D16, PullUpDown_LCD_D17, PullUpDown_LCD_D18, 
 * PullUpDown_LCD_D19, PullUpDown_LCD_D20, PullUpDown_LCD_D21, 
 * PullUpDown_LCD_D22, PullUpDown_LCD_D23, PullUpDown_LCD_CS1_N, 
 * PullUpDown_LCD_M1, PullUpDown_LCD_DC1, PullUpDown_HDMI_INT, 
 * PullUpDown_DDC_SCL, PullUpDown_DDC_SDA, PullUpDown_CRT_HSYNC, 
 * PullUpDown_CRT_VSYNC, PullUpDown_UART2_RXD, PullUpDown_UART2_TXD, 
 * PullUpDown_UART2_RTS_N, PullUpDown_UART2_CTS_N, PullUpDown_UART3_TXD, 
 * PullUpDown_UART3_RXD, PullUpDown_UART3_CTS_N, PullUpDown_UART3_RTS_N, 
 * PullUpDown_GPIO_PU0, PullUpDown_GPIO_PU1, PullUpDown_GPIO_PU2, 
 * PullUpDown_GPIO_PU3, PullUpDown_GPIO_PU4, PullUpDown_GPIO_PU5, 
 * PullUpDown_GPIO_PU6, PullUpDown_DAP4_FS, PullUpDown_DAP4_DIN, 
 * PullUpDown_DAP4_DOUT, PullUpDown_DAP4_SCLK, PullUpDown_CLK3_OUT, 
 * PullUpDown_CLK3_REQ, PullUpDown_GMI_WP_N, PullUpDown_GMI_IORDY, 
 * PullUpDown_GMI_WAIT, PullUpDown_GMI_ADV_N, PullUpDown_GMI_CLK, 
 * PullUpDown_GMI_CS0_N, PullUpDown_GMI_CS1_N, PullUpDown_GMI_CS2_N, 
 * PullUpDown_GMI_CS3_N, PullUpDown_GMI_CS4_N, PullUpDown_GMI_CS6_N, 
 * PullUpDown_GMI_CS7_N, PullUpDown_GMI_AD0, PullUpDown_GMI_AD1, 
 * PullUpDown_GMI_AD2, PullUpDown_GMI_AD3, PullUpDown_GMI_AD4, 
 * PullUpDown_GMI_AD5, PullUpDown_GMI_AD6, PullUpDown_GMI_AD7, 
 * PullUpDown_GMI_AD8, PullUpDown_GMI_AD9, PullUpDown_GMI_AD10, 
 * PullUpDown_GMI_AD11, PullUpDown_GMI_AD12, PullUpDown_GMI_AD13, 
 * PullUpDown_GMI_AD14, PullUpDown_GMI_AD15, PullUpDown_GMI_A16, 
 * PullUpDown_GMI_A17, PullUpDown_GMI_A18, PullUpDown_GMI_A19, 
 * PullUpDown_GMI_WR_N, PullUpDown_GMI_OE_N, PullUpDown_GMI_DQS, 
 * PullUpDown_GMI_RST_N, PullUpDown_CAM_MCLK, PullUpDown_GPIO_PCC1, 
 * PullUpDown_GPIO_PBB0, PullUpDown_GPIO_PBB3, PullUpDown_GPIO_PBB4, 
 * PullUpDown_GPIO_PBB5, PullUpDown_GPIO_PBB6, PullUpDown_GPIO_PBB7, 
 * PullUpDown_GPIO_PCC2, PullUpDown_JTAG_RTCK, PullUpDown_KB_ROW0, 
 * PullUpDown_KB_ROW1, PullUpDown_KB_ROW2, PullUpDown_KB_ROW3, 
 * PullUpDown_KB_ROW4, PullUpDown_KB_ROW5, PullUpDown_KB_ROW6, 
 * PullUpDown_KB_ROW7, PullUpDown_KB_ROW8, PullUpDown_KB_ROW9, 
 * PullUpDown_KB_ROW10, PullUpDown_KB_ROW11, PullUpDown_KB_ROW12, 
 * PullUpDown_KB_ROW13, PullUpDown_KB_ROW14, PullUpDown_KB_ROW15, 
 * PullUpDown_KB_COL0, PullUpDown_KB_COL1, PullUpDown_KB_COL2, 
 * PullUpDown_KB_COL3, PullUpDown_KB_COL4, PullUpDown_KB_COL5, 
 * PullUpDown_KB_COL6, PullUpDown_KB_COL7, PullUpDown_CLK_32K_OUT, 
 * PullUpDown_SYS_CLK_REQ, PullUpDown_CORE_PWR_REQ, PullUpDown_CPU_PWR_REQ, 
 * PullUpDown_PWR_INT_N, PullUpDown_CLK_32K_IN, PullUpDown_OWR, 
 * PullUpDown_DAP1_FS, PullUpDown_DAP1_DIN, PullUpDown_DAP1_DOUT, 
 * PullUpDown_DAP1_SCLK, PullUpDown_CLK1_REQ, PullUpDown_CLK1_OUT, 
 * PullUpDown_SPDIF_IN, PullUpDown_SPDIF_OUT, PullUpDown_DAP2_FS, 
 * PullUpDown_DAP2_DIN, PullUpDown_DAP2_DOUT, PullUpDown_DAP2_SCLK, 
 * PullUpDown_SPI2_MOSI, PullUpDown_SPI2_MISO, PullUpDown_SPI2_CS0_N, 
 * PullUpDown_SPI2_SCK, PullUpDown_SPI1_MOSI, PullUpDown_SPI1_SCK, 
 * PullUpDown_SPI1_CS0_N, PullUpDown_SPI1_MISO, PullUpDown_SPI2_CS1_N, 
 * PullUpDown_SPI2_CS2_N, PullUpDown_SDMMC3_CLK, PullUpDown_SDMMC3_CMD, 
 * PullUpDown_SDMMC3_DAT0, PullUpDown_SDMMC3_DAT1, PullUpDown_SDMMC3_DAT2, 
 * PullUpDown_SDMMC3_DAT3, PullUpDown_SDMMC3_DAT4, PullUpDown_SDMMC3_DAT5, 
 * PullUpDown_SDMMC3_DAT6, PullUpDown_SDMMC3_DAT7, PullUpDown_PEX_L0_PRSNT_N, 
 * PullUpDown_PEX_L0_RST_N, PullUpDown_PEX_L0_CLKREQ_N, PullUpDown_PEX_WAKE_N, 
 * PullUpDown_PEX_L1_PRSNT_N, PullUpDown_PEX_L1_RST_N, 
 * PullUpDown_PEX_L1_CLKREQ_N, PullUpDown_PEX_L2_PRSNT_N, 
 * PullUpDown_PEX_L2_RST_N and PullUpDown_PEX_L2_CLKREQ_N registers.
 *
 * @param PM : Pinmux optioni set 0.  Valid Range 0 - 3
 * @param PUPD : Configure internal pull-up/down (0 = normal, 1 = pull-down, 2 = pull-up).  Valid Range 0 - 3
 * @param TRISTATE : Tristate enable (1)/disable(0): Set to 0.  Valid Range 0 - 1
 * @param E_INPUT : Input enable , set to 0.  Valid Range 0 - 1
 * @param LOCK : Lock disable/enable(1) , set to 0.  Valid Range 0 - 1
 */

#define NVODM_QUERY_PIN_T30_PULLUPDOWN_ULPI_DATA0(PM, PUPD, TRISTATE, E_INPUT, LOCK) \
    ((((PM)&3UL) << 0) | (((PUPD)&3UL) << 2) | (((TRISTATE)&1UL) << 4) | \
     (((E_INPUT)&1UL) << 5) | (((LOCK)&1UL) << 7))

/**
 * Use this macro to program the PullUpDown_VI_D0, PullUpDown_VI_D1, 
 * PullUpDown_VI_D2, PullUpDown_VI_D3, PullUpDown_VI_D4, PullUpDown_VI_D5, 
 * PullUpDown_VI_D6, PullUpDown_VI_D7, PullUpDown_VI_D8, PullUpDown_VI_D9, 
 * PullUpDown_VI_D10, PullUpDown_VI_D11, PullUpDown_VI_PCLK, 
 * PullUpDown_VI_MCLK, PullUpDown_VI_VSYNC, PullUpDown_VI_HSYNC, 
 * PullUpDown_SDMMC4_CLK, PullUpDown_SDMMC4_CMD, PullUpDown_SDMMC4_DAT0, 
 * PullUpDown_SDMMC4_DAT1, PullUpDown_SDMMC4_DAT2, PullUpDown_SDMMC4_DAT3, 
 * PullUpDown_SDMMC4_DAT4, PullUpDown_SDMMC4_DAT5, PullUpDown_SDMMC4_DAT6, 
 * PullUpDown_SDMMC4_DAT7 and PullUpDown_SDMMC4_RST_N registers.
 *
 * @param PM : Pinmux optioni set 0.  Valid Range 0 - 3
 * @param PUPD : Configure internal pull-up/down (0 = normal, 1 = pull-down, 2 = pull-up).  Valid Range 0 - 3
 * @param TRISTATE : Tristate enable (1)/disable(0): Set to 0.  Valid Range 0 - 1
 * @param E_INPUT : Input enable , set to 0.  Valid Range 0 - 1
 * @param LOCK : Lock disable/enable(1) , set to 0.  Valid Range 0 - 1
 * @param IO_RESET : .  Valid Range 0 - 1
 */

#define NVODM_QUERY_PIN_T30_PULLUPDOWN_VI_D0(PM, PUPD, TRISTATE, E_INPUT, LOCK, IO_RESET) \
    ((((PM)&3UL) << 0) | (((PUPD)&3UL) << 2) | (((TRISTATE)&1UL) << 4) | \
     (((E_INPUT)&1UL) << 5) | (((LOCK)&1UL) << 7) | (((IO_RESET)&1UL) << 8))

/**
 * Use this macro to program the PullUpDown_GEN1_I2C_SDA, 
 * PullUpDown_GEN1_I2C_SCL, PullUpDown_GEN2_I2C_SCL, PullUpDown_GEN2_I2C_SDA, 
 * PullUpDown_CAM_I2C_SCL, PullUpDown_CAM_I2C_SDA, PullUpDown_PWR_I2C_SCL, 
 * PullUpDown_PWR_I2C_SDA and PullUpDown_HDMI_CEC registers.
 *
 * @param PM : Pinmux optioni set 0.  Valid Range 0 - 3
 * @param PUPD : Configure internal pull-up/down (0 = normal, 1 = pull-down, 2 = pull-up).  Valid Range 0 - 3
 * @param TRISTATE : Tristate enable (1)/disable(0): Set to 0.  Valid Range 0 - 1
 * @param E_INPUT : Input enable , set to 0.  Valid Range 0 - 1
 * @param OD : .  Valid Range 0 - 1
 * @param LOCK : Lock disable/enable(1) , set to 0.  Valid Range 0 - 1
 */

#define NVODM_QUERY_PIN_T30_PULLUPDOWN_GEN1_I2C_SDA(PM, PUPD, TRISTATE, E_INPUT, OD, LOCK) \
    ((((PM)&3UL) << 0) | (((PUPD)&3UL) << 2) | (((TRISTATE)&1UL) << 4) | \
     (((E_INPUT)&1UL) << 5) | (((OD)&1UL) << 6) | (((LOCK)&1UL) << 7))

#ifdef __cplusplus
}
#endif

/** @} */
#endif // INCLUDED_NVODM_QUERY_PINS_T30_H

