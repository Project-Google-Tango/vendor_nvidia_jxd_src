/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: Fuse API</b>
 *
 * @b Description: Contains the NvRM chip unique id implementation.
 */

#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvrm_hardware_access.h"
#include "nvrm_hwintf.h"
#include "t12x/arclk_rst.h"
#include "common/common_misc_private.h"
//#include "t12xrm_clocks.h"
#include "t12x/arfuse.h"
#include "t12x/arapb_misc.h"

// Local DRF defines for ECID fields
#define ECID_ECID0_0_RSVD1_RANGE    5:0     // 6 bits
#define ECID_ECID0_0_Y_RANGE        14:6    // 9 bits
#define ECID_ECID0_0_X_RANGE        23:15   // 9 bits
#define ECID_ECID0_0_WAFER_RANGE    29:24   // 6 bits
#define ECID_ECID0_0_LOT1_RANGE     31:30   // 2 bits
#define ECID_ECID1_0_LOT1_RANGE     25:0    // 26 bits
#define ECID_ECID1_0_LOT0_RANGE     31:26   // 6 bits
#define ECID_ECID2_0_LOT0_RANGE     25:0    // 26 bits
#define ECID_ECID2_0_FAB_RANGE      31:26   // 6 bits
#define ECID_ECID3_0_VENDOR_RANGE   3:0     // 4 bits

NvError NvRmQueryChipUniqueId(NvRmDeviceHandle hDevHandle, NvU32 IdSize, void* pId)
{
    NvU32       OldRegData;         // Old register contents
    NvU32       NewRegData;         // New register contents
    NvBootECID  *Uid;               // Unique ID
    NvU32       Vendor;             // Vendor
    NvU32       Fab;                // Fab
    NvU32       Wafer;              // Wafer
    NvU32       Lot0;               // Lot 0
    NvU32       Lot1;               // Lot 1
    NvU32       X;                  // X-coordinate
    NvU32       Y;                  // Y-coordinate
    NvU32       Rsvd1;              // Reserved
    NvU32       Reg;                // Scratch register

    NV_ASSERT(hDevHandle);
    if((pId == NULL) || (IdSize < sizeof(NvBootECID)))
    {
        return NvError_BadParameter;
    }
    Uid = (NvBootECID *)pId;

    // Access to unique id is protected, so make sure all registers visible
    // first.
    OldRegData = NV_REGR(hDevHandle,
            NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_MISC_CLK_ENB_0);
    NewRegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_CLK_ENB,
            CFG_ALL_VISIBLE, 1, OldRegData);
    NV_REGW(hDevHandle, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_MISC_CLK_ENB_0, NewRegData);

  /** For t12x:

    * Field        Bits     Data
    * (LSB first)
    * --------     ----     ----------------------------------------
    * Reserved       6
    * Y              9      Wafer Y-coordinate
    * X              9      Wafer X-coordinate
    * WAFER          6      Wafer id
    * LOT_0         32      Lot code 0
    * LOT_1         28      Lot code 1
    * FAB            6      FAB code
    * VENDOR         4      Vendor code
    * --------     ----
    * Total        100
    *
    * Gather up all the bits and pieces.
    *
    * <Vendor:4><Fab:6><Lot0:26><Lot0:6><Lot1:26><Lot1:2><Wafer:6><X:9><Y:9><Reserved:6>
  **/

    // Vendor
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_VENDOR_CODE_0);
    Vendor = NV_DRF_VAL(FUSE, OPT_VENDOR_CODE, OPT_VENDOR_CODE, Reg);

    // Fab
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_FAB_CODE_0);
    Fab = NV_DRF_VAL(FUSE, OPT_FAB_CODE, OPT_FAB_CODE, Reg);

    // Lot 0
    Lot0 = 0;
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_LOT_CODE_0_0);
    Lot0 = NV_DRF_VAL(FUSE, OPT_LOT_CODE_0, OPT_LOT_CODE_0, Reg);

    // Lot 1
    Lot1 = 0;
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_LOT_CODE_1_0);
    Lot1 = NV_DRF_VAL(FUSE, OPT_LOT_CODE_1, OPT_LOT_CODE_1, Reg);

    // Wafer
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_WAFER_ID_0);
    Wafer = NV_DRF_VAL(FUSE, OPT_WAFER_ID, OPT_WAFER_ID, Reg);

    // X-coordinate
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_X_COORDINATE_0);
    X = NV_DRF_VAL(FUSE, OPT_X_COORDINATE, OPT_X_COORDINATE, Reg);

    // Y-coordinate
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_Y_COORDINATE_0);
    Y = NV_DRF_VAL(FUSE, OPT_Y_COORDINATE, OPT_Y_COORDINATE, Reg);

    // Reserved bits
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
            FUSE_OPT_OPS_RESERVED_0);
    Rsvd1 = NV_DRF_VAL(FUSE, OPT_OPS_RESERVED, OPT_OPS_RESERVED, Reg);

    Reg = 0;
    Reg |= NV_DRF_NUM(ECID,ECID0,RSVD1,Rsvd1);    // <Reserved:6> = 6 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,Y,Y);            // <Y:9><Reserved:6> = 15 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,X,X);            // <X:9><Y:9><Reserved:6> = 24 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,WAFER,Wafer);    // <Wafer:6><X:9>
                                                  // <Y:9><Reserved:6> = 30 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,LOT1,Lot1);      // <Lot1:2><Wafer:6>
                                                  // <X:9><Y:9><Reserved:6> = 32 bits
    Uid->ECID_0 = Reg;

    // discard 2 bits as they are already copied into ECID_0 field
    Lot1 >>= 2;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID1,LOT1,Lot1);      // <Lot1:26> = 26 bits
    Reg |= NV_DRF_NUM(ECID,ECID1,LOT0,Lot0);      // <Lot0:6><Lot1:26> = 32 bits
    Uid->ECID_1 = Reg;

    // discard 6 bits as they are already copied into ECID_1 field
    Lot0 >>= 6;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID2,LOT0,Lot0);      // <Lot0:26> = 26 bits
    Reg |= NV_DRF_NUM(ECID,ECID2,FAB,Fab);        // <Fab:6><Lot0:26> = 32 bits
    Uid->ECID_2 = Reg;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID3,VENDOR,Vendor);  // <Vendor:4> = 4 bits
    Uid->ECID_3 = Reg;

    // Restore the protected registers enable to the way we found it.
    NV_REGW(hDevHandle, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_MISC_CLK_ENB_0, OldRegData);
    return NvError_Success;
}
