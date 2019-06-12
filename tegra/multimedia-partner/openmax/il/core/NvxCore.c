/* Copyright (c) 2007 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <OMX_Other.h>
#include <NVOMX_ComponentRegister.h>
#include <NVOMX_CustomProtocol.h>
#include "NvxResourceManager.h"
#include "NvxComponent.h"
#include "NvxHelpers.h"
#include "NvxTrace.h"
#include "nvos.h"

#include "../nvmm/include/nvmm_sock.h"
#include "../nvmm/include/nvlocalfilecontentpipe.h"
#include "nvcustomprotocol.h"

#include <string.h>

#define MAX_HANDLES 1024

static OMX_U32 nNumRegisteredComponents = 0;
static OMX_U32 nActiveHandles = 0;
static OMX_HANDLETYPE aAllActiveHandles[MAX_HANDLES];

#define NVX_MAX_COMPONENTS 128
#define NVX_COMPONENT_MAX_ROLES 16

typedef struct NVX_COMPONENT {    
    char *pName;
    OMX_COMPONENTINITTYPE pInitialize;
    OMX_U32 nNumRoles;
    OMX_U8 *sCompRoles[NVX_COMPONENT_MAX_ROLES];
} NVX_COMPONENT;

static NVX_COMPONENT NVX_Components[NVX_MAX_COMPONENTS];

#define HANDLE_HASH(_X_) ((((OMX_U32)(_X_)) / sizeof(OMX_COMPONENTTYPE)) % MAX_HANDLES)

static OMX_U32 FindHandleEntry(OMX_U32 uHash, OMX_HANDLETYPE hToMatch)
{
    OMX_U32 i = uHash;

    while (i < MAX_HANDLES && aAllActiveHandles[i] != hToMatch)
        i++;

    if (i >= MAX_HANDLES)
        for (i = 0; i < uHash && aAllActiveHandles[i] != hToMatch; i++)
            ;

    if (aAllActiveHandles[i] != hToMatch)
        return -1;

    return i;
}

static OMX_ERRORTYPE NvxBuildComponentRoleTable(int nComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE hHandle;
    OMX_COMPONENTTYPE *pComp;
    OMX_U32 nRoleIndex = 0;
    OMX_BOOL bRoleTableError = OMX_FALSE;

    NvxAddCompsToSched(OMX_FALSE);

    pComp = (OMX_COMPONENTTYPE *)NvOsAlloc(sizeof(OMX_COMPONENTTYPE));
    if (!pComp)
        return OMX_ErrorInsufficientResources;

    hHandle = (OMX_HANDLETYPE)pComp;
    pComp->nVersion.s.nVersionMajor    = 1;
    pComp->nVersion.s.nVersionMinor    = 1;
    pComp->nVersion.s.nRevision        = 1;
    pComp->nVersion.s.nStep            = 0;
    pComp->nSize = sizeof(OMX_COMPONENTTYPE);

    eError = NVX_Components[nComp].pInitialize(hHandle);
    if (eError != OMX_ErrorNone) 
        goto cleanupNoComponent;

    /* Find and copy all the roles of the component to the component role 
     * table. 
     */
    nRoleIndex = 0;
    while (eError != OMX_ErrorNoMore)
    {           
        NVX_Components[nComp].sCompRoles[nRoleIndex] = (OMX_U8 *)NvOsAlloc(OMX_MAX_STRINGNAME_SIZE);
        if (NVX_Components[nComp].sCompRoles[nRoleIndex] == NULL) 
        {
            eError = OMX_ErrorInsufficientResources;
            bRoleTableError = OMX_TRUE;
            goto cleanup;
        }
           
        eError = pComp->ComponentRoleEnum(hHandle, 
                        NVX_Components[nComp].sCompRoles[nRoleIndex],
                        nRoleIndex);
 
        if (eError == OMX_ErrorNoMore)
        {
            NvOsFree(NVX_Components[nComp].sCompRoles[nRoleIndex]);
            break;
        }
        else if (eError != OMX_ErrorNone) 
        {
            bRoleTableError = OMX_TRUE;
            goto cleanup;
        }

        nRoleIndex++;
    } 

    NVX_Components[nComp].nNumRoles = nRoleIndex;

    /* Finished finding roles of this component. Release it. */
    if (hHandle) 
    {
        eError = pComp->ComponentDeInit(hHandle);
        hHandle = NULL;
        if (eError != OMX_ErrorNone)
        {
            bRoleTableError = OMX_TRUE;
        }
    }

cleanup:
    /* Clean up in case of an error occured during the above. */
    if (bRoleTableError) 
    {
        OMX_U32 j;
        for (j = 0; j < NVX_Components[nComp].nNumRoles; j++)
        {
            NvOsFree(NVX_Components[nComp].sCompRoles[j]);
            NVX_Components[nComp].sCompRoles[j] = NULL;
        }
        NVX_Components[nComp].nNumRoles = 0;

        if (hHandle)
            pComp->ComponentDeInit(hHandle);
    }

cleanupNoComponent:
    if (pComp)
        NvOsFree(pComp);       

    NvxAddCompsToSched(OMX_TRUE);

    return eError;

}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_RegisterComponent(
    OMX_IN  OMX_COMPONENTREGISTERTYPE *pComponentReg)
{
    int i;
    if (!pComponentReg || !pComponentReg->pName || !pComponentReg->pInitialize)
        return OMX_ErrorBadParameter;

    for (i = 0; i < NVX_MAX_COMPONENTS; i++)
    {
        if (NVX_Components[i].pName &&
            !NvOsStrcmp(NVX_Components[i].pName, pComponentReg->pName))
            return OMX_ErrorInvalidComponentName;
    }

    if (!NVX_Components[nNumRegisteredComponents].pName &&
        !NVX_Components[nNumRegisteredComponents].pInitialize)
    {
        i = nNumRegisteredComponents;
    }
    else
    {
        for (i = 0; i < NVX_MAX_COMPONENTS; i++)
        {
            if (!NVX_Components[i].pName &&
                !NVX_Components[i].pInitialize)
            {
                break;
            }
        }

        if (i == NVX_MAX_COMPONENTS)
            return OMX_ErrorInsufficientResources;
    }

    NVX_Components[i].pName = NvOsAlloc(OMX_MAX_STRINGNAME_SIZE);
    if (!NVX_Components[i].pName)
        return OMX_ErrorInsufficientResources;

    NvOsStrncpy(NVX_Components[i].pName, pComponentReg->pName,
                OMX_MAX_STRINGNAME_SIZE);

    NVX_Components[i].pInitialize = pComponentReg->pInitialize;

    NvxBuildComponentRoleTable(i);

    nNumRegisteredComponents++;
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_DeRegisterComponent(
    OMX_IN OMX_STRING pComponentName)
{
    OMX_U32 i, j;

    if (!pComponentName)
        return OMX_ErrorBadParameter;

    for (i = 0; i < NVX_MAX_COMPONENTS; i++)
    {
        if (!NvOsStrcmp(NVX_Components[i].pName, pComponentName))
        {
            for (j = 0; j < NVX_Components[i].nNumRoles; j++)
            {
                NvOsFree(NVX_Components[i].sCompRoles[j]);
                NVX_Components[i].sCompRoles[j] = NULL;
            }
            NVX_Components[i].nNumRoles = 0;
            NvOsFree(NVX_Components[i].pName);
            NVX_Components[i].pName = NULL;

            nNumRegisteredComponents--;
            break;
        }
    }

    if (i == NVX_MAX_COMPONENTS)
        return OMX_ErrorInvalidComponentName;

    // copy higher vals down
    for (; i < NVX_MAX_COMPONENTS - 1; i++)
    {
        NVX_Components[i] = NVX_Components[i+1];
    }

    return OMX_ErrorNone;
}

static NvOsMutexHandle s_Mutex = 0;
static int s_Initialized = 0;

#if NVOS_IS_LINUX
void __attribute__ ((constructor)) omx_loadlib(void);
void __attribute__ ((destructor)) omx_unloadlib(void);

void omx_loadlib()
{
    if (s_Mutex == NULL)
    {
        NvError e = NvOsMutexCreate(&s_Mutex);
        if (e != NvSuccess)
            NvOsDebugPrintf("Error in creating global mutex for OMX_Init\n");
    }
}

void omx_unloadlib()
{
    if (s_Mutex)
    {
        NvOsMutexDestroy(s_Mutex);
        s_Mutex = NULL;
    }
}
#endif

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int i;
#if !NVOS_IS_LINUX
    {
        NvOsMutexHandle tmpMutex = NULL;

        if (NvSuccess != NvOsMutexCreate(&tmpMutex))
            return OMX_ErrorInsufficientResources;
        if (NvOsAtomicCompareExchange32((NvS32*)&s_Mutex, 0, (NvS32)tmpMutex) != 0)
        {
            NvOsMutexDestroy(tmpMutex);
        }
    }
#endif
    if (s_Mutex == NULL)
    {
        NvOsDebugPrintf("OMX_Init fails as Global mutex is NULL\n");
        return OMX_ErrorBadParameter;
    }
    NvOsMutexLock(s_Mutex);

    if (s_Initialized)
    {
        s_Initialized++;
        NvOsMutexUnlock(s_Mutex);
        return OMX_ErrorNone;
    }

    s_Initialized = 1;

    for (i = 0; i < MAX_HANDLES; i++)
        aAllActiveHandles[i] = NULL;

    for (i=0; i < NVX_MAX_COMPONENTS; i++) 
    {
        NvOsMemset(&NVX_Components[i], 0, sizeof(NVX_COMPONENT));
    }

    eError = NvxInitPlatform();
    if (OMX_ErrorNone != eError)
    {
        NvOsMutexUnlock(s_Mutex);
        return eError;
    }

    NvxtTraceInit();
    NvxtConfigReader("NvxTrace.ini");
    //NvxtConfigWriter("NvxTraceOut.ini");

    i = 0;
    while (OMX_ComponentRegistered[i].pName &&
           OMX_ComponentRegistered[i].pInitialize)
    {
        NVOMX_RegisterComponent(&OMX_ComponentRegistered[i]);
        ++i;
    }

    NvOsMutexUnlock(s_Mutex);

    return eError;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i, j;

    if (!s_Mutex)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(s_Mutex);

    s_Initialized--;
    if (s_Initialized > 0)
    {
        NvOsMutexUnlock(s_Mutex);
        return OMX_ErrorNone;
    }

    for (i = 0; i < MAX_HANDLES && nActiveHandles > 0; i++)
    {
        if (aAllActiveHandles[i])
        {
            OMX_COMPONENTTYPE *pComp = aAllActiveHandles[i];
            OMX_ERRORTYPE eCompError = pComp->ComponentDeInit(aAllActiveHandles[i]);
            aAllActiveHandles[i] = NULL;
            --nActiveHandles;
            NvOsFree(pComp);

            if (eError == OMX_ErrorNone)
                eError = eCompError;
        }
    }

    for (i = 0; i < NVX_MAX_COMPONENTS; i++)
    {
        if (NVX_Components[i].pName)
        {
            for (j = 0; j < NVX_Components[i].nNumRoles; j++)
            {
                NvOsFree(NVX_Components[i].sCompRoles[j]);
                NVX_Components[i].sCompRoles[j] = NULL;
            }
            NVX_Components[i].nNumRoles = 0;
            NvOsFree(NVX_Components[i].pName);
            NVX_Components[i].pName = NULL;

            nNumRegisteredComponents--;
        }
    }

    NvxtTraceDeInit();
    NvxDeinitPlatform();

    NvFreeAllProtocols();

    s_Initialized = 0;
    NvOsMutexUnlock(s_Mutex);
#if !NVOS_IS_LINUX
    NvOsMutexDestroy(s_Mutex);
    s_Mutex = NULL;
#endif


    return eError;
}

/* OMX_ComponentNameEnum */
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    if (nIndex >= nNumRegisteredComponents)
    {
        eError = OMX_ErrorNoMore;
    } 
    else if (cComponentName == NULL || 
             NvOsStrlen(NVX_Components[nIndex].pName) + 1 >= nNameLength)
    {
        eError = OMX_ErrorBadParameter;
    }
    else
    {
        NvOsMemset(cComponentName, 0, nNameLength);
        NvOsStrncpy(cComponentName, NVX_Components[nIndex].pName,
                    NvOsStrlen(NVX_Components[nIndex].pName));
    }

    return eError;
}

/* OMX_GetHandle */
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle, 
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{    
    OMX_ERRORTYPE eError;
    OMX_COMPONENTTYPE *pComp;
    OMX_U32 i, uEntry;

    if (pHandle == NULL || cComponentName == NULL || pCallBacks == NULL)
        return OMX_ErrorBadParameter;

    if (!s_Mutex)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(s_Mutex);

    for (i = 0; i < nNumRegisteredComponents; i++) 
    {
        if (!NvOsStrcmp(NVX_Components[i].pName, cComponentName)) 
        {
            OMX_HANDLETYPE hHandle = 0;
            pComp = (OMX_COMPONENTTYPE *)NvOsAlloc(sizeof(OMX_COMPONENTTYPE));
            if (!pComp)
            {
                NvOsMutexUnlock(s_Mutex);
                return OMX_ErrorInsufficientResources;
            }

            hHandle = (OMX_HANDLETYPE)pComp;
            pComp->nVersion.s.nVersionMajor    = 1;
            pComp->nVersion.s.nVersionMinor    = 1;
            pComp->nVersion.s.nRevision        = 1;
            pComp->nVersion.s.nStep            = 0;
            pComp->nSize = sizeof(OMX_COMPONENTTYPE);

            eError = NVX_Components[i].pInitialize(hHandle);
            if (eError == OMX_ErrorNone) 
            {
                eError = pComp->SetCallbacks(hHandle, pCallBacks, pAppData);
                if (eError != OMX_ErrorNone)
                    pComp->ComponentDeInit(hHandle);
            }
            
            if (eError == OMX_ErrorNone) 
            {
                uEntry = FindHandleEntry(HANDLE_HASH(hHandle), NULL);
                if (uEntry == -1)
                    eError = OMX_ErrorInsufficientResources;
            }

            if (eError == OMX_ErrorNone)
            {
                nActiveHandles++;
                *pHandle = hHandle;
                aAllActiveHandles[uEntry] = hHandle;
            }
            else 
            {
                *pHandle = NULL;
                NvOsFree(pComp);
            }

            NvOsMutexUnlock(s_Mutex);
            return eError;
        }
    }

    NvOsMutexUnlock(s_Mutex);
    return OMX_ErrorComponentNotFound;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComp;
    OMX_U32 uEntry;

    if (hComponent == NULL || s_Mutex == NULL)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(s_Mutex);

    uEntry = FindHandleEntry(HANDLE_HASH(hComponent), hComponent);
    if (uEntry == -1)
    {
        NvOsMutexUnlock(s_Mutex);
        return OMX_ErrorBadParameter;
    }

    pComp = (OMX_COMPONENTTYPE*)hComponent;
    eError = pComp->ComponentDeInit(hComponent);
    if (eError == OMX_ErrorNone) {
        aAllActiveHandles[uEntry] = NULL;
        --nActiveHandles;
        NvOsFree(pComp);
    }

    NvOsMutexUnlock(s_Mutex);
    return eError;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN  OMX_HANDLETYPE hOutput,
    OMX_IN  OMX_U32 nPortOutput,
    OMX_IN  OMX_HANDLETYPE hInput,
    OMX_IN  OMX_U32 nPortInput)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pCompIn, *pCompOut;
    OMX_TUNNELSETUPTYPE oTunnelSetup;

    if ((hOutput == NULL && hInput == NULL) || s_Mutex == NULL)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(s_Mutex);

    oTunnelSetup.nTunnelFlags = 0;
    oTunnelSetup.eSupplier = OMX_BufferSupplyUnspecified;

    pCompOut = (OMX_COMPONENTTYPE*)hOutput;

    if (hOutput)
    {
        eError = pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, hInput,
                                                  nPortInput, &oTunnelSetup);
    }

    if (eError == OMX_ErrorNone && hInput) 
    {
        pCompIn = (OMX_COMPONENTTYPE*)hInput;
        eError = pCompIn->ComponentTunnelRequest(hInput, nPortInput, hOutput, 
                                                 nPortOutput, &oTunnelSetup);

        if (eError != OMX_ErrorNone && hOutput) 
        {
            /* cancel tunnel request on output port since input port failed */
            pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, 
                                             NULL);
        }
    }

    NvOsMutexUnlock(s_Mutex);
    return eError;
}

OMX_API OMX_ERRORTYPE OMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE *hPipe,
    OMX_IN OMX_STRING szURI)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMGetContentPipe((CP_PIPETYPE_EXTENDED **)hPipe);
    return eError;
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(
    OMX_IN     OMX_STRING role,
    OMX_INOUT  OMX_U32 *numComps,
    OMX_INOUT  OMX_U8 **compNames) 
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i, j, matchingComps;

    if (role == NULL || numComps == NULL)
        return OMX_ErrorBadParameter;
 
    // for the case where we're checking how many comps there are
    if (*numComps == 0 && compNames == NULL)
        *numComps = 256;
    
    matchingComps = 0;
    for (i = 0; (i < nNumRegisteredComponents) && 
                (matchingComps < *numComps); i++) 
    {
        for (j = 0; (j < NVX_Components[i].nNumRoles) && 
                    (matchingComps < *numComps); j++) 
        {
            if (strstr((char*)(NVX_Components[i].sCompRoles[j]), role) != NULL)
            {
                if (compNames != NULL) 
                {
                    NvOsStrncpy((char*)(compNames[matchingComps]),
                                NVX_Components[i].pName,
                                OMX_MAX_STRINGNAME_SIZE);
                }

                matchingComps++;
            }
        }
    }       

    *numComps = matchingComps;

    return eError;
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent(
    OMX_IN     OMX_STRING compName,
    OMX_INOUT  OMX_U32 *numRoles,
    OMX_INOUT  OMX_U8 **roles)
{
    OMX_U32 i, j;
    OMX_U32 totRoles;

    if (compName == NULL)
        return OMX_ErrorInvalidComponentName;      
   
    for (i = 0; i < nNumRegisteredComponents; i++) 
    {        
        if (!NvOsStrcmp(NVX_Components[i].pName, compName))
            break;   // component found.
    }

    if (i >= nNumRegisteredComponents) 
        return OMX_ErrorInvalidComponentName;   

    if (roles == NULL)
    {
        *numRoles = NVX_Components[i].nNumRoles; 
        return OMX_ErrorNone;
    }

    totRoles = NVX_Components[i].nNumRoles;
    if (*numRoles < totRoles)
        totRoles = *numRoles;
    
    for (j=0; j < totRoles; j++)
        NvOsStrncpy((char*)(roles[j]), (char*)(NVX_Components[i].sCompRoles[j]),
                    OMX_MAX_STRINGNAME_SIZE);

    *numRoles = totRoles;

    return OMX_ErrorNone;
}   

CPresult NVOMX_RegisterNVCustomProtocol(OMX_STRING szProto, 
                                        NV_CUSTOM_PROTOCOL *pProtocol)
{
    return NvRegisterProtocol(szProto, pProtocol);
}

void NVOMX_BlockAllSocketActivity(int block)
{
    NvMMSockSetBlockActivity(block);
}

