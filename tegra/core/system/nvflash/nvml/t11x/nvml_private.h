/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nv3p.h"
#include "nvtest.h"
#include "t11x/nvboot_bit.h"
#include "t11x/arapb_misc.h"
#include "t11x/arapbpm.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvboot_sdram_int.h"
#include "t11x/nvboot_fuse.h"
#include "nvboot_fuse_int.h"
#include "t11x/arfuse.h"
#include "nvboot_aes_int.h"
#include "t11x/nvboot_version.h"
#include "t11x/nvboot_clocks.h"
#include "nvboot_clocks_int.h"
#include "nvml_aes.h"
#include "nvml_hash.h"
#include "t11x/arse.h"
#include "t11x/nvboot_se_aes.h"
#include "t11x/nvboot_se_defs.h"
#include "nvboot_se_int.h"

#define BOOTROM_BCT_ALIGNMENT (0x7F)
