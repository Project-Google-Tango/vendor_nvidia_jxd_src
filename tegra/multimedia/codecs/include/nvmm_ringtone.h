/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_RINGTONE_H
#define INCLUDED_NVMM_RINGTONE_H


#if defined(__cplusplus)
extern "C"
{
#endif

/*Attributes for the Set Attribute call */
/* 0 - Attrib for sending the input File Size */
typedef enum
{
    NvMMRingtoneAttribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 700),
    NvMMRingtoneAttribute_InFileSize,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMRingtoneAttribute_Force32 = 0x7FFFFFFF

}NvMMAudioRingtoneAttribute;

/* BUFFERFLAG values for Input Buffer data */
/* Enum to specify whether File EndOfStream had hit or
   the data passed in the buffer get corrupted */
typedef enum
{
    NVMM_RINGTONE_BUFFERFLAG_DATACORRUPT = 0x0,
    NVMM_RINGTONE_BUFFERFLAG_EOS,

}NVMM_RINGTONE_BUFFERFLAG;

/*
  Structure added to pass the Ringtone Attributes
*/
typedef struct 
{
    NvU32 structSize;
    NvU64 inFileSize;

} NvMMRingtoneAttribute;

#if defined(__cplusplus)
extern "C"
}
#endif

#endif //INCLUDED_NVMM_RINGTONE_H

