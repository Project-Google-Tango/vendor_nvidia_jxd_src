//
// Copyright (c) 2013 NVIDIA Corporation.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the NVIDIA Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

//
// DO NOT EDIT - generated by simspec!
//

#ifndef ___ARKFUSE_H_INC_
#define ___ARKFUSE_H_INC_

// Register KFUSE_FUSECTRL_0
#define KFUSE_FUSECTRL_0                        _MK_ADDR_CONST(0x0)
#define KFUSE_FUSECTRL_0_SECURE                         0x0
#define KFUSE_FUSECTRL_0_WORD_COUNT                     0x1
#define KFUSE_FUSECTRL_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_RESET_MASK                     _MK_MASK_CONST(0x3)
#define KFUSE_FUSECTRL_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_READ_MASK                      _MK_MASK_CONST(0xf0003)
#define KFUSE_FUSECTRL_0_WRITE_MASK                     _MK_MASK_CONST(0x3)
#define KFUSE_FUSECTRL_0_CMD_SHIFT                      _MK_SHIFT_CONST(0)
#define KFUSE_FUSECTRL_0_CMD_FIELD                      _MK_FIELD_CONST(0x3, KFUSE_FUSECTRL_0_CMD_SHIFT)
#define KFUSE_FUSECTRL_0_CMD_RANGE                      1:0
#define KFUSE_FUSECTRL_0_CMD_WOFFSET                    0x0
#define KFUSE_FUSECTRL_0_CMD_DEFAULT                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_CMD_DEFAULT_MASK                       _MK_MASK_CONST(0x3)
#define KFUSE_FUSECTRL_0_CMD_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_CMD_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_CMD_IDLE                       _MK_ENUM_CONST(0)
#define KFUSE_FUSECTRL_0_CMD_READ                       _MK_ENUM_CONST(1)
#define KFUSE_FUSECTRL_0_CMD_WRITE                      _MK_ENUM_CONST(2)
#define KFUSE_FUSECTRL_0_CMD_VERIFY                     _MK_ENUM_CONST(3)

#define KFUSE_FUSECTRL_0_STATE_SHIFT                    _MK_SHIFT_CONST(16)
#define KFUSE_FUSECTRL_0_STATE_FIELD                    _MK_FIELD_CONST(0xf, KFUSE_FUSECTRL_0_STATE_SHIFT)
#define KFUSE_FUSECTRL_0_STATE_RANGE                    19:16
#define KFUSE_FUSECTRL_0_STATE_WOFFSET                  0x0
#define KFUSE_FUSECTRL_0_STATE_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_STATE_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_STATE_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_STATE_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define KFUSE_FUSECTRL_0_STATE_IN_RESET                 _MK_ENUM_CONST(0)
#define KFUSE_FUSECTRL_0_STATE_POST_RESET                       _MK_ENUM_CONST(1)
#define KFUSE_FUSECTRL_0_STATE_IDLE                     _MK_ENUM_CONST(2)
#define KFUSE_FUSECTRL_0_STATE_READ_SETUP                       _MK_ENUM_CONST(3)
#define KFUSE_FUSECTRL_0_STATE_READ_STROBE                      _MK_ENUM_CONST(4)
#define KFUSE_FUSECTRL_0_STATE_SAMPLE_FUSES                     _MK_ENUM_CONST(5)
#define KFUSE_FUSECTRL_0_STATE_READ_OHLD                        _MK_ENUM_CONST(6)
#define KFUSE_FUSECTRL_0_STATE_WRITE_SETUP                      _MK_ENUM_CONST(7)
#define KFUSE_FUSECTRL_0_STATE_WRITE_ADDR_SETUP                 _MK_ENUM_CONST(8)
#define KFUSE_FUSECTRL_0_STATE_WRITE_PROGRAM                    _MK_ENUM_CONST(9)
#define KFUSE_FUSECTRL_0_STATE_WRITE_ADDR_HOLD                  _MK_ENUM_CONST(10)


// Register KFUSE_FUSEADDR_0
#define KFUSE_FUSEADDR_0                        _MK_ADDR_CONST(0x4)
#define KFUSE_FUSEADDR_0_SECURE                         0x0
#define KFUSE_FUSEADDR_0_WORD_COUNT                     0x1
#define KFUSE_FUSEADDR_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define KFUSE_FUSEADDR_0_RESET_MASK                     _MK_MASK_CONST(0xff)
#define KFUSE_FUSEADDR_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define KFUSE_FUSEADDR_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSEADDR_0_READ_MASK                      _MK_MASK_CONST(0xff)
#define KFUSE_FUSEADDR_0_WRITE_MASK                     _MK_MASK_CONST(0xff)
#define KFUSE_FUSEADDR_0_ADDR_SHIFT                     _MK_SHIFT_CONST(0)
#define KFUSE_FUSEADDR_0_ADDR_FIELD                     _MK_FIELD_CONST(0xff, KFUSE_FUSEADDR_0_ADDR_SHIFT)
#define KFUSE_FUSEADDR_0_ADDR_RANGE                     7:0
#define KFUSE_FUSEADDR_0_ADDR_WOFFSET                   0x0
#define KFUSE_FUSEADDR_0_ADDR_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSEADDR_0_ADDR_DEFAULT_MASK                      _MK_MASK_CONST(0xff)
#define KFUSE_FUSEADDR_0_ADDR_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSEADDR_0_ADDR_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register KFUSE_FUSEDATA0_0
#define KFUSE_FUSEDATA0_0                       _MK_ADDR_CONST(0x8)
#define KFUSE_FUSEDATA0_0_SECURE                        0x0
#define KFUSE_FUSEDATA0_0_WORD_COUNT                    0x1
#define KFUSE_FUSEDATA0_0_RESET_VAL                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_RESET_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSEDATA0_0_WRITE_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_DATA_SHIFT                    _MK_SHIFT_CONST(0)
#define KFUSE_FUSEDATA0_0_DATA_FIELD                    _MK_FIELD_CONST(0xffffffff, KFUSE_FUSEDATA0_0_DATA_SHIFT)
#define KFUSE_FUSEDATA0_0_DATA_RANGE                    31:0
#define KFUSE_FUSEDATA0_0_DATA_WOFFSET                  0x0
#define KFUSE_FUSEDATA0_0_DATA_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_DATA_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_DATA_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSEDATA0_0_DATA_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)


// Register KFUSE_FUSEWRDATA0_0
#define KFUSE_FUSEWRDATA0_0                     _MK_ADDR_CONST(0xc)
#define KFUSE_FUSEWRDATA0_0_SECURE                      0x0
#define KFUSE_FUSEWRDATA0_0_WORD_COUNT                  0x1
#define KFUSE_FUSEWRDATA0_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_RESET_MASK                  _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSEWRDATA0_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_READ_MASK                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_WRITE_MASK                  _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSEWRDATA0_0_WRDATA_SHIFT                        _MK_SHIFT_CONST(0)
#define KFUSE_FUSEWRDATA0_0_WRDATA_FIELD                        _MK_FIELD_CONST(0xffffffff, KFUSE_FUSEWRDATA0_0_WRDATA_SHIFT)
#define KFUSE_FUSEWRDATA0_0_WRDATA_RANGE                        31:0
#define KFUSE_FUSEWRDATA0_0_WRDATA_WOFFSET                      0x0
#define KFUSE_FUSEWRDATA0_0_WRDATA_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_WRDATA_DEFAULT_MASK                 _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSEWRDATA0_0_WRDATA_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSEWRDATA0_0_WRDATA_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)


// Register KFUSE_FUSETIME_RD1_0
#define KFUSE_FUSETIME_RD1_0                    _MK_ADDR_CONST(0x10)
#define KFUSE_FUSETIME_RD1_0_SECURE                     0x0
#define KFUSE_FUSETIME_RD1_0_WORD_COUNT                         0x1
#define KFUSE_FUSETIME_RD1_0_RESET_VAL                  _MK_MASK_CONST(0x20202)
#define KFUSE_FUSETIME_RD1_0_RESET_MASK                         _MK_MASK_CONST(0xffffff)
#define KFUSE_FUSETIME_RD1_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD1_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD1_0_READ_MASK                  _MK_MASK_CONST(0xffffff)
#define KFUSE_FUSETIME_RD1_0_WRITE_MASK                         _MK_MASK_CONST(0xffffff)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_SHIFT                     _MK_SHIFT_CONST(0)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_FIELD                     _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_RD1_0_TSUR_MAX_SHIFT)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_RANGE                     7:0
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_WOFFSET                   0x0
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_DEFAULT                   _MK_MASK_CONST(0x2)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_DEFAULT_MASK                      _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD1_0_TSUR_MAX_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_SHIFT                 _MK_SHIFT_CONST(8)
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_FIELD                 _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_SHIFT)
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_RANGE                 15:8
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_WOFFSET                       0x0
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_DEFAULT                       _MK_MASK_CONST(0x2)
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_DEFAULT_MASK                  _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD1_0_TSUR_FUSEOUT_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_RD1_0_THR_MAX_SHIFT                      _MK_SHIFT_CONST(16)
#define KFUSE_FUSETIME_RD1_0_THR_MAX_FIELD                      _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_RD1_0_THR_MAX_SHIFT)
#define KFUSE_FUSETIME_RD1_0_THR_MAX_RANGE                      23:16
#define KFUSE_FUSETIME_RD1_0_THR_MAX_WOFFSET                    0x0
#define KFUSE_FUSETIME_RD1_0_THR_MAX_DEFAULT                    _MK_MASK_CONST(0x2)
#define KFUSE_FUSETIME_RD1_0_THR_MAX_DEFAULT_MASK                       _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_RD1_0_THR_MAX_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD1_0_THR_MAX_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register KFUSE_FUSETIME_RD2_0
#define KFUSE_FUSETIME_RD2_0                    _MK_ADDR_CONST(0x14)
#define KFUSE_FUSETIME_RD2_0_SECURE                     0x0
#define KFUSE_FUSETIME_RD2_0_WORD_COUNT                         0x1
#define KFUSE_FUSETIME_RD2_0_RESET_VAL                  _MK_MASK_CONST(0x7)
#define KFUSE_FUSETIME_RD2_0_RESET_MASK                         _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_RD2_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD2_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD2_0_READ_MASK                  _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_RD2_0_WRITE_MASK                         _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_SHIFT                    _MK_SHIFT_CONST(0)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_FIELD                    _MK_FIELD_CONST(0xffff, KFUSE_FUSETIME_RD2_0_TWIDTH_RD_SHIFT)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_RANGE                    15:0
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_WOFFSET                  0x0
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_DEFAULT                  _MK_MASK_CONST(0x7)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_DEFAULT_MASK                     _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_RD2_0_TWIDTH_RD_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)


// Register KFUSE_FUSETIME_PGM1_0
#define KFUSE_FUSETIME_PGM1_0                   _MK_ADDR_CONST(0x18)
#define KFUSE_FUSETIME_PGM1_0_SECURE                    0x0
#define KFUSE_FUSETIME_PGM1_0_WORD_COUNT                        0x1
#define KFUSE_FUSETIME_PGM1_0_RESET_VAL                         _MK_MASK_CONST(0x4010101)
#define KFUSE_FUSETIME_PGM1_0_RESET_MASK                        _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM1_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_READ_MASK                         _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM1_0_WRITE_MASK                        _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_SHIFT                    _MK_SHIFT_CONST(0)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_FIELD                    _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_PGM1_0_TSUP_MAX_SHIFT)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_RANGE                    7:0
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_WOFFSET                  0x0
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_DEFAULT                  _MK_MASK_CONST(0x1)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_DEFAULT_MASK                     _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_TSUP_MAX_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_SHIFT                   _MK_SHIFT_CONST(8)
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_FIELD                   _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_SHIFT)
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_RANGE                   15:8
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_WOFFSET                 0x0
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_DEFAULT                 _MK_MASK_CONST(0x1)
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_DEFAULT_MASK                    _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_TSUP_ADDR_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_SHIFT                    _MK_SHIFT_CONST(16)
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_FIELD                    _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_PGM1_0_THP_ADDR_SHIFT)
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_RANGE                    23:16
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_WOFFSET                  0x0
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_DEFAULT                  _MK_MASK_CONST(0x1)
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_DEFAULT_MASK                     _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_THP_ADDR_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_PGM1_0_THP_PS_SHIFT                      _MK_SHIFT_CONST(24)
#define KFUSE_FUSETIME_PGM1_0_THP_PS_FIELD                      _MK_FIELD_CONST(0xff, KFUSE_FUSETIME_PGM1_0_THP_PS_SHIFT)
#define KFUSE_FUSETIME_PGM1_0_THP_PS_RANGE                      31:24
#define KFUSE_FUSETIME_PGM1_0_THP_PS_WOFFSET                    0x0
#define KFUSE_FUSETIME_PGM1_0_THP_PS_DEFAULT                    _MK_MASK_CONST(0x4)
#define KFUSE_FUSETIME_PGM1_0_THP_PS_DEFAULT_MASK                       _MK_MASK_CONST(0xff)
#define KFUSE_FUSETIME_PGM1_0_THP_PS_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM1_0_THP_PS_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register KFUSE_FUSETIME_PGM2_0
#define KFUSE_FUSETIME_PGM2_0                   _MK_ADDR_CONST(0x1c)
#define KFUSE_FUSETIME_PGM2_0_SECURE                    0x0
#define KFUSE_FUSETIME_PGM2_0_WORD_COUNT                        0x1
#define KFUSE_FUSETIME_PGM2_0_RESET_VAL                         _MK_MASK_CONST(0x40241)
#define KFUSE_FUSETIME_PGM2_0_RESET_MASK                        _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM2_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM2_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM2_0_READ_MASK                         _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM2_0_WRITE_MASK                        _MK_MASK_CONST(0xffffffff)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_SHIFT                  _MK_SHIFT_CONST(0)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_FIELD                  _MK_FIELD_CONST(0xffff, KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_SHIFT)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_RANGE                  15:0
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_WOFFSET                        0x0
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_DEFAULT                        _MK_MASK_CONST(0x241)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_DEFAULT_MASK                   _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM2_0_TWIDTH_PGM_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_SHIFT                     _MK_SHIFT_CONST(16)
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_FIELD                     _MK_FIELD_CONST(0xffff, KFUSE_FUSETIME_PGM2_0_TSUP_PS_SHIFT)
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_RANGE                     31:16
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_WOFFSET                   0x0
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_DEFAULT                   _MK_MASK_CONST(0x4)
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_DEFAULT_MASK                      _MK_MASK_CONST(0xffff)
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define KFUSE_FUSETIME_PGM2_0_TSUP_PS_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register KFUSE_REGULATOR_0
#define KFUSE_REGULATOR_0                       _MK_ADDR_CONST(0x20)
#define KFUSE_REGULATOR_0_SECURE                        0x0
#define KFUSE_REGULATOR_0_WORD_COUNT                    0x1
#define KFUSE_REGULATOR_0_RESET_VAL                     _MK_MASK_CONST(0x202)
#define KFUSE_REGULATOR_0_RESET_MASK                    _MK_MASK_CONST(0x10303)
#define KFUSE_REGULATOR_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_READ_MASK                     _MK_MASK_CONST(0x10303)
#define KFUSE_REGULATOR_0_WRITE_MASK                    _MK_MASK_CONST(0x10303)
#define KFUSE_REGULATOR_0_REF_CTRL_SHIFT                        _MK_SHIFT_CONST(0)
#define KFUSE_REGULATOR_0_REF_CTRL_FIELD                        _MK_FIELD_CONST(0x3, KFUSE_REGULATOR_0_REF_CTRL_SHIFT)
#define KFUSE_REGULATOR_0_REF_CTRL_RANGE                        1:0
#define KFUSE_REGULATOR_0_REF_CTRL_WOFFSET                      0x0
#define KFUSE_REGULATOR_0_REF_CTRL_DEFAULT                      _MK_MASK_CONST(0x2)
#define KFUSE_REGULATOR_0_REF_CTRL_DEFAULT_MASK                 _MK_MASK_CONST(0x3)
#define KFUSE_REGULATOR_0_REF_CTRL_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_REF_CTRL_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

#define KFUSE_REGULATOR_0_BIAS_CTRL_SHIFT                       _MK_SHIFT_CONST(8)
#define KFUSE_REGULATOR_0_BIAS_CTRL_FIELD                       _MK_FIELD_CONST(0x3, KFUSE_REGULATOR_0_BIAS_CTRL_SHIFT)
#define KFUSE_REGULATOR_0_BIAS_CTRL_RANGE                       9:8
#define KFUSE_REGULATOR_0_BIAS_CTRL_WOFFSET                     0x0
#define KFUSE_REGULATOR_0_BIAS_CTRL_DEFAULT                     _MK_MASK_CONST(0x2)
#define KFUSE_REGULATOR_0_BIAS_CTRL_DEFAULT_MASK                        _MK_MASK_CONST(0x3)
#define KFUSE_REGULATOR_0_BIAS_CTRL_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_BIAS_CTRL_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)

#define KFUSE_REGULATOR_0_PWRGD_SHIFT                   _MK_SHIFT_CONST(16)
#define KFUSE_REGULATOR_0_PWRGD_FIELD                   _MK_FIELD_CONST(0x1, KFUSE_REGULATOR_0_PWRGD_SHIFT)
#define KFUSE_REGULATOR_0_PWRGD_RANGE                   16:16
#define KFUSE_REGULATOR_0_PWRGD_WOFFSET                 0x0
#define KFUSE_REGULATOR_0_PWRGD_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_PWRGD_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define KFUSE_REGULATOR_0_PWRGD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_REGULATOR_0_PWRGD_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register KFUSE_STATE_0
#define KFUSE_STATE_0                   _MK_ADDR_CONST(0x80)
#define KFUSE_STATE_0_SECURE                    0x0
#define KFUSE_STATE_0_WORD_COUNT                        0x1
#define KFUSE_STATE_0_RESET_VAL                         _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_RESET_MASK                        _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_SW_DEFAULT_VAL                    _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_READ_MASK                         _MK_MASK_CONST(0x83033f3f)
#define KFUSE_STATE_0_WRITE_MASK                        _MK_MASK_CONST(0x83000000)
#define KFUSE_STATE_0_CURBLOCK_SHIFT                    _MK_SHIFT_CONST(0)
#define KFUSE_STATE_0_CURBLOCK_FIELD                    _MK_FIELD_CONST(0x3f, KFUSE_STATE_0_CURBLOCK_SHIFT)
#define KFUSE_STATE_0_CURBLOCK_RANGE                    5:0
#define KFUSE_STATE_0_CURBLOCK_WOFFSET                  0x0
#define KFUSE_STATE_0_CURBLOCK_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CURBLOCK_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CURBLOCK_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CURBLOCK_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_ERRBLOCK_SHIFT                    _MK_SHIFT_CONST(8)
#define KFUSE_STATE_0_ERRBLOCK_FIELD                    _MK_FIELD_CONST(0x3f, KFUSE_STATE_0_ERRBLOCK_SHIFT)
#define KFUSE_STATE_0_ERRBLOCK_RANGE                    13:8
#define KFUSE_STATE_0_ERRBLOCK_WOFFSET                  0x0
#define KFUSE_STATE_0_ERRBLOCK_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_ERRBLOCK_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_ERRBLOCK_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_ERRBLOCK_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_DONE_SHIFT                        _MK_SHIFT_CONST(16)
#define KFUSE_STATE_0_DONE_FIELD                        _MK_FIELD_CONST(0x1, KFUSE_STATE_0_DONE_SHIFT)
#define KFUSE_STATE_0_DONE_RANGE                        16:16
#define KFUSE_STATE_0_DONE_WOFFSET                      0x0
#define KFUSE_STATE_0_DONE_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_DONE_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_DONE_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_DONE_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_CRCPASS_SHIFT                     _MK_SHIFT_CONST(17)
#define KFUSE_STATE_0_CRCPASS_FIELD                     _MK_FIELD_CONST(0x1, KFUSE_STATE_0_CRCPASS_SHIFT)
#define KFUSE_STATE_0_CRCPASS_RANGE                     17:17
#define KFUSE_STATE_0_CRCPASS_WOFFSET                   0x0
#define KFUSE_STATE_0_CRCPASS_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CRCPASS_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CRCPASS_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_CRCPASS_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_RESTART_SHIFT                     _MK_SHIFT_CONST(24)
#define KFUSE_STATE_0_RESTART_FIELD                     _MK_FIELD_CONST(0x1, KFUSE_STATE_0_RESTART_SHIFT)
#define KFUSE_STATE_0_RESTART_RANGE                     24:24
#define KFUSE_STATE_0_RESTART_WOFFSET                   0x0
#define KFUSE_STATE_0_RESTART_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_RESTART_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_RESTART_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_RESTART_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_STOP_SHIFT                        _MK_SHIFT_CONST(25)
#define KFUSE_STATE_0_STOP_FIELD                        _MK_FIELD_CONST(0x1, KFUSE_STATE_0_STOP_SHIFT)
#define KFUSE_STATE_0_STOP_RANGE                        25:25
#define KFUSE_STATE_0_STOP_WOFFSET                      0x0
#define KFUSE_STATE_0_STOP_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_STOP_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_STOP_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_STOP_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

#define KFUSE_STATE_0_SOFTRESET_SHIFT                   _MK_SHIFT_CONST(31)
#define KFUSE_STATE_0_SOFTRESET_FIELD                   _MK_FIELD_CONST(0x1, KFUSE_STATE_0_SOFTRESET_SHIFT)
#define KFUSE_STATE_0_SOFTRESET_RANGE                   31:31
#define KFUSE_STATE_0_SOFTRESET_WOFFSET                 0x0
#define KFUSE_STATE_0_SOFTRESET_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_SOFTRESET_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_SOFTRESET_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_STATE_0_SOFTRESET_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register KFUSE_ERRCOUNT_0
#define KFUSE_ERRCOUNT_0                        _MK_ADDR_CONST(0x84)
#define KFUSE_ERRCOUNT_0_SECURE                         0x0
#define KFUSE_ERRCOUNT_0_WORD_COUNT                     0x1
#define KFUSE_ERRCOUNT_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_RESET_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_READ_MASK                      _MK_MASK_CONST(0x7f7f7f7f)
#define KFUSE_ERRCOUNT_0_WRITE_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_1_SHIFT                    _MK_SHIFT_CONST(0)
#define KFUSE_ERRCOUNT_0_ERR_1_FIELD                    _MK_FIELD_CONST(0x7f, KFUSE_ERRCOUNT_0_ERR_1_SHIFT)
#define KFUSE_ERRCOUNT_0_ERR_1_RANGE                    6:0
#define KFUSE_ERRCOUNT_0_ERR_1_WOFFSET                  0x0
#define KFUSE_ERRCOUNT_0_ERR_1_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_1_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_1_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_1_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_ERRCOUNT_0_ERR_2_SHIFT                    _MK_SHIFT_CONST(8)
#define KFUSE_ERRCOUNT_0_ERR_2_FIELD                    _MK_FIELD_CONST(0x7f, KFUSE_ERRCOUNT_0_ERR_2_SHIFT)
#define KFUSE_ERRCOUNT_0_ERR_2_RANGE                    14:8
#define KFUSE_ERRCOUNT_0_ERR_2_WOFFSET                  0x0
#define KFUSE_ERRCOUNT_0_ERR_2_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_2_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_2_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_2_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_ERRCOUNT_0_ERR_3_SHIFT                    _MK_SHIFT_CONST(16)
#define KFUSE_ERRCOUNT_0_ERR_3_FIELD                    _MK_FIELD_CONST(0x7f, KFUSE_ERRCOUNT_0_ERR_3_SHIFT)
#define KFUSE_ERRCOUNT_0_ERR_3_RANGE                    22:16
#define KFUSE_ERRCOUNT_0_ERR_3_WOFFSET                  0x0
#define KFUSE_ERRCOUNT_0_ERR_3_DEFAULT                  _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_3_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_3_SW_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_3_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)

#define KFUSE_ERRCOUNT_0_ERR_FATAL_SHIFT                        _MK_SHIFT_CONST(24)
#define KFUSE_ERRCOUNT_0_ERR_FATAL_FIELD                        _MK_FIELD_CONST(0x7f, KFUSE_ERRCOUNT_0_ERR_FATAL_SHIFT)
#define KFUSE_ERRCOUNT_0_ERR_FATAL_RANGE                        30:24
#define KFUSE_ERRCOUNT_0_ERR_FATAL_WOFFSET                      0x0
#define KFUSE_ERRCOUNT_0_ERR_FATAL_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_FATAL_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_FATAL_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define KFUSE_ERRCOUNT_0_ERR_FATAL_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)


// Register KFUSE_KEYADDR_0
#define KFUSE_KEYADDR_0                 _MK_ADDR_CONST(0x88)
#define KFUSE_KEYADDR_0_SECURE                  0x0
#define KFUSE_KEYADDR_0_WORD_COUNT                      0x1
#define KFUSE_KEYADDR_0_RESET_VAL                       _MK_MASK_CONST(0x10000)
#define KFUSE_KEYADDR_0_RESET_MASK                      _MK_MASK_CONST(0x100ff)
#define KFUSE_KEYADDR_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define KFUSE_KEYADDR_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define KFUSE_KEYADDR_0_READ_MASK                       _MK_MASK_CONST(0x100ff)
#define KFUSE_KEYADDR_0_WRITE_MASK                      _MK_MASK_CONST(0x100ff)
#define KFUSE_KEYADDR_0_ADDR_SHIFT                      _MK_SHIFT_CONST(0)
#define KFUSE_KEYADDR_0_ADDR_FIELD                      _MK_FIELD_CONST(0xff, KFUSE_KEYADDR_0_ADDR_SHIFT)
#define KFUSE_KEYADDR_0_ADDR_RANGE                      7:0
#define KFUSE_KEYADDR_0_ADDR_WOFFSET                    0x0
#define KFUSE_KEYADDR_0_ADDR_DEFAULT                    _MK_MASK_CONST(0x0)
#define KFUSE_KEYADDR_0_ADDR_DEFAULT_MASK                       _MK_MASK_CONST(0xff)
#define KFUSE_KEYADDR_0_ADDR_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define KFUSE_KEYADDR_0_ADDR_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)

#define KFUSE_KEYADDR_0_AUTOINC_SHIFT                   _MK_SHIFT_CONST(16)
#define KFUSE_KEYADDR_0_AUTOINC_FIELD                   _MK_FIELD_CONST(0x1, KFUSE_KEYADDR_0_AUTOINC_SHIFT)
#define KFUSE_KEYADDR_0_AUTOINC_RANGE                   16:16
#define KFUSE_KEYADDR_0_AUTOINC_WOFFSET                 0x0
#define KFUSE_KEYADDR_0_AUTOINC_DEFAULT                 _MK_MASK_CONST(0x1)
#define KFUSE_KEYADDR_0_AUTOINC_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define KFUSE_KEYADDR_0_AUTOINC_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define KFUSE_KEYADDR_0_AUTOINC_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register KFUSE_KEYS_0
#define KFUSE_KEYS_0                    _MK_ADDR_CONST(0x8c)
#define KFUSE_KEYS_0_SECURE                     0x0
#define KFUSE_KEYS_0_WORD_COUNT                         0x1
#define KFUSE_KEYS_0_RESET_VAL                  _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_RESET_MASK                         _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_READ_MASK                  _MK_MASK_CONST(0xffffffff)
#define KFUSE_KEYS_0_WRITE_MASK                         _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_DATA_SHIFT                 _MK_SHIFT_CONST(0)
#define KFUSE_KEYS_0_DATA_FIELD                 _MK_FIELD_CONST(0xffffffff, KFUSE_KEYS_0_DATA_SHIFT)
#define KFUSE_KEYS_0_DATA_RANGE                 31:0
#define KFUSE_KEYS_0_DATA_WOFFSET                       0x0
#define KFUSE_KEYS_0_DATA_DEFAULT                       _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_DATA_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_DATA_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define KFUSE_KEYS_0_DATA_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)


//
// REGISTER LIST
//
#define LIST_ARKFUSE_REGS(_op_) \
_op_(KFUSE_FUSECTRL_0) \
_op_(KFUSE_FUSEADDR_0) \
_op_(KFUSE_FUSEDATA0_0) \
_op_(KFUSE_FUSEWRDATA0_0) \
_op_(KFUSE_FUSETIME_RD1_0) \
_op_(KFUSE_FUSETIME_RD2_0) \
_op_(KFUSE_FUSETIME_PGM1_0) \
_op_(KFUSE_FUSETIME_PGM2_0) \
_op_(KFUSE_REGULATOR_0) \
_op_(KFUSE_STATE_0) \
_op_(KFUSE_ERRCOUNT_0) \
_op_(KFUSE_KEYADDR_0) \
_op_(KFUSE_KEYS_0)


//
// ADDRESS SPACES
//

#define BASE_ADDRESS_KFUSE      0x00000000

//
// ARKFUSE REGISTER BANKS
//

#define KFUSE0_FIRST_REG 0x0000 // KFUSE_FUSECTRL_0
#define KFUSE0_LAST_REG 0x0020 // KFUSE_REGULATOR_0
#define KFUSE1_FIRST_REG 0x0080 // KFUSE_STATE_0
#define KFUSE1_LAST_REG 0x008c // KFUSE_KEYS_0

// To satisfy various compilers and platforms,
// we let users control the types and syntax of certain constants, using macros.
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

#endif // ifndef ___ARKFUSE_H_INC_
