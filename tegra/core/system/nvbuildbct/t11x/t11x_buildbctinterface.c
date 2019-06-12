/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvapputil.h"
#include "nvassert.h"
#include "t11x_set.h"
#include "t11x_parse.h"
#include "t11x/nvboot_bct.h"
#include "../nvbuildbct.h"
#include "t11x_data_layout.h"
#include "../parse_hal.h"

static void t11xGetBctSize(BuildBctContext *Context)
{
    Context->NvBCTLength =  sizeof(NvBootConfigTable);
}

void t11xGetBuildBctInterf(BuildBctParseInterface *pBuildBctInterf)
{
    pBuildBctInterf->ProcessConfigFile = t11xProcessConfigFile;
    pBuildBctInterf->SetValue = t11xSetValue;
    pBuildBctInterf->GetBctSize = t11xGetBctSize;
    pBuildBctInterf->UpdateBct = t11xUpdateBct;
    pBuildBctInterf->UpdateBl = t11xUpdateBl;
}
