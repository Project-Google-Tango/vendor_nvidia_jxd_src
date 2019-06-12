/**
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvapputil.h"
#include "nvassert.h"
#include "t12x_set.h"
#include "t12x_parse.h"
#include "t12x/nvboot_bct.h"
#include "../nvbuildbct.h"
#include "t12x_data_layout.h"
#include "../parse_hal.h"

static void t12xGetBctSize(BuildBctContext *Context)
{
    Context->NvBCTLength =  sizeof(NvBootConfigTable);
}

void t12xGetBuildBctInterf(BuildBctParseInterface *pBuildBctInterf)
{
    pBuildBctInterf->ProcessConfigFile = t12xProcessConfigFile;
    pBuildBctInterf->SetValue = t12xSetValue;
    pBuildBctInterf->GetBctSize = t12xGetBctSize;
    pBuildBctInterf->UpdateBct = t12xUpdateBct;
    pBuildBctInterf->UpdateBl = t12xUpdateBl;
}
