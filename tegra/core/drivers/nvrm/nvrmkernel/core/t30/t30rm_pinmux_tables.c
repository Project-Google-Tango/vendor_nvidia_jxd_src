/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_structure.h"
#include "nvrm_pinmux.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nvrm_hwintf.h"
#include "t30/arapb_misc.h"
#include "t30/arclk_rst.h"
#include "t30/arapbpm.h"
#include "t30/t30rm_pinmux_utils.h"
#include "nvrm_clocks.h"
#include "nvodm_query_pinmux.h"

/* Define the GPIO port/pin to tristate mappings */

const NvU16 g_T30GpioPadMapping[] =
{
    //  Port A
    GPIO_TRISTATE(CLK_32K_OUT), GPIO_TRISTATE(UART3_CTS_N), GPIO_TRISTATE(DAP2_FS), GPIO_TRISTATE(DAP2_SCLK),
    GPIO_TRISTATE(DAP2_DIN), GPIO_TRISTATE(DAP2_DOUT), GPIO_TRISTATE(SDMMC3_CLK), GPIO_TRISTATE(SDMMC3_CMD),
    //  Port B
    GPIO_TRISTATE(GMI_A17), GPIO_TRISTATE(GMI_A18), GPIO_TRISTATE(LCD_PWR0), GPIO_TRISTATE(LCD_PCLK),
    GPIO_TRISTATE(SDMMC3_DAT3), GPIO_TRISTATE(SDMMC3_DAT2), GPIO_TRISTATE(SDMMC3_DAT1), GPIO_TRISTATE(SDMMC3_DAT0),
    //  Port C
    GPIO_TRISTATE(UART3_RTS_N), GPIO_TRISTATE(LCD_PWR1), GPIO_TRISTATE(UART2_TXD), GPIO_TRISTATE(UART2_RXD),
    GPIO_TRISTATE(GEN1_I2C_SCL), GPIO_TRISTATE(GEN1_I2C_SDA), GPIO_TRISTATE(LCD_PWR2), GPIO_TRISTATE(GMI_WP_N),
    //  Port D
    GPIO_TRISTATE(SDMMC3_DAT5), GPIO_TRISTATE(SDMMC3_DAT4), GPIO_TRISTATE(LCD_DC1), GPIO_TRISTATE(SDMMC3_DAT6),
    GPIO_TRISTATE(SDMMC3_DAT7), GPIO_TRISTATE(VI_D1), GPIO_TRISTATE(VI_VSYNC), GPIO_TRISTATE(VI_HSYNC),
    //  Port E
    GPIO_TRISTATE(LCD_D0), GPIO_TRISTATE(LCD_D1), GPIO_TRISTATE(LCD_D2), GPIO_TRISTATE(LCD_D3),
    GPIO_TRISTATE(LCD_D4), GPIO_TRISTATE(LCD_D5), GPIO_TRISTATE(LCD_D6), GPIO_TRISTATE(LCD_D7),
    //  Port F
    GPIO_TRISTATE(LCD_D8),GPIO_TRISTATE(LCD_D9),GPIO_TRISTATE(LCD_D10), GPIO_TRISTATE(LCD_D11),
    GPIO_TRISTATE(LCD_D12),GPIO_TRISTATE(LCD_D13),GPIO_TRISTATE(LCD_D14), GPIO_TRISTATE(LCD_D15),
    //  Port G
    GPIO_TRISTATE(GMI_AD0), GPIO_TRISTATE(GMI_AD1),GPIO_TRISTATE(GMI_AD2), GPIO_TRISTATE(GMI_AD3),
    GPIO_TRISTATE(GMI_AD4), GPIO_TRISTATE(GMI_AD5),GPIO_TRISTATE(GMI_AD6), GPIO_TRISTATE(GMI_AD7),
    //  Port H
    GPIO_TRISTATE(GMI_AD8), GPIO_TRISTATE(GMI_AD9),GPIO_TRISTATE(GMI_AD10), GPIO_TRISTATE(GMI_AD11),
    GPIO_TRISTATE(GMI_AD12), GPIO_TRISTATE(GMI_AD13),GPIO_TRISTATE(GMI_AD14), GPIO_TRISTATE(GMI_AD15),
    //  Port I
    GPIO_TRISTATE(GMI_WR_N),GPIO_TRISTATE(GMI_OE_N), GPIO_TRISTATE(GMI_DQS), GPIO_TRISTATE(GMI_CS6_N),
    GPIO_TRISTATE(GMI_RST_N),GPIO_TRISTATE(GMI_IORDY), GPIO_TRISTATE(GMI_CS7_N), GPIO_TRISTATE(GMI_WAIT),
    //  Port J
    GPIO_TRISTATE(GMI_CS0_N),GPIO_TRISTATE(LCD_DE), GPIO_TRISTATE(GMI_CS1_N), GPIO_TRISTATE(LCD_HSYNC),
    GPIO_TRISTATE(LCD_VSYNC),GPIO_TRISTATE(UART2_CTS_N), GPIO_TRISTATE(UART2_RTS_N), GPIO_TRISTATE(GMI_A16),
    //  Port K
    GPIO_TRISTATE(GMI_ADV_N),GPIO_TRISTATE(GMI_CLK), GPIO_TRISTATE(GMI_CS4_N), GPIO_TRISTATE(GMI_CS2_N),
    GPIO_TRISTATE(GMI_CS3_N),GPIO_TRISTATE(SPDIF_OUT), GPIO_TRISTATE(SPDIF_IN), GPIO_TRISTATE(GMI_A19),
    //  Port L
    GPIO_TRISTATE(VI_D2),GPIO_TRISTATE(VI_D3), GPIO_TRISTATE(VI_D4), GPIO_TRISTATE(VI_D5),
    GPIO_TRISTATE(VI_D6),GPIO_TRISTATE(VI_D7), GPIO_TRISTATE(VI_D8), GPIO_TRISTATE(VI_D9),
    //  Port M
    GPIO_TRISTATE(LCD_D16),GPIO_TRISTATE(LCD_D17), GPIO_TRISTATE(LCD_D18), GPIO_TRISTATE(LCD_D19),
    GPIO_TRISTATE(LCD_D20),GPIO_TRISTATE(LCD_D21), GPIO_TRISTATE(LCD_D22), GPIO_TRISTATE(LCD_D23),
    //  Port N
    GPIO_TRISTATE(DAP1_FS),GPIO_TRISTATE(DAP1_DIN), GPIO_TRISTATE(DAP1_DOUT), GPIO_TRISTATE(DAP1_SCLK),
    GPIO_TRISTATE(LCD_CS0_N),GPIO_TRISTATE(LCD_SDOUT), GPIO_TRISTATE(LCD_DC0), GPIO_TRISTATE(HDMI_INT),
    //  Port O
    GPIO_TRISTATE(ULPI_DATA7),GPIO_TRISTATE(ULPI_DATA0), GPIO_TRISTATE(ULPI_DATA1), GPIO_TRISTATE(ULPI_DATA2),
    GPIO_TRISTATE(ULPI_DATA3),GPIO_TRISTATE(ULPI_DATA4), GPIO_TRISTATE(ULPI_DATA5), GPIO_TRISTATE(ULPI_DATA6),
    //  Port P
    GPIO_TRISTATE(DAP3_FS),GPIO_TRISTATE(DAP3_DIN), GPIO_TRISTATE(DAP3_DOUT), GPIO_TRISTATE(DAP3_SCLK),
    GPIO_TRISTATE(DAP4_FS),GPIO_TRISTATE(DAP4_DIN), GPIO_TRISTATE(DAP4_DOUT), GPIO_TRISTATE(DAP4_SCLK),
    //  Port Q
    GPIO_TRISTATE(KB_COL0),GPIO_TRISTATE(KB_COL1), GPIO_TRISTATE(KB_COL2), GPIO_TRISTATE(KB_COL3),
    GPIO_TRISTATE(KB_COL4),GPIO_TRISTATE(KB_COL5), GPIO_TRISTATE(KB_COL6), GPIO_TRISTATE(KB_COL7),
    //  Port R
    GPIO_TRISTATE(KB_ROW0),GPIO_TRISTATE(KB_ROW1), GPIO_TRISTATE(KB_ROW2), GPIO_TRISTATE(KB_ROW3),
    GPIO_TRISTATE(KB_ROW4),GPIO_TRISTATE(KB_ROW5), GPIO_TRISTATE(KB_ROW6), GPIO_TRISTATE(KB_ROW7),
    //  Port S
    GPIO_TRISTATE(KB_ROW8),GPIO_TRISTATE(KB_ROW9), GPIO_TRISTATE(KB_ROW10), GPIO_TRISTATE(KB_ROW11),
    GPIO_TRISTATE(KB_ROW12),GPIO_TRISTATE(KB_ROW13), GPIO_TRISTATE(KB_ROW14), GPIO_TRISTATE(KB_ROW15),
    //  Port T
    GPIO_TRISTATE(VI_PCLK), GPIO_TRISTATE(VI_MCLK), GPIO_TRISTATE(VI_D10), GPIO_TRISTATE(VI_D11),
    GPIO_TRISTATE(VI_D0), GPIO_TRISTATE(GEN2_I2C_SCL), GPIO_TRISTATE(GEN2_I2C_SDA), GPIO_TRISTATE(SDMMC4_CMD),
    //  Port U
    GPIO_TRISTATE(GPIO_PU0), GPIO_TRISTATE(GPIO_PU1), GPIO_TRISTATE(GPIO_PU2), GPIO_TRISTATE(GPIO_PU3),
    GPIO_TRISTATE(GPIO_PU4), GPIO_TRISTATE(GPIO_PU5), GPIO_TRISTATE(GPIO_PU6), GPIO_TRISTATE(JTAG_RTCK),
    //  Port V
    GPIO_TRISTATE(GPIO_PV0), GPIO_TRISTATE(GPIO_PV1), GPIO_TRISTATE(GPIO_PV2), GPIO_TRISTATE(GPIO_PV3),
    GPIO_TRISTATE(DDC_SCL), GPIO_TRISTATE(DDC_SDA), GPIO_TRISTATE(CRT_HSYNC), GPIO_TRISTATE(CRT_VSYNC),
    //  Port W
    GPIO_TRISTATE(LCD_CS1_N), GPIO_TRISTATE(LCD_M1), GPIO_TRISTATE(SPI2_CS1_N), GPIO_TRISTATE(SPI2_CS2_N),
    GPIO_TRISTATE(CLK1_OUT), GPIO_TRISTATE(CLK2_OUT),GPIO_TRISTATE(UART3_TXD),GPIO_TRISTATE(UART3_RXD),
    //  Port X
    GPIO_TRISTATE(SPI2_MOSI),GPIO_TRISTATE(SPI2_MISO),GPIO_TRISTATE(SPI2_SCK),GPIO_TRISTATE(SPI2_CS0_N),
    GPIO_TRISTATE(SPI1_MOSI),GPIO_TRISTATE(SPI1_SCK),GPIO_TRISTATE(SPI1_CS0_N),GPIO_TRISTATE(SPI1_MISO),
    //  Port Y
    GPIO_TRISTATE(ULPI_CLK),GPIO_TRISTATE(ULPI_DIR),GPIO_TRISTATE(ULPI_NXT),GPIO_TRISTATE(ULPI_STP),
    GPIO_TRISTATE(SDMMC1_DAT3),GPIO_TRISTATE(SDMMC1_DAT2),GPIO_TRISTATE(SDMMC1_DAT1),GPIO_TRISTATE(SDMMC1_DAT0),
    //  Port Z
    GPIO_TRISTATE(SDMMC1_CLK),GPIO_TRISTATE(SDMMC1_CMD),GPIO_TRISTATE(LCD_SDIN),GPIO_TRISTATE(LCD_WR_N),
    GPIO_TRISTATE(LCD_SCK), GPIO_TRISTATE(SYS_CLK_REQ), GPIO_TRISTATE(PWR_I2C_SCL), GPIO_TRISTATE(PWR_I2C_SDA),
    //  Port AA
    GPIO_TRISTATE(SDMMC4_DAT0), GPIO_TRISTATE(SDMMC4_DAT1), GPIO_TRISTATE(SDMMC4_DAT2), GPIO_TRISTATE(SDMMC4_DAT3),
    GPIO_TRISTATE(SDMMC4_DAT4), GPIO_TRISTATE(SDMMC4_DAT5), GPIO_TRISTATE(SDMMC4_DAT6), GPIO_TRISTATE(SDMMC4_DAT7),
    //  Port BB
    GPIO_TRISTATE(GPIO_PBB0), GPIO_TRISTATE(CAM_I2C_SCL), GPIO_TRISTATE(CAM_I2C_SDA), GPIO_TRISTATE(GPIO_PBB3),
    GPIO_TRISTATE(GPIO_PBB4), GPIO_TRISTATE(GPIO_PBB5), GPIO_TRISTATE(GPIO_PBB6), GPIO_TRISTATE(GPIO_PBB7),
    //  Port CC
    GPIO_TRISTATE(CAM_MCLK), GPIO_TRISTATE(GPIO_PCC1), GPIO_TRISTATE(GPIO_PCC2), GPIO_TRISTATE(SDMMC4_RST_N),
     GPIO_TRISTATE(SDMMC4_CLK), GPIO_TRISTATE(CLK2_REQ), GPIO_TRISTATE(PEX_L2_RST_N), GPIO_TRISTATE(PEX_L2_CLKREQ_N),
    //  Port DD
     GPIO_TRISTATE(PEX_L0_PRSNT_N), GPIO_TRISTATE(PEX_L0_RST_N), GPIO_TRISTATE(PEX_L0_CLKREQ_N), GPIO_TRISTATE(PEX_WAKE_N),
     GPIO_TRISTATE(PEX_L1_PRSNT_N), GPIO_TRISTATE(PEX_L1_RST_N), GPIO_TRISTATE(PEX_L1_CLKREQ_N), GPIO_TRISTATE(PEX_L2_PRSNT_N),
    //  Port EE
     GPIO_TRISTATE(CLK3_OUT), GPIO_TRISTATE(CLK3_REQ), GPIO_TRISTATE(CLK1_REQ)
    // Port FF is still not part of the pinout spec. will add if there are any new special purpose pins.
};

static NvS16
    s_TristateRefCount[PINMUX_AUX_PEX_L2_CLKREQ_N_0 - PINMUX_AUX_ULPI_DATA0_0];

NvBool
NvRmGetPinForGpio(NvRmDeviceHandle hDevice,
                           NvU32 Port,
                           NvU32 Pin,
                           NvU32 *pMapping)
{
    const NvU32 GpiosPerPort = 8;
    NvU32 Index = Port*GpiosPerPort + Pin;

    if ((Pin >= GpiosPerPort) || (Index >= NV_ARRAY_SIZE(g_T30GpioPadMapping)))
        return NV_FALSE;

    *pMapping = (NvU32)g_T30GpioPadMapping[Index];
    return NV_TRUE;
}

void NvRmPrivInitTrisateRefCount(NvRmDeviceHandle hDevice)
{
    NvU32 i, RegVal;

    NvOsMutexLock(hDevice->mutex);
    NvOsMemset(s_TristateRefCount, 0, sizeof(s_TristateRefCount));

    for (i=0; i<=((PINMUX_AUX_PEX_L2_CLKREQ_N_0-
                   PINMUX_AUX_ULPI_DATA0_0)>>2); i++)
    {
        RegVal = NV_REGR(hDevice, NVRM_MODULE_ID( NvRmModuleID_Misc, 0 ),
                    PINMUX_AUX_ULPI_DATA0_0 + 4*i);
        // swap from 0=normal, 1=tristate to 0=tristate, 1=normal
        RegVal = ~RegVal;

        /* the oppositely-named tristate reference count keeps track
         * of the number of active users of each pad, and
         * enables tristate when the count reaches zero. */
        s_TristateRefCount[i] =
            (NvS16)(NV_DRF_VAL(PINMUX_AUX, ULPI_DATA0, TRISTATE, RegVal) & 0x1);
    }
    NvOsMutexUnlock(hDevice->mutex);
}

/*  FindConfigStart searches through an array of configuration data to find the
 *  starting position of a particular configuration in a module instance array.
 *  The stop position is programmable, so that sub-routines can be placed after
 *  the last valid true configuration */

const NvU32*
NvRmPrivFindConfigStart(
    const NvU32* Instance,
    NvU32 Config,
    NvU32 EndMarker)
{
    NvU32 Cnt = 0;
    while ((Cnt < Config) && (*Instance!=EndMarker))
    {
        switch (NV_DRF_VAL(MUX, ENTRY, STATE, *Instance)) {
        case PinMuxConfig_BranchLink:
        case PinMuxConfig_OpcodeExtend:
            if (*Instance==CONFIGEND())
                Cnt++;
            Instance++;
            break;
        default:
            Instance += NVRM_PINMUX_SET_OPCODE_SIZE;
            break;
        }
    }

    /* Ugly postfix.  In modules with bonafide subroutines, the last
     * configuration CONFIGEND() will be followed by a MODULEDONE()
     * token, with the first Set/Unset/Branch of the subroutine
     * following that.  To avoid leaving the "PC" pointing to a
     * MODULEDONE() in the case where the first subroutine should be
     * executed, fudge the "PC" up by one, to point to the subroutine. */
    if (EndMarker==SUBROUTINESDONE() && *Instance==MODULEDONE())
        Instance++;

    if (*Instance==EndMarker)
        Instance = NULL;

    return Instance;
}

void
NvRmPrivSetGpioTristate(
    NvRmDeviceHandle hDevice,
    NvU32 Port,
    NvU32 Pin,
    NvBool EnableTristate)
{
    NvU32 Mapping = 0;
    NvBool ret = NV_FALSE;

    NV_ASSERT(hDevice);

    if (hDevice->ChipId.Id < 0x30 && hDevice->ChipId.Id != 0x14)
    {
        NV_ASSERT(!"Wrong chip ID");
        return;
    }

    ret = NvRmGetPinForGpio(hDevice, Port, Pin, &Mapping);

    if (ret)
    {
        NvS16 SkipUpdate;
        NvU32 TsOffs  = NV_DRF_VAL(MUX, GPIOMAP, TS_OFFSET, Mapping);
        NvU32 TsShift = PINMUX_AUX_ULPI_DATA0_0_TRISTATE_SHIFT;

        NvOsMutexLock(hDevice->mutex);

        if (EnableTristate)
#if (SKIP_TRISTATE_REFCNT == 0)
            SkipUpdate = --s_TristateRefCount[TsOffs];
        else
            SkipUpdate = s_TristateRefCount[TsOffs]++;
#else
            SkipUpdate = 1;
        else
            SkipUpdate = 0;
#endif

#if (SKIP_TRISTATE_REFCNT == 0)
        if (SkipUpdate < 0)
        {
            s_TristateRefCount[TsOffs] = 0;
            NV_DEBUG_PRINTF(("(%s:%s) Negative reference count detected "
            "on TsOffs=0x%x\n", __FILE__, __LINE__, TsOffs));
        }
#endif

        if (!SkipUpdate)
        {
            NvU32 Curr = NV_REGR(hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Misc, 0 ),
            PINMUX_AUX_ULPI_DATA0_0 + 4*TsOffs);
            Curr &= ~(1<<TsShift);
#if (SKIP_TRISTATE_REFCNT == 0)
            Curr |= (EnableTristate?1:0)<<TsShift;
#endif
            NV_REGW(hDevice, NVRM_MODULE_ID( NvRmModuleID_Misc, 0 ),
            PINMUX_AUX_ULPI_DATA0_0 + 4*TsOffs, Curr);
        }

        NvOsMutexUnlock(hDevice->mutex);
    }
}

NvBool NvRmPrivChipSpecificRmModuleToOdmModule(
    NvRmModuleID RmModule,
    NvOdmIoModule *OdmModule,
    NvU32 *OdmInstance,
    NvU32 *pCnt)
{
    NvRmModuleID Module = NVRM_MODULE_ID_MODULE(RmModule);
    NvU32 Instance = NVRM_MODULE_ID_INSTANCE(RmModule);
    NvBool Success = NV_TRUE;
    *pCnt = 1;
    switch (Module)
    {
    case NvRmModuleID_Usb2Otg:
        switch (Instance)
        {
        case 0:
            *OdmModule = NvOdmIoModule_Usb;
            *OdmInstance = 0;
            break;
        case 1:
            *OdmModule = NvOdmIoModule_Ulpi;
            *OdmInstance = 0;
            break;
        case 2:
            *OdmModule = NvOdmIoModule_Usb;
            *OdmInstance = 1;
            break;
        default:
            NV_ASSERT(!"Invalid USB instance");
            break;
        }
        break;
    case NvRmModuleID_OneWire:
        *OdmModule = NvOdmIoModule_OneWire;
        *OdmInstance = Instance;
        break;
    case NvRmModuleID_SyncNor:
        *OdmModule = NvOdmIoModule_SyncNor;
        *OdmInstance = Instance;
        break;
    case NvRmPrivModuleID_Pcie:
        *OdmModule = NvOdmIoModule_PciExpress;
        *OdmInstance = Instance;
        break;
    default:
        Success = NV_FALSE;
        *pCnt = 0;
        break;
    }
    return  Success;
}
