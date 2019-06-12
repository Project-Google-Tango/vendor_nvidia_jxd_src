/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "ap15rm_private.h"


#define NVRM_MAX_BOND_OUT_REG    6

/*
 * Check BondOut register to determine which module and/or module instance
 * is not available.
 */
void
NvRmPrivCheckBondOut( NvRmDeviceHandle hDevice )
{
    NvRmModuleTable     *mod_table = 0;
    NvRmModule          *modules = 0;
    NvRmModuleInstance  *instance = 0;
    NvU32               bondOut[NVRM_MAX_BOND_OUT_REG] = {0, 0, 0, 0, 0, 0};
    NvU32               j, i, k;
    const NvU32         *table = NULL;
    NvU8                *pb = NULL;
    NvU8                val;

    NV_ASSERT( hDevice );

    NvRmGetBondOut(hDevice, &table, bondOut);

    if ( !bondOut[0] && !bondOut[1] && !bondOut[2] )
        return;

    mod_table = NvRmPrivGetModuleTable( hDevice );
    modules = mod_table->Modules;

    for ( i = 0, j = 0; j < NVRM_MAX_BOND_OUT_REG; j++ )
    {
        if ( !bondOut[j] )
        {
            i += 32;        // skip full 32-bit
            continue;
        }
        pb = (NvU8 *)&bondOut[j];
        for ( k = 0; k < 4; k++ )
        {
            val = *pb++;
            if ( !val )
            {
                i += 8;
                continue;
            }
            for( ; ; )
            {
                if ( val & 1 )
                {
                    NvU32   moduleIdInst = table[i];
                    if ( NVRM_DEVICE_UNKNOWN != moduleIdInst )
                    {
                        if ( NvSuccess == NvRmPrivGetModuleInstance(hDevice,
                                                        moduleIdInst, &instance) )
                        {
                            /* Mark instance's DevIdx to invalid value -1.  if all
                               instances for the module are invalid, mark the module
                               itself INVALID.
                               Keep instance->DeviceId to maintain instance ordering
                               since we could be bonding out, say, UARTA but UARTB and
                               UARTC still available. */
                            NvRmModuleID moduleId =
                                            NVRM_MODULE_ID_MODULE( moduleIdInst );
                            instance->DevIdx = (NvU8)-1;
                            if (0 == NvRmModuleGetNumInstances( hDevice, moduleId ))
                                modules[moduleId].Index = NVRM_MODULE_INVALID;
                        }
                    }
                }
                val = val >> 1;     // Use ARM's clz?
                if ( !val )
                {
                    i = (i + 8) & ~7;       // skip to next byte
                    break;
                }
                i++;
            }
        }
    }
}

