/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvassert.h"
#include "nvrm_diag.h"
#include "nvrm_init.h"
#include "nvrm_module.h"
#include "nvutil.h"
#include "tio_gdbt.h"
#include "tio_local_target.h"
#include "tio_shmoo.h"
#include "nvrm_channel.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

/*
 * Data structure for caching the results of the initial shmoo queries.
 * This is needed to map IDs to handles when executing relevant shmoo
 * calls.
 */
typedef struct ShmooRmDataRec
{
    NvBool                      Valid; /* False after an unrecoverable error */

    NvRmDeviceHandle            RmDeviceHandle;

    NvU32                       ModuleIdCnt;
    NvRmDiagModuleID           *ModuleIds;

    NvU32                       ClockSourceCnt;
    NvRmDiagClockSourceHandle  *ClockSources;

    NvU32                      *ModClockSourceCnt;
    NvRmDiagClockSourceHandle **ModClockSources;

    NvU32                       PowerRailCnt;
    NvRmDiagPowerRailHandle    *PowerRails;

    NvU32                      *ModPowerRailCnt;
    NvRmDiagPowerRailHandle   **ModPowerRails;

} ShmooRmData;

static ShmooRmData ShmooData;
static NvBool      RmInitialized = NV_FALSE;

/*
 * Note: NvTioGdbtCmdBegin() has already been called for cmd.
 *       Shmoo commands add their output to the reply,
 *       and the caller completes the transmission of the reply.
 */

static void
InitShmooData(void)
{
    ShmooData.Valid             = NV_TRUE;

    ShmooData.RmDeviceHandle    = NULL;
    
    ShmooData.ModuleIdCnt       = 0;
    ShmooData.ModuleIds         = NULL;

    ShmooData.ClockSourceCnt    = 0;
    ShmooData.ClockSources      = NULL;

    ShmooData.ModClockSourceCnt = NULL;
    ShmooData.ModClockSources   = NULL;

    ShmooData.PowerRailCnt      = 0;
    ShmooData.PowerRails        = NULL;

    ShmooData.ModPowerRailCnt   = NULL;
    ShmooData.ModPowerRails     = NULL;
}

/* Cleanup the shmoo data after a failure. */
static void
CleanupShmooData(void)
{
    NvOsFree(ShmooData.ModuleIds        );
    NvOsFree(ShmooData.ClockSources     );
    NvOsFree(ShmooData.ModClockSourceCnt);
    NvOsFree(ShmooData.ModClockSources  );
    NvOsFree(ShmooData.PowerRails       );
    NvOsFree(ShmooData.ModPowerRailCnt  );
    NvOsFree(ShmooData.ModPowerRails    );

    ShmooData.Valid = NV_FALSE;
}

static void
InitRm(void)
{
    NvError e;

    if (!RmInitialized)
    {
        InitShmooData();

        e = NvRmOpen(&ShmooData.RmDeviceHandle, 0);
        NvTioDebugf("NvRmOpen returned %d\n", e);
        if (e)
            return;

        e = NvRmDiagEnable(ShmooData.RmDeviceHandle);
        NvTioDebugf("NvRmDiagEnable returned %d\n", e);

        if(e == NvSuccess)
        {
            RmInitialized = NV_TRUE;
        }
    }
}

NvError
NvTioShmooCmdModules(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   i;
    NvU32   n=0; /* Local for the # of modules */
    NvError e;
    NvRmDiagModuleID module_id = 0;

    InitRm();

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Fetch the data if not already cached. */
    if (ShmooData.ModuleIds == NULL)
    {
        /* First, query the number of modules (n==0 means query) */
        e = NvRmDiagListModules(&n, &module_id);
        ShmooData.ModuleIdCnt = n;
        NvTioDebugf("NvRmDiagListModules returned %d\n", e); 
        NvTioDebugf("ModuleIdCnt = %d\n", ShmooData.ModuleIdCnt);
        if (e)
            return e;

        /* Allocate per-module data. */
        ShmooData.ModuleIds         = NvOsAlloc(n * sizeof(NvRmDiagModuleID));
        ShmooData.ModClockSourceCnt = NvOsAlloc(n * sizeof(NvU32));
        ShmooData.ModClockSources   = NvOsAlloc(n *
                                                sizeof(NvRmDiagClockSourceHandle*));
        ShmooData.ModPowerRailCnt   = NvOsAlloc(n * sizeof(NvU32));
        ShmooData.ModPowerRails     = NvOsAlloc(n *
                                                sizeof(NvRmDiagPowerRailHandle*));
        if ((ShmooData.ModuleIds         == NULL) ||
            (ShmooData.ModClockSourceCnt == NULL) ||
            (ShmooData.ModClockSources   == NULL) ||
            (ShmooData.ModPowerRailCnt   == NULL) ||
            (ShmooData.ModPowerRails     == NULL))
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        e = NvRmDiagListModules(&n, ShmooData.ModuleIds);
        ShmooData.ModuleIdCnt = n;
        NvTioDebugf("NvRmDiagListModules returned %d\n", e); 
        if (e)
            goto fail;
    }

    for (i = 0; i < ShmooData.ModuleIdCnt; i++)
    {
        /* Initialize per-module data. */
        ShmooData.ModClockSourceCnt[i] = 0;
        ShmooData.ModClockSources  [i] = NULL;
        ShmooData.ModPowerRailCnt  [i] = 0;
        ShmooData.ModPowerRails    [i] = NULL;
        
        /* Add the ID of module i to the return data. */
        if (i)
            NvTioGdbtCmdChar(cmd, ';');
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, ShmooData.ModuleIds[i], 1);
    }

    return NvSuccess;

 fail:
    CleanupShmooData();
    return e;
}

NvError
NvTioShmooCmdListClks(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   i;
    NvU32   n = 0; /* Local for the # of clocks */
    NvError e;
    NvRmDiagClockSourceHandle  hdl = 0;
    
    InitRm();

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Fetch the data if not already cached. */
    if (ShmooData.ClockSources == NULL)
    {
        /* First, query the number of clock sources (n==0 means query).  */
        e = NvRmDiagListClockSources(&n, &hdl);
        ShmooData.ClockSourceCnt = n;
        NvTioDebugf("NvRmDiagListClockSources returned %d\n", e); 
        NvTioDebugf("ClockSourceCnt = %d\n", ShmooData.ClockSourceCnt);

        /* Allocate per-clock data. */
        ShmooData.ClockSources = NvOsAlloc(n * sizeof(NvRmDiagClockSourceHandle));
        if (ShmooData.ClockSources == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        e = NvRmDiagListClockSources(&n, ShmooData.ClockSources);
        ShmooData.ClockSourceCnt = n;
        NvTioDebugf("NvRmDiagListClockSources returned %d\n", e); 
        if (e)
            goto fail;
    }

    /* Just send a list of indices. */
    for (i = 0; i < ShmooData.ClockSourceCnt; i++)
    {
        if (i)
            NvTioGdbtCmdChar(cmd, ';');
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, i, 8);
    }

    return NvSuccess;

 fail:
    CleanupShmooData();
    return e;
}

/*
 * Returns the name for a known clock.
 */
NvError
NvTioShmooCmdClkName(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ClockNum;
    NvU64   ClockName;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the clock index. */
    ClockNum = NvUStrtoul(parms->parm[1].str, NULL, 16);

    NvTioDebugf("ClockNum = %d\n", ClockNum);

    if (ClockNum >= ShmooData.ClockSourceCnt)
        return NvError_BadValue;

    /* Return the name of the clock. */
    ClockName = NvRmDiagClockSourceGetName(ShmooData.ClockSources[ClockNum]);

    /* Protect the name from misinterpretation with a leading ';' */
    NvTioGdbtCmdChar  (cmd, ';');
    NvTioGdbtCmdName64(cmd, ClockName);

    return NvSuccess;
}

NvError
NvTioShmooCmdListModClks(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32                      i;
    NvU32                      ModuleId;
    NvError                    e;
    NvU32                      n = 0;
    NvRmDiagClockSourceHandle *ClkSrcs;
    
    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the module ID. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);

    if (ModuleId >= ShmooData.ModuleIdCnt)
        return NvError_BadValue;

    /* Fetch the data if not already cached. */
    if (ShmooData.ModClockSources[ModuleId] == NULL)
    {
        /* First, query the number of clock sources.  */
        e = NvRmDiagModuleListClockSources(ModuleId, &n, NULL);
        ShmooData.ModClockSourceCnt[ModuleId] = n;
        NvTioDebugf("NvRmDiagModuleListClockSources returned %d\n", e); 
        NvTioDebugf("NumModClockSources = %d\n", n);

        ClkSrcs = NvOsAlloc(n * sizeof(NvRmDiagClockSourceHandle));
        if (ClkSrcs == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvRmDiagModuleListClockSources(ModuleId,
                                                              &n,
                                                              ClkSrcs));

        ShmooData.ModClockSourceCnt[ModuleId] = n;
        ShmooData.ModClockSources  [ModuleId] = ClkSrcs;
    }

    /* Just send a list of indices. */
    for (i = 0; i < ShmooData.ModClockSourceCnt[ModuleId]; i++)
    {
        if (i)
            NvTioGdbtCmdChar(cmd, ';');
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, i, 1);
    }

    return NvSuccess;

 fail:
    CleanupShmooData();
    return e;
}

NvError
NvTioShmooCmdListPw(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   i;
    NvU32   n = 0; /* Local for the # of power rails */
    NvError e;
    NvRmDiagPowerRailHandle  hdl = 0;
    
    InitRm();

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Fetch the data if not already cached. */
    if (ShmooData.PowerRails == NULL)
    {
        /* First, query the number of power rails.  */
        e = NvRmDiagListPowerRails(&n, &hdl);
        ShmooData.PowerRailCnt = n;
        NvTioDebugf("NvRmDiagListPowerRails returned %d\n", e); 
        NvTioDebugf("PowerRailCnt = %d\n", ShmooData.PowerRailCnt);

        /* Allocate per-power rail data. */
        ShmooData.PowerRails = NvOsAlloc(n * sizeof(NvRmDiagPowerRailHandle));
        if (ShmooData.PowerRails == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        e = NvRmDiagListPowerRails(&n, ShmooData.PowerRails);
        ShmooData.PowerRailCnt = n;
        NvTioDebugf("NvRmDiagListPowerRails returned %d\n", e); 
        if (e)
            goto fail;
    }

    /* Just send a list of indices. */
    for (i = 0; i < ShmooData.PowerRailCnt; i++)
    {
        if (i)
            NvTioGdbtCmdChar(cmd, ';');
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, i, 1);
    }

    return NvSuccess;

 fail:
    CleanupShmooData();
    return e;
}

/*
 * Returns the name for a known power rail.
 */
NvError
NvTioShmooCmdPwName(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   PwNum;
    NvU64   PwName;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the power rail index. */
    PwNum = NvUStrtoul(parms->parm[1].str, NULL, 16);

    if (PwNum >= ShmooData.PowerRailCnt)
        return NvError_BadValue;

    /* Return the name of the power rail. */
    PwName = NvRmDiagPowerRailGetName(ShmooData.PowerRails[PwNum]);

    /* Protect the name from misinterpretation with a leading ';' */
    NvTioGdbtCmdChar  (cmd, ';');
    NvTioGdbtCmdName64(cmd, PwName);

    return NvSuccess;
}

NvError
NvTioShmooCmdListModPw(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32                    i;
    NvU32                    ModuleId;
    NvError                  e;
    NvU32                    n = 0;
    NvRmDiagPowerRailHandle *PwRails;
    
    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the module ID. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);

    if (ModuleId >= ShmooData.ModuleIdCnt)
        return NvError_BadValue;

    /* Fetch the data if not already cached. */
    if (ShmooData.ModPowerRails[ModuleId] == NULL)
    {
        /* First, query the number of power rails.  */
        e = NvRmDiagModuleListPowerRails(ModuleId, &n, NULL);
        ShmooData.ModPowerRailCnt[ModuleId] = n;
        NvTioDebugf("NvRmDiagModuleListPowerRails returned %d\n", e); 
        NvTioDebugf("NumModPowerRails = %d\n", n);

        PwRails = NvOsAlloc(n * sizeof(NvRmDiagPowerRailHandle));
        if (PwRails == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvRmDiagModuleListPowerRails(ModuleId,
                                                            &n,
                                                            PwRails));

        ShmooData.ModPowerRailCnt[ModuleId] = n;
        ShmooData.ModPowerRails  [ModuleId] = PwRails;
    }

    /* Just send a list of indices. */
    for (i = 0; i < ShmooData.ModPowerRailCnt[ModuleId]; i++)
    {
        if (i)
            NvTioGdbtCmdChar(cmd, ';');
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, i, 1);
    }

    return NvSuccess;

 fail:
    CleanupShmooData();
    return e;
}

NvError
NvTioShmooCmdClockEnable(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ModuleId;
    NvU32   State;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    State    = NvUStrtoul(parms->parm[2].str, NULL, 16);

    /* Disable the clock. */
    e = NvRmDiagModuleClockEnable(ModuleId, State);

    NvTioDebugf("NvRmDiagModuleClockEnable returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}


NvError
NvTioShmooCmdModuleReset(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ModuleId;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);

    /* Reset the module. */
    e = NvRmDiagModuleReset(ModuleId, NV_FALSE);

    NvTioDebugf("NvRmDiagModuleClockEnable returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}

NvError
NvTioShmooCmdModuleClock(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ModuleId;
    NvU32   ClockId;
    NvU32   Divider;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    ClockId  = NvUStrtoul(parms->parm[2].str, NULL, 16);
    Divider  = NvUStrtoul(parms->parm[3].str, NULL, 16);

    if (ClockId >= ShmooData.ClockSourceCnt)
        return NvError_BadValue;

    /* Configure the clock for the module. */
    e = NvRmDiagModuleClockConfigure(ModuleId,
                                     ShmooData.ClockSources[ClockId],
                                     Divider,
                                     NV_TRUE);

    NvTioDebugf("NvRmDiagModuleClockConfigure returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}

NvError
NvTioShmooCmdSetPll(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ClockId;
    NvU32   m;
    NvU32   n;
    NvU32   p;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ClockId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    m       = NvUStrtoul(parms->parm[2].str, NULL, 16);
    n       = NvUStrtoul(parms->parm[3].str, NULL, 16);
    p       = NvUStrtoul(parms->parm[4].str, NULL, 16);

    if (ClockId >= ShmooData.ClockSourceCnt)
        return NvError_BadValue;

    /* Set the pll. */
    e = NvRmDiagPllConfigure(ShmooData.ClockSources[ClockId], m, n, p);
    NvTioDebugf("NvRmDiagPllConfigure returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}


NvError
NvTioShmooCmdClockScaler(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   TgtClockId;
    NvU32   RefClockId;
    NvU32   m;
    NvU32   n;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    TgtClockId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    RefClockId = NvUStrtoul(parms->parm[2].str, NULL, 16);
    m          = NvUStrtoul(parms->parm[3].str, NULL, 16);
    n          = NvUStrtoul(parms->parm[4].str, NULL, 16);

    if (TgtClockId >= ShmooData.ClockSourceCnt)
        return NvError_BadValue;

    if (RefClockId >= ShmooData.ClockSourceCnt)
        return NvError_BadValue;

    /* Scale the clocks. */
    e = NvRmDiagClockScalerConfigure(ShmooData.ClockSources[TgtClockId],
                                     ShmooData.ClockSources[RefClockId],
                                     m,
                                     n);

    NvTioDebugf("NvRmDiagClockScalerConfigure returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}

NvError
NvTioShmooCmdSetVoltage(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   PowerRailId;
    NvU32   Voltage;
    NvError e;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the power rail index. */
    PowerRailId = NvUStrtoul(parms->parm[1].str, NULL, 16);

    if (PowerRailId >= ShmooData.PowerRailCnt)
        return NvError_BadValue;

    /* Extract the voltage (units are millivolts). */
    Voltage = NvUStrtoul(parms->parm[2].str, NULL, 16);

    /* Set the voltage on the rail. */
    e = NvRmDiagConfigurePowerRail(ShmooData.PowerRails[PowerRailId],
                                   Voltage);
    NvTioDebugf("NvRmDiagConfigurePowerRail returned %d\n", e); 

    if (!e)
        NvTioGdbtCmdString(cmd, "OK");

    return e;
}

NvError
NvTioShmooCmdModRegRd(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ModuleId;
    NvU32   Offset;
    NvU32   Value;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    Offset   = NvUStrtoul(parms->parm[2].str, NULL, 16);

#if 0 /* FIXME: use something else for register read/writes */
    /* Read the register. */
    NvRmHostModuleRegRd(ShmooData.RmDeviceHandle, ModuleId, 1, &Offset, &Value);
#endif

    /* Return the value */
    NvTioGdbtCmdString(cmd, "0x");
    NvTioGdbtCmdHexU32(cmd, Value, 8);

    return NvSuccess;
}

NvError
NvTioShmooCmdModRegWr(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   ModuleId;
    NvU32   Offset;
    NvU32   Value;

    if (!ShmooData.Valid) return NvError_InvalidState;

    /* Extract and check the arguments. */
    ModuleId = NvUStrtoul(parms->parm[1].str, NULL, 16);
    Offset   = NvUStrtoul(parms->parm[2].str, NULL, 16);
    Value    = NvUStrtoul(parms->parm[3].str, NULL, 16);

#if 0 /* FIXME: use something else for register read/writes */
    /* Read the register. */
    NvRmHostModuleRegWr(ShmooData.RmDeviceHandle, ModuleId, 1, &Offset, &Value);
#endif

    /* Return success. */
    NvTioGdbtCmdString(cmd, "OK");

    return NvSuccess;
}
