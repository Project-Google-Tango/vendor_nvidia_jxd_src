/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_clocks.h"
#include "nvrm_module.h"
#include "nvrm_drf.h"
#include "nvrm_relocation_table.h"
#include "ap15/ap15rm_private.h"
#include "t30/arclk_rst.h"
#include "t30/arapbpm.h"
#include "t30/arahb_arbc.h"

#define T30_DISPLAY_DEFAULT_SOURCE_PLLA (1)

// This list requires pre-sorted info in bond-out registers order and bond-out
// register bit shift order (MSB-to-LSB).  It also assumes one bit info.
static const NvU32 s_T30BondOutTable[] =
{
    // BOND_OUT_L bits
    NVRM_DEVICE_UNKNOWN,  // NV_DEVID_CPU
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmModuleID_Rtc,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Timer,                         0 ),
    NVRM_MODULE_ID( NvRmModuleID_Uart,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_Uart,                          1 ),
    NVRM_MODULE_ID( NvRmPrivModuleID_Gpio,                      0 ),
    NVRM_MODULE_ID( NvRmModuleID_Sdio,                          1 ),
    NVRM_MODULE_ID( NvRmModuleID_Spdif,                         0 ),
    NVRM_MODULE_ID( NvRmModuleID_I2s,                           1 ),
    NVRM_MODULE_ID( NvRmModuleID_I2c,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Nand,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_Sdio,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_Sdio,                          3 ),
    NVRM_MODULE_ID( NvRmModuleID_Twc,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Pwm,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_I2s,                           2 ),
    NVRM_MODULE_ID( NvRmModuleID_Epp,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Vi,                            0 ),
    NVRM_MODULE_ID( NvRmModuleID_2D,                            0 ),
    NVRM_MODULE_ID( NvRmModuleID_Usb2Otg,                       0 ),
    NVRM_MODULE_ID( NvRmModuleID_Isp,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_3D,                            0 ),
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmModuleID_Display,                       1 ),
    NVRM_MODULE_ID( NvRmModuleID_Display,                       0 ),
    NVRM_MODULE_ID( NvRmModuleID_GraphicsHost,                  0 ),
    NVRM_MODULE_ID( NvRmModuleID_Vcp,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_I2s,                           0 ),
    NVRM_DEVICE_UNKNOWN,    // NV_DEVID_COP_CACHE

    // BOND_OUT_H bits
    NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController,          0 ),
    NVRM_DEVICE_UNKNOWN,    // NV_DEVID_AHB_DMA
    NVRM_MODULE_ID( NvRmPrivModuleID_ApbDma,                    0 ),
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmModuleID_Kbc,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_SysStatMonitor,                0 ),
    NVRM_DEVICE_UNKNOWN,    // PMC
    NVRM_MODULE_ID( NvRmModuleID_Fuse,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_KFuse,                         0 ),
    NVRM_DEVICE_UNKNOWN,    // SBC1
    NVRM_MODULE_ID( NvRmModuleID_Nor,                           0 ),
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,    // SBC2
    NVRM_MODULE_ID( NvRmModuleID_Xio,                           0 ),
    NVRM_DEVICE_UNKNOWN,    // SBC3
    NVRM_MODULE_ID( NvRmModuleID_Dvc,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Dsi,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Tvo,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Mipi,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_Hdmi,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_Csi,                           0 ),
    NVRM_DEVICE_UNKNOWN,    // TVDAC
    NVRM_MODULE_ID( NvRmModuleID_I2c,                           1 ),
    NVRM_MODULE_ID( NvRmModuleID_Uart,                          2 ),
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmPrivModuleID_ExternalMemoryController,  0 ),
    NVRM_MODULE_ID( NvRmModuleID_Usb2Otg,                       1 ),
    NVRM_MODULE_ID( NvRmModuleID_Usb2Otg,                       2 ),
    NVRM_MODULE_ID( NvRmModuleID_Mpe,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_Vde,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_BseA,                          0 ),
    NVRM_DEVICE_UNKNOWN,    // BSEV

    // BOND_OUT_U bits
    NVRM_DEVICE_UNKNOWN,    // NV_DEVID_SPEEDO
    NVRM_MODULE_ID( NvRmModuleID_Uart,                          3 ),
    NVRM_MODULE_ID( NvRmModuleID_Uart,                          4 ),
    NVRM_MODULE_ID( NvRmModuleID_I2c,                           2 ),
    NVRM_DEVICE_UNKNOWN,    // SBC4
    NVRM_MODULE_ID( NvRmModuleID_Sdio,                          2 ),
    NVRM_MODULE_ID( NvRmPrivModuleID_Pcie,                      0 ),
    NVRM_MODULE_ID( NvRmModuleID_OneWire,                       0 ),
    NVRM_DEVICE_UNKNOWN,    // AFI
    NVRM_DEVICE_UNKNOWN,    // CSITE
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmModuleID_AvpUcq,                        0 ),
    NVRM_DEVICE_UNKNOWN,    // LA
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_MODULE_ID( NvRmModuleID_DTV,                           0 ),
    NVRM_DEVICE_UNKNOWN,    // NAND_SPEED
    NVRM_MODULE_ID( NvRmModuleID_I2c,                           5 ),
    NVRM_DEVICE_UNKNOWN,    // DSIB
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,    // IRAMA
    NVRM_DEVICE_UNKNOWN,    // IRAMB
    NVRM_DEVICE_UNKNOWN,    // IRAMC
    NVRM_DEVICE_UNKNOWN,    // IRAMD
    NVRM_DEVICE_UNKNOWN,    // CRAM2
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,    // CLK_M_DOUBLER
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,    // SUS_OUT
    NVRM_DEVICE_UNKNOWN,    // DEV2_OUT
    NVRM_DEVICE_UNKNOWN,    // DEV1_OUT
    NVRM_DEVICE_UNKNOWN,

    // BOND_OUT_V bits
    NVRM_DEVICE_UNKNOWN,    // CPUG
    NVRM_DEVICE_UNKNOWN,    // CPULP
    NVRM_MODULE_ID( NvRmModuleID_3D,                            1 ),
    NVRM_MODULE_ID( NvRmPrivModuleID_Mselect,                   0 ),
    NVRM_MODULE_ID( NvRmModuleID_Tsensor,                       0 ),
    NVRM_MODULE_ID( NvRmModuleID_I2s,                           3 ),
    NVRM_MODULE_ID( NvRmModuleID_I2s,                           4 ),
    NVRM_MODULE_ID( NvRmModuleID_I2c,                           3 ),
    NVRM_DEVICE_UNKNOWN,    // SBC5
    NVRM_DEVICE_UNKNOWN,    // SBC6
    NVRM_MODULE_ID( NvRmModuleID_Audio,                         0 ),
    NVRM_MODULE_ID( NvRmModuleID_Apbif,                         0 ),
    NVRM_MODULE_ID( NvRmModuleID_DAM,                           0 ),
    NVRM_MODULE_ID( NvRmModuleID_DAM,                           1 ),
    NVRM_MODULE_ID( NvRmModuleID_DAM,                           2 ),
    NVRM_DEVICE_UNKNOWN,    // HDA2CODEX_2X
    NVRM_MODULE_ID( NvRmModuleID_Atomics,                       0 ),
    NVRM_DEVICE_UNKNOWN,    // I2S0_DOUBLER
    NVRM_DEVICE_UNKNOWN,    // I2S1_DOUBLER
    NVRM_DEVICE_UNKNOWN,    // I2S2_DOUBLER
    NVRM_DEVICE_UNKNOWN,    // I2S3_DOUBLER
    NVRM_DEVICE_UNKNOWN,    // I2S4_DOUBLER
    NVRM_DEVICE_UNKNOWN,    // SPDIF_DOUBLER
    NVRM_MODULE_ID( NvRmModuleID_Actmon,                        0 ),
    NVRM_MODULE_ID( NvRmModuleID_ExtPeriphClk,                  0 ),    // EXTPERIPH1
    NVRM_MODULE_ID( NvRmModuleID_ExtPeriphClk,                  1 ),    // EXTPERIPH2
    NVRM_MODULE_ID( NvRmModuleID_ExtPeriphClk,                  2 ),    // EXTPERIPH3
    NVRM_DEVICE_UNKNOWN,    // SATA_OOB
    NVRM_MODULE_ID( NvRmModuleID_Sata,                          0 ),
    NVRM_MODULE_ID( NvRmModuleID_HDA,                           0 ),
    NVRM_DEVICE_UNKNOWN,    // TZRAM
    NVRM_MODULE_ID( NvRmModuleID_Se,                            0 ),

    // BOND_OUT_W bits
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,    // PCIERX0
    NVRM_DEVICE_UNKNOWN,    // PCIERX1
    NVRM_DEVICE_UNKNOWN,    // PCIERX2
    NVRM_DEVICE_UNKNOWN,    // PCIERX3
    NVRM_DEVICE_UNKNOWN,    // PCIERX4
    NVRM_DEVICE_UNKNOWN,    // PCIERX5
    NVRM_DEVICE_UNKNOWN,    // CEC
    NVRM_DEVICE_UNKNOWN,    // PCIE2_IOBIST
    NVRM_DEVICE_UNKNOWN,    // EMC_IOBIST
    NVRM_DEVICE_UNKNOWN,    // HDMI_IOBIST
    NVRM_DEVICE_UNKNOWN,    // SATA_IOBIST
    NVRM_DEVICE_UNKNOWN,    // MIPI_IOBIST
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
    NVRM_DEVICE_UNKNOWN,
};

void
NvRmGetBondOut( NvRmDeviceHandle hDevice,
                       const NvU32     **pTable,
                       NvU32           *bondOut )
{
    *pTable = s_T30BondOutTable;
    bondOut[0] = NV_REGR(hDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_BOND_OUT_L_0);
    bondOut[1] = NV_REGR(hDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_BOND_OUT_H_0);
    bondOut[2] = NV_REGR(hDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_BOND_OUT_U_0);
    bondOut[3] = NV_REGR(hDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_BOND_OUT_V_0);
    bondOut[4] = NV_REGR(hDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_BOND_OUT_W_0);
}

void
EnableTvDacClock(
    NvRmDeviceHandle hDevice,
    ModuleClockState ClockState)
{
    CLOCK_ENABLE( hDevice, H, TVDAC, ClockState );
}

void
NvRmEnableModuleClock(
    NvRmDeviceHandle hDevice,
    NvRmModuleID ModuleId,
    ModuleClockState ClockState)
{
    // Extract module and instance from composite module id.
    NvU32 Module   = NVRM_MODULE_ID_MODULE( ModuleId );
    NvU32 Instance = NVRM_MODULE_ID_INSTANCE( ModuleId );

    if (ClockState == ModuleClockState_Enable)
    {
        NvRmPrivConfigureClockSource(hDevice, ModuleId, NV_TRUE);
    }
    switch ( Module ) {
        case NvRmModuleID_CacheMemCtrl:
            NV_ASSERT( Instance < 2 );
            if( Instance == 0 )
            {
                NV_ASSERT(!"T30 doesn't have such device");
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, L, CACHE2, ClockState );
            }
            break;
        case NvRmModuleID_Vcp:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, VCP, ClockState );
            break;
        case NvRmModuleID_GraphicsHost:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, HOST1X, ClockState );
            break;
        case NvRmModuleID_Display:
            NV_ASSERT( Instance < 2 );
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, L, DISP1, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, L, DISP2, ClockState );
            }
            break;
        case NvRmModuleID_3D:
            NV_ASSERT( Instance < 2 );
            // Always keep both 3D cores in sync
            CLOCK_ENABLE( hDevice, L, 3D, ClockState );
            CLOCK_ENABLE( hDevice, V, 3D2, ClockState );
            break;
        case NvRmModuleID_Isp:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, ISP, ClockState );
            break;
        case NvRmModuleID_Usb2Otg:
            if (Instance == 0)
            {
                CLOCK_ENABLE( hDevice, L, USBD, ClockState );
            }
            else if (Instance == 1)
            {
                CLOCK_ENABLE( hDevice, H, USB2, ClockState );
            }
            else if (Instance == 2)
            {
                CLOCK_ENABLE( hDevice, H, USB3, ClockState );
            }
            else
            {
                NV_ASSERT(!"Invalid USB instance");
            }
            break;
        case NvRmModuleID_2D:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, 2D, ClockState );
            break;
        case NvRmModuleID_Epp:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, EPP, ClockState );
            break;
        case NvRmModuleID_Vi:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, VI, ClockState );
            break;
        case NvRmModuleID_I2s:
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, L, I2S0, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, L, I2S1, ClockState );
            }
            else if( Instance == 2 )
            {
                CLOCK_ENABLE( hDevice, L, I2S2, ClockState );
            }
            else if( Instance == 3 )
            {
                CLOCK_ENABLE( hDevice, V, I2S3, ClockState );
            }
            else if( Instance == 4 )
            {
                CLOCK_ENABLE( hDevice, V, I2S4, ClockState );
            }
            else
            {
                NV_ASSERT(!"Invalid I2S instance");
            }
            break;
        case NvRmModuleID_Twc:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, TWC, ClockState );
            break;
        case NvRmModuleID_Pwm:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, PWM, ClockState );
            break;
        case NvRmModuleID_Sdio:
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, L, SDMMC1, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, L, SDMMC2, ClockState );
            } else if (Instance == 2)
            {
                CLOCK_ENABLE( hDevice, U, SDMMC3, ClockState );
            } else if (Instance == 3)
            {
                CLOCK_ENABLE( hDevice, L, SDMMC4, ClockState );
            } else
            {
                NV_ASSERT(!"Invalid SDIO instance");
            }
            break;
        case NvRmModuleID_Spdif:
            NV_ASSERT( Instance < 1 );
            CLOCK_ENABLE( hDevice, L, SPDIF, ClockState );
            break;
        case NvRmModuleID_Nand:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, NDFLASH, ClockState );
            CLOCK_ENABLE( hDevice, U, NAND_SPEED, ClockState );
            break;
        case NvRmModuleID_I2c:
            switch (Instance)
            {
            case 0:
                CLOCK_ENABLE( hDevice, L, I2C1, ClockState );
                break;
            case 1:
                CLOCK_ENABLE( hDevice, H, I2C2, ClockState );
                break;
            case 2:
                CLOCK_ENABLE( hDevice, U, I2C3, ClockState );
                break;
            case 3:
                CLOCK_ENABLE( hDevice, V, I2C4, ClockState );
                break;
#if defined(CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_DVC_I2C_SHIFT)
            case 4:
                CLOCK_ENABLE( hDevice, H, DVC_I2C, ClockState );
                break;
#else
#if defined(CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_I2C5_SHIFT)
            case 4:
                CLOCK_ENABLE( hDevice, H, I2C5, ClockState );
                break;
#endif
#endif
            case 5:
                CLOCK_ENABLE( hDevice, U, I2C_SLOW, ClockState );
                break;
            default:
                NV_ASSERT(!"Invalid I2C instance");
            }
            break;
        case NvRmPrivModuleID_Gpio:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, GPIO, ClockState );
            break;
        case NvRmModuleID_Uart:
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, L, UARTA, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, L, UARTB, ClockState );
            }
            else if ( Instance == 2)
            {
                CLOCK_ENABLE( hDevice, H, UARTC, ClockState );
            } else if (Instance == 3)
            {
                CLOCK_ENABLE( hDevice, U, UARTD, ClockState );
            } else if ( Instance == 4)
            {
                CLOCK_ENABLE( hDevice, U, UARTE, ClockState );
            } else
            {
                NV_ASSERT(!"Invlaid UART instance");
            }
            break;
        case NvRmModuleID_Vfir:
            // Same as UARTB
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, UARTB, ClockState );
            break;
        case NvRmModuleID_Rtc:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, RTC, ClockState );
            break;
        case NvRmModuleID_Timer:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, TMR, ClockState );
            break;
        case NvRmModuleID_BseA:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, BSEA, ClockState );
            break;
        case NvRmModuleID_Vde:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, VDE, ClockState );
            CLOCK_ENABLE( hDevice, H, BSEV, ClockState );
            break;
        case NvRmModuleID_Mpe:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, MPE, ClockState );
            break;
        case NvRmModuleID_Tvo:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, TVO, ClockState );
            CLOCK_ENABLE( hDevice, H, TVDAC, ClockState );
            break;
        case NvRmModuleID_Csi:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, CSI, ClockState );
            break;
        case NvRmModuleID_Hdmi:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, HDMI, ClockState );
            break;
        case NvRmModuleID_Mipi:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, HSI, ClockState );
            break;
        case NvRmModuleID_Dsi:
            NV_ASSERT( Instance < 2 );
            if( Instance == 0)
            {
                CLOCK_ENABLE( hDevice, H, DSI, ClockState );
            }
            else if( Instance == 1)
            {
                CLOCK_ENABLE( hDevice, U, DSIB, ClockState );
            }
            break;
        case NvRmModuleID_Xio:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, XIO, ClockState );
            break;
        case NvRmModuleID_Fuse:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, FUSE, ClockState );
            break;
        case NvRmModuleID_KFuse:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, KFUSE, ClockState );
            break;
        case NvRmModuleID_Slink:
            // Supporting only the slink controller.
            NV_ASSERT( Instance < 6 );
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, H, SBC1, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, H, SBC2, ClockState );
            }
            else if ( Instance == 2)
            {
                CLOCK_ENABLE( hDevice, H, SBC3, ClockState );
            }
            else if ( Instance == 3)
            {
                CLOCK_ENABLE( hDevice, U, SBC4, ClockState );
            }
            else if ( Instance == 4)
            {
                CLOCK_ENABLE( hDevice, V, SBC5, ClockState );
            }
            else if ( Instance == 5)
            {
                CLOCK_ENABLE( hDevice, V, SBC6, ClockState );
            }
            break;
        case NvRmModuleID_Pmif:
            NV_ASSERT( Instance == 0 );
            // PMC clock must not be disabled
            if (ClockState == ModuleClockState_Enable)
                CLOCK_ENABLE( hDevice, H, PMC, ClockState );
            break;
        case NvRmModuleID_SysStatMonitor:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, STAT_MON, ClockState );
            break;
        case NvRmModuleID_Kbc:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, KBC, ClockState );
            break;
        case NvRmPrivModuleID_ApbDma:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, APBDMA, ClockState );
            break;
        case NvRmPrivModuleID_MemoryController:
            // FIXME: should this be allowed?
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, MEM, ClockState );
            break;
        case NvRmPrivModuleID_ExternalMemoryController:
            // FIXME: should this be allowed?
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, EMC, ClockState );
            break;
        case NvRmModuleID_Cpu:
            // FIXME: should this be allowed?
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, L, CPU, ClockState );
            break ;
        case NvRmModuleID_SyncNor:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, H, SNOR, ClockState );
            break;
        case NvRmModuleID_AvpUcq:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, U, AVPUCQ, ClockState );
            break;
        case NvRmModuleID_OneWire:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, U, OWR, ClockState );
            break;
        case NvRmPrivModuleID_Pcie:
            NV_ASSERT( Instance == 0 );
            // Keep in sync both PCIE wrapper (AFI) and core clocks
            CLOCK_ENABLE( hDevice, U, PCIE, ClockState );
            CLOCK_ENABLE( hDevice, U, AFI, ClockState );
            break;
        case NvRmModuleID_HDA:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, HDA, ClockState );
            CLOCK_ENABLE( hDevice, V, HDA2CODEC_2X, ClockState );
            CLOCK_ENABLE( hDevice, W, HDA2HDMICODEC, ClockState );
            break;
        case NvRmModuleID_Apbif:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, APBIF, ClockState );
            break;
        case NvRmModuleID_Audio:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, AUDIO, ClockState );
            break;
        case NvRmModuleID_DAM:
            NV_ASSERT( Instance < 3 );
            if( Instance == 0 )
            {
                CLOCK_ENABLE( hDevice, V, DAM0, ClockState );
            }
            else if( Instance == 1 )
            {
                CLOCK_ENABLE( hDevice, V, DAM1, ClockState );
            }
            else if ( Instance == 2)
            {
                CLOCK_ENABLE( hDevice, V, DAM2, ClockState );
            }
            break;
        case NvRmPrivModuleID_Mselect:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, MSELECT, ClockState );
            break;
        case NvRmModuleID_Sata:
            NV_ASSERT(Instance == 0);
            CLOCK_ENABLE(hDevice, V, SATA, ClockState);
            if (ClockState == ModuleClockState_Enable)
            {
                CLOCK_ENABLE(hDevice, V, SATA_OOB, ClockState);
            }
            break;
        case NvRmModuleID_DTV:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, U, DTV, ClockState );
            break;
        case NvRmModuleID_Tsensor:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, TSENSOR, ClockState );
            break;
        case NvRmModuleID_Actmon:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, ACTMON, ClockState );
            break;
        case NvRmModuleID_Atomics:
            NV_ASSERT( Instance == 0 );
            if ( ClockState == ModuleClockState_Enable )
            {
                CLOCK_ENABLE( hDevice, V, ATOMICS, ClockState );
            }
            break;
        case NvRmModuleID_Se:
            NV_ASSERT( Instance == 0 );
            CLOCK_ENABLE( hDevice, V, SE, ClockState );
            break;
        case NvRmModuleID_ExtPeriphClk:
            NV_ASSERT(Instance < 3);
            if (Instance == 0)
            {
                CLOCK_ENABLE(hDevice, V, EXTPERIPH1, ClockState);
            }
            else if (Instance == 1)
            {
                CLOCK_ENABLE(hDevice, V, EXTPERIPH2, ClockState);
            }
            else if (Instance == 2)
            {
                CLOCK_ENABLE(hDevice, V, EXTPERIPH3, ClockState);
            }
            break;
        default:
            NV_ASSERT(!"Unknown NvRmModuleID passed to NvRmEnableModuleClock().");
    }

    if (ClockState == ModuleClockState_Disable)
    {
        NvRmPrivConfigureClockSource(hDevice, ModuleId, NV_FALSE);
    }
}

// KBC reset is available in the pmc control register.
#define RESET_KBC( rm, delay ) \
    do { \
        NvU32 reg; \
        reg = NV_REGR((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
             APBDEV_PMC_CNTRL_0); \
        reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, KBC_RST, ENABLE, reg); \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
            APBDEV_PMC_CNTRL_0, reg); \
        if (hold) \
        {\
            break; \
        }\
        NvOsWaitUS(delay); \
        reg = NV_REGR((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
            APBDEV_PMC_CNTRL_0); \
        reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, KBC_RST, DISABLE, reg); \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
            APBDEV_PMC_CNTRL_0, reg); \
    } while( 0 )

// Use PMC control to reset the entire SoC. Just wait forever after reset is
// issued - h/w would auto-clear it and restart SoC
#define RESET_SOC( rm ) \
    do { \
        volatile NvBool b = NV_TRUE; \
        NvU32 reg; \
        reg = NV_REGR((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
            APBDEV_PMC_CNTRL_0); \
        reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, MAIN_RST, ENABLE, reg); \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), \
            APBDEV_PMC_CNTRL_0, reg); \
        while( b ) { ; } \
    } while( 0 )

void ModuleReset(NvRmDeviceHandle hDevice, NvRmModuleID ModuleId, NvBool hold)
{
    // Extract module and instance from composite module id.
    NvU32 Module   = NVRM_MODULE_ID_MODULE( ModuleId );
    NvU32 Instance = NVRM_MODULE_ID_INSTANCE( ModuleId );

    // Note that VDE has different reset sequence requirement
    // FIMXE: NV blocks - hot reset issues
    #define RESET( rm, offset, field, delay ) \
        do { \
            NvU32 reg; \
            reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_##offset##_SET, \
                SET_##field##_RST, 1); \
            NV_REGW((rm), \
                NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
                CLK_RST_CONTROLLER_RST_DEV_##offset##_SET_0, reg); \
            if (hold) \
            {  \
                break; \
            } \
            NvOsWaitUS( (delay) ); \
            reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_##offset##_CLR, \
                CLR_##field##_RST, 1); \
            NV_REGW((rm), \
                NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
                CLK_RST_CONTROLLER_RST_DEV_##offset##_CLR_0, reg); \
        } while( 0 )


    switch( Module ) {
    case NvRmPrivModuleID_MemoryController:
        // FIXME: should this be allowed?
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, MEM, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Kbc:
        NV_ASSERT( Instance == 0 );
        RESET_KBC(hDevice, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_SysStatMonitor:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, STAT_MON, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Pmif:
        NV_ASSERT( Instance == 0 );
        NV_ASSERT(!"PMC reset is not allowed, and does nothing on T30");
        break;
    case NvRmModuleID_Fuse:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, FUSE, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_KFuse:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, KFUSE, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Slink:
        // Supporting only the slink controller.
        NV_ASSERT( Instance < 6 );
        if( Instance == 0 )
        {
            RESET( hDevice, H, SBC1, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, H, SBC2, NVRM_RESET_DELAY );
        }
        else if (Instance == 2)
        {
            RESET( hDevice, H, SBC3, NVRM_RESET_DELAY );
        } else if (Instance == 3)
        {
            RESET( hDevice, U, SBC4, NVRM_RESET_DELAY );
        }
        else if ( Instance == 4)
        {
            RESET( hDevice, V, SBC5, NVRM_RESET_DELAY );
        }
        else if ( Instance == 5)
        {
            RESET( hDevice, V, SBC6, NVRM_RESET_DELAY );
        }

        break;
    case NvRmModuleID_Xio:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, XIO, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Dsi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, DSI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Tvo:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, TVO, NVRM_RESET_DELAY );
        RESET( hDevice, H, TVDAC, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Mipi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, HSI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Hdmi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, HDMI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Csi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, CSI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_I2c:
        switch (Instance)
        {
        case 0:
            RESET( hDevice, L, I2C1, NVRM_RESET_DELAY );
            break;
        case 1:
            RESET( hDevice, H, I2C2, NVRM_RESET_DELAY );
            break;
        case 2:
            RESET( hDevice, U, I2C3, NVRM_RESET_DELAY );
            break;
        case 3:
            RESET( hDevice, V, I2C4, NVRM_RESET_DELAY );
            break;
#if defined(CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_DVC_I2C_RST_SHIFT)
        case 4:
            RESET( hDevice, H, DVC_I2C, NVRM_RESET_DELAY );
            break;
#else
#if defined(CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_I2C5_RST_SHIFT)
        case 4:
            RESET( hDevice, H, I2C5, NVRM_RESET_DELAY );
            break;
#endif
#endif
        default:
            NV_ASSERT(!"Invalid I2C instace");
        }
        break;
    case NvRmModuleID_Mpe:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, MPE, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Vde:
        NV_ASSERT( Instance == 0 );
        {
            NvU32 reg;

            reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_SET,
                    SET_VDE_RST, 1)
                | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_SET,
                    SET_BSEV_RST, 1);
            NV_REGW(hDevice,
                NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                CLK_RST_CONTROLLER_RST_DEV_H_SET_0, reg);

            if (hold)
            {
                break;
            }
            NvOsWaitUS( NVRM_RESET_DELAY );

            reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_CLR,
                    CLR_VDE_RST, 1)
                | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_CLR,
                    CLR_BSEV_RST, 1);
            NV_REGW(hDevice,
                NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                CLK_RST_CONTROLLER_RST_DEV_H_CLR_0, reg);
        }
        break;
    case NvRmModuleID_BseA:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, BSEA, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Cpu:
        // FIXME: should this be allowed?
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, CPU, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Avp:
        // FIXME: should this be allowed?
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, COP, NVRM_RESET_DELAY );
        break;
    case NvRmPrivModuleID_System:
        /* THIS WILL DO A FULL SYSTEM RESET */
        NV_ASSERT( Instance == 0 );
        RESET_SOC(hDevice);
        break;
    case NvRmModuleID_Rtc:
        NV_ASSERT(!"RTC cannot be reset");
        break;
    case NvRmModuleID_Timer:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, TMR, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Uart:
        if( Instance == 0 )
        {
            RESET( hDevice, L, UARTA, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, L, UARTB, NVRM_RESET_DELAY );
        }
        else if ( Instance == 2)
        {
            RESET( hDevice, H, UARTC, NVRM_RESET_DELAY );
        } else if (Instance == 3)
        {
            RESET( hDevice, U, UARTD, NVRM_RESET_DELAY );
        } else if (Instance == 4)
        {
            RESET( hDevice, U, UARTE, NVRM_RESET_DELAY );
        } else
        {
            NV_ASSERT(!"Invalid UART instance");
        }
        break;
    case NvRmModuleID_Vfir:
        // Same as UARTB
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, UARTB, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Sdio:
        if( Instance == 0 )
        {
            RESET( hDevice, L, SDMMC1, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, L, SDMMC2, NVRM_RESET_DELAY );
        } else if (Instance == 2)
        {
            RESET( hDevice, U, SDMMC3, NVRM_RESET_DELAY );
        } else if (Instance == 3)
        {
            RESET( hDevice, L, SDMMC4, NVRM_RESET_DELAY );
        } else
        {
            NV_ASSERT(!"Invalid SDIO instance");
        }
        break;
    case NvRmModuleID_Spdif:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, SPDIF, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_I2s:
        if( Instance == 0 )
        {
            RESET( hDevice, L, I2S1, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, L, I2S2, NVRM_RESET_DELAY );
        } else
        {
            NV_ASSERT(!"Invalid I2S instance");
        }
        break;
    case NvRmModuleID_Nand:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, NDFLASH, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Tsensor:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, V, TSENSOR, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Twc:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, TWC, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Pwm:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, PWM, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Epp:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, EPP, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Vi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, VI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_3D:
        NV_ASSERT( Instance < 2 );
        // Always keep both 3D cores in sync
        RESET( hDevice, L, 3D, NVRM_RESET_DELAY );
        RESET( hDevice, V, 3D2, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_2D:
        NV_ASSERT( Instance == 0 );
        // RESET( hDevice, L, 2D, NVRM_RESET_DELAY );
        // WAR for bug 364497, se also RmReset2D()
        NV_ASSERT(!"2D reset after RM open is no longer allowed");
        break;
    case NvRmModuleID_Usb2Otg:
        if (Instance == 0)
        {
            RESET( hDevice, L, USBD, NVRM_RESET_DELAY );
        } else if (Instance == 1)
        {
            RESET( hDevice, H, USB2, NVRM_RESET_DELAY );
        } else if (Instance == 2)
        {
            RESET( hDevice, H, USB3, NVRM_RESET_DELAY );
        } else
        {
            NV_ASSERT(!"Invalid USB instance");
        }
        break;
    case NvRmModuleID_Isp:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, ISP, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Display:
        NV_ASSERT( Instance < 2 );
        if( Instance == 0 )
        {
            RESET( hDevice, L, DISP1, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, L, DISP2, NVRM_RESET_DELAY );
        }
        break;
    case NvRmModuleID_Vcp:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, VCP, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_CacheMemCtrl:
        NV_ASSERT( Instance < 2 );
        if( Instance == 0 )
        {
            NV_ASSERT(!"There is not such module on T30");
        }
        else if ( Instance == 1 )
        {
            RESET( hDevice, L, CACHE2, NVRM_RESET_DELAY );
        }
        break;
    case NvRmPrivModuleID_ApbDma:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, APBDMA, NVRM_RESET_DELAY );
        break;
    case NvRmPrivModuleID_Gpio:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, GPIO, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_GraphicsHost:
        // FIXME: should this be allowed?
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, L, HOST1X, NVRM_RESET_DELAY );
        break;
    case NvRmPrivModuleID_PcieXclk:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, U, PCIEXCLK, NVRM_RESET_DELAY );
        break;
    case NvRmPrivModuleID_Pcie:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, U, PCIE, NVRM_RESET_DELAY );
        break;
    case NvRmPrivModuleID_Afi:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, U, AFI, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_SyncNor:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, H, SNOR, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_AvpUcq:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, U, AVPUCQ, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_OneWire:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, U, OWR, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_HDA:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, V, HDA, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Apbif:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, V, APBIF, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_Audio:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, V, AUDIO, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_DAM:
        NV_ASSERT( Instance < 3 );
        if( Instance == 0 )
        {
            RESET( hDevice, V, DAM0, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, V, DAM1, NVRM_RESET_DELAY );
        }
        else if ( Instance == 2 )
        {
            RESET( hDevice, V, DAM2, NVRM_RESET_DELAY );
        }
        break;

    case NvRmModuleID_Sata:
        NV_ASSERT( Instance == 0 );
        RESET( hDevice, V, SATA, NVRM_RESET_DELAY );
        RESET( hDevice, V, SATA_OOB, NVRM_RESET_DELAY );
        break;
    case NvRmModuleID_ExtPeriphClk:
        NV_ASSERT( Instance < 3 );
        if( Instance == 0 )
        {
            RESET( hDevice, V, EXTPERIPH1, NVRM_RESET_DELAY );
        }
        else if( Instance == 1 )
        {
            RESET( hDevice, V, EXTPERIPH2, NVRM_RESET_DELAY );
        }
        else if ( Instance == 2 )
        {
            RESET( hDevice, V, EXTPERIPH3, NVRM_RESET_DELAY );
        }
        break;

    default:
        NV_ASSERT(!"Invalid ModuleId");
    }

    #undef RESET
}

// Safe PLLM (max 1000MHz) divider for GPU modules
#define NVRM_SAFE_GPU_DIVIDER (10)

#define NVRM_CONFIG_CLOCK(Module, SrcDef, DivNum) \
do\
{\
    reg = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
                  CLK_RST_CONTROLLER_CLK_SOURCE_##Module##_0); \
    if ((DivNum) > NV_DRF_VAL(CLK_RST_CONTROLLER, CLK_SOURCE_##Module, \
                            Module##_CLK_DIVISOR, reg)) \
    {\
        reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_##Module, \
                                 Module##_CLK_DIVISOR, (DivNum), reg); \
        NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
                CLK_RST_CONTROLLER_CLK_SOURCE_##Module##_0, reg); \
        NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY); \
    }\
    reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_##Module, \
                             Module##_CLK_SRC, SrcDef, reg); \
    NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
            CLK_RST_CONTROLLER_CLK_SOURCE_##Module##_0, reg); \
    NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY); \
    if ((DivNum) < NV_DRF_VAL(CLK_RST_CONTROLLER, CLK_SOURCE_##Module, \
                            Module##_CLK_DIVISOR, reg))\
    {\
        reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_##Module, \
                                 Module##_CLK_DIVISOR, (DivNum), reg); \
        NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), \
                CLK_RST_CONTROLLER_CLK_SOURCE_##Module##_0, reg); \
        NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY);\
    }\
} while(0)

void
NvRmBasicReset( NvRmDeviceHandle rm )
{
    NvU32 reg, ClkOutL, ClkOutH, ClkOutU, ClkOutV, ClkOutW;
    ExecPlatform env;

    // This takes the Big Hammer Approach.  Take everything out
    // of reset and enable all of the clocks. Then keep enabled only boot
    // clocks and graphics host.

    // save boot clock enable state
    ClkOutL = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);
    ClkOutH = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0);
    ClkOutU = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0);
    ClkOutV = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0);
    ClkOutW = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0);

    // Enable all module clocks
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_ENB_L_SET_0,
        CLK_RST_CONTROLLER_CLK_ENB_L_SET_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_ENB_H_SET_0,
        CLK_RST_CONTROLLER_CLK_ENB_H_SET_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
       CLK_RST_CONTROLLER_CLK_ENB_U_SET_0,
       CLK_RST_CONTROLLER_CLK_ENB_U_SET_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
       CLK_RST_CONTROLLER_CLK_ENB_V_SET_0,
       CLK_RST_CONTROLLER_CLK_ENB_V_SET_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
       CLK_RST_CONTROLLER_CLK_ENB_W_SET_0,
       CLK_RST_CONTROLLER_CLK_ENB_W_SET_0_WRITE_MASK );

    // The default clock source selection is out of range for some modules.
    // Just configure safe clocks so that reset is propagated correctly.
    env = NvRmPrivGetExecPlatform(rm);
    if (env == ExecPlatform_Soc)
    {
        /*
         * For peripheral modules default clock source is oscillator, and
         * it is safe. Special case SPDIFIN - set on PLLP_OUT0/(1+16/2)
         * and VDE - set on PLLP_OUT0/(1+1/2)
         */
        reg = NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                      CLK_RST_CONTROLLER_RST_DEVICES_L_0);
        if (reg & CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_SPDIF_RST_FIELD)
        {
            reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPDIF_IN,
                             SPDIFIN_CLK_SRC, PLLP_OUT0) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPDIF_IN,
                             SPDIFIN_CLK_DIVISOR, 16);
            NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0, reg);
        }
        NVRM_CONFIG_CLOCK(VDE, PLLP_OUT0, 1);

        /*
         * For graphic clocks use PLLM_OUT0 as a source, and set divider
         * so that initial frequency is below maximum module limit
         */
        NVRM_CONFIG_CLOCK(HOST1X, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(EPP, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(G2D, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(G3D, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(G3D2, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(MPE, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(VI, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
        NVRM_CONFIG_CLOCK(VI_SENSOR, PLLM_OUT0, NVRM_SAFE_GPU_DIVIDER);
    }

#if T30_DISPLAY_DEFAULT_SOURCE_PLLA
    {
        /* Switch display from default PLLA source (bug 773954) to main clock.
         * Need both "old" and "new" source clocks to run. Hence, temporarily
         * by-pass PLLA to provide clock for switching */
        NvU32 base =
            NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    CLK_RST_CONTROLLER_PLLA_BASE_0);
            reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLA_BASE,
                                     PLLA_BYPASS, ENABLE, base);
            NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    CLK_RST_CONTROLLER_PLLA_BASE_0, reg);
            NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY);

            reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_DISP1,
                             DISP1_CLK_SRC, CLK_M);
            NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0, reg);
            reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_DISP2,
                             DISP2_CLK_SRC, CLK_M);
            NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0, reg);
            NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY);

            NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    CLK_RST_CONTROLLER_PLLA_BASE_0, base);
    }
#endif

    // Make sure Host1x clock will be kept enabled
    ClkOutL = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L,
                                 CLK_ENB_HOST1X, ENABLE, ClkOutL);
    // Make sure VDE, BSEV and BSEA clocks will be kept disabled
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_VDE, DISABLE, ClkOutH);
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_BSEV, DISABLE, ClkOutH);
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_BSEA, DISABLE, ClkOutH);
#if !NVOS_IS_QNX
    // Make sure SNOR clock will be kept disabled
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_SNOR, DISABLE, ClkOutH);
#endif

    // Take modules out of reset
    NvOsWaitUS(NVRM_RESET_DELAY);
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_L_CLR_0,
        CLK_RST_CONTROLLER_RST_DEV_L_CLR_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_H_CLR_0,
        CLK_RST_CONTROLLER_RST_DEV_H_CLR_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_U_CLR_0,
        CLK_RST_CONTROLLER_RST_DEV_U_CLR_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_V_CLR_0,
        CLK_RST_CONTROLLER_RST_DEV_V_CLR_0_WRITE_MASK );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_W_CLR_0,
        CLK_RST_CONTROLLER_RST_DEV_W_CLR_0_WRITE_MASK );

    /* Put USB2 and USB3 controllers back into reset and keep clock disabled
       to reduce power. Usb driver will bring them out of the reset during
       initialization */
    reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_SET, SET_USB3_RST, 1) |
          NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_H_SET, SET_USB2_RST, 1);
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_RST_DEV_H_SET_0, reg);
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_USB2, DISABLE, ClkOutH);
    ClkOutH = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                                 CLK_ENB_USB3, DISABLE, ClkOutH);

    // restore clock enable state (= disable those clocks that
    // were disabled on boot)
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, ClkOutL );
    NV_REGW( rm, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, ClkOutH );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0, ClkOutU );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0, ClkOutV );
    NV_REGW( rm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0, ClkOutW );

        // FIXME: Don't know the reserved production fuse yet.
        /* enable hdcp and macrovision */
        // NvRmPrivContentProtectionFuses( rm );
}

#define NVRM_PCIE_REF_FREQUENCY (12000)

void NvRmPrivPllEControl(NvRmDeviceHandle hRmDevice, NvBool Enable)
{
    static NvBool s_Started = NV_FALSE;

    NvU32 base, reg, offset;

    if (NvRmPrivGetExecPlatform(hRmDevice) != ExecPlatform_Soc)
        return;

    if (NvRmPrivGetClockSourceFreq(NvRmClockSource_PllRef) !=
        NVRM_PCIE_REF_FREQUENCY)
    {
        NV_ASSERT(!"Not supported primary frequency");
        return;
    }

    // No run time power management for PCIE PLL - once started, it will never
    // be disabled
    if (s_Started || !Enable)
        return;

    s_Started = NV_TRUE;

    // Set PLLE base = 0x0D18C801 (configured, but disabled)
    offset = CLK_RST_CONTROLLER_PLLE_BASE_0;
    base= NV_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_ENABLE_CML, DISABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_ENABLE, DISABLE)     |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_PLDIV_CML,  0x0D)    |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_PLDIV,      0x18)    |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_NDIV,       0xC8)    |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE, PLLE_MDIV,       0x01);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        offset, base);

    // Remove clamping
    offset = APBDEV_PMC_PCX_EDPD_CNTRL_0;
    reg = NV_REGR(hRmDevice, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), offset);
    reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, PCX_EDPD_CNTRL, EN, 0x1, reg);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), offset, reg);

    NvOsWaitUS(NVRM_CLOCK_CHANGE_DELAY); // wait > 1us

    reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, PCX_EDPD_CNTRL, EN, 0x0, reg);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ), offset, reg);

    // Poll PLLE ready
    offset = CLK_RST_CONTROLLER_PLLE_MISC_0;
    do
    {
        reg = NV_REGR(hRmDevice,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ), offset);
    }
    while (!(NV_DRF_VAL(CLK_RST_CONTROLLER, PLLE_MISC, PLLE_PLL_READY, reg)));

    // Set PLLE base = 0xCD18C801 (configured and enabled)
    offset = CLK_RST_CONTROLLER_PLLE_BASE_0;
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLE_BASE, PLLE_ENABLE_CML, ENABLE, base);
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLE_BASE, PLLE_ENABLE, ENABLE, base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        offset, base);

    // use MIPI PLL delay for now - TODO: confirm or find PLLE specific
    NvOsWaitUS(NVRM_PLL_MIPI_STABLE_DELAY_US);
}

NvError
NvRmPrivOscDoublerConfigure(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz OscKHz)
{
    NvU32 reg;

    // Always disable doubler on T30
    reg = NV_DRF_NUM(
        CLK_RST_CONTROLLER, CLK_ENB_U_CLR, CLR_CLK_M_DOUBLER_ENB, 1);
    NV_REGW(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_CLK_ENB_U_CLR_0, reg);

    return NvError_NotSupported;
}

void
SetDsibClkToPllD(NvRmDeviceHandle hRmDevice)
{
    return;
}
