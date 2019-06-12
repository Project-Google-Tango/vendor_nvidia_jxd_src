/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_ULP_KPI_LOGGER_H
#define INCLUDED_NVMM_ULP_KPI_LOGGER_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {
    // Normal mode is used to get the final KPIs on to the output selected.
    KpiMode_Normal = 1,
    // Tuning mode logs parse, idle and read times for a track into memory. 
    KpiMode_Tuning = 2,
    KpiMode_Force32 = 0x7FFFFFFF

}KpiMode;

// This enum is used to update various KPIs with a single call.
typedef enum {
    // Idle Start Flag.
    KpiFlag_IdleStart = 0x00000001,
    // Idle End Flag.
    KpiFlag_IdleEnd = 0x00000002,
    // Read Start Flag.
    KpiFlag_ReadStart = 0x00000004,
    // Read End Flag.
    KpiFlag_ReadEnd = 0x00000008,
    // Parse Start Flag.
    KpiFlag_ParseStart = 0x00000010,
    // Parse End Flag.
    KpiFlag_ParseEnd = 0x00000020,
    // Seek Start Flag.
    KpiFlag_SeekStart = 0x00000040,
    // Seek End Flag.
    KpiFlag_SeekEnd = 0x00000080,
    // File IO Start Flag.
    KpiFlag_FileIOStart = 0x00000040,
    // File IO End Flag.
    KpiFlag_FileIOEnd = 0x00000080,
    // Force32 value.
    KpiFlag_Force32 = 0x7FFFFFFF

}KpiFlag;


enum 
{
    //Conversion value for 100 ns to micro sec and vice versa
    US_100NS = 10,
    //Conversion value for 100 ns to milli sec and vice versa
    MS_100NS = 10000,
    //Conversion value for 100 ns to sec and vice versa
    SEC_100NS = 10000000        
};

 
/**
 *  All the units of time are in 100ns.
 *  In OpenParser funtion, check if KPIs are requested and call NvmmUlpKpiLoggerInit().
 *  Following that set KPI mode using NvmmUlpSetKpiMode() API.
 *  Call NvmmUlpKpiSetIdleStartTime() after the mode is set.
 *
 *  In GetNextWorkUnit() funtion, call NvmmUlpKpiSetIdleEndTime() and NvmmUlpKpiSetParseStartTime().
 *  Call NvmmUlpKpiSetReadStartTime() and NvmmUlpKpiSetReadEndTime() before and after the 
 *  read calls.
 * 
 *  At the end of GetNextWorkUnit() function, call NvmmUlpKpiSetParseEndTime() and 
 *  NvmmUlpKpiSetIdleStartTime().
 * 
 *  These values will be used to calculate total read time, parse time and idle time in normal mode
 *  and the same will stored into memory for your reference in tuning mode.
 */


/**
 * NvmmUlpSetKpiMode
 * @brief This method should be called to set the KPI mode. 
 * This should be called after NvmmUlpKpiLoggerInit before calling any other funtion.
 * KPI normal mode gives the values of three KPIs. 
 * KPI_ParseTimeIdleTimeRatio - This KPI indicates the parse time and idle time ratio.
 * KPI_AverageTimeBetweenReadRequests - This KPI indicates the average time between read requests.
 * KPI_TotalReadRequests - This KPI gives the total number of read requests per track.
 *
 * @param mode This signifies the kpi mode (KpiMode_Normal or KpiMode_Tuning Mode)
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpSetKpiMode(KpiMode mode);

/**
 * NvmmUlpGetKpiMode
 * @brief This method should be called to get the KPI mode. 
 *
 * @param mode This signifies the kpi mode (KpiMode_Normal or KpiMode_Tuning Mode)
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpGetKpiMode(KpiMode* pMode);

/**
 * NvmmUlpKpiLoggerInit
 * @brief This is the first method that needs to be called from the parser and 
 * it is the responsibility of the parser to call the NvmmUlpKpiLoggerDeInit funtion 
 * at the end of the track.
 * This method initializes the KPI logger and allocates memory for the logger. 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiLoggerInit(void);

/**
 * NvmmUlpKpiLoggerDeInit
 * @brief This method de-initializes the KPI logger and deallocates memory for the logger. 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiLoggerDeInit(void);

/**
 * NvmmUlpKpiSetLogger
 * @brief This method should select the logger into which the KPIs should be logged. 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetLogger(void);

/**
 * NvmmUlpKpiGetLogger
 * @brief This method gets the KPI logger. 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetLogger(void);

/**
 * NvmmUlpKpiSetSongDuration
 * @brief This method is used to set the actual song duration in 100ns. 
 * @param SongDurationUs song duration in 100ns
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetSongDuration(NvU64 SongDuration);

/**
 * NvmmUlpKpiSetReadStartTime
 * @brief This method should be called before every read call in the parser block.
 * @param StartTime Read Start time
 * @param print NV_TRUE to display read information and NV_FALSE if no prints required.
 * @param size Bytes to Read or read buffer size
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetReadStartTime(NvU64 StartTime, NvBool print, NvU32 size);

/**
 * NvmmUlpKpiSetReadEndTime
 * @brief This method should be called on completion of every read call in the parser block.
 * @param EndTime Read End time
 * @param EndOfStream Pass NV_TRUE if it is end of stream
 * @param print NV_TRUE to display read information and NV_FALSE if no prints required.
 * @param size Bytes Acutally Read
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetReadEndTime(NvU64 EndTime, NvBool EndOfStream, NvBool print, NvU32 size);

/**
 * NvmmUlpKpiSetParseStartTime
 * @brief This method should be called on starting of every work unit  in the parser block.
 * @param StartTime Parse Start time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetParseStartTime(NvU64 StartTime);

/**
 * NvmmUlpKpiSetParseEndTime
 * @brief This method should be called on completion of work unit in the parser block.
 * @param EndTime Parse End time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetParseEndTime(NvU64 EndTime);

/**
 * NvmmUlpKpiSetSeekStartTime
 * @brief This method should be called on before every seek.
 * @param StartTime Seek Start time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetSeekStartTime(NvU64 StartTime);

/**
 * NvmmUlpKpiSetSeekEndTime
 * @brief This method should be called on completion seek call.
 * @param EndTime Seek End time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetSeekEndTime(NvU64 EndTime);

/**
 * NvmmUlpKpiSetFileIOStartTime
 * @brief This method should be called on before every File operation.
 * @param StartTime FileIO Start time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetFileIOStartTime(NvU64 StartTime);

/**
 * NvmmUlpKpiSetFileIOEndTime
 * @brief This method should be called on completion of FileIO call.
 * @param EndTime FileIO End time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetFileIOEndTime(NvU64 EndTime);

/**
 * NvmmUlpKpiSetIdleStartTime
 * @brief This method needs to be called in openParser function after calling NvmmUlpKpiLoggerInit.
 * Also, this method should be called on completion of every work unit on the parser block.
 * @param StartTime Idle Start time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetIdleStartTime(NvU64 StartTime);

/**
 * NvmmUlpKpiSetIdleEndTime
 * @brief This method should be called when next work unit is called on the parser block.
 * @param EndTime Idle End time
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiSetIdleEndTime(NvU64 EndTime);

/**
 * NvmmUlpKpiGetParseTimeIdleTimeRatio
 *
 * @brief This method gets KPI - ParseTime and Idle Time Ratio. The formula used for this
 * KPI is (1- (total parse time/total idle time) )*100.
 * The main goal of this KPI is to reduce the total time spent in Parser.
 * TimeSpentParsing = TotalTimeSpent - ContentDiskReadTime - IdleTime 
 * Total Content Time and Disk Read time are not in our control 
 * (so is CPU frequency hence assuming fixed CPU frequency too), so assuming they are constants
 * KPI1 is calculated with parse and idle time ratio.
 * As parse time increases, idle time decreases and KPI goes away from 100%.
 * As parse time reduces to 0, we achieve 100% KPI or increased efficiency.
 *
 * @param pTime Returns Parse/Idle time ratio
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetParseTimeIdleTimeRatio(NvF64* pTime);

/**
 * NvmmUlpKpiGetAverageTimeBwReadRequests
 * @brief This KPI calculates average time between disk read requests
 * 
 * The goal is to minimize disk read requests and maximize time between requests
 * KPI2 = sum of time in ms between consecutive read requests / total number of read requests
 * 
 * @param pAverageIdleTime Return Average time between Read requests
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetAverageTimeBwReadRequests(NvU64* pAverageIdleTime);

/**
 * NvmmUlpKpiGetTotalReadRequests
 * @brief This method gets total number of read requests per track
 * For a fixed codec and fixed buffer we _must_ avoid reads that will cause overheads.
 * Small reads will be costly since FS has to be involved every time to run FS logic before it can hit the disk.
 * KPI3 = total read requests from parser to the disk
 * This is a decreasing KPI. SOL is 1, but will be >1 when we start and should try to approach 1
 *
 * @param pCount Gets total number of read requests.
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalReadRequests(NvU32* pCount);

/**
 * NvmmUlpKpiGetSongDuration
 * @brief This funtion gets the actual song duration
 * 
 * @param pSongDuration Return total song duration
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetSongDuration(NvU64 *pSongDuration);


/**
 * NvmmUlpKpiGetTotalTimeSpent
 * @brief This funtion gets the total time spent between parser open and parser close
 * 
 * @param pTimeSpentUs Return total time spent between parser open and parser close
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalTimeSpent(NvU64* pTimeSpent);

/**
 * NvmmUlpKpiGetTotalIdleTime
 * @brief This funtion gets the total idle time
 * 
 * @param pTotalIdleTimeUs Return Total Idle time from the total time spent
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalIdleTime(NvU64* pTotalIdleTime);

/**
 * NvmmUlpKpiGetTotalParseTime
 * @brief This funtion gets the total parse time
 * 
 * @param pTotalParseTimeUs Return Total Parse time from the total time spent
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalParseTime(NvU64* pTotalParseTime);

/**
 * NvmmUlpKpiGetTotalReadTime
 * @brief This funtion gets the total read time.
 * 
 * @param pTotalReadTimeUs Return Total Read time from the total time spent
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalReadTime(NvU64* pTotalReadTime);

/**
 * NvmmUlpKpiGetTotalTimeBwReads
 * @brief This funtion gets the total time spent between read requests from the total time spent
 * 
 * @param pTotalTimeBwReadsUs Return total time spent between read requests
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiGetTotalTimeBwReads(NvU64* pTotalTimeBwReads);

/**
 * NvmmUlpKpiPrintAllKpis
 * @brief This funtion prints all the kpis on console
 * 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpKpiPrintAllKpis(void);

/**
 * NvmmUlpUpdateKpis
 * @brief This funtion updates the KPIs according to flags set. This can be used
 * to set more than one kpi at a given time.
 * 
 * @retval NvSuccess or NvError_NotInitialized
 */
NvError NvmmUlpUpdateKpis(KpiFlag flags, NvBool EndOfStream, NvBool print, NvU32 size);

#if defined(__cplusplus)
}
#endif

#endif

