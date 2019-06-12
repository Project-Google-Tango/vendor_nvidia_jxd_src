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
#include "t30_set.h"
#include "t30_parse.h"
#include "t30/nvboot_bct.h"
#include "../nvbuildbct.h"
#include "t30_data_layout.h"
#include "../parse_hal.h"

static void t30GetBctSize(BuildBctContext *Context)
{
    Context->NvBCTLength =  sizeof(NvBootConfigTable);
}

void t30GetBuildBctInterf(BuildBctParseInterface *pBuildBctInterf)
{
    pBuildBctInterf->ProcessConfigFile = t30ProcessConfigFile;
    pBuildBctInterf->GetBctSize = t30GetBctSize;
    pBuildBctInterf->SetValue = t30SetValue;
    pBuildBctInterf->UpdateBct = t30UpdateBct;
    pBuildBctInterf->UpdateBl = t30UpdateBl;
}

