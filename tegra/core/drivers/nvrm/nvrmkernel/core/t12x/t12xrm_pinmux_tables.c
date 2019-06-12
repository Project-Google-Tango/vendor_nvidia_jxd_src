/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvrm_structure.h"
#include "nvrm_pinmux.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nvrm_hwintf.h"
#include "t12x/arapb_misc.h"
#include "t30/t30rm_pinmux_utils.h" // Using t30 header to avoid duplication
#include "nvrm_clocks.h"
#include "nvodm_query_pinmux.h"

//This macro will be used for those GPIO pins which are not present
#define GPIO_RESERVED() 0xFFFF


/* Define the GPIO port/pin to tristate mappings */
const NvU16 g_T12xGpioPadMapping[] =
{
    //  Port A
    GPIO_TRISTATE(CLK_32K_OUT), GPIO_TRISTATE(UART3_CTS_N), GPIO_TRISTATE(DAP2_FS), GPIO_TRISTATE(DAP2_SCLK),
    GPIO_TRISTATE(DAP2_DIN), GPIO_TRISTATE(DAP2_DOUT), GPIO_TRISTATE(SDMMC3_CLK), GPIO_TRISTATE(SDMMC3_CMD),
    //  Port B
    GPIO_TRISTATE(GPIO_PB0), GPIO_TRISTATE(GPIO_PB1), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_TRISTATE(SDMMC3_DAT3), GPIO_TRISTATE(SDMMC3_DAT2), GPIO_TRISTATE(SDMMC3_DAT1), GPIO_TRISTATE(SDMMC3_DAT0),
    //  Port C
    GPIO_TRISTATE(UART3_RTS_N), GPIO_RESERVED(), GPIO_TRISTATE(UART2_TXD), GPIO_TRISTATE(UART2_RXD),
    GPIO_TRISTATE(GEN1_I2C_SCL), GPIO_TRISTATE(GEN1_I2C_SDA), GPIO_RESERVED(), GPIO_TRISTATE(GPIO_PC7),
    //Port D
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //Port E
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //Port F
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port G
    GPIO_TRISTATE(GPIO_PG0), GPIO_TRISTATE(GPIO_PG1),GPIO_TRISTATE(GPIO_PG2), GPIO_TRISTATE(GPIO_PG3),
    GPIO_TRISTATE(GPIO_PG4), GPIO_TRISTATE(GPIO_PG5),GPIO_TRISTATE(GPIO_PG6), GPIO_TRISTATE(GPIO_PG7),
    //  Port H
    GPIO_TRISTATE(GPIO_PH0), GPIO_TRISTATE(GPIO_PH1),GPIO_TRISTATE(GPIO_PH2), GPIO_TRISTATE(GPIO_PH3),
    GPIO_TRISTATE(GPIO_PH4), GPIO_TRISTATE(GPIO_PH5),GPIO_TRISTATE(GPIO_PH6), GPIO_TRISTATE(GPIO_PH7),
    //  Port I
    GPIO_TRISTATE(GPIO_PI0), GPIO_TRISTATE(GPIO_PI1), GPIO_TRISTATE(GPIO_PI2), GPIO_TRISTATE(GPIO_PI3),
    GPIO_TRISTATE(GPIO_PI4),GPIO_TRISTATE(GPIO_PI5), GPIO_TRISTATE(GPIO_PI6), GPIO_TRISTATE(GPIO_PI7),
    //  Port J
    GPIO_TRISTATE(GPIO_PJ0), GPIO_RESERVED(),  GPIO_TRISTATE(GPIO_PJ2), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_TRISTATE(UART2_CTS_N), GPIO_TRISTATE(UART2_RTS_N), GPIO_TRISTATE(GPIO_PJ7),
    //  Port K
    GPIO_TRISTATE(GPIO_PK0),GPIO_TRISTATE(GPIO_PK1), GPIO_TRISTATE(GPIO_PK2), GPIO_TRISTATE(GPIO_PK3),
    GPIO_TRISTATE(GPIO_PK4),GPIO_TRISTATE(SPDIF_OUT), GPIO_TRISTATE(SPDIF_IN), GPIO_TRISTATE(GPIO_PK7),
    // Port L
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port M
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port N
    GPIO_TRISTATE(DAP1_FS),GPIO_TRISTATE(DAP1_DIN), GPIO_TRISTATE(DAP1_DOUT), GPIO_TRISTATE(DAP1_SCLK),
    GPIO_TRISTATE(USB_VBUS_EN0),GPIO_TRISTATE(USB_VBUS_EN1), GPIO_RESERVED(), GPIO_TRISTATE(HDMI_INT),
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
    GPIO_TRISTATE(KB_ROW8),GPIO_TRISTATE(KB_ROW9), GPIO_TRISTATE(KB_ROW10),  GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port T
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_TRISTATE(GEN2_I2C_SCL), GPIO_TRISTATE(GEN2_I2C_SDA), GPIO_TRISTATE(SDMMC4_CMD),
    //  Port U
    GPIO_TRISTATE(GPIO_PU0), GPIO_TRISTATE(GPIO_PU1), GPIO_TRISTATE(GPIO_PU2), GPIO_TRISTATE(GPIO_PU3),
    GPIO_TRISTATE(GPIO_PU4), GPIO_TRISTATE(GPIO_PU5), GPIO_TRISTATE(GPIO_PU6), GPIO_TRISTATE(JTAG_RTCK),
    //  Port V
    GPIO_TRISTATE(GPIO_PV0), GPIO_TRISTATE(GPIO_PV1), GPIO_TRISTATE(SDMMC3_CD_N), GPIO_TRISTATE(SDMMC1_WP_N),
    GPIO_TRISTATE(DDC_SCL), GPIO_TRISTATE(DDC_SDA), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port W
    GPIO_RESERVED(), GPIO_RESERVED(),  GPIO_TRISTATE(GPIO_W2_AUD), GPIO_TRISTATE(GPIO_W3_AUD),
    GPIO_TRISTATE(DAP_MCLK1), GPIO_TRISTATE(CLK2_OUT),GPIO_TRISTATE(UART3_TXD),GPIO_TRISTATE(UART3_RXD),
    //  Port X
    GPIO_TRISTATE(DVFS_PWM),GPIO_TRISTATE(GPIO_X1_AUD),GPIO_TRISTATE(DVFS_CLK),GPIO_TRISTATE(GPIO_X3_AUD),
    GPIO_TRISTATE(GPIO_X4_AUD),GPIO_TRISTATE(GPIO_X5_AUD),GPIO_TRISTATE(GPIO_X6_AUD),GPIO_TRISTATE(GPIO_X7_AUD),
    //  Port Y
    GPIO_TRISTATE(ULPI_CLK),GPIO_TRISTATE(ULPI_DIR),GPIO_TRISTATE(ULPI_NXT),GPIO_TRISTATE(ULPI_STP),
    GPIO_TRISTATE(SDMMC1_DAT3),GPIO_TRISTATE(SDMMC1_DAT2),GPIO_TRISTATE(SDMMC1_DAT1),GPIO_TRISTATE(SDMMC1_DAT0),
    //  Port Z
    GPIO_TRISTATE(SDMMC1_CLK),GPIO_TRISTATE(SDMMC1_CMD), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_TRISTATE(PWR_I2C_SCL), GPIO_TRISTATE(PWR_I2C_SDA),
    //  Port AA
    GPIO_TRISTATE(SDMMC4_DAT0), GPIO_TRISTATE(SDMMC4_DAT1), GPIO_TRISTATE(SDMMC4_DAT2), GPIO_TRISTATE(SDMMC4_DAT3),
    GPIO_TRISTATE(SDMMC4_DAT4), GPIO_TRISTATE(SDMMC4_DAT5), GPIO_TRISTATE(SDMMC4_DAT6), GPIO_TRISTATE(SDMMC4_DAT7),
    //  Port BB
    GPIO_TRISTATE(GPIO_PBB0), GPIO_TRISTATE(CAM_I2C_SCL), GPIO_TRISTATE(CAM_I2C_SDA), GPIO_TRISTATE(GPIO_PBB3),
    GPIO_TRISTATE(GPIO_PBB4), GPIO_TRISTATE(GPIO_PBB5), GPIO_TRISTATE(GPIO_PBB6), GPIO_TRISTATE(GPIO_PBB7),
    //  Port CC
    GPIO_TRISTATE(CAM_MCLK), GPIO_TRISTATE(GPIO_PCC1), GPIO_TRISTATE(GPIO_PCC2), GPIO_RESERVED(),
    GPIO_TRISTATE(SDMMC4_CLK), GPIO_TRISTATE(CLK2_REQ), GPIO_RESERVED(), GPIO_RESERVED(),
    //Port DD
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(), GPIO_RESERVED(),
    //  Port EE
    GPIO_TRISTATE(CLK3_OUT), GPIO_TRISTATE(CLK3_REQ), GPIO_TRISTATE(DAP_MCLK1_REQ), GPIO_TRISTATE(HDMI_CEC),
    GPIO_TRISTATE(SDMMC3_CLK_LB_OUT), GPIO_TRISTATE(SDMMC3_CLK_LB_IN), GPIO_RESERVED(), GPIO_RESERVED()
    // Port FF is still not part of the pinout spec. will add if there are any new special purpose pins.

};

static NvS16
    s_TristateRefCount[PINMUX_AUX_RESET_OUT_N_0 - PINMUX_AUX_ULPI_DATA0_0];

NvBool
NvRmGetPinForGpio(NvRmDeviceHandle hDevice,
                           NvU32 Port,
                           NvU32 Pin,
                           NvU32 *pMapping)
{
    const NvU32 GpiosPerPort = 8;
    NvU32 Index = Port*GpiosPerPort + Pin;

    if ((Pin >= GpiosPerPort) || (Index >= NV_ARRAY_SIZE(g_T12xGpioPadMapping)))
        return NV_FALSE;

    *pMapping = (NvU32)g_T12xGpioPadMapping[Index];

    //This check is for the removed gpio pins and
    //denoted by GPIO_RESERVED macro
    if (*pMapping != (NvU32)0xFFFF)
        return NV_TRUE;
    else
        return NV_FALSE;
}

void NvRmPrivInitTrisateRefCount(NvRmDeviceHandle hDevice)
{
    NvU32 i, RegVal;

    NvOsMutexLock(hDevice->mutex);
    NvOsMemset(s_TristateRefCount, 0, sizeof(s_TristateRefCount));

    for (i=0; i<=((PINMUX_AUX_RESET_OUT_N_0-
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
