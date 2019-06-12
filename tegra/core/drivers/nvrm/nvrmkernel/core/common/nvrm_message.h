/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVRM_MESSAGE_H
#define INCLUDED_NVRM_MESSAGE_H

#include "nvrm_memmgr.h"
#include "nvrm_module.h"
#include "nvrm_transport.h"
#include "nvrm_power.h"

// For AOS, use legacy way of AVP kernel and moduleloader.
#if defined(NV_USE_AOS) && defined(NV_IS_AVP)
  #if NV_USE_AOS == 1 && NV_IS_AVP == 1
    #error "Unknown case"
  #endif
#endif
#ifndef NV_LEGACY_AVP_KERNEL
  #if defined(NV_USE_AOS) && NV_USE_AOS == 1
    #define NV_LEGACY_AVP_KERNEL 1
  #else
    #define NV_LEGACY_AVP_KERNEL 0
  #endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// Maximum message queue depth
enum {MAX_QUEUE_DEPTH = 5};
// Maximum message length
enum {MAX_MESSAGE_LENGTH = 256};
// Maximum argument size
enum {MAX_ARGS_SIZE = 220};
// Max String length
enum {MAX_STRING_LENGTH = 200};

typedef struct NvRmRPCRec 
{
    NvRmTransportHandle svcTransportHandle;
    NvOsSemaphoreHandle TransportRecvSemId;
    NvOsMutexHandle RecvLock;
    NvRmDeviceHandle hRmDevice;
    NvBool isConnected;
} NvRmRPC;

typedef struct NvRmRPCRec *NvRmRPCHandle;

void NvRmPrivProcessMessage(NvRmRPCHandle hRPCHandle, char *pRecvMessage, int messageLength);

typedef enum
{
    NvRmMsg_MemHandleCreate = 0x0,
    NvRmMsg_MemHandleCreate_Response,
    NvRmMsg_MemHandleOpen,
    NvRmMsg_MemHandleFree,
    NvRmMsg_MemAlloc,
    NvRmMsg_MemAlloc_Response,
    NvRmMsg_MemPin,
    NvRmMsg_MemPin_Response,
    NvRmMsg_MemUnpin,
    NvRmMsg_MemUnpin_Response,
    NvRmMsg_MemGetAddress,
    NvRmMsg_MemGetAddress_Response,
    NvRmMsg_HandleFromId,
    NvRmMsg_HandleFromId_Response,
    NvRmMsg_PowerModuleClockControl,
    NvRmMsg_PowerModuleClockControl_Response,
    NvRmMsg_ModuleReset,
    NvRmMsg_ModuleReset_Response,
    NvRmMsg_PowerRegister,
    NvRmMsg_PowerUnRegister,    
    NvRmMsg_PowerStarvationHint,
    NvRmMsg_PowerBusyHint,
    NvRmMsg_PowerBusyMultiHint,
    NvRmMsg_PowerDfsGetState,    
    NvRmMsg_PowerDfsGetState_Response,
    NvRmMsg_PowerResponse,
    NvRmMsg_PowerModuleGetMaxFreq,
    NvRmMsg_InitiateLP0,
    NvRmMsg_InitiateLP0_Response,
    NvRmMsg_RemotePrintf,
    NvRmMsg_AttachModule,
    NvRmMsg_AttachModule_Response,
    NvRmMsg_DetachModule,
    NvRmMsg_DetachModule_Response,
    NvRmMsg_AVP_Reset,
    NvRmMsg_PowerDfsGetClockUtilization,
    NvRmMsg_PowerDfsGetClockUtilization_Response,
    NvRmMsg_PowerModuleClockConfig,
    NvRmMsg_PowerModuleClockConfig_Response,
    NvRmMsg_PowerModuleGetClockConfig,
    NvRmMsg_PowerModuleGetClockConfig_Response,
    NvRmMsg_Force32 = 0x7FFFFFFF
} NvRmMsg;

struct NvRmMessage_HandleCreat {
    NvRmMsg         msg;
    NvU32           size;
} __attribute__((packed));
typedef struct NvRmMessage_HandleCreat NvRmMessage_HandleCreat;

typedef struct {
    NvRmMsg         msg;
    NvRmMemHandle   hMem;
    NvError         error;
} NvRmMessage_HandleCreatResponse;

struct NvRmMessage_HandleFree {
    NvRmMsg        msg;
    NvRmMemHandle  hMem;
} __attribute((packed));
typedef struct NvRmMessage_HandleFree NvRmMessage_HandleFree;

typedef struct {
    NvRmMsg     msg;
    NvError     error;
} NvRmMessage_Response;

struct NvRmMessage_MemAlloc {
    NvRmMsg             msg;
    NvRmMemHandle       hMem;
    NvRmHeap            Heaps[4];
    NvU32               NumHeaps;
    NvU32               Alignment;
    NvOsMemAttribute    Coherency;
} __attribute__((packed));
typedef struct NvRmMessage_MemAlloc NvRmMessage_MemAlloc;

struct NvRmMessage_GetAddress {
    NvRmMsg       msg;
    NvRmMemHandle hMem;
    NvU32         Offset;
} __attribute__((packed));
typedef struct NvRmMessage_GetAddress NvRmMessage_GetAddress;

struct NvRmMessage_GetAddressResponse {
    NvRmMsg       msg;
    NvU32         address;
} __attribute__((packed));
typedef struct NvRmMessage_GetAddressResponse NvRmMessage_GetAddressResponse;

struct NvRmMessage_HandleFromId {
    NvRmMsg       msg;
    NvU32         id;
} __attribute__((packed));
typedef struct NvRmMessage_HandleFromId NvRmMessage_HandleFromId;

struct NvRmMessage_Pin {
    NvRmMsg       msg;
    NvRmMemHandle hMem;
} __attribute__((packed));
typedef struct NvRmMessage_Pin NvRmMessage_Pin;

typedef struct {
    NvRmMsg       msg;
    NvU32         address;
} NvRmMessage_PinResponse;

struct NvRmMessage_Module {
    NvRmMsg         msg;
    NvRmModuleID    ModuleId;
    NvU32           ClientId;
    NvBool          Enable;
} __attribute__((packed));
typedef struct NvRmMessage_Module NvRmMessage_Module;

struct NvRmMessage_PowerRegister {
    NvRmMsg msg;
    NvU32 clientId;
    NvOsSemaphoreHandle eventSema;    
} __attribute__((packed));
typedef struct NvRmMessage_PowerRegister NvRmMessage_PowerRegister;

struct NvRmMessage_PowerUnRegister {
    NvRmMsg msg;
    NvU32 clientId;
} __attribute__((packed));
typedef struct NvRmMessage_PowerUnRegister NvRmMessage_PowerUnRegister;

struct NvRmMessage_PowerStarvationHint {
    NvRmMsg msg;
    NvRmDfsClockId clockId;
    NvU32 clientId;
    NvBool starving;
} __attribute__((packed));
typedef struct NvRmMessage_PowerStarvationHint NvRmMessage_PowerStarvationHint;

struct NvRmMessage_PowerBusyHint{
    NvRmMsg msg;
    NvRmDfsClockId clockId;
    NvU32 clientId;
    NvU32 boostDurationMS;
    NvRmFreqKHz boostKHz;
} __attribute__((packed));
typedef struct NvRmMessage_PowerBusyHint NvRmMessage_PowerBusyHint;

struct NvRmMessage_PowerBusyMultiHint {
    NvRmMsg msg;
    NvU32 numHints;
    NvU32 busyHints[MAX_STRING_LENGTH/4];
} __attribute__((packed));
typedef struct NvRmMessage_PowerBusyMultiHint NvRmMessage_PowerBusyMultiHint;

typedef struct {
    NvRmMsg msg;
} NvRmMessage_PowerDfsGetState;

typedef struct {
    NvRmMsg msg;
    NvError error;
    NvU32 clientId;
} NvRmMessage_PowerRegister_Response;

typedef struct{
    NvRmMsg msg;
    NvRmDfsRunState state;
} NvRmMessage_PowerDfsGetState_Response;

struct NvRmMessage_PowerModuleGetMaxFreq {
    NvRmMsg msg;
    NvRmModuleID moduleID;
} __attribute__((packed));
typedef struct NvRmMessage_PowerModuleGetMaxFreq NvRmMessage_PowerModuleGetMaxFreq;

typedef struct {
    NvRmMsg msg;
    NvRmFreqKHz freqKHz;
} NvRmMessage_PowerModuleGetMaxFreq_Response;

struct NvRmMessage_PowerDfsGetClockUtilization {
    NvRmMsg msg;
    NvError error;
    NvRmDfsClockId clockId;
} __attribute__((packed));
typedef struct NvRmMessage_PowerDfsGetClockUtilization NvRmMessage_PowerDfsGetClockUtilization;

typedef struct {
    NvRmMsg msg;
    NvError error;
    NvRmDfsClockUsage clockUsage;
} NvRmMessage_PowerDfsGetClockUtilization_Response;

typedef struct {
    NvRmMsg msg;
    NvU32 sourceAddr;
    NvU32 bufferAddr;
    NvU32 bufferSize;
} NvRmMessage_InitiateLP0;

struct NvRmMessage_RemotePrintf{
    NvRmMsg msg;
    const char string[MAX_STRING_LENGTH];
} __attribute__((packed));
typedef struct NvRmMessage_RemotePrintf NvRmMessage_RemotePrintf;

typedef struct {
    NvRmMsg      msg;
    NvRmModuleID ModuleId;
    NvRmFreqKHz  Freq;
} NvRmMessage_PowerModuleClockConfig;

typedef struct {
    NvRmMsg     msg;
    NvError     error;
    NvRmFreqKHz CurrentFreq;
} NvRmMessage_PowerModuleClockConfig_Response;

typedef struct {
    NvRmMsg       msg;
#if NV_LEGACY_AVP_KERNEL == 1
    NvU32         entryAddress;
#else
    NvU32         address;
#endif
    NvU32         size;
    NvU32         filesize;
    char          args[MAX_ARGS_SIZE];
    NvU32         reason;
} NvRmMessage_AttachModule;

typedef struct {
    NvRmMsg       msg;
    NvError       error;
#if NV_LEGACY_AVP_KERNEL == 1
#else
    NvU32         libraryId;
#endif
} NvRmMessage_AttachModuleResponse;

typedef struct {
    NvRmMsg       msg;
    NvU32         reason;
#if NV_LEGACY_AVP_KERNEL == 1
    NvU32         entryAddress;
#else
    NvU32         libraryId;
#endif
} NvRmMessage_DetachModule;

typedef struct {
    NvRmMsg       msg;
    NvError       error;
} NvRmMessage_DetachModuleResponse;

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif
