/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef ARDEV_T_FPCI_XUSB_H_
#define ARDEV_T_FPCI_XUSB_H_

#define XUSB_CFG_0_0                                       _MK_ADDR_CONST(0x00000000)
#define XUSB_CFG_0_0_SECURE                                0
#define XUSB_CFG_0_0_WORD_COUNT                            1
#define XUSB_CFG_0_0_RESET_VAL                             _MK_MASK_CONST(0xe1610de)
#define XUSB_CFG_0_0_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_0_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0xe1610de)
#define XUSB_CFG_0_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_0_0_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_0_0_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_0_0_VENDOR_ID_SHIFT                        _MK_SHIFT_CONST(0)
#define XUSB_CFG_0_0_VENDOR_ID_FIELD                       (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_0_0_VENDOR_ID_SHIFT)
#define XUSB_CFG_0_0_VENDOR_ID_RANGE                       15:0
#define XUSB_CFG_0_0_VENDOR_ID_WOFFSET                     0
#define XUSB_CFG_0_0_VENDOR_ID_DEFAULT                     (_MK_MASK_CONST(0x000010DE)
#define XUSB_CFG_0_0_VENDOR_ID_DEFAULT_MASK                (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_0_0_VENDOR_ID_SW_DEFAULT                  (_MK_MASK_CONST(0x000010DE)
#define XUSB_CFG_0_0_VENDOR_ID_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_0_0_VENDOR_ID_NVIDIA                      _MK_ENUM_CONST(0x000010DE)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_SHIFT                   _MK_SHIFT_CONST(16)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_FIELD                  (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_0_0_DEVICE_ID_UNIT_SHIFT)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_RANGE                  31:16
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_WOFFSET                0
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_DEFAULT                (_MK_MASK_CONST(0x00000E16)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_DEFAULT_MASK           (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_SW_DEFAULT             (_MK_MASK_CONST(0x00000E16)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_SW_DEFAULT_MASK        (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_0_0_DEVICE_ID_UNIT_XUSB                   _MK_ENUM_CONST(0x00000E16)
#define XUSB_CFG_1_0                                       _MK_ADDR_CONST(0x00000004)
#define XUSB_CFG_1_0_SECURE                                0
#define XUSB_CFG_1_0_WORD_COUNT                            1
#define XUSB_CFG_1_0_RESET_VAL                             _MK_MASK_CONST(0xb00000)
#define XUSB_CFG_1_0_RESET_MASK                            _MK_MASK_CONST(0xffb007ff)
#define XUSB_CFG_1_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0xb00000)
#define XUSB_CFG_1_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffb007ff)
#define XUSB_CFG_1_0_READ_MASK                             _MK_MASK_CONST(0xffb807ff)
#define XUSB_CFG_1_0_WRITE_MASK                            _MK_MASK_CONST(0x407)
#define XUSB_CFG_1_0_IO_SPACE_SHIFT                         _MK_SHIFT_CONST(0)
#define XUSB_CFG_1_0_IO_SPACE_FIELD                        (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_IO_SPACE_SHIFT)
#define XUSB_CFG_1_0_IO_SPACE_RANGE                        0:0
#define XUSB_CFG_1_0_IO_SPACE_WOFFSET                      0
#define XUSB_CFG_1_0_IO_SPACE_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_IO_SPACE_DEFAULT_MASK                 (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_IO_SPACE_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_IO_SPACE_SW_DEFAULT_MASK              (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_IO_SPACE__WRNPRDCHK                   _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_IO_SPACE_DISABLED                     _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_IO_SPACE_ENABLED                      _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_MEMORY_SPACE_SHIFT                     _MK_SHIFT_CONST(1)
#define XUSB_CFG_1_0_MEMORY_SPACE_FIELD                    (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_MEMORY_SPACE_SHIFT)
#define XUSB_CFG_1_0_MEMORY_SPACE_RANGE                    1:1
#define XUSB_CFG_1_0_MEMORY_SPACE_WOFFSET                  0
#define XUSB_CFG_1_0_MEMORY_SPACE_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_MEMORY_SPACE_DEFAULT_MASK             (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_MEMORY_SPACE_SW_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_MEMORY_SPACE_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_MEMORY_SPACE__WRNPRDCHK               _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_MEMORY_SPACE_DISABLED                 _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_MEMORY_SPACE_ENABLED                  _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_BUS_MASTER_SHIFT                       _MK_SHIFT_CONST(2)
#define XUSB_CFG_1_0_BUS_MASTER_FIELD                      (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_BUS_MASTER_SHIFT)
#define XUSB_CFG_1_0_BUS_MASTER_RANGE                      2:2
#define XUSB_CFG_1_0_BUS_MASTER_WOFFSET                    0
#define XUSB_CFG_1_0_BUS_MASTER_DEFAULT                    (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_BUS_MASTER_DEFAULT_MASK               (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_BUS_MASTER_SW_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_BUS_MASTER_SW_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_BUS_MASTER__WRNPRDCHK                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_BUS_MASTER_DISABLED                   _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_BUS_MASTER_ENABLED                    _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_SHIFT                    _MK_SHIFT_CONST(3)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_FIELD                   (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_SPECIAL_CYCLE_SHIFT)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_RANGE                   3:3
#define XUSB_CFG_1_0_SPECIAL_CYCLE_WOFFSET                 0
#define XUSB_CFG_1_0_SPECIAL_CYCLE_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_DISABLED                _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_SPECIAL_CYCLE_ENABLED                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_SHIFT                  _MK_SHIFT_CONST(4)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_FIELD                 (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_WRITE_AND_INVAL_SHIFT)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_RANGE                 4:4
#define XUSB_CFG_1_0_WRITE_AND_INVAL_WOFFSET               0
#define XUSB_CFG_1_0_WRITE_AND_INVAL_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_SW_DEFAULT_MASK       (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_DISABLED              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_WRITE_AND_INVAL_ENABLED               _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_PALETTE_SNOOP_SHIFT                    _MK_SHIFT_CONST(5)
#define XUSB_CFG_1_0_PALETTE_SNOOP_FIELD                   (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_PALETTE_SNOOP_SHIFT)
#define XUSB_CFG_1_0_PALETTE_SNOOP_RANGE                   5:5
#define XUSB_CFG_1_0_PALETTE_SNOOP_WOFFSET                 0
#define XUSB_CFG_1_0_PALETTE_SNOOP_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_PALETTE_SNOOP_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_PALETTE_SNOOP_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_PALETTE_SNOOP_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_PALETTE_SNOOP_DISABLED                _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_PALETTE_SNOOP_ENABLED                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_PERR_SHIFT                             _MK_SHIFT_CONST(6)
#define XUSB_CFG_1_0_PERR_FIELD                            (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_PERR_SHIFT)
#define XUSB_CFG_1_0_PERR_RANGE                            6:6
#define XUSB_CFG_1_0_PERR_WOFFSET                          0
#define XUSB_CFG_1_0_PERR_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_PERR_DEFAULT_MASK                     (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_PERR_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_PERR_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_PERR_DISABLED                         _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_PERR_ENABLED                          _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_STEP_SHIFT                             _MK_SHIFT_CONST(7)
#define XUSB_CFG_1_0_STEP_FIELD                            (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_STEP_SHIFT)
#define XUSB_CFG_1_0_STEP_RANGE                            7:7
#define XUSB_CFG_1_0_STEP_WOFFSET                          0
#define XUSB_CFG_1_0_STEP_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_STEP_DEFAULT_MASK                     (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_STEP_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_STEP_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_STEP_DISABLED                         _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_STEP_ENABLED                          _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_SERR_SHIFT                             _MK_SHIFT_CONST(8)
#define XUSB_CFG_1_0_SERR_FIELD                            (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_SERR_SHIFT)
#define XUSB_CFG_1_0_SERR_RANGE                            8:8
#define XUSB_CFG_1_0_SERR_WOFFSET                          0
#define XUSB_CFG_1_0_SERR_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SERR_DEFAULT_MASK                     (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SERR_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SERR_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SERR_DISABLED                         _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_SERR_ENABLED                          _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_BACK2BACK_SHIFT                        _MK_SHIFT_CONST(9)
#define XUSB_CFG_1_0_BACK2BACK_FIELD                       (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_BACK2BACK_SHIFT)
#define XUSB_CFG_1_0_BACK2BACK_RANGE                       9:9
#define XUSB_CFG_1_0_BACK2BACK_WOFFSET                     0
#define XUSB_CFG_1_0_BACK2BACK_DEFAULT                     (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_BACK2BACK_DEFAULT_MASK                (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_BACK2BACK_SW_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_BACK2BACK_SW_DEFAULT_MASK             (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_BACK2BACK_DISABLED                    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_BACK2BACK_ENABLED                     _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_INTR_DISABLE_SHIFT                     _MK_SHIFT_CONST(10)
#define XUSB_CFG_1_0_INTR_DISABLE_FIELD                    (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_INTR_DISABLE_SHIFT)
#define XUSB_CFG_1_0_INTR_DISABLE_RANGE                    10:10
#define XUSB_CFG_1_0_INTR_DISABLE_WOFFSET                  0
#define XUSB_CFG_1_0_INTR_DISABLE_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_INTR_DISABLE_DEFAULT_MASK             (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_INTR_DISABLE_SW_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_INTR_DISABLE_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_INTR_DISABLE_ON                       _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_INTR_DISABLE_OFF                      _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_INTR_STATUS_SHIFT                      _MK_SHIFT_CONST(19)
#define XUSB_CFG_1_0_INTR_STATUS_FIELD                     (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_INTR_STATUS_SHIFT)
#define XUSB_CFG_1_0_INTR_STATUS_RANGE                     19:19
#define XUSB_CFG_1_0_INTR_STATUS_WOFFSET                   0
#define XUSB_CFG_1_0_INTR_STATUS_0                         _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_INTR_STATUS_1                         _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_CAPLIST_SHIFT                          _MK_SHIFT_CONST(20)
#define XUSB_CFG_1_0_CAPLIST_FIELD                         (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_CAPLIST_SHIFT)
#define XUSB_CFG_1_0_CAPLIST_RANGE                         20:20
#define XUSB_CFG_1_0_CAPLIST_WOFFSET                       0
#define XUSB_CFG_1_0_CAPLIST_DEFAULT                       (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_CAPLIST_DEFAULT_MASK                  (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_CAPLIST_SW_DEFAULT                    (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_CAPLIST_SW_DEFAULT_MASK               (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_CAPLIST_NOT_PRESENT                   _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_CAPLIST_PRESENT                       _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_66MHZ_SHIFT                            _MK_SHIFT_CONST(21)
#define XUSB_CFG_1_0_66MHZ_FIELD                           (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_66MHZ_SHIFT)
#define XUSB_CFG_1_0_66MHZ_RANGE                           21:21
#define XUSB_CFG_1_0_66MHZ_WOFFSET                         0
#define XUSB_CFG_1_0_66MHZ_DEFAULT                         (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_66MHZ_DEFAULT_MASK                    (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_66MHZ_SW_DEFAULT                      (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_66MHZ_SW_DEFAULT_MASK                 (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_66MHZ_INCAPABLE                       _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_66MHZ_CAPABLE                         _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_FAST_BACK2BACK_SHIFT                   _MK_SHIFT_CONST(23)
#define XUSB_CFG_1_0_FAST_BACK2BACK_FIELD                  (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_FAST_BACK2BACK_SHIFT)
#define XUSB_CFG_1_0_FAST_BACK2BACK_RANGE                  23:23
#define XUSB_CFG_1_0_FAST_BACK2BACK_WOFFSET                0
#define XUSB_CFG_1_0_FAST_BACK2BACK_DEFAULT                (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_FAST_BACK2BACK_DEFAULT_MASK           (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_FAST_BACK2BACK_SW_DEFAULT             (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_1_0_FAST_BACK2BACK_SW_DEFAULT_MASK        (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_FAST_BACK2BACK_INCAPABLE              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_FAST_BACK2BACK_CAPABLE                _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_SHIFT                 _MK_SHIFT_CONST(24)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_FIELD                (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_MASTER_DATA_PERR_SHIFT)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_RANGE                24:24
#define XUSB_CFG_1_0_MASTER_DATA_PERR_WOFFSET              0
#define XUSB_CFG_1_0_MASTER_DATA_PERR_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_SW_DEFAULT           (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_SW_DEFAULT_MASK      (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_NOT_ACTIVE           _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_ACTIVE               _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_MASTER_DATA_PERR_CLEAR                _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_DEVSEL_TIMING_SHIFT                    _MK_SHIFT_CONST(25)
#define XUSB_CFG_1_0_DEVSEL_TIMING_FIELD                   (_MK_SHIFT_CONST(0x3) << XUSB_CFG_1_0_DEVSEL_TIMING_SHIFT)
#define XUSB_CFG_1_0_DEVSEL_TIMING_RANGE                   26:25
#define XUSB_CFG_1_0_DEVSEL_TIMING_WOFFSET                 0
#define XUSB_CFG_1_0_DEVSEL_TIMING_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_DEVSEL_TIMING_DEFAULT_MASK            (_MK_MASK_CONST(0x3)
#define XUSB_CFG_1_0_DEVSEL_TIMING_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_DEVSEL_TIMING_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x3)
#define XUSB_CFG_1_0_DEVSEL_TIMING_FAST                    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_DEVSEL_TIMING_MEDIUM                  _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_DEVSEL_TIMING_SLOW                    _MK_ENUM_CONST(0x00000002)
#define XUSB_CFG_1_0_SIGNALED_TARGET_SHIFT                  _MK_SHIFT_CONST(27)
#define XUSB_CFG_1_0_SIGNALED_TARGET_FIELD                 (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_SIGNALED_TARGET_SHIFT)
#define XUSB_CFG_1_0_SIGNALED_TARGET_RANGE                 27:27
#define XUSB_CFG_1_0_SIGNALED_TARGET_WOFFSET               0
#define XUSB_CFG_1_0_SIGNALED_TARGET_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_TARGET_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SIGNALED_TARGET_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_TARGET_SW_DEFAULT_MASK       (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SIGNALED_TARGET_NO_ABORT              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_TARGET_ABORT                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_SIGNALED_TARGET_CLEAR                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_RECEIVED_TARGET_SHIFT                  _MK_SHIFT_CONST(28)
#define XUSB_CFG_1_0_RECEIVED_TARGET_FIELD                 (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_RECEIVED_TARGET_SHIFT)
#define XUSB_CFG_1_0_RECEIVED_TARGET_RANGE                 28:28
#define XUSB_CFG_1_0_RECEIVED_TARGET_WOFFSET               0
#define XUSB_CFG_1_0_RECEIVED_TARGET_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_TARGET_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_RECEIVED_TARGET_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_TARGET_SW_DEFAULT_MASK       (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_RECEIVED_TARGET_NO_ABORT              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_TARGET_ABORT                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_RECEIVED_TARGET_CLEAR                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_RECEIVED_MASTER_SHIFT                  _MK_SHIFT_CONST(29)
#define XUSB_CFG_1_0_RECEIVED_MASTER_FIELD                 (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_RECEIVED_MASTER_SHIFT)
#define XUSB_CFG_1_0_RECEIVED_MASTER_RANGE                 29:29
#define XUSB_CFG_1_0_RECEIVED_MASTER_WOFFSET               0
#define XUSB_CFG_1_0_RECEIVED_MASTER_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_MASTER_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_RECEIVED_MASTER_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_MASTER_SW_DEFAULT_MASK       (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_RECEIVED_MASTER_NO_ABORT              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_RECEIVED_MASTER_ABORT                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_RECEIVED_MASTER_CLEAR                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_SIGNALED_SERR_SHIFT                    _MK_SHIFT_CONST(30)
#define XUSB_CFG_1_0_SIGNALED_SERR_FIELD                   (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_SIGNALED_SERR_SHIFT)
#define XUSB_CFG_1_0_SIGNALED_SERR_RANGE                   30:30
#define XUSB_CFG_1_0_SIGNALED_SERR_WOFFSET                 0
#define XUSB_CFG_1_0_SIGNALED_SERR_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_SERR_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SIGNALED_SERR_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_SERR_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_SIGNALED_SERR_NOT_ACTIVE              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_SIGNALED_SERR_ACTIVE                  _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_SIGNALED_SERR_CLEAR                   _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_DETECTED_PERR_SHIFT                    _MK_SHIFT_CONST(31)
#define XUSB_CFG_1_0_DETECTED_PERR_FIELD                   (_MK_SHIFT_CONST(0x1) << XUSB_CFG_1_0_DETECTED_PERR_SHIFT)
#define XUSB_CFG_1_0_DETECTED_PERR_RANGE                   31:31
#define XUSB_CFG_1_0_DETECTED_PERR_WOFFSET                 0
#define XUSB_CFG_1_0_DETECTED_PERR_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_DETECTED_PERR_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_DETECTED_PERR_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_1_0_DETECTED_PERR_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_1_0_DETECTED_PERR_NOT_ACTIVE              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_DETECTED_PERR_ACTIVE                  _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_DETECTED_PERR_CLEAR                   _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_2_0                                       _MK_ADDR_CONST(0x00000008)
#define XUSB_CFG_2_0_SECURE                                0
#define XUSB_CFG_2_0_WORD_COUNT                            1
#define XUSB_CFG_2_0_RESET_VAL                             _MK_MASK_CONST(0xc0330a1)
#define XUSB_CFG_2_0_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_2_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0xc0330a1)
#define XUSB_CFG_2_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_2_0_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_2_0_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_2_0_REVISION_ID_SHIFT                      _MK_SHIFT_CONST(0)
#define XUSB_CFG_2_0_REVISION_ID_FIELD                     (_MK_SHIFT_CONST(0xff) << XUSB_CFG_2_0_REVISION_ID_SHIFT)
#define XUSB_CFG_2_0_REVISION_ID_RANGE                     7:0
#define XUSB_CFG_2_0_REVISION_ID_WOFFSET                   0
#define XUSB_CFG_2_0_REVISION_ID_DEFAULT                   (_MK_MASK_CONST(0x000000A1)
#define XUSB_CFG_2_0_REVISION_ID_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_REVISION_ID_SW_DEFAULT                (_MK_MASK_CONST(0x000000A1)
#define XUSB_CFG_2_0_REVISION_ID_SW_DEFAULT_MASK           (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_REVISION_ID_VAL                       _MK_ENUM_CONST(0x000000A1)
#define XUSB_CFG_2_0_REVISION_ID_A01                       _MK_ENUM_CONST(0x000000A1)
#define XUSB_CFG_2_0_PROG_IF_SHIFT                          _MK_SHIFT_CONST(8)
#define XUSB_CFG_2_0_PROG_IF_FIELD                         (_MK_SHIFT_CONST(0xff) << XUSB_CFG_2_0_PROG_IF_SHIFT)
#define XUSB_CFG_2_0_PROG_IF_RANGE                         15:8
#define XUSB_CFG_2_0_PROG_IF_WOFFSET                       0
#define XUSB_CFG_2_0_PROG_IF_DEFAULT                       (_MK_MASK_CONST(0x00000030)
#define XUSB_CFG_2_0_PROG_IF_DEFAULT_MASK                  (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_PROG_IF_SW_DEFAULT                    (_MK_MASK_CONST(0x00000030)
#define XUSB_CFG_2_0_PROG_IF_SW_DEFAULT_MASK               (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_PROG_IF_XHCI                          _MK_ENUM_CONST(0x00000030)
#define XUSB_CFG_2_0_SUB_CLASS_SHIFT                        _MK_SHIFT_CONST(16)
#define XUSB_CFG_2_0_SUB_CLASS_FIELD                       (_MK_SHIFT_CONST(0xff) << XUSB_CFG_2_0_SUB_CLASS_SHIFT)
#define XUSB_CFG_2_0_SUB_CLASS_RANGE                       23:16
#define XUSB_CFG_2_0_SUB_CLASS_WOFFSET                     0
#define XUSB_CFG_2_0_SUB_CLASS_DEFAULT                     (_MK_MASK_CONST(0x00000003)
#define XUSB_CFG_2_0_SUB_CLASS_DEFAULT_MASK                (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_SUB_CLASS_SW_DEFAULT                  (_MK_MASK_CONST(0x00000003)
#define XUSB_CFG_2_0_SUB_CLASS_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_SUB_CLASS_XUSB                        _MK_ENUM_CONST(0x00000003)
#define XUSB_CFG_2_0_BASE_CLASS_SHIFT                       _MK_SHIFT_CONST(24)
#define XUSB_CFG_2_0_BASE_CLASS_FIELD                      (_MK_SHIFT_CONST(0xff) << XUSB_CFG_2_0_BASE_CLASS_SHIFT)
#define XUSB_CFG_2_0_BASE_CLASS_RANGE                      31:24
#define XUSB_CFG_2_0_BASE_CLASS_WOFFSET                    0
#define XUSB_CFG_2_0_BASE_CLASS_DEFAULT                    (_MK_MASK_CONST(0x0000000C)
#define XUSB_CFG_2_0_BASE_CLASS_DEFAULT_MASK               (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_BASE_CLASS_SW_DEFAULT                 (_MK_MASK_CONST(0x0000000C)
#define XUSB_CFG_2_0_BASE_CLASS_SW_DEFAULT_MASK            (_MK_MASK_CONST(0xff)
#define XUSB_CFG_2_0_BASE_CLASS_SBC                        _MK_ENUM_CONST(0x0000000C)
#define XUSB_CFG_3_0                                       _MK_ADDR_CONST(0x0000000C)
#define XUSB_CFG_3_0_SECURE                                0
#define XUSB_CFG_3_0_WORD_COUNT                            1
#define XUSB_CFG_3_0_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_3_0_RESET_MASK                            _MK_MASK_CONST(0xfff8ff)
#define XUSB_CFG_3_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_3_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xfff8ff)
#define XUSB_CFG_3_0_READ_MASK                             _MK_MASK_CONST(0xfff8ff)
#define XUSB_CFG_3_0_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_SHIFT                  _MK_SHIFT_CONST(0)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_FIELD                 (_MK_SHIFT_CONST(0xff) << XUSB_CFG_3_0_CACHE_LINE_SIZE_SHIFT)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_RANGE                 7:0
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_WOFFSET               0
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_DEFAULT_MASK          (_MK_MASK_CONST(0xff)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_SW_DEFAULT_MASK       (_MK_MASK_CONST(0xff)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_0                     _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_32                    _MK_ENUM_CONST(0x00000020)
#define XUSB_CFG_3_0_CACHE_LINE_SIZE_64                    _MK_ENUM_CONST(0x00000040)
#define XUSB_CFG_3_0_LATENCY_TIMER_SHIFT                    _MK_SHIFT_CONST(11)
#define XUSB_CFG_3_0_LATENCY_TIMER_FIELD                   (_MK_SHIFT_CONST(0x1f) << XUSB_CFG_3_0_LATENCY_TIMER_SHIFT)
#define XUSB_CFG_3_0_LATENCY_TIMER_RANGE                   15:11
#define XUSB_CFG_3_0_LATENCY_TIMER_WOFFSET                 0
#define XUSB_CFG_3_0_LATENCY_TIMER_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_LATENCY_TIMER_DEFAULT_MASK            (_MK_MASK_CONST(0x1f)
#define XUSB_CFG_3_0_LATENCY_TIMER_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_LATENCY_TIMER_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1f)
#define XUSB_CFG_3_0_LATENCY_TIMER_0_CLOCKS                _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_3_0_LATENCY_TIMER_8_CLOCKS                _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_3_0_LATENCY_TIMER_240_CLOCKS              _MK_ENUM_CONST(0x0000001E)
#define XUSB_CFG_3_0_LATENCY_TIMER_248_CLOCKS              _MK_ENUM_CONST(0x0000001F)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_SHIFT               _MK_SHIFT_CONST(16)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_FIELD              (_MK_SHIFT_CONST(0x7f) << XUSB_CFG_3_0_HEADER_TYPE_DEVICE_SHIFT)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_RANGE              22:16
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_WOFFSET            0
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_DEFAULT_MASK       (_MK_MASK_CONST(0x7f)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_SW_DEFAULT         (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_SW_DEFAULT_MASK    (_MK_MASK_CONST(0x7f)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_NON_BRIDGE         _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_DEVICE_P2P_BRIDGE         _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_SHIFT                 _MK_SHIFT_CONST(23)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_FIELD                (_MK_SHIFT_CONST(0x1) << XUSB_CFG_3_0_HEADER_TYPE_FUNC_SHIFT)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_RANGE                23:23
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_WOFFSET              0
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_SW_DEFAULT           (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_SW_DEFAULT_MASK      (_MK_MASK_CONST(0x1)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_SINGLE               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_3_0_HEADER_TYPE_FUNC_MULTI                _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_4_0                                       _MK_ADDR_CONST(0x00000010)
#define XUSB_CFG_4_0_SECURE                                0
#define XUSB_CFG_4_0_WORD_COUNT                            1
#define XUSB_CFG_4_0_RESET_VAL                             _MK_MASK_CONST(0xc)
#define XUSB_CFG_4_0_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_4_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0xc)
#define XUSB_CFG_4_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_4_0_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_4_0_WRITE_MASK                            _MK_MASK_CONST(0xffff8000)
#define XUSB_CFG_4_0_SPACE_TYPE_SHIFT                       _MK_SHIFT_CONST(0)
#define XUSB_CFG_4_0_SPACE_TYPE_FIELD                      (_MK_SHIFT_CONST(0x1) << XUSB_CFG_4_0_SPACE_TYPE_SHIFT)
#define XUSB_CFG_4_0_SPACE_TYPE_RANGE                      0:0
#define XUSB_CFG_4_0_SPACE_TYPE_WOFFSET                    0
#define XUSB_CFG_4_0_SPACE_TYPE_DEFAULT                    (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_SPACE_TYPE_DEFAULT_MASK               (_MK_MASK_CONST(0x1)
#define XUSB_CFG_4_0_SPACE_TYPE_SW_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_SPACE_TYPE_SW_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_CFG_4_0_SPACE_TYPE_MEMORY                     _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_4_0_SPACE_TYPE_IO                         _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_4_0_ADDRESS_TYPE_SHIFT                     _MK_SHIFT_CONST(1)
#define XUSB_CFG_4_0_ADDRESS_TYPE_FIELD                    (_MK_SHIFT_CONST(0x3) << XUSB_CFG_4_0_ADDRESS_TYPE_SHIFT)
#define XUSB_CFG_4_0_ADDRESS_TYPE_RANGE                    2:1
#define XUSB_CFG_4_0_ADDRESS_TYPE_WOFFSET                  0
#define XUSB_CFG_4_0_ADDRESS_TYPE_DEFAULT                  (_MK_MASK_CONST(0x00000002)
#define XUSB_CFG_4_0_ADDRESS_TYPE_DEFAULT_MASK             (_MK_MASK_CONST(0x3)
#define XUSB_CFG_4_0_ADDRESS_TYPE_SW_DEFAULT               (_MK_MASK_CONST(0x00000002)
#define XUSB_CFG_4_0_ADDRESS_TYPE_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x3)
#define XUSB_CFG_4_0_ADDRESS_TYPE_32_BIT                   _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_4_0_ADDRESS_TYPE_64_BIT                   _MK_ENUM_CONST(0x00000002)
#define XUSB_CFG_4_0_PREFETCHABLE_SHIFT                     _MK_SHIFT_CONST(3)
#define XUSB_CFG_4_0_PREFETCHABLE_FIELD                    (_MK_SHIFT_CONST(0x1) << XUSB_CFG_4_0_PREFETCHABLE_SHIFT)
#define XUSB_CFG_4_0_PREFETCHABLE_RANGE                    3:3
#define XUSB_CFG_4_0_PREFETCHABLE_WOFFSET                  0
#define XUSB_CFG_4_0_PREFETCHABLE_DEFAULT                  (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_4_0_PREFETCHABLE_DEFAULT_MASK             (_MK_MASK_CONST(0x1)
#define XUSB_CFG_4_0_PREFETCHABLE_SW_DEFAULT               (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_4_0_PREFETCHABLE_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x1)
#define XUSB_CFG_4_0_PREFETCHABLE_NOT                      _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_4_0_PREFETCHABLE_MERGABLE                 _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_SHIFT                    _MK_SHIFT_CONST(4)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_FIELD                   (_MK_SHIFT_CONST(0x7ff) << XUSB_CFG_4_0_BAR_SIZE_32KB_SHIFT)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_RANGE                   14:4
#define XUSB_CFG_4_0_BAR_SIZE_32KB_WOFFSET                 0
#define XUSB_CFG_4_0_BAR_SIZE_32KB_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_DEFAULT_MASK            (_MK_MASK_CONST(0x7ff)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x7ff)
#define XUSB_CFG_4_0_BAR_SIZE_32KB_RSVD                    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_4_0_BASE_ADDRESS_SHIFT                     _MK_SHIFT_CONST(15)
#define XUSB_CFG_4_0_BASE_ADDRESS_FIELD                    (_MK_SHIFT_CONST(0x1ffff) << XUSB_CFG_4_0_BASE_ADDRESS_SHIFT)
#define XUSB_CFG_4_0_BASE_ADDRESS_RANGE                    31:15
#define XUSB_CFG_4_0_BASE_ADDRESS_WOFFSET                  0
#define XUSB_CFG_4_0_BASE_ADDRESS_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_BASE_ADDRESS_DEFAULT_MASK             (_MK_MASK_CONST(0x1ffff)
#define XUSB_CFG_4_0_BASE_ADDRESS_SW_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_4_0_BASE_ADDRESS_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x1ffff)
#define XUSB_CFG_4_0_BASE_ADDRESS__NOPRDCHK                _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_5_0                                       _MK_ADDR_CONST(0x00000014)
#define XUSB_CFG_5_0_SECURE                                0
#define XUSB_CFG_5_0_WORD_COUNT                            1
#define XUSB_CFG_5_0_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_5_0_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_5_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_WRITE_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_SHIFT                   _MK_SHIFT_CONST(0)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_FIELD                  (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_5_0_BASE_ADDRESSHI_SHIFT)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_RANGE                  31:0
#define XUSB_CFG_5_0_BASE_ADDRESSHI_WOFFSET                0
#define XUSB_CFG_5_0_BASE_ADDRESSHI_DEFAULT                (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_DEFAULT_MASK           (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_SW_DEFAULT             (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_5_0_BASE_ADDRESSHI_SW_DEFAULT_MASK        (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_5_0_BASE_ADDRESSHI__NOPRDCHK              _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_6_0                                       _MK_ADDR_CONST(0x18)
#define XUSB_CFG_6_0_SECURE                                0
#define XUSB_CFG_6_0_WORD_COUNT                            1
#define XUSB_CFG_6_0_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_0_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_0_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_0_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_0_RSVD_SHIFT                             _MK_SHIFT_CONST(0)
#define XUSB_CFG_6_0_RSVD_FIELD                            (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_6_0_RSVD_SHIFT)
#define XUSB_CFG_6_0_RSVD_RANGE                            31:0
#define XUSB_CFG_6_0_RSVD_WOFFSET                          0
#define XUSB_CFG_6_0_RSVD_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_0_RSVD_DEFAULT_MASK                     (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_0_RSVD_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_0_RSVD_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_0_RSVD_00                               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_6_1                                       _MK_ADDR_CONST(0x1c)
#define XUSB_CFG_6_1_SECURE                                0
#define XUSB_CFG_6_1_WORD_COUNT                            1
#define XUSB_CFG_6_1_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_1_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_1_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_1_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_1_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_1_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_1_RSVD_SHIFT                             _MK_SHIFT_CONST(0)
#define XUSB_CFG_6_1_RSVD_FIELD                            (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_6_1_RSVD_SHIFT)
#define XUSB_CFG_6_1_RSVD_RANGE                            31:0
#define XUSB_CFG_6_1_RSVD_WOFFSET                          0
#define XUSB_CFG_6_1_RSVD_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_1_RSVD_DEFAULT_MASK                     (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_1_RSVD_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_1_RSVD_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_1_RSVD_00                               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_6_2                                       _MK_ADDR_CONST(0x20)
#define XUSB_CFG_6_2_SECURE                                0
#define XUSB_CFG_6_2_WORD_COUNT                            1
#define XUSB_CFG_6_2_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_2_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_2_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_2_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_2_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_2_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_2_RSVD_SHIFT                             _MK_SHIFT_CONST(0)
#define XUSB_CFG_6_2_RSVD_FIELD                            (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_6_2_RSVD_SHIFT)
#define XUSB_CFG_6_2_RSVD_RANGE                            31:0
#define XUSB_CFG_6_2_RSVD_WOFFSET                          0
#define XUSB_CFG_6_2_RSVD_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_2_RSVD_DEFAULT_MASK                     (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_2_RSVD_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_2_RSVD_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_2_RSVD_00                               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_6_3                                       _MK_ADDR_CONST(0x24)
#define XUSB_CFG_6_3_SECURE                                0
#define XUSB_CFG_6_3_WORD_COUNT                            1
#define XUSB_CFG_6_3_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_3_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_3_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_3_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_3_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_3_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_3_RSVD_SHIFT                             _MK_SHIFT_CONST(0)
#define XUSB_CFG_6_3_RSVD_FIELD                            (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_6_3_RSVD_SHIFT)
#define XUSB_CFG_6_3_RSVD_RANGE                            31:0
#define XUSB_CFG_6_3_RSVD_WOFFSET                          0
#define XUSB_CFG_6_3_RSVD_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_3_RSVD_DEFAULT_MASK                     (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_3_RSVD_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_3_RSVD_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_3_RSVD_00                               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_6_4                                       _MK_ADDR_CONST(0x28)
#define XUSB_CFG_6_4_SECURE                                0
#define XUSB_CFG_6_4_WORD_COUNT                            1
#define XUSB_CFG_6_4_RESET_VAL                             _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_4_RESET_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_4_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_4_SW_DEFAULT_MASK                       _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_4_READ_MASK                             _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_4_WRITE_MASK                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_6_4_RSVD_SHIFT                             _MK_SHIFT_CONST(0)
#define XUSB_CFG_6_4_RSVD_FIELD                            (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_6_4_RSVD_SHIFT)
#define XUSB_CFG_6_4_RSVD_RANGE                            31:0
#define XUSB_CFG_6_4_RSVD_WOFFSET                          0
#define XUSB_CFG_6_4_RSVD_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_4_RSVD_DEFAULT_MASK                     (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_4_RSVD_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_6_4_RSVD_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_6_4_RSVD_00                               _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_11_0                                      _MK_ADDR_CONST(0x0000002C)
#define XUSB_CFG_11_0_SECURE                               0
#define XUSB_CFG_11_0_WORD_COUNT                           1
#define XUSB_CFG_11_0_RESET_VAL                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_11_0_RESET_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_11_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define XUSB_CFG_11_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_11_0_READ_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_11_0_WRITE_MASK                           _MK_MASK_CONST(0x0)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_SHIFT             _MK_SHIFT_CONST(0)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_FIELD            (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_SHIFT)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_RANGE            15:0
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_WOFFSET          0
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_DEFAULT          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_DEFAULT_MASK     (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_SW_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_SW_DEFAULT_MASK  (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_11_0_SUBSYSTEM_VENDOR_ID_NONE             _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_SHIFT                    _MK_SHIFT_CONST(16)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_FIELD                   (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_11_0_SUBSYSTEM_ID_SHIFT)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_RANGE                   31:16
#define XUSB_CFG_11_0_SUBSYSTEM_ID_WOFFSET                 0
#define XUSB_CFG_11_0_SUBSYSTEM_ID_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_DEFAULT_MASK            (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_SW_DEFAULT_MASK         (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_11_0_SUBSYSTEM_ID_NONE                    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_12_0                                      _MK_ADDR_CONST(0x00000030)
#define XUSB_CFG_12_0_SECURE                               0
#define XUSB_CFG_12_0_WORD_COUNT                           1
#define XUSB_CFG_12_0_RESET_VAL                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_12_0_RESET_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_12_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define XUSB_CFG_12_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_12_0_READ_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_12_0_WRITE_MASK                           _MK_MASK_CONST(0x0)
#define XUSB_CFG_12_0_RESERVED_SHIFT                        _MK_SHIFT_CONST(0)
#define XUSB_CFG_12_0_RESERVED_FIELD                       (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_12_0_RESERVED_SHIFT)
#define XUSB_CFG_12_0_RESERVED_RANGE                       31:0
#define XUSB_CFG_12_0_RESERVED_WOFFSET                     0
#define XUSB_CFG_12_0_RESERVED_DEFAULT                     (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_12_0_RESERVED_DEFAULT_MASK                (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_12_0_RESERVED_SW_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_12_0_RESERVED_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_12_0_RESERVED_0                           _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_13_0                                      _MK_ADDR_CONST(0x00000034)
#define XUSB_CFG_13_0_SECURE                               0
#define XUSB_CFG_13_0_WORD_COUNT                           1
#define XUSB_CFG_13_0_RESET_VAL                            _MK_MASK_CONST(0x44)
#define XUSB_CFG_13_0_RESET_MASK                           _MK_MASK_CONST(0xff)
#define XUSB_CFG_13_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x44)
#define XUSB_CFG_13_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xff)
#define XUSB_CFG_13_0_READ_MASK                            _MK_MASK_CONST(0xff)
#define XUSB_CFG_13_0_WRITE_MASK                           _MK_MASK_CONST(0x0)
#define XUSB_CFG_13_0_CAP_PTR_SHIFT                         _MK_SHIFT_CONST(0)
#define XUSB_CFG_13_0_CAP_PTR_FIELD                        (_MK_SHIFT_CONST(0xff) << XUSB_CFG_13_0_CAP_PTR_SHIFT)
#define XUSB_CFG_13_0_CAP_PTR_RANGE                        7:0
#define XUSB_CFG_13_0_CAP_PTR_WOFFSET                      0
#define XUSB_CFG_13_0_CAP_PTR_DEFAULT                      (_MK_MASK_CONST(0x00000044)
#define XUSB_CFG_13_0_CAP_PTR_DEFAULT_MASK                 (_MK_MASK_CONST(0xff)
#define XUSB_CFG_13_0_CAP_PTR_SW_DEFAULT                   (_MK_MASK_CONST(0x00000044)
#define XUSB_CFG_13_0_CAP_PTR_SW_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_CFG_13_0_CAP_PTR_PMCAP                        _MK_ENUM_CONST(0x00000044)
#define XUSB_CFG_13_0_CAP_PTR_MSI                          _MK_ENUM_CONST(0x000000C0)
#define XUSB_CFG_13_0_CAP_PTR_MSIX                         _MK_ENUM_CONST(0x00000070)
#define XUSB_CFG_13_0_CAP_PTR_MMAP                         _MK_ENUM_CONST(0x000000DC)
#define XUSB_CFG_14_0                                      _MK_ADDR_CONST(0x00000038)
#define XUSB_CFG_14_0_SECURE                               0
#define XUSB_CFG_14_0_WORD_COUNT                           1
#define XUSB_CFG_14_0_RESET_VAL                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_14_0_RESET_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_14_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define XUSB_CFG_14_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_14_0_READ_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_14_0_WRITE_MASK                           _MK_MASK_CONST(0x0)
#define XUSB_CFG_14_0_RESERVED_SHIFT                        _MK_SHIFT_CONST(0)
#define XUSB_CFG_14_0_RESERVED_FIELD                       (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_14_0_RESERVED_SHIFT)
#define XUSB_CFG_14_0_RESERVED_RANGE                       31:0
#define XUSB_CFG_14_0_RESERVED_WOFFSET                     0
#define XUSB_CFG_14_0_RESERVED_DEFAULT                     (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_14_0_RESERVED_DEFAULT_MASK                (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_14_0_RESERVED_SW_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_14_0_RESERVED_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_14_0_RESERVED_0                           _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_15_0                                      _MK_ADDR_CONST(0x0000003C)
#define XUSB_CFG_15_0_SECURE                               0
#define XUSB_CFG_15_0_WORD_COUNT                           1
#define XUSB_CFG_15_0_RESET_VAL                            _MK_MASK_CONST(0x100)
#define XUSB_CFG_15_0_RESET_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_15_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x100)
#define XUSB_CFG_15_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_15_0_READ_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_15_0_WRITE_MASK                           _MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_INTR_LINE_SHIFT                       _MK_SHIFT_CONST(0)
#define XUSB_CFG_15_0_INTR_LINE_FIELD                      (_MK_SHIFT_CONST(0xff) << XUSB_CFG_15_0_INTR_LINE_SHIFT)
#define XUSB_CFG_15_0_INTR_LINE_RANGE                      7:0
#define XUSB_CFG_15_0_INTR_LINE_WOFFSET                    0
#define XUSB_CFG_15_0_INTR_LINE_DEFAULT                    (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_INTR_LINE_DEFAULT_MASK               (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_INTR_LINE_SW_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_INTR_LINE_SW_DEFAULT_MASK            (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_INTR_LINE__NOPRDCHK                  _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_15_0_INTR_LINE_IRQ0                       _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_15_0_INTR_LINE_IRQ1                       _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_15_0_INTR_LINE_IRQ15                      _MK_ENUM_CONST(0x0000000F)
#define XUSB_CFG_15_0_INTR_LINE_UNKNOWN                    _MK_ENUM_CONST(0x000000FF)
#define XUSB_CFG_15_0_INTR_PIN_SHIFT                        _MK_SHIFT_CONST(8)
#define XUSB_CFG_15_0_INTR_PIN_FIELD                       (_MK_SHIFT_CONST(0xff) << XUSB_CFG_15_0_INTR_PIN_SHIFT)
#define XUSB_CFG_15_0_INTR_PIN_RANGE                       15:8
#define XUSB_CFG_15_0_INTR_PIN_WOFFSET                     0
#define XUSB_CFG_15_0_INTR_PIN_DEFAULT                     (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_15_0_INTR_PIN_DEFAULT_MASK                (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_INTR_PIN_SW_DEFAULT                  (_MK_MASK_CONST(0x00000001)
#define XUSB_CFG_15_0_INTR_PIN_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_INTR_PIN_NONE                        _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_15_0_INTR_PIN_INTA                        _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_15_0_INTR_PIN_INTB                        _MK_ENUM_CONST(0x00000002)
#define XUSB_CFG_15_0_INTR_PIN_INTC                        _MK_ENUM_CONST(0x00000003)
#define XUSB_CFG_15_0_INTR_PIN_INTD                        _MK_ENUM_CONST(0x00000004)
#define XUSB_CFG_15_0_MIN_GNT_SHIFT                         _MK_SHIFT_CONST(16)
#define XUSB_CFG_15_0_MIN_GNT_FIELD                        (_MK_SHIFT_CONST(0xff) << XUSB_CFG_15_0_MIN_GNT_SHIFT)
#define XUSB_CFG_15_0_MIN_GNT_RANGE                        23:16
#define XUSB_CFG_15_0_MIN_GNT_WOFFSET                      0
#define XUSB_CFG_15_0_MIN_GNT_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_MIN_GNT_DEFAULT_MASK                 (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_MIN_GNT_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_MIN_GNT_SW_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_MIN_GNT_NO_REQUIREMENTS              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_15_0_MIN_GNT_240NS                        _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_15_0_MAX_LAT_SHIFT                         _MK_SHIFT_CONST(24)
#define XUSB_CFG_15_0_MAX_LAT_FIELD                        (_MK_SHIFT_CONST(0xff) << XUSB_CFG_15_0_MAX_LAT_SHIFT)
#define XUSB_CFG_15_0_MAX_LAT_RANGE                        31:24
#define XUSB_CFG_15_0_MAX_LAT_WOFFSET                      0
#define XUSB_CFG_15_0_MAX_LAT_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_MAX_LAT_DEFAULT_MASK                 (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_MAX_LAT_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_15_0_MAX_LAT_SW_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_CFG_15_0_MAX_LAT_NO_REQUIREMENTS              _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_15_0_MAX_LAT_5US                          _MK_ENUM_CONST(0x00000014)
#define XUSB_CFG_15_0_MAX_LAT_20US                         _MK_ENUM_CONST(0x00000050)
#define XUSB_CFG_16_0                                      _MK_ADDR_CONST(0x00000040)
#define XUSB_CFG_16_0_SECURE                               0
#define XUSB_CFG_16_0_WORD_COUNT                           1
#define XUSB_CFG_16_0_RESET_VAL                            _MK_MASK_CONST(0x0)
#define XUSB_CFG_16_0_RESET_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_16_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define XUSB_CFG_16_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_16_0_READ_MASK                            _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_16_0_WRITE_MASK                           _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_SHIFT             _MK_SHIFT_CONST(0)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_FIELD            (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_SHIFT)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_RANGE            15:0
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_WOFFSET          0
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_DEFAULT          (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_DEFAULT_MASK     (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_SW_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_SW_DEFAULT_MASK  (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID__NOPRDCHK        _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_16_0_SUBSYSTEM_VENDOR_ID_NONE             _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_SHIFT                    _MK_SHIFT_CONST(16)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_FIELD                   (_MK_SHIFT_CONST(0xffff) << XUSB_CFG_16_0_SUBSYSTEM_ID_SHIFT)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_RANGE                   31:16
#define XUSB_CFG_16_0_SUBSYSTEM_ID_WOFFSET                 0
#define XUSB_CFG_16_0_SUBSYSTEM_ID_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_DEFAULT_MASK            (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_SW_DEFAULT_MASK         (_MK_MASK_CONST(0xffff)
#define XUSB_CFG_16_0_SUBSYSTEM_ID__NOPRDCHK               _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_16_0_SUBSYSTEM_ID_NONE                    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_EMU_RSVD_0                                _MK_ADDR_CONST(0x80)
#define XUSB_CFG_EMU_RSVD_0_SECURE                         0
#define XUSB_CFG_EMU_RSVD_0_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_0_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_0_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_0_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_0_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_0_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_0_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_0_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_0_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_0_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_0_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_1                                _MK_ADDR_CONST(0x84)
#define XUSB_CFG_EMU_RSVD_1_SECURE                         0
#define XUSB_CFG_EMU_RSVD_1_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_1_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_1_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_1_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_1_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_1_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_1_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_1_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_1_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_1_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_1_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_1_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_2                                _MK_ADDR_CONST(0x88)
#define XUSB_CFG_EMU_RSVD_2_SECURE                         0
#define XUSB_CFG_EMU_RSVD_2_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_2_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_2_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_2_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_2_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_2_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_2_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_2_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_2_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_2_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_2_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_2_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_3                                _MK_ADDR_CONST(0x8c)
#define XUSB_CFG_EMU_RSVD_3_SECURE                         0
#define XUSB_CFG_EMU_RSVD_3_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_3_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_3_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_3_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_3_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_3_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_3_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_3_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_3_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_3_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_3_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_3_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_4                                _MK_ADDR_CONST(0x90)
#define XUSB_CFG_EMU_RSVD_4_SECURE                         0
#define XUSB_CFG_EMU_RSVD_4_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_4_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_4_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_4_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_4_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_4_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_4_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_4_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_4_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_4_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_4_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_4_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_5                                _MK_ADDR_CONST(0x94)
#define XUSB_CFG_EMU_RSVD_5_SECURE                         0
#define XUSB_CFG_EMU_RSVD_5_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_5_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_5_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_5_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_5_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_5_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_5_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_5_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_5_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_5_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_5_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_5_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_6                                _MK_ADDR_CONST(0x98)
#define XUSB_CFG_EMU_RSVD_6_SECURE                         0
#define XUSB_CFG_EMU_RSVD_6_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_6_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_6_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_6_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_6_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_6_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_6_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_6_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_6_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_6_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_6_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_6_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_7                                _MK_ADDR_CONST(0x9c)
#define XUSB_CFG_EMU_RSVD_7_SECURE                         0
#define XUSB_CFG_EMU_RSVD_7_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_7_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_7_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_7_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_7_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_7_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_7_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_7_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_7_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_7_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_7_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_7_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_8                                _MK_ADDR_CONST(0xa0)
#define XUSB_CFG_EMU_RSVD_8_SECURE                         0
#define XUSB_CFG_EMU_RSVD_8_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_8_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_8_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_8_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_8_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_8_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_8_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_8_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_8_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_8_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_8_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_8_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_9                                _MK_ADDR_CONST(0xa4)
#define XUSB_CFG_EMU_RSVD_9_SECURE                         0
#define XUSB_CFG_EMU_RSVD_9_WORD_COUNT                     1
#define XUSB_CFG_EMU_RSVD_9_RESET_VAL                      _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_9_RESET_MASK                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_9_SW_DEFAULT_VAL                 _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_9_SW_DEFAULT_MASK                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_9_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_9_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_9_FIELD_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_9_FIELD_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_9_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_9_FIELD_RANGE                    31:0
#define XUSB_CFG_EMU_RSVD_9_FIELD_WOFFSET                  0
#define XUSB_CFG_EMU_RSVD_10                               _MK_ADDR_CONST(0xa8)
#define XUSB_CFG_EMU_RSVD_10_SECURE                        0
#define XUSB_CFG_EMU_RSVD_10_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_10_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_10_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_10_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_10_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_10_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_10_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_10_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_10_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_10_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_10_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_10_FIELD_WOFFSET                 0
#define XUSB_CFG_EMU_RSVD_11                               _MK_ADDR_CONST(0xac)
#define XUSB_CFG_EMU_RSVD_11_SECURE                        0
#define XUSB_CFG_EMU_RSVD_11_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_11_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_11_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_11_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_11_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_11_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_11_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_11_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_11_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_11_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_11_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_11_FIELD_WOFFSET                 0
#define XUSB_CFG_EMU_RSVD_12                               _MK_ADDR_CONST(0xb0)
#define XUSB_CFG_EMU_RSVD_12_SECURE                        0
#define XUSB_CFG_EMU_RSVD_12_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_12_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_12_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_12_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_12_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_12_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_12_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_12_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_12_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_12_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_12_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_12_FIELD_WOFFSET                 0
#define XUSB_CFG_EMU_RSVD_13                               _MK_ADDR_CONST(0xb4)
#define XUSB_CFG_EMU_RSVD_13_SECURE                        0
#define XUSB_CFG_EMU_RSVD_13_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_13_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_13_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_13_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_13_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_13_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_13_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_13_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_13_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_13_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_13_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_13_FIELD_WOFFSET                 0
#define XUSB_CFG_EMU_RSVD_14                               _MK_ADDR_CONST(0xb8)
#define XUSB_CFG_EMU_RSVD_14_SECURE                        0
#define XUSB_CFG_EMU_RSVD_14_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_14_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_14_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_14_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_14_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_14_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_14_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_14_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_14_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_14_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_14_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_14_FIELD_WOFFSET                 0
#define XUSB_CFG_EMU_RSVD_15                               _MK_ADDR_CONST(0xbc)
#define XUSB_CFG_EMU_RSVD_15_SECURE                        0
#define XUSB_CFG_EMU_RSVD_15_WORD_COUNT                    1
#define XUSB_CFG_EMU_RSVD_15_RESET_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_15_RESET_MASK                    _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_15_SW_DEFAULT_VAL                _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_15_SW_DEFAULT_MASK               _MK_MASK_CONST(0x0)
#define XUSB_CFG_EMU_RSVD_15_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_15_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define XUSB_CFG_EMU_RSVD_15_FIELD_SHIFT                    _MK_SHIFT_CONST(0)
#define XUSB_CFG_EMU_RSVD_15_FIELD_FIELD                   (_MK_SHIFT_CONST(0xffffffff) << XUSB_CFG_EMU_RSVD_15_FIELD_SHIFT)
#define XUSB_CFG_EMU_RSVD_15_FIELD_RANGE                   31:0
#define XUSB_CFG_EMU_RSVD_15_FIELD_WOFFSET                 0
#define XUSB_MSI_CTRL_0                                    _MK_ADDR_CONST(0x000000C0)
#define XUSB_MSI_CTRL_0_SECURE                             0
#define XUSB_MSI_CTRL_0_WORD_COUNT                         1
#define XUSB_MSI_CTRL_0_RESET_VAL                          _MK_MASK_CONST(0x80e005)
#define XUSB_MSI_CTRL_0_RESET_MASK                         _MK_MASK_CONST(0x1ffffff)
#define XUSB_MSI_CTRL_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x80e005)
#define XUSB_MSI_CTRL_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x1ffffff)
#define XUSB_MSI_CTRL_0_READ_MASK                          _MK_MASK_CONST(0x1ffffff)
#define XUSB_MSI_CTRL_0_WRITE_MASK                         _MK_MASK_CONST(0x710000)
#define XUSB_MSI_CTRL_0_CAP_ID_SHIFT                        _MK_SHIFT_CONST(0)
#define XUSB_MSI_CTRL_0_CAP_ID_FIELD                       (_MK_SHIFT_CONST(0xff) << XUSB_MSI_CTRL_0_CAP_ID_SHIFT)
#define XUSB_MSI_CTRL_0_CAP_ID_RANGE                       7:0
#define XUSB_MSI_CTRL_0_CAP_ID_WOFFSET                     0
#define XUSB_MSI_CTRL_0_CAP_ID_DEFAULT                     (_MK_MASK_CONST(0x00000005)
#define XUSB_MSI_CTRL_0_CAP_ID_DEFAULT_MASK                (_MK_MASK_CONST(0xff)
#define XUSB_MSI_CTRL_0_CAP_ID_SW_DEFAULT                  (_MK_MASK_CONST(0x00000005)
#define XUSB_MSI_CTRL_0_CAP_ID_SW_DEFAULT_MASK             (_MK_MASK_CONST(0xff)
#define XUSB_MSI_CTRL_0_CAP_ID_MSI                         _MK_ENUM_CONST(0x00000005)
#define XUSB_MSI_CTRL_0_NEXT_PTR_SHIFT                      _MK_SHIFT_CONST(8)
#define XUSB_MSI_CTRL_0_NEXT_PTR_FIELD                     (_MK_SHIFT_CONST(0xff) << XUSB_MSI_CTRL_0_NEXT_PTR_SHIFT)
#define XUSB_MSI_CTRL_0_NEXT_PTR_RANGE                     15:8
#define XUSB_MSI_CTRL_0_NEXT_PTR_WOFFSET                   0
#define XUSB_MSI_CTRL_0_NEXT_PTR_DEFAULT                   (_MK_MASK_CONST(0x000000E0)
#define XUSB_MSI_CTRL_0_NEXT_PTR_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_MSI_CTRL_0_NEXT_PTR_SW_DEFAULT                (_MK_MASK_CONST(0x000000E0)
#define XUSB_MSI_CTRL_0_NEXT_PTR_SW_DEFAULT_MASK           (_MK_MASK_CONST(0xff)
#define XUSB_MSI_CTRL_0_NEXT_PTR_MSIX                      _MK_ENUM_CONST(0x00000070)
#define XUSB_MSI_CTRL_0_NEXT_PTR_MMAP                      _MK_ENUM_CONST(0x000000DC)
#define XUSB_MSI_CTRL_0_NEXT_PTR_PMCAP                     _MK_ENUM_CONST(0x00000044)
#define XUSB_MSI_CTRL_0_NEXT_PTR_NULL                      _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_NEXT_PTR_MAILBOX                   _MK_ENUM_CONST(0x000000E0)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_SHIFT                    _MK_SHIFT_CONST(16)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_FIELD                   (_MK_SHIFT_CONST(0x1) << XUSB_MSI_CTRL_0_MSI_ENABLE_SHIFT)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_RANGE                   16:16
#define XUSB_MSI_CTRL_0_MSI_ENABLE_WOFFSET                 0
#define XUSB_MSI_CTRL_0_MSI_ENABLE_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_DEFAULT_MASK            (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_SW_DEFAULT              (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_SW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_OFF                     _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MSI_ENABLE_ON                      _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_SHIFT                  _MK_SHIFT_CONST(17)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_FIELD                 (_MK_SHIFT_CONST(0x7) << XUSB_MSI_CTRL_0_MULT_MSG_CAP_SHIFT)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_RANGE                 19:17
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_WOFFSET               0
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_DEFAULT_MASK          (_MK_MASK_CONST(0x7)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_SW_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_SW_DEFAULT_MASK       (_MK_MASK_CONST(0x7)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_1                     _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_2                     _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_4                     _MK_ENUM_CONST(0x00000002)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_8                     _MK_ENUM_CONST(0x00000003)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_16                    _MK_ENUM_CONST(0x00000004)
#define XUSB_MSI_CTRL_0_MULT_MSG_CAP_32                    _MK_ENUM_CONST(0x00000005)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_SHIFT               _MK_SHIFT_CONST(20)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_FIELD              (_MK_SHIFT_CONST(0x7) << XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_SHIFT)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_RANGE              22:20
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_WOFFSET            0
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_DEFAULT_MASK       (_MK_MASK_CONST(0x7)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_SW_DEFAULT         (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_SW_DEFAULT_MASK    (_MK_MASK_CONST(0x7)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_1                  _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_2                  _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_4                  _MK_ENUM_CONST(0x00000002)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_8                  _MK_ENUM_CONST(0x00000003)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_16                 _MK_ENUM_CONST(0x00000004)
#define XUSB_MSI_CTRL_0_MULT_MSG_ENABLE_32                 _MK_ENUM_CONST(0x00000005)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_SHIFT                   _MK_SHIFT_CONST(23)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_FIELD                  (_MK_SHIFT_CONST(0x1) << XUSB_MSI_CTRL_0_64_ADDR_CAP_SHIFT)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_RANGE                  23:23
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_WOFFSET                0
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_DEFAULT                (_MK_MASK_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_DEFAULT_MASK           (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_SW_DEFAULT             (_MK_MASK_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_SW_DEFAULT_MASK        (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_DIS                    _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_64_ADDR_CAP_EN                     _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_SHIFT               _MK_SHIFT_CONST(24)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_FIELD              (_MK_SHIFT_CONST(0x1) << XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_SHIFT)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_RANGE              24:24
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_WOFFSET            0
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_DEFAULT            (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_DEFAULT_MASK       (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_SW_DEFAULT         (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_SW_DEFAULT_MASK    (_MK_MASK_CONST(0x1)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_DIS                _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_CTRL_0_VECTOR_MASK_CAP_EN                 _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_ADDR1_0                                   _MK_ADDR_CONST(0x000000C4)
#define XUSB_MSI_ADDR1_0_SECURE                            0
#define XUSB_MSI_ADDR1_0_WORD_COUNT                        1
#define XUSB_MSI_ADDR1_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define XUSB_MSI_ADDR1_0_RESET_MASK                        _MK_MASK_CONST(0xfffffffc)
#define XUSB_MSI_ADDR1_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define XUSB_MSI_ADDR1_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0xfffffffc)
#define XUSB_MSI_ADDR1_0_READ_MASK                         _MK_MASK_CONST(0xfffffffc)
#define XUSB_MSI_ADDR1_0_WRITE_MASK                        _MK_MASK_CONST(0xfffffffc)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_SHIFT                     _MK_SHIFT_CONST(2)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_FIELD                    (_MK_SHIFT_CONST(0x3fffffff) << XUSB_MSI_ADDR1_0_MSG_ADDR_SHIFT)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_RANGE                    31:2
#define XUSB_MSI_ADDR1_0_MSG_ADDR_WOFFSET                  0
#define XUSB_MSI_ADDR1_0_MSG_ADDR_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_DEFAULT_MASK             (_MK_MASK_CONST(0x3fffffff)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_SW_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_ADDR1_0_MSG_ADDR_SW_DEFAULT_MASK          (_MK_MASK_CONST(0x3fffffff)
#define XUSB_MSI_ADDR2_0                                   _MK_ADDR_CONST(0x000000C8)
#define XUSB_MSI_ADDR2_0_SECURE                            0
#define XUSB_MSI_ADDR2_0_WORD_COUNT                        1
#define XUSB_MSI_ADDR2_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define XUSB_MSI_ADDR2_0_RESET_MASK                        _MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_ADDR2_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define XUSB_MSI_ADDR2_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_ADDR2_0_READ_MASK                         _MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_ADDR2_0_WRITE_MASK                        _MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_SHIFT                     _MK_SHIFT_CONST(0)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_FIELD                    (_MK_SHIFT_CONST(0xffffffff) << XUSB_MSI_ADDR2_0_MSG_ADDR_SHIFT)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_RANGE                    31:0
#define XUSB_MSI_ADDR2_0_MSG_ADDR_WOFFSET                  0
#define XUSB_MSI_ADDR2_0_MSG_ADDR_DEFAULT                  (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_DEFAULT_MASK             (_MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_SW_DEFAULT               (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_ADDR2_0_MSG_ADDR_SW_DEFAULT_MASK          (_MK_MASK_CONST(0xffffffff)
#define XUSB_MSI_DATA_0                                    _MK_ADDR_CONST(0x000000CC)
#define XUSB_MSI_DATA_0_SECURE                             0
#define XUSB_MSI_DATA_0_WORD_COUNT                         1
#define XUSB_MSI_DATA_0_RESET_VAL                          _MK_MASK_CONST(0x0)
#define XUSB_MSI_DATA_0_RESET_MASK                         _MK_MASK_CONST(0xffff)
#define XUSB_MSI_DATA_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_MSI_DATA_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0xffff)
#define XUSB_MSI_DATA_0_READ_MASK                          _MK_MASK_CONST(0xffff)
#define XUSB_MSI_DATA_0_WRITE_MASK                         _MK_MASK_CONST(0xffff)
#define XUSB_MSI_DATA_0_MSG_DATA_SHIFT                      _MK_SHIFT_CONST(0)
#define XUSB_MSI_DATA_0_MSG_DATA_FIELD                     (_MK_SHIFT_CONST(0xffff) << XUSB_MSI_DATA_0_MSG_DATA_SHIFT)
#define XUSB_MSI_DATA_0_MSG_DATA_RANGE                     15:0
#define XUSB_MSI_DATA_0_MSG_DATA_WOFFSET                   0
#define XUSB_MSI_DATA_0_MSG_DATA_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_DATA_0_MSG_DATA_DEFAULT_MASK              (_MK_MASK_CONST(0xffff)
#define XUSB_MSI_DATA_0_MSG_DATA_SW_DEFAULT                (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_DATA_0_MSG_DATA_SW_DEFAULT_MASK           (_MK_MASK_CONST(0xffff)
#define XUSB_MSI_MASK_0                                    _MK_ADDR_CONST(0x000000D0)
#define XUSB_MSI_MASK_0_SECURE                             0
#define XUSB_MSI_MASK_0_WORD_COUNT                         1
#define XUSB_MSI_MASK_0_RESET_VAL                          _MK_MASK_CONST(0x0)
#define XUSB_MSI_MASK_0_RESET_MASK                         _MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_MSI_MASK_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_READ_MASK                          _MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_WRITE_MASK                         _MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_BIT_SHIFT                           _MK_SHIFT_CONST(0)
#define XUSB_MSI_MASK_0_BIT_FIELD                          (_MK_SHIFT_CONST(0x1) << XUSB_MSI_MASK_0_BIT_SHIFT)
#define XUSB_MSI_MASK_0_BIT_RANGE                          0:0
#define XUSB_MSI_MASK_0_BIT_WOFFSET                        0
#define XUSB_MSI_MASK_0_BIT_DEFAULT                        (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MASK_0_BIT_DEFAULT_MASK                   (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_BIT_SW_DEFAULT                     (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MASK_0_BIT_SW_DEFAULT_MASK                (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MASK_0_BIT__NOPRDCHK                      _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_PEND_0                                    _MK_ADDR_CONST(0x000000D4)
#define XUSB_MSI_PEND_0_SECURE                             0
#define XUSB_MSI_PEND_0_WORD_COUNT                         1
#define XUSB_MSI_PEND_0_RESET_VAL                          _MK_MASK_CONST(0x0)
#define XUSB_MSI_PEND_0_RESET_MASK                         _MK_MASK_CONST(0x1)
#define XUSB_MSI_PEND_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define XUSB_MSI_PEND_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define XUSB_MSI_PEND_0_READ_MASK                          _MK_MASK_CONST(0x1)
#define XUSB_MSI_PEND_0_WRITE_MASK                         _MK_MASK_CONST(0x0)
#define XUSB_MSI_PEND_0_BIT_SHIFT                           _MK_SHIFT_CONST(0)
#define XUSB_MSI_PEND_0_BIT_FIELD                          (_MK_SHIFT_CONST(0x1) << XUSB_MSI_PEND_0_BIT_SHIFT)
#define XUSB_MSI_PEND_0_BIT_RANGE                          0:0
#define XUSB_MSI_PEND_0_BIT_WOFFSET                        0
#define XUSB_MSI_PEND_0_BIT_DEFAULT                        (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_PEND_0_BIT_DEFAULT_MASK                   (_MK_MASK_CONST(0x1)
#define XUSB_MSI_PEND_0_BIT_SW_DEFAULT                     (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_PEND_0_BIT_SW_DEFAULT_MASK                (_MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0                                   _MK_ADDR_CONST(0x000000D8)
#define XUSB_MSI_QUEUE_0_SECURE                            0
#define XUSB_MSI_QUEUE_0_WORD_COUNT                        1
#define XUSB_MSI_QUEUE_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define XUSB_MSI_QUEUE_0_RESET_MASK                        _MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define XUSB_MSI_QUEUE_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_READ_MASK                         _MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_WRITE_MASK                        _MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_BITS_SHIFT                         _MK_SHIFT_CONST(0)
#define XUSB_MSI_QUEUE_0_BITS_FIELD                        (_MK_SHIFT_CONST(0x1) << XUSB_MSI_QUEUE_0_BITS_SHIFT)
#define XUSB_MSI_QUEUE_0_BITS_RANGE                        0:0
#define XUSB_MSI_QUEUE_0_BITS_WOFFSET                      0
#define XUSB_MSI_QUEUE_0_BITS_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_QUEUE_0_BITS_DEFAULT_MASK                 (_MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_BITS_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_QUEUE_0_BITS_SW_DEFAULT_MASK              (_MK_MASK_CONST(0x1)
#define XUSB_MSI_QUEUE_0_BITS_ALL_NONISO                   _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_QUEUE_0_BITS_ALL_ISO                      _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_QUEUE_0_BITS__PROD                        _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_MAP_0                                     _MK_ADDR_CONST(0x000000DC)
#define XUSB_MSI_MAP_0_SECURE                              0
#define XUSB_MSI_MAP_0_WORD_COUNT                          1
#define XUSB_MSI_MAP_0_RESET_VAL                           _MK_MASK_CONST(0xa8020008)
#define XUSB_MSI_MAP_0_RESET_MASK                          _MK_MASK_CONST(0xf803ffff)
#define XUSB_MSI_MAP_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0xa8020008)
#define XUSB_MSI_MAP_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0xf803ffff)
#define XUSB_MSI_MAP_0_READ_MASK                           _MK_MASK_CONST(0xf803ffff)
#define XUSB_MSI_MAP_0_WRITE_MASK                          _MK_MASK_CONST(0x10000)
#define XUSB_MSI_MAP_0_CAP_TYPE_SHIFT                       _MK_SHIFT_CONST(27)
#define XUSB_MSI_MAP_0_CAP_TYPE_FIELD                      (_MK_SHIFT_CONST(0x1f) << XUSB_MSI_MAP_0_CAP_TYPE_SHIFT)
#define XUSB_MSI_MAP_0_CAP_TYPE_RANGE                      31:27
#define XUSB_MSI_MAP_0_CAP_TYPE_WOFFSET                    0
#define XUSB_MSI_MAP_0_CAP_TYPE_DEFAULT                    (_MK_MASK_CONST(0x00000015)
#define XUSB_MSI_MAP_0_CAP_TYPE_DEFAULT_MASK               (_MK_MASK_CONST(0x1f)
#define XUSB_MSI_MAP_0_CAP_TYPE_SW_DEFAULT                 (_MK_MASK_CONST(0x00000015)
#define XUSB_MSI_MAP_0_CAP_TYPE_SW_DEFAULT_MASK            (_MK_MASK_CONST(0x1f)
#define XUSB_MSI_MAP_0_FIXD_SHIFT                           _MK_SHIFT_CONST(17)
#define XUSB_MSI_MAP_0_FIXD_FIELD                          (_MK_SHIFT_CONST(0x1) << XUSB_MSI_MAP_0_FIXD_SHIFT)
#define XUSB_MSI_MAP_0_FIXD_RANGE                          17:17
#define XUSB_MSI_MAP_0_FIXD_WOFFSET                        0
#define XUSB_MSI_MAP_0_FIXD_DEFAULT                        (_MK_MASK_CONST(0x00000001)
#define XUSB_MSI_MAP_0_FIXD_DEFAULT_MASK                   (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MAP_0_FIXD_SW_DEFAULT                     (_MK_MASK_CONST(0x00000001)
#define XUSB_MSI_MAP_0_FIXD_SW_DEFAULT_MASK                (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MAP_0_FIXD_ON                             _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_MAP_0_EN_SHIFT                             _MK_SHIFT_CONST(16)
#define XUSB_MSI_MAP_0_EN_FIELD                            (_MK_SHIFT_CONST(0x1) << XUSB_MSI_MAP_0_EN_SHIFT)
#define XUSB_MSI_MAP_0_EN_RANGE                            16:16
#define XUSB_MSI_MAP_0_EN_WOFFSET                          0
#define XUSB_MSI_MAP_0_EN_DEFAULT                          (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MAP_0_EN_DEFAULT_MASK                     (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MAP_0_EN_SW_DEFAULT                       (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MAP_0_EN_SW_DEFAULT_MASK                  (_MK_MASK_CONST(0x1)
#define XUSB_MSI_MAP_0_EN__NOPRDCHK                        _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_MAP_0_EN_OFF                              _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_MAP_0_EN_ON                               _MK_ENUM_CONST(0x00000001)
#define XUSB_MSI_MAP_0_NEXT_PTR_SHIFT                       _MK_SHIFT_CONST(8)
#define XUSB_MSI_MAP_0_NEXT_PTR_FIELD                      (_MK_SHIFT_CONST(0xff) << XUSB_MSI_MAP_0_NEXT_PTR_SHIFT)
#define XUSB_MSI_MAP_0_NEXT_PTR_RANGE                      15:8
#define XUSB_MSI_MAP_0_NEXT_PTR_WOFFSET                    0
#define XUSB_MSI_MAP_0_NEXT_PTR_DEFAULT                    (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MAP_0_NEXT_PTR_DEFAULT_MASK               (_MK_MASK_CONST(0xff)
#define XUSB_MSI_MAP_0_NEXT_PTR_SW_DEFAULT                 (_MK_MASK_CONST(0x00000000)
#define XUSB_MSI_MAP_0_NEXT_PTR_SW_DEFAULT_MASK            (_MK_MASK_CONST(0xff)
#define XUSB_MSI_MAP_0_NEXT_PTR_MSI                        _MK_ENUM_CONST(0x000000C0)
#define XUSB_MSI_MAP_0_NEXT_PTR_MSIX                       _MK_ENUM_CONST(0x00000070)
#define XUSB_MSI_MAP_0_NEXT_PTR_PMCAP                      _MK_ENUM_CONST(0x00000044)
#define XUSB_MSI_MAP_0_NEXT_PTR_NULL                       _MK_ENUM_CONST(0x00000000)
#define XUSB_MSI_MAP_0_CAP_ID_SHIFT                         _MK_SHIFT_CONST(0)
#define XUSB_MSI_MAP_0_CAP_ID_FIELD                        (_MK_SHIFT_CONST(0xff) << XUSB_MSI_MAP_0_CAP_ID_SHIFT)
#define XUSB_MSI_MAP_0_CAP_ID_RANGE                        7:0
#define XUSB_MSI_MAP_0_CAP_ID_WOFFSET                      0
#define XUSB_MSI_MAP_0_CAP_ID_DEFAULT                      (_MK_MASK_CONST(0x00000008)
#define XUSB_MSI_MAP_0_CAP_ID_DEFAULT_MASK                 (_MK_MASK_CONST(0xff)
#define XUSB_MSI_MAP_0_CAP_ID_SW_DEFAULT                   (_MK_MASK_CONST(0x00000008)
#define XUSB_MSI_MAP_0_CAP_ID_SW_DEFAULT_MASK              (_MK_MASK_CONST(0xff)
#define XUSB_MSI_MAP_0_CAP_ID_MSI                          _MK_ENUM_CONST(0x00000008)
#define XUSB_FPCICFG_0                                     _MK_ADDR_CONST(0x000000F8)
#define XUSB_FPCICFG_0_SECURE                              0
#define XUSB_FPCICFG_0_WORD_COUNT                          1
#define XUSB_FPCICFG_0_RESET_VAL                           _MK_MASK_CONST(0xe08010)
#define XUSB_FPCICFG_0_RESET_MASK                          _MK_MASK_CONST(0xffe080ff)
#define XUSB_FPCICFG_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0xe08010)
#define XUSB_FPCICFG_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0xffe080ff)
#define XUSB_FPCICFG_0_READ_MASK                           _MK_MASK_CONST(0xffe080ff)
#define XUSB_FPCICFG_0_WRITE_MASK                          _MK_MASK_CONST(0xffe080ff)
#define XUSB_FPCICFG_0_ISOCMD_SHIFT                         _MK_SHIFT_CONST(0)
#define XUSB_FPCICFG_0_ISOCMD_FIELD                        (_MK_SHIFT_CONST(0x3) << XUSB_FPCICFG_0_ISOCMD_SHIFT)
#define XUSB_FPCICFG_0_ISOCMD_RANGE                        1:0
#define XUSB_FPCICFG_0_ISOCMD_WOFFSET                      0
#define XUSB_FPCICFG_0_ISOCMD_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_ISOCMD_DEFAULT_MASK                 (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_ISOCMD_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_ISOCMD_SW_DEFAULT_MASK              (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_ISOCMD_TOY                          _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_ISOCMD_ISO                          _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_ISOCMD_NONISO                       _MK_ENUM_CONST(0x00000002)
#define XUSB_FPCICFG_0_PASSPW_SHIFT                         _MK_SHIFT_CONST(2)
#define XUSB_FPCICFG_0_PASSPW_FIELD                        (_MK_SHIFT_CONST(0x3) << XUSB_FPCICFG_0_PASSPW_SHIFT)
#define XUSB_FPCICFG_0_PASSPW_RANGE                        3:2
#define XUSB_FPCICFG_0_PASSPW_WOFFSET                      0
#define XUSB_FPCICFG_0_PASSPW_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_PASSPW_DEFAULT_MASK                 (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_PASSPW_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_PASSPW_SW_DEFAULT_MASK              (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_PASSPW_TOY                          _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_PASSPW_PASS                         _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_PASSPW_NOPASS                       _MK_ENUM_CONST(0x00000002)
#define XUSB_FPCICFG_0_RSPPASSPW_SHIFT                      _MK_SHIFT_CONST(4)
#define XUSB_FPCICFG_0_RSPPASSPW_FIELD                     (_MK_SHIFT_CONST(0x3) << XUSB_FPCICFG_0_RSPPASSPW_SHIFT)
#define XUSB_FPCICFG_0_RSPPASSPW_RANGE                     5:4
#define XUSB_FPCICFG_0_RSPPASSPW_WOFFSET                   0
#define XUSB_FPCICFG_0_RSPPASSPW_DEFAULT                   (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_RSPPASSPW_DEFAULT_MASK              (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_RSPPASSPW_SW_DEFAULT                (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_RSPPASSPW_SW_DEFAULT_MASK           (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_RSPPASSPW_TOY                       _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_RSPPASSPW_PASS                      _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_RSPPASSPW_NOPASS                    _MK_ENUM_CONST(0x00000002)
#define XUSB_FPCICFG_0_RSPPASSPW__PROD                     _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_COHCMD_SHIFT                         _MK_SHIFT_CONST(6)
#define XUSB_FPCICFG_0_COHCMD_FIELD                        (_MK_SHIFT_CONST(0x3) << XUSB_FPCICFG_0_COHCMD_SHIFT)
#define XUSB_FPCICFG_0_COHCMD_RANGE                        7:6
#define XUSB_FPCICFG_0_COHCMD_WOFFSET                      0
#define XUSB_FPCICFG_0_COHCMD_DEFAULT                      (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_COHCMD_DEFAULT_MASK                 (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_COHCMD_SW_DEFAULT                   (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_COHCMD_SW_DEFAULT_MASK              (_MK_MASK_CONST(0x3)
#define XUSB_FPCICFG_0_COHCMD_TOY                          _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_COHCMD_COH                          _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_COHCMD_NONCOH                       _MK_ENUM_CONST(0x00000002)
#define XUSB_FPCICFG_0_ERR_SEVERITY_SHIFT                   _MK_SHIFT_CONST(15)
#define XUSB_FPCICFG_0_ERR_SEVERITY_FIELD                  (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_ERR_SEVERITY_SHIFT)
#define XUSB_FPCICFG_0_ERR_SEVERITY_RANGE                  15:15
#define XUSB_FPCICFG_0_ERR_SEVERITY_WOFFSET                0
#define XUSB_FPCICFG_0_ERR_SEVERITY_DEFAULT                (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_ERR_SEVERITY_DEFAULT_MASK           (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_ERR_SEVERITY_SW_DEFAULT             (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_ERR_SEVERITY_SW_DEFAULT_MASK        (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_ERR_SEVERITY_NONFATAL               _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_ERR_SEVERITY_FATAL                  _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_UID_CLUMPING_SHIFT                   _MK_SHIFT_CONST(21)
#define XUSB_FPCICFG_0_UID_CLUMPING_FIELD                  (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_UID_CLUMPING_SHIFT)
#define XUSB_FPCICFG_0_UID_CLUMPING_RANGE                  21:21
#define XUSB_FPCICFG_0_UID_CLUMPING_WOFFSET                0
#define XUSB_FPCICFG_0_UID_CLUMPING_DEFAULT                (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_UID_CLUMPING_DEFAULT_MASK           (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_UID_CLUMPING_SW_DEFAULT             (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_UID_CLUMPING_SW_DEFAULT_MASK        (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_UID_CLUMPING_DISABLED               _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_UID_CLUMPING_ENABLED                _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_SHIFT                 _MK_SHIFT_CONST(22)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_FIELD                (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_TGTDONE_PASSPW_SHIFT)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_RANGE                22:22
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_WOFFSET              0
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_DEFAULT              (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_DEFAULT_MASK         (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_SW_DEFAULT           (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_SW_DEFAULT_MASK      (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_SET                  _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_TGTDONE_PASSPW_CLR                  _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_SHIFT            _MK_SHIFT_CONST(23)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_FIELD           (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_SHIFT)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_RANGE           23:23
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_WOFFSET         0
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_DEFAULT         (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_DEFAULT_MASK    (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_SW_DEFAULT      (_MK_MASK_CONST(0x00000001)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_SW_DEFAULT_MASK (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_OFF             _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_FIX_DEADLOCK_ENABLE_ON              _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_SHIFT             _MK_SHIFT_CONST(24)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_FIELD            (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_SHIFT)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_RANGE            24:24
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_WOFFSET          0
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_DEFAULT          (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_DEFAULT_MASK     (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_SW_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_SW_DEFAULT_MASK  (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_OFF              _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_ERR_ENABLE_ON               _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_SHIFT          _MK_SHIFT_CONST(25)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_FIELD         (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_SHIFT)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_RANGE         25:25
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_WOFFSET       0
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_DEFAULT_MASK  (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_SW_DEFAULT    (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_SW_DEFAULT_MASK (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_OFF           _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_MA_ERR_ENABLE_ON            _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_SHIFT          _MK_SHIFT_CONST(26)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_FIELD         (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_SHIFT)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_RANGE         26:26
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_WOFFSET       0
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_DEFAULT_MASK  (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_SW_DEFAULT    (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_SW_DEFAULT_MASK (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_OFF           _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_DROP_ON_TA_ERR_ENABLE_ON            _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_SHIFT          _MK_SHIFT_CONST(27)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_FIELD         (_MK_SHIFT_CONST(0x1) << XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_SHIFT)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_RANGE         27:27
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_WOFFSET       0
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_DEFAULT       (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_DEFAULT_MASK  (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_SW_DEFAULT    (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_SW_DEFAULT_MASK (_MK_MASK_CONST(0x1)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_OFF           _MK_ENUM_CONST(0x00000000)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ENABLE_ON            _MK_ENUM_CONST(0x00000001)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_SHIFT              _MK_SHIFT_CONST(28)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_FIELD             (_MK_SHIFT_CONST(0xf) << XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_SHIFT)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_RANGE             31:28
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_WOFFSET           0
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_DEFAULT           (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_DEFAULT_MASK      (_MK_MASK_CONST(0xf)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_SW_DEFAULT        (_MK_MASK_CONST(0x00000000)
#define XUSB_FPCICFG_0_DEVID_OVERRIDE_ID_SW_DEFAULT_MASK   (_MK_MASK_CONST(0xf)
#define LIST_XUSB_REGS(_op_) \
  _op_(XUSB_CFG_0_0) \
  _op_(XUSB_CFG_1_0) \
  _op_(XUSB_CFG_2_0) \
  _op_(XUSB_CFG_3_0) \
  _op_(XUSB_CFG_4_0) \
  _op_(XUSB_CFG_5_0) \
  _op_(XUSB_CFG_6_0) \
  _op_(XUSB_CFG_6_1) \
  _op_(XUSB_CFG_6_2) \
  _op_(XUSB_CFG_6_3) \
  _op_(XUSB_CFG_6_4) \
  _op_(XUSB_CFG_11_0) \
  _op_(XUSB_CFG_12_0) \
  _op_(XUSB_CFG_13_0) \
  _op_(XUSB_CFG_14_0) \
  _op_(XUSB_CFG_15_0) \
  _op_(XUSB_CFG_16_0) \
  _op_(XUSB_CFG_EMU_RSVD_0) \
  _op_(XUSB_CFG_EMU_RSVD_1) \
  _op_(XUSB_CFG_EMU_RSVD_2) \
  _op_(XUSB_CFG_EMU_RSVD_3) \
  _op_(XUSB_CFG_EMU_RSVD_4) \
  _op_(XUSB_CFG_EMU_RSVD_5) \
  _op_(XUSB_CFG_EMU_RSVD_6) \
  _op_(XUSB_CFG_EMU_RSVD_7) \
  _op_(XUSB_CFG_EMU_RSVD_8) \
  _op_(XUSB_CFG_EMU_RSVD_9) \
  _op_(XUSB_CFG_EMU_RSVD_10) \
  _op_(XUSB_CFG_EMU_RSVD_11) \
  _op_(XUSB_CFG_EMU_RSVD_12) \
  _op_(XUSB_CFG_EMU_RSVD_13) \
  _op_(XUSB_CFG_EMU_RSVD_14) \
  _op_(XUSB_CFG_EMU_RSVD_15) \
  _op_(XUSB_MSI_CTRL_0) \
  _op_(XUSB_MSI_ADDR1_0) \
  _op_(XUSB_MSI_ADDR2_0) \
  _op_(XUSB_MSI_DATA_0) \
  _op_(XUSB_MSI_MASK_0) \
  _op_(XUSB_MSI_PEND_0) \
  _op_(XUSB_MSI_QUEUE_0) \
  _op_(XUSB_MSI_MAP_0) \
  _op_(XUSB_FPCICFG_0)
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
#endif
