/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>API defined for libnvchip </b>
  *
  * @b Description: Implements the chipid utility function.
  *
  */
#include "nvchip_utility.h"
#include "arapb_misc.h"
#include "nvrm_drf.h"
#define REGR(a)     *((const volatile NvU32 *)(a))

NvBool  NvIsCurrentChipLaterThan(NvU32 Chip_id)
{
    NvU32 tmp, i = 0;
    NvU32 Currentchipid;
    NvU32 Chips[] = {0x30, 0x35, 0x14, 0x40};

    tmp = REGR( MISC_PA_BASE + APB_MISC_GP_HIDREV_0 );
    Currentchipid = NV_DRF_VAL( APB_MISC_GP, HIDREV, CHIPID, tmp );

    for(i = 0; i < NV_ARRAY_SIZE(Chips); i++)
    {
       if (Chips[i] == Currentchipid)
           return NV_FALSE;
       if (Chips[i] == Chip_id)
           return NV_TRUE;
    }
    return NV_FALSE;
}
