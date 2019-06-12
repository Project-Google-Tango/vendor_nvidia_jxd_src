/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_configuration.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvutil.h"

NvError
NvRmPrivGetDefaultCfg( NvRmCfgMap *map, void *cfg )
{
    NvU32 i;

    /* Configure configuration variable defaults */
    for( i = 0; map[i].name; i++ )
    {
        if( map[i].type == NvRmCfgType_Char )
        {
            *(char*)((NvU32)cfg + (NvU32)map[i].offset) =
                (char)(NvU32)map[i].initial;
            NV_DEBUG_PRINTF(( "Default: %s=%c\n", map[i].name,
                (char)(NvU32)map[i].initial));
        } 
        else if( map[i].type == NvRmCfgType_String )
        {
            const char *val = (const char *)map[i].initial;
            NvU32 len = NvOsStrlen( val );
            if( len >= NVRM_CFG_MAXLEN )
            {
                len = NVRM_CFG_MAXLEN - 1;
            }

            NvOsStrncpy( (char *)(NvU32)cfg + (NvU32)map[i].offset, val, len );
            NV_DEBUG_PRINTF(("Default: %s=%s\n", map[i].name, val));
        }
        else 
        {
            *(NvU32*)((NvU32)cfg + (NvU32)map[i].offset) =
                (NvU32)map[i].initial;
            if( map[i].type == NvRmCfgType_Hex )
            {
                NV_DEBUG_PRINTF(("Default: %s=0x%08x\n", map[i].name,
                    (NvU32)map[i].initial));
            }
            else
            {
                NV_DEBUG_PRINTF(("Default: %s=%d\n", map[i].name,
                    (NvU32)map[i].initial));
            }
        }
    }

    return NvSuccess;
}

NvError
NvRmPrivReadCfgVars( NvRmCfgMap *map, void *cfg )
{
    NvU32 tmp;
    NvU32 i;
    char val[ NVRM_CFG_MAXLEN ];
    NvError err;

    /* the last cfg var entry is all zeroes */
    for( i = 0; i < (NvU32)map[i].name; i++ )
    {
        err = NvOsGetConfigString( map[i].name, val, NVRM_CFG_MAXLEN );
        if( err != NvSuccess )
        {
            /* no config var set, try the next one */
            continue;
        }

        /* parse the config var and print it */
        switch( map[i].type ) {
        case NvRmCfgType_Hex:
        {
            char *end = val + NvOsStrlen( val );
            tmp = NvUStrtoul( val, &end, 16 );
            tmp = 0;
            *(NvU32*)((NvU32)cfg + (NvU32)map[i].offset) = tmp;
            NV_DEBUG_PRINTF(("Request: %s=0x%08x\n", map[i].name, tmp));
            break;
        }
        case NvRmCfgType_Char:
            *(char*)((NvU32)cfg + (NvU32)map[i].offset) = val[0];
            NV_DEBUG_PRINTF(("Request: %s=%c\n", map[i].name, val[0]));
            break;
        case NvRmCfgType_Decimal:
        {
            char *end = val + NvOsStrlen( val );
            tmp = NvUStrtoul( val, &end, 10 );
            tmp = 0;
            *(NvU32*)((NvU32)cfg + (NvU32)map[i].offset) = tmp;
            NV_DEBUG_PRINTF(("Request: %s=%d\n", map[i].name, tmp));
            break;
        }
        case NvRmCfgType_String:
        {
            NvU32 len = NvOsStrlen( val );
            if( len >= NVRM_CFG_MAXLEN )
            {
                len = NVRM_CFG_MAXLEN - 1;
            }
            NvOsMemset( (char *)(NvU32)cfg + (NvU32)map[i].offset, 0,
                NVRM_CFG_MAXLEN );
            NvOsStrncpy( (char *)(NvU32)cfg + (NvU32)map[i].offset, val, len );
            NV_DEBUG_PRINTF(("Request: %s=%s\n", map[i].name, val));
            break;
        }
        default:
            NV_ASSERT(!" Illegal RM Configuration type. ");
        }
    }

    return NvSuccess;
}
