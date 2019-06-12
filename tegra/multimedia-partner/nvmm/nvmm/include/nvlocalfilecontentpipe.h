/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* This file contains declarations for getting handles of Content Pipes for different kind of contents.*/

#ifndef NV_LOCAL_FILE_CONTENT_PIPE_H
#define NV_LOCAL_FILE_CONTENT_PIPE_H

#ifndef OMXVERSION
#define OMXVERSION 0
#endif

#include "openmax/il/OMX_Types.h"

#include "openmax/il/OMX_ContentPipe.h"
#include "nverror.h"
#include "nvrm_memmgr.h"
    
/* local copy of content pipe header, for 1.0 compatability */

/** Map types from OMX standard types only here so interface is as generic as possible. */
#ifndef CP_LLTYPES_DEF
#define CP_LLTYPES_DEF
typedef OMX_U64 CPuint64;
typedef OMX_S64 CPint64;
#endif
typedef OMX_U32 CPuint32; 

typedef CPresult (*ClientCallbackFunction)(void *pClientContext, CP_EVENTTYPE eEvent, CPuint iParam);

#define CP_OriginTime 0X70000000

typedef enum CP_ConfigIndexEnum {
    CP_ConfigIndexCacheSize,
    CP_ConfigIndexThreshold,
    CP_ConfigQueryMetaInterval,
    CP_ConfigQueryActualSeekTime,
    CP_ConfigQueryRTCPAppData,
    CP_ConfigQueryRTCPSdesCname,
    CP_ConfigQueryRTCPSdesName,
    CP_ConfigQueryRTCPSdesEmail,
    CP_ConfigQueryRTCPSdesPhone,
    CP_ConfigQueryRTCPSdesLoc,
    CP_ConfigQueryRTCPSdesTool,
    CP_ConfigQueryRTCPSdesNote,
    CP_ConfigQueryRTCPSdesPriv,
    CP_ConfigQueryTimeStamps,
}CP_ConfigIndex;

typedef struct CP_ConfigThreshold {
    NvU64 HighMark;
    NvU64 LowMark;
}CP_ConfigThreshold;


typedef struct CP_PositionInfoTypeRec
{
    CPuint64    nDataBegin; //Read: The beginning of the available content, specifically the position of the first (still) available byte of the content's data stream. Typically zero for file reading.
                            //Write: The beginning of content, specifically the position of the first writable byte of the content's data stream. Typically zero

    CPuint64    nDataFirst; //Read: The beginning of the available content, specifically the position of the first (still) available byte of the content's data stream. Typically zero for file reading.
                            //Write: Not applicable
                            
    CPuint64    nDataCur;   //Read/Write: Current position
                             
    CPuint64    nDataLast;  //Read: The end of the available content, specifically the position of the last available byte of the content's data stream. Typically equaling nDataEnd for file reading.
                            //Write: Not applicable

    CPuint64    nDataEnd;   //Read: The end of content, specifically the position of the last byte of the content's data stream. This may return CPE_POSITION_NA if the position is unknown. Typically equaling the file size for file reading.
                            //Write: Not applicable

}CP_PositionInfoType;


/** content pipe definition 
 * 
 */
typedef struct CP_PIPETYPE_EXTENDED
{
    CP_PIPETYPE cpipe;

       /** Seek to certain position in the content relative to the specified origin. */
    CPresult (*SetPosition64)( CPhandle hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin);

    /** Retrieve the current position relative to the start of the content. */
    CPresult (*GetPosition64)( CPhandle hContent, CPuint64 *pPosition);

    /** Retrieve the base address of the content */
    CPresult (*GetBaseAddress)( CPhandle hContent, NvRmPhysAddr *pPhyAddress, void **pVirtAddress);

    /** Register the client and its callback with the content pipe. */
    CPresult (*RegisterClientCallback)( CPhandle hContent, void *pClientContext, ClientCallbackFunction pCallback);

    /** Retrieve the number of available bytes */
    CPresult (*GetAvailableBytes)( CPhandle hContent, CPuint64 *nBytesAvailable);

    /** Gets the file size */
    CPresult (*GetSize)( CPhandle hContent, CPuint64 *pFileSize);

   /** Enable internal buffer manager for managing ReadBuffer */
    CPresult (*CreateBufferManager)(CPhandle* hContent, CPuint nSize);

    /** Initialize the Content Pipe 
     * 
     * MinCacheSize : Indicates the minimum size of the cache.
     * 
     * MaxCacheSize : Indicates the maximum size of the cache
     * 
     * SpareAreaSize : Indicates the size of the SpareArea
     * The Cached CP uses a circular buffer. If the client reads the data and if 
     * the requested block straddles the boundary of the circular buffer, then we 
     * use spare area. The size of the spare area is decided based on the largest
     * read that the parser can do.
     * 
     */
    CPresult (*InitializeCP)(CPhandle hContent, CPuint64 MinCacheSize, CPuint64 MaxCacheSize, CPuint32 SpareAreaSize, CPuint64* pActualSize);

    CPbool (*IsStreaming)(CPhandle hContent);

    CPresult (*InvalidateCache)(CPhandle hContent);

    /** Pause/unpause caching thread */
    CPresult (*PauseCaching)(CPhandle hContent, CPbool bPause);

    CPresult (*GetPositionEx)(CPhandle hContent, CP_PositionInfoType* pPosition);

    CPresult (*GetConfig)(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure);

    CPresult (*SetConfig)(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure);

    CPresult (*StopCaching)(CPhandle hContent);

    CPresult (*StartCaching)(CPhandle hContent);

}CP_PIPETYPE_EXTENDED;

void NvMMGetContentPipe(CP_PIPETYPE_EXTENDED **pipe);

#endif
