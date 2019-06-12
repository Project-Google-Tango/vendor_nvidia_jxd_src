/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvassert.h"
#include "nvos.h"
#include "nvrm_drf.h"
#include "armc.h"
#include "nvboot_bit.h"
#include "nvboot_hardware_access_int.h"
#include "nvboot_reset_int.h"
#include "nvboot_clocks_int.h"
#include "nvrm_drf.h"
#include "arahb_arbc.h"
#include "arclk_rst.h"
#include "armc.h"
#include "nvrm_memmgr.h"
#include "nvddk_xusb_context.h"
#include "nvddk_xusb_arc.h"

#define HW_REGR(d, p, r)  NV_READ32(NV_ADDRESS_MAP_##d##_BASE + p##_##r##_0)
#define HW_REGW(b, p, r, v) NV_WRITE32(b + p##_##r##_0, v)

#define NV_ADDRESS_MAP_CAR_BASE                        1610637312
#define NV_ADDRESS_MAP_MCB_BASE                        1879150592
#define NV_ADDRESS_MAP_IRAM_A_BASE                    1073741824
#define NV_ADDRESS_MAP_IRAM_D_LIMIT                    1074003967
#define NV_ADDRESS_MAP_MC0_BASE                        1879146496
#define NV_ADDRESS_MAP_EMEM_BASE                    (~(2147483647))
#define NV_ADDRESS_MAP_EMEM_LIMIT                    (~(1048576))


void NvBootArcEnable(void)
{
    NvU32 RegData;
    // LV: Unclamp PLLM PX
    RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_PLLM_BASE_0);
    RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE,
                        PLLM_CLAMP_PX, 0, RegData);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_PLLM_BASE_0, RegData);

    // moving clocks enable before EMC programming..
    // Enable the clocks for EMC(EMC0) and MC(MC0)
    XusbClocksSetEnable(NvBootClocksClockId_EmcId, NV_TRUE);
    XusbClocksSetEnable(NvBootClocksClockId_McId,  NV_TRUE);
    // Enable the clocks for EMC1 and MC1
    XusbClocksSetEnable(NvBootClocksClockId_Emc1Id, NV_TRUE);
    XusbClocksSetEnable(NvBootClocksClockId_Mc1Id,  NV_TRUE);

    // SW reset to MC is disabled. so only reset emc0/1
    XusbResetSetEnable(NvBootResetDeviceId_EmcId, NV_FALSE);
    XusbResetSetEnable(NvBootResetDeviceId_Emc1Id, NV_FALSE);

    RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0);
    RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_EMC,
                        EMC_2X_CLK_SRC, PLLP_OUT0, RegData);
    // to avoid the ERROR reported due to tick counter..
    // MC_EMC_SAME_FREQ is enabled..
    //                       | NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_EMC,
    //                            MC_EMC_SAME_FREQ, ENABLE, RegData);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0, RegData);

    NvOsWaitUS(5);


    // also enable ARC_CLK_OVR_ON of CLK_RST_CONTROLLER_LVL2_CLK_GATE_OVRD_0
    // since this was not enabled mem requests from xusb to iram is not happening.
    RegData = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_LVL2_CLK_GATE_OVRD_0);

    //(1 << 19);
    RegData |= NV_DRF_NUM(CLK_RST_CONTROLLER,
                        LVL2_CLK_GATE_OVRD, ARC_CLK_OVR_ON, 1);

    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
           CLK_RST_CONTROLLER_LVL2_CLK_GATE_OVRD_0,
           RegData);

    // Enable ARC (AHB redirection ) path
    RegData = HW_REGR(MCB, MC, IRAM_REG_CTRL);
    // update for ARC(AHB redirection ) path
    HW_REGW(NV_ADDRESS_MAP_MCB_BASE, MC, IRAM_BOM,
            (NV_ADDRESS_MAP_IRAM_A_BASE & MC_IRAM_BOM_0_WRITE_MASK));
    HW_REGW(NV_ADDRESS_MAP_MCB_BASE, MC, IRAM_TOM,
            (NV_ADDRESS_MAP_IRAM_D_LIMIT & MC_IRAM_TOM_0_WRITE_MASK));
}

void NvBootArcDisable(NvU32 TargetAddr)
{
    NvU32 RegData;

    // Check if ARC was already locked-down
    RegData = HW_REGR(MC0, MC, IRAM_REG_CTRL);

    if (NV_DRF_VAL(MC, IRAM_REG_CTRL, IRAM_CFG_WRITE_ACCESS, RegData))
        return;

    if ( (TargetAddr >= (NvU32)NV_ADDRESS_MAP_EMEM_BASE) &&
         (TargetAddr < (NvU32)NV_ADDRESS_MAP_EMEM_LIMIT) ) {
        // Disable ARC
        // Set MC_IRAM_TOM_0[IRAM_TOM] to 0x0000_0000
        // Set MC_IRAM_BOM_0[IRAM_BOM] to 0xFFFF_F000
        HW_REGW(NV_ADDRESS_MAP_MCB_BASE, MC, IRAM_BOM, MC_IRAM_BOM_0_RESET_VAL);
        HW_REGW(NV_ADDRESS_MAP_MCB_BASE, MC, IRAM_TOM, MC_IRAM_TOM_0_RESET_VAL);

        // Lock-down ARC
        RegData = NV_DRF_DEF(MC, IRAM_REG_CTRL, IRAM_CFG_WRITE_ACCESS, DISABLED);
        HW_REGW(NV_ADDRESS_MAP_MCB_BASE, MC, IRAM_REG_CTRL, RegData);
    }
}
