/*
 * Copyright (c) 2012-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVVIDEOENC_DFS
#define NVVIDEOENC_DFS

#define ENABLE_VIDEOENC_DFS  1
#define PRINT_BUFFS 0
#define VERY_LARGE_FREQUENCY  0x7FFFFFFF
#define THRESHOLD_UP_SEVERE  5 // current
#define THRESHOLD_UP 4 // current
#define FREQ_UP_COUNT 25 // count
#define FREQ_DOWN_COUNT 25 // count
#define ENCODE_DFS_BUFF_SIZE 16 // window size
#define THRESHOLD_UP_LONG_TERM 3 // avg
#define LOW_CORNER_720x576  150000
#define LOW_CORNER_720p     150000
#define LOW_CORNER_1080p    200000
#define THRESHOLD_DOWN  1 // avg

typedef struct VideoEncDfsStruct_ {
        NvU32   PendingInputBuffers;
        NvU32   MaxFreq;
        NvU32   LowCorner;
        NvU32   PendingInputBuffers_Array[ENCODE_DFS_BUFF_SIZE];
        NvU32   idx;
        NvU32   CountUp;
        NvU32   CountDown;
        NvU32   PrevFreq;
        NvBool  bFreqIncreased;
        NvBool  bFreqDecreased;
        NvBool  bMSENC;
        NvU32   MaxInputBuffers;
        NvU32   TargetInputBuffers;
        NvU32   NumBframes;
        NvMMVideoEncQualityLevel QualityLevel;
} VideoEncDfsStruct;

void
VideoEncDFS_Init(
    NvRmChannelHandle ch,
    VideoEncDfsStruct *pVideoEncDfs,
    NvU32 FrameWidthInMB,
    NvU32 FrameHeightInMB,
    NvU32 FrameRate);

void VideoEncDFS(
    NvRmChannelHandle ch,
    VideoEncDfsStruct *pVideoEncDfs);

#endif // NVVIDEOENC_DFS
