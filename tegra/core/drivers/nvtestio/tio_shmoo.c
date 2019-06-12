/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
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
#include "nvutil.h"
#include "tio_gdbt.h"
#include "tio_local_target.h"
#include "tio_shmoo.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

/*
 * Shmoo command table.  Maps shmoo commands to data needed for error checking
 * and function pointers for executing them.
 */
typedef struct NvShmooItemRec
{
    const char   *Name;
    NvS32         ArgCount; /* -1 = variable, so checking is in CallbackFn */
    NvTioShmooCb  CallbackFn;
} NvTioShmooItem;

static NvTioShmooItem ShmooTable[] =
{
    { "nvlistmod",         0, NvTioShmooCmdModules     },
    { "nvlistclk",         0, NvTioShmooCmdListClks    },
    { "nvclkname",         1, NvTioShmooCmdClkName     },
    { "nvlistmodclk",      1, NvTioShmooCmdListModClks },
    { "nvlistpw",          0, NvTioShmooCmdListPw      },
    { "nvpwname",          1, NvTioShmooCmdPwName      },
    { "nvlistmodpw",       1, NvTioShmooCmdListModPw   },
    { "nvmclken",          2, NvTioShmooCmdClockEnable },
    { "nvmreset",          1, NvTioShmooCmdModuleReset },
    { "nvmodclk",          3, NvTioShmooCmdModuleClock },
    { "nvpll",             4, NvTioShmooCmdSetPll      },
    { "nvscaler",          4, NvTioShmooCmdClockScaler },
    { "nvpw",              2, NvTioShmooCmdSetVoltage  },
#if 0
    { "nvsetdramt",        3, NvTioShmooCmd }, /* shsetdramtiming */
    { "shaictl_???",       ?, NvTioShmooCmd }, /* shaictl */
#endif

    { "nvrd32",            1, NvTioShmooCmdRead32      },
    { "nvwr32",            2, NvTioShmooCmdWrite32     },
    { "nvrw",              3, NvTioShmooCmdModRegWr    },
    { "nvrr",              2, NvTioShmooCmdModRegRd    },

#if 0
    { "nvd0", 0, NvTioShmooCmd }, /* shdcache off     */
    { "nvd1", 0, NvTioShmooCmd }, /* shdcache on      */
    { "nvdc", 0, NvTioShmooCmd }, /* shdcache clean   */
    { "nvdb", 0, NvTioShmooCmd }, /* shdcache barrier */

    { "nvi0", 0, NvTioShmooCmd }, /* shicache off     */
    { "nvi1", 0, NvTioShmooCmd }, /* shicache on      */
    { "nvic", 0, NvTioShmooCmd }, /* shicache clean   */
#endif
};

static const NvU32 NumShmooItems = sizeof(ShmooTable)/sizeof(ShmooTable[0]);

/*
 * Implementation of shmoo commands that are RM-agnostic.
 */

/*
 * Returns the 32-bit value at a physical address.
 */
NvError
NvTioShmooCmdRead32(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   *VirtualAddress;
    NvU32    PhysicalAddress;
    NvU32    Value;
    size_t   size = sizeof(NvU32);
    NvError  e;

    /* Extract the address. */
    PhysicalAddress = NvUStrtoul(parms->parm[1].str, NULL, 16);

    NvTioDebugf("PhysicalAddress = 0x%08x\n", PhysicalAddress);

    /* Map the physical address to a virtual address. */
    e = NvOsPhysicalMemMap(PhysicalAddress,
                           size,
                           NvOsMemAttribute_WriteBack,
                           NVOS_MEM_READ,
                           (void**)&VirtualAddress);
    NvTioDebugf("NvOsPhysicalMemMap() returned %d\n", e);
    NvTioDebugf("VirtualAddress = 0x%08x\n", VirtualAddress);
    
    if (e == NvSuccess)
    {
        Value = *VirtualAddress;

        /* Return the value */
        NvTioGdbtCmdString(cmd, "0x");
        NvTioGdbtCmdHexU32(cmd, Value, 8);

        /* Unmap the address */
        NvOsPhysicalMemUnmap(VirtualAddress, size);
    }

    return e;
}

/*
 * Writes the 32-bit value at a physical address.
 */
NvError
NvTioShmooCmdWrite32(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32   *VirtualAddress;
    NvU32    PhysicalAddress;
    NvU32    Value;
    size_t   size = sizeof(NvU32);
    NvError  e;

    /* Extract the address and value. */
    PhysicalAddress = NvUStrtoul(parms->parm[1].str, NULL, 16);
    Value           = NvUStrtoul(parms->parm[2].str, NULL, 16);

    NvTioDebugf("PhysicalAddress = 0x%08x\n", PhysicalAddress);
    NvTioDebugf("Value           = 0x%08x\n", Value);

    /* Map the physical address to a virtual address. */
    e = NvOsPhysicalMemMap(PhysicalAddress,
                           size,
                           NvOsMemAttribute_WriteBack,
                           NVOS_MEM_WRITE,
                           (void**)&VirtualAddress);
    NvTioDebugf("NvOsPhysicalMemMap() returned %d\n", e);
    NvTioDebugf("VirtualAddress = 0x%08x\n", VirtualAddress);
    
    if (e == NvSuccess)
    {
        *VirtualAddress = Value;

        /* Return OK to indicate success */
        NvTioGdbtCmdString(cmd, "OK");

        /* Unmap the address */
        NvOsPhysicalMemUnmap(VirtualAddress, size);
    }

    return e;
}


/*
 * Note: NvTioGdbtCmdBegin() has already been called for cmd.
 *       Shmoo commands add their output to the reply,
 *       and the caller completes the transmission of the reply.
 */
NvError
NvTioShmooProcessCommand(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    char* name = parms->parm[0].str;
    NvU32   i;

    NvTioDebugf("target: Got command %s\n", name);

    for (i = 0; i < NumShmooItems; i++)
    {
        if (NvOsStrcmp(name, ShmooTable[i].Name) == 0)
        {
            /* Check arg count.  Note: parmCnt includes the cmd name */
            if ((ShmooTable[i].ArgCount >= 0) &&
                (ShmooTable[i].ArgCount != ((NvS32)parms->parmCnt - 1)))
            {
                return NvError_BadParameter;
            }

            /* Execute the command callback. */
            return ShmooTable[i].CallbackFn(parms, cmd);
        }
    }

    /* bad or unknown command - send null response by doing nothing here */

    return NvSuccess;
}
