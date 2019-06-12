/*
 * Copyright (c)  2010 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvboot_misc_t30.h"
#include "t30/arclk_rst.h"
#include "t30/artimerus.h"
#include "t30/arapbpm.h"
#include "nvrm_drf.h"
#include "t30/include/nvboot_hardware_access_int.h"
#include "t30/arfuse.h"
#include "nvassert.h"

// Get the oscillator frequency, from the corresponding HW configuration field
NvBootClocksOscFreq NvBootClocksGetOscFreqT30(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_OSC_CTRL_0);

    return (NvBootClocksOscFreq) NV_DRF_VAL(CLK_RST_CONTROLLER,
                                            OSC_CTRL,
                                            OSC_FREQ,
                                            RegData);
}

//  Start PLL using the provided configuration parameters
void
NvBootClocksStartPllT30(
                     NvBootClocksPllId PllId,
                     NvU32 M,
                     NvU32 N,
                     NvU32 P,
                     NvU32 CPCON,
                     NvU32 LFCON,
                     NvU32 *StableTime)
{
    NvU32 RegData;

    NVBOOT_CLOCKS_CHECK_PLLID(PllId);
    NV_ASSERT (StableTime != NULL);

    // we cheat by treating all PLL (except PLLU) in the same fashion
    // this works only because
    // - same fields are always mapped at same offsets, except DCCON
    // - DCCON is always 0, doesn't conflict
    // - M,N, P of PLLP values are ignored for PLLP

    if (PllId == NvBootClocksPllId_PllU)
    {
        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER,
                             PLLU_MISC,
                             PLLU_CPCON,
                             CPCON) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                             PLLU_MISC,
                             PLLU_LFCON,
                             LFCON);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId),
                   RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, M)     |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, N)     |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, P) |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS,
                             DISABLE)                                         |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE,
                             ENABLE);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                   RegData);
    }
    else
    {
        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_MISC, PLLC_CPCON,
                             CPCON) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_MISC, PLLC_LFCON, LFCON);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId),
                   RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, P) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, M) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, N) |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS,
                             DISABLE) |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE,
                             ENABLE);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                   RegData);
    }
    // calculate the stable time
    *StableTime = NV_READ32(NV_ADDRESS_MAP_TMRUS_BASE + TIMERUS_CNTR_1US_0) +
                  NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY;
}

// Enable the clock, this corresponds generally to level 1 clock gating
void NvBootClocksSetEnableT30(NvBootClocksClockId ClockId, NvBool Enable)
{
    NvU32 RegData = 0;
    NvU8  BitOffset;
    NvU8  RegOffset;

    NVBOOT_CLOCKS_CHECK_CLOCKID(ClockId);
    NV_ASSERT(((int) Enable == 0) || ((int) Enable == 1));

    // The simplest case is via bits in register ENB_CLK
    // But there are also special cases
    if(NVBOOT_CLOCKS_HAS_STANDARD_ENB(ClockId))
    {
        // there is a CLK_ENB bit to kick
        BitOffset = NVBOOT_CLOCKS_BIT_OFFSET(ClockId);
        RegOffset = NVBOOT_CLOCKS_REG_OFFSET(ClockId);
        /* no simple way to use the access macro in this case */
        if(Enable)
        {
            RegData |=  (1 << BitOffset);
        }
        else
        {
            RegData &= ~(1 << BitOffset);
        }

        if (Enable)
        {
            RegData |=  (1 << BitOffset);
            NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_CLK_ENB_L_SET_0 + \
                                RegOffset, RegData);
        } else
        {
            RegData &= ~(1 << BitOffset);
            NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_CLK_ENB_L_CLR_0 + \
                                RegOffset, RegData);
        }
    }
    else
    {
        // there is no bit in CLK_ENB, less regular processing needed
        switch(ClockId)
        {
        case NvBootClocksClockId_SclkId:
            // there is no way to stop Sclk, for documentation purpose
            break;

        case NvBootClocksClockId_HclkId:
            RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                                CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0);
            RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                                         CLK_SYSTEM_RATE,
                                         HCLK_DIS,
                                         Enable,
                                         RegData);
            NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                       CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0,
                       RegData);
            break;

        case NvBootClocksClockId_PclkId:
            RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                                CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0);
            RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                                         CLK_SYSTEM_RATE,
                                         PCLK_DIS,
                                         Enable,
                                         RegData);
            NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                       CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0,
                       RegData);
            break;

        default :
            // nothing for other enums, make that explicit for compiler
            break;
        };
    };

    NvBootUtilWaitUST30(NVBOOT_CLOCKS_CLOCK_STABILIZATION_TIME);
}

void NvBootUtilWaitUST30(NvU32 usec)
{
    NvU32 t0;
    NvU32 t1;

    t0 = NvBootUtilGetTimeUST30();
    t1 = t0;

    // Use the difference for the comparison to be wraparound safe
    while((t1 - t0) < usec)
    {
        t1 = NvBootUtilGetTimeUST30();
    }
}

NvU32 NvBootUtilGetTimeUST30(void)
{
    // Should we take care of roll over of us counter? roll over happens after 71.58 minutes.
    NvU32 usec;
    usec = *(volatile NvU32 *)(NV_ADDRESS_MAP_TMRUS_BASE);
    return usec;
}

void NvBootResetSetEnableT30(const NvBootResetDeviceId DeviceId, const NvBool Enable)
{

    NvU32 RegData = 0;
    NvU32 BitOffset;
    NvU32 RegOffset;

    NVBOOT_RESET_CHECK_ID(DeviceId);
    NV_ASSERT(((int) Enable == 0) || ((int) Enable == 1));

    BitOffset = NVBOOT_RESET_BIT_OFFSET(DeviceId);
    RegOffset = NVBOOT_RESET_REG_OFFSET(DeviceId);
    /* no simple way to use the access macro in this case */
    if (Enable)
    {
        RegData |=  (1 << BitOffset);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEV_L_SET_0 + \
                            RegOffset, RegData);
    } else {
        RegData &= ~(1 << BitOffset);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEV_L_CLR_0 + \
                        RegOffset, RegData);
    }

    // wait stabilization time (always)
    NvBootUtilWaitUST30(NVBOOT_RESET_STABILIZATION_DELAY);

    return;
}


void NvBootFuseGetSkuRawT30(NvU32 *pSku)
{
    NV_ASSERT(pSku != NULL);
    *pSku = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_SKU_INFO_0);
}

