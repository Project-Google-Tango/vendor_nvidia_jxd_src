
/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NVX_Trace.h */

#ifndef NVXTRACE_H_
#define NVXTRACE_H_

#include <stdio.h>
#include <stdarg.h>
/* for NV_INLINE and OMX_U32 types */
#include "nvcommon.h"
#include "nvos.h"

#if NV_ENABLE_DEBUG_PRINTS
#define NVXT_DO_TRACE 1
#else
#define NVXT_DO_TRACE 0
#endif

#define NV_DEBUG_OMX (0)

#if (NV_DEBUG_OMX)
#define NvDebugOmx(x) NvOsDebugPrintf x
#else
#define NvDebugOmx(x)
#endif

/* TYPE OF TRACING */

/* ALL is only for setting object type trace masks, not for use when using a
   NvxTrace call.
   NvxTrace may be called with two (or more) flags or'd together to only display
   when all types are turned on (like NVXT_ERROR|NVXT_BUFFERING)
 */

typedef enum ENvxtTraceType
{
  NVXT_NONE      = 0x00000000,
  NVXT_ERROR     = 0x00000001,
  NVXT_WARNING   = 0x00000002,
  NVXT_INFO      = 0x00000004,
  NVXT_BUFFER    = 0x00000008,
  NVXT_WORKER    = 0x00000010,
  NVXT_STATE     = 0x00000020,
  NVXT_CALLGRAPH = 0x00000040,
  NVXT_ALLTYPES  = NVXT_ERROR | NVXT_WARNING | NVXT_INFO |
                   NVXT_BUFFER |  NVXT_WORKER | NVXT_STATE | NVXT_CALLGRAPH,
  NVXT_MAX_TRACETYPES
} ENvxtTraceType;

// object types that may have different trace settings


typedef enum ENvxtObjectType{
    NVXT_CORE = 0,
    NVXT_SCHEDULER,
    NVXT_FILE_READER,
    NVXT_VIDEO_DECODER,
    NVXT_IMAGE_DECODER,
    NVXT_AUDIO_DECODER,
    NVXT_VIDEO_RENDERER,
    NVXT_AUDIO_RENDERER,
    NVXT_VIDEO_SCHEDULER,
    NVXT_AUDIO_MIXER,
    NVXT_VIDEO_POSTPROCESSOR,
    NVXT_IMAGE_POSTPROCESSOR,
    NVXT_CAMERA,
    NVXT_AUDIO_RECORDER,
    NVXT_VIDEO_ENCODER,
    NVXT_IMAGE_ENCODER,
    NVXT_AUDIO_ENCODER,
    NVXT_FILE_WRITER,
    NVXT_CLOCK,
    NVXT_GENERIC,
    NVXT_UNDESIGNATED,
    NVXT_ALLOBJECTS,
    NVXT_DRM_PLAY,
    NVXT_AUDIO_CAPTURE,
#ifdef USE_TVMR_OMX
    NVXT_VIDEO_MIXER,
    NVXT_VIDEO_CAPTURE,
    NVXT_VIDEO_TVMRRENDERER,
#endif
    NVXT_MAX_OBJECTS
}ENvxtObjectType;

typedef struct NVXTRACEOBJECT
{
    const char *objectname;       /* Component name, 128 byte limit (including \0) applies */
    OMX_BOOL    TraceEnable;
    OMX_U32     NvxtTraceMask;
}NVXTRACEOBJECT;

/* Each object has a trace mask defining what types of output will be generated. */
extern struct NVXTRACEOBJECT NVX_TraceObjects[];
extern FILE* pTraceDest;

#define DEFAULT_LOG_FILENAME "defaultout.log"

/* need to define a null NvxTrace if non-verbose*/

/* Trace destination flags */
#define NVXT_STDOUT     0x0001
#define NVXT_LOGFILE    0x0002
#define NVXT_MEMORY     0x0003
#define NVXT_CUSTOM     0x0004 /* e.g. platform specific logging utility */

extern OMX_U32 NvxtDestinationFlags;
extern OMX_U8 NvxtLogFileName[256];


/* trace implementation function poOMX_U32er types */
typedef void (*NvxtTraceInitFunction)(void);
typedef void (*NvxtPrintfFunction)(OMX_U32 , OMX_U32 , const char *, ...);
typedef void (*NvxtFlushFunction)(void);
typedef void (*NvxtTraceDeInitFunction)(void);

/* Actual OMX_U32erface used by trace clients */
extern NvxtTraceInitFunction    NvxtTraceInit;              /* setup trace implementation */
extern NvxtPrintfFunction       NvxtTrace;   /* emit a trace message */
extern NvxtFlushFunction        NvxtFlush;                  /* flush any messages in memory */
extern NvxtTraceDeInitFunction  NvxtTraceDeInit;       /* cleanup trace implemenation */


//This function is not Inline otherwise it fails to take in variable number of arguments .
#if NVXT_DO_TRACE

#define NVXTRACE(x) NvxtTrace x

static NV_INLINE void NvxtPrintfDefault(OMX_U32 type, OMX_U32 objType, const char *format, ...)
{
    va_list args;

    if (((NVX_TraceObjects[objType].NvxtTraceMask & (type)) == (type)) &&
        (NVX_TraceObjects[objType].TraceEnable==OMX_TRUE)) {

             va_start(args, format);
             vfprintf(pTraceDest, (char*)format, args);
             fflush(pTraceDest);
             va_end(args);
    }
}
#else

#define NVXTRACE(x) do {} while (0)

static NV_INLINE void NvxtPrintfDefault(OMX_U32 objType, OMX_U32 type, const char *format,...)
{
}

#endif

/* Means of programmatically overriding the trace implementation at runtime */
void NvxtOverrideTraceImplementation(NvxtTraceInitFunction initFunction,
                                     NvxtPrintfFunction printfFunction,
                                     NvxtFlushFunction flushFunction,
                                     NvxtTraceDeInitFunction deInitFunction);

//void NvxtPrintfDefault(OMX_U32 type, OMX_U32 objType, OMX_U8 *format, ...);
void NvxtTraceInitDefault(void);
void NvxtFlushDefault(void);
void NvxtTraceDeInitDefault(void);
void NvxtConfigReader(OMX_STRING sFilename);
void NvxtSetTraceFlag(char*  type, OMX_U32 *flag);
void NvxtConfigWriter(OMX_STRING sFilename);
void NvxtSetDestination(OMX_U32 destinationFlags, const char *logFile);
void NvxtGetDestination(OMX_U32 *pDestinationFlags, OMX_U8 *logFile);

#endif

