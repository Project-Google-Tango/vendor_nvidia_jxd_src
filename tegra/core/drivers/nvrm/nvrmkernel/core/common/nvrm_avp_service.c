/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *              Testcases for the xpc </b>
 *
 * @b Description: This file implements the AVP service to handle AVP messages.
 */

#ifndef NVRM_TRANSPORT_IN_KERNEL
#define NVRM_TRANSPORT_IN_KERNEL 0
#endif

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_init.h"
#include "nvrm_message.h"
#include "nvrm_rpc.h"
#include "nvrm_moduleloader_private.h"
#include "nvrm_avp_private.h"
#include "arres_sema.h"
#include "arflow_ctlr.h"
#include "arapbpm.h"
#include "nvrm_avp_swi_registry.h"
#include "arevp.h"
#include "nvrm_hardware_access.h"

static NvOsMutexHandle s_AvpInitMutex = NULL;
static NvU32 s_AvpInitialized = NV_FALSE;
static NvRmLibraryHandle s_hAvpLibrary = NULL;

#define NV_USE_AOS 1

#define AVP_EXECUTABLE_NAME "nvrm_avp.axf"

void NvRmPrivProcessMessage(NvRmRPCHandle hRPCHandle, char *pRecvMessage, int messageLength)
{
    NvError Error = NvSuccess;
    NvRmMemHandle    hMem;

    switch ((NvRmMsg)*pRecvMessage)
    {
    case NvRmMsg_MemHandleCreate :
        {
            NvRmMessage_HandleCreat *msgHandleCreate = NULL;
            NvRmMessage_HandleCreatResponse msgRHandleCreate;

            msgHandleCreate = (NvRmMessage_HandleCreat*)pRecvMessage;

            msgRHandleCreate.msg = NvRmMsg_MemHandleCreate;
            Error = NvRmMemHandleCreate(hRPCHandle->hRmDevice,&hMem, msgHandleCreate->size);
            if (!Error)
            {
                msgRHandleCreate.hMem = hMem;
            }
            msgRHandleCreate.msg = NvRmMsg_MemHandleCreate_Response;
            msgRHandleCreate.error = Error;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgRHandleCreate, \
                                sizeof(msgRHandleCreate));
        }
        break;
    case NvRmMsg_MemHandleOpen :
        break;
    case NvRmMsg_MemHandleFree :
        {
            NvRmMessage_HandleFree *msgHandleFree = NULL;
            msgHandleFree = (NvRmMessage_HandleFree*)pRecvMessage;
            NvRmMemHandleFree(msgHandleFree->hMem);
        }
        break;
    case NvRmMsg_MemAlloc :
        {
            NvRmMessage_MemAlloc *msgMemAlloc = NULL;
            NvRmMessage_Response msgResponse;
            msgMemAlloc = (NvRmMessage_MemAlloc*)pRecvMessage;

            Error = NvRmMemAllocTagged(
                 msgMemAlloc->hMem,
                 (msgMemAlloc->NumHeaps == 0) ? NULL : msgMemAlloc->Heaps,
                 msgMemAlloc->NumHeaps,
                 msgMemAlloc->Alignment,
                 msgMemAlloc->Coherency,
                 NVRM_MEM_TAG_RM_MISC);
            msgResponse.msg   = NvRmMsg_MemAlloc_Response;
            msgResponse.error = Error;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_MemPin :
        {
            NvRmMessage_Pin *msgHandleFree = NULL;
            NvRmMessage_PinResponse msgResponse;
            msgHandleFree = (NvRmMessage_Pin*)pRecvMessage;

            msgResponse.address = NvRmMemPin(msgHandleFree->hMem);
            msgResponse.msg   = NvRmMsg_MemPin_Response;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_MemUnpin :
        {
            NvRmMessage_HandleFree *msgHandleFree = NULL;
            NvRmMessage_Response msgResponse;
            msgHandleFree = (NvRmMessage_HandleFree*)pRecvMessage;

            NvRmMemUnpin(msgHandleFree->hMem);

            msgResponse.msg   = NvRmMsg_MemUnpin_Response;
            msgResponse.error = NvSuccess;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_MemGetAddress :
        {
            NvRmMessage_GetAddress *msgGetAddress = NULL;
            NvRmMessage_GetAddressResponse msgGetAddrResponse;

            msgGetAddress = (NvRmMessage_GetAddress*)pRecvMessage;

            msgGetAddrResponse.msg     = NvRmMsg_MemGetAddress_Response;
            msgGetAddrResponse.address = NvRmMemGetAddress(msgGetAddress->hMem,msgGetAddress->Offset);

            NvRmPrivRPCSendMsg(hRPCHandle, &msgGetAddrResponse, sizeof(msgGetAddrResponse));
        }
        break;
    case NvRmMsg_HandleFromId :
        {
            NvRmMessage_HandleFromId *msgHandleFromId = NULL;
            NvRmMessage_Response msgResponse;
            NvRmMemHandle hMem;

            msgHandleFromId = (NvRmMessage_HandleFromId*)pRecvMessage;

            msgResponse.msg     = NvRmMsg_HandleFromId_Response;
            msgResponse.error = NvRmMemHandleFromId(msgHandleFromId->id, &hMem);

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(NvRmMessage_Response));
        }
        break;
    case NvRmMsg_PowerModuleClockControl:
        {
            NvRmMessage_Module *msgPMCC;
            NvRmMessage_Response msgPMCCR;
            msgPMCC = (NvRmMessage_Module*)pRecvMessage;

            msgPMCCR.msg = NvRmMsg_PowerModuleClockControl_Response;
            msgPMCCR.error = NvRmPowerModuleClockControl(hRPCHandle->hRmDevice,
                                                          msgPMCC->ModuleId,
                                                          msgPMCC->ClientId,
                                                          msgPMCC->Enable);

            NvRmPrivRPCSendMsg(hRPCHandle, &msgPMCCR, sizeof(msgPMCCR));
        }
        break;
    case NvRmMsg_ModuleReset:
        {
            NvRmMessage_Module *msgPMCC;
            NvRmMessage_Response msgPMCCR;
            msgPMCC = (NvRmMessage_Module*)pRecvMessage;

            msgPMCCR.msg = NvRmMsg_ModuleReset_Response;

            NvRmModuleReset(hRPCHandle->hRmDevice, msgPMCC->ModuleId);
            /// Send response since clients to this call needs to wait
            /// for some time before they can start using the HW module
            NvRmPrivRPCSendMsg(hRPCHandle, &msgPMCCR, sizeof(msgPMCCR));
        }
        break;

    case NvRmMsg_PowerRegister:
        {
            NvRmMessage_PowerRegister *msgPower;
            NvRmMessage_PowerRegister_Response msgResponse;
            NvU32 clientId;

            msgPower = (NvRmMessage_PowerRegister*)pRecvMessage;
            clientId = msgPower->clientId;

            Error = NvRmPowerRegister(hRPCHandle->hRmDevice, msgPower->eventSema, &clientId);
            msgResponse.msg   = NvRmMsg_PowerResponse;
            msgResponse.error = Error;
            msgResponse.clientId = clientId;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));

        }
        break;

    case NvRmMsg_PowerUnRegister:
        {
            NvRmMessage_PowerUnRegister *msgPower;

            msgPower = (NvRmMessage_PowerUnRegister*)pRecvMessage;
            NvRmPowerUnRegister(hRPCHandle->hRmDevice, msgPower->clientId);

        }
        break;
    case NvRmMsg_PowerStarvationHint:
        {
            NvRmMessage_PowerStarvationHint *msgPowerHint;
            NvRmMessage_Response msgResponse;

            msgPowerHint = (NvRmMessage_PowerStarvationHint*)pRecvMessage;
            Error = NvRmPowerStarvationHint(hRPCHandle->hRmDevice, msgPowerHint->clockId, msgPowerHint->clientId, msgPowerHint->starving);

            msgResponse.msg   = NvRmMsg_PowerResponse;
            msgResponse.error = Error;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));


        }
        break;
    case NvRmMsg_PowerBusyHint:
        {
            NvRmMessage_PowerBusyHint *msgPowerHint;
            NvRmMessage_Response msgResponse;

            msgPowerHint = (NvRmMessage_PowerBusyHint*)pRecvMessage;
            Error = NvRmPowerBusyHint(hRPCHandle->hRmDevice, msgPowerHint->clockId, msgPowerHint->clientId, msgPowerHint->boostDurationMS, msgPowerHint->boostKHz);

            msgResponse.msg   = NvRmMsg_PowerResponse;
            msgResponse.error = Error;

            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));


        }
        break;
    case NvRmMsg_PowerBusyMultiHint:
        {
            NvRmMessage_PowerBusyMultiHint *multiHint;
            NvRm_PowerBusyHint *msgPowerHint;
            NvU32 i;
            NvRm_PowerBusyHint *busyPtr;

            multiHint = (NvRmMessage_PowerBusyMultiHint*)pRecvMessage;
            busyPtr = (NvRm_PowerBusyHint*)multiHint->busyHints;

            for (i=0;i<multiHint->numHints;i++)
            {
                msgPowerHint = busyPtr;
                (void)NvRmPowerBusyHint(hRPCHandle->hRmDevice, msgPowerHint->clockId, msgPowerHint->clientId, msgPowerHint->boostDurationMS, msgPowerHint->boostKHz);

                busyPtr++;
            }
        }
        break;
    case NvRmMsg_PowerDfsGetState:
        {
            NvRmMessage_PowerDfsGetState_Response msgResponse;
            NvRmDfsRunState dfsState;

            dfsState = NvRmDfsGetState(hRPCHandle->hRmDevice);

            msgResponse.msg = NvRmMsg_PowerDfsGetState_Response;
            msgResponse.state = dfsState;
            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_PowerModuleGetMaxFreq:
        {
            NvRmMessage_PowerModuleGetMaxFreq *msgGetFreq;
            NvRmMessage_PowerModuleGetMaxFreq_Response msgResponse;
            NvRmFreqKHz freqKHz;

            msgGetFreq = (NvRmMessage_PowerModuleGetMaxFreq*)pRecvMessage;
            freqKHz = NvRmPowerModuleGetMaxFrequency(hRPCHandle->hRmDevice, msgGetFreq->moduleID);

            msgResponse.msg = NvRmMsg_PowerModuleGetMaxFreq;
            msgResponse.freqKHz = freqKHz;
            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_PowerDfsGetClockUtilization:
        {
            NvRmMessage_PowerDfsGetClockUtilization *msgGetClockUtilization;
            NvRmMessage_PowerDfsGetClockUtilization_Response msgResponse;
            NvRmDfsClockUsage ClockUsage;
            NvRmDfsClockUsage *pClockUsage = &ClockUsage;

            msgGetClockUtilization = (NvRmMessage_PowerDfsGetClockUtilization*)pRecvMessage;
            Error = NvRmDfsGetClockUtilization(
                        hRPCHandle->hRmDevice,
                        msgGetClockUtilization->clockId,
                        pClockUsage);

            msgResponse.msg = NvRmMsg_PowerDfsGetClockUtilization_Response;
            msgResponse.error =  Error;
            NvOsMemcpy(&msgResponse.clockUsage, pClockUsage, sizeof(NvRmDfsClockUsage));
            NvRmPrivRPCSendMsg(hRPCHandle, &msgResponse, sizeof(msgResponse));
        }
        break;
    case NvRmMsg_InitiateLP0:
        {
            //Just for testing purposes.
        }
        break;
    case NvRmMsg_RemotePrintf:
        {
            NvRmMessage_RemotePrintf *msg;

            msg = (NvRmMessage_RemotePrintf*)pRecvMessage;
            NvOsDebugPrintf("AVP: %s", msg->string);
        }
        break;
    case NvRmMsg_AVP_Reset:
        NvOsDebugPrintf("AVP has been reset by WDT\n");
        break;
    default:
        NV_ASSERT( !"AVP Service::ProcessMessage: bad message" );
        break;
    }
}

/* FIXME */
extern NvError NvRmPrivInitModuleLoaderRPC(NvRmDeviceHandle hDevice);
extern void NvRmPrivDeInitModuleLoaderRPC(void);
typedef unsigned char __attribute__((aligned)) alignedNvU8;

NvError
NvRmPrivInitAvp(NvRmDeviceHandle hDevice)
{
    NvError err = NvSuccess;
#if !NVOS_IS_WINDOWS
    void* avpExecutionJumpAddress;
    NvU32 RegVal, resetVector;
#endif
    NvOsMutexHandle avpMutex = NULL;
    NvRmPhysAddr phys_ev, phys_flowctrl;
    NvU32 size_ev, size_flowctrl;
    alignedNvU8 *virtualAddress_ev = NULL;
    alignedNvU8 *virtualAddress_flowctrl = NULL;

#if NVOS_IS_LINUX || NVOS_IS_LINUX_KERNEL || NVOS_IS_WINDOWS
    /* FIXME - return sucess for now */
    return NvSuccess;
#endif

    if (s_AvpInitMutex == NULL)
    {
        err = NvOsMutexCreate(&avpMutex);
        if (err != NvSuccess)
            return err;

        if (NvOsAtomicCompareExchange32((NvS32*)&s_AvpInitMutex, 0,
                (NvS32)avpMutex) != 0)
            NvOsMutexDestroy(avpMutex);
    }

    NvOsMutexLock(s_AvpInitMutex);
    if (s_AvpInitialized == NV_FALSE)
    {
        NvRmModuleGetBaseAddress(hDevice, NVRM_MODULE_ID(NvRmModuleID_ExceptionVector, 0),
                &phys_ev, &size_ev);
        err = NvRmPhysicalMemMap(phys_ev, size_ev, NVOS_MEM_READ_WRITE,
                  NvOsMemAttribute_Uncached, (void **)&(virtualAddress_ev));
        if (err)
        {
            NvOsMutexUnlock(s_AvpInitMutex);
            return err;
        }

        NvRmModuleGetBaseAddress(hDevice, NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0),
                &phys_flowctrl, &size_flowctrl);
        err = NvRmPhysicalMemMap(phys_flowctrl, size_flowctrl,
                  NVOS_MEM_READ_WRITE, NvOsMemAttribute_Uncached,
                  (void **)&(virtualAddress_flowctrl));
        if (err)
        {
            NvOsPhysicalMemUnmap((void *)virtualAddress_ev, size_ev);
            NvOsMutexUnlock(s_AvpInitMutex);
            return err;
        }

#if !NVOS_IS_WINDOWS
        err = NvRmPrivLoadKernelLibrary(hDevice, AVP_EXECUTABLE_NAME, &s_hAvpLibrary);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("AVP executable file not found\n"));
            NV_ASSERT(!"AVP executable file not found");
        }
        err = NvRmGetProcAddress(s_hAvpLibrary, "main", &avpExecutionJumpAddress);
        NV_ASSERT(err == NvSuccess);

        resetVector = (NvU32)avpExecutionJumpAddress & 0xFFFFFFFE;

        NV_WRITE32(virtualAddress_ev +  EVP_COP_RESET_VECTOR_0, resetVector);
        RegVal = NV_READ32(virtualAddress_ev +  EVP_COP_RESET_VECTOR_0);
        NV_ASSERT( RegVal == resetVector );

        NvRmModuleReset(hDevice, NvRmModuleID_Avp);

        /// Resume AVP
        RegVal = NV_DRF_DEF(FLOW_CTLR, HALT_COP_EVENTS, MODE, FLOW_MODE_NONE);
        NV_WRITE32(virtualAddress_flowctrl +  FLOW_CTLR_HALT_COP_EVENTS_0, RegVal);
#endif
        NvOsPhysicalMemUnmap((void *)virtualAddress_ev, size_ev);
        NvOsPhysicalMemUnmap((void *)virtualAddress_flowctrl, size_flowctrl);
#if NVRM_TRANSPORT_IN_KERNEL
        err = NvRmTransportInit(hDevice);
#else
        err = NvRmPrivTransportInit(hDevice);
#endif
        if (err)
        {
            goto fail;
        }
        s_AvpInitialized = NV_TRUE;
        // at this point you can load libraries and stuff.

        err = NvRmPrivInitService(hDevice);
        if (err)
        {
            goto fail;
        }
        err = NvRmPrivInitModuleLoaderRPC(hDevice);
        if (err)
        {
            goto fail;
        }
    }
fail:
    NvOsMutexUnlock(s_AvpInitMutex);
    return err;
}

void NvRmPrivDeInitAvp(NvRmDeviceHandle hDevice)
{
    if (s_AvpInitialized == NV_FALSE || s_AvpInitMutex == NULL)
    {
        /* AVP is not used */
        return;
    }

    // this has to happen before transport shutdown, since it uses the
    // transport.
    NvRmPrivDeInitModuleLoaderRPC();
    NvRmPrivServiceDeInit();

    // Deinitializing the transport
#if NVRM_TRANSPORT_IN_KERNEL
    NvRmTransportDeInit(hDevice);
#else
    NvRmPrivTransportDeInit(hDevice);
#endif

#if !NVOS_IS_WINDOWS
    NvRmFreeLibrary(s_hAvpLibrary);
#endif
    s_AvpInitialized = NV_FALSE;
    NvOsMutexDestroy(s_AvpInitMutex);
}


