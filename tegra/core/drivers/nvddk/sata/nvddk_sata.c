/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_sata.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nverror.h"
#include "nvos.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"
#include "nvrm_memmgr.h"
#include "nvrm_pmu.h"

#include "ardev_t_fpci_sata0.h"
#include "ardev_t_ahci.h"
#include "dev_t_sata.h"
#include "ardev_t_sata.h"
#include "arapb_misc.h"
#include "arclk_rst.h"
#include "arapbpm.h"

// Sector size is 512 bytes.
#define SECTOR_SIZE_LOG2    9
#define NV_ADDRESS_MAP_CLK_RST_BASE     0x60006000
#define NV_ADDRESS_MAP_APB_PMC_BASE     0x7000e400
#define NV_ADDRESS_MAP_SATA_BASE        0x70020000
#define NV_ADDRESS_MAP_MISC_BASE        0x70000000
#define NV_ADDRESS_MAP_XUSB_BASE        0x7009f000

#define NV_SATA_APB_DFPCI_CFG           0x70021000
#define NV_SATA_APB_BAR0_START          0x70022000
#define NV_SATA_APB_BAR5_START          0x70027000
#define SATA_AUX_PAD_PLL_CNTL_1_0       0x1100
#define SATA_AUX_PAD_PLL_CNTL_1_0_REFCLK_SEL_INT_CML    0
#define SATA_AUX_PAD_PLL_CNTL_1_0_REFCLK_SEL_RANGE      12:11
#define SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET_SHIFT         _MK_SHIFT_CONST(6)
#define SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET_FIELD         _MK_FIELD_CONST(0x1, SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET_SHIFT)
#define SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET_RANGE         6:6
#define XUSB_PADCTL_USB3_PAD_MUX_0                      0x134
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0                 0x138
#define XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_3_0           0x150
#define XUSB_PADCTL_ELPG_PROGRAM_0                      0x1c
#define SATA_CONFIGURAION_0                             0x180
#define T_AHCI_HBA_GHC                                  0x004
#define PXCLB                                           0x100
#define PXFB                                            0x108
#define PXIE                                            0x114
#define PXCI                                            0x138
#define PXIS                                            0x110
#define PXSERR                                          0x130
#define PXTFD                                           0x120
#define PXCMD                                           0x118
#define SQUELCH_OFFSET                                  0x1114
#define DEVSLP_OFFSET                                   0x1108
#define SQUELCH_SETTINGS                                0x1120
#define T_SATA0_BKDOOR_CC                               0x14A4
#define T_SATA0_CFG_1                                   0x1004
#define T_SATA0_CFG_LINK_0                              0x1174
#define T_SATA0_INDEX                                   0x1680
#define T_SATA0_CTRL                                    0x1540
#define T_SATA0_CFG_9                                   0x1024
#define SATA_FPCI_BAR5_0                                0x0094
#define SATA_INTR_MASK_0                                0x188
#define FORCE_SATA_PAD_IDDQ_DISABLE_MASK0               (1 << 6)
#define SDS_SUPPORT                                     (1 << 13)

#define PLL_BASE_DIVP_MASK              (0x7<<20)
#define PLL_BASE_DIVP_SHIFT             20
#define PLL_BASE_DIVN_MASK              (0x3FF<<8)
#define PLL_BASE_DIVN_SHIFT             8
#define PLL_BASE_DIVM_MASK              (0x1F)
#define PLL_BASE_DIVM_SHIFT             0

#define PLLE_BASE_DIVCML_SHIFT          24
#define PLLE_BASE_DIVCML_MASK           (0xf<<PLLE_BASE_DIVCML_SHIFT)

#define PLLE_BASE_DIVN_MASK             (0xFF<<PLL_BASE_DIVN_SHIFT)
#define PLLE_BASE_DIVM_MASK             (0xFF<<PLL_BASE_DIVM_SHIFT)

#define NVDDK_SW_CYA_SATA_8KB_PAGE_SIZE_SUPPORT_DISABLE 0
#define FPCI_BAR5_0_FINAL_VALUE 0x40020100

#define NV_READ32(a)        *((const volatile NvU32 *)(a))
#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)

#define NVDDK_CLOCKS_PLL_STABILIZATION_DELAY 300
#define NVDDK_CLOCKS_PLL_STABILIZATION_DELAY_AFTER_LOCK 100

#define PLLE_BASE_CONFIG    0x4d00c801
#define PLLE_MISC_CONFIG    0x00070300
#define PLLE_CML1_OEN       0x2

#define XUSB_PADCTL_ELPG_PROGRAM_0_CONFIG 0x00770000
#define FORCE_SATA_PAD_IDDQ_DISABLE_MASK0_DISABLE 0x08000040
#define SATA_PADPLL_IDDQ2LANE_RESET_VALUE 0x00002008
#define SATA_CONFIGURAION_0_EN_FPCI 0x800e8e41
#define SQUELCH_EN 0x4d000000
#define SQUELCH_GEN3_WAR 0x3328A5AC
#define T_SATA0_BKDOOR_CC 0x14A4
#define T_SATA0_BKDOOR_CC_CLASS_CODE 0x01060000
#define T_SATA0_BKDOOR_CC_PROG_IF 0x00008500
#define T_SATA0_CFG_1_IO_SPACE_ENABLED                   0x00000001
#define T_SATA0_CFG_1_MEMORY_SPACE_ENABLED               0x00000010
#define T_SATA0_CFG_LINK_0_CONFIG 0x00dd239A
#define T_SATA0_INDEX_CH1_SELECTED 0x00000001
#define T_SATA0_CFG_1_CONFIG 0x00b00147
#define T_SATA0_CTRL_ADMA_SPACE_EN 0x60300007
#define T_SATA0_CFG_9_ABAR 0x40020000
#define SATA_FPCI_BAR5_0_FPCI_BAR5_START 0x00400200
#define SATA_FPCI_BAR5_0_FPCI_BAR5_ACCESS_TYPE 0x00000001
#define T_AHCI_HBA_GHC_AE_ENANLE 0x80000000
#define T_AHCI_HBA_GHC_HR_ENANLE 0x00000001
#define SATA_INTR_MASK_0_IP_INT_MASK 0x00010000
#define T_AHCI_HBA_GHC_IE 0x80000002
#define PXIE_CONFIG 0x00400040
#define PXIS_ERROR_CLEAR 0x05950000
#define PXCMD_CONFIG 0x00000016
#define PXIS_CLEAR 0xFFFFFFFF
#define PXSERR_CLEAR 0xFFFFFFFF
#define PXCMD_ST 0x4017
#define PXIE_DPE 0x20
#define SATA_COMMAND_DEFAULT 0x00008027
#define SATA_COMMAND_PIO_READ 0x00200000
#define SATA_COMMAND_DMA_READ 0x00c80000
#define SATA_START_SECTOR_DEFAULT 0x40000000
#define SATA_NUM_SECTORS_DEFAULT 0x80000000
#define SATA_COMMAND_PIO_WRITE 0x00300000
#define SATA_COMMAND_DMA_WRITE 0x00ca0000
#define SATA_COMMAND_IDENTIFY 0x90ec0000
#define SATA_COMMAND_ERASE 0x00c00000
#define SATA_PXCI_COUNT 1000000
#define SATA_PXIS_COUNT 100
#define XUSB_PLL_CONFIG 0x3130000A
#define XUSB_PAD_CONFIG 0x00000200

#define CLKRST_REGR(Reg, value)                                               \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_CLK_RST_BASE +                       \
            CLK_RST_CONTROLLER_##Reg##_0);                                    \
    } while (0)

#define CLKRST_REGW(Reg, value)                                               \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_CLK_RST_BASE +                             \
            CLK_RST_CONTROLLER_##Reg##_0),                                    \
                   value);                                                    \
    } while (0)

#define XUSB_REGR(Reg, value)                                               \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_XUSB_BASE +                       \
            XUSB_PADCTL_##Reg##_0);                                    \
    } while (0)

#define XUSB_REGW(Reg, value)                                               \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_XUSB_BASE +                             \
            XUSB_PADCTL_##Reg##_0),                                    \
                   value);                                                    \
    } while (0)

static NvDdkSataContext gs_SataContext;
static NvDdkSataParams s_DefaultSataParams;
static NvDdkSataParams *s_pSataParams = NULL;
static NvDdkSataStatus *s_pSataBitInfo;
static NvU8 *s_pCommandListBaseAddres;
static NvU8 *s_pFisBaseAddress;

static void SetSataOobClockSource(void);

static void SetSataClockSource(void);

static NvError NvDdkSataReadSectors(
    NvU32 StartSector,
    NvU32 NumSectors,
    NvU8* Buffer);

static NvError NvDdkSataWriteSectors(
    NvU32 StartSector,
    NvU32 NumSectors,
    const NvU8* Buffer);

static NvError NvDdkSataHwInit(void);

static NvError SataEnablePowerRail(NvBool IsEnable)
{
    const NvOdmPeripheralConnectivity *pPeripheralConn = NULL;
    NvRmPmuVddRailCapabilities RailCaps;
    NvRmDeviceHandle hRmDevice;
    NvError e = NvSuccess;
    NvU32 Voltage;
    NvU32 i;

    e = NvRmOpen(&hRmDevice, 0);
    if (e != NvSuccess)
    {
        goto fail;
    }

    pPeripheralConn = NvOdmPeripheralGetGuid(
                          NV_ODM_GUID('N','V','D','D','S','A','T','A'));

    if (pPeripheralConn == NULL)
    {
        SATA_ERROR_PRINT(("%s: get guid failed\n", __func__));
        goto fail;
    }

    for (i = 0; i < pPeripheralConn->NumAddress; i++)
    {
        // Search for the vdd rail entry in sata
        if ((pPeripheralConn->AddressList[i].Interface == NvOdmIoModule_Vdd) &&
             pPeripheralConn->AddressList[i].Address)
        {
            NvRmPmuGetCapabilities(hRmDevice,
                pPeripheralConn->AddressList[i].Address,
                &RailCaps);

            Voltage = IsEnable ? RailCaps.requestMilliVolts : 0;

            NvRmPmuSetVoltage(hRmDevice, pPeripheralConn->AddressList[i].Address,
                Voltage, NULL);
        }
    }
    return NvSuccess;

fail:
    SATA_ERROR_PRINT(("%s: enable power rail failed\n", __func__));
    return NvError_NotInitialized;
}

static void SetSataOobClockSource(void)
{
    NvU32 RegValue = 0;

    CLKRST_REGR(CLK_SOURCE_SATA_OOB, RegValue);

    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SATA_OOB,
           SATA_OOB_CLK_DIVISOR, s_pSataParams->SataOobClockDivider, RegValue);

   switch (s_pSataParams->SataOobClockSource)
    {
        case NvDdkSataClockSource_ClkM:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA_OOB,
                SATA_OOB_CLK_SRC,
                CLK_M,
                RegValue);
            break;

        case NvDdkSataClockSource_PllPOut0:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA_OOB,
                SATA_OOB_CLK_SRC,
                PLLP_OUT0,
                RegValue);
            break;

        case NvDdkSataClockSource_PllMOut0:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA_OOB,
                SATA_OOB_CLK_SRC,
                PLLM_OUT0,
                RegValue);
            break;

        default:
            break;
    }
    CLKRST_REGW(CLK_SOURCE_SATA_OOB, RegValue);

    s_pSataBitInfo->SataOobClockSource = s_pSataParams->SataOobClockSource;
    s_pSataBitInfo->SataOobClockDivider = s_pSataParams->SataOobClockDivider;
}

static void SetSataClockSource(void)
{
    NvU32 RegValue = 0;

    CLKRST_REGR(CLK_SOURCE_SATA, RegValue);

    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SATA,
        SATA_CLK_DIVISOR, s_pSataParams->SataClockDivider, RegValue);

    switch (s_pSataParams->SataClockSource)
    {
        case NvDdkSataClockSource_ClkM:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA,
                SATA_CLK_SRC,
                CLK_M,
                RegValue);
            break;

        case NvDdkSataClockSource_PllPOut0:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA,
                SATA_CLK_SRC,
                PLLP_OUT0,
                RegValue);
            break;

        case NvDdkSataClockSource_PllMOut0:
            RegValue = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
                CLK_SOURCE_SATA,
                SATA_CLK_SRC,
                PLLM_OUT0,
                RegValue);
            break;

        default:
            break;
    }
    CLKRST_REGW(CLK_SOURCE_SATA, RegValue);

    s_pSataBitInfo->SataClockSource = s_pSataParams->SataClockSource;
    s_pSataBitInfo->SataClockDivider = s_pSataParams->SataClockDivider;

}

static NvError
EnablePLLE(void)
{
    NvU32 RegValue = 0;

    // Move REFPLLE and PLLE out of IDDQ
    CLKRST_REGR(PLLREFE_MISC, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                   PLLREFE_MISC, PLLREFE_IDDQ, 0, RegValue);
    CLKRST_REGW(PLLREFE_MISC, RegValue);

    CLKRST_REGR(PLLE_MISC, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
                   PLLE_IDDQ_OVERRIDE_VALUE, 0, RegValue);
    CLKRST_REGW(PLLE_MISC, RegValue);

    NvOsWaitUS(5);

    // configure REFPLLE
    CLKRST_REGR(PLLREFE_BASE, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
                   PLLREFE_MDIV, 1, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
                   PLLREFE_NDIV, 25, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
                   PLLREFE_PLDIV, 0, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
                   PLLREFE_KVCO, 480, RegValue);
    // enable PLL
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
                   PLLREFE_ENABLE, 1, RegValue);
    CLKRST_REGW(PLLREFE_BASE, RegValue);

    // enable REFPLLE LOCK output
    CLKRST_REGR(PLLREFE_MISC, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_MISC,
                   PLLREFE_LOCK_ENABLE, 1, RegValue);
    CLKRST_REGW(PLLREFE_MISC, RegValue);

    // poll to ensure REFPLLE is locked
    do
    {
        CLKRST_REGR(PLLREFE_MISC, RegValue);
    }while (!(RegValue & CLK_RST_CONTROLLER_PLLREFE_MISC_0_PLLREFE_LOCK_FIELD));

    // configure PLLE_BASE
    CLKRST_REGW(PLLE_BASE, PLLE_BASE_CONFIG);

    //configure PLLE_MISC
    CLKRST_REGW(PLLE_MISC, PLLE_MISC_CONFIG);

    //configure PLLE_AUX
    CLKRST_REGR(PLLE_AUX, RegValue);
    RegValue = RegValue | PLLE_CML1_OEN;
    CLKRST_REGW(PLLE_AUX, RegValue);

    //poll to ensure PLLE is locked
    do
    {
        CLKRST_REGR(PLLE_MISC, RegValue);
    }while (!(RegValue & CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_FIELD));

    return NvSuccess;
}

NvError
NvDdkSataInit(void)
{
    NvU32 RegValue = 0;
    NvError e = NvSuccess;

    s_pSataParams = &s_DefaultSataParams;

    // Set sata mode to Ahci i.e., DMA mode
    s_DefaultSataParams.SataMode = NvDdkSataMode_Ahci;

    /// Specifies the clock source for SATA controller.
    s_DefaultSataParams.SataClockSource = NvDdkSataClockSource_PllPOut0;

    /// Specifes the clock divider to be used.
    s_DefaultSataParams.SataClockDivider = 6;

    /// Specifies the clock source for SATA controller.
    s_DefaultSataParams.SataOobClockSource = NvDdkSataClockSource_PllPOut0;

    /// Specifes the clock divider to be used.
    s_DefaultSataParams.SataOobClockDivider = 2;

    if (!gs_SataContext.IsSataInitialized)
    {
        // Enable Sata Power rails
        e = SataEnablePowerRail(NV_TRUE);
        if (e != NvSuccess)
            goto fail;

        // Once only Initialization of SATA controller
        // Step 1 This is warm reset.
        CLKRST_REGR(RST_DEVICES_V, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_V,
                       SWR_SATA_RST,
                       CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_SATA_RST_ENABLE,
                       RegValue);
        // There's no signal corresponding to SWR_SATA_OOB_RST. Therefore, don't
        // program SWR_SATA_OOB_RST
        CLKRST_REGW(RST_DEVICES_V, RegValue);

        // Cold reset bit for sata init
        CLKRST_REGR(RST_DEV_W_SET, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_W_SET,
                       SET_SATACOLD_RST, 1, RegValue);
        CLKRST_REGW(RST_DEV_W_SET, RegValue);

        // Step 2(a) SATA OOB clock should be 216MHz using PLLP_OUT0 as source
        SetSataOobClockSource();

        //SATA controller clock should be 108MHz using PLLP_OUT0 as source
        // Step 2(b)
        SetSataClockSource();

        // Step 2(c) Enable Clocks to SATA controller and SATA OOB
        CLKRST_REGR(CLK_OUT_ENB_V, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_V,
                       CLK_ENB_SATA, 1, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_V,
                       CLK_ENB_SATA_OOB, 1, RegValue);
        CLKRST_REGW(CLK_OUT_ENB_V, RegValue);

        // *******************************************************
        // Removing Clamps for SAX Partition, this is required
        // just after power on SAX Partition
        // *******************************************************
        NV_WRITE32(0x7000e434, 0x100);

        // *******************************************************
        // Disable RESET to SATA
        // CLK_RST_CONTROLLER_RST_DEV_W_CLR_0[CLR_SATACOLD_RST] <= 1
        // CLK_RST_CONTROLLER_RST_DEV_V_CLR_0[CLR_SATA_RST]     <= 1
        // CLK_RST_CONTROLLER_RST_DEV_V_CLR_0[SET_SATA_OOB_RST] <= 1
        // *******************************************************
        CLKRST_REGR(RST_DEV_V_CLR, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_V_CLR,
                CLR_SATA_RST, 1, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_V_CLR,
                CLR_SATA_OOB_RST, 1, RegValue);
        CLKRST_REGW(RST_DEV_V_CLR, RegValue);

        CLKRST_REGR(RST_DEV_W_CLR, RegValue);
        RegValue = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_W_CLR,
                CLR_SATACOLD_RST, 1, RegValue);
        CLKRST_REGW(RST_DEV_W_CLR, RegValue);

        // Step 8 Enable PLLE
        EnablePLLE();

        //pad_config
        XUSB_REGW(IOPHY_PLL_S0_CTL1, XUSB_PLL_CONFIG);
        XUSB_REGW(IOPHY_MISC_PAD_S0_CTL_3, XUSB_PAD_CONFIG);

        e = NvDdkSataHwInit();
        if (e == NvSuccess)
        {
            SATA_DEBUG_PRINT(("Sata Init Success\n"));
        }
        else
        {
            SATA_ERROR_PRINT(("Sata Init Failed\n"));
        }
    }

fail:
    return e;
}

static NvError NvDdkSataHwInit(void)
{
    NvError e = NvSuccess;
    NvU32 RegValue;
    NvU32 d;
    NvU32 Pxis;
    NvU32 Count;
    NvU32 Pxtfd;
    NvU8 *pPxfbAddress;
    NvRmMemHandle hRmMemHandle = NULL;
    NvRmMemHandle hRmMemHandle1 = NULL;
    NvRmMemHandle hRmMemHandle2 = NULL;
    NvRmDeviceHandle hRm;

    d = NV_READ32(0x70001130);
    d = d | (1 << 2) | (1 << 3);
    NV_WRITE32(0x70001130, d);

    // PROGRAMMING REQUIRED TO UNBLOCK CONTROLLER TO PAD PATH
    // *******************************************************
    // XUSB_PADCTL_ELPG_PROGRAM_0[AUX_MUX_LP0_VCORE_DOWN]     <= NO
    // XUSB_PADCTL_ELPG_PROGRAM_0[AUX_MUX_LP0_CLAMP_EN_EARLY] <= NO
    // XUSB_PADCTL_ELPG_PROGRAM_0[AUX_MUX_LP0_CLAMP_EN]       <= NO
    // *******************************************************
    RegValue = 0;
    RegValue |= XUSB_PADCTL_ELPG_PROGRAM_0_CONFIG;
    XUSB_REGW(ELPG_PROGRAM, RegValue);

    // *******************************************************
    // XUSB_PADCTL_USB3_PAD_MUX_0[FORCE_SATA_PAD_IDDQ_DISABLE_MASK0] <= DISABLED
    // *******************************************************
    RegValue = 0;
    RegValue |= FORCE_SATA_PAD_IDDQ_DISABLE_MASK0_DISABLE;
    XUSB_REGW(USB3_PAD_MUX, RegValue);

    // *******************************************************
    // CLK_RST_CONTROLLER_SATA_PLL_CFG1_0[SATA_LANE_IDDQ2_PADPLL_RESETDLY]<=0x0
    // CLK_RST_CONTROLLER_SATA_PLL_CFG1_0[SATA_PADPLL_IDDQ2LANE_SLUMBERDLY]<=0x0
    // *******************************************************
    RegValue = 0;
    RegValue |= SATA_PADPLL_IDDQ2LANE_RESET_VALUE;
    NV_WRITE32(NV_ADDRESS_MAP_CLK_RST_BASE +
        CLK_RST_CONTROLLER_SATA_PLL_CFG1_0, RegValue);
    NvOsWaitUS(100000);

    // SATA IPFS and SATA HBA Initialization
    // *******************************************************
    // Enable FPCI Transactions in IPFS
    // SATA_CONFIGURAION_0[EN_FPCI] <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= SATA_CONFIGURAION_0_EN_FPCI;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SATA_CONFIGURAION_0, RegValue);

    // *******************************************************
    // SQUELCH
    // *******************************************************
    RegValue = 0;
    RegValue |= SQUELCH_EN;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SQUELCH_OFFSET, RegValue);

    // *******************************************************
    // Some WAR for GEN3 drive only, without this GEN3 will be detected in GEN1
    // Also has some SQELCH related settings
    // *******************************************************
    RegValue = 0;
    RegValue |= SQUELCH_GEN3_WAR;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SQUELCH_SETTINGS, RegValue);

    // *******************************************************
    // T_SATA0_BKDOOR_CC[CLASS_CODE] <= 0x0106
    // T_SATA0_BKDOOR_CC[PROG_IF]    <= 0x85
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_BKDOOR_CC_CLASS_CODE;
    RegValue |= T_SATA0_BKDOOR_CC_PROG_IF;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_BKDOOR_CC, RegValue);

    // *******************************************************
    // T_SATA0_CFG_1
    // CFG.CMD[IOSE] <= 1
    // CFG.CMD[MSE]  <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_CFG_1_IO_SPACE_ENABLED;
    RegValue |= T_SATA0_CFG_1_MEMORY_SPACE_ENABLED;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_CFG_1, RegValue);

    // *******************************************************
    // T_SATA0_CFG_LINK_0[DEBOUNCE_PHYRDY]        <= YES
    // T_SATA0_CFG_LINK_0[DEBOUNCE_PHYRDY_PERIOD] <= SHORT
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_CFG_LINK_0_CONFIG;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_CFG_LINK_0, RegValue);

    // *******************************************************
    // T_SATA0_INDEX[CH1] <= SELECTED
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_INDEX_CH1_SELECTED;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_INDEX, RegValue);

    // *******************************************************
    // T_SATA0_CFG_1
    // CFG.CMD[MSE] => 1
    // CFG.CMD[BME] => 1
    // CFG.CMD[SEE] => 1
    // CFG.CMD[PEE] => 1
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_CFG_1_CONFIG;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_CFG_1, RegValue);

    // *******************************************************
    // T_SATA0_CTRL[ADMA_SPACE_EN] <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_CTRL_ADMA_SPACE_EN;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_CTRL, RegValue);

    // *******************************************************
    // T_SATA0_CFG_9
    // CFG.ABAR[BA] <= 0x40020000 >> 12 (BA == 31:13)
    // *******************************************************
    RegValue = 0;
    RegValue |= T_SATA0_CFG_9_ABAR;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + T_SATA0_CFG_9, RegValue);

    // *******************************************************
    // SATA_FPCI_BAR5_0[FPCI_BAR5_START]       <= 0x0040020
    // SATA_FPCI_BAR5_0[FPCI_BAR5_ACCESS_TYPE] <= 0x1
    // *******************************************************
    RegValue = 0;
    RegValue |= SATA_FPCI_BAR5_0_FPCI_BAR5_START;
    RegValue |= SATA_FPCI_BAR5_0_FPCI_BAR5_ACCESS_TYPE;
    NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SATA_FPCI_BAR5_0, RegValue);

    // *******************************************************
    // T_AHCI_HBA_GHC
    // HBA.GHC[AE] <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= T_AHCI_HBA_GHC_AE_ENANLE;
    NV_WRITE32(NV_SATA_APB_BAR5_START + T_AHCI_HBA_GHC, RegValue);

    // *******************************************************
    // T_AHCI_HBA_GHC
    // HBA.GHC[HR] <= 1
    // *******************************************************
    RegValue = NV_READ32(NV_SATA_APB_BAR5_START + T_AHCI_HBA_GHC);
    RegValue |= T_AHCI_HBA_GHC_HR_ENANLE;
    NV_WRITE32(NV_SATA_APB_BAR5_START + T_AHCI_HBA_GHC, RegValue);

    // *******************************************************
    // SATA_INTR_MASK_0[IP_INT_MASK] <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= SATA_INTR_MASK_0_IP_INT_MASK;
    NV_WRITE32(NV_SATA_APB_BAR5_START + SATA_INTR_MASK_0, RegValue);

    // *******************************************************
    // T_AHCI_HBA_GHC
    // HBA.GHC[IE] <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= T_AHCI_HBA_GHC_IE;
    NV_WRITE32(NV_SATA_APB_BAR5_START + T_AHCI_HBA_GHC, RegValue);

    NvRmOpen(&hRm, 1);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 256, NvOsMemAttribute_WriteCombined,
        256, 0, 1, &hRmMemHandle));
    pPxfbAddress = (NvU8*)NvRmMemPin(hRmMemHandle);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 1024, NvOsMemAttribute_WriteCombined,
        1024, 0, 1, &hRmMemHandle1));
    s_pCommandListBaseAddres = (NvU8*)NvRmMemPin(hRmMemHandle1);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 1024, NvOsMemAttribute_WriteCombined,
        1024, 0, 1, &hRmMemHandle2));
    s_pFisBaseAddress = (NvU8*)NvRmMemPin(hRmMemHandle2);

    // *******************************************************
    // PXCLB[CLB] <= SDRAM address
    // *******************************************************
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCLB, (NvU32)s_pCommandListBaseAddres);

    NV_WRITE32(NV_SATA_APB_BAR5_START + PXFB, (NvU32)pPxfbAddress);

    // *******************************************************
    // PXIE[PCE]  <= 1
    // PXIE[PCRE] <= 1
    // *******************************************************a
    RegValue = 0;
    RegValue |= PXIE_CONFIG;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXIE, RegValue);
    NvOsWaitUS(500000);

    // Poll for PXIS[PCS] <= 1 && PXIS[PCRS] <= 1
    Count = 0;
    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    while ((Pxis & 0x00400040) != 0x00400040)
    {
        Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
        Count++;
        if (Count == SATA_PXIS_COUNT)
            break;
    }

    SATA_DEBUG_PRINT(("PXIS <= %08x\n",Pxis));
    SATA_DEBUG_PRINT(("PXSERR <= %08x\n",
        NV_READ32(NV_SATA_APB_BAR5_START + PXSERR)));

    if (Count == SATA_PXIS_COUNT)
    {
        SATA_ERROR_PRINT(("Sata Init: Pxis failed\n"));
        e = NvError_Timeout;
        goto fail;
    }

    // Clear PXIS[PCS] && PXIS[PCRS] && Other error sources
    RegValue = 0;
    RegValue |= PXIS_ERROR_CLEAR;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXSERR, RegValue);

    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    SATA_DEBUG_PRINT(("PXIS <= %08x\n",Pxis));

    RegValue = 0;
    RegValue |= PXCMD_CONFIG;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCMD, RegValue);

    // *******************************************************
    // Poll for PxTFD[STS.ERR] <= 0
    // Poll for PxTFD[STS.DRQ] <= 0
    // Poll for PxTFD[STS.BSY] <= 0
    // *******************************************************
    Count = 0;
    Pxtfd = NV_READ32(NV_SATA_APB_BAR5_START + PXTFD);
    while ((Pxtfd & 0x85) != 0)
    {
       Pxtfd = NV_READ32(NV_SATA_APB_BAR5_START + PXTFD);
        Count++;
        if (Count == SATA_PXCI_COUNT)
            break;
    }

    SATA_DEBUG_PRINT(("PXTFD <= %08x\n",Pxtfd));
    if (Count == SATA_PXCI_COUNT)
    {
        SATA_ERROR_PRINT(("Sata Init: Pxtfd failed\n"));
        e = NvError_Timeout;
        goto fail;
    }

    RegValue = 0;
    RegValue |= PXIS_CLEAR;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXIS, RegValue);

    RegValue = 0;
    RegValue |= PXSERR_CLEAR;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXSERR, RegValue);

    // PXCMD[ST] <= 1
    RegValue = 0;
    RegValue |= PXCMD_ST;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCMD, RegValue);

    gs_SataContext.IsSataInitialized = NV_TRUE;
    return e;
fail:
    if (hRmMemHandle)
    {
        NvRmMemUnpin(hRmMemHandle);
        NvRmMemHandleFree(hRmMemHandle);
    }
    if (hRmMemHandle1)
    {
        NvRmMemUnpin(hRmMemHandle1);
        NvRmMemHandleFree(hRmMemHandle1);
    }
    if (hRmMemHandle2)
    {
        NvRmMemUnpin(hRmMemHandle2);
        NvRmMemHandleFree(hRmMemHandle2);
    }

    NvRmClose(hRm);
    return e;
}

static NvError NvDdkSataReadSectors(
    NvU32 StartSector,
    NvU32 NumSectors,
    NvU8* Buffer)
{
    NvError e = NvSuccess;
    NvU32 Count;
    NvU32 Pxis;
    NvU32 Pxci;
    NvU32 RegValue;
    NvU8 *Buf;
    NvRmMemHandle hRmMemHandle = NULL;
    NvRmDeviceHandle hRm;

    SATA_DEBUG_PRINT(("SataRead : StartSector=%d, NO.Sectors = %d\n", \
        StartSector, NumSectors));

    NvRmOpen(&hRm, 1);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 4096, NvOsMemAttribute_WriteCombined,
        NumSectors * (1 << SECTOR_SIZE_LOG2), 0,1, &hRmMemHandle));


    Buf = (NvU8*)NvRmMemPin(hRmMemHandle);

    // *******************************************************
    // PXIE[DPE]   <= 1
    // *******************************************************
    RegValue = 0;
    RegValue |= PXIE_DPE;
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXIE, RegValue);

    NV_WRITE32((NvU32)s_pCommandListBaseAddres, 0x00010005);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x4,
        NumSectors * (1 << SECTOR_SIZE_LOG2));
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x8,
        (NvU32)s_pFisBaseAddress);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0xc, 0x0);

    // *******************************************************
    // Command Table for Command 0 in Command List
    // Identify Device Command
    // *******************************************************
    RegValue = 0;
    RegValue |= SATA_COMMAND_DEFAULT;
    if (s_pSataParams->SataMode == NvDdkSataMode_Ahci)
    {
        RegValue |= SATA_COMMAND_DMA_READ;
    }
    else if (s_pSataParams->SataMode == NvDdkSataMode_Legacy)
    {
        RegValue |= SATA_COMMAND_PIO_READ;
    }
    else
    {
        e = NvError_BadValue;
        SATA_ERROR_PRINT(("Sata Read: UnsupportedMode: %d\n", \
            s_pSataParams->SataMode));
        goto fail;
    }

    NV_WRITE32((NvU32)s_pFisBaseAddress, RegValue);

    RegValue = 0;
    RegValue |= SATA_START_SECTOR_DEFAULT;
    RegValue |= StartSector;
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x4, RegValue);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0xc, NumSectors);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x10, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x80, (NvU32)Buf);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x84, 0x0);

    RegValue = 0;
    RegValue |= SATA_NUM_SECTORS_DEFAULT;
    RegValue |= ((NumSectors * (1 << SECTOR_SIZE_LOG2)) - 1);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8c, RegValue);

    // *******************************************************
    // PXCI <= 0x1
    // *******************************************************
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCI, 0x1);

    // *******************************************************
    // Poll for PXCI <= 0x0
    // *******************************************************
    Count = 0;
    Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
    while (Pxci != 0x0)
    {
        Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
        Count++;
        if (Count == SATA_PXCI_COUNT)
            break ;
    }

    if (Count == SATA_PXCI_COUNT)
    {
        SATA_ERROR_PRINT(("sata read: pxci failed\n"));
        e = NvError_Timeout;
        goto fail;
    }

    Count = 0;
    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    while ((Pxis & 0x20) != 0x20)
    {
        Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
        Count++;
        if (Count == SATA_PXIS_COUNT)
            break ;
    }

    if (Count == SATA_PXIS_COUNT)
        SATA_ERROR_PRINT(("sata read: pxis failed\n"));

    NvOsMemcpy(Buffer, Buf, NumSectors * 512);

fail:
    if (hRmMemHandle)
    {
        NvRmMemUnpin(hRmMemHandle);
        NvRmMemHandleFree(hRmMemHandle);
    }
    NvRmClose(hRm);
    return e;
}

static NvError NvDdkSataWriteSectors(
    NvU32 StartSector,
    NvU32 NumSectors,
    const NvU8* Buffer)
{
    NvError e = NvSuccess;
    NvU32 Count;
    NvU32 Pxci;
    NvU32 Pxis;
    NvU32 RegValue;
    NvU8* Temp;
    NvRmMemHandle hRmMemHandle = NULL;
    NvRmDeviceHandle hRm;

    SATA_DEBUG_PRINT(("SataWrite: StartSector=%d, NO.Sectors = %d\n", \
        StartSector, NumSectors));

    NvRmOpen(&hRm, 1);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 4096, NvOsMemAttribute_WriteCombined,
        NumSectors * (1 << SECTOR_SIZE_LOG2), 0, 1, &hRmMemHandle));

    Temp = (NvU8*)NvRmMemPin(hRmMemHandle);

    NvOsMemcpy(Temp, Buffer, NumSectors * (1 << SECTOR_SIZE_LOG2));

    NV_WRITE32(0x70027114, 0x21);

    NV_WRITE32((NvU32)s_pCommandListBaseAddres, 0x00010045);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x4,
        NumSectors * ((1 << SECTOR_SIZE_LOG2)));
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x8,
        (NvU32)s_pFisBaseAddress);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0xc, 0x00000000);

    RegValue = 0;
    RegValue |= SATA_COMMAND_DEFAULT;
    if (s_pSataParams->SataMode == NvDdkSataMode_Ahci)
    {
        RegValue |= SATA_COMMAND_DMA_WRITE;
    }
    else if (s_pSataParams->SataMode == NvDdkSataMode_Legacy)
    {
        RegValue |= SATA_COMMAND_PIO_WRITE;
    }
    else
    {
        e = NvError_BadValue;
        SATA_ERROR_PRINT(("Sata Write: UnsupportedMode: %d\n", \
            s_pSataParams->SataMode));
        goto fail;
    }

    NV_WRITE32((NvU32)s_pFisBaseAddress, RegValue);

    RegValue = 0;
    RegValue |= SATA_START_SECTOR_DEFAULT;
    RegValue |= StartSector;
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x4, RegValue);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0xc, NumSectors);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x10, 0x0);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x80, (NvU32)Temp);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x84, 0x0);

    RegValue = 0;
    RegValue |= SATA_NUM_SECTORS_DEFAULT;
    RegValue |= ((NumSectors * (1 << SECTOR_SIZE_LOG2)) - 1);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8c, RegValue);

    // *******************************************************
    // PXCI <= 0x1
    // *******************************************************
    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCI, 0x1);

    // *******************************************************
    // Poll for PXCI <= 0x0
    // *******************************************************
    Count = 0;
    Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
    while (Pxci != 0x0)
    {
      Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
      Count = Count + 1;
      if (Count == SATA_PXCI_COUNT)
        break ;
    }

    if (Count == SATA_PXCI_COUNT)
    {
      SATA_ERROR_PRINT(("sata write: pxci failed\n"));
      e = NvError_Timeout;
      goto fail;
    }

    Count = 0;
    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    while ((Pxis & 0x20) != 0x20)
    {
      Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
      Count = Count + 1;
      if (Count == SATA_PXIS_COUNT)
        break ;
    }

    if (Count == SATA_PXIS_COUNT)
      SATA_ERROR_PRINT(("sata write:pxis failed\n"));

fail:
    if (hRmMemHandle)
    {
        NvRmMemUnpin(hRmMemHandle);
        NvRmMemHandleFree(hRmMemHandle);
    }
    NvRmClose(hRm);
    return e;
}

NvError NvDdkSataWrite(
    NvU32 SectorNum,
    const NvU8 *pWriteBuffer,
    NvU32 NumberOfSectors)
{
    return NvDdkSataWriteSectors(SectorNum, NumberOfSectors, pWriteBuffer);
}

NvError NvDdkSataRead(
    NvU32 SectorNum,
    NvU8 *pWriteBuffer,
    NvU32 NumberOfSectors)
{
    return NvDdkSataReadSectors(SectorNum, NumberOfSectors, pWriteBuffer);
}

NvError NvDdkSataIdentifyDevice(NvU8 *Buf)
{
    NvError e = NvSuccess;
    NvU32 Count, Pxci, Pxis;
    NvU32 RegValue;
    NvU8* Temp;
    NvRmMemHandle hRmMemHandle = NULL;
    NvRmDeviceHandle hRm;

    NvRmOpen(&hRm, 1);

    (NvRmMemHandleAlloc(hRm, NULL, 0, 2, NvOsMemAttribute_WriteCombined,
        (1 << SECTOR_SIZE_LOG2), 0, 1, &hRmMemHandle));

    Temp = (NvU8*)NvRmMemPin(hRmMemHandle);
    NvOsMemset(Temp, 0, (1 << SECTOR_SIZE_LOG2));

    NV_WRITE32(0x70027114, 0x22);

    NV_WRITE32((NvU32)s_pCommandListBaseAddres, 0x00010005);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x4, (1 << SECTOR_SIZE_LOG2));
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x8,
        (NvU32)s_pFisBaseAddress);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0xc, 0x00000000);

    RegValue = 0;
    RegValue |= SATA_COMMAND_DEFAULT;
    RegValue |= SATA_COMMAND_IDENTIFY;
    NV_WRITE32((NvU32)s_pFisBaseAddress, RegValue);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x4, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0xc, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x10, 0x0);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x80, (NvU32)Temp);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x84, 0x0);

    RegValue = 0;
    RegValue |= SATA_NUM_SECTORS_DEFAULT;
    RegValue |= ((1 << SECTOR_SIZE_LOG2) - 1);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8c, RegValue);

    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCI, 0x1);

    Count = 0;
    Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
    while (Pxci != 0x0)
    {
      Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
      Count = Count + 1;
      if (Count == SATA_PXCI_COUNT)
        break ;
    }

    if (Count == SATA_PXCI_COUNT)
    {
        SATA_ERROR_PRINT(("sata identify device: pxci failed \n"));
        e = NvError_Timeout;
        goto fail;
    }

    Count = 0;
    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    while ((Pxis & 0x20) != 0x20)
    {
      Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
      Count = Count + 1;
      if (Count == SATA_PXIS_COUNT)
        break ;
    }

    if (Count == SATA_PXIS_COUNT)
      SATA_ERROR_PRINT(("sata identify device:pxis failed\n"));

    NvOsMemcpy(Buf, Temp, (1 << SECTOR_SIZE_LOG2));

fail:
    if (hRmMemHandle)
    {
        NvRmMemUnpin(hRmMemHandle);
        NvRmMemHandleFree(hRmMemHandle);
    }
    NvRmClose(hRm);
    return e;
}

NvError NvDdkSataErase(NvU32 StartSector, NvU32 NumSectors)
{
    NvError e = NvSuccess;
    NvU32 Count;
    NvU32 Pxci;
    NvU32 Pxis;
    NvU32 RegValue;

    SATA_DEBUG_PRINT(("SataErase: StartSector=%d, NO.Sectors = %d\n", \
        StartSector, NumSectors));

    NV_WRITE32(0x70027114, 0x21);

    NV_WRITE32((NvU32)s_pCommandListBaseAddres, 0x00000045);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x4,
        NumSectors * (1 << SECTOR_SIZE_LOG2));
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0x8,
        (NvU32)s_pFisBaseAddress);
    NV_WRITE32((NvU32)s_pCommandListBaseAddres + 0xc, 0x0);

    RegValue = 0;
    RegValue |= SATA_COMMAND_DEFAULT;
    RegValue |= SATA_COMMAND_ERASE;
    NV_WRITE32((NvU32)s_pFisBaseAddress, RegValue);

    RegValue = 0;
    RegValue |= SATA_START_SECTOR_DEFAULT;
    RegValue |= StartSector;
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x4, RegValue);

    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x8, 0x0);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0xc, NumSectors);
    NV_WRITE32((NvU32)s_pFisBaseAddress + 0x10, 0x0);

    NV_WRITE32(NV_SATA_APB_BAR5_START + PXCI, 0x1);

    Count = 0;
    Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
    while (Pxci != 0x0)
    {
      Pxci = NV_READ32(NV_SATA_APB_BAR5_START + PXCI);
      Count = Count + 1;
      if (Count == SATA_PXCI_COUNT)
        break ;
    }

    if (Count == SATA_PXCI_COUNT)
    {
      SATA_ERROR_PRINT(("sata identify device: pxci failed \n"));
      e = NvError_Timeout;
      goto fail;
    }

    Count = 0;
    Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
    while ((Pxis & 0x20) != 0x20)
    {
      Pxis = NV_READ32(NV_SATA_APB_BAR5_START + PXIS);
      Count = Count + 1;
      if (Count == SATA_PXIS_COUNT)
        break ;
    }

    if (Count == SATA_PXIS_COUNT)
      SATA_ERROR_PRINT(("sata identify device:pxis failed\n"));
fail:
    return e;
}
