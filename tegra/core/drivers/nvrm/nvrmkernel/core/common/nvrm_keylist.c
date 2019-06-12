/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvutil.h"
#include "nvrm_keylist.h"

#define NVRM_KEY_ARRAY_LEN 15

// Key node structure
typedef struct KeyRec
{
    NvU32 KeyID[NVRM_KEY_ARRAY_LEN];
    NvU32 Value[NVRM_KEY_ARRAY_LEN];
    NvU32 Count;
    struct KeyRec *pNextKey;
}Key;

// Pointer to the Key Linked-List
static Key s_InitialKeyList;
static Key* s_pKeyList = NULL;

// Handle to mutex for tread safety
static NvOsMutexHandle s_Mutex = NULL;

// Add a new key node to the existing list
static NvError AddKeyToList(NvU32 KeyID, NvU32 Value);

// Frees the Linked-List
static void FreeKeyList(void);

void NvRmPrivDeInitKeyList(NvRmDeviceHandle hRm);


NvError NvRmPrivInitKeyList(
    NvRmDeviceHandle hRm,
    const NvU32 * InitialValues,
    NvU32 InitialCount);

NvError NvRmPrivInitKeyList(NvRmDeviceHandle hRm, 
                            const NvU32 *InitialValues,
                            NvU32 InitialCount)
{
    NvError Error = NvSuccess;
    NvU32 i;

    if (!s_Mutex)
    {
        Error = NvOsMutexCreate(&s_Mutex);
        if (Error!=NvSuccess)
            return Error;
    }

    if (!s_pKeyList)
    {
        s_pKeyList = &s_InitialKeyList;
        s_InitialKeyList.Count = 0;
        s_InitialKeyList.pNextKey = NULL;
        for (i=0; i<InitialCount; i++)
        {
            AddKeyToList(NvOdmKeyListId_ReservedAreaStart + i, 
                InitialValues[i]);
        }
    }

    // Creating the Mutex
    return Error;
}

void NvRmPrivDeInitKeyList(NvRmDeviceHandle hRm)
{
    NvOsMutexLock(s_Mutex);
    FreeKeyList();
    s_pKeyList = NULL;
    NvOsMutexUnlock(s_Mutex);
    NvOsMutexDestroy(s_Mutex);
    s_Mutex = NULL;
}

NvU32 NvRmGetKeyValue(NvRmDeviceHandle hRm, NvU32 KeyID)
{
    Key *pList = s_pKeyList;
    NvU32 Value = 0;
    unsigned int i;

    if (!s_pKeyList)
    {
        return Value;
    }

    NvOsMutexLock(s_Mutex);
    while (pList)
    {
        for (i=0; i<pList->Count; i++)
        {
            if (pList->KeyID[i] == KeyID)
            {
                Value = pList->Value[i];
                goto cleanup;
            }
        }
        pList = pList->pNextKey;
    }
cleanup:
    NvOsMutexUnlock(s_Mutex);
    // Returning value as 0 since key is not present
    return Value;
}

NvError NvRmSetKeyValuePair(NvRmDeviceHandle hRm, NvU32 KeyID, NvU32 Value)
{
    Key *pList = s_pKeyList;
    NvError e = NvSuccess;
    unsigned int i;

    if (KeyID >= NvOdmKeyListId_ReservedAreaStart &&
        KeyID <= NvOdmKeyListId_ReservedAreaEnd)
        return NvError_NotSupported;

    NvOsMutexLock(s_Mutex);
    // Checking if key already exists
    while (pList)
    {
        for (i=0; i<pList->Count; i++)
        {
            if (pList->KeyID[i] == KeyID)
            {
                pList->Value[i] = Value;
                goto cleanup;
            }
        }
        pList = pList->pNextKey;
    }
    // Adding The new key to the list
    e = AddKeyToList(KeyID, Value);
cleanup:
    NvOsMutexUnlock(s_Mutex);
    return e;
}


NvError AddKeyToList(NvU32 KeyID, NvU32 Value)
{
    Key *pList;

    if (s_pKeyList->Count < NVRM_KEY_ARRAY_LEN)
    {
        s_pKeyList->KeyID[s_pKeyList->Count] = KeyID;
        s_pKeyList->Value[s_pKeyList->Count] = Value;
        s_pKeyList->Count++;
    }
    else
    {
        pList = NvOsAlloc(sizeof(Key));

        if (pList == NULL)
            return NvError_InsufficientMemory;

        pList->KeyID[0] = KeyID;
        pList->Value[0] = Value;
        pList->Count = 1;
        pList->pNextKey = s_pKeyList;
        s_pKeyList = pList;
    }

    return NvSuccess;
}

void FreeKeyList(void)
{
    Key *pTemp = s_pKeyList;
    while (s_pKeyList != &s_InitialKeyList)
    {
        pTemp = s_pKeyList->pNextKey ;
        NvOsFree(s_pKeyList);
        s_pKeyList = pTemp;
    }
}
