/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_disasm.h"

typedef void (* NvStreamDisasmInitializeFunc)(NvStreamDisasmContext *context);
typedef NvStreamDisasmContext * (* NvStreamDisasmCreateFunc)(void);
typedef void (* NvStreamDisasmDestroyFunc)(NvStreamDisasmContext *context);
typedef NvS32 (* NvStreamDisasmPrintFunc)(NvStreamDisasmContext *context,
                                          NvU32 value,
                                          char *buffer,
                                          size_t bufferSize);
typedef const char * (* NvStreamDisasmClassToStringFunc)(NvU32 classId);
typedef const char * (* NvStreamDisasmRegisterToStringFunc)(NvU32 classId,
                                                            NvU32 registerAddr);

static int                          s_disasmRefCount = 0;
static NvBool                       s_disasmLoadFailed = NV_FALSE;
static NvOsMutexHandle              s_disasmMutex = NULL;
static NvOsLibraryHandle            s_disasmLibrary = NULL;
static NvStreamDisasmPrintFunc      s_disasmPrint = NULL;
static NvStreamDisasmInitializeFunc s_disasmInitialize = NULL;
static NvStreamDisasmCreateFunc     s_disasmCreate = NULL;
static NvStreamDisasmDestroyFunc    s_disasmDestroy = NULL;

static void
CreateMutex(void)
{
    NvS32 comp;
    NvError err;
    NvOsMutexHandle mutex = NULL;

    err = NvOsMutexCreate(&mutex);

    if (err != NvSuccess)
        return;

    comp = NvOsAtomicCompareExchange32((NvS32 *) &s_disasmMutex, 0,
                                        (NvS32) mutex);

    if (comp != 0)
        NvOsMutexDestroy(mutex);
}

NvBool
NvRmDisasmLibraryLoad(void)
{
    if (s_disasmLoadFailed)
        return NV_FALSE;

    if (s_disasmMutex == NULL)
    {
        CreateMutex();

        if (s_disasmMutex == NULL)
        {
            NvOsDebugPrintf("NvRmDisasmLibraryLoad failure: unable to create mutex\n");
            s_disasmLoadFailed = NV_TRUE;
            return NV_FALSE;
        }
    }

    NvOsMutexLock(s_disasmMutex);

    if (s_disasmLibrary == NULL)
    {
        NvError err;

        err = NvOsLibraryLoad("libnvstreamdisasm", &s_disasmLibrary);

        if (err == NvSuccess)
        {
            s_disasmPrint = (NvStreamDisasmPrintFunc)
                NvOsLibraryGetSymbol(s_disasmLibrary, "NvStreamDisasmPrint");

            s_disasmInitialize = (NvStreamDisasmInitializeFunc)
               NvOsLibraryGetSymbol(s_disasmLibrary, "NvStreamDisasmInitialize");

            s_disasmCreate = (NvStreamDisasmCreateFunc)
               NvOsLibraryGetSymbol(s_disasmLibrary, "NvStreamDisasmCreate");

            s_disasmDestroy = (NvStreamDisasmDestroyFunc)
               NvOsLibraryGetSymbol(s_disasmLibrary, "NvStreamDisasmDestroy");
        }
    }

    if (s_disasmPrint != NULL && s_disasmInitialize != NULL &&
        s_disasmCreate != NULL && s_disasmDestroy != NULL)
    {
        s_disasmRefCount++;
    }
    else
    {
        s_disasmLoadFailed = NV_TRUE;
    }

    NvOsMutexUnlock(s_disasmMutex);

    return s_disasmLoadFailed == NV_FALSE;
}

void
NvRmDisasmLibraryUnload(void)
{
    NvOsMutexLock(s_disasmMutex);

    s_disasmRefCount--;

    if(s_disasmRefCount == 0)
    {
        if (s_disasmLibrary != NULL)
        {
            NvOsLibraryUnload(s_disasmLibrary);
        }

        s_disasmLibrary = NULL;
        s_disasmPrint = NULL;
        s_disasmInitialize = NULL;
        s_disasmCreate = NULL;
        s_disasmDestroy = NULL;
    }

    NvOsMutexUnlock(s_disasmMutex);
}

void
NvRmDisasmInitialize(NvStreamDisasmContext *context)
{
    if (s_disasmInitialize == NULL)
        return;

    s_disasmInitialize(context);
}

NvStreamDisasmContext *
NvRmDisasmCreate(void)
{
    if (s_disasmCreate == NULL)
        return NULL;

    return s_disasmCreate();
}

void
NvRmDisasmDestroy(NvStreamDisasmContext *context)
{
    if (s_disasmDestroy == NULL)
        return;

    s_disasmDestroy(context);
}

/**
 * Interprets the specified value as a channel command value and generates a
 * string represention, which is stored in the specified buffer.
 *
 * This function is a wrapper around NvStreamDisasmPrint, refer to
 * nvstreamdisasm.h for more explanation.
 */
NvS32
NvRmDisasmPrint(NvStreamDisasmContext *context,
                NvU32 value,
                char *buffer,
                size_t bufferSize)
{
    if (s_disasmPrint == NULL)
        return -1;

    return s_disasmPrint(context, value, buffer, bufferSize);
}
