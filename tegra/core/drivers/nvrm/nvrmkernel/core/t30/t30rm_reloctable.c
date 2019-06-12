/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"
#include "common/nvrm_hwintf.h"
#include "t30/project_relocation_table.h"

NvU32 *
NvRmPrivGetRelocationTable( NvRmDeviceHandle hDevice );

static NvU32 s_RelocationTable[] =
{
    NV_RELOCATION_TABLE_INIT
};

NvU32 *
NvRmPrivGetRelocationTable( NvRmDeviceHandle hDevice )
{
    return s_RelocationTable;
}
