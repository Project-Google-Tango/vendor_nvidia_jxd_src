/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvddk_operatingmodes.h"
#include "nvddk_fuse.h"
#include "nvddk_fuse_common.h"

NvBool NvDdkFuseSkipDevSelStraps(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_SW_0);
    RegData = NV_DRF_VAL(FUSE, RESERVED_SW, SKIP_DEV_SEL_STRAPS, RegData);

    if (RegData)
        return NV_TRUE;
    else
        return NV_FALSE;
}

NvBool IsSbkOrDkSet(void)
{
    NvU32 AllKeysOred;

    AllKeysOred  = NvDdkFuseIsSbkSet();
    AllKeysOred |= FUSE_NV_READ32(FUSE_PRIVATE_KEY4_NONZERO_0);

    if (AllKeysOred)
        return NV_TRUE;
    else
        return NV_FALSE;
}

NvBool NvDdkFuseIsSbkSet(void)
{
    NvU32 AllKeysOred;

    AllKeysOred  = FUSE_NV_READ32(FUSE_PRIVATE_KEY0_NONZERO_0);
    AllKeysOred |= FUSE_NV_READ32(FUSE_PRIVATE_KEY1_NONZERO_0);
    AllKeysOred |= FUSE_NV_READ32(FUSE_PRIVATE_KEY2_NONZERO_0);
    AllKeysOred |= FUSE_NV_READ32(FUSE_PRIVATE_KEY3_NONZERO_0);
    if (AllKeysOred)
        return NV_TRUE;
    else
        return NV_FALSE;
}

NvBool NvDdkFuseIsFailureAnalysisMode(void)
{
    volatile NvU32 RegValue;
    RegValue = FUSE_NV_READ32(FUSE_FA_0);
    if (RegValue)
    {
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}

NvBool NvDdkFuseIsOdmProductionModeFuseSet(void)
{
    NvU32 RegValue;

    RegValue = FUSE_NV_READ32(FUSE_SECURITY_MODE_0);
    if (RegValue)
    {
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}

NvBool NvDdkFuseIsNvProductionModeFuseSet(void)
{
    NvU32 RegValue;
    RegValue = FUSE_NV_READ32(FUSE_PRODUCTION_MODE_0);
    if (RegValue)
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

NvError NvDdkFuseCopyBytes(NvU32 RegAddress, NvU8 *pByte, const NvU32 nBytes)
{
    NvU32 RegData;
    NvU32 i;

    if ((nBytes == 0) || (RegAddress == 0))
        return NvError_BadParameter;

    for (i = 0, RegData = 0; i < nBytes; i++)
    {
        if ((i&3) == 0)
        {
            RegData = FUSE_NV_READ32(RegAddress);
            RegAddress += 4;
        }
        pByte[i] = RegData & 0xFF;
        RegData >>= 8;
    }

    return NvSuccess;
}

NvU32 NvDdkFuseGetBootDevInfo(void)
{
    return NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_BOOT_DEVICE_INFO_0);
}

/* Expose (Visibility = 1) or hide (Visibility = 0) the fuse registers.
*/
void SetFuseRegVisibility(NvU32 Visibility)
{
    NvU32 RegData;

    RegData = CLOCK_NV_READ32(CLK_RST_CONTROLLER_MISC_CLK_ENB_0);
    RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                                 MISC_CLK_ENB,
                                 CFG_ALL_VISIBLE,
                                 Visibility,
                                 RegData);
    CLOCK_NV_WRITE32(CLK_RST_CONTROLLER_MISC_CLK_ENB_0, RegData);
}

/**
 * Reports whether the Disable Register Program register is set.
 *
 * @param none
 *
 * @return NV_TRUE if  Disable reg program is iset, else NV_FALSE
 */

NvBool NvDdkPrivIsDisableRegProgramSet(void)
{
    NvU32 RegValue;
    RegValue = FUSE_NV_READ32(FUSE_DISABLEREGPROGRAM_0);
    if (RegValue)
    {
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}

NvU32 NvFuseUtilGetTimeUS( void )
{
    /*
     * Should we take care of roll over of us counter?
     * roll over happens after 71.58 minutes.
     */
    NvU32 usec;
    usec = *(volatile NvU32 *)(NV_ADDRESS_MAP_TMRUS_BASE);
    return usec;
}

void NvFuseUtilWaitUS( NvU32 usec )
{
    NvU32 t0;
    NvU32 t1;

    t0 = NvFuseUtilGetTimeUS();
    t1 = t0;

    // Use the difference for the comparison to be wraparound safe
    while ((t1 - t0) < usec)
    {
        t1 = NvFuseUtilGetTimeUS();
    }
}
void fusememset(void* Source, NvU8 val, NvU32 size)
{
    NvU32 count=0;
    NvU8 *src = (NvU8*)Source;
    if(src == NULL)
        return;
    for(count = 0; count < size; count++)
        *(src + count) = val;
}

void fusememcpy(void* Destination, void* Source, NvU32 size)
{
    NvU32 count = 0;
    NvU8* src = (NvU8*) Source;
    NvU8* dest = (NvU8*) Destination;

    if ((src == NULL) || (dest == NULL))
        return;
    for (count = 0;count < size; count++)
        *(dest + count) = *(src + count);
}
