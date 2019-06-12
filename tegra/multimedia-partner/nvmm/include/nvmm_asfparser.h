/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** @file
* @brief <b>NVIDIA Driver Development Kit:
*           NvMMBlock ASF Parser specific data-structures</b>
*
* @b Description: Declares Interface for ASF Parser.
*/

#ifndef INCLUDED_ASF_PARSER_H
#define INCLUDED_ASF_PARSER_H

#include "nvcommon.h"
#include "nvmm.h"

typedef enum
{
/*
There will be no input Stream to the ASF Parser as it will be reading directly from an ASF file.
There could be a Video, Audio and probably a MetaData Output Stream
    */
    NvMMBlockAsfParserStream_OutVideo = 0,
    NvMMBlockAsfParserStream_OutAudio,
    /*Total No of Streams (Both Input and Output)*/
    NvMMBlockAsfParserStream_StreamCount,
    NvMMBlockAsfStream_Force32 = 0x7FFFFFFF
} NvMMBlockAsfStream;

typedef enum
{
    /*
    What are the events we are interested in?
    Do we need to send events both to client and
    the connecting decoder block?
    */
    NvMMBlockAsfParserEvent_Force32 = 0x7FFFFFFF
} NvMMBlockAsfParserEvent;

/**
@ASF Parser Status
**/

typedef enum
{
    //Have to decide what are the status informations needed.
    NvMMBlockAsfParserStatus_Force32 = 0x7FFFFFFF
} NvMMBlockAsfParserStatus;

/**
@ASF Parser Attributes
**/
typedef enum
{
    /*Attributes to be set by the client*/
    NvMMBlockAsfParserAttribute_ASFInputFile = 1,
    NvMMBlockAsfParserAttribute_PlayRateAndDirection,
    NvMMBlockAsfParserAttribute_SeekToTime,

    /*Attribute to be only read by the client*/
    NvMMBlockAsfParserAttribute_NoOfStreams,
    NvMMBlockAsfParserAttribute_StreamInfo,
    NvMMBlockAsfParserAttribute_CurrentPositionInFile,
    //How about the MetaData Size? How do we give this back?
    NvMMBlockAsfParserAttribute_MetaData,
    NvMMBlockAsfParserAttribute_Force32 = 0x7FFFFFFF
} NvMMBlockAsfParserAttribute;




#endif

