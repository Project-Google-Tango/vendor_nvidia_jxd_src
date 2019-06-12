/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/*
 * JTAG WAKEUP ENABLE:
 *  - Set to 0 if you don't want a JTAG event to wake up the AVP.
 *    This is the normal case for everyone except the the handfull
 *    of people who actually debug the AVP-side code.
 *  - Set to 1 if you are actively debugging the AVP code and
 *    you want a JTAG event to wake up the AVP. Note, if set to
 *    1 and you have JTAG connected, it may prevent the AVP from
 *    properly entering LP0.
 */
#define NVBL_AVP_JTAG_WAKEUP_ENABLE  0

#include "bootloader.h"
#include "nvassert.h"
#include "aos.h"
#include "arapb_misc.h"
#include "arapbpm.h"
#include "arevp.h"
#include "arflow_ctlr.h"
#include "arpg.h"
#include "arfuse.h"
#include "nvuart.h"
#include "nvbct.h"
#include "nvboot_bit.h"
#include "nvbct_customer_platform_data.h"
#include "nvddk_fuse.h"
#include "nverror.h"
#include "nvsdmmc.h"
#include "aos_avp_board_info.h"
#include "nvboot_device_int.h"
#include "nvddk_i2c.h"
#include "nvnct.h"
#include "nvodm_pinmux_init.h"
#include "nvbl_query.h"

volatile NvU32 *g_ptimerus = (NvU32*)TIMERUS_PA_BASE;
NvU32 g_BlAvpTimeStamp = 22;
NvU32 s_boardIdread = 1;
NvU32 e_boardIdread = 2;
NvU32 s_enablePowerRail = 3;
NvU32 e_enablePowerRail = 4;

NvBoardInfo g_BoardInfo;
NvBoardInfo g_PmuBoardInfo;
NvBoardInfo g_DisplayBoardInfo;

#define NUM_PAGE_SECTOR      8
#define EMMC_PAGE_SIZE       512
#define EMMC_PAGES_PER_BLOCK 8

#define DBG_CONSOLE_BIT_START 15
#define DBG_CONSOLE_SD        5

#ifndef NV_TEST_LOADER
#include "nvbl_arm_processor.h"

#if !__GNUC__
#pragma arm
#endif

NvU32 s_ChipId;
volatile NvU32 s_bFirstBoot = 1;

extern NvAosChip s_Chip;
extern NvBool s_QuickTurnEmul;
extern NvBool s_Simulation;
extern NvBool s_FpgaEmulation;
extern NvU32  s_Netlist;
extern NvU32  s_NetlistPatch;

NvU8 NV_ALIGN(4096) Dest[4096];

static NvError NvBlFuseBypassing(void)
{
    NvError e = NvSuccess;
#if __GNUC__
    NvBctAuxInfo *auxInfo;
    NvFuseBypassInfo *fusebypass_info;
    const NvBootInfoTable *pBit;
    NvU8 *ptr, *alignedPtr;
    NvU32 total_size = sizeof(*fusebypass_info);

    pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);
    NV_ASSERT(pBit->BctPtr);
    ptr = pBit->BctPtr->CustomerData + sizeof(NvBctCustomerPlatformData);
    alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2) << 2);
    auxInfo = (NvBctAuxInfo *)alignedPtr;

    if (auxInfo->StartFuseSector == 0 && auxInfo->NumFuseSector == 0)
        goto fail;

    NV_CHECK_ERROR_CLEANUP(NvNvBlSdmmcInit());

    if(total_size > 4096)
        goto fail;

    NV_CHECK_ERROR_CLEANUP(
        NvNvBlSdmmcReadMultiPage(auxInfo->StartFuseSector, 0,
                                         NUM_PAGE_SECTOR, Dest));
    NV_CHECK_ERROR_CLEANUP(NvNvBlSdmmcWaitForIdle());

    fusebypass_info = (NvFuseBypassInfo *)Dest;

    e = NvDdkFuseBypassSku(fusebypass_info);

    if(e != NvSuccess && e != NvError_InvalidConfigVar)
        goto fail;

fail:
#endif
    return e;
}

NvBool NvBlIsMmuEnabled(void)
{
    NvU32   cpsr;   // Current Processor Status Register
    NvU32   cr;     // CP15 Control Register

    // If the processor is in USER mode (a non-privileged mode) the MMU is assumed to
    // be on because the MRC instruction to definitively determine the state of the MMU
    // is not available to non-privileged callers. This is a safe assumption because
    // the only time the processor is in user mode is well after the kernel has
    // been started and the kernel virtual memory environment has been established.

    GET_CPSR(cpsr);
    if ((cpsr & PSR_MODE_MASK) != PSR_MODE_USR)
    {
        // Read the CP15 Control Register
        MRC(p15, 0, cr, c1, c0, 0);

        // Is the MMU on?
        if (!(cr & M_CP15_C1_C0_0_M))
        {
            // MMU is off.
            return NV_FALSE;
        }
    }

    // MMU is on.
    return NV_TRUE;
}


void NvBlAvpUnHaltCpu(void)
{
    NV_FLOW_REGW(FLOW_PA_BASE, HALT_CPU_EVENTS, 0);
}

#define SLAVE_FREQUENCY              400   // KHz
#define SLAVE_TRANSACTION_TIMEOUT    1000  // Ms

static NvError NvBlEnablePm342Debug(void)
{
    NvDdkI2cSlaveHandle hSlave = NULL;
    NvError Error = NvSuccess;
    NvU8 Value = 0;

    Error = NvDdkI2cDeviceOpen(NvDdkI2c1, 0x40, 0,
            SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
    if (Error != NvSuccess)
        goto fail;

    Value = 0xEf;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x07, &Value, 1);
    if (Error != NvSuccess)
        goto fail;

    Value = 0x10;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x03, &Value, 1);
    if (Error != NvSuccess)
        goto fail;
fail:
    return Error;
}

void bootloader(void)
{
    NvU32 reg;
    NvU32 major;
    NvU32 minor;
    NvU32 pllpFreq;
    NvU32 boardId = 0x0;
    const NvBootInfoTable*  pBootInfo = (NvBootInfoTable*)(NV_BIT_ADDRESS);
    NvBctAuxInfo *auxInfo;
    NvBctAuxInternalData *pBctAuxInternal;
    const NvBootInfoTable *pBit;
    NvU8 *ptr, *alignedPtr;
#ifdef NV_DETECT_FB_PULSE
    NvBool IsFastbootPressed = NV_FALSE;
    NvU32 StartTime;
    NvU32 gpio_pi1_val;
#endif

    /* We don't expect to have s_bFirstBoot as 1 on CPU side.
     * The following code is required for Tegraboot case when AVP-part of
     * Bl doesn't executes. */
    if(*(volatile NvU32*)(PG_UP_PA_BASE + PG_UP_TAG_0) == PG_UP_TAG_0_PID_CPU)
    {
        if (s_bFirstBoot)
        {
            NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &g_BoardInfo);
            boardId = g_BoardInfo.BoardId;
#ifndef BUILD_FOR_COSIM
            NvOdmPinmuxInit(boardId);

            pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);
            NV_ASSERT(pBit->BctPtr);
            pBctAuxInternal = (NvBctAuxInternalData *)
                               ((NvU32)(pBit->BctPtr->CustomerData) +
                               NVBOOT_BCT_CUSTOMER_DATA_SIZE -
                               sizeof(NvBctAuxInternalData));
            if (((pBctAuxInternal->CustomerOption >> DBG_CONSOLE_BIT_START)
                               & 7) == DBG_CONSOLE_SD)
                 NvOdmSdmmc3UartPinmuxInit();
#endif
        }

        s_bFirstBoot = 0;
    }

    s_QuickTurnEmul = NV_FALSE;
    s_FpgaEmulation = NV_FALSE;
    s_Simulation = NV_FALSE;

    reg = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_HIDREV_0 );
    s_ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, reg);
    major = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, reg);
    minor = NV_DRF_VAL(APB_MISC, GP_HIDREV, MINORREV, reg);

    reg = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_EMU_REVID_0);
    s_Netlist = NV_DRF_VAL(APB_MISC, GP_EMU_REVID, NETLIST, reg);
    s_NetlistPatch = NV_DRF_VAL(APB_MISC, GP_EMU_REVID, PATCH, reg);

    if( major == 0 )
    {
        if( s_Netlist == 0 )
        {
            s_Simulation = NV_TRUE;
        }
        else
        {
            if( minor == 0 )
            {
                s_QuickTurnEmul = NV_TRUE;
            }
            else
            {
                s_FpgaEmulation = NV_TRUE;
            }
        }
    }

    if (!s_FpgaEmulation)
    {
        s_boardIdread = *g_ptimerus;
        e_boardIdread = *g_ptimerus;
    }

    if(s_bFirstBoot)
    {
        if (!s_FpgaEmulation)
        {
            NvBlInitI2cPinmux();
            NvBlInitGPIOExpander();

            e_boardIdread = *g_ptimerus;

            NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &g_BoardInfo);
            NvBlReadBoardInfo(NvBoardType_PmuBoard, &g_PmuBoardInfo);
            NvBlReadBoardInfo(NvBoardType_DisplayBoard,&g_DisplayBoardInfo);

            if (g_BoardInfo.BoardId == 0x167)
                NvBlEnablePm342Debug();

            pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);
            NV_ASSERT(pBit->BctPtr);
            ptr = pBit->BctPtr->CustomerData + sizeof(NvBctCustomerPlatformData);
            alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2) << 2);
            auxInfo = (NvBctAuxInfo *)alignedPtr;

            if (g_BoardInfo.BoardId > 0xA00 || g_BoardInfo.BoardId == 0)
            {
                g_BoardInfo.BoardId = auxInfo->NCTBoardInfo.proc_board_id;
                g_BoardInfo.Fab = auxInfo->NCTBoardInfo.proc_fab;
                g_BoardInfo.Sku = auxInfo->NCTBoardInfo.proc_sku;
                g_BoardInfo.Revision = 0;
                g_BoardInfo.MinorRevision = 0;
            }
            if (g_PmuBoardInfo.BoardId > 0xA00 || g_PmuBoardInfo.BoardId == 0)
            {
                g_PmuBoardInfo.BoardId = auxInfo->NCTBoardInfo.pmu_board_id;
                g_PmuBoardInfo.Fab = auxInfo->NCTBoardInfo.pmu_fab;
                g_PmuBoardInfo.Sku = auxInfo->NCTBoardInfo.pmu_sku;
                g_PmuBoardInfo.Revision = 0;
                g_PmuBoardInfo.MinorRevision = 0;
            }
            if (g_DisplayBoardInfo.BoardId > 0xA00 || g_DisplayBoardInfo.BoardId == 0)
            {
                g_DisplayBoardInfo.BoardId = auxInfo->NCTBoardInfo.display_board_id;
                g_DisplayBoardInfo.Fab = auxInfo->NCTBoardInfo.display_fab;
                g_DisplayBoardInfo.Sku = auxInfo->NCTBoardInfo.display_sku;
                g_DisplayBoardInfo.Revision = 0;
                g_DisplayBoardInfo.MinorRevision = 0;
            }
        }
#if __GNUC__
        // FIXME: use car register to find PLLP frequency and initialize uart to the same
        //currently based on chip and whether miniloader/microboot is
        // present in path before bootloader,  uart is initialized to
        //correspoding PLLP frequency.
        pllpFreq = PLLP_FIXED_FREQ_KHZ_216000;
        switch( s_ChipId ) {
            case 0x30:
            case 0x35:
                if(pBootInfo->BctPtr->NumSdramSets == 0)
                  {
                      pllpFreq = PLLP_FIXED_FREQ_KHZ_408000;
                  }
                break;
            case 0x40:
                boardId = g_BoardInfo.BoardId;
                pllpFreq = PLLP_FIXED_FREQ_KHZ_408000;
                break;
        }
#ifndef BUILD_FOR_COSIM
        NvOdmPinmuxInit(boardId);
#endif
        NvAvpUartInit(pllpFreq);
#endif
#ifndef BOOT_MINIMAL_BL

        if(!s_FpgaEmulation && !s_QuickTurnEmul)
        {
            // check for Production mode fuse
            if (*((const volatile NvU32 *)
                  (FUSE_PA_BASE + FUSE_PRODUCTION_MODE_0)) == 0)
            {
                // Function reads FB parition info(start sector , no. of sectors) from the BCT.
                // After finding valid partition info, it reads fuse values to be bypassed.
                // It then performs fuse bypassing after error handling
                NvBlFuseBypassing();
            }
        }
#endif
    }

#ifdef NV_DETECT_FB_PULSE
// GPIO Addresses for FORCE_RECOVERY_PIN
#define GPIO_PI1_CNF       0x6000D200
#define GPIO_PI1_OE        0x6000D210
#define GPIO_PI1_IN        0x6000D230

// GPIO Addresses for FORCE_RECOVERY_PIN
#define GPIO_PK0_CNF       0x6000D208
#define GPIO_PK0_OE        0x6000D218
#define GPIO_PK0_OUT       0x6000D228

#define GPIO_PK0_IN        0x6000D238
#define PMC_FASTBOOT_BIT   30
#define FASTBOOT_WAIT_uS   10000

        // Set pinmux for GPIO_PK0
        reg = NV_READ32( PINMUX_BASE + PINMUX_AUX_GPIO_PK0_0);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PK0, E_INPUT, 0, reg);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PK0, PUPD, 2, reg);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PK0, TRISTATE, 0,reg);
        NV_WRITE32( PINMUX_BASE + PINMUX_AUX_GPIO_PK0_0, reg);

        // Configure GPIO_PK0 as GPIO
        NV_WRITE32(GPIO_PK0_CNF, (NV_READ32(GPIO_PK0_CNF) | 0x01));
        // Set GPIO as output mode
        // Set OUT before OE to avoid unnecessary strobe from 0 to 1
        NV_WRITE32(GPIO_PK0_OUT, (NV_READ32(GPIO_PK0_OUT) | 0x01));
        NV_WRITE32(GPIO_PK0_OE, (NV_READ32(GPIO_PK0_OE) | 0x01));

        // Set pinmux for GPIO_PI1 as input
        reg = NV_READ32( PINMUX_BASE + PINMUX_AUX_GPIO_PI1_0);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PI1, E_INPUT, 1, reg);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PI1, PUPD, 0, reg);
        reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PI1, TRISTATE, 0,reg);
        NV_WRITE32( PINMUX_BASE + PINMUX_AUX_GPIO_PI1_0, reg);

        // Configure GPIO_PI1 as GPIO
        NV_WRITE32(GPIO_PI1_CNF, (NV_READ32(GPIO_PI1_CNF) | 0x02));
        // Disable output for this GPIO
        NV_WRITE32(GPIO_PI1_OE, (NV_READ32(GPIO_PI1_OE)  & 0xFD));

        StartTime = *g_ptimerus;
        while(!IsFastbootPressed)
        {
              gpio_pi1_val = NV_READ32(GPIO_PI1_IN);
              IsFastbootPressed = (gpio_pi1_val & 0x02) ? NV_FALSE : NV_TRUE;
              if (IsFastbootPressed)
              {
                    reg = NV_PMC_REGR(PMC_PA_BASE, SCRATCH0);
                    reg |= (1 << PMC_FASTBOOT_BIT);
                    NV_PMC_REGW(PMC_PA_BASE, SCRATCH0, reg);
              }
              if(*g_ptimerus - StartTime > FASTBOOT_WAIT_uS)
              {
                    break;
              }
        }
#endif

#ifndef BOOT_MINIMAL_BL

    NvBlConfigJtagAccess();
    NvBlConfigFusePrivateTZ();
#endif
    if( s_bFirstBoot )
    {
        /* need to set this before cold-booting, otherwise we'll end up in
         * an infinite loop.
         */
        s_bFirstBoot = 0;
        switch( s_ChipId ) {
        case 0x30:
            s_Chip = NvAosChip_T30;
            break;
        case 0x35:
            s_Chip = NvAosChip_T114;
            break;
        case 0x40:
            s_Chip = NvAosChip_T124;
            break;
        default:
            NV_ASSERT( !"unknown chipid" );
            break;
        }
        ColdBoot();
    }

    return;
}
#endif

NvBool NvBlRamRepairRequired(void)
{
    NvU32  Bit10, Bit11, Reg;


    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, MISC_CLK_ENB);
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_CLK_ENB,
            CFG_ALL_VISIBLE, 1, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, MISC_CLK_ENB, Reg);

    Bit10 = NV_FUSE_REGR(FUSE_PA_BASE, SPARE_BIT_10);
    Bit11 = NV_FUSE_REGR(FUSE_PA_BASE, SPARE_BIT_11);

    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_CLK_ENB,
            CFG_ALL_VISIBLE, 0, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, MISC_CLK_ENB, Reg);
    return (Bit10 | Bit11);
}

// Mem routines
void NvOsAvpMemset(void* s, NvU8 c, NvU32 n)
{
    NvU8* cur = (NvU8*)s;
    NvU8* end = (NvU8*)s + n;
    char cc = (char)c;
    NvU32 w = (c << 24) | (c << 16) | (c << 8) | c;

    // Set any bytes up to the first word boundary.
    while ( (((NvU32)cur) & (sizeof(NvU32) - 1)) && (cur < end) )
    {
        *(cur++) = cc;
    }

    // Do 4-word writes for as long as we can.
    while ((NvUPtr)(end - cur) >= sizeof(NvU32) * 4)
    {
        *((NvU32*)(cur+0)) = w;
        *((NvU32*)(cur+4)) = w;
        *((NvU32*)(cur+8)) = w;
        *((NvU32*)(cur+12)) = w;
        cur += sizeof(NvU32) * 4;
    }

    // Set any bytes after the last 4-word boundary.
    while (cur < end)
    {
        *(cur++) = cc;
    }
    return;
}

void NvBlAvpSetCpuResetVector(NvU32 reset)
{
    NV_EVP_REGW(EVP_PA_BASE, CPU_RESET_VECTOR, reset);
}

void NvBlAvpStallUs(NvU32 MicroSec)
{
    NvU32           Reg;            // Flow controller register
    NvU32           Delay;          // Microsecond delay time
    NvU32           MaxUs;          // Maximum flow controller delay

    // Get the maxium delay per loop.
    MaxUs = NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, ZERO, 0xFFFFFFFF);

    while (MicroSec)
    {
        Delay     = (MicroSec > MaxUs) ? MaxUs : MicroSec;
        MicroSec -= Delay;

        Reg = NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, ZERO, Delay)
            | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, uSEC, 1)
            | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, MODE, 2);

        NV_FLOW_REGW(FLOW_PA_BASE, HALT_COP_EVENTS, Reg);
    }
}

void NvBlAvpStallMs(NvU32 MilliSec)
{
    NvU32           Reg;            // Flow controller register
    NvU32           Delay;          // Millisecond delay time
    NvU32           MaxMs;          // Maximum flow controller delay

    // Get the maxium delay per loop.
    MaxMs = NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, ZERO, 0xFFFFFFFF);

    while (MilliSec)
    {
        Delay     = (MilliSec > MaxMs) ? MaxMs : MilliSec;
        MilliSec -= Delay;

        Reg = NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, ZERO, Delay)
            | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, MSEC, 1)
            | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, MODE, 2);

        NV_FLOW_REGW(FLOW_PA_BASE, HALT_COP_EVENTS, Reg);
    }
}

void NvBlAvpHalt(void)
{
    NvU32   Reg;    // Scratch reg

    for (;;)
    {
        Reg = NV_DRF_DEF(FLOW_CTLR, HALT_COP_EVENTS, MODE, FLOW_MODE_STOP) |
              NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, JTAG,
                         NVBL_AVP_JTAG_WAKEUP_ENABLE);
        NV_FLOW_REGW(FLOW_PA_BASE, HALT_COP_EVENTS, Reg);
    }
}


