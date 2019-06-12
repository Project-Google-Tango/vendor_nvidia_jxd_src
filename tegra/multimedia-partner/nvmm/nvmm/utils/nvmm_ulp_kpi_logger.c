/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_ulp_kpi_logger.h"
 //All the units of time are in 100 ns  
typedef struct {
    // Signifies the KPI mode
    KpiMode mode;
    // Variable to store parse start time
    NvU64 ParseStartTime;
    // Variable to store parse end time
    NvU64 ParseEndTime;
    // Variable to store seek start time
    NvU64 SeekStartTime;
    // Variable to store seek end time
    NvU64 SeekEndTime;
    // Variable to store FileIO start time
    NvU64 FileIOStartTime;
    // Variable to store FileIO end time
    NvU64 FileIOEndTime;
    // Variable to store read start time
    NvU64 ReadStartTime;
    // Variable to store read end time
    NvU64 ReadEndTime;
    // Variable to store idle start time
    NvU64 IdleStartTime;
    // Variable to store idle end time
    NvU64 IdleEndTime;
    // Total song duration
    NvF64 TotalSongDuration;
    // Stores total time spent between parser open and parse close
    NvF64 TotalTimeSpent;
    // Indicates Total read time for a track
    NvF64 TotalReadTime;
    // Indicates Total seek time for a track
    NvU64 TotalSeekTime;
    // Indicates Total time spent on file operations for a track
    NvF64 TotalFileIOTime;
    // Indicates total parse time for a track
    NvF64 TotalParseTime;
    // Indicates total idle time for a track
    NvF64 TotalIdleTime;
    // Signifies total time spent between read requests
    NvF64 TotalTimeBwReadRequests;
    // Stores KPI - Parse time and Idle time ratio
    NvF64 KPI_ParseIdleRatio;
    // Indicates KPI - Average time between read requests
    NvF64 KPI_AvgTimeBwReadRequests;
    // Gives the total number of read requests
    NvU32 KPI_TotalReadRequests;

    //These logs may be changed to dynamic allocated memories in next versions.
    // Used to store parse times during tuning mode.
    NvU64 ParseTimeLog[1024];
    // Used to store idle times during tuning mode.
    NvU64 IdleTimeLog[1024];
    // Used to stores read times during tuning mode.
    NvU64 ReadTimeLog[1024];

    // Index for read time log
    NvU32 readindex;
    // Index for parse time log
    NvU32 parseindex;
    // Index for idle time log
    NvU32 idleindex;
    // Used to store the relative time stamp
    NvU64 RelativeTimeStamp;


} UlpKpiLog;


UlpKpiLog* KpiLog;

#if !NV_IS_AVP


NvError NvmmUlpSetKpiMode(KpiMode mode)
{

    if(!KpiLog) return NvError_NotInitialized;

    KpiLog->mode = mode;

    if(KpiLog->mode == KpiMode_Tuning) {
        //To do - Dynamic allocation of memory for logs
    //    KpiLog->ParseTimeLog = (NvU64*)NvOsAlloc(sizeof(NvU64) * 1024);
     //   KpiLog->IdleTimeLog =  (NvU64*)NvOsAlloc(sizeof(NvU64) * 1024);;
    //    KpiLog->ReadTimeLog =  (NvU64*)NvOsAlloc(sizeof(NvU64) * 1024);;

    }
    return NvSuccess;
}

NvError NvmmUlpGetKpiMode(KpiMode* pMode)
{
    if(!KpiLog) return NvError_NotInitialized;

    *pMode = KpiLog->mode;
     return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiLoggerInit(void)
{
    if(!KpiLog) {
        KpiLog = (UlpKpiLog*)NvOsAlloc(sizeof(UlpKpiLog));
        NvOsMemset(KpiLog, 0, sizeof(UlpKpiLog));
   //     KpiLog->ParseTimeLog = NULL;
   //     KpiLog->IdleTimeLog = NULL;
   //     KpiLog->ReadTimeLog = NULL;
   }
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiLoggerDeInit(void)
{

    if(!KpiLog) return NvError_NotInitialized;
    if(KpiLog->mode == KpiMode_Tuning) {
     //   NvOsFree(KpiLog->ParseTimeLog);
     //   NvOsFree(KpiLog->IdleTimeLog);
    //    NvOsFree(KpiLog->ReadTimeLog);
     //   KpiLog->ParseTimeLog = NULL;
     //   KpiLog->IdleTimeLog = NULL;
     //   KpiLog->ReadTimeLog = NULL;
    }

   NvOsFree(KpiLog);
   KpiLog = NULL;
   return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetLogger(void)
{
    if(!KpiLog) return NvError_NotInitialized;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetLogger(void)
{
    if(!KpiLog) return NvError_NotInitialized;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetSongDuration(NvU64 SongDuration)
{
    if(!KpiLog) return NvError_NotInitialized;
    KpiLog->TotalSongDuration = (NvF64)SongDuration;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetReadStartTime(NvU64 StartTime, NvBool print, NvU32 size)
{
    if(!KpiLog) return NvError_NotInitialized;

    KpiLog->ReadStartTime = StartTime;

    //Initialize total time between read requests at the first read request.
    //This value will be reused at last read request.
    if(KpiLog->KPI_TotalReadRequests == 0)     
    {
        KpiLog->RelativeTimeStamp = StartTime;
    }


    KpiLog->KPI_TotalReadRequests++;

    if(print)
    {
            NvOsDebugPrintf("Read Start: %d, size = %d, start time = %ld ms ", 
            KpiLog->KPI_TotalReadRequests,
            size,
            (StartTime - KpiLog->RelativeTimeStamp)/MS_100NS);
    }

    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetReadEndTime(NvU64 EndTime, NvBool EndOfStream, NvBool print, NvU32 size)
{
    NvU64 tempTime = 0, readTime = 0;
    if(!KpiLog) return NvError_NotInitialized;

    if(size != 0) {     
        KpiLog->ReadEndTime = EndTime;
        readTime = KpiLog->ReadEndTime - KpiLog->ReadStartTime;
         KpiLog->TotalReadTime += readTime;
    }
    //If End of stream calculate the total time spent between read requests.
    //This is required to calculate KPI_AvgTimeBwReadRequests
    if(EndOfStream) {
        tempTime = (NvU64)(KpiLog->ReadEndTime - KpiLog->RelativeTimeStamp);
        KpiLog->TotalTimeBwReadRequests = tempTime - KpiLog->TotalReadTime;
    }

    if(size != 0) 
    {
        if(print)
        {
            NvOsDebugPrintf("Read End: %d, size = %d, end time = %ld ms", 
                KpiLog->KPI_TotalReadRequests,
                size,
                (EndTime - KpiLog->RelativeTimeStamp)/MS_100NS);
            NvOsDebugPrintf("Read Time = %ld ms ", readTime/MS_100NS);
        }

        if(KpiLog->mode == KpiMode_Tuning) {
            KpiLog->ReadTimeLog[KpiLog->readindex++] = KpiLog->ReadEndTime - KpiLog->ReadStartTime;
        }
    }
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetParseStartTime(NvU64 StartTime)
{
    if(!KpiLog) return NvError_NotInitialized;
   KpiLog->ParseStartTime = StartTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetParseEndTime(NvU64 EndTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    KpiLog->ParseEndTime = EndTime;
    KpiLog->TotalParseTime += KpiLog->ParseEndTime - KpiLog->ParseStartTime;
   if(KpiLog->mode == KpiMode_Tuning) {
        KpiLog->ParseTimeLog[KpiLog->parseindex++] = KpiLog->ParseEndTime - KpiLog->ParseStartTime;
   }
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetSeekStartTime(NvU64 StartTime)
{
    if(!KpiLog) return NvError_NotInitialized;
   KpiLog->SeekStartTime = StartTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetSeekEndTime(NvU64 EndTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    KpiLog->SeekEndTime = EndTime;
    KpiLog->TotalSeekTime += KpiLog->SeekEndTime - KpiLog->SeekStartTime;
    return NvSuccess;
}


/**
 *
 */
NvError NvmmUlpKpiSetFileIOStartTime(NvU64 StartTime)
{
    if(!KpiLog) return NvError_NotInitialized;
   KpiLog->FileIOStartTime = StartTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetFileIOEndTime(NvU64 EndTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    KpiLog->FileIOEndTime = EndTime;
    KpiLog->TotalFileIOTime += KpiLog->FileIOEndTime - KpiLog->FileIOStartTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetIdleStartTime(NvU64 StartTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    if(!KpiLog->TotalTimeSpent)
    {    
        KpiLog->TotalTimeSpent = (NvF64)StartTime;
    }
    KpiLog->IdleStartTime = StartTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiSetIdleEndTime(NvU64 EndTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    KpiLog->IdleEndTime = EndTime;
    KpiLog->TotalIdleTime += (NvF64)(KpiLog->IdleEndTime - KpiLog->IdleStartTime);
   if(KpiLog->mode == KpiMode_Tuning) {
        KpiLog->IdleTimeLog[KpiLog->idleindex++] = KpiLog->IdleEndTime - KpiLog->IdleStartTime;
   }
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetParseTimeIdleTimeRatio(NvF64* pTime)
{

     NvF64 time;
    if(!KpiLog) return NvError_NotInitialized;

   if(!KpiLog->TotalIdleTime) {
        return NvError_BadParameter;
   }

   //Calculate total time spent at the end of song.
   KpiLog->TotalTimeSpent = KpiLog->IdleEndTime - KpiLog->TotalTimeSpent;

    time =  (NvF64)KpiLog->TotalParseTime * 100;
    KpiLog->KPI_ParseIdleRatio =  100 - (time / KpiLog->TotalIdleTime);
    *pTime = KpiLog->KPI_ParseIdleRatio;

    return NvSuccess;
}

/**
 * Calculating Average time between read requests.
 * Suppose there are 5 read requests, there are 4 gaps between those read requests
 * So, average time between read requests is:
 * TotalTimeBetweenReadRequests / (Total Number of read requests - 1)
 *    ______    _____      ____     _____     _____
 *  _|   1  |__|   2  |__|   3  |__|  4   |__|  5   |_____________
 *
 *            ^              ^           ^            ^
 *            1              2            3             4
 */
NvError NvmmUlpKpiGetAverageTimeBwReadRequests(NvU64* pAverageIdleTime)
{

    if(!KpiLog) return NvError_NotInitialized;
   if(!KpiLog->KPI_TotalReadRequests) {
        return NvError_BadParameter;
   }

    KpiLog->KPI_AvgTimeBwReadRequests = KpiLog->TotalTimeBwReadRequests / (KpiLog->KPI_TotalReadRequests - 1);
    *pAverageIdleTime = (NvU64)KpiLog->KPI_AvgTimeBwReadRequests;
   return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetTotalReadRequests(NvU32* pCount)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pCount = KpiLog->KPI_TotalReadRequests;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetSongDuration(NvU64 *pSongDuration)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pSongDuration = (NvU64)KpiLog->TotalSongDuration;
    return NvSuccess;
}


/**
 *
 */
NvError NvmmUlpKpiGetTotalTimeSpent(NvU64* pTimeSpent)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pTimeSpent = (NvU64)KpiLog->TotalTimeSpent;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetTotalIdleTime(NvU64* pTotalIdleTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pTotalIdleTime = (NvU64)KpiLog->TotalIdleTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetTotalParseTime(NvU64* pTotalParseTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pTotalParseTime = (NvU64)KpiLog->TotalParseTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetTotalReadTime(NvU64* pTotalReadTime)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pTotalReadTime = (NvU64)KpiLog->TotalReadTime;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiGetTotalTimeBwReads(NvU64* pTotalTimeBwReads)
{
    if(!KpiLog) return NvError_NotInitialized;
    *pTotalTimeBwReads = (NvU64)KpiLog->TotalTimeBwReadRequests;
    return NvSuccess;
}

/**
 *
 */
NvError NvmmUlpKpiPrintAllKpis(void)
{
    if(!KpiLog) return NvError_NotInitialized;

   // NvU8* PrintBuffer = NvOsAlloc(1024);

    // Calculate the KPIs and print them on console.
    NvOsDebugPrintf("\n ULP KPIs: KPI_ParseTimeIdleTimeRatio = %0.4lf ", KpiLog->KPI_ParseIdleRatio);
    NvOsDebugPrintf("\n ULP KPIs: Total Time Between Read Requests = %0.2lf ms", KpiLog->TotalTimeBwReadRequests/MS_100NS);
    NvOsDebugPrintf("\n ULP KPIs: KPI_AverageTimeBwReadRequests = %0.2lf ms", KpiLog->KPI_AvgTimeBwReadRequests/MS_100NS);
    NvOsDebugPrintf("\n ULP KPIs: KPI_TotalReadRequests = %d", KpiLog->KPI_TotalReadRequests);
    NvOsDebugPrintf("\n ULP KPIs: Total Song Duration = %0.2lf sec", KpiLog->TotalSongDuration/SEC_100NS);
    NvOsDebugPrintf("\n ULP KPIs: Total Time Spent = %0.2lf sec", KpiLog->TotalTimeSpent/SEC_100NS);
    NvOsDebugPrintf("\n ULP KPIs: Total Idle Time = %0.2lf sec", KpiLog->TotalIdleTime/SEC_100NS);
    NvOsDebugPrintf("\n ULP KPIs: Total Parse Time = %0.2lf ms", KpiLog->TotalParseTime/MS_100NS);
    NvOsDebugPrintf("\n ULP KPIs: Total Read Time = %0.2lf ms", KpiLog->TotalReadTime/MS_100NS);
    NvOsDebugPrintf("\n ULP KPIs: Total File IO Time = %0.2lf ms", KpiLog->TotalFileIOTime/MS_100NS);


//   NvOsFree(PrintBuffer);

    return NvSuccess;
}

NvError NvmmUlpUpdateKpis(KpiFlag flags, NvBool EndOfStream, NvBool print, NvU32 size)
{
    NvU64 time =  0;
    time = NvOsGetTimeUS() * US_100NS;

    if(!KpiLog) return NvError_NotInitialized;

    if(KpiLog->mode) 
    {
        if(flags & KpiFlag_IdleStart) NvmmUlpKpiSetIdleStartTime(time);
        if(flags & KpiFlag_IdleEnd) NvmmUlpKpiSetIdleEndTime(time);
        if(flags & KpiFlag_ReadStart) NvmmUlpKpiSetReadStartTime(time, print, size);
        if(flags & KpiFlag_ReadEnd) NvmmUlpKpiSetReadEndTime(time, EndOfStream, print, size);
        if(flags & KpiFlag_ParseStart) NvmmUlpKpiSetParseStartTime(time);
        if(flags & KpiFlag_ParseEnd) NvmmUlpKpiSetParseEndTime(time);
        //if(flags & KpiFlag_SeekStart) NvmmUlpKpiSetSeekStartTime(time);
        //if(flags & KpiFlag_SeekEnd) NvmmUlpKpiSetSeekEndTime(time);
        if(flags & KpiFlag_FileIOStart) NvmmUlpKpiSetFileIOStartTime(time);
        if(flags & KpiFlag_FileIOEnd) NvmmUlpKpiSetFileIOEndTime(time);
    }
    return NvSuccess;
}


#endif //#if !NV_IS_AVP




