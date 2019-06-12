/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvboot_misc_t1xx.h"
#ifdef ENABLE_TXX
#include "t11x/arclk_rst.h"
#include "t11x/artimerus.h"
#include "t11x/arapbpm.h"
#include "t114/include/nvboot_hardware_access_int.h"
#include "t11x/arfuse.h"
#else
#include "arclk_rst.h"
#include "artimerus.h"
#include "arapbpm.h"
#include "include/nvboot_hardware_access_int.h"
#include "arfuse.h"
#endif

#if ENABLE_T124
static const NvU32 CarResetRegTbl[CarResetReg_Num] = {
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_L_0,
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_H_0,
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_U_0,
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_X_0,
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_V_0,
    NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_RST_DEVICES_W_0,
};
#endif
// Get the oscillator frequency, from the corresponding HW configuration field
NvBootClocksOscFreq NvBootClocksGetOscFreqT1xx(void)
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
NvBootClocksStartPllT1xx(
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

    switch (PllId) {
        /* We can now handle each PLL explicitly by making each PLL its own
         * case construct. Unsupported PLL will fall into NV_ASSERT(0). Misc1
         * and Misc2 are flexible additional arguments for programming up to
         * 2 32-bit registers.If more is required, one or both can be used as
         * pointer to struct.
         */

    case NvBootClocksPllId_PllU:
        /* PLLU: Misc1 - CPCON
         *       Misc2 - LFCON
         */
#ifdef ENABLE_T148
        RegData =
              NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_CPCON, CPCON) |
              NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_LFCON, LFCON);
#else
        RegData =
              NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_CPCON, CPCON) |
              NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_LFCON, LFCON) |
              NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_MISC,
                    PLLU_LOCK_ENABLE, ENABLE);
#endif
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId),
                   RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, M)     |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, N)     |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, P) |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS,
                             DISABLE)                                         |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                    PLLU_BASE, PLLU_CLKENABLE_48M, 1) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                    PLLU_BASE, PLLU_CLKENABLE_USB, 1) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                    PLLU_BASE, PLLU_CLKENABLE_HSIC, 1) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                    PLLU_BASE, PLLU_CLKENABLE_ICUSB, 1) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER,
                    PLLU_BASE, PLLU_OVERRIDE, 1);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                   RegData);

        RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE,
            ENABLE, RegData);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                   RegData);
#ifdef ENABLE_T148
        // Bug 1194468 - Set PLLU_LOCK_ENABLE = 1
        RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId));
        RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_MISC,
                                PLLU_LOCK_ENABLE, ENABLE, RegData);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId), RegData);
#endif
        // Wait for PLL-U to lock. Bug 954659
        do
        {
            RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId));
        } while (!NV_DRF_VAL(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, RegData));
        break;

    case NvBootClocksPllId_PllM:
        // Enable OUTx divider reset (Bug 954159)
        // 0 - Reset Enable, 1 - Reset Disable
        RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE | CLK_RST_CONTROLLER_PLLM_OUT_0);
        RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLM_OUT, PLLM_OUT1_RSTN,
                        0, RegData);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_PLLM_OUT_0, RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH135, \
                        NV_DRF_VAL(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1, PLLM_PD_LSHIFT_PH135, CPCON) ) | \
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH90, \
                                  NV_DRF_VAL(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1, PLLM_PD_LSHIFT_PH90, CPCON) ) | \
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH45, \
                                  NV_DRF_VAL(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1, PLLM_PD_LSHIFT_PH45, CPCON) ) | \
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_SETUP, \
                                  NV_DRF_VAL(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1, PLLM_SETUP, CPCON) );
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_PLLM_MISC1_0, RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC2, PLLM_KVCO, \
                        NV_DRF_VAL(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2, PLLM_KVCO, LFCON) ) | \
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC2, PLLM_KCP, \
                                  NV_DRF_VAL(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2, PLLM_KCP, LFCON ));
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_MISC(PllId),
                        RegData);

        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, M) |
                NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, N) |
#ifdef ENABLE_T148
                NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, P) |
#else
                NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIV2, P) |
#endif

                NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASSPLL,
                                DISABLE);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                RegData);

        RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE,
            ENABLE, RegData);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                RegData);
        break;

    case NvBootClocksPllId_PllX:
    //NOTE:This is outdated code
        RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, M) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, N) |
                  NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, P) |
                  NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS,
                          DISABLE);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                RegData);

        RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE,
            ENABLE, RegData);
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + NVBOOT_CLOCKS_PLL_BASE(PllId),
                RegData);
        break;

    default:
        NV_ASSERT(0);
        break;
    }

    /* Stabilization delay changed for PLLU on T114.
     * Now 1 ms delay is required instead of 300 usec.
     * (HW Bug 1018795 and 954659)
     */
#ifndef ENABLE_T148
#ifndef ENABLE_T124
    if(PllId == NvBootClocksPllId_PllU)
        *StableTime = NVBOOT_CLOCKS_PLLU_STABILIZATION_DELAY;
    else
#endif
#endif
        *StableTime = NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY;
}

// Enable the clock, this corresponds generally to level 1 clock gating
void NvBootClocksSetEnableT1xx(NvBootClocksClockId ClockId, NvBool Enable)
{
       NvU32 RegData;
    NvU8  BitOffset;
    NvU8  RegOffset;

    NVBOOT_CLOCKS_CHECK_CLOCKID(ClockId);
    NV_ASSERT(((int) Enable == 0) ||((int) Enable == 1));

    // The simplest case is via bits in register ENB_CLK
    // But there are also special cases
    if(NVBOOT_CLOCKS_HAS_STANDARD_ENB(ClockId))
    {
        // there is a CLK_ENB bit to kick
        BitOffset = NVBOOT_CLOCKS_BIT_OFFSET(ClockId);
        RegOffset = NVBOOT_CLOCKS_REG_OFFSET(ClockId);

        if (NVBOOT_CLOCKS_HAS_VWREG_ENB(ClockId))
        {
            RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0 +
                RegOffset);
        }
#ifdef ENABLE_T148
        else if (NVBOOT_CLOCKS_HAS_YREG_ENB(ClockId))
        {
            RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_X_0 +
                RegOffset);
        }
#endif
        else
        {
            RegData   = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0 +
                RegOffset);
        }
        /* no simple way to use the access macro in this case */
        if(Enable)
        {
          RegData |=  (1 << BitOffset);
        }
        else
        {
          RegData &= ~(1 << BitOffset);
        }

        if (NVBOOT_CLOCKS_HAS_VWREG_ENB(ClockId))
        {
            RegData = NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0 +
                RegOffset,
                RegData);
        }
#ifdef ENABLE_T148
        else if (NVBOOT_CLOCKS_HAS_YREG_ENB(ClockId))
        {
                RegData = NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                                CLK_RST_CONTROLLER_CLK_OUT_ENB_X_0 +
                                RegOffset,
                                RegData);
        }
#endif
        else
        {
            NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0 +
                RegOffset,
                RegData);
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

    NvBootUtilWaitUST1xx(NVBOOT_CLOCKS_CLOCK_STABILIZATION_TIME);

}

void NvBootUtilWaitUST1xx(NvU32 usec)
{
    NvU32 t0;
    NvU32 t1;

    t0 = NvBootUtilGetTimeUST1xx();
    t1 = t0;

    // Use the difference for the comparison to be wraparound safe
    while((t1 - t0) < usec)
    {
        t1 = NvBootUtilGetTimeUST1xx();
    }
}

NvU32 NvBootUtilGetTimeUST1xx(void)
{
    // Should we take care of roll over of us counter? roll over happens after 71.58 minutes.
    NvU32 usec;
    usec = *(volatile NvU32 *)(NV_ADDRESS_MAP_TMRUS_BASE);
    return usec;
}

void NvBootResetSetEnableT1xx(const NvBootResetDeviceId DeviceId, const NvBool Enable)
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
    NvBootUtilWaitUST1xx(NVBOOT_RESET_STABILIZATION_DELAY);

    return;
}

#ifdef ENABLE_T124
void
NvBootResetSetEnableT124(const NvBootResetDeviceId DeviceId, const NvBool Enable) {

    NvU32 RegData ;
    NvU32 BitOffset ;
    NvU32 RegOffset ;

    NVBOOT_RESET_CHECK_ID(DeviceId) ;
    NV_ASSERT( ((int) Enable == 0) ||
               ((int) Enable == 1) ) ;

    BitOffset = NVBOOT_RESET_BIT_OFFSET(DeviceId) ;
    RegOffset = NVBOOT_RESET_REG_OFFSET(DeviceId) ;

    RegData = NV_READ32(CarResetRegTbl[RegOffset]);

    /* no simple way to use the access macro in this case */
    if (Enable) {
        RegData |=  (1 << BitOffset) ;
    } else {
        RegData &= ~(1 << BitOffset) ;
    }

    NV_WRITE32(CarResetRegTbl[RegOffset], RegData);

    // wait stabilization time (always)
    NvBootUtilWaitUST1xx(NVBOOT_RESET_STABILIZATION_DELAY);
    return ;
}
#endif

void NvBootFuseGetSkuRawT1xx(NvU32 *pSku)
{
    NV_ASSERT(pSku != NULL);
    *pSku = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_SKU_INFO_0);
}
