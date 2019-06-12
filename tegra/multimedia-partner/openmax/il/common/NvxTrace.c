/* Copyright (c) 2006-2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdio.h>   /* for file handling operations */
#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <OMX_Other.h>
#include <NvxResourceManager.h>
#include "NvxTrace.h"
#include <stdarg.h>

#include <stdlib.h>  /* malloc, null, etc */
#include <string.h>  /* for strcpy/cmp, etc. */


FILE *pTraceDest = NULL;

OMX_U8 NvxtLogFileName[256];
OMX_U32 NvxtDestinationFlags;

struct NVXTRACEOBJECT NVX_TraceObjects[] = 
{
     /*  {"ObjName", TraceEnableFlag, TraceMask} */
    {"Core",               OMX_FALSE, 0}, 
    {"Scheduler",          OMX_FALSE, 0},
    {"FileReader",         OMX_FALSE, 0}, 
    {"VideoDecoder",       OMX_FALSE, 0},
    {"ImageDecoder",       OMX_FALSE, 0},
    {"AudioDecoder",       OMX_FALSE, 0},
    {"VideoRenderer",      OMX_FALSE, 0},
    {"AudioRenderer",      OMX_FALSE, 0},
    {"VideoScheduler",     OMX_FALSE, 0},
    {"AudioMixer",         OMX_FALSE, 0},
    {"VideoPostProcessor", OMX_FALSE, 0},
    {"ImagePostProcessor", OMX_FALSE, 0},
    {"Camera",             OMX_FALSE, 0},
    {"AudioRecorder",      OMX_FALSE, 0},
    {"VideoEncoder",       OMX_FALSE, 0},
    {"ImageEncoder",       OMX_FALSE, 0},
    {"AudioEncoder",       OMX_FALSE, 0},
    {"FileWriter",         OMX_FALSE, 0},
    {"Clock",              OMX_FALSE, 0},
    {"Generic",            OMX_FALSE, 0},
    {"Undesignated",       OMX_FALSE, 0},
    {"All",                OMX_FALSE, 0},
    {"MaxObjects",         OMX_FALSE, 0}
};

#define NVXT_SET_TRACE_MASK(_o_, _m_)\
    NVX_TraceObjects[_o_].NvxtTraceMask = _m_

#define NVXT_CLEAR_TRACE_MASK(_o_)\
    NVX_TraceObjects[_o_].NvxtTraceMask = 0x0

#define NVXT_SET_TRACE_MASK_BIT(_o_, _m_)\
    NVX_TraceObjects[_o_].NvxtTraceMask |= _m_

#define NVXT_CLEAR_TRACE_MASK_BIT(_o_, _m_)\
    NVX_TraceObjects[_o_].NvxtTraceMask &= ~_m_

static NV_INLINE void NvxtSetTraceMask(OMX_U32 objType, OMX_U32 traceLevels)
{
    NVX_TraceObjects[objType].NvxtTraceMask = traceLevels;
}

static NV_INLINE void NvxtGetTraceMask(OMX_U32 objType, OMX_U32 *pTraceLevels )
{
    if (objType < NVXT_ALLOBJECTS) 
        *pTraceLevels = NVX_TraceObjects[objType].NvxtTraceMask;
}

void NvxtSetDestination(OMX_U32 destinationFlags, const char *logFile)
{
    NvxtDestinationFlags = destinationFlags;
    if(NvxtDestinationFlags == NVXT_STDOUT)
        pTraceDest = stdout;
    else
    {  
        if(pTraceDest != NULL)
        {
            if (pTraceDest != stdout)
                fclose(pTraceDest);
            pTraceDest =(FILE*)fopen((char*)logFile,"w");
        }
    }
}

void NvxtGetDestination(OMX_U32 *pDestinationFlags, OMX_U8 *logFile)
{
    OMX_U8 *s, *d;
    s = NvxtLogFileName;
    d = logFile;
    while (*s != '\0') *d++ = *s++;
    *pDestinationFlags = NvxtDestinationFlags;
}


/* Default implementation */
void NvxtTraceInitDefault(void)
{
    NvxtDestinationFlags = NVXT_STDOUT;
    pTraceDest = stdout;
}
 
void NvxtFlushDefault(void)
{
    //TODO: implement 
}

void NvxtTraceDeInitDefault(void)
{
    if (NvxtDestinationFlags != NVXT_STDOUT ) 
    {
        if (pTraceDest)
            fclose(pTraceDest);
        pTraceDest = NULL;
    }    
}

/* Actual OMX_U32erface used by trace clients*/
NvxtTraceInitFunction   NvxtTraceInit               = NvxtTraceInitDefault;//();    
NvxtPrintfFunction      NvxtTrace                   = NvxtPrintfDefault;
NvxtFlushFunction       NvxtFlush                   = NvxtFlushDefault;                  
NvxtTraceDeInitFunction NvxtTraceDeInit             = NvxtTraceDeInitDefault; 


/* Means of programmatically overriding the trace implementation at runtime */
void NvxtOverrideTraceImplementation(NvxtTraceInitFunction initFunction, 
                                     NvxtPrintfFunction printfFunction,
                                     NvxtFlushFunction  flushFunction,
                                     NvxtTraceDeInitFunction deInitFunction)
{
    NvxtTraceInit   = initFunction; 
    NvxtTrace       = printfFunction;
    NvxtFlush       = flushFunction;
    NvxtTraceDeInit = deInitFunction;
}

void NvxtSetTraceFlag(char* tracetype, OMX_U32 *flag)
{
    if (!strcmp(tracetype, "AllTypes"))
        (*flag) = (*flag) | NVXT_ALLTYPES;
    else if (!strcmp(tracetype, "Error"))           
        (*flag) = (*flag) | NVXT_ERROR;
    else if (!strcmp(tracetype, "Warning"))   
        (*flag) = (*flag) | NVXT_WARNING;
    else if (!strcmp(tracetype, "Buffer"))    
        (*flag) = (*flag) | NVXT_BUFFER;
    else if (!strcmp(tracetype, "Info")) 
        (*flag) = (*flag) | NVXT_INFO;
    else if (!strcmp(tracetype, "Worker")) 
        (*flag) = (*flag) | NVXT_WORKER;
    else if (!strcmp(tracetype, "CallGraph")) 
        (*flag) = (*flag) | NVXT_CALLGRAPH;
    else if (!strcmp(tracetype, "State")) 
        (*flag) = (*flag) | NVXT_STATE;
}

void NvxtConfigReader(OMX_STRING sFilename)
{   
    FILE *fptr = NULL;
    char sLine[512];
    char *sTraceType;
    OMX_U32 iTraceFlag = 0,TraceValue = 0;
    OMX_U32 j=0,k=0;
    char *sArgument;
    char *pC = NULL;
    char *pEnd;
    OMX_U32 ilen;
    char*  sObject;

    memset(sLine, 0, 512);
    pEnd = sLine + 510;
        
    fptr = fopen(sFilename,"r");    

    if (!fptr) {
        //printf("Failed to open the trace config file.\n");
        return;
    }
     
    while (!feof(fptr))
    {
        // add if (blah) {} to get around compiler warning
        if (fgets(sLine, 512, fptr)) {}
        for (pC = &sLine[0]; ((*pC == ' ') || (*pC == '\t')) && pC < pEnd; pC++)
            ; // strip spaces before command

        if (*pC == 'f')                                            // extract filename  
        {              
            pC = pC+1;            
            for (; ((*pC == ' ') || (*pC == '\t')) && pC < pEnd; pC++)
                ; // strip spaces before argument
            
            sArgument = pC;
            ilen = strlen(sArgument); 
            sArgument[ilen - 1] = '\0';   
    
            if (*sArgument == '\0')
                 NvxtSetDestination(NVXT_LOGFILE, DEFAULT_LOG_FILENAME);
            else
            {    //If Log-filename specified
                 NvxtSetDestination(NVXT_LOGFILE, sArgument);
            }             
        }          
        else if (*pC == '{') //Get Object Name
        {                   
            pC = pC + 1;
            // extract objectname
            for (; ((*pC == ' ') || (*pC == ']')) && pC < pEnd; pC++)
                ; // strip spaces before objectname
            sObject = pC;
       
            for (; (*pC != ' ') && (*pC != '\t') && (*pC != '\0') && 
                   (*pC != '\n') && pC < pEnd; pC++)
                ; // null terminate objectname
            *(pC) = '\0';
            for (j = 0; j < NVXT_MAX_OBJECTS; j++)
            {                    
                if (strstr((char*)(NVX_TraceObjects[j].objectname), sObject) != 0)
                {
                    break;
                }
            }
        }
        else if (*pC == '}') //Set TraceMask for the Object
        {
            if (j == NVXT_ALLOBJECTS)
            {
                for (k = 0; k < NVXT_ALLOBJECTS; k++)
                    NvxtSetTraceMask(k, iTraceFlag);
            }
            else
                NvxtSetTraceMask(j, iTraceFlag);
            iTraceFlag = 0;
        }         
        else
        {                 
            for (pC = &sLine[0]; (*pC == ' ') && pC < pEnd; pC++)
                ;     // strip spaces before argument
            sTraceType = pC;
            for (; (*pC != ' ') && pC < pEnd; pC++)
                ;     // null terminate argument
            *(pC) = '\0'; 

            for(pC = &sLine[0]; (*pC != '=') && pC < pEnd; pC++)
                ; //locate '='
            for (; (*pC != ' ') && pC < pEnd; pC++)
                ; //strip spaces till trace value

            if (!strcmp(sTraceType, "enable"))
            {
                if (j == NVXT_ALLOBJECTS)
                {
                    for (k = 0; k < NVXT_ALLOBJECTS; k++)
                    {
                        NVX_TraceObjects[k].TraceEnable = *(pC+1) - '0';
                    }
                }
                else
                    NVX_TraceObjects[j].TraceEnable = *(pC+1) - '0';
            }              
            else
            {
                TraceValue = *(pC+1) - '0';
                if (TraceValue)
                   NvxtSetTraceFlag(sTraceType, &iTraceFlag);
            }
        }
    } //end of while

    fclose(fptr);   
}

void NvxtConfigWriter(OMX_STRING sFilename)
{
    OMX_U32 i;
    FILE *fptr;   
    OMX_U32 pTraceLevels = 0;
    OMX_U32 nMaxObjects = sizeof(NVX_TraceObjects)/sizeof(NVX_TraceObjects[0]);

    fptr = fopen(sFilename,"w");
    if (!fptr)
        return;

    for (i = 0; i < nMaxObjects; i++)
    {        
        fputs("\nObject TYPE is :  ",fptr);
        fprintf(fptr, "%s", (char*)(NVX_TraceObjects[i].objectname));

        NvxtGetTraceMask(i, &pTraceLevels);
  
        if ((pTraceLevels & NVXT_ALLTYPES) == NVXT_ALLTYPES)
            fputs("\nAllTypes",fptr);

        if ((pTraceLevels & NVXT_ERROR) == NVXT_ERROR)
            fputs("\nError",fptr);

        if ((pTraceLevels & NVXT_INFO) == NVXT_INFO)
            fputs("\nInfo",fptr);

        if ((pTraceLevels & NVXT_BUFFER) == NVXT_BUFFER)
            fputs("\nBuffering",fptr);

        if ((pTraceLevels & NVXT_WARNING) == NVXT_WARNING)
            fputs("\nWarning",fptr);  
          
        if ((pTraceLevels & NVXT_WORKER) == NVXT_WORKER)
            fputs("\nWorker",fptr);                            

        if ((pTraceLevels & NVXT_CALLGRAPH) == NVXT_CALLGRAPH)
            fputs("\nCallGraph",fptr); 
        
        if ((pTraceLevels & NVXT_STATE) == NVXT_STATE)
            fputs("\nState",fptr);   
    }
    fclose(fptr);
}

