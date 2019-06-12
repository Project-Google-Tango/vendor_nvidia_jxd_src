/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#ifndef INCLUDED_NVODM_TMON_ADT7461_CHANNEL_H
#define INCLUDED_NVODM_TMON_ADT7461_CHANNEL_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    // Local sensor
    ADT7461ChannelID_Local = 1,

    // Remote sensor
    ADT7461ChannelID_Remote,

    ADT7461ChannelID_Num,
    ADT7461ChannelID_Force32 = 0x7FFFFFFFUL
} ADT7461ChannelID;

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVODM_TMON_ADT7461_CHANNEL_H

