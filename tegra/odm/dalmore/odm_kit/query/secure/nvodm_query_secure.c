/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
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
#include "nvos.h"
#include "nvbct.h"
#include "nvboot_sdram_param.h"

/**
 * This function is called from early boot process.
 * Therefore, it cannot use global variables.
 */
NvU64 NvOdmQueryOsMemSize(NvOdmMemoryType MemType, const NvOdmOsOsInfo *pOsInfo)
{
    NvBctHandle hBct = NULL;
    static NvU32 Size = 0;
    NvU32 Instance = 0;
    void* pSdramData = NULL;
    NvError e = NvSuccess;
    NvBootSdramParams * pSdram;

    if (pOsInfo == NULL)
        return 0;

    switch (MemType)
    {
        case NvOdmMemoryType_Sdram:

            // If SDRAM size is already got, don't call these functions again
            if (!Size)
            {
                NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));
                Size = 0;
                NV_CHECK_ERROR_CLEANUP(NvBctGetData(hBct,
                                       NvBctDataType_SdramConfigInfo,
                                       &Size, &Instance, NULL));
                Instance = 0;
                pSdramData = (void*)NvOdmOsAlloc(Size);
                NvOdmOsMemset(pSdramData, 0, Size);
                NV_CHECK_ERROR_CLEANUP(NvBctGetData(hBct,
                                       NvBctDataType_SdramConfigInfo,
                                       &Size, &Instance, pSdramData));
                pSdram = (NvBootSdramParams *)pSdramData;
                Size = pSdram->McEmemCfg;
                NvBctDeinit(hBct);
                NvOdmOsFree(pSdramData);
                // McEmemCfg return size in MB, So multiplied by 1024*1024
                Size <<= 20;
            }
            return (NvU64)Size;

        case NvOdmMemoryType_Nor:
            return 0x00400000;  // 4 MB

        case NvOdmMemoryType_Vpr:
            return 96*1024*1024;

        case NvOdmMemoryType_Tsec:
            return 4 * 1024 * 1024;

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
fail:
     NvOdmOsPrintf("Error while getting the SDRam size\n");
     NvBctDeinit(hBct);
     NvOdmOsFree(pSdramData);
     return 0;
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

