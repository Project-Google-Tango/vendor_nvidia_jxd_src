/*
 * Copyright (c) 2006-2010 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         DTV Tuner Interface</b>
 *
 * @b Description: Defines the DTV tuner adaptation interface.
 */

#ifndef INCLUDED_NVODM_DTVTUNER_H
#define INCLUDED_NVODM_DTVTUNER_H


#if defined(_cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"

/**
 * @defgroup nvodm_dtvtuner DTV tuner Adaptation Interface
 *  
 *  This API handles the abstraction of external DTV tuners.
 *
 *  This API access the external device only. It does not program 
 *  registers. It can be replaced by external 3rd party programs if so
 *  desired.
 *
 * @ingroup nvodm_adaptation
 * @{
 */

/**
 * Defines the TV tuner types.
 */
typedef enum
{
    /** Specifies ISDB-T tuner. */ 
    NvOdmDtvtunerType_ISDBT = 1,

    /** Specifies DVB-H tuner. */ 
    NvOdmDtvtunerType_DVBH,

    /** Specifies DVB-T tuner. */ 
    NvOdmDtvtunerType_DVBT,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmDtvtunerType_Force32 = 0x7FFFFFFF
} NvOdmDtvtunerType;

/**
 * Defines the connection interface.
 */
typedef enum
{
    /** Specifies to connect through VIP. */ 
    NvOdmDtvtunerConnect_VIP = 1,

    /** Specifies to connect through SPI. */ 
    NvOdmDtvtunerConnect_SPI,

    /** Specifies to connect through SDIO. */ 
    NvOdmDtvtunerConnect_SDIO,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmDtvtunerConnect_Force32 = 0x7FFFFFFF

} NvOdmDtvtunerConnection;

/**
 * Defines DTV tuner power levels.
 */
typedef enum NvOdmDtvtunerPowerLevelREC
{
    NvOdmDtvtunerPowerLevel_Off = 1,
    NvOdmDtvtunerPowerLevel_Suspend,
    NvOdmDtvtunerPowerLevel_On,
    NvOdmDtvtunerPowerLevel_Force32 = 0x7FFFFFFF,
} NvOdmDtvtunerPowerLevel;

/**
 * Defines DTV tuner timing data. 
 */
typedef enum
{
  NvOdmDtvtuner_StoreUpstreamErrorPktsParam_Discard = 1,
  NvOdmDtvtuner_StoreUpstreamErrorPktsParam_Store,
  NvOdmDtvtuner_StoreUpstreamErrorPktsParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_StoreUpstreamErrorPktsParam;
typedef enum
{
  NvOdmDtvtuner_BodyValidSelectParam_Ignore = 1,
  NvOdmDtvtuner_BodyValidSelectParam_Gate,
  NvOdmDtvtuner_BodyValidSelectParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_BodyValidSelectParam;
typedef enum
{
  NvOdmDtvtuner_StartSelectParam_Psync = 1,
  NvOdmDtvtuner_StartSelectParam_Valid,
  NvOdmDtvtuner_StartSelectParam_Both,
  NvOdmDtvtuner_StartSelectParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_StartSelectParam;
typedef enum
{
  NvOdmDtvtuner_ClockPolarityParam_Low = 1,
  NvOdmDtvtuner_ClockPolarityParam_High,
  NvOdmDtvtuner_ClockPolarityParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_ClockPolarityParam;
typedef enum
{
  NvOdmDtvtuner_ErrorPolarityParam_Low = 1,
  NvOdmDtvtuner_ErrorPolarityParam_High,
  NvOdmDtvtuner_ErrorPolarityParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_ErrorPolarityParam;
typedef enum
{
  NvOdmDtvtuner_PsyncPolarityParam_Low = 1,
  NvOdmDtvtuner_PsyncPolarityParam_High,
  NvOdmDtvtuner_PsyncPolarityParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_PsyncPolarityParam;
typedef enum
{
  NvOdmDtvtuner_ValidPolarityParam_Low = 1,
  NvOdmDtvtuner_ValidPolarityParam_High,
  NvOdmDtvtuner_ValidPolarityParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_ValidPolarityParam;
typedef enum
{
  NvOdmDtvtuner_ProtocolSelectParam_TsError = 1,
  NvOdmDtvtuner_ProtocolSelectParam_TsPsync,
  NvOdmDtvtuner_ProtocolSelectParam_None,
  NvOdmDtvtuner_ProtocolSelectParam_Force32 = 0x7fffffff
} NvOdmDtvtuner_ProtocolSelectParam;

/** Defines a container for DTV tuner VIP port timing. */
typedef struct NvOdmDtvtunerVIPTimingREC
{
    NvU32 FecSize;  //!< Specifies the byte-size of checksum following TS packet, usually 16 bytes.
    NvU32 BodySize; //!< Specifies the byte-size of TS packet, usually 188 bytes.

    /** Specifies to capture packets marked as error. */
    NvOdmDtvtuner_StoreUpstreamErrorPktsParam StoreUpstreamErrorPkts; 

    /** Specifies to capture TS_VALID gates data. */
    NvOdmDtvtuner_BodyValidSelectParam BodyValidSelect;

    NvOdmDtvtuner_StartSelectParam StartSelect;        //!< Specifies the packet start trigger mode.
    NvOdmDtvtuner_ClockPolarityParam ClockPolarity;    //!< Specifies the polarity for TS_CLOCK.
    NvOdmDtvtuner_ErrorPolarityParam ErrorPolarity;    //!< Specifies the polarity for TS_ERROR.
    NvOdmDtvtuner_PsyncPolarityParam PsyncPolarity;    //!< Specifies the polarity for TS_PSYNC.
    NvOdmDtvtuner_ValidPolarityParam ValidPolarity;    //!< Specifies the polarity for TS_VALID.

    /** Specifies the pin configuration for VIP VD1.
        The following options are available:
        - TsError: TS_ERROR is on VD1, TS_PSYNC is tied to 0
        - TsPsync  TS_ERROR is tied to 0, TS_PSYNC is on VD1
        - None:    TS_ERROR is tied to 0, TS_PSYNC is tied to 0
    */
    NvOdmDtvtuner_ProtocolSelectParam ProtocolSelect;

} NvOdmDtvtunerVIPTiming;

/** Defines DTV tuner properties. */
typedef struct NvOdmDtvtunerCapREC
{
    /**
     * Specifies the globally unique identifier (GUID) of the tuner.
     */
    NvU64 guid;

    /**
     * Specifies the tuner type, as enum in ::NvOdmDtvtunerType.
     */
    NvOdmDtvtunerType TunerType;

    /**
     * Specifies the connection interface of the tuner, as enum in ::NvOdmDtvtunerConnection.
     */
    NvOdmDtvtunerConnection DeviceConnection;

    /**
     * Specifies VIP timing data of the tuner, if it is connected via VIP.
     */
    NvOdmDtvtunerVIPTiming VIPTiming;
} NvOdmDtvtunerCap;

/**
 * Defines ISDB-T tuner segment settings.
 */
typedef enum
{
    NvOdmISDBTtuner1SegTVUHF = 1,
    NvOdmISDBTtuner1SegRadioVHF,
    NvOdmISDBTtuner3SegRadioVHF,
    NvOdmISDBTtunerSegment_Force32 = 0x7FFFFFFF
} NvOdmISDBTSegment;

/**
 * Defines ISDB-t tuner init informations.
 */
typedef struct NvOdmDtvtunerISDBTInitREC
{
    NvOdmISDBTSegment seg;
    NvU32 channel;
}NvOdmDtvtunerISDBTInit;

/**
 * Defines ISDB-t tuner segment, chammel and PID informations.
 */
typedef struct NvOdmDtvtunerISDBTSetSegChPidlREC
{
    NvOdmISDBTSegment seg;
    NvU32 channel;
    NvU32 PID;
}NvOdmDtvtunerISDBTSetSegChPid;
 
/**
 * Defines TV signal informations.
 */
typedef struct NvOdmDtvtunerStatREC
{
    NvU32 SignalStrength;
    NvU32 BitErrorRate;
}NvOdmDtvtunerStat;

/**
 * Defines the DTV tuner parameters.
 */
typedef enum
{
    /** Get tuner device capibility.
        To be implemented in NvOdmDtvtunerGetParameter() only.
        Meaning of block passed in parameter \a pValue:
        <pre>
        NvOdmDtvtunerCap capibility structure
        </pre>
    */
    NvOdmDtvtunerParameter_Cap,

    /** Initialize DTV tuner. For ISDB-t tuner only
        To be implemented in NvOdmDtvtunerSetParameter() only.
        Meaning of block passed in parameter \a pValue:
        <pre>
        NvOdmDtvtunerISDBTInit init structure
        </pre>
    */
    NvOdmDtvtunerParameter_ISDBTInit,

    /** Set ISDB-t tuner segment, channel and PID. For ISDB-t tuner only
        To be implemented in NvOdmDtvtunerSetParameter() only.
        Meaning of block passed in parameter \a pValue:
        <pre>
        NvOdmDtvtunerISDBTSetSegChPid segment, channel and pid
        </pre>
    */
    NvOdmDtvtunerParameter_ISDBTSetSegChPid,

    /** Get tuner singal statistic.
        To be implemented in NvOdmDtvtunerGetParameter() only.
        Meaning of block passed in parameter \a pValue:
        <pre>
        NvOdmDtvtunerStat signal stat
        </pre>
    */
    NvOdmDtvtunerParameter_DtvtunerStat,

    NvOdmDtvtunerParameter_Force32 = 0x7FFFFFFF
} NvOdmDtvtunerParameter;

/** TV tuner device context. */
typedef struct NvOdmDtvtunerContextREC* NvOdmDtvtunerHandle;

/** Opens a TV tuner.
 *  Will open the current tuner.
 *  @return A device handle on success, or NULL on failure.
*/
NvOdmDtvtunerHandle NvOdmDtvtunerOpen(void);

/** Closes a TV tuner.
 *  @param phDTVTuner A pointer to a TV tuner device handle; 
 *                    will be reset to NULL; \a *phDTVTuner can be NULL.
*/
void NvOdmDtvtunerClose(NvOdmDtvtunerHandle* phDTVTuner);

/** Sets the TV tuner to on, off, or suspend.
 *  @param hDTVTuner The TV tuner device handle.
 *  @param PowerLevel Specifies the power level to set.
*/
void NvOdmDtvtunerSetPowerLevel(
        NvOdmDtvtunerHandle hDTVTuner,
        NvOdmDtvtunerPowerLevel PowerLevel);

/** Changes a tuner device setting.
 * @param hDtvtuner The DTV tuner device handle.
 * @param Param A parameter ID.
 * @param SizeOfValue The byte size of buffer in \a value.
 * @param pValue A pointer to the buffer to hold parameter value.
 * @return NV_TRUE if successful, or NV_FALSE otherwise, such as if the
 * parameter is not supported or the parameter is not set.
 */
NvBool NvOdmDtvtunerSetParameter(
        NvOdmDtvtunerHandle hDtvtuner,
        NvOdmDtvtunerParameter Param,
        NvU32 SizeOfValue,
        const void *pValue);

/** Queries a variable-sized DTV tuner device parameter.
 * @param hDtvtuner The DTV tuner device handle
 * @param Param The parameter ID.
 * @param SizeOfValue The byte size of buffer in \a value.
 * @param pValue A pointer to the buffer to receive the parameter value.
 * @return NV_TRUE if successful, NV_FALSE otherwise.
 */
NvBool NvOdmDtvtunerGetParameter(
        NvOdmDtvtunerHandle hDtvtuner,
        NvOdmDtvtunerParameter Param,
        NvU32 SizeOfValue,
        void *pValue);

#if defined(_cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_DTVTUNER_H

