/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMM_AMRNB_H
#define INCLUDED_NVMM_AMRNB_H


#include "nvmm.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define AMR_MAGIC_NUMBER "#!AMR\n"

typedef enum
{
    NvMMBlockAmrStream_In = 0,
    NvMMBlockAmrStream_Out,
    NvMMBlockAmrStreamCount

} NvMMBlockAmrStream;

typedef enum
{
    NvMMAudioAmrMode_475 = 0,
    NvMMAudioAmrMode_515,
    NvMMAudioAmrMode_59,
    NvMMAudioAmrMode_67,
    NvMMAudioAmrMode_74,
    NvMMAudioAmrMode_795,
    NvMMAudioAmrMode_102,
    NvMMAudioAmrMode_122,
    NvMMAudioAmrMode_DTX,
    NvMMAudioAmrMode_Force32 = 0x7FFFFFFF
}NvMMAudioAmrMode;

typedef enum
{
    NvMMAmrnbAttribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 200),
    NvMMAudioAmrAttribute_All,
    NvMMAudioAmrAttribute_Mode,
    NvMMAudioAmrAttribute_DTxState,
    NvMMAudioAmrAttribute_Tx_Type,
    NvMMAudioAmrAttribute_Rx_Type,
    NvMMAudioAmrAttribute_MMS_Format,
    NvMMAudioAmrAttribute_MMS_Inp,
    NvMMAudioAmrAttribute_Used_Mode,
    NvMMAudioAmrAttribute_EnableProfiling,
    NvMMAudioAmrAttribute_Conformance,
    NvMMAudioAmrAttribute_Force32 = 0x7FFFFFFF

} NvMMAudioAmrAttribute;


typedef struct
{
    NvU32 structSize;
    NvU32   FileFormat;
    NvMMAudioAmrMode   Mode;
    NvU32 DTxState;

} NvMMAudioEncPropertyAMRNB;

/** AMR Frame format */
typedef enum  {
    NvMM_AMRFrameFormatUnused = 0,
    NvMM_AMRFrameFormatConformance,  /**< Frame Format is AMR Conformance
                                                   (Standard) Format */
    NvMM_AMRFrameFormatIF1,              /**< Frame Format is AMR Interface
                                                   Format 1 */
    NvMM_AMRFrameFormatIF2,              /**< Frame Format is AMR Interface
                                                   Format 2*/
    NvMM_AMRFrameFormatFSF,              /**< Frame Format is AMR File Storage
                                                   Format */
    NvMM_AMRFrameFormatRTPPayload,       /**< Frame Format is AMR Real-Time
                                                   Transport Protocol Payload Format */
    NvMM_AMRFrameFormatITU,              /**< Frame Format is ITU Format (added at Motorola request) */
    NvMM_AMRFrameFormatMax = 0x7FFFFFFF
} NvAmrFrameFormat;


/** AMR band mode */
typedef enum {
    NvMM_AMRBandModeUnused = 0,          /**< AMRNB Mode unused / unknown */
    NvMM_AMRBandModeNB0,                 /**< AMRNB Mode 0 =  4750 bps */
    NvMM_AMRBandModeNB1,                 /**< AMRNB Mode 1 =  5150 bps */
    NvMM_AMRBandModeNB2,                 /**< AMRNB Mode 2 =  5900 bps */
    NvMM_AMRBandModeNB3,                 /**< AMRNB Mode 3 =  6700 bps */
    NvMM_AMRBandModeNB4,                 /**< AMRNB Mode 4 =  7400 bps */
    NvMM_AMRBandModeNB5,                 /**< AMRNB Mode 5 =  7950 bps */
    NvMM_AMRBandModeNB6,                 /**< AMRNB Mode 6 = 10200 bps */
    NvMM_AMRBandModeNB7,                 /**< AMRNB Mode 7 = 12200 bps */
    NvMM_AMRBandModeWB0,                 /**< AMRWB Mode 0 =  6600 bps */
    NvMM_AMRBandModeWB1,                 /**< AMRWB Mode 1 =  8850 bps */
    NvMM_AMRBandModeWB2,                 /**< AMRWB Mode 2 = 12650 bps */
    NvMM_AMRBandModeWB3,                 /**< AMRWB Mode 3 = 14250 bps */
    NvMM_AMRBandModeWB4,                 /**< AMRWB Mode 4 = 15850 bps */
    NvMM_AMRBandModeWB5,                 /**< AMRWB Mode 5 = 18250 bps */
    NvMM_AMRBandModeWB6,                 /**< AMRWB Mode 6 = 19850 bps */
    NvMM_AMRBandModeWB7,                 /**< AMRWB Mode 7 = 23050 bps */
    NvMM_AMRBandModeWB8,                 /**< AMRWB Mode 8 = 23850 bps */
    NvMM_AMRBandModeMax = 0x7FFFFFFF
} NvAmrBandModeType;

/** AMR Discontinuous Transmission mode */
typedef enum  {
    NvMM_AMRDTXModeUnused = 0,
    NvMM_AMRDTXModeOff,        /**< AMR Discontinuous Transmission Mode is disabled */
    NvMM_AMRDTXModeOnVAD1,         /**< AMR Discontinuous Transmission Mode using
                                             Voice Activity Detector 1 (VAD1) is enabled */
    NvMM_AMRDTXModeOnVAD2,         /**< AMR Discontinuous Transmission Mode using
                                             Voice Activity Detector 2 (VAD2) is enabled */
    NvMM_AMRDTXModeOnAuto,         /**< The codec will automatically select between
                                             Off, VAD1 or VAD2 modes */

    NvMM_AMRDTXasEFR,             /**< DTX as EFR instead of AMR standard (3GPP 26.101, frame type =8,9,10) */
    NvMM_AMRDTXModeMax = 0x7FFFFFFF
} NvAmrDtxMode;

/** AMR params */
typedef struct  {
    NvU32 nChannels;                      /**< Number of channels */
    NvU32 nBitRate;                       /**< Bit rate read only field */
    NvAmrBandModeType eAMRBandMode; /**< AMR Band Mode enumeration */
    NvAmrDtxMode  eAMRDTXMode;  /**< AMR DTX Mode enumeration */
    NvAmrFrameFormat eAMRFrameFormat; /**< AMR frame format enumeration */
} NvmmAudioParamAmr;


#if defined(__cplusplus)
}
#endif


#endif // INCLUDED_NVMM_AMRNB_H
