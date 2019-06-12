/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#ifndef _NVAVP_H
#define _NVAVP_H

#include "nvcommon.h"
#include "nvrm_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NVAVP_UEVENT_MSG_TAG "change@/devices/platform/host1x/nvavp"

typedef void* NvAvpHandle;

/**
 * Relocation structure that allows address patching in the kernel rather than
 * several traps to update from user space. - ref. nvrm_channel.h
 */
typedef struct NvAvpRelocationEntryRec
{
    // The submitted command buffer
    NvRmMemHandle hCmdBufMem;

    // The offset into the command buffer for the relocation
    NvU32 CmdBufOffset;

    // The memory that will be pinned and patched into the command buffer

    NvRmMemHandle hMemTarget;

    // The offset within hMemTarget for the memory address patching
    NvU32 TargetOffset;
} NvAvpRelocationEntry;

/**
 * Structure describing a command buffer being submitted to AVP channel.
 */
typedef struct NvAvpCommandBufferRec
{
    /* The handle of the memory allocation in which the commands live */
    NvRmMemHandle hMem;
    /* The byte offset where the commands start within that memory. */
    /* Must be a multiple of 4. */
    NvU32 Offset;
    /* The number of 32-bit words in the command buffer. */
    NvU32 Words;
} NvAvpCommandBuffer;

/**
 * Returns a handle to the AVP struct for Audio channel
 *
 * @param phAvp Pointer to NvAvpHandle
 *
 * @retval NvError_ResourceError indicates that the AVP device could
           not be opened
 * @retval NvError_BadParameter indicates a invalid handle
 * @retval NvSuccess Indicates the AVP was successfully opened
 */

NvError NvAvpAudioOpen(NvAvpHandle *phAvp);

/**
 * Enables Audio clocks (bsea/vcp)
 *
 * @param hAvp Handle from NvAvpAudioOpen
 * @param ModuleId Module Id to enable clock
 *
 * @retval NvError_BadParameter indicates a invalid handle
 * @retval NvSuccess
 */

NvError NvAvpAudioEnableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId);

/**
 * Disables Audio clocks (bsea/vcp)
 *
 * @param hAvp Handle from NvAvpAudioOpen
 * @param ModuleId Module Id to disable clock
 *
 * @retval NvError_BadParameter indicates a invalid handle
 * @retval NvSuccess
 */

NvError NvAvpAudioDisableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId);

/**
 * Returns a handle to the AVP struct for Video channel
 *
 * @param phAvp Pointer to NvAvpHandle
 *
 * @retval NvError_ResourceError indicates that the AVP device could
           not be opened
 * @retval NvError_BadParameter indicates a invalid handle
 * @retval NvSuccess Indicates the AVP was successfully opened
 */
NvError NvAvpOpen(NvAvpHandle *phAvp);

/**
 * Closes an AVP handle
 * API will never fail.
 *
 * @param hAvp The AVP handle to close.
 *
 * @retval NvSuccess
 */
NvError NvAvpClose(NvAvpHandle hAvp);

/* AVP client submit flags */
#define NVAVP_CLIENT_NONE      0x00000000
#define NVAVP_CLIENT_UCODE_EXT 0x00000001 /*use external ucode provided */

/**
 * Submits a command buffer to the AVP channel.  Command buffers will
 * be executed in the order received, uninterrupted by other blocks of commands
 * in the same channel.
 *
 * The command buffer memory needs to be pinned by the client.
 *
 * Passing a non-NULL fence param (pPbFence) will insert a sync point increment
 * command into the buffer. The syncpoint ID and value to wait on will be returned
 * through the fence.
 *
 * @param hAvp Handle from NvAvpOpen
 * @param pCommandBuf Structure describing the command buffer
 * @param pRelocations Array of relocation entries
 * @param NumRelocations Number of relocations for this submit
 * @param pPbFence If non-NULL, will insert a syncpoint increment to the cmd buffer.
 *     Will return the syncpoint ID and value the client should wait on.
 * @param flags additional actions to perform with current submit call.
 *
 * @retval NvError_NotInitialized Indicates that the AVP has not been initialized
 * @retval NvSuccess Indicates that the cmd buf has been successfully submitted to
 *     AVP
 */
NvError NvAvpSubmitBuffer(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence
);

NvError NvAvpSubmitBufferNew(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence,
    NvU32 flags
);

/**
 * Query the AVP syncpoint ID
 *
 * @param hAvp The AVP handle to close.
 *
 * @param pSyncPointID Pointer to variable which holds the return value.
 *
 * @retval NvSuccess Indicates that the query has been successfull.
 */
NvError NvAvpGetSyncPointID(
    NvAvpHandle hAvp,
    NvU32 *pSyncPointID);

/**
 * Query the Clock rate
 *
 * @param hAvp Handle from NvAvpOpen.
 *
 * @param ModuleId Module Id to query the rate.
 *
 * @param pRate Pointer to variable which holds the queried rate.
 *
 * @retval NvSuccess Indicates that the query has been successfull.
 */
NvError NvAvpGetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 *pRate);

/**
 * Set clock rate
 *
 * @param hAvp Handle from NvAvpOpen.
 *
 * @param ModuleId Module Id to set rate.
 *
 * @param Rate New rate value to update.
 *
 * @param pFreqSet Actual frequency value updated.
 *
 * @retval NvSuccess Indicates that the set has been successfull.
 */
NvError NvAvpSetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 Rate,
    NvU32 *pFreqSet);

/**
 * Wake AVP
 *
 * @param hAvp Handle from NvAvpOpen.
 *
 * @retval NvSuccess Indicates that the request has been successfull.
 */
NvError NvAvpWakeAvp(
    NvAvpHandle hAvp);

/**
 * Enum client makes AVP clock stay on or not
 */
enum
{
    NVAVP_CLIENT_CLOCK_STAY_ON_DISABLED = 0,
    NVAVP_CLIENT_CLOCK_STAY_ON_ENABLED
};

/**
 * Set clock state
 *
 * @param hAvp Handle from NvAvpOpen.
 *
 * @param flag clock state to be set.
 *
 * @retval NvSuccess Indicates that the set has been successfull.
 */
NvError NvAvpForceClockStayOn(
    NvAvpHandle hAvp,
    NvU32 flag);

/**
 * Set minimum number of online CPUs
 *
 * @param hAvp Handle from NvAvpOpen.
 *
 * @param num Minimum number of CPUs to keep online.
 *
 * @retval NvSuccess Indicates that the set has been successfull.
 */
NvError NvAvpSetMinOnlineCpus(
    NvAvpHandle hAvp,
    NvU32 num);

/**
 * Map the Memory
 *
 * @param hAvp Handle from NvAvpOpen
 *
 * @param MemHandle Memory handle
 *
 * @param MapAddr Address returned
 *
 * @retval NvSuccess Indicates that memory mapping has been successful
 */
NvError NvAvpMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 *MapAddr);

/**
 * UnMap the Memory
 *
 * @param hAvp Handle from NvAvpOpen
 *
 * @param MemHandle Memory handle
 *
 * @param MapAddr Address to unmap
 *
 * @retval NvSuccess Indicates that memory unmapping has been successful
 */
NvError NvAvpUnMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 MapAddr);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVAVP_H */

