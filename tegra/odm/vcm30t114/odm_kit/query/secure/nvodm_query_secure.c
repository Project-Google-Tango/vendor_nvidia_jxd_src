/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit:
 *         Implementation of the ODM Query API</b>
 *
 * @b Description: Implements the query functions for ODMs that may be
 *                 accessed at boot-time, runtime, or anywhere in between.
 */

#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvodm_keylist_reserved.h"
#include "nvrm_drf.h"
#include "../tegra_devkit_custopt.h"

static NvU32 GetBctKeyValue(void);

/* --- Function Implementations ---*/

static NvU32
GetBctKeyValue(void)
{
    NvOdmServicesKeyListHandle hKeyList = NULL;
    NvU32 BctCustOpt = 0;

    hKeyList = NvOdmServicesKeyListOpen();
    if (hKeyList)
    {
        BctCustOpt =
            NvOdmServicesGetKeyValue(hKeyList,
                                     NvOdmKeyListId_ReservedBctCustomerOption);
        NvOdmServicesKeyListClose(hKeyList);
    }

    return BctCustOpt;
}

/**
 * This function is called from early boot process.
 * Therefore, it cannot use global variables.
 */
NvU64 NvOdmQueryOsMemSize(NvOdmMemoryType MemType, const NvOdmOsOsInfo *pOsInfo)
{
    NvU32 SdramBctCustOpt;
    NvU32 SdramSize;
    static NvU32 keyValue = 0;

    if (pOsInfo == NULL)
    {
        return 0;
    }

    switch (MemType)
    {
        case NvOdmMemoryType_Sdram:

            /* Skip reading Bct key value everytime */
            if (!keyValue)
            {
                SdramBctCustOpt = GetBctKeyValue();
                keyValue = SdramBctCustOpt;
            }
            else
            {
                SdramBctCustOpt = keyValue;
            }

            switch (NV_DRF_VAL(TEGRA_DEVKIT, BCT_SYSTEM, MEMORY, SdramBctCustOpt))
            {
                // Default to the safest option
                case TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_DEFAULT:
                default:

                case TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_1:
                    SdramSize = 0x10000000; // 256 MB
                    break;

                case TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_2:
                    SdramSize = 0x20000000; // 512 MB
                    break;

                case TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_4:
                    SdramSize = 0x40000000; // 1 GB
                    break;

                case TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_8:
                    SdramSize = 0x80000000; // 2 GB
                    break;
            }
            return (NvU64)SdramSize;

        case NvOdmMemoryType_Nor:
            return 0x00400000;  // 4 MB

        case NvOdmMemoryType_Vpr:
            return 96*1024*1024;

        case NvOdmMemoryType_Tsec:
            return 32 * 1024 * 1024;

        case NvOdmMemoryType_Xusb:
            return 2*1024*1024;

        case NvOdmMemoryType_Debug:
            // It shouldn't exceed 256MB
            return 32 * 1024 * 1024;

        case NvOdmMemoryType_Nand:
        case NvOdmMemoryType_I2CEeprom:
        case NvOdmMemoryType_Hsmmc:
        case NvOdmMemoryType_Mio:
        default:
            return 0;
    }
}

/**
 * This function is called from early boot process.
 * Therefore, it cannot use global variables.
 */
NvU32 NvOdmQueryMemSize(NvOdmMemoryType MemType)
{
    NvOdmOsOsInfo Info;

    // Get the information about the calling OS.
    (void)NvOdmOsGetOsInformation(&Info);

    return (NvU32)NvOdmQueryOsMemSize(MemType, &Info);
}

NvU32 NvOdmQuerySecureRegionSize(void)
{
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    return 0x200000; // 2 MB
#else
    return 0x0;
#endif
}

NvU32 NvOdmQueryCarveoutSize(void)
{
    return 0x08000000;  // 128 MB
}

