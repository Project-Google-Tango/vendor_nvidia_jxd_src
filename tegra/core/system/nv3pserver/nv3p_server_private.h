/**
 * Copyright (c) 2009 - 2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV3P_SERVER_PRIVATE_H
#define INCLUDED_NV3P_SERVER_PRIVATE_H

#include "nvcommon.h"
#include "nvbct.h"
#include "nvbit.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Error handler macro
 *
 * Like NV_CHECK_ERROR_CLEANUP, except additionally assigns 'status' (an
 * Nv3pStatus code) to the local variable 's' if/when 'expr' fails
 */

#define NV_CHECK_ERROR_FAIL_3P(expr, status)                                   \
    do                                                                         \
    {                                                                          \
        e = (expr);                                                            \
        if (e != NvSuccess)                                                    \
        {                                                                      \
            s = status;                                                        \
            NvOsSnprintf(Message, NV3P_STRING_MAX, "nverror:0x%x (0x%x) %s %d",\
                e & 0xFFFFF, e, __FUNCTION__, __LINE__);                       \
            goto fail;                                                         \
        }                                                                      \
    } while (0)

#define NV_CHECK_ERROR_CLEAN_3P(expr, status)                                 \
    do                                                                        \
    {                                                                         \
         e = (expr);                                                          \
         if (e != NvSuccess)                                                  \
         {                                                                    \
            s = status;                                                       \
            NvOsSnprintf(Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__,    \
                __LINE__);                                                    \
            goto clean;                                                       \
         }                                                                    \
    } while (0)


/**
 * Process 3P command
 *
 * Execution of received 3P commands are carried out by the COMMAND_HANDLER's
 *
 * The command handler must handle all errors related to the request internally,
 * except for those that arise from low-level 3P communication and protocol
 * errors.  Only in this latter case is the command handler allowed to report a
 * return value other than NvSuccess.
 *
 * Thus, the return value does not indicate whether the command itself was
 * carried out successfully, but rather that the 3P communication link is still
 * operating properly.  The success status of the command is generally returned
 * to the 3P client by sending a Nv3pCommand_Status message.
 *
 * @param hSock pointer to the handle for the 3P socket state
 * @param command 3P command to execute
 * @param args pointer to command arguments
 *
 * @retval NvSuccess 3P communication link/protocol still valid
 * @retval NvError_* 3P communication link/protocol failure
 */
#define COMMAND_HANDLER(name)                   \
            NvError                                     \
            name(                                       \
                Nv3pSocketHandle hSock,                 \
                Nv3pCommand command,                    \
                void *arg                               \
                )

/**
* size of shared scratch buffer used when carrying out Nv3p operations
*
* Note that lower-level read operations currently only accept transfer sizes
* that will fit in an NvU32, so assert that staging buffer does not exceed
* this size; otherwise implementation of data buffering for partition reads
* and writes must change.
*/
#define NV3P_STAGING_SIZE (1024 * 1024)
#define NV3P_AES_HASH_BLOCK_LEN 16

/**
 * Report 3P command status to client
 *
 * Sends Nv3pCommand_Status message to client, reporting status of most-recent
 * 3P command.
 *
 * @param hSock pointer to the handle for the 3P socket state
 * @param Message string describing status
 * @param Status status code
 * @param Flags Reserved, must be zero.
 *
 * @retval NvSuccess 3P Status message sent successfully
 * @retval NvError_* Unable to send message
 */
NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags);

/**
 * Load partition table into memory. Needed to perform
 * FS operations.
 *
 *
 * @retval NvSuccess Successfully loaded partition table
 * @retval NvError_* Failed to load partition table
 */
NvError
LoadPartitionTable(Nv3pPartitionTableLayout *arg);


/**
 *For encrypting the d_session key generated on device using the public key
 */
COMMAND_HANDLER(Symkeygen);

/**
 *For decrypting the fuse data followed by fuse burning
 */
COMMAND_HANDLER(DSession_fuseBurn);


/**
 * State information for the NvFlash command processor
 */
typedef struct Nv3pServerStateRec
{
    /// NV_TRUE if device parameters are valid, else NV_FALSE
    NvBool IsValidDeviceInfo;

    /// device parameters from previous SetDevice operation
    NvRmModuleID DeviceId;
    NvU32 DeviceInstance;

    /// NV_TRUE if BCT was loaded from boot device, else NV_FALSE
    NvBool IsLocalBct;
    /// NV_TRUE if PT was loaded from boot device, else NV_FALSE
    NvBool IsLocalPt;
    /// NV_TRUE if a valid PT is available, else NV_FALSE
    NvBool IsValidPt;
    /// NV_TRUE if partition creation is in progress, else NV_FALSE
    NvBool IsCreatingPartitions;
    /// Number of partitions remaining to be created
    NvU32 NumPartitionsRemaining;

    /// Partition id's for specialty partitions (those that required special
    /// handling)
    NvU32 PtPartitionId;

    /// staging buffer
    /// * allocated on entry to 3p server
    /// * used as scratch buffer for all commands
    /// * freed on exit from 3p server
    NvU8 *pStaging;

    //Store the IRAM BCT so we can update
    //it with new info.
    NvBctHandle BctHandle;

    // Store a handle to the BIT; we'll need it for some boot loader update
    // operations.
    NvBitHandle BitHandle;

} Nv3pServerState;

typedef struct Nv3pServerUtilsHalRec
{
    NvError
    (*ConvertBootToRmDeviceType)(
        NvBootDevType bootDevId,
        NvDdkBlockDevMgrDeviceId *blockDevId);
    NvError
    (*Convert3pToRmDeviceType)(
        Nv3pDeviceType DevNv3p,
        NvRmModuleID *pDevNvRm);
    NvBool
    (*ValidateRmDevice)(
         NvBootDevType BootDevice,
         NvRmModuleID RmDeviceId);
    void
    (*UpdateBct)(
        NvU8* data,
        NvU32 BctSection);
    void
    (*UpdateCustDataBct)(
        NvU8* data);
} Nv3pServerUtilsHal;

NvBool Nv3pServerUtilsGetT30Hal(Nv3pServerUtilsHal * pHal);
NvBool Nv3pServerUtilsGetT1xxHal(Nv3pServerUtilsHal * pHal);
NvBool Nv3pServerUtilsGetT12xHal(Nv3pServerUtilsHal * pHal);

#ifdef __cplusplus
}
#endif

#endif
