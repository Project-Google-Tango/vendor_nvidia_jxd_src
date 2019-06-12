/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nverror.h"
#include "nv3p.h"
#include "nvos.h"
#include "nvcommon.h"
#include "nvrm_drf.h"
#include "arapbpm.h"
#include "nvgetdtb_platform.h"
#include "nvddk_avp.h"

#define COMMAND_HANDLER(name)                   \
    static NvError                              \
    name(                                       \
        Nv3pSocketHandle hSock,                 \
        Nv3pCommand command,                    \
        void *arg                               \
        )

#define NV_READ32(a)     *((volatile NvU32 *)(a))
#define NV_WRITE32(a,d)  *((volatile NvU32 *)(a)) = (d)

static NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags)
{
    NvError e = NvSuccess;
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

#define NV_CHECK_ERROR_FAIL_3P(expr, status)                                 \
    do                                                                       \
    {                                                                        \
        e = (expr);                                                          \
        if (e != NvSuccess)                                                  \
        {                                                                    \
            Status.Code= status;                                             \
            NvOsSnprintf(                                                    \
                       Message, NV3P_STRING_MAX, "nverror:0x%x (0x%x) %s %d",\
                e & 0xFFFFF, e, __FUNCTION__, __LINE__);                     \
            goto fail;                                                       \
        }                                                                    \
    } while (0)

#define NV_CHECK_ERROR_CLEAN_3P(expr, status)                               \
    do                                                                      \
    {                                                                       \
        e = (expr);                                                         \
        if (e != NvSuccess)                                                 \
        {                                                                   \
            NvOsSnprintf(                                                   \
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);\
                    Status.Code = status;                                   \
            goto clean;                                                     \
        }                                                                   \
    } while (0)

#define VERIFY(exp, code) \
    if(!(exp)) { \
        code; \
    }

COMMAND_HANDLER(ReadBoardId)
{
    NvGetDtbBoardInfo BoardInfo[NvGetDtbBoardType_MaxNumBoards];
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU32 i = 0;

    Status.Code = Nv3pStatus_Ok;

    NvOsMemset((NvU8 *)&BoardInfo, 0x0, sizeof(BoardInfo));

    NvGetdtbInitI2cPinmux();
    NvGetdtbEepromPowerRail();
    NvGetdtbInitGPIOExpander();
    NvGetdtbEnablePowerRail();

    for (i = 0; i < NvGetDtbBoardType_MaxNumBoards; i++)
        NvGetdtbReadBoardInfo((NvGetDtbBoardType )i, &BoardInfo[i]);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, (NvU8 *)&BoardInfo, sizeof(BoardInfo), 0),
        Nv3pStatus_DataTransferFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(Recovery)
{
    NvU32 RegVal;
    NvU32 PMC_PA_BASE = 0x7000E400;
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    NV_CHECK_ERROR_CLEANUP(
        ReportStatus(hSock, Message, Status.Code, 0));

    Nv3pClose(hSock);

    RegVal = NV_READ32(PMC_PA_BASE + APBDEV_PMC_SCRATCH0_0);
    RegVal = RegVal | (1 << 1);

    NV_WRITE32(PMC_PA_BASE + APBDEV_PMC_SCRATCH0_0, RegVal);

    RegVal = NV_DRF_DEF(APBDEV_PMC, CNTRL, MAIN_RST, ENABLE);
    NV_WRITE32(PMC_PA_BASE, RegVal);

fail:
    return NvError_BadParameter;
}

static void server(void)
{
    NvError e;
    Nv3pSocketHandle hSock = 0;
    Nv3pCommand Command;
    void *arg;

    e = Nv3pOpen(&hSock, Nv3pTransportMode_default, 0);
    VERIFY(e == NvSuccess, goto clean);

    for( ;; )
    {
        e = Nv3pCommandReceive(hSock, &Command, &arg, 0);
        VERIFY(e == NvSuccess, goto clean);

        switch(Command)
        {
        case Nv3pCommand_Recovery:
            e = Recovery(hSock, Command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_ReadBoardInfo:
            e = ReadBoardId(hSock, Command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        default:
            Nv3pNack(hSock, Nv3pNackCode_BadCommand);
            break;
        }
    }
clean:
    Nv3pClose(hSock);
}

void NvOsAvpShellThread(int argc, char **argv)
{
    server();
}
