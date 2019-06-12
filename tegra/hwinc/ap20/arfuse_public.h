
#ifndef ___ARFUSE_PUBLIC_H_INC_
#define ___ARFUSE_PUBLIC_H_INC_
// --------------------------------------------------------------------------
//
// Copyright (c) 2010, NVIDIA Corp.
// All Rights Reserved.
//

// Fuse macro bypass register which controls fuse macro bypass
// Disabled once production_mode is set.

// Register FUSE_FUSEBYPASS_0  
#define FUSE_FUSEBYPASS_0                       _MK_ADDR_CONST(0x24)
#define FUSE_FUSEBYPASS_0_SECURE                        0x0
#define FUSE_FUSEBYPASS_0_WORD_COUNT                    0x1
#define FUSE_FUSEBYPASS_0_RESET_VAL                     _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_RESET_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_FUSEBYPASS_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_READ_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_FUSEBYPASS_0_WRITE_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_SHIFT)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_RANGE                  0:0
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_WOFFSET                        0x0
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_INIT_ENUM                      DISABLED
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_DISABLED                       _MK_ENUM_CONST(0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_ENABLED                        _MK_ENUM_CONST(1)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_DISABLE                        _MK_ENUM_CONST(0)
#define FUSE_FUSEBYPASS_0_FUSEBYPASS_VAL_ENABLE                 _MK_ENUM_CONST(1)

// Software read/write control and status register
// Controls the write access of chip option regs.
// Bit 0 of the register is the write control register. When set to 1,
// it disables writes to chip options.
// Bit 16 is the write access status register. It gets set to 1 when 
// there is a write access to chip option and the write control is high

// Register FUSE_WRITE_ACCESS_SW_0  
#define FUSE_WRITE_ACCESS_SW_0                  _MK_ADDR_CONST(0x30)
#define FUSE_WRITE_ACCESS_SW_0_SECURE                   0x0
#define FUSE_WRITE_ACCESS_SW_0_WORD_COUNT                       0x1
#define FUSE_WRITE_ACCESS_SW_0_RESET_VAL                        _MK_MASK_CONST(0x1)
#define FUSE_WRITE_ACCESS_SW_0_RESET_MASK                       _MK_MASK_CONST(0x1)
#define FUSE_WRITE_ACCESS_SW_0_SW_DEFAULT_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_READ_MASK                        _MK_MASK_CONST(0x10001)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_MASK                       _MK_MASK_CONST(0x10001)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_SHIFT                       _MK_SHIFT_CONST(0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_FIELD                       (_MK_MASK_CONST(0x1) << FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_SHIFT)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_RANGE                       0:0
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_WOFFSET                     0x0
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_DEFAULT                     _MK_MASK_CONST(0x1)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_DEFAULT_MASK                        _MK_MASK_CONST(0x1)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_INIT_ENUM                   READONLY
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_READWRITE                   _MK_ENUM_CONST(0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_READONLY                    _MK_ENUM_CONST(1)

#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_SHIFT                     _MK_SHIFT_CONST(16)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_FIELD                     (_MK_MASK_CONST(0x1) << FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_SHIFT)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_RANGE                     16:16
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_WOFFSET                   0x0
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_DEFAULT                   _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_NOWRITE                   _MK_ENUM_CONST(0)
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_STATUS_WRITE                     _MK_ENUM_CONST(1)


// Chip Option: jtag_secureID_0
// NV - Secure JTAG ID / Unique ID (UID0)
// Write protected by production_mode

// Register FUSE_JTAG_SECUREID_0_0  
#define FUSE_JTAG_SECUREID_0_0                  _MK_ADDR_CONST(0x108)
#define FUSE_JTAG_SECUREID_0_0_SECURE                   0x0
#define FUSE_JTAG_SECUREID_0_0_WORD_COUNT                       0x1
#define FUSE_JTAG_SECUREID_0_0_RESET_VAL                        _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_0_0_RESET_MASK                       _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_0_0_SW_DEFAULT_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_0_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_0_0_READ_MASK                        _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_0_0_WRITE_MASK                       _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_FIELD                    (_MK_MASK_CONST(0xffffffff) << FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_SHIFT)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_RANGE                    31:0
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_WOFFSET                  0x0
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_DEFAULT_MASK                     _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_0_0_JTAG_SECUREID_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: jtag_secureID_1
// NV - Secure JTAG ID / Unique ID (UID1)
// Write protected by production_mode

// Register FUSE_JTAG_SECUREID_1_0  
#define FUSE_JTAG_SECUREID_1_0                  _MK_ADDR_CONST(0x10c)
#define FUSE_JTAG_SECUREID_1_0_SECURE                   0x0
#define FUSE_JTAG_SECUREID_1_0_WORD_COUNT                       0x1
#define FUSE_JTAG_SECUREID_1_0_RESET_VAL                        _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_1_0_RESET_MASK                       _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_1_0_SW_DEFAULT_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_1_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_1_0_READ_MASK                        _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_1_0_WRITE_MASK                       _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_FIELD                    (_MK_MASK_CONST(0xffffffff) << FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_SHIFT)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_RANGE                    31:0
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_WOFFSET                  0x0
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_DEFAULT_MASK                     _MK_MASK_CONST(0xffffffff)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_JTAG_SECUREID_1_0_JTAG_SECUREID_1_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: sku_info
// NV - Chip feature info
// Write protected by production_mode

// Register FUSE_SKU_INFO_0  
#define FUSE_SKU_INFO_0                 _MK_ADDR_CONST(0x110)
#define FUSE_SKU_INFO_0_SECURE                  0x0
#define FUSE_SKU_INFO_0_WORD_COUNT                      0x1
#define FUSE_SKU_INFO_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SKU_INFO_0_RESET_MASK                      _MK_MASK_CONST(0xff)
#define FUSE_SKU_INFO_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define FUSE_SKU_INFO_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define FUSE_SKU_INFO_0_READ_MASK                       _MK_MASK_CONST(0xff)
#define FUSE_SKU_INFO_0_WRITE_MASK                      _MK_MASK_CONST(0xff)
#define FUSE_SKU_INFO_0_SKU_INFO_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SKU_INFO_0_SKU_INFO_FIELD                  (_MK_MASK_CONST(0xff) << FUSE_SKU_INFO_0_SKU_INFO_SHIFT)
#define FUSE_SKU_INFO_0_SKU_INFO_RANGE                  7:0
#define FUSE_SKU_INFO_0_SKU_INFO_WOFFSET                        0x0
#define FUSE_SKU_INFO_0_SKU_INFO_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SKU_INFO_0_SKU_INFO_DEFAULT_MASK                   _MK_MASK_CONST(0xff)
#define FUSE_SKU_INFO_0_SKU_INFO_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SKU_INFO_0_SKU_INFO_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: process_calib
// NV - Process calibration
// Write protected by production_mode

// Register FUSE_PROCESS_CALIB_0  
#define FUSE_PROCESS_CALIB_0                    _MK_ADDR_CONST(0x114)
#define FUSE_PROCESS_CALIB_0_SECURE                     0x0
#define FUSE_PROCESS_CALIB_0_WORD_COUNT                         0x1
#define FUSE_PROCESS_CALIB_0_RESET_VAL                  _MK_MASK_CONST(0x0)
#define FUSE_PROCESS_CALIB_0_RESET_MASK                         _MK_MASK_CONST(0x3)
#define FUSE_PROCESS_CALIB_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define FUSE_PROCESS_CALIB_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define FUSE_PROCESS_CALIB_0_READ_MASK                  _MK_MASK_CONST(0x3)
#define FUSE_PROCESS_CALIB_0_WRITE_MASK                         _MK_MASK_CONST(0x3)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_SHIFT                        _MK_SHIFT_CONST(0)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_FIELD                        (_MK_MASK_CONST(0x3) << FUSE_PROCESS_CALIB_0_PROCESS_CALIB_SHIFT)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_RANGE                        1:0
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_WOFFSET                      0x0
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_DEFAULT                      _MK_MASK_CONST(0x0)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_DEFAULT_MASK                 _MK_MASK_CONST(0x3)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define FUSE_PROCESS_CALIB_0_PROCESS_CALIB_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

// Chip Option: io_calib
// NV - IO calibration
// Write protected by production_mode

// Register FUSE_IO_CALIB_0  
#define FUSE_IO_CALIB_0                 _MK_ADDR_CONST(0x118)
#define FUSE_IO_CALIB_0_SECURE                  0x0
#define FUSE_IO_CALIB_0_WORD_COUNT                      0x1
#define FUSE_IO_CALIB_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_IO_CALIB_0_RESET_MASK                      _MK_MASK_CONST(0x3ff)
#define FUSE_IO_CALIB_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define FUSE_IO_CALIB_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define FUSE_IO_CALIB_0_READ_MASK                       _MK_MASK_CONST(0x3ff)
#define FUSE_IO_CALIB_0_WRITE_MASK                      _MK_MASK_CONST(0x3ff)
#define FUSE_IO_CALIB_0_IO_CALIB_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_IO_CALIB_0_IO_CALIB_FIELD                  (_MK_MASK_CONST(0x3ff) << FUSE_IO_CALIB_0_IO_CALIB_SHIFT)
#define FUSE_IO_CALIB_0_IO_CALIB_RANGE                  9:0
#define FUSE_IO_CALIB_0_IO_CALIB_WOFFSET                        0x0
#define FUSE_IO_CALIB_0_IO_CALIB_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_IO_CALIB_0_IO_CALIB_DEFAULT_MASK                   _MK_MASK_CONST(0x3ff)
#define FUSE_IO_CALIB_0_IO_CALIB_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_IO_CALIB_0_IO_CALIB_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: dac_crt_calib
// NV - DAC calibration (set 1/3)
// Write protected by production_mode

// Register FUSE_DAC_CRT_CALIB_0  
#define FUSE_DAC_CRT_CALIB_0                    _MK_ADDR_CONST(0x11c)
#define FUSE_DAC_CRT_CALIB_0_SECURE                     0x0
#define FUSE_DAC_CRT_CALIB_0_WORD_COUNT                         0x1
#define FUSE_DAC_CRT_CALIB_0_RESET_VAL                  _MK_MASK_CONST(0x0)
#define FUSE_DAC_CRT_CALIB_0_RESET_MASK                         _MK_MASK_CONST(0xff)
#define FUSE_DAC_CRT_CALIB_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define FUSE_DAC_CRT_CALIB_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define FUSE_DAC_CRT_CALIB_0_READ_MASK                  _MK_MASK_CONST(0xff)
#define FUSE_DAC_CRT_CALIB_0_WRITE_MASK                         _MK_MASK_CONST(0xff)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_SHIFT                        _MK_SHIFT_CONST(0)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_FIELD                        (_MK_MASK_CONST(0xff) << FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_SHIFT)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_RANGE                        7:0
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_WOFFSET                      0x0
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_DEFAULT                      _MK_MASK_CONST(0x0)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_DEFAULT_MASK                 _MK_MASK_CONST(0xff)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define FUSE_DAC_CRT_CALIB_0_DAC_CRT_CALIB_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

// Chip Option: dac_hdtv_calib
// NV - DAC calibration (set 2/3)
// Write protected by production_mode

// Register FUSE_DAC_HDTV_CALIB_0  
#define FUSE_DAC_HDTV_CALIB_0                   _MK_ADDR_CONST(0x120)
#define FUSE_DAC_HDTV_CALIB_0_SECURE                    0x0
#define FUSE_DAC_HDTV_CALIB_0_WORD_COUNT                        0x1
#define FUSE_DAC_HDTV_CALIB_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define FUSE_DAC_HDTV_CALIB_0_RESET_MASK                        _MK_MASK_CONST(0xff)
#define FUSE_DAC_HDTV_CALIB_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_DAC_HDTV_CALIB_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_DAC_HDTV_CALIB_0_READ_MASK                         _MK_MASK_CONST(0xff)
#define FUSE_DAC_HDTV_CALIB_0_WRITE_MASK                        _MK_MASK_CONST(0xff)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_SHIFT                      _MK_SHIFT_CONST(0)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_FIELD                      (_MK_MASK_CONST(0xff) << FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_SHIFT)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_RANGE                      7:0
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_WOFFSET                    0x0
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_DEFAULT                    _MK_MASK_CONST(0x0)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_DEFAULT_MASK                       _MK_MASK_CONST(0xff)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define FUSE_DAC_HDTV_CALIB_0_DAC_HDTV_CALIB_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)

// Chip Option: dac_sdtv_calib
// NV - DAC calibration (set 3/3)
// Write protected by production_mode

// Register FUSE_DAC_SDTV_CALIB_0  
#define FUSE_DAC_SDTV_CALIB_0                   _MK_ADDR_CONST(0x124)
#define FUSE_DAC_SDTV_CALIB_0_SECURE                    0x0
#define FUSE_DAC_SDTV_CALIB_0_WORD_COUNT                        0x1
#define FUSE_DAC_SDTV_CALIB_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define FUSE_DAC_SDTV_CALIB_0_RESET_MASK                        _MK_MASK_CONST(0xff)
#define FUSE_DAC_SDTV_CALIB_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_DAC_SDTV_CALIB_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_DAC_SDTV_CALIB_0_READ_MASK                         _MK_MASK_CONST(0xff)
#define FUSE_DAC_SDTV_CALIB_0_WRITE_MASK                        _MK_MASK_CONST(0xff)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_SHIFT                      _MK_SHIFT_CONST(0)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_FIELD                      (_MK_MASK_CONST(0xff) << FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_SHIFT)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_RANGE                      7:0
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_WOFFSET                    0x0
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_DEFAULT                    _MK_MASK_CONST(0x0)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_DEFAULT_MASK                       _MK_MASK_CONST(0xff)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define FUSE_DAC_SDTV_CALIB_0_DAC_SDTV_CALIB_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Chip Option: reserved_production
// NV - Reserved for NVIDIA usage
//   bit  0:   macrovision enable
//   bit  1:   hdcp enable
//   bits 2-3: reserved
//   number of bits set to fill fuse block
// Write protected by production_mode

// Register FUSE_RESERVED_PRODUCTION_0  
#define FUSE_RESERVED_PRODUCTION_0                      _MK_ADDR_CONST(0x14c)
#define FUSE_RESERVED_PRODUCTION_0_SECURE                       0x0
#define FUSE_RESERVED_PRODUCTION_0_WORD_COUNT                   0x1
#define FUSE_RESERVED_PRODUCTION_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_RESERVED_PRODUCTION_0_RESET_MASK                   _MK_MASK_CONST(0xf)
#define FUSE_RESERVED_PRODUCTION_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_RESERVED_PRODUCTION_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_RESERVED_PRODUCTION_0_READ_MASK                    _MK_MASK_CONST(0xf)
#define FUSE_RESERVED_PRODUCTION_0_WRITE_MASK                   _MK_MASK_CONST(0xf)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_FIELD                    (_MK_MASK_CONST(0xf) << FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_SHIFT)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_RANGE                    3:0
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_WOFFSET                  0x0
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_DEFAULT_MASK                     _MK_MASK_CONST(0xf)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_RESERVED_PRODUCTION_0_RESERVED_PRODUCTION_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)


// Chip Option: spare_bit_0

// Register FUSE_SPARE_BIT_0_0  
#define FUSE_SPARE_BIT_0_0                      _MK_ADDR_CONST(0x200)
#define FUSE_SPARE_BIT_0_0_SECURE                       0x0
#define FUSE_SPARE_BIT_0_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_0_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_0_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_0_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_0_0_SPARE_BIT_0_SHIFT)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_RANGE                    0:0
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_WOFFSET                  0x0
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_0_0_SPARE_BIT_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_1

// Register FUSE_SPARE_BIT_1_0  
#define FUSE_SPARE_BIT_1_0                      _MK_ADDR_CONST(0x204)
#define FUSE_SPARE_BIT_1_0_SECURE                       0x0
#define FUSE_SPARE_BIT_1_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_1_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_1_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_1_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_1_0_SPARE_BIT_1_SHIFT)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_RANGE                    0:0
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_WOFFSET                  0x0
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_1_0_SPARE_BIT_1_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_2

// Register FUSE_SPARE_BIT_2_0  
#define FUSE_SPARE_BIT_2_0                      _MK_ADDR_CONST(0x208)
#define FUSE_SPARE_BIT_2_0_SECURE                       0x0
#define FUSE_SPARE_BIT_2_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_2_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_2_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_2_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_2_0_SPARE_BIT_2_SHIFT)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_RANGE                    0:0
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_WOFFSET                  0x0
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_2_0_SPARE_BIT_2_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_3

// Register FUSE_SPARE_BIT_3_0  
#define FUSE_SPARE_BIT_3_0                      _MK_ADDR_CONST(0x20c)
#define FUSE_SPARE_BIT_3_0_SECURE                       0x0
#define FUSE_SPARE_BIT_3_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_3_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_3_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_3_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_3_0_SPARE_BIT_3_SHIFT)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_RANGE                    0:0
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_WOFFSET                  0x0
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_3_0_SPARE_BIT_3_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_4

// Register FUSE_SPARE_BIT_4_0  
#define FUSE_SPARE_BIT_4_0                      _MK_ADDR_CONST(0x210)
#define FUSE_SPARE_BIT_4_0_SECURE                       0x0
#define FUSE_SPARE_BIT_4_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_4_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_4_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_4_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_4_0_SPARE_BIT_4_SHIFT)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_RANGE                    0:0
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_WOFFSET                  0x0
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_4_0_SPARE_BIT_4_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_5

// Register FUSE_SPARE_BIT_5_0  
#define FUSE_SPARE_BIT_5_0                      _MK_ADDR_CONST(0x214)
#define FUSE_SPARE_BIT_5_0_SECURE                       0x0
#define FUSE_SPARE_BIT_5_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_5_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_5_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_5_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_5_0_SPARE_BIT_5_SHIFT)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_RANGE                    0:0
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_WOFFSET                  0x0
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_5_0_SPARE_BIT_5_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_6

// Register FUSE_SPARE_BIT_6_0  
#define FUSE_SPARE_BIT_6_0                      _MK_ADDR_CONST(0x218)
#define FUSE_SPARE_BIT_6_0_SECURE                       0x0
#define FUSE_SPARE_BIT_6_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_6_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_6_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_6_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_6_0_SPARE_BIT_6_SHIFT)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_RANGE                    0:0
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_WOFFSET                  0x0
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_6_0_SPARE_BIT_6_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_7

// Register FUSE_SPARE_BIT_7_0  
#define FUSE_SPARE_BIT_7_0                      _MK_ADDR_CONST(0x21c)
#define FUSE_SPARE_BIT_7_0_SECURE                       0x0
#define FUSE_SPARE_BIT_7_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_7_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_7_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_7_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_7_0_SPARE_BIT_7_SHIFT)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_RANGE                    0:0
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_WOFFSET                  0x0
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_7_0_SPARE_BIT_7_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_8

// Register FUSE_SPARE_BIT_8_0  
#define FUSE_SPARE_BIT_8_0                      _MK_ADDR_CONST(0x220)
#define FUSE_SPARE_BIT_8_0_SECURE                       0x0
#define FUSE_SPARE_BIT_8_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_8_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_8_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_8_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_8_0_SPARE_BIT_8_SHIFT)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_RANGE                    0:0
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_WOFFSET                  0x0
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_8_0_SPARE_BIT_8_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_9

// Register FUSE_SPARE_BIT_9_0  
#define FUSE_SPARE_BIT_9_0                      _MK_ADDR_CONST(0x224)
#define FUSE_SPARE_BIT_9_0_SECURE                       0x0
#define FUSE_SPARE_BIT_9_0_WORD_COUNT                   0x1
#define FUSE_SPARE_BIT_9_0_RESET_VAL                    _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_RESET_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_9_0_SW_DEFAULT_VAL                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_READ_MASK                    _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_9_0_WRITE_MASK                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_SHIFT                    _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_FIELD                    (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_9_0_SPARE_BIT_9_SHIFT)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_RANGE                    0:0
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_WOFFSET                  0x0
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_DEFAULT                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_DEFAULT_MASK                     _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_9_0_SPARE_BIT_9_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_10

// Register FUSE_SPARE_BIT_10_0  
#define FUSE_SPARE_BIT_10_0                     _MK_ADDR_CONST(0x228)
#define FUSE_SPARE_BIT_10_0_SECURE                      0x0
#define FUSE_SPARE_BIT_10_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_10_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_10_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_10_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_10_0_SPARE_BIT_10_SHIFT)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_RANGE                  0:0
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_WOFFSET                        0x0
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_10_0_SPARE_BIT_10_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_11

// Register FUSE_SPARE_BIT_11_0  
#define FUSE_SPARE_BIT_11_0                     _MK_ADDR_CONST(0x22c)
#define FUSE_SPARE_BIT_11_0_SECURE                      0x0
#define FUSE_SPARE_BIT_11_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_11_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_11_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_11_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_11_0_SPARE_BIT_11_SHIFT)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_RANGE                  0:0
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_WOFFSET                        0x0
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_11_0_SPARE_BIT_11_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_12

// Register FUSE_SPARE_BIT_12_0  
#define FUSE_SPARE_BIT_12_0                     _MK_ADDR_CONST(0x230)
#define FUSE_SPARE_BIT_12_0_SECURE                      0x0
#define FUSE_SPARE_BIT_12_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_12_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_12_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_12_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_12_0_SPARE_BIT_12_SHIFT)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_RANGE                  0:0
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_WOFFSET                        0x0
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_12_0_SPARE_BIT_12_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_13

// Register FUSE_SPARE_BIT_13_0  
#define FUSE_SPARE_BIT_13_0                     _MK_ADDR_CONST(0x234)
#define FUSE_SPARE_BIT_13_0_SECURE                      0x0
#define FUSE_SPARE_BIT_13_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_13_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_13_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_13_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_13_0_SPARE_BIT_13_SHIFT)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_RANGE                  0:0
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_WOFFSET                        0x0
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_13_0_SPARE_BIT_13_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_14

// Register FUSE_SPARE_BIT_14_0  
#define FUSE_SPARE_BIT_14_0                     _MK_ADDR_CONST(0x238)
#define FUSE_SPARE_BIT_14_0_SECURE                      0x0
#define FUSE_SPARE_BIT_14_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_14_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_14_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_14_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_14_0_SPARE_BIT_14_SHIFT)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_RANGE                  0:0
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_WOFFSET                        0x0
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_14_0_SPARE_BIT_14_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_15

// Register FUSE_SPARE_BIT_15_0  
#define FUSE_SPARE_BIT_15_0                     _MK_ADDR_CONST(0x23c)
#define FUSE_SPARE_BIT_15_0_SECURE                      0x0
#define FUSE_SPARE_BIT_15_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_15_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_15_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_15_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_15_0_SPARE_BIT_15_SHIFT)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_RANGE                  0:0
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_WOFFSET                        0x0
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_15_0_SPARE_BIT_15_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_16

// Register FUSE_SPARE_BIT_16_0  
#define FUSE_SPARE_BIT_16_0                     _MK_ADDR_CONST(0x240)
#define FUSE_SPARE_BIT_16_0_SECURE                      0x0
#define FUSE_SPARE_BIT_16_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_16_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_16_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_16_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_16_0_SPARE_BIT_16_SHIFT)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_RANGE                  0:0
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_WOFFSET                        0x0
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_16_0_SPARE_BIT_16_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_17

// Register FUSE_SPARE_BIT_17_0  
#define FUSE_SPARE_BIT_17_0                     _MK_ADDR_CONST(0x244)
#define FUSE_SPARE_BIT_17_0_SECURE                      0x0
#define FUSE_SPARE_BIT_17_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_17_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_17_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_17_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_17_0_SPARE_BIT_17_SHIFT)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_RANGE                  0:0
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_WOFFSET                        0x0
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_17_0_SPARE_BIT_17_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_18

// Register FUSE_SPARE_BIT_18_0  
#define FUSE_SPARE_BIT_18_0                     _MK_ADDR_CONST(0x248)
#define FUSE_SPARE_BIT_18_0_SECURE                      0x0
#define FUSE_SPARE_BIT_18_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_18_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_18_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_18_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_18_0_SPARE_BIT_18_SHIFT)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_RANGE                  0:0
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_WOFFSET                        0x0
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_18_0_SPARE_BIT_18_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_19

// Register FUSE_SPARE_BIT_19_0  
#define FUSE_SPARE_BIT_19_0                     _MK_ADDR_CONST(0x24c)
#define FUSE_SPARE_BIT_19_0_SECURE                      0x0
#define FUSE_SPARE_BIT_19_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_19_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_19_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_19_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_19_0_SPARE_BIT_19_SHIFT)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_RANGE                  0:0
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_WOFFSET                        0x0
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_19_0_SPARE_BIT_19_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_20

// Register FUSE_SPARE_BIT_20_0  
#define FUSE_SPARE_BIT_20_0                     _MK_ADDR_CONST(0x250)
#define FUSE_SPARE_BIT_20_0_SECURE                      0x0
#define FUSE_SPARE_BIT_20_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_20_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_20_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_20_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_20_0_SPARE_BIT_20_SHIFT)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_RANGE                  0:0
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_WOFFSET                        0x0
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_20_0_SPARE_BIT_20_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_21

// Register FUSE_SPARE_BIT_21_0  
#define FUSE_SPARE_BIT_21_0                     _MK_ADDR_CONST(0x254)
#define FUSE_SPARE_BIT_21_0_SECURE                      0x0
#define FUSE_SPARE_BIT_21_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_21_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_21_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_21_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_21_0_SPARE_BIT_21_SHIFT)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_RANGE                  0:0
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_WOFFSET                        0x0
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_21_0_SPARE_BIT_21_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_22

// Register FUSE_SPARE_BIT_22_0  
#define FUSE_SPARE_BIT_22_0                     _MK_ADDR_CONST(0x258)
#define FUSE_SPARE_BIT_22_0_SECURE                      0x0
#define FUSE_SPARE_BIT_22_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_22_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_22_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_22_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_22_0_SPARE_BIT_22_SHIFT)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_RANGE                  0:0
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_WOFFSET                        0x0
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_22_0_SPARE_BIT_22_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_23

// Register FUSE_SPARE_BIT_23_0  
#define FUSE_SPARE_BIT_23_0                     _MK_ADDR_CONST(0x25c)
#define FUSE_SPARE_BIT_23_0_SECURE                      0x0
#define FUSE_SPARE_BIT_23_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_23_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_23_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_23_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_23_0_SPARE_BIT_23_SHIFT)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_RANGE                  0:0
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_WOFFSET                        0x0
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_23_0_SPARE_BIT_23_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_24

// Register FUSE_SPARE_BIT_24_0  
#define FUSE_SPARE_BIT_24_0                     _MK_ADDR_CONST(0x260)
#define FUSE_SPARE_BIT_24_0_SECURE                      0x0
#define FUSE_SPARE_BIT_24_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_24_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_24_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_24_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_24_0_SPARE_BIT_24_SHIFT)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_RANGE                  0:0
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_WOFFSET                        0x0
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_24_0_SPARE_BIT_24_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_25

// Register FUSE_SPARE_BIT_25_0  
#define FUSE_SPARE_BIT_25_0                     _MK_ADDR_CONST(0x264)
#define FUSE_SPARE_BIT_25_0_SECURE                      0x0
#define FUSE_SPARE_BIT_25_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_25_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_25_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_25_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_25_0_SPARE_BIT_25_SHIFT)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_RANGE                  0:0
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_WOFFSET                        0x0
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_25_0_SPARE_BIT_25_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_26

// Register FUSE_SPARE_BIT_26_0  
#define FUSE_SPARE_BIT_26_0                     _MK_ADDR_CONST(0x268)
#define FUSE_SPARE_BIT_26_0_SECURE                      0x0
#define FUSE_SPARE_BIT_26_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_26_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_26_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_26_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_26_0_SPARE_BIT_26_SHIFT)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_RANGE                  0:0
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_WOFFSET                        0x0
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_26_0_SPARE_BIT_26_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_27

// Register FUSE_SPARE_BIT_27_0  
#define FUSE_SPARE_BIT_27_0                     _MK_ADDR_CONST(0x26c)
#define FUSE_SPARE_BIT_27_0_SECURE                      0x0
#define FUSE_SPARE_BIT_27_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_27_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_27_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_27_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_27_0_SPARE_BIT_27_SHIFT)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_RANGE                  0:0
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_WOFFSET                        0x0
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_27_0_SPARE_BIT_27_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_28

// Register FUSE_SPARE_BIT_28_0  
#define FUSE_SPARE_BIT_28_0                     _MK_ADDR_CONST(0x270)
#define FUSE_SPARE_BIT_28_0_SECURE                      0x0
#define FUSE_SPARE_BIT_28_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_28_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_28_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_28_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_28_0_SPARE_BIT_28_SHIFT)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_RANGE                  0:0
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_WOFFSET                        0x0
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_28_0_SPARE_BIT_28_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_29

// Register FUSE_SPARE_BIT_29_0  
#define FUSE_SPARE_BIT_29_0                     _MK_ADDR_CONST(0x274)
#define FUSE_SPARE_BIT_29_0_SECURE                      0x0
#define FUSE_SPARE_BIT_29_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_29_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_29_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_29_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_29_0_SPARE_BIT_29_SHIFT)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_RANGE                  0:0
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_WOFFSET                        0x0
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_29_0_SPARE_BIT_29_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_30

// Register FUSE_SPARE_BIT_30_0  
#define FUSE_SPARE_BIT_30_0                     _MK_ADDR_CONST(0x278)
#define FUSE_SPARE_BIT_30_0_SECURE                      0x0
#define FUSE_SPARE_BIT_30_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_30_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_30_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_30_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_30_0_SPARE_BIT_30_SHIFT)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_RANGE                  0:0
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_WOFFSET                        0x0
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_30_0_SPARE_BIT_30_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_31

// Register FUSE_SPARE_BIT_31_0  
#define FUSE_SPARE_BIT_31_0                     _MK_ADDR_CONST(0x27c)
#define FUSE_SPARE_BIT_31_0_SECURE                      0x0
#define FUSE_SPARE_BIT_31_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_31_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_31_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_31_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_31_0_SPARE_BIT_31_SHIFT)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_RANGE                  0:0
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_WOFFSET                        0x0
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_31_0_SPARE_BIT_31_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_32

// Register FUSE_SPARE_BIT_32_0  
#define FUSE_SPARE_BIT_32_0                     _MK_ADDR_CONST(0x280)
#define FUSE_SPARE_BIT_32_0_SECURE                      0x0
#define FUSE_SPARE_BIT_32_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_32_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_32_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_32_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_32_0_SPARE_BIT_32_SHIFT)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_RANGE                  0:0
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_WOFFSET                        0x0
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_32_0_SPARE_BIT_32_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_33

// Register FUSE_SPARE_BIT_33_0  
#define FUSE_SPARE_BIT_33_0                     _MK_ADDR_CONST(0x284)
#define FUSE_SPARE_BIT_33_0_SECURE                      0x0
#define FUSE_SPARE_BIT_33_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_33_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_33_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_33_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_33_0_SPARE_BIT_33_SHIFT)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_RANGE                  0:0
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_WOFFSET                        0x0
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_33_0_SPARE_BIT_33_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_34

// Register FUSE_SPARE_BIT_34_0  
#define FUSE_SPARE_BIT_34_0                     _MK_ADDR_CONST(0x288)
#define FUSE_SPARE_BIT_34_0_SECURE                      0x0
#define FUSE_SPARE_BIT_34_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_34_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_34_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_34_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_34_0_SPARE_BIT_34_SHIFT)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_RANGE                  0:0
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_WOFFSET                        0x0
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_34_0_SPARE_BIT_34_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_35

// Register FUSE_SPARE_BIT_35_0  
#define FUSE_SPARE_BIT_35_0                     _MK_ADDR_CONST(0x28c)
#define FUSE_SPARE_BIT_35_0_SECURE                      0x0
#define FUSE_SPARE_BIT_35_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_35_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_35_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_35_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_35_0_SPARE_BIT_35_SHIFT)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_RANGE                  0:0
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_WOFFSET                        0x0
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_35_0_SPARE_BIT_35_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_36

// Register FUSE_SPARE_BIT_36_0  
#define FUSE_SPARE_BIT_36_0                     _MK_ADDR_CONST(0x290)
#define FUSE_SPARE_BIT_36_0_SECURE                      0x0
#define FUSE_SPARE_BIT_36_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_36_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_36_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_36_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_36_0_SPARE_BIT_36_SHIFT)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_RANGE                  0:0
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_WOFFSET                        0x0
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_36_0_SPARE_BIT_36_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_37

// Register FUSE_SPARE_BIT_37_0  
#define FUSE_SPARE_BIT_37_0                     _MK_ADDR_CONST(0x294)
#define FUSE_SPARE_BIT_37_0_SECURE                      0x0
#define FUSE_SPARE_BIT_37_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_37_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_37_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_37_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_37_0_SPARE_BIT_37_SHIFT)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_RANGE                  0:0
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_WOFFSET                        0x0
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_37_0_SPARE_BIT_37_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_38

// Register FUSE_SPARE_BIT_38_0  
#define FUSE_SPARE_BIT_38_0                     _MK_ADDR_CONST(0x298)
#define FUSE_SPARE_BIT_38_0_SECURE                      0x0
#define FUSE_SPARE_BIT_38_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_38_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_38_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_38_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_38_0_SPARE_BIT_38_SHIFT)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_RANGE                  0:0
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_WOFFSET                        0x0
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_38_0_SPARE_BIT_38_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_39

// Register FUSE_SPARE_BIT_39_0  
#define FUSE_SPARE_BIT_39_0                     _MK_ADDR_CONST(0x29c)
#define FUSE_SPARE_BIT_39_0_SECURE                      0x0
#define FUSE_SPARE_BIT_39_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_39_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_39_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_39_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_39_0_SPARE_BIT_39_SHIFT)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_RANGE                  0:0
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_WOFFSET                        0x0
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_39_0_SPARE_BIT_39_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_40

// Register FUSE_SPARE_BIT_40_0  
#define FUSE_SPARE_BIT_40_0                     _MK_ADDR_CONST(0x2a0)
#define FUSE_SPARE_BIT_40_0_SECURE                      0x0
#define FUSE_SPARE_BIT_40_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_40_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_40_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_40_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_40_0_SPARE_BIT_40_SHIFT)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_RANGE                  0:0
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_WOFFSET                        0x0
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_40_0_SPARE_BIT_40_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_41

// Register FUSE_SPARE_BIT_41_0  
#define FUSE_SPARE_BIT_41_0                     _MK_ADDR_CONST(0x2a4)
#define FUSE_SPARE_BIT_41_0_SECURE                      0x0
#define FUSE_SPARE_BIT_41_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_41_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_41_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_41_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_41_0_SPARE_BIT_41_SHIFT)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_RANGE                  0:0
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_WOFFSET                        0x0
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_41_0_SPARE_BIT_41_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_42

// Register FUSE_SPARE_BIT_42_0  
#define FUSE_SPARE_BIT_42_0                     _MK_ADDR_CONST(0x2a8)
#define FUSE_SPARE_BIT_42_0_SECURE                      0x0
#define FUSE_SPARE_BIT_42_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_42_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_42_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_42_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_42_0_SPARE_BIT_42_SHIFT)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_RANGE                  0:0
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_WOFFSET                        0x0
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_42_0_SPARE_BIT_42_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_43

// Register FUSE_SPARE_BIT_43_0  
#define FUSE_SPARE_BIT_43_0                     _MK_ADDR_CONST(0x2ac)
#define FUSE_SPARE_BIT_43_0_SECURE                      0x0
#define FUSE_SPARE_BIT_43_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_43_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_43_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_43_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_43_0_SPARE_BIT_43_SHIFT)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_RANGE                  0:0
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_WOFFSET                        0x0
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_43_0_SPARE_BIT_43_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_44

// Register FUSE_SPARE_BIT_44_0  
#define FUSE_SPARE_BIT_44_0                     _MK_ADDR_CONST(0x2b0)
#define FUSE_SPARE_BIT_44_0_SECURE                      0x0
#define FUSE_SPARE_BIT_44_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_44_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_44_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_44_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_44_0_SPARE_BIT_44_SHIFT)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_RANGE                  0:0
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_WOFFSET                        0x0
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_44_0_SPARE_BIT_44_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_45

// Register FUSE_SPARE_BIT_45_0  
#define FUSE_SPARE_BIT_45_0                     _MK_ADDR_CONST(0x2b4)
#define FUSE_SPARE_BIT_45_0_SECURE                      0x0
#define FUSE_SPARE_BIT_45_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_45_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_45_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_45_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_45_0_SPARE_BIT_45_SHIFT)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_RANGE                  0:0
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_WOFFSET                        0x0
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_45_0_SPARE_BIT_45_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_46

// Register FUSE_SPARE_BIT_46_0  
#define FUSE_SPARE_BIT_46_0                     _MK_ADDR_CONST(0x2b8)
#define FUSE_SPARE_BIT_46_0_SECURE                      0x0
#define FUSE_SPARE_BIT_46_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_46_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_46_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_46_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_46_0_SPARE_BIT_46_SHIFT)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_RANGE                  0:0
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_WOFFSET                        0x0
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_46_0_SPARE_BIT_46_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_47

// Register FUSE_SPARE_BIT_47_0  
#define FUSE_SPARE_BIT_47_0                     _MK_ADDR_CONST(0x2bc)
#define FUSE_SPARE_BIT_47_0_SECURE                      0x0
#define FUSE_SPARE_BIT_47_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_47_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_47_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_47_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_47_0_SPARE_BIT_47_SHIFT)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_RANGE                  0:0
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_WOFFSET                        0x0
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_47_0_SPARE_BIT_47_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_48

// Register FUSE_SPARE_BIT_48_0  
#define FUSE_SPARE_BIT_48_0                     _MK_ADDR_CONST(0x2c0)
#define FUSE_SPARE_BIT_48_0_SECURE                      0x0
#define FUSE_SPARE_BIT_48_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_48_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_48_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_48_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_48_0_SPARE_BIT_48_SHIFT)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_RANGE                  0:0
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_WOFFSET                        0x0
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_48_0_SPARE_BIT_48_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_49

// Register FUSE_SPARE_BIT_49_0  
#define FUSE_SPARE_BIT_49_0                     _MK_ADDR_CONST(0x2c4)
#define FUSE_SPARE_BIT_49_0_SECURE                      0x0
#define FUSE_SPARE_BIT_49_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_49_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_49_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_49_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_49_0_SPARE_BIT_49_SHIFT)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_RANGE                  0:0
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_WOFFSET                        0x0
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_49_0_SPARE_BIT_49_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_50

// Register FUSE_SPARE_BIT_50_0  
#define FUSE_SPARE_BIT_50_0                     _MK_ADDR_CONST(0x2c8)
#define FUSE_SPARE_BIT_50_0_SECURE                      0x0
#define FUSE_SPARE_BIT_50_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_50_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_50_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_50_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_50_0_SPARE_BIT_50_SHIFT)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_RANGE                  0:0
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_WOFFSET                        0x0
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_50_0_SPARE_BIT_50_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_51

// Register FUSE_SPARE_BIT_51_0  
#define FUSE_SPARE_BIT_51_0                     _MK_ADDR_CONST(0x2cc)
#define FUSE_SPARE_BIT_51_0_SECURE                      0x0
#define FUSE_SPARE_BIT_51_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_51_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_51_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_51_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_51_0_SPARE_BIT_51_SHIFT)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_RANGE                  0:0
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_WOFFSET                        0x0
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_51_0_SPARE_BIT_51_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_52

// Register FUSE_SPARE_BIT_52_0  
#define FUSE_SPARE_BIT_52_0                     _MK_ADDR_CONST(0x2d0)
#define FUSE_SPARE_BIT_52_0_SECURE                      0x0
#define FUSE_SPARE_BIT_52_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_52_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_52_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_52_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_52_0_SPARE_BIT_52_SHIFT)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_RANGE                  0:0
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_WOFFSET                        0x0
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_52_0_SPARE_BIT_52_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_53

// Register FUSE_SPARE_BIT_53_0  
#define FUSE_SPARE_BIT_53_0                     _MK_ADDR_CONST(0x2d4)
#define FUSE_SPARE_BIT_53_0_SECURE                      0x0
#define FUSE_SPARE_BIT_53_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_53_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_53_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_53_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_53_0_SPARE_BIT_53_SHIFT)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_RANGE                  0:0
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_WOFFSET                        0x0
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_53_0_SPARE_BIT_53_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_54

// Register FUSE_SPARE_BIT_54_0  
#define FUSE_SPARE_BIT_54_0                     _MK_ADDR_CONST(0x2d8)
#define FUSE_SPARE_BIT_54_0_SECURE                      0x0
#define FUSE_SPARE_BIT_54_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_54_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_54_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_54_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_54_0_SPARE_BIT_54_SHIFT)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_RANGE                  0:0
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_WOFFSET                        0x0
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_54_0_SPARE_BIT_54_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_55

// Register FUSE_SPARE_BIT_55_0  
#define FUSE_SPARE_BIT_55_0                     _MK_ADDR_CONST(0x2dc)
#define FUSE_SPARE_BIT_55_0_SECURE                      0x0
#define FUSE_SPARE_BIT_55_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_55_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_55_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_55_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_55_0_SPARE_BIT_55_SHIFT)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_RANGE                  0:0
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_WOFFSET                        0x0
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_55_0_SPARE_BIT_55_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_56

// Register FUSE_SPARE_BIT_56_0  
#define FUSE_SPARE_BIT_56_0                     _MK_ADDR_CONST(0x2e0)
#define FUSE_SPARE_BIT_56_0_SECURE                      0x0
#define FUSE_SPARE_BIT_56_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_56_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_56_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_56_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_56_0_SPARE_BIT_56_SHIFT)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_RANGE                  0:0
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_WOFFSET                        0x0
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_56_0_SPARE_BIT_56_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_57

// Register FUSE_SPARE_BIT_57_0  
#define FUSE_SPARE_BIT_57_0                     _MK_ADDR_CONST(0x2e4)
#define FUSE_SPARE_BIT_57_0_SECURE                      0x0
#define FUSE_SPARE_BIT_57_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_57_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_57_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_57_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_57_0_SPARE_BIT_57_SHIFT)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_RANGE                  0:0
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_WOFFSET                        0x0
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_57_0_SPARE_BIT_57_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_58

// Register FUSE_SPARE_BIT_58_0  
#define FUSE_SPARE_BIT_58_0                     _MK_ADDR_CONST(0x2e8)
#define FUSE_SPARE_BIT_58_0_SECURE                      0x0
#define FUSE_SPARE_BIT_58_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_58_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_58_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_58_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_58_0_SPARE_BIT_58_SHIFT)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_RANGE                  0:0
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_WOFFSET                        0x0
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_58_0_SPARE_BIT_58_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_59

// Register FUSE_SPARE_BIT_59_0  
#define FUSE_SPARE_BIT_59_0                     _MK_ADDR_CONST(0x2ec)
#define FUSE_SPARE_BIT_59_0_SECURE                      0x0
#define FUSE_SPARE_BIT_59_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_59_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_59_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_59_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_59_0_SPARE_BIT_59_SHIFT)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_RANGE                  0:0
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_WOFFSET                        0x0
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_59_0_SPARE_BIT_59_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_60

// Register FUSE_SPARE_BIT_60_0  
#define FUSE_SPARE_BIT_60_0                     _MK_ADDR_CONST(0x2f0)
#define FUSE_SPARE_BIT_60_0_SECURE                      0x0
#define FUSE_SPARE_BIT_60_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_60_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_60_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_60_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_60_0_SPARE_BIT_60_SHIFT)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_RANGE                  0:0
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_WOFFSET                        0x0
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_60_0_SPARE_BIT_60_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

// Chip Option: spare_bit_61

// Register FUSE_SPARE_BIT_61_0  
#define FUSE_SPARE_BIT_61_0                     _MK_ADDR_CONST(0x2f4)
#define FUSE_SPARE_BIT_61_0_SECURE                      0x0
#define FUSE_SPARE_BIT_61_0_WORD_COUNT                  0x1
#define FUSE_SPARE_BIT_61_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_RESET_MASK                  _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_61_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_READ_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_61_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_SHIFT                  _MK_SHIFT_CONST(0)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_FIELD                  (_MK_MASK_CONST(0x1) << FUSE_SPARE_BIT_61_0_SPARE_BIT_61_SHIFT)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_RANGE                  0:0
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_WOFFSET                        0x0
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_DEFAULT                        _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define FUSE_SPARE_BIT_61_0_SPARE_BIT_61_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

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

#endif // ifndef ___ARFUSE_PUBLIC_H_INC_
