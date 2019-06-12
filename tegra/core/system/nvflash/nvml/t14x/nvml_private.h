/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv3p.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvboot_clocks_int.h"
#include "nvboot_fuse_int.h"
#include "nvboot_sdram_int.h"
#include "t14x/nvboot_bit.h"
#include "t14x/arapb_misc.h"
#include "t14x/arfuse.h"
#include "t14x/nvboot_version.h"
#include "t14x/nvboot_aes.h"
#include "t14x/arse.h"
#include "t14x/nvboot_se_aes.h"
#include "t14x/nvboot_se_defs.h"
#include "nvboot_se_int.h"

#define BOOTROM_BCT_ALIGNMENT (0x7F)
