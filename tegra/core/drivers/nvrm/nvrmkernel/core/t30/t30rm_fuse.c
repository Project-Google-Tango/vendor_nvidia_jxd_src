/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
#include "t30/arclk_rst.h"
#include "common/common_misc_private.h"
#include "t30/arfuse.h"
#include "t30/arapb_misc.h"

// Local DRF defines for UID fields.
#define UID_UID_0_CID_RANGE     63:60
#define UID_UID_0_VENDOR_RANGE  59:56
#define UID_UID_0_FAB_RANGE     55:50
#define UID_UID_0_LOT_RANGE     49:24
#define UID_UID_0_WAFER_RANGE   23:18
#define UID_UID_0_X_RANGE       17:9
#define UID_UID_0_Y_RANGE       8:0

NvError NvRmQueryChipUniqueId(NvRmDeviceHandle hDevHandle, NvU32 IdSize, void* pId)
{
    NvU32   OldRegData;         // Old register contents
    NvU16   ChipId;             // Chip id
    NvU32   NewRegData;         // New register contents
    NvU64   Uid;                // Unique ID
    NvU32   Vendor;             // Vendor
    NvU32   Fab;                // Fab
    NvU32   Lot;                // Lot
    NvU32   Wafer;              // Wafer
    NvU32   X;                  // X-coordinate
    NvU32   Y;                  // Y-coordinate
    NvU32   Cid;                // Chip id
    NvU32   Reg;                // Scratch register

    NvU32 i = 0;

    NV_ASSERT(hDevHandle);
    if ((pId == NULL)||(IdSize < sizeof(NvU64)))
    {
        return NvError_BadParameter;
    }

    // Access to unique id is protected, so make sure all registers visible
    // first.
    OldRegData = NV_REGR(hDevHandle,
                NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
                CLK_RST_CONTROLLER_MISC_CLK_ENB_0);
    NewRegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_CLK_ENB,
                CFG_ALL_VISIBLE, 1, OldRegData);
    NV_REGW(hDevHandle, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_MISC_CLK_ENB_0, NewRegData);

    // This used to be so much easier in prior chips. Unfortunately, there is
    // no one-stop shopping for the unique id anymore. It must be constructed
    // from various bits of information burned into the fuses during the
    // manufacturing process. The 64-bit unique id is formed by concatenating
    // several bit fields. The notation used is <fieldname>:
    //
    // <CID:4><VENDOR:4><FAB:6><LOT:26><WAFER:6><X:9><Y:9>
    //
    // Where:
    //          Field    Bits   Data
    //          -------  ----   ----------------------------------------
    //          CID        4    Chip id (encoded as zero for T30)
    //          VENDOR     4    Vendor code
    //          FAB        6    FAB code
    //          LOT       26    Lot code (5-digit base-36-coded-decimal,
    //                              re-encoded to 26 bits binary)
    //          WAFER      6    Wafer id
    //          X          9    Wafer X-coordinate
    //          Y          9    Wafer Y-coordinate
    //          -------  ----
    //          Total     64
    //
    // Gather up all the bits and pieces.
    //

    // Chip id -- encode T30 as zero.
    // Read the Chip ID revision register
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Misc, 0),
                APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, Reg);

    switch (ChipId)
    {
        case 0x30: // if it is T30:
            Cid = 0;
            break;
        default:
            //NV_ASSERT(!"Unsupported chip type");
            Cid = 0xF;
            break;
    }

    // Vendor
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
                FUSE_OPT_VENDOR_CODE_0);
    Vendor = NV_DRF_VAL(FUSE, OPT_VENDOR_CODE, OPT_VENDOR_CODE, Reg);

    // Fab
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
                FUSE_OPT_FAB_CODE_0);
    Fab = NV_DRF_VAL(FUSE, OPT_FAB_CODE, OPT_FAB_CODE, Reg);

    // Lot code must be re-encoded from a 5 digit base-36 'BCD' number
    // to a binary number.
    Lot = 0;
    Reg = NV_REGR(hDevHandle, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),
                FUSE_OPT_LOT_CODE_0_0);
    Reg <<= 2;  // Two unused bits (5 6-bit 'digits' == 30 bits)
    for (i = 0; i < 5; ++i)
    {
        NvU32 Digit;

        Digit = (Reg & 0xFC000000) >> 26;
        NV_ASSERT(Digit < 36);
        Lot *= 36;
        Lot += Digit;
        Reg <<= 6;
    }

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

    Uid = NV_DRF64_NUM(UID, UID, CID, Cid)
        | NV_DRF64_NUM(UID, UID, VENDOR, Vendor)
        | NV_DRF64_NUM(UID, UID, FAB, Fab)
        | NV_DRF64_NUM(UID, UID, LOT, Lot)
        | NV_DRF64_NUM(UID, UID, WAFER, Wafer)
        | NV_DRF64_NUM(UID, UID, X, X)
        | NV_DRF64_NUM(UID, UID, Y, Y);
    *(NvU64 *)pId = Uid;

    // Restore the protected registers enable to the way we found it.
    NV_REGW(hDevHandle, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_MISC_CLK_ENB_0, OldRegData);
    return NvError_Success;
}
