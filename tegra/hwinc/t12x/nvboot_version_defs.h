/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines version information for the boot rom.
 */

#ifndef INCLUDED_NVBOOT_VERSION_DEFS_H
#define INCLUDED_NVBOOT_VERSION_DEFS_H

#include "arapb_misc_gp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines a macro for assembling a 32-bit version number out of
 * a 16-bit major revision number a and a 16-bit minor revision number b.
 */
#define NVBOOT_VERSION(a,b) ((((a)&0xffff) << 16) | ((b)&0xffff))

#define NVBOOT_PRODUCTION_CHIPID (APB_MISC_GP_HIDREV_0_CHIPID_DEFAULT)

#define NVBOOT_PRODUCTION_BOOTROM_MINORREV  1
#define NVBOOT_PRODUCTION_RCM_MINORREV      1
#define NVBOOT_PRODUCTION_BOOTDATA_MINORREV 1

/**
 * Defines the version of the bootrom code.
 *
 * Revision history (update with significant/special releases):
 *
 * The Major Revision is taken from APB_MISC_GP_HIDREV_0_CHIPID_DEFAULT
 * starting with T124.
 *
 */
#define CONST_NVBOOT_BOOTROM_VERSION (NVBOOT_VERSION(NVBOOT_PRODUCTION_CHIPID, NVBOOT_PRODUCTION_BOOTROM_MINORREV))

/**
 * Defines the version of the RCM protocol.
 *
 * Revision history (update with each revision change):
 *
 * Major Revision 1:
 *   Rev 0: First tapeout for AP15.
 *
 * Major Revision 2:
 *   Rev 1: First tapeout for AP20:
 *          - Added ProgramFuseArray and VerifyFuseArray commands.
 *          - Deprecated ProgramFuses and VerifyFuses commands.
 *          - Changed the allowed applet size.
 *   Note: There are no changes yet for T30, so this version
 *         number remains unchanged.
 *
 * Major Revision 3:
 *   Rev 1: First tapeout for T30:
 *          - Deprecated ProgramFuseArray and VerifyFuseArray commands.
 *
 * Major Revision 0x35:
 *   Rev 1: First tapeout for T35.
 *
 * T124 and going forward:
 * Major Revision is now taken from APB_MISC_GP_HIDREV_0_CHIPID_DEFAULT.
 */
#define CONST_NVBOOT_RCM_VERSION (NVBOOT_VERSION(NVBOOT_PRODUCTION_CHIPID, NVBOOT_PRODUCTION_RCM_MINORREV))


/**
 * Defines the version of the boot data structures (BCT, BIT).
 *
 * Revision history (update with each revision change):
 *
 * Major Revision 1:
 *   Rev 0: First tapeout for AP15.
 *
 * Major Revision 2:
 *   Rev 1: First tapeout for AP20.
 *
 *   Note: There are no changes yet for T30, so this version
 *         number remains unchanged.
 *
 * Major Revision 0x35:
 *   Rev 1: First tapeout for T35.
 *
 * T124 and going forward:
 * Major Revision is now taken from APB_MISC_GP_HIDREV_0_CHIPID_DEFAULT.

 */
#define CONST_NVBOOT_BOOTDATA_VERSION (NVBOOT_VERSION(NVBOOT_PRODUCTION_CHIPID, NVBOOT_PRODUCTION_BOOTDATA_MINORREV))


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_VERSION_DEFS_H */
