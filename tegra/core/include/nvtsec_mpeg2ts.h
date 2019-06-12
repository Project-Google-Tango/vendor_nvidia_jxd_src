/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *            TSEC control</b>
 *
 * @b Description: defines the private implementation for TSEC ts decrypter.
 */

#ifndef INCLUDED_NVTSEC_MPEG2TS_H
#define INCLUDED_NVTSEC_MPEG2TS_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned int    uint;
typedef unsigned char   BYTE;
typedef unsigned char   byte;
typedef unsigned int    DWORD;
typedef unsigned short  ushort;

#define ES_BUFFER_SIZE          (0x00000100)
#define TS_PACKET_SIZE          (0x000000c0)
#define ALIGNED_UNIT_SIZE       (0x00001800)

// AES block size constants
#define AES_B           16
#define AES_DW          04
#define AES_RK          44

#define PID_MAINVIDEO           (0x1011)
#define PID_SUBVIDEO            (0x00001b00)

typedef struct
{
    NvU32    patch_mode;
    NvU32    patch_vm_blocks;
    NvU32    codec_type_main;
    NvU32    main_stream_id;
    NvU32    codec_type_sub;
    NvU32    sub_stream_id;
    NvU32    dwTSInBufferSize;
    NvU32    dwTSOutBufferSize;
    NvU32    dwESMainBufferSize;
    NvU32    dwESSubBufferSize;
    NvU32    CtrES0[AES_DW];
    NvU32    CtrES1[AES_DW];
    NvU32    CtrTS[AES_DW];
}NvTsecParams;

typedef struct
{
    void*    TSInHandle;
    void*    KeyStoreHandle;
    void*    VMHandle;
    NvU32    dwTSInBufferOffset;
    NvU32    dwSkipPacketCount;
    NvU32    dwProcessPacketCount;
    NvBool   bEndOfStream;
    void*    pPatchParam;
} NvTsecInputParams;

typedef struct
{
    void*    TSOutHandle;
    void*    ESMainHandle;
    void*    ESSubHandle;
    NvU32*   pTSOutOffset;
    NvU32*   pESMainOffset;
    NvU32*   pESSubOffset;
} NvTsecOutputParams;

NvError
NvTsecOpen(
    void* ppNvTsec);

void
NvTsecClose(
    void *pTsecCtx);

NvError
NvTsecSetAttribute(
    void *pTsecCtx,
    NvU32 AttributeType,
    NvU32 SetAttrFlag,
    NvU32 AttributeSize,
    NvTsecParams *pAttribute);

NvError
NvTsecGetAttribute(
    void *pTsecCtx,
    NvU32 AttributeType,
    NvU32 AttributeSize,
    NvTsecParams *pAttribute);

NvError
NvTsecSetInputParams(
    void *pCtx,
    NvTsecInputParams *pInpParams);

NvError
NvTsecSetOutputParams(
    void *pCtx,
    NvTsecOutputParams *pOutParams);

NvError
NvTsecExecute(
    void *pCtx);

#if defined(__cplusplus)
}
#endif

#endif
