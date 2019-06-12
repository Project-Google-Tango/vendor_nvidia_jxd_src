/*
 * Copyright (c) 2007-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Video Encode control</b>
 *
 * @b Description: Defines the common structures for video encoders
 */

#ifndef INCLUDED_NVVIDEOENC_COMMON_H
#define INCLUDED_NVVIDEOENC_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define NV_VIDEOENC_STATE_COMMON_SETUP_DONE     0x00000001
#define NV_VIDEOENC_STATE_BITSTREAM_SETUP_DONE  0x00000002
#define NV_VIDEOENC_STATE_INIT                  0x00000004
#define NV_VIDEOENC_STATE_RUN                   0x00000008
#define NV_VIDEOENC_STATE_PAUSE                 0x00000010

#define MEM_ALIGN_8(X)          (((X) + 0x7)&~0x7)
#define MEM_ALIGN_16(X)         (((X) + 0xF)&~0xF)
#define MEM_ALIGN_32(X)         (((X) + 0x1F)&~0x1F)
#define MEM_ALIGN_64(X)         (((X) + 0x3F)&~0x3F)
#define MEM_ALIGN_128(X)        (((X) + 0x7F)&~0x7F)
#define MEM_ALIGN_256(X)        (((X) + 0xFF)&~0xFF)
#define MEM_ALIGN(X, ALIGN)     (((X) + ALIGN - 1)&~(ALIGN - 1))    // Valid only for power of 2

#define NVMPE_FIXEDPOINT_FBITS      2
#define NVMPE_FIXEDPOINT_MUL        (1<<NVMPE_FIXEDPOINT_FBITS)
#define NVMPE_FLOAT_TO_FIXED(X) \
      ((NvS32)((X)*((float)NVMPE_FIXEDPOINT_MUL)))
#define NVMPE_FIXED_TO_FLOAT(X) \
      (((float)(X))/((float)NVMPE_FIXEDPOINT_MUL))

#define EXTRA_PIX_ROWS_IN_REFF  96
#define QD_INPBUFS_LIMIT 1
#define MSENC_QD_INPBUFS_LIMIT 1

#define CONVERT_TO_FIXED(_type, _field, _factor) ((_type) (((double)(_field))*((double)(_factor))))
#define CLIP3(low,hi,x) (((x) < (low)) ? (low) : (((x) > (hi)) ? (hi) : (x)))
#define ABS(x) (((x) < 0) ? (-(x)):(x))

enum { MAX_B_FRAMES   =  2 };
enum { MAX_EBM_BUFFERS = 16 };
enum { SUB_QCIF_WIDTH = 128, SUB_QCIF_HEIGHT = 96 };
enum { QCIF_WIDTH = 176, QCIF_HEIGHT = 144 };
enum { CIF_WIDTH = 352, CIF_HEIGHT = 288 };
enum { FOUR_CIF_WIDTH = 704, FOUR_CIF_HEIGHT = 576 };
enum { VGA_WIDTH = 640, VGA_HEIGHT = 480 };
enum { D1_WIDTH = 720, D1_HEIGHT = 480 };
enum { P720_WIDTH = 1280, P720_HEIGHT = 720 };
enum { P1920_WIDTH = 1920, P1080_HEIGHT = 1080, P1088_HEIGHT = 1088 };
enum { P2560_WIDTH = 2560, P1440_HEIGHT = 1440 };

enum {
    EVENT_TYPE_NONE = 0x0,
    EVENT_TYPE_EBMEOF = 0x1,
    EVENT_TYPE_SKIPPED = 0x2,
    EVENT_TYPE_EBMEOB = 0x3
};

/**
 * InputBuffer Parameters
 * These parameters are updated with the source sourface parameters
 * They are used as a shadow registers for Input Buffer Registers
 */
typedef struct
{
    NvRmMemHandle hMHBufferAddrY;   // Mem handle for Y
    NvRmMemHandle hMHBufferAddrU;   // Mem handle for U
    NvRmMemHandle hMHBufferAddrV;   // Mem handle for V

    NvU32   FirstBufferVertSize;    // FIRST_IB_V_SIZE
    NvU32   BufferStartAddrYOff;    // IB0_START_ADDR_Y
    NvU32   BufferStartAddrUOff;    // IB0_START_ADDR_U
    NvU32   BufferStartAddrVOff;    // IB0_START_ADDR_V
    NvU32   InputBufferVertSize;    // IB0_SIZE
    NvU32   LineStrideLuma;         // IB0_LINE_STRIDE
    NvU32   LineStrideChroma;       // IB0_LINE_STRIDE
    NvU32   BufferStrideLuma;       // IB0_BUFFER_STRIDE_LUMA
    NvU32   BufferStrideChroma;     // IB0_BUFFER_STRIDE_CHROMA
    NvU32   TileBufStride;
    NvU32   IBTileMode;
}NvVEncInputBuffer;

/**
 * EBM Parameters
 * The following defines EBM shadow registers which controls the process of
 * writing final encoded bitstream to the encoded bitstream (EB) buffers
 *
 */
typedef struct
{
    NvU32   NumEBMBuff;
    NvU32   EBSize;
    NvU32   EBActivate;
    NvU32   AllEBActive;
    NvU32   StartAddrEBMOff[MAX_EBM_BUFFERS];
    NvU32   LastOffset;
    NvU32   TimeStamp;
    NvU32   FrameNumber;
    NvU32   FrameLength;
    NvU32   LastEBMBuffer;
    NvU32   FrameType;
    NvU32   StreamId;
    NvU32   EBMOverflow;
    NvU32   ActivateEBBufs;
}NvVEncEBM;

/**
 * Reference/Reconstructed Frame Parameters
 */
typedef struct
{
    NvU32   RefYStride;             // REF_STRIDE
    NvU32   RefUVStride;            // REF_STRIDE
    NvU32   RefBufferLength;        // REF_BUFFER_LEN
}NvVEncRefBuffer;

typedef struct
{
    NvU8    *pSram;
    NvU32   MinForIFrame;               // INTRA_REF_MIN_COUNTER
    NvU32   MinForPFrame;               // INTRA_REF_MIN_COUNTER
    NvU32   LoadCounter;                // INTRA_REF_LOAD_CMD
    NvU32   Counter;                    // INTRA_REF_LOAD_ONE_CMD
    NvU32   MBPerEntry;                 // INTRA_REF_CTRL
    NvU32   IntraRefreshControl;        // INTRA_REF_CTRL
    NvU32   IntraRefMode;               // INTRA_REF_CTRL
    NvU32   IMRFAttributeSet;
}NvVEncIMRF;

typedef struct
{
    NvU32   SyncPntTh;          // Sync Point threshold
    NvU32   SyncPntIdx;         // Sync Point index
}NvVEncSyncPnt;

typedef struct
{
    NvU32    Cond;
    NvU32    Index;
}NvVEncIncrSyncPnt;

/**
 * MPE hardware capabilities struct
 */
typedef struct
{
    NvU32 ChipId;
}NvVEncMPECapabilities;


#define VENC_DEBUG_LOG_L0(x) \
do { \
    if (bEnableEncGeneralLogs) \
       NvOsDebugPrintf x; \
} while(0);

#define VENC_DEBUG_LOG_L1(x) \
do { \
    if (bEnableEncBufferLogs) \
       NvOsDebugPrintf x; \
} while(0);

#define VENC_DEBUG_LOG_L2(x) \
do { \
    if (bEnableEncProfileLogs) \
       NvOsDebugPrintf x; \
} while(0);

#ifdef __cplusplus
}
#endif

#endif
