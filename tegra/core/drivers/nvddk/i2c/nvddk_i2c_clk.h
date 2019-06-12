/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_I2C_CLK_H
#define INCLUDED_NVDDK_I2C_CLK_H

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

#define CLK_RST_BASE                                                        0x60006000
#define NV_ADDRESS_MAP_TMRUS_BASE                                           0x60005010
#define TIMERUS_CNTR_1US_0                                                  _MK_ADDR_CONST(0x0)
#define CLK_RST_CONTROLLER_OSC_CTRL_0                                       _MK_ADDR_CONST(0x50)
#define CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_RANGE                        31:28

#define CLK_RST_CONTROLLER_RST_DEVICES_L_0                                  _MK_ADDR_CONST(0x4)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0                                  _MK_ADDR_CONST(0x10)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0                                _MK_ADDR_CONST(0x124)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0_I2C1_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0_I2C1_CLK_SRC_CLK_M             _MK_ENUM_CONST(3)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0_I2C1_CLK_SRC_RANGE             31:30
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_I2C1_RANGE               12:12
#define CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_I2C1_RST_RANGE               12:12

#define CLK_RST_CONTROLLER_RST_DEVICES_H_0                                  _MK_ADDR_CONST(0x8)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0                                  _MK_ADDR_CONST(0x14)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0                                _MK_ADDR_CONST(0x198)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0_I2C2_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0_I2C2_CLK_SRC_CLK_M             _MK_ENUM_CONST(3)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0_I2C2_CLK_SRC_RANGE             31:30
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_I2C2_RANGE               22:22
#define CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_I2C2_RST_RANGE               22:22

#define CLK_RST_CONTROLLER_RST_DEVICES_U_0                                  _MK_ADDR_CONST(0xc)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0                                  _MK_ADDR_CONST(0x18)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0                                _MK_ADDR_CONST(0x1b8)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0_I2C3_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0_I2C3_CLK_SRC_CLK_M             _MK_ENUM_CONST(3)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0_I2C3_CLK_SRC_RANGE             31:30
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_ENB_I2C3_RANGE               3:3
#define CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_I2C3_RST_RANGE               3:3

#define CLK_RST_CONTROLLER_RST_DEVICES_V_0                                  _MK_ADDR_CONST(0x358)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0                                  _MK_ADDR_CONST(0x360)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0                                _MK_ADDR_CONST(0x3c4)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0_I2C4_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0_I2C4_CLK_SRC_CLK_M             _MK_ENUM_CONST(3)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0_I2C4_CLK_SRC_RANGE             31:30
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0_CLK_ENB_I2C4_RANGE               7:7
#define CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_I2C4_RST_RANGE               7:7

#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0                                _MK_ADDR_CONST(0x128)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0_I2C5_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0_RESET_VAL                      _MK_MASK_CONST(0xc0000000)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0_I2C5_CLK_SRC_CLK_M             _MK_ENUM_CONST(3)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0_I2C5_CLK_SRC_RANGE             31:30
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_I2C5_RANGE               15:15
#define CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_I2C5_RST_RANGE               15:15

#define CLK_RST_CONTROLLER_RST_DEVICES_X_0                                  _MK_ADDR_CONST(0x28c)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_X_0                                  _MK_ADDR_CONST(0x280)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0                                _MK_ADDR_CONST(0x65c)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0_I2C6_CLK_DIVISOR_RANGE         15:0
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0_I2C6_CLK_SRC_CLK_M                 _MK_ENUM_CONST(6)
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0_I2C6_CLK_SRC_RANGE                 31:29
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_X_0_CLK_ENB_I2C6_RANGE               6:6
#define CLK_RST_CONTROLLER_RST_DEVICES_X_0_SWR_I2C6_RST_RANGE               6:6

#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0                                  _MK_ADDR_CONST(0x364)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_RANGE               27:27
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_ENABLE              _MK_ENUM_CONST(1)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_SHIFT               _MK_SHIFT_CONST(27)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_FIELD               _MK_FIELD_CONST(0x1, CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_SHIFT)
#define CLK_RST_CONTROLLER_RST_DEVICES_W_0_SWR_DVFS_RST_ENABLE              _MK_ENUM_CONST(1)

#define CLK_RST_CONTROLLER_RST_DEVICES_W_0                                  _MK_ADDR_CONST(0x35c)
#define CLK_RST_CONTROLLER_RST_DEVICES_W_0_SWR_DVFS_RST_RANGE               27:27
#define CLK_RST_CONTROLLER_RST_DEVICES_W_0_SWR_DVFS_RST_DISABLE             _MK_ENUM_CONST(0)

#if defined(__cplusplus)
}
#endif
#endif

