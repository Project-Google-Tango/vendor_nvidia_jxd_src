/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_I2C_CONTROLLER_H
#define INCLUDED_NVDDK_I2C_CONTROLLER_H

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

/* I2C offset Registers */
#define NV_I2C1_OFFSET   0x00000000
#define NV_I2C2_OFFSET   0x00000400
#define NV_I2C3_OFFSET   0x00000500
#define NV_I2C4_OFFSET   0x00000700
#define NV_I2C5_OFFSET   0x00001000
#define NV_I2C6_OFFSET   0x00001100

#define NV_ADDRESS_MAP_I2C_BASE                                             0x7000C000
#define I2C_I2C_CNFG_0                                                      _MK_ADDR_CONST(0x0)
#define I2C_I2C_CNFG_0_LENGTH_RANGE                                         3:1
#define I2C_I2C_CNFG_0_CMD1_RANGE                                           6:6
#define I2C_I2C_CNFG_0_CMD1_ENABLE                                          _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_CMD2_RANGE                                           7:7
#define I2C_I2C_CNFG_0_CMD2_ENABLE                                          _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_SLV2_ENABLE                                          _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_SLV2_RANGE                                           4:4
#define I2C_I2C_CNFG_0_SEND_GO                                              _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_SEND_RANGE                                           9:9
#define I2C_I2C_CNFG_0_A_MOD_RANGE                                          0:0
#define I2C_I2C_CNFG_0_A_MOD_SHIFT                                          _MK_SHIFT_CONST(0)
#define I2C_I2C_CNFG_0_A_MOD_FIELD                                          _MK_FIELD_CONST(0x1, I2C_I2C_CNFG_0_A_MOD_SHIFT)
#define I2C_I2C_CNFG_0_A_MOD_SEVEN_BIT_DEVICE_ADDRESS                       _MK_ENUM_CONST(0)
#define I2C_I2C_CNFG_0_A_MOD_TEN_BIT_DEVICE_ADDRESS                         _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_NOACK_SHIFT                                          _MK_SHIFT_CONST(8)
#define I2C_I2C_CNFG_0_NOACK_FIELD                                          _MK_FIELD_CONST(0x1, I2C_I2C_CNFG_0_NOACK_SHIFT)
#define I2C_I2C_CNFG_0_NOACK_RANGE                                          8:8
#define I2C_I2C_CNFG_0_NOACK_ENABLE                                         _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_START_SHIFT                                          _MK_SHIFT_CONST(5)
#define I2C_I2C_CNFG_0_START_FIELD                                          _MK_FIELD_CONST(0x1, I2C_I2C_CNFG_0_START_SHIFT)
#define I2C_I2C_CNFG_0_START_RANGE                                          5:5
#define I2C_I2C_CNFG_0_START_ENABLE                                         _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_CMD2_ENABLE                                          _MK_ENUM_CONST(1)
#define I2C_I2C_CNFG_0_NEW_MASTER_FSM_RANGE                                 11:11
#define I2C_I2C_CNFG_0_NEW_MASTER_FSM_ENABLE                                _MK_ENUM_CONST(1)

#define I2C_I2C_STATUS_0                                                    _MK_ADDR_CONST(0x1c)
#define I2C_I2C_STATUS_0_BUSY_SHIFT                                         _MK_SHIFT_CONST(8)
#define I2C_I2C_STATUS_0_BUSY_FIELD                                         _MK_FIELD_CONST(0x1, I2C_I2C_STATUS_0_BUSY_SHIFT)
#define I2C_I2C_STATUS_0_CMD1_STAT_SHIFT                                    _MK_SHIFT_CONST(0)
#define I2C_I2C_STATUS_0_CMD1_STAT_FIELD                                    _MK_FIELD_CONST(0xf, I2C_I2C_STATUS_0_CMD1_STAT_SHIFT)
#define I2C_I2C_STATUS_0_CMD2_STAT_SHIFT                                    _MK_SHIFT_CONST(4)
#define I2C_I2C_STATUS_0_CMD2_STAT_FIELD                                    _MK_FIELD_CONST(0xf, I2C_I2C_STATUS_0_CMD2_STAT_SHIFT)

#define I2C_I2C_CMD_ADDR0_0                                                 _MK_ADDR_CONST(0x4)
#define I2C_I2C_CMD_ADDR1_0                                                 _MK_ADDR_CONST(0x8)

#define I2C_I2C_CMD_DATA1_0                                                 _MK_ADDR_CONST(0xc)
#define I2C_I2C_CMD_DATA2_0                                                 _MK_ADDR_CONST(0x10)

#define I2C_I2C_BUS_CLEAR_CONFIG_0                                          _MK_ADDR_CONST(0x84)
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_STOP_COND_NO_STOP                     _MK_ENUM_CONST(0)
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_STOP_COND_STOP                        _MK_ENUM_CONST(1)
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_STOP_COND_RANGE                       2:2
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_TERMINATE_RANGE                       1:1
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_TERMINATE_THRESHOLD                   _MK_ENUM_CONST(0)
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_SCLK_THRESHOLD_RANGE                  23:16
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_ENABLE_RANGE                          0:0
#define I2C_I2C_BUS_CLEAR_CONFIG_0_BC_TERMINATE_IMMEDIATE                   _MK_ENUM_CONST(1)

#define I2C_I2C_BUS_CLEAR_STATUS_0                                          _MK_ADDR_CONST(0x88)
#define I2C_I2C_BUS_CLEAR_STATUS_0_BC_STATUS_CLEARED                        _MK_ENUM_CONST(1)
#define I2C_I2C_BUS_CLEAR_STATUS_0_BC_STATUS_RANGE                          0:0

#define I2C_INTERRUPT_STATUS_REGISTER_0                                     _MK_ADDR_CONST(0x68)
#define I2C_INTERRUPT_STATUS_REGISTER_0_BUS_CLEAR_DONE_SET                  _MK_ENUM_CONST(1)
#define I2C_INTERRUPT_STATUS_REGISTER_0_BUS_CLEAR_DONE_RANGE                11:11

#define I2C_I2C_CONFIG_LOAD_0                                               _MK_ADDR_CONST(0x8c)
#define I2C_I2C_CONFIG_LOAD_0_MSTR_CONFIG_LOAD_RANGE                        0:0
#define I2C_I2C_CONFIG_LOAD_0_MSTR_CONFIG_LOAD_ENABLE                       _MK_ENUM_CONST(1)

#define I2C_I2C_CLK_DIVISOR_REGISTER_0                                      _MK_ADDR_CONST(0x6c)
#define I2C_I2C_CLK_DIVISOR_REGISTER_0_I2C_CLK_DIVISOR_STD_FAST_MODE_RANGE  31:16
#define I2C_I2C_CLK_DIVISOR_REGISTER_0_I2C_CLK_DIVISOR_HSMODE_RANGE         15:0

#if defined(__cplusplus)
}
#endif
#endif

