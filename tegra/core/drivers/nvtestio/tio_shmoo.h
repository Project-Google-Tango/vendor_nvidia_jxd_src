#ifndef INCLUDED_TIO_SHMOO_H
#define INCLUDED_TIO_SHMOO_H
/*
 * Copyright (c) 2005-2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * tio_shmoo.h - Definitions for the entry points and data structures
 * used by the schmoo implementation.
 *
 * Due to divergent capabilities on different platforms, the implementation
 * of the shmoo commands is divided into three parts:
 * - tio_shmoo.c      Common code used on all platforms.
 * - tio_shmoo_rm.c   Code used on platforms that can use calls to the
 *                    NvRm Diags interface (defined in nvrm_diags.h)
 * - tio_schmo_norm.c Code for platforms that cannot call into RM.  Initialy,
 *                    these functions return codes indicating that their
 *                    capabilities are not supported.  Conceivably, versions
 *                    of these functions could be written for non-RM platforms,
 *                    either via direct implementations or by mapping higher-
 *                    level functions onto simple memory reads & writes.
 */

#if defined(__cplusplus)
extern "C"
{
#endif

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nverror.h"
#include "tio_local.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

/* Callback function for executing a shmoo command. */
typedef NvError (*NvTioShmooCb)(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);


/*
 * Function prototypes
 */
NvError NvTioShmooProcessCommand(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);

/*
 * Shmoo command callback functions.
 * Common callbacks are implemented in tio_shmoo.c.
 * The rest are implemented in both tio_shmoo_rm.c and tio_shmoo_norm.c, only
 * one of which will be linked in any single library.
 */
NvError NvTioShmooCmdModules    (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdListClks   (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdClkName    (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdListModClks(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdListPw     (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdPwName     (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdListModPw  (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdSetVoltage (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdRead32     (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdWrite32    (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdClockEnable(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdModuleReset(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdModuleClock(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdSetPll     (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdClockScaler(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdModRegRd   (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
NvError NvTioShmooCmdModRegWr   (NvTioGdbtParms *parms, NvTioGdbtCmd *cmd);
#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_TIO_SHMOO_H
