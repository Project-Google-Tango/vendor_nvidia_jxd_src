/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvos.h"
#include "nvcommon.h"
#include "nv3p.h"
#include "nvassert.h"
#include "nverror.h"
#include "runner.h"
#include "registerer.h"
#include "nvflash_version.h"
#include "nvrm_hardware_access.h"
#include "nvaboot.h"

/**
 * Max Data Transfer Supported
 */
#define NVBDK_STAGING_SIZE (64 * 1024)

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#ifndef PMC_PA_BASE
#define PMC_PA_BASE             0x7000E400  // Base address for arapbpm.h registers
#define PMC_SCRATCH0_REG_OFFSET 0x50  // offset for APBDEV_PMC_SCRATCH0_0
#endif

#define   RECOVERY_MODE         BIT(31)
#define   BOOTLOADER_MODE       BIT(30)
#define   FORCED_RECOVERY_MODE  BIT(1)

/**
 * Error handler macro
 *
 * Like NV_CHECK_ERROR_CLEANUP, except additionally assigns 'status' (an
 * Nv3pStatus code) to the local variable 's' if/when 'expr' fails
 */

#define NV_CHECK_ERROR_CLEANUP_3P(expr, status)                            \
    do                                                                     \
    {                                                                      \
        e = (expr);                                                        \
        if (e != NvSuccess)                                                \
        {                                                                  \
            s = status;                                                    \
            NvOsSnprintf(Message, NV3P_STRING_MAX, "nverror:0x%x (0x%x)", e & 0xFFFFF,e);     \
            goto fail;                                                     \
        }                                                                  \
    } while (0)

#define NV_CHECK_ERROR_GOTO_CLEAN_3P(expr, status)                            \
    do                                                                     \
    {                                                                      \
        e = (expr);                                                        \
        if (e != NvSuccess)                                                \
        {                                                                  \
            s = status;                                                    \
            goto clean;                                                    \
        }                                                                  \
    } while (0)

/**
     * Process BDKTest command
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

static COMMAND_HANDLER(RunBDKTest);
static COMMAND_HANDLER(GetNv3pServerVersion);
static COMMAND_HANDLER (GetSuitInfo);
static COMMAND_HANDLER(SetBDKTest);
static NvError BDKTestReset(Nv3pSocketHandle hSock, Nv3pCommand command, void *arg, NvAbootHandle hAboot);


static NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags);

NvError
NvBDKTestServer(NvAbootHandle hAboot);

NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags)
{
    NvError e;
    Nv3pCmdStatus CmdStatus;

    CmdStatus.Code = Status;
    CmdStatus.Flags = Flags;
    if (Message)
        NvOsStrncpy(CmdStatus.Message, Message, NV3P_STRING_MAX);
    else
        NvOsMemset(CmdStatus.Message, 0x0, NV3P_STRING_MAX);

    e = Nv3pCommandSend(hSock, Nv3pCommand_Status, (NvU8 *)&CmdStatus, 0);

    return e;
}

COMMAND_HANDLER(RunBDKTest)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdRunBDKTest *a = (Nv3pCmdRunBDKTest *)arg;
    NvU32 bytesLeftToTransfer;
    NvU32 transferSize;
    NvU8 *tempBuff = NULL;

    // Query the number of tests which need to be executed for the current test entry
    a->NumTests = NvBDKTest_GetNumTests(a->Suite, a->Argument);

    // Send that number to the host, so that host can wait for n number of test result logs
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, a, 0)
    );
    a->PrivData.File_buff = NULL;

    if (a->PrivData.File_size)
    {
        a->PrivData.File_buff = NvOsAlloc(a->PrivData.File_size);
        if (!a->PrivData.File_buff)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        tempBuff = a->PrivData.File_buff;
        bytesLeftToTransfer = a->PrivData.File_size;
        do
        {
            transferSize = (bytesLeftToTransfer > NVBDK_STAGING_SIZE) ?
                            NVBDK_STAGING_SIZE:
                            bytesLeftToTransfer;
            e = Nv3pDataReceive( hSock, tempBuff, transferSize, 0, 0);
            if (e)
                goto clean;
            tempBuff += transferSize;
            bytesLeftToTransfer -= transferSize;
        } while (bytesLeftToTransfer);
    }
    // If non zero tests are available for the test entry run the test plan
    if (a->NumTests)
        NV_CHECK_ERROR_GOTO_CLEAN_3P(
           NvBDKTest_RunInputTestPlan(a->Suite, a->Argument, a->PrivData, hSock),
           Nv3pStatus_BDKTestRunFailure
        );

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
    }
clean:
    if (e)
        NvOsDebugPrintf("\nRunBDKTest failed. NvError %u NvStatus %u\n", e, s);
    if (a->PrivData.File_buff)
        NvOsFree(a->PrivData.File_buff);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(GetNv3pServerVersion)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdGetNv3pServerVersion a;

    NvOsSnprintf(a.Version, sizeof(a.Version), "%s", NVFLASH_VERSION);
    NvOsDebugPrintf("Nv3pServer Version %s\n", NVFLASH_VERSION);

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, &a, 0)
    );

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nGetNv3pServerVersion failed. NvError %u NvStatus %u\n", e, s);
    }

    return ReportStatus(hSock, "", s, 0);
}

COMMAND_HANDLER (GetSuitInfo)
{

    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdGetSuitesInfo *a = (Nv3pCmdGetSuitesInfo *)arg;
    NvU8 *buffer = NULL;
    NvU32 size = sizeof(NvU32);

    buffer = NvOsAlloc(size);
    buffer = NvBDKTest_GetSuiteInfo(buffer, &size);
    a->size = size;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, a, 0)
    );
    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataSend(hSock, buffer, size, 0)
    );
fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nGetSuitName failed. NvError %u NvStatus %u\n", e, s);
    }
    if (buffer)
        NvOsFree(buffer);
    return ReportStatus(hSock, "", s, 0);

}

NvError BDKTestReset(Nv3pSocketHandle hSock, Nv3pCommand command, void *arg, NvAbootHandle hAboot)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU32 pmc_scratch0 = 0;
    Nv3pCmdReset *a = (Nv3pCmdReset *)arg;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, arg, 0)
    );

    NvOsSleepMS(a->DelayMs);

    switch (a->Reset) {
    case Nv3pResetType_RecoveryMode:
        pmc_scratch0 = NV_READ32(PMC_PA_BASE + PMC_SCRATCH0_REG_OFFSET)
                                | FORCED_RECOVERY_MODE;
        NV_WRITE32(PMC_PA_BASE + PMC_SCRATCH0_REG_OFFSET, pmc_scratch0);
        break;
    case Nv3pResetType_NormalBoot:
        break;
    default:
        break;
    }

fail:
    if (e)
    {
        s = Nv3pStatus_InvalidState;
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nBDKTestReset failed. NvError %u NvStatus %u\n",
                            e, s);
        return ReportStatus(hSock, "", s, 0);
    }

    e = ReportStatus(hSock, "", s, 0);
    NvAbootReset(hAboot);
    return e;
}

COMMAND_HANDLER(SetBDKTest)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdSetBDKTest *a = (Nv3pCmdSetBDKTest *)arg;

    NvBDKTest_SetBDKTestEnv(a->StopOnFail, a->Timeout);

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, &a, 0)
    );

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nGetNv3pServerVersion failed. "
                        "NvError %u NvStatus %u\n", e, s);
    }
    return ReportStatus(hSock, "", s, 0);
}

NvError
NvBDKTestServer(NvAbootHandle hAboot)
{
    NvError e;
    Nv3pSocketHandle hSock = 0;
    Nv3pCommand command;
    Nv3pCmdStatus cmd_stat;
    void *arg;
    NvBool bDone = NV_FALSE;

    // Call default registration function
    e = NvBDKTest_CreateDefaultRegistration();
    if (e != NvSuccess)
    {
        e = NvError_BDKTestCreateDefaultRegFailed;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(Nv3pOpen(&hSock, Nv3pTransportMode_default, 0));

    /* send an ok -- the nvflash application waits for the bootloader
     * (this code) to initialize.
     */
    command = Nv3pCommand_Status;
    cmd_stat.Code = Nv3pStatus_Ok;
    cmd_stat.Flags = 0;

    NvOsMemset(cmd_stat.Message, 0, NV3P_STRING_MAX );
    NV_CHECK_ERROR_CLEANUP(Nv3pCommandSend(hSock, command,
        (NvU8 *)&cmd_stat, 0));

    for( ;; )
    {
        NV_CHECK_ERROR_CLEANUP(Nv3pCommandReceive(hSock, &command, &arg, 0));
        switch(command) {
            case Nv3pCommand_Go:
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pCommandComplete(hSock, command, arg, 0)
                );
                NV_CHECK_ERROR_CLEANUP(
                    ReportStatus(hSock, "", Nv3pStatus_Ok, 0)
                );
                bDone = NV_TRUE;
                break;

            case Nv3pCommand_RunBDKTest:
                NV_CHECK_ERROR_CLEANUP(
                    RunBDKTest(hSock, command, arg)
                );
                break;

            case Nv3pCommand_GetNv3pServerVersion:
                NV_CHECK_ERROR_CLEANUP(
                    GetNv3pServerVersion(hSock, command, arg)
                );
                break;

            case Nv3pCommand_BDKGetSuiteInfo:
                NV_CHECK_ERROR_CLEANUP(
                    GetSuitInfo(hSock, command, arg)
                );
                break;
            case Nv3pCommand_SetBDKTest:
                NV_CHECK_ERROR_CLEANUP(
                    SetBDKTest(hSock, command, arg)
                );
                break;

            case Nv3pCommand_Reset:
                NV_CHECK_ERROR_CLEANUP(
                    BDKTestReset(hSock, command, arg, hAboot)
                );
                break;

            default:
                goto fail;
        }

        /* Break when go command was received, sync done
          * and partition verification is not pending.
          */
        if(bDone)
        {
            break;
        }

    }
    goto clean;

fail:
    if (hSock)
    {
        Nv3pNack( hSock, Nv3pNackCode_BadCommand );
        NV_CHECK_ERROR_CLEANUP(
           ReportStatus(hSock, "", Nv3pStatus_Unknown, 0)
        );
    }

clean:
    if (hSock)
        Nv3pClose(hSock);

    return e;
}

