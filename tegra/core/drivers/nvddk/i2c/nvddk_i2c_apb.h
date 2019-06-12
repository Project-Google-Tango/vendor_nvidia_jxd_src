/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_I2C_APB_H
#define INCLUDED_NVDDK_I2C_APB_H

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef _MK_SHIFT_CONST
  #define _MK_SHIFT_CONST(_constant_) _constant_
#endif
#ifndef _MK_MASK_CONST
  #define _MK_MASK_CONST(_constant_) _constant_
#endif
#ifndef _MK_ENUM_CONST
  #define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif
#ifndef _MK_ADDR_CONST
  #define _MK_ADDR_CONST(_constant_) _constant_
#endif
#ifndef _MK_FIELD_CONST
  #define _MK_FIELD_CONST(_mask_, _shift_) (_MK_MASK_CONST(_mask_) << _MK_SHIFT_CONST(_shift_))
#endif

#define APB_MISC_PA_BASE                    0x70000000
#define APB_MISC_GP_HIDREV_0                _MK_ADDR_CONST(0x804)
#define APB_MISC_GP_HIDREV_0_CHIPID_SHIFT   _MK_SHIFT_CONST(8)
#define APB_MISC_GP_HIDREV_0_CHIPID_FIELD   _MK_FIELD_CONST(0xff, APB_MISC_GP_HIDREV_0_CHIPID_SHIFT)
#define APB_MISC_GP_HIDREV_0_CHIPID_RANGE   15:8

#if defined(__cplusplus)
}
#endif
#endif

