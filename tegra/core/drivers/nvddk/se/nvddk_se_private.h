/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_SE_PRIVATE_H
#define INCLUDED_NVDDK_SE_PRIVATE_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define ARSE_SECURE     0
#define ARSE_NONSECURE  1
#define ARSE_SHA1_HASH_SIZE     160
#define ARSE_SHA224_HASH_SIZE   224
#define ARSE_SHA256_HASH_SIZE   256
#define ARSE_SHA384_HASH_SIZE   384
#define ARSE_SHA512_HASH_SIZE   512
#define ARSE_SHA_HASH_SIZE      512
#define ARSE_AES_HASH_SIZE      128
#define ARSE_HASH_SIZE  512
#define ARSE_CRYPTO_NUM_KEY_CHUNKS      16
#define ARSE_CRYPTO_KEY_INDEX_WIDTH     4

// Packet SE_MODE_PKT
#define SE_MODE_PKT_SIZE 8

#define SE_MODE_PKT_MODE_SHIFT                  _MK_SHIFT_CONST(0)
#define SE_MODE_PKT_MODE_FIELD                  _MK_FIELD_CONST(0xff, SE_MODE_PKT_MODE_SHIFT)
#define SE_MODE_PKT_MODE_RANGE                  _MK_SHIFT_CONST(7):_MK_SHIFT_CONST(0)
#define SE_MODE_PKT_MODE_ROW                    0

#define SE_MODE_PKT_AESMODE_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_MODE_PKT_AESMODE_FIELD                       _MK_FIELD_CONST(0x3, SE_MODE_PKT_AESMODE_SHIFT)
#define SE_MODE_PKT_AESMODE_RANGE                       _MK_SHIFT_CONST(1):_MK_SHIFT_CONST(0)
#define SE_MODE_PKT_AESMODE_ROW                 0
#define SE_MODE_PKT_AESMODE_KEY128                      _MK_ENUM_CONST(0)
#define SE_MODE_PKT_AESMODE_KEY192                      _MK_ENUM_CONST(1)
#define SE_MODE_PKT_AESMODE_KEY256                      _MK_ENUM_CONST(2)

#define SE_MODE_PKT_SHAMODE_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_MODE_PKT_SHAMODE_FIELD                       _MK_FIELD_CONST(0x7, SE_MODE_PKT_SHAMODE_SHIFT)
#define SE_MODE_PKT_SHAMODE_RANGE                       _MK_SHIFT_CONST(2):_MK_SHIFT_CONST(0)
#define SE_MODE_PKT_SHAMODE_ROW                 0
#define SE_MODE_PKT_SHAMODE_SHA1                        _MK_ENUM_CONST(0)
#define SE_MODE_PKT_SHAMODE_SHA224                      _MK_ENUM_CONST(4)
#define SE_MODE_PKT_SHAMODE_SHA256                      _MK_ENUM_CONST(5)
#define SE_MODE_PKT_SHAMODE_SHA384                      _MK_ENUM_CONST(6)
#define SE_MODE_PKT_SHAMODE_SHA512                      _MK_ENUM_CONST(7)

// Register SE_SE_SECURITY_0
#define SE_SE_SECURITY_0                                _MK_ADDR_CONST(0x0)
#define SE_SE_SECURITY_0_KEY_SCHED_READ_SHIFT           _MK_SHIFT_CONST(3)
#define SE_SE_SECURITY_0_KEY_SCHED_READ_FIELD           _MK_FIELD_CONST(0x1, SE_SE_SECURITY_0_KEY_SCHED_READ_SHIFT)
#define SE_SE_SECURITY_0_KEY_SCHED_READ_RANGE           3:3
#define SE_SE_SECURITY_0_KEY_SCHED_READ_DISABLE         _MK_ENUM_CONST(0)

// Register SE_OPERATION_0
#define SE_OPERATION_0                  _MK_ADDR_CONST(0x8)
#define SE_OPERATION_0_SECURE                   0x0
#define SE_OPERATION_0_WORD_COUNT                       0x1
#define SE_OPERATION_0_RESET_VAL                        _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_RESET_MASK                       _MK_MASK_CONST(0x3)
#define SE_OPERATION_0_SW_DEFAULT_VAL                   _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_READ_MASK                        _MK_MASK_CONST(0x3)
#define SE_OPERATION_0_WRITE_MASK                       _MK_MASK_CONST(0x3)
#define SE_OPERATION_0_OP_SHIFT                 _MK_SHIFT_CONST(0)
#define SE_OPERATION_0_OP_FIELD                 _MK_FIELD_CONST(0x3, SE_OPERATION_0_OP_SHIFT)
#define SE_OPERATION_0_OP_RANGE                 1:0
#define SE_OPERATION_0_OP_WOFFSET                       0x0
#define SE_OPERATION_0_OP_DEFAULT                       _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_OP_DEFAULT_MASK                  _MK_MASK_CONST(0x3)
#define SE_OPERATION_0_OP_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_OP_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_OPERATION_0_OP_INIT_ENUM                     ABORT
#define SE_OPERATION_0_OP_START                 _MK_ENUM_CONST(1)
#define SE_OPERATION_0_OP_RESTART_OUT                   _MK_ENUM_CONST(2)
#define SE_OPERATION_0_OP_CTX_SAVE                      _MK_ENUM_CONST(3)
#define SE_OPERATION_0_OP_ABORT                 _MK_ENUM_CONST(0)


// Register SE_INT_ENABLE_0
#define SE_INT_ENABLE_0                 _MK_ADDR_CONST(0xc)
#define SE_INT_ENABLE_0_SECURE                  0x0
#define SE_INT_ENABLE_0_WORD_COUNT                      0x1
#define SE_INT_ENABLE_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_RESET_MASK                      _MK_MASK_CONST(0x1001f)
#define SE_INT_ENABLE_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_READ_MASK                       _MK_MASK_CONST(0x1001f)
#define SE_INT_ENABLE_0_WRITE_MASK                      _MK_MASK_CONST(0x1001f)
#define SE_INT_ENABLE_0_ERR_STAT_SHIFT                  _MK_SHIFT_CONST(16)
#define SE_INT_ENABLE_0_ERR_STAT_FIELD                  _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_ERR_STAT_SHIFT)
#define SE_INT_ENABLE_0_ERR_STAT_RANGE                  16:16
#define SE_INT_ENABLE_0_ERR_STAT_WOFFSET                        0x0
#define SE_INT_ENABLE_0_ERR_STAT_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_ERR_STAT_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_ERR_STAT_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_ERR_STAT_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_ERR_STAT_INIT_ENUM                      DISABLE
#define SE_INT_ENABLE_0_ERR_STAT_DISABLE                        _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_ERR_STAT_ENABLE                 _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_SHIFT                     _MK_SHIFT_CONST(5)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_FIELD                     _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_SHIFT)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_RANGE                     5:5
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_WOFFSET                   0x0
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_INIT_ENUM                 DISABLE
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_DISABLE                   _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_ENABLE                    _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_SE_OP_DONE_SHIFT                        _MK_SHIFT_CONST(4)
#define SE_INT_ENABLE_0_SE_OP_DONE_FIELD                        _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_SE_OP_DONE_SHIFT)
#define SE_INT_ENABLE_0_SE_OP_DONE_RANGE                        4:4
#define SE_INT_ENABLE_0_SE_OP_DONE_WOFFSET                      0x0
#define SE_INT_ENABLE_0_SE_OP_DONE_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_SE_OP_DONE_DEFAULT_MASK                 _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_SE_OP_DONE_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_SE_OP_DONE_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_SE_OP_DONE_INIT_ENUM                    DISABLE
#define SE_INT_ENABLE_0_SE_OP_DONE_DISABLE                      _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_SE_OP_DONE_ENABLE                       _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_OUT_DONE_SHIFT                  _MK_SHIFT_CONST(3)
#define SE_INT_ENABLE_0_OUT_DONE_FIELD                  _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_OUT_DONE_SHIFT)
#define SE_INT_ENABLE_0_OUT_DONE_RANGE                  3:3
#define SE_INT_ENABLE_0_OUT_DONE_WOFFSET                        0x0
#define SE_INT_ENABLE_0_OUT_DONE_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_DONE_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_OUT_DONE_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_DONE_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_DONE_INIT_ENUM                      DISABLE
#define SE_INT_ENABLE_0_OUT_DONE_DISABLE                        _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_OUT_DONE_ENABLE                 _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_SHIFT                     _MK_SHIFT_CONST(2)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_FIELD                     _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_OUT_LL_BUF_WR_SHIFT)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_RANGE                     2:2
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_WOFFSET                   0x0
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_INIT_ENUM                 DISABLE
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_DISABLE                   _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_OUT_LL_BUF_WR_ENABLE                    _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_IN_DONE_SHIFT                   _MK_SHIFT_CONST(1)
#define SE_INT_ENABLE_0_IN_DONE_FIELD                   _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_IN_DONE_SHIFT)
#define SE_INT_ENABLE_0_IN_DONE_RANGE                   1:1
#define SE_INT_ENABLE_0_IN_DONE_WOFFSET                 0x0
#define SE_INT_ENABLE_0_IN_DONE_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_DONE_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_IN_DONE_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_DONE_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_DONE_INIT_ENUM                       DISABLE
#define SE_INT_ENABLE_0_IN_DONE_DISABLE                 _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_IN_DONE_ENABLE                  _MK_ENUM_CONST(1)

#define SE_INT_ENABLE_0_IN_LL_BUF_RD_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_FIELD                      _MK_FIELD_CONST(0x1, SE_INT_ENABLE_0_IN_LL_BUF_RD_SHIFT)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_RANGE                      0:0
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_WOFFSET                    0x0
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_DEFAULT_MASK                       _MK_MASK_CONST(0x1)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_INIT_ENUM                  DISABLE
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_DISABLE                    _MK_ENUM_CONST(0)
#define SE_INT_ENABLE_0_IN_LL_BUF_RD_ENABLE                     _MK_ENUM_CONST(1)


// Register SE_INT_STATUS_0
#define SE_INT_STATUS_0                 _MK_ADDR_CONST(0x10)
#define SE_INT_STATUS_0_SECURE                  0x0
#define SE_INT_STATUS_0_WORD_COUNT                      0x1
#define SE_INT_STATUS_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_RESET_MASK                      _MK_MASK_CONST(0x1001f)
#define SE_INT_STATUS_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_READ_MASK                       _MK_MASK_CONST(0x1001f)
#define SE_INT_STATUS_0_WRITE_MASK                      _MK_MASK_CONST(0x1001f)
#define SE_INT_STATUS_0_ERR_STAT_SHIFT                  _MK_SHIFT_CONST(16)
#define SE_INT_STATUS_0_ERR_STAT_FIELD                  _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_ERR_STAT_SHIFT)
#define SE_INT_STATUS_0_ERR_STAT_RANGE                  16:16
#define SE_INT_STATUS_0_ERR_STAT_WOFFSET                        0x0
#define SE_INT_STATUS_0_ERR_STAT_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_ERR_STAT_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_ERR_STAT_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_ERR_STAT_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_ERR_STAT_INIT_ENUM                      CLEAR
#define SE_INT_STATUS_0_ERR_STAT_CLEAR                  _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_ERR_STAT_ACTIVE                 _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_ERR_STAT_SW_CLEAR                       _MK_ENUM_CONST(1)

#define SE_INT_STATUS_0_SE_OP_DONE_SHIFT                        _MK_SHIFT_CONST(4)
#define SE_INT_STATUS_0_SE_OP_DONE_FIELD                        _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_SE_OP_DONE_SHIFT)
#define SE_INT_STATUS_0_SE_OP_DONE_RANGE                        4:4
#define SE_INT_STATUS_0_SE_OP_DONE_WOFFSET                      0x0
#define SE_INT_STATUS_0_SE_OP_DONE_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_SE_OP_DONE_DEFAULT_MASK                 _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_SE_OP_DONE_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_SE_OP_DONE_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_SE_OP_DONE_INIT_ENUM                    CLEAR
#define SE_INT_STATUS_0_SE_OP_DONE_CLEAR                        _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_SE_OP_DONE_ACTIVE                       _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_SE_OP_DONE_SW_CLEAR                     _MK_ENUM_CONST(1)

#define SE_INT_STATUS_0_OUT_DONE_SHIFT                  _MK_SHIFT_CONST(3)
#define SE_INT_STATUS_0_OUT_DONE_FIELD                  _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_OUT_DONE_SHIFT)
#define SE_INT_STATUS_0_OUT_DONE_RANGE                  3:3
#define SE_INT_STATUS_0_OUT_DONE_WOFFSET                        0x0
#define SE_INT_STATUS_0_OUT_DONE_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_DONE_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_OUT_DONE_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_DONE_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_DONE_INIT_ENUM                      CLEAR
#define SE_INT_STATUS_0_OUT_DONE_CLEAR                  _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_OUT_DONE_ACTIVE                 _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_OUT_DONE_SW_CLEAR                       _MK_ENUM_CONST(1)

#define SE_INT_STATUS_0_OUT_LL_BUF_WR_SHIFT                     _MK_SHIFT_CONST(2)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_FIELD                     _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_OUT_LL_BUF_WR_SHIFT)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_RANGE                     2:2
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_WOFFSET                   0x0
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_INIT_ENUM                 CLEAR
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_CLEAR                     _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_ACTIVE                    _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_OUT_LL_BUF_WR_SW_CLEAR                  _MK_ENUM_CONST(1)

#define SE_INT_STATUS_0_IN_DONE_SHIFT                   _MK_SHIFT_CONST(1)
#define SE_INT_STATUS_0_IN_DONE_FIELD                   _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_IN_DONE_SHIFT)
#define SE_INT_STATUS_0_IN_DONE_RANGE                   1:1
#define SE_INT_STATUS_0_IN_DONE_WOFFSET                 0x0
#define SE_INT_STATUS_0_IN_DONE_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_DONE_DEFAULT_MASK                    _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_IN_DONE_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_DONE_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_DONE_INIT_ENUM                       CLEAR
#define SE_INT_STATUS_0_IN_DONE_CLEAR                   _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_IN_DONE_ACTIVE                  _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_IN_DONE_SW_CLEAR                        _MK_ENUM_CONST(1)

#define SE_INT_STATUS_0_IN_LL_BUF_RD_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_FIELD                      _MK_FIELD_CONST(0x1, SE_INT_STATUS_0_IN_LL_BUF_RD_SHIFT)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_RANGE                      0:0
#define SE_INT_STATUS_0_IN_LL_BUF_RD_WOFFSET                    0x0
#define SE_INT_STATUS_0_IN_LL_BUF_RD_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_DEFAULT_MASK                       _MK_MASK_CONST(0x1)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_INIT_ENUM                  CLEAR
#define SE_INT_STATUS_0_IN_LL_BUF_RD_CLEAR                      _MK_ENUM_CONST(0)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_ACTIVE                     _MK_ENUM_CONST(1)
#define SE_INT_STATUS_0_IN_LL_BUF_RD_SW_CLEAR                   _MK_ENUM_CONST(1)


// Register SE_CONFIG_0
#define SE_CONFIG_0                     _MK_ADDR_CONST(0x14)
#define SE_CONFIG_0_SECURE                      0x0
#define SE_CONFIG_0_WORD_COUNT                  0x1
#define SE_CONFIG_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_READ_MASK                   _MK_MASK_CONST(0xffffff0c)
#define SE_CONFIG_0_WRITE_MASK                  _MK_MASK_CONST(0xffffff0c)
#define SE_CONFIG_0_DST_SHIFT                   _MK_SHIFT_CONST(2)
#define SE_CONFIG_0_DST_FIELD                   _MK_FIELD_CONST(0x3, SE_CONFIG_0_DST_SHIFT)
#define SE_CONFIG_0_DST_RANGE                   3:2
#define SE_CONFIG_0_DST_WOFFSET                 0x0
#define SE_CONFIG_0_DST_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DST_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DST_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DST_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DST_MEMORY                  _MK_ENUM_CONST(0)
#define SE_CONFIG_0_DST_HASH_REG                        _MK_ENUM_CONST(1)
#define SE_CONFIG_0_DST_KEYTABLE                        _MK_ENUM_CONST(2)
#define SE_CONFIG_0_DST_SRK                     _MK_ENUM_CONST(3)

#define SE_CONFIG_0_DEC_ALG_SHIFT                       _MK_SHIFT_CONST(8)
#define SE_CONFIG_0_DEC_ALG_FIELD                       _MK_FIELD_CONST(0xf, SE_CONFIG_0_DEC_ALG_SHIFT)
#define SE_CONFIG_0_DEC_ALG_RANGE                       11:8
#define SE_CONFIG_0_DEC_ALG_WOFFSET                     0x0
#define SE_CONFIG_0_DEC_ALG_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_ALG_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_ALG_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_ALG_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_ALG_NOP                 _MK_ENUM_CONST(0)
#define SE_CONFIG_0_DEC_ALG_AES_DEC                     _MK_ENUM_CONST(1)

#define SE_CONFIG_0_ENC_ALG_SHIFT                       _MK_SHIFT_CONST(12)
#define SE_CONFIG_0_ENC_ALG_FIELD                       _MK_FIELD_CONST(0xf, SE_CONFIG_0_ENC_ALG_SHIFT)
#define SE_CONFIG_0_ENC_ALG_RANGE                       15:12
#define SE_CONFIG_0_ENC_ALG_WOFFSET                     0x0
#define SE_CONFIG_0_ENC_ALG_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_ALG_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_ALG_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_ALG_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_ALG_NOP                 _MK_ENUM_CONST(0)
#define SE_CONFIG_0_ENC_ALG_AES_ENC                     _MK_ENUM_CONST(1)
#define SE_CONFIG_0_ENC_ALG_X931_RNG                    _MK_ENUM_CONST(2)
#define SE_CONFIG_0_ENC_ALG_SHA                 _MK_ENUM_CONST(3)

#define SE_CONFIG_0_DEC_MODE_SHIFT                      _MK_SHIFT_CONST(16)
#define SE_CONFIG_0_DEC_MODE_FIELD                      _MK_FIELD_CONST(0xff, SE_CONFIG_0_DEC_MODE_SHIFT)
#define SE_CONFIG_0_DEC_MODE_RANGE                      23:16
#define SE_CONFIG_0_DEC_MODE_WOFFSET                    0x0
#define SE_CONFIG_0_DEC_MODE_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_MODE_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_MODE_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_DEC_MODE_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)

#define SE_CONFIG_0_ENC_MODE_SHIFT                      _MK_SHIFT_CONST(24)
#define SE_CONFIG_0_ENC_MODE_FIELD                      _MK_FIELD_CONST(0xff, SE_CONFIG_0_ENC_MODE_SHIFT)
#define SE_CONFIG_0_ENC_MODE_RANGE                      31:24
#define SE_CONFIG_0_ENC_MODE_WOFFSET                    0x0
#define SE_CONFIG_0_ENC_MODE_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_MODE_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_MODE_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_CONFIG_0_ENC_MODE_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_IN_LL_ADDR_0
#define SE_IN_LL_ADDR_0                 _MK_ADDR_CONST(0x18)
#define SE_IN_LL_ADDR_0_SECURE                  0x0
#define SE_IN_LL_ADDR_0_WORD_COUNT                      0x1
#define SE_IN_LL_ADDR_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_RESET_MASK                      _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_READ_MASK                       _MK_MASK_CONST(0xffffffff)
#define SE_IN_LL_ADDR_0_WRITE_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_IN_LL_ADDR_0_VAL_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_IN_LL_ADDR_0_VAL_FIELD                       _MK_FIELD_CONST(0xffffffff, SE_IN_LL_ADDR_0_VAL_SHIFT)
#define SE_IN_LL_ADDR_0_VAL_RANGE                       31:0
#define SE_IN_LL_ADDR_0_VAL_WOFFSET                     0x0
#define SE_IN_LL_ADDR_0_VAL_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_VAL_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_VAL_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_IN_LL_ADDR_0_VAL_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)


// Register SE_OUT_LL_ADDR_0
#define SE_OUT_LL_ADDR_0                        _MK_ADDR_CONST(0x24)
#define SE_OUT_LL_ADDR_0_SECURE                         0x0
#define SE_OUT_LL_ADDR_0_WORD_COUNT                     0x1
#define SE_OUT_LL_ADDR_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_OUT_LL_ADDR_0_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_OUT_LL_ADDR_0_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_OUT_LL_ADDR_0_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_OUT_LL_ADDR_0_VAL_SHIFT)
#define SE_OUT_LL_ADDR_0_VAL_RANGE                      31:0
#define SE_OUT_LL_ADDR_0_VAL_WOFFSET                    0x0
#define SE_OUT_LL_ADDR_0_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_OUT_LL_ADDR_0_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_0
#define SE_HASH_RESULT_0                        _MK_ADDR_CONST(0x30)
#define SE_HASH_RESULT_0_SECURE                         0x0
#define SE_HASH_RESULT_0_WORD_COUNT                     0x1
#define SE_HASH_RESULT_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_0_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_0_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_0_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_0_VAL_SHIFT)
#define SE_HASH_RESULT_0_VAL_RANGE                      31:0
#define SE_HASH_RESULT_0_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_0_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_0_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT
#define SE_HASH_RESULT                  _MK_ADDR_CONST(0x30)
#define SE_HASH_RESULT_SECURE                   0x0
#define SE_HASH_RESULT_WORD_COUNT                       0x1
#define SE_HASH_RESULT_RESET_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_RESET_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_SW_DEFAULT_VAL                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_SW_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_READ_MASK                        _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_WRITE_MASK                       _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_VAL_SHIFT                        _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_VAL_FIELD                        _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_VAL_SHIFT)
#define SE_HASH_RESULT_VAL_RANGE                        31:0
#define SE_HASH_RESULT_VAL_WOFFSET                      0x0
#define SE_HASH_RESULT_VAL_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_VAL_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_VAL_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_VAL_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_1
#define SE_HASH_RESULT_1                        _MK_ADDR_CONST(0x34)
#define SE_HASH_RESULT_1_SECURE                         0x0
#define SE_HASH_RESULT_1_WORD_COUNT                     0x1
#define SE_HASH_RESULT_1_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_1_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_1_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_1_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_1_VAL_SHIFT)
#define SE_HASH_RESULT_1_VAL_RANGE                      31:0
#define SE_HASH_RESULT_1_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_1_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_1_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_2
#define SE_HASH_RESULT_2                        _MK_ADDR_CONST(0x38)
#define SE_HASH_RESULT_2_SECURE                         0x0
#define SE_HASH_RESULT_2_WORD_COUNT                     0x1
#define SE_HASH_RESULT_2_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_2_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_2_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_2_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_2_VAL_SHIFT)
#define SE_HASH_RESULT_2_VAL_RANGE                      31:0
#define SE_HASH_RESULT_2_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_2_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_2_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_3
#define SE_HASH_RESULT_3                        _MK_ADDR_CONST(0x3c)
#define SE_HASH_RESULT_3_SECURE                         0x0
#define SE_HASH_RESULT_3_WORD_COUNT                     0x1
#define SE_HASH_RESULT_3_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_3_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_3_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_3_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_3_VAL_SHIFT)
#define SE_HASH_RESULT_3_VAL_RANGE                      31:0
#define SE_HASH_RESULT_3_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_3_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_3_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_4
#define SE_HASH_RESULT_4                        _MK_ADDR_CONST(0x40)
#define SE_HASH_RESULT_4_SECURE                         0x0
#define SE_HASH_RESULT_4_WORD_COUNT                     0x1
#define SE_HASH_RESULT_4_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_4_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_4_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_4_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_4_VAL_SHIFT)
#define SE_HASH_RESULT_4_VAL_RANGE                      31:0
#define SE_HASH_RESULT_4_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_4_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_4_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_5
#define SE_HASH_RESULT_5                        _MK_ADDR_CONST(0x44)
#define SE_HASH_RESULT_5_SECURE                         0x0
#define SE_HASH_RESULT_5_WORD_COUNT                     0x1
#define SE_HASH_RESULT_5_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_5_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_5_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_5_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_5_VAL_SHIFT)
#define SE_HASH_RESULT_5_VAL_RANGE                      31:0
#define SE_HASH_RESULT_5_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_5_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_5_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_6
#define SE_HASH_RESULT_6                        _MK_ADDR_CONST(0x48)
#define SE_HASH_RESULT_6_SECURE                         0x0
#define SE_HASH_RESULT_6_WORD_COUNT                     0x1
#define SE_HASH_RESULT_6_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_6_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_6_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_6_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_6_VAL_SHIFT)
#define SE_HASH_RESULT_6_VAL_RANGE                      31:0
#define SE_HASH_RESULT_6_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_6_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_6_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_7
#define SE_HASH_RESULT_7                        _MK_ADDR_CONST(0x4c)
#define SE_HASH_RESULT_7_SECURE                         0x0
#define SE_HASH_RESULT_7_WORD_COUNT                     0x1
#define SE_HASH_RESULT_7_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_7_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_7_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_7_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_7_VAL_SHIFT)
#define SE_HASH_RESULT_7_VAL_RANGE                      31:0
#define SE_HASH_RESULT_7_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_7_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_7_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_8
#define SE_HASH_RESULT_8                        _MK_ADDR_CONST(0x50)
#define SE_HASH_RESULT_8_SECURE                         0x0
#define SE_HASH_RESULT_8_WORD_COUNT                     0x1
#define SE_HASH_RESULT_8_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_8_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_8_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_8_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_8_VAL_SHIFT)
#define SE_HASH_RESULT_8_VAL_RANGE                      31:0
#define SE_HASH_RESULT_8_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_8_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_8_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_9
#define SE_HASH_RESULT_9                        _MK_ADDR_CONST(0x54)
#define SE_HASH_RESULT_9_SECURE                         0x0
#define SE_HASH_RESULT_9_WORD_COUNT                     0x1
#define SE_HASH_RESULT_9_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_9_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_9_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_9_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_9_VAL_SHIFT)
#define SE_HASH_RESULT_9_VAL_RANGE                      31:0
#define SE_HASH_RESULT_9_VAL_WOFFSET                    0x0
#define SE_HASH_RESULT_9_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_9_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_10
#define SE_HASH_RESULT_10                       _MK_ADDR_CONST(0x58)
#define SE_HASH_RESULT_10_SECURE                        0x0
#define SE_HASH_RESULT_10_WORD_COUNT                    0x1
#define SE_HASH_RESULT_10_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_10_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_10_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_10_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_10_VAL_SHIFT)
#define SE_HASH_RESULT_10_VAL_RANGE                     31:0
#define SE_HASH_RESULT_10_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_10_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_10_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_11
#define SE_HASH_RESULT_11                       _MK_ADDR_CONST(0x5c)
#define SE_HASH_RESULT_11_SECURE                        0x0
#define SE_HASH_RESULT_11_WORD_COUNT                    0x1
#define SE_HASH_RESULT_11_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_11_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_11_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_11_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_11_VAL_SHIFT)
#define SE_HASH_RESULT_11_VAL_RANGE                     31:0
#define SE_HASH_RESULT_11_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_11_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_11_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_12
#define SE_HASH_RESULT_12                       _MK_ADDR_CONST(0x60)
#define SE_HASH_RESULT_12_SECURE                        0x0
#define SE_HASH_RESULT_12_WORD_COUNT                    0x1
#define SE_HASH_RESULT_12_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_12_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_12_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_12_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_12_VAL_SHIFT)
#define SE_HASH_RESULT_12_VAL_RANGE                     31:0
#define SE_HASH_RESULT_12_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_12_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_12_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_13
#define SE_HASH_RESULT_13                       _MK_ADDR_CONST(0x64)
#define SE_HASH_RESULT_13_SECURE                        0x0
#define SE_HASH_RESULT_13_WORD_COUNT                    0x1
#define SE_HASH_RESULT_13_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_13_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_13_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_13_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_13_VAL_SHIFT)
#define SE_HASH_RESULT_13_VAL_RANGE                     31:0
#define SE_HASH_RESULT_13_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_13_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_13_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_14
#define SE_HASH_RESULT_14                       _MK_ADDR_CONST(0x68)
#define SE_HASH_RESULT_14_SECURE                        0x0
#define SE_HASH_RESULT_14_WORD_COUNT                    0x1
#define SE_HASH_RESULT_14_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_14_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_14_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_14_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_14_VAL_SHIFT)
#define SE_HASH_RESULT_14_VAL_RANGE                     31:0
#define SE_HASH_RESULT_14_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_14_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_14_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_HASH_RESULT_15
#define SE_HASH_RESULT_15                       _MK_ADDR_CONST(0x6c)
#define SE_HASH_RESULT_15_SECURE                        0x0
#define SE_HASH_RESULT_15_WORD_COUNT                    0x1
#define SE_HASH_RESULT_15_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_15_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_HASH_RESULT_15_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_HASH_RESULT_15_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_HASH_RESULT_15_VAL_SHIFT)
#define SE_HASH_RESULT_15_VAL_RANGE                     31:0
#define SE_HASH_RESULT_15_VAL_WOFFSET                   0x0
#define SE_HASH_RESULT_15_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_HASH_RESULT_15_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


#define ARSE_SHA_MSG_LENGTH_SIZE        128

// Register SE_SHA_CONFIG_0
#define SE_SHA_CONFIG_0                 _MK_ADDR_CONST(0x200)
#define SE_SHA_CONFIG_0_SECURE                  0x0
#define SE_SHA_CONFIG_0_WORD_COUNT                      0x1
#define SE_SHA_CONFIG_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_RESET_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_READ_MASK                       _MK_MASK_CONST(0x1)
#define SE_SHA_CONFIG_0_WRITE_MASK                      _MK_MASK_CONST(0x1)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_FIELD                      _MK_FIELD_CONST(0x1, SE_SHA_CONFIG_0_HW_INIT_HASH_SHIFT)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_RANGE                      0:0
#define SE_SHA_CONFIG_0_HW_INIT_HASH_WOFFSET                    0x0
#define SE_SHA_CONFIG_0_HW_INIT_HASH_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_DISABLE                    _MK_ENUM_CONST(0)
#define SE_SHA_CONFIG_0_HW_INIT_HASH_ENABLE                     _MK_ENUM_CONST(1)


// Register SE_SHA_MSG_LENGTH_0
#define SE_SHA_MSG_LENGTH_0                     _MK_ADDR_CONST(0x204)
#define SE_SHA_MSG_LENGTH_0_SECURE                      0x0
#define SE_SHA_MSG_LENGTH_0_WORD_COUNT                  0x1
#define SE_SHA_MSG_LENGTH_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_READ_MASK                   _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_0_WRITE_MASK                  _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_0_VAL_SHIFT                   _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LENGTH_0_VAL_FIELD                   _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LENGTH_0_VAL_SHIFT)
#define SE_SHA_MSG_LENGTH_0_VAL_RANGE                   31:0
#define SE_SHA_MSG_LENGTH_0_VAL_WOFFSET                 0x0
#define SE_SHA_MSG_LENGTH_0_VAL_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_VAL_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_VAL_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_0_VAL_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LENGTH
#define SE_SHA_MSG_LENGTH                       _MK_ADDR_CONST(0x204)
#define SE_SHA_MSG_LENGTH_SECURE                        0x0
#define SE_SHA_MSG_LENGTH_WORD_COUNT                    0x1
#define SE_SHA_MSG_LENGTH_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LENGTH_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LENGTH_VAL_SHIFT)
#define SE_SHA_MSG_LENGTH_VAL_RANGE                     31:0
#define SE_SHA_MSG_LENGTH_VAL_WOFFSET                   0x0
#define SE_SHA_MSG_LENGTH_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LENGTH_1
#define SE_SHA_MSG_LENGTH_1                     _MK_ADDR_CONST(0x208)
#define SE_SHA_MSG_LENGTH_1_SECURE                      0x0
#define SE_SHA_MSG_LENGTH_1_WORD_COUNT                  0x1
#define SE_SHA_MSG_LENGTH_1_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_READ_MASK                   _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_1_WRITE_MASK                  _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_1_VAL_SHIFT                   _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LENGTH_1_VAL_FIELD                   _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LENGTH_1_VAL_SHIFT)
#define SE_SHA_MSG_LENGTH_1_VAL_RANGE                   31:0
#define SE_SHA_MSG_LENGTH_1_VAL_WOFFSET                 0x0
#define SE_SHA_MSG_LENGTH_1_VAL_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_VAL_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_VAL_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_1_VAL_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LENGTH_2
#define SE_SHA_MSG_LENGTH_2                     _MK_ADDR_CONST(0x20c)
#define SE_SHA_MSG_LENGTH_2_SECURE                      0x0
#define SE_SHA_MSG_LENGTH_2_WORD_COUNT                  0x1
#define SE_SHA_MSG_LENGTH_2_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_READ_MASK                   _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_2_WRITE_MASK                  _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_2_VAL_SHIFT                   _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LENGTH_2_VAL_FIELD                   _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LENGTH_2_VAL_SHIFT)
#define SE_SHA_MSG_LENGTH_2_VAL_RANGE                   31:0
#define SE_SHA_MSG_LENGTH_2_VAL_WOFFSET                 0x0
#define SE_SHA_MSG_LENGTH_2_VAL_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_VAL_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_VAL_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_2_VAL_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LENGTH_3
#define SE_SHA_MSG_LENGTH_3                     _MK_ADDR_CONST(0x210)
#define SE_SHA_MSG_LENGTH_3_SECURE                      0x0
#define SE_SHA_MSG_LENGTH_3_WORD_COUNT                  0x1
#define SE_SHA_MSG_LENGTH_3_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_READ_MASK                   _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_3_WRITE_MASK                  _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LENGTH_3_VAL_SHIFT                   _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LENGTH_3_VAL_FIELD                   _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LENGTH_3_VAL_SHIFT)
#define SE_SHA_MSG_LENGTH_3_VAL_RANGE                   31:0
#define SE_SHA_MSG_LENGTH_3_VAL_WOFFSET                 0x0
#define SE_SHA_MSG_LENGTH_3_VAL_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_VAL_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_VAL_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LENGTH_3_VAL_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LEFT_0
#define SE_SHA_MSG_LEFT_0                       _MK_ADDR_CONST(0x214)
#define SE_SHA_MSG_LEFT_0_SECURE                        0x0
#define SE_SHA_MSG_LEFT_0_WORD_COUNT                    0x1
#define SE_SHA_MSG_LEFT_0_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_0_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_0_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LEFT_0_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LEFT_0_VAL_SHIFT)
#define SE_SHA_MSG_LEFT_0_VAL_RANGE                     31:0
#define SE_SHA_MSG_LEFT_0_VAL_WOFFSET                   0x0
#define SE_SHA_MSG_LEFT_0_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_0_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LEFT
#define SE_SHA_MSG_LEFT                 _MK_ADDR_CONST(0x214)
#define SE_SHA_MSG_LEFT_SECURE                  0x0
#define SE_SHA_MSG_LEFT_WORD_COUNT                      0x1
#define SE_SHA_MSG_LEFT_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_RESET_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_READ_MASK                       _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_WRITE_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_VAL_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LEFT_VAL_FIELD                       _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LEFT_VAL_SHIFT)
#define SE_SHA_MSG_LEFT_VAL_RANGE                       31:0
#define SE_SHA_MSG_LEFT_VAL_WOFFSET                     0x0
#define SE_SHA_MSG_LEFT_VAL_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_VAL_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_VAL_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_VAL_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LEFT_1
#define SE_SHA_MSG_LEFT_1                       _MK_ADDR_CONST(0x218)
#define SE_SHA_MSG_LEFT_1_SECURE                        0x0
#define SE_SHA_MSG_LEFT_1_WORD_COUNT                    0x1
#define SE_SHA_MSG_LEFT_1_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_1_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_1_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LEFT_1_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LEFT_1_VAL_SHIFT)
#define SE_SHA_MSG_LEFT_1_VAL_RANGE                     31:0
#define SE_SHA_MSG_LEFT_1_VAL_WOFFSET                   0x0
#define SE_SHA_MSG_LEFT_1_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_1_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LEFT_2
#define SE_SHA_MSG_LEFT_2                       _MK_ADDR_CONST(0x21c)
#define SE_SHA_MSG_LEFT_2_SECURE                        0x0
#define SE_SHA_MSG_LEFT_2_WORD_COUNT                    0x1
#define SE_SHA_MSG_LEFT_2_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_2_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_2_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LEFT_2_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LEFT_2_VAL_SHIFT)
#define SE_SHA_MSG_LEFT_2_VAL_RANGE                     31:0
#define SE_SHA_MSG_LEFT_2_VAL_WOFFSET                   0x0
#define SE_SHA_MSG_LEFT_2_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_2_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)


// Register SE_SHA_MSG_LEFT_3
#define SE_SHA_MSG_LEFT_3                       _MK_ADDR_CONST(0x220)
#define SE_SHA_MSG_LEFT_3_SECURE                        0x0
#define SE_SHA_MSG_LEFT_3_WORD_COUNT                    0x1
#define SE_SHA_MSG_LEFT_3_RESET_VAL                     _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_RESET_MASK                    _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_SW_DEFAULT_VAL                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_READ_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_3_WRITE_MASK                    _MK_MASK_CONST(0xffffffff)
#define SE_SHA_MSG_LEFT_3_VAL_SHIFT                     _MK_SHIFT_CONST(0)
#define SE_SHA_MSG_LEFT_3_VAL_FIELD                     _MK_FIELD_CONST(0xffffffff, SE_SHA_MSG_LEFT_3_VAL_SHIFT)
#define SE_SHA_MSG_LEFT_3_VAL_RANGE                     31:0
#define SE_SHA_MSG_LEFT_3_VAL_WOFFSET                   0x0
#define SE_SHA_MSG_LEFT_3_VAL_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_VAL_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_VAL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_SHA_MSG_LEFT_3_VAL_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)



// Packet SE_CRYPTO_SCHEDULE_PKT
#define SE_CRYPTO_SCHEDULE_PKT_SIZE 6

#define SE_CRYPTO_SCHEDULE_PKT_ADDR_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_CRYPTO_SCHEDULE_PKT_ADDR_FIELD                       _MK_FIELD_CONST(0x3f, SE_CRYPTO_SCHEDULE_PKT_ADDR_SHIFT)
#define SE_CRYPTO_SCHEDULE_PKT_ADDR_RANGE                       _MK_SHIFT_CONST(5):_MK_SHIFT_CONST(0)
#define SE_CRYPTO_SCHEDULE_PKT_ADDR_ROW                 0


// Packet SRK_PKT
#define SRK_PKT_SIZE 34

#define SRK_PKT_DATA_SHIFT                      _MK_SHIFT_CONST(0)
#define SRK_PKT_DATA_FIELD                      _MK_FIELD_CONST(0xffffffff, SRK_PKT_DATA_SHIFT)
#define SRK_PKT_DATA_RANGE                      _MK_SHIFT_CONST(31):_MK_SHIFT_CONST(0)
#define SRK_PKT_DATA_ROW                        0

#define SRK_PKT_ADDR_SHIFT                      _MK_SHIFT_CONST(32)
#define SRK_PKT_ADDR_FIELD                      _MK_FIELD_CONST(0x3, SRK_PKT_ADDR_SHIFT)
#define SRK_PKT_ADDR_RANGE                      _MK_SHIFT_CONST(33):_MK_SHIFT_CONST(32)
#define SRK_PKT_ADDR_ROW                        0


// Register SE_STATUS_0
#define SE_STATUS_0                     _MK_ADDR_CONST(0x800)
#define SE_STATUS_0_SECURE                      0x0
#define SE_STATUS_0_WORD_COUNT                  0x1
#define SE_STATUS_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_STATUS_0_RESET_MASK                  _MK_MASK_CONST(0x0)
#define SE_STATUS_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_STATUS_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_STATUS_0_READ_MASK                   _MK_MASK_CONST(0x3)
#define SE_STATUS_0_WRITE_MASK                  _MK_MASK_CONST(0x0)
#define SE_STATUS_0_STATE_SHIFT                 _MK_SHIFT_CONST(0)
#define SE_STATUS_0_STATE_FIELD                 _MK_FIELD_CONST(0x3, SE_STATUS_0_STATE_SHIFT)
#define SE_STATUS_0_STATE_RANGE                 1:0
#define SE_STATUS_0_STATE_WOFFSET                       0x0
#define SE_STATUS_0_STATE_DEFAULT                       _MK_MASK_CONST(0x0)
#define SE_STATUS_0_STATE_DEFAULT_MASK                  _MK_MASK_CONST(0x0)
#define SE_STATUS_0_STATE_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_STATUS_0_STATE_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_STATUS_0_STATE_IDLE                  _MK_ENUM_CONST(0)
#define SE_STATUS_0_STATE_BUSY                  _MK_ENUM_CONST(1)
#define SE_STATUS_0_STATE_WAIT_OUT                      _MK_ENUM_CONST(2)

#define ARSE_CRYPTO_KEYTABLE_DATA_WORDS 4

#define SE_CRYPTO_KEYTABLE_DATA_0                       _MK_ADDR_CONST(0x320)

#define SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT                     _MK_SHIFT_CONST(4)

#define SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT                     _MK_SHIFT_CONST(3)
#define SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY                       _MK_ENUM_CONST(0)
#define SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV                        _MK_ENUM_CONST(1)

#define SE_CRYPTO_KEYIV_PKT_WORD_QUAD_SHIFT                     _MK_SHIFT_CONST(2)
#define SE_CRYPTO_KEYIV_PKT_WORD_QUAD_FIELD                     _MK_FIELD_CONST(0x3, SE_CRYPTO_KEYIV_PKT_WORD_QUAD_SHIFT)
#define SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS                       _MK_ENUM_CONST(3)

// Register SE_CRYPTO_KEYTABLE_ADDR_0
#define SE_CRYPTO_KEYTABLE_ADDR_0                       _MK_ADDR_CONST(0x31c)
#define SE_CRYPTO_KEYTABLE_ADDR_0_OP_RANGE                      9:9
#define SE_CRYPTO_KEYTABLE_ADDR_0_OP_WRITE                      _MK_ENUM_CONST(1)

#define SE_CRYPTO_KEYTABLE_ADDR_0_TABLE_SEL_RANGE                       8:8
#define SE_CRYPTO_KEYTABLE_ADDR_0_TABLE_SEL_KEYIV                       _MK_ENUM_CONST(0)

#define SE_CRYPTO_KEYTABLE_ADDR_0_PKT_RANGE                     7:0

// Register SE_CRYPTO_CONFIG_0
#define SE_CRYPTO_CONFIG_0                     _MK_ADDR_CONST(0x304)
#define SE_CRYPTO_CONFIG_0_KEY_INDEX_RANGE                      27:24

#define SE_CRYPTO_CONFIG_0_CORE_SEL_RANGE                       8:8
#define SE_CRYPTO_CONFIG_0_CORE_SEL_DECRYPT                     _MK_ENUM_CONST(0)
#define SE_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT                     _MK_ENUM_CONST(1)

#define SE_CRYPTO_CONFIG_0_IV_SELECT_RANGE                      7:7
#define SE_CRYPTO_CONFIG_0_IV_SELECT_UPDATED                    _MK_ENUM_CONST(1)

#define SE_CRYPTO_CONFIG_0_VCTRAM_SEL_RANGE                     6:5
#define SE_CRYPTO_CONFIG_0_VCTRAM_SEL_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_VCTRAM_SEL_INIT_AESOUT                       _MK_ENUM_CONST(2)
#define SE_CRYPTO_CONFIG_0_VCTRAM_SEL_INIT_PREVAHB                      _MK_ENUM_CONST(3)

#define SE_CRYPTO_CONFIG_0_INPUT_SEL_RANGE                      4:3
#define SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB                        _MK_ENUM_CONST(0)

#define SE_CRYPTO_CONFIG_0_XOR_POS_RANGE                        2:1
#define SE_CRYPTO_CONFIG_0_XOR_POS_BYPASS                       _MK_ENUM_CONST(0)
#define SE_CRYPTO_CONFIG_0_XOR_POS_TOP                  _MK_ENUM_CONST(2)
#define SE_CRYPTO_CONFIG_0_XOR_POS_BOTTOM                       _MK_ENUM_CONST(3)

#define SE_CRYPTO_CONFIG_0_HASH_ENB_RANGE                       0:0
#define SE_CRYPTO_CONFIG_0_HASH_ENB_ENABLE                      _MK_ENUM_CONST(1)
// Register SE_CRYPTO_LAST_BLOCK_0
#define SE_CRYPTO_LAST_BLOCK_0                  _MK_ADDR_CONST(0x318)

// Register SE_CRYPTO_KEYTABLE_ACCESS_0
#define SE_CRYPTO_KEYTABLE_ACCESS_0                     _MK_ADDR_CONST(0x284)
#define SE_CRYPTO_KEYTABLE_ACCESS_0_KEYREAD_RANGE                       0:0
#define SE_CRYPTO_KEYTABLE_ACCESS_0_KEYUPDATE_RANGE                       1:1
#define SE_CRYPTO_KEYTABLE_ACCESS_0_KEYREAD_DISABLE                     _MK_ENUM_CONST(0)
#define SE_CRYPTO_KEYTABLE_ACCESS_0_KEYUPDATE_DISABLE                   _MK_ENUM_CONST(0)

#define SE_CONFIG_0_ENC_ALG_RNG                 _MK_ENUM_CONST(2)
#define SE_CRYPTO_KEYTABLE_DST_0_KEY_INDEX_RANGE                        11:8
#define SE_CRYPTO_KEYTABLE_DST_0_WORD_QUAD_KEYS_0_3                     _MK_ENUM_CONST(0)
#define SE_CRYPTO_KEYTABLE_DST_0_WORD_QUAD_RANGE                        1:0
#define SE_CRYPTO_KEYTABLE_DST_0                        _MK_ADDR_CONST(0x330)
#define SE_CRYPTO_CONFIG_0_INPUT_SEL_RANDOM                     _MK_ENUM_CONST(1)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_SHIFT                      _MK_SHIFT_CONST(7)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_FIELD                      _MK_FIELD_CONST(0x1, SE_CRYPTO_CONFIG_0_IV_SELECT_SHIFT)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_RANGE                      7:7
#define SE_CRYPTO_CONFIG_0_IV_SELECT_WOFFSET                    0x0
#define SE_CRYPTO_CONFIG_0_IV_SELECT_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_ORIGINAL                   _MK_ENUM_CONST(0)
#define SE_CRYPTO_CONFIG_0_IV_SELECT_UPDATED                    _MK_ENUM_CONST(1)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_SHIFT                       _MK_SHIFT_CONST(0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_FIELD                       _MK_FIELD_CONST(0x1, SE_CRYPTO_CONFIG_0_HASH_ENB_SHIFT)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_RANGE                       0:0
#define SE_CRYPTO_CONFIG_0_HASH_ENB_WOFFSET                     0x0
#define SE_CRYPTO_CONFIG_0_HASH_ENB_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_DISABLE                     _MK_ENUM_CONST(0)
#define SE_CRYPTO_CONFIG_0_HASH_ENB_ENABLE                      _MK_ENUM_CONST(1)

// Register SE_RNG_CONFIG_0
#define SE_RNG_CONFIG_0                 _MK_ADDR_CONST(0x340)
#define SE_RNG_CONFIG_0_SECURE                  0x0
#define SE_RNG_CONFIG_0_WORD_COUNT                      0x1
#define SE_RNG_CONFIG_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_RESET_MASK                      _MK_MASK_CONST(0xf)
#define SE_RNG_CONFIG_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_READ_MASK                       _MK_MASK_CONST(0xf)
#define SE_RNG_CONFIG_0_WRITE_MASK                      _MK_MASK_CONST(0xf)
#define SE_RNG_CONFIG_0_SRC_SHIFT                       _MK_SHIFT_CONST(2)
#define SE_RNG_CONFIG_0_SRC_FIELD                       _MK_FIELD_CONST(0x3, SE_RNG_CONFIG_0_SRC_SHIFT)
#define SE_RNG_CONFIG_0_SRC_RANGE                       3:2
#define SE_RNG_CONFIG_0_SRC_WOFFSET                     0x0
#define SE_RNG_CONFIG_0_SRC_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_SRC_DEFAULT_MASK                        _MK_MASK_CONST(0x3)
#define SE_RNG_CONFIG_0_SRC_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_SRC_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_SRC_INIT_ENUM                   NONE
#define SE_RNG_CONFIG_0_SRC_NONE                        _MK_ENUM_CONST(0)
#define SE_RNG_CONFIG_0_SRC_ENTROPY                     _MK_ENUM_CONST(1)
#define SE_RNG_CONFIG_0_SRC_LFSR                        _MK_ENUM_CONST(2)

#define SE_RNG_CONFIG_0_MODE_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_RNG_CONFIG_0_MODE_FIELD                      _MK_FIELD_CONST(0x3, SE_RNG_CONFIG_0_MODE_SHIFT)
#define SE_RNG_CONFIG_0_MODE_RANGE                      1:0
#define SE_RNG_CONFIG_0_MODE_WOFFSET                    0x0
#define SE_RNG_CONFIG_0_MODE_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_MODE_DEFAULT_MASK                       _MK_MASK_CONST(0x3)
#define SE_RNG_CONFIG_0_MODE_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_MODE_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_RNG_CONFIG_0_MODE_INIT_ENUM                  NORMAL
#define SE_RNG_CONFIG_0_MODE_NORMAL                     _MK_ENUM_CONST(0)
#define SE_RNG_CONFIG_0_MODE_FORCE_INSTANTION                   _MK_ENUM_CONST(1)
#define SE_RNG_CONFIG_0_MODE_FORCE_RESEED                       _MK_ENUM_CONST(2)


// Register SE_RNG_SRC_CONFIG_0
#define SE_RNG_SRC_CONFIG_0                     _MK_ADDR_CONST(0x344)
#define SE_RNG_SRC_CONFIG_0_SECURE                      0x0
#define SE_RNG_SRC_CONFIG_0_WORD_COUNT                  0x1
#define SE_RNG_SRC_CONFIG_0_RESET_VAL                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RESET_MASK                  _MK_MASK_CONST(0x177)
#define SE_RNG_SRC_CONFIG_0_SW_DEFAULT_VAL                      _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_READ_MASK                   _MK_MASK_CONST(0x177)
#define SE_RNG_SRC_CONFIG_0_WRITE_MASK                  _MK_MASK_CONST(0x177)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_SHIFT                 _MK_SHIFT_CONST(8)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_FIELD                 _MK_FIELD_CONST(0x1, SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_SHIFT)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_RANGE                 8:8
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_WOFFSET                       0x0
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_DEFAULT                       _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_DEFAULT_MASK                  _MK_MASK_CONST(0x1)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_SW_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_DATA_FLUSH_SW_DEFAULT_MASK                       _MK_MASK_CONST(0x0)

#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_SHIFT                  _MK_SHIFT_CONST(4)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_FIELD                  _MK_FIELD_CONST(0x7, SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_SHIFT)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_RANGE                  6:4
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_WOFFSET                        0x0
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_DEFAULT_MASK                   _MK_MASK_CONST(0x7)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SUBSAMPLE_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_SHIFT                     _MK_SHIFT_CONST(2)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_FIELD                     _MK_FIELD_CONST(0x1, SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_SHIFT)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_RANGE                     2:2
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_WOFFSET                   0x0
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_INIT_ENUM                 DISABLE
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_DISABLE                   _MK_ENUM_CONST(0)
#define SE_RNG_SRC_CONFIG_0_RO_HW_DISABLE_CYA_ENABLE                    _MK_ENUM_CONST(1)

#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_SHIFT                     _MK_SHIFT_CONST(1)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_FIELD                     _MK_FIELD_CONST(0x1, SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_SHIFT)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_RANGE                     1:1
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_WOFFSET                   0x0
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_DEFAULT_MASK                      _MK_MASK_CONST(0x1)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_INIT_ENUM                 ENABLE
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_ENABLE                    _MK_ENUM_CONST(0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_DISABLE                   _MK_ENUM_CONST(1)

#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_SHIFT                        _MK_SHIFT_CONST(0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_FIELD                        _MK_FIELD_CONST(0x1, SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_SHIFT)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_RANGE                        0:0
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_WOFFSET                      0x0
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_DEFAULT_MASK                 _MK_MASK_CONST(0x1)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_INIT_ENUM                    DISABLE
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_DISABLE                      _MK_ENUM_CONST(0)
#define SE_RNG_SRC_CONFIG_0_RO_ENTROPY_SOURCE_LOCK_ENABLE                       _MK_ENUM_CONST(1)


// Register SE_RNG_RESEED_INTERVAL_0
#define SE_RNG_RESEED_INTERVAL_0                        _MK_ADDR_CONST(0x348)
#define SE_RNG_RESEED_INTERVAL_0_SECURE                         0x0
#define SE_RNG_RESEED_INTERVAL_0_WORD_COUNT                     0x1
#define SE_RNG_RESEED_INTERVAL_0_RESET_VAL                      _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_RESET_MASK                     _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_SW_DEFAULT_VAL                         _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_READ_MASK                      _MK_MASK_CONST(0xffffffff)
#define SE_RNG_RESEED_INTERVAL_0_WRITE_MASK                     _MK_MASK_CONST(0xffffffff)
#define SE_RNG_RESEED_INTERVAL_0_VAL_SHIFT                      _MK_SHIFT_CONST(0)
#define SE_RNG_RESEED_INTERVAL_0_VAL_FIELD                      _MK_FIELD_CONST(0xffffffff, SE_RNG_RESEED_INTERVAL_0_VAL_SHIFT)
#define SE_RNG_RESEED_INTERVAL_0_VAL_RANGE                      31:0
#define SE_RNG_RESEED_INTERVAL_0_VAL_WOFFSET                    0x0
#define SE_RNG_RESEED_INTERVAL_0_VAL_DEFAULT                    _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_VAL_DEFAULT_MASK                       _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_VAL_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define SE_RNG_RESEED_INTERVAL_0_VAL_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)


// Register SE_CTX_SAVE_CONFIG_0
#define SE_CTX_SAVE_CONFIG_0                    _MK_ADDR_CONST(0x70)
#define SE_CTX_SAVE_CONFIG_0_SECURE                     0x0
#define SE_CTX_SAVE_CONFIG_0_WORD_COUNT                         0x1
#define SE_CTX_SAVE_CONFIG_0_RESET_VAL                  _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RESET_MASK                         _MK_MASK_CONST(0x30000)
#define SE_CTX_SAVE_CONFIG_0_SW_DEFAULT_VAL                     _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_SW_DEFAULT_MASK                    _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_READ_MASK                  _MK_MASK_CONST(0xe103ff03)
#define SE_CTX_SAVE_CONFIG_0_WRITE_MASK                         _MK_MASK_CONST(0xe103ff03)
#define SE_CTX_SAVE_CONFIG_0_SRC_SHIFT                  _MK_SHIFT_CONST(29)
#define SE_CTX_SAVE_CONFIG_0_SRC_FIELD                  _MK_FIELD_CONST(0x7, SE_CTX_SAVE_CONFIG_0_SRC_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_SRC_RANGE                  31:29
#define SE_CTX_SAVE_CONFIG_0_SRC_WOFFSET                        0x0
#define SE_CTX_SAVE_CONFIG_0_SRC_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_SRC_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_SRC_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_SRC_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_SRC_STICKY_BITS                    _MK_ENUM_CONST(0)
#define SE_CTX_SAVE_CONFIG_0_SRC_AES_KEYTABLE                   _MK_ENUM_CONST(2)
#define SE_CTX_SAVE_CONFIG_0_SRC_MEM                    _MK_ENUM_CONST(4)
#define SE_CTX_SAVE_CONFIG_0_SRC_SRK                    _MK_ENUM_CONST(6)
#define SE_CTX_SAVE_CONFIG_0_SRC_RSA_KEYTABLE                   _MK_ENUM_CONST(1)

#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_SHIFT                     _MK_SHIFT_CONST(24)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_FIELD                     _MK_FIELD_CONST(0x1, SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_RANGE                     24:24
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_WOFFSET                   0x0
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_SW_DEFAULT                        _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_SW_DEFAULT_MASK                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_WORDS_0_3                 _MK_ENUM_CONST(0)
#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_WORDS_4_7                 _MK_ENUM_CONST(1)

#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SHIFT                        _MK_SHIFT_CONST(16)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_FIELD                        _MK_FIELD_CONST(0x3, SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_RANGE                        17:16
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_WOFFSET                      0x0
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_DEFAULT_MASK                 _MK_MASK_CONST(0x3)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_INIT_ENUM                    SLOT0_EXPONENT
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SLOT0_EXPONENT                       _MK_ENUM_CONST(0)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SLOT0_MODULUS                        _MK_ENUM_CONST(1)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SLOT1_EXPONENT                       _MK_ENUM_CONST(2)
#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SLOT1_MODULUS                        _MK_ENUM_CONST(3)

#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_SHIFT                        _MK_SHIFT_CONST(12)
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_FIELD                        _MK_FIELD_CONST(0xf, SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_RANGE                        15:12
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_WOFFSET                      0x0
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_SHIFT                        _MK_SHIFT_CONST(8)
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_FIELD                        _MK_FIELD_CONST(0xf, SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_RANGE                        11:8
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_WOFFSET                      0x0
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)

#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_SHIFT                        _MK_SHIFT_CONST(0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_FIELD                        _MK_FIELD_CONST(0x3, SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_SHIFT)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_RANGE                        1:0
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_WOFFSET                      0x0
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_DEFAULT                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_DEFAULT_MASK                 _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_KEYS_0_3                     _MK_ENUM_CONST(0)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_KEYS_4_7                     _MK_ENUM_CONST(1)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_ORIGINAL_IVS                 _MK_ENUM_CONST(2)
#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_UPDATED_IVS                  _MK_ENUM_CONST(3)


//RSA
#define SE_OPERATION_0_OP_RESTART_IN                    _MK_ENUM_CONST(4)
#define SE_RSA_KEY_SIZE_0_VAL_WIDTH_512                 _MK_ENUM_CONST(0)
#define SE_RSA_KEY_SIZE_0_VAL_WIDTH_1024                        _MK_ENUM_CONST(1)
#define SE_RSA_KEY_SIZE_0_VAL_WIDTH_1536                        _MK_ENUM_CONST(2)
#define SE_RSA_KEY_SIZE_0_VAL_WIDTH_2048                        _MK_ENUM_CONST(3)
#define SE_RSA_EXP_SIZE_0                       _MK_ADDR_CONST(0x408)
#define SE_RSA_KEYTABLE_DATA_0                  _MK_ADDR_CONST(0x424)
#define SE_RSA_KEY_PKT_KEY_SLOT_SHIFT                   _MK_SHIFT_CONST(7)
#define SE_RSA_KEY_PKT_EXPMOD_SEL_MODULUS                       _MK_ENUM_CONST(1)
#define SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT                 _MK_SHIFT_CONST(6)
#define SE_RSA_KEY_PKT_INPUT_MODE_REGISTER                      _MK_ENUM_CONST(0)
#define SE_RSA_KEY_PKT_INPUT_MODE_SHIFT                 _MK_SHIFT_CONST(8)
#define SE_RSA_KEY_PKT_WORD_ADDR_FIELD                  _MK_FIELD_CONST(0x3f, SE_RSA_KEY_PKT_WORD_ADDR_SHIFT)
#define SE_RSA_KEYTABLE_ADDR_0_OP_WRITE                 _MK_ENUM_CONST(1)
#define SE_RSA_KEYTABLE_ADDR_0_OP_RANGE                 10:10
#define SE_RSA_KEYTABLE_ADDR_0_PKT_RANGE                        8:0
#define SE_RSA_KEYTABLE_ADDR_0                  _MK_ADDR_CONST(0x420)
#define SE_RSA_KEY_PKT_EXPMOD_SEL_EXPONENT                      _MK_ENUM_CONST(0)
#define SE_CONFIG_0_DST_RSA_REG                 _MK_ENUM_CONST(4)
#define SE_CONFIG_0_ENC_ALG_RSA                 _MK_ENUM_CONST(4)
#define SE_RSA_CONFIG_0_KEY_SLOT_RANGE                  24:24
#define SE_RSA_CONFIG_0                 _MK_ADDR_CONST(0x400)
#define SE_RSA_OUTPUT_0                 _MK_ADDR_CONST(0x428)
#define SE_RSA_KEY_SIZE_0                       _MK_ADDR_CONST(0x404)
#define SE_RSA_KEY_PKT_WORD_ADDR_SHIFT                  _MK_SHIFT_CONST(0)

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

#if defined(__cplusplus)
}
#endif

#endif // ifndef INCLUDED_NVDDK_SE_PRIVATE_H
