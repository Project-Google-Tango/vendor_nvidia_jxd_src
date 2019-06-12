/*
 * Copyright (c) 2007 NVIDIA Corporation. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief        Contains function declaration of the MTS writer library
 */

#ifndef INCLUDED_CORE_MTSWRITER_H
#define INCLUDED_CORE_MTSWRITER_H

//#define FILE_DUMP_IN_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    // typedefs
    typedef NvU16          flag;

    //#define H263


    // Constants
#define SUCCESS 0
#define FAILURE 1
#define FILE_SIZE_LIMIT_EXCEEDED 2

#define TIMESTAMP_RESOLUTION  1000000

#define MAX_TRACK 3 //maximum no of tracks to be present in the MTS file
#define TEMP_INPUT_BUFF_SIZE    250 //input buffer max size to be used before
    //file dumping
#define SOUND_TRACK 0
#define VIDEO_TRACK 1

    //Give thresholds in seconds.
#define MOOV_THRESHOLD 20
#define MOOF_THRESHOLD 10

#define MAX_VIDEO_FRAMES_MOOV (MOOV_THRESHOLD * 30)  //5 min video@ 30fps
#define MAX_VIDEO_FRAMES_MOOF ((MOOF_THRESHOLD * 30)) //5 min video@ 30fps
#define TEMP_SPEECH_SAMPLE_SIZE_ARRAY 50

#define MAX_VIDEO_FRAMES    21600   //108000 //9000 //(3600*30fps) for 5 mins video at 30fps;
#define MAX_SPEECH_DATA     720000  //144000 //60000 //(4*50*60*60) for 5 mins data in bytes(50 frames of audio per second)

#define TIMESTAMP_RESOLUTION  1000000
#define TIME_SCALE 10000 //time scale is 10000 i.e 10000 parts in 1 sec
#define LANGUAGE_CODE 0 //undetermined
#define CREATION_TIME 0 //time in year 2003
#define MODIFICATION_TIME 0 //time in year 2003
#define NEXT_TRACK_ID 3


#define RUN_PROCESS_FIRST_FRAME     0
#define RUN_PROCESS_SPEECH          1
#define RUN_PROCESS_VIDEO           2
#define WRITE_MOOV_ATOM             3
#define INITIALISE_NEW_MOOF_ATOM    4
#define RUN_PROCESS_FRAG_SPEECH     5
#define RUN_PROCESS_FRAG_VIDEO      6
#define WRITE_MOOF_ATOM             7

#define CLOSE_COMPLETE              1000
#define CLOSE_INCOMPLETE            2000

#define EXTRACT_FIVE_BITS   0x1f

    //Close Mux Statuses (Re-entrant)
#define START_MOOV_DATA_WRITE       0
#define WRITE_SPEECH_DATA           1
#define WRITE_VIDEO_DATA            2
#define WRITE_MDAT_DATA             3

#define WRITE_TRAK_TILL_MINF        4
#define WRITE_MINF_TILL_STCO        5
#define WRITE_STCO_TILL_END         6
#define WRITE_MINF_TILL_ESDS        7
#define WRITE_ESDS_TILL_STSC        8
#define WRITE_STSC_TILL_STCO        9
#define WRITE_STCO_TILL_STSS        10
#define WRITE_STSS_TILL_CTTS        11
#define WRITE_CTTS_TILL_END         12
#define WRITE_MINF_TILL_D263        13
#define WRITE_D263_TILL_STSC        14
#define SPEECH_DATA_COMPLETE        15
#define VIDEO_DATA_COMPLETE         16
#define NO_STATUS                   17
#define CLOSE_MUX_NOT_COMPLETE      18
#define CLOSE_MUX_COMPLETE          19
#define START_DATE_WRITE            20
#define WRITE_USER_DATA             21

#define STTS_INIT_DONE                  1

#define AUDIO_PRESENT               1
#define VIDEO_PRESENT               2
#define AUDIO_VIDEO_BOTH            3

// Structures


// Function Prototypes
NvS16 NvMM_MTSWriteInit(structInpParam *pstructInpParam, NvU8 *pBufferOut, NvS32 *Offset, void *StructMem);
NvS16 NvMM_MTSWriteRun(structSVParam *pstructSVParam, structInpParam *pstructInpParam, NvU8 *pBufferOut, NvS32 *Offset, tmem ptrstructMem);
NvS16 NvMM_MTSWriteClose(structInpParam *pstructInpParam, NvU8 *pBufferOut, NvS32 *Offset, tmem ptrstructMem);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_CORE_MTSWRITER_H

