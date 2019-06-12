/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#ifndef NvV32
#define NvV32 NvU32
#endif
#include "nvavp.h"
#include "tvmr_sim.h"
#include "cle276.h"
#include "cle26e.h"

NvError NvAvpAudioOpen(NvAvpHandle *phAvp)
{
    return NvSuccess;
}

NvError NvAvpAudioEnableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    return NvSuccess;
}

NvError NvAvpAudioDisableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    return NvSuccess;
}

NvError NvAvpOpen(NvAvpHandle *phAvp)
{
    return NvSuccess;
}

NvError NvAvpClose(NvAvpHandle hAvp)
{
    return NvSuccess;
}

NvError NvAvpSubmitBuffer(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence
)
{
    return NvSuccess;
}

NvError NvAvpSubmitBufferNew(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence,
    NvU32 flags
)
{
    NvU32 AppMethodData[512];   //This is similar to NvE276Control AppMethodData
    NvU32 i;
    NvU32 *CommandBufferPtr = (NvU32 *)pCommandBuf->hMem;
    NvU32 method = 0, data;
    NvU32 Methods = pCommandBuf->Words;
    NvU32 op=0, count=0, app_id = 0;
    NvOsMemset(&AppMethodData, 0, 512*4);
    // get physical address for all reloc buffers
    for (i = 0; i < NumRelocations; i++)
    {
        CommandBufferPtr[(pRelocations[i].CmdBufOffset>>2)] = NvRmMemGetAddress(pRelocations[i].hMemTarget, pRelocations[i].TargetOffset);
    }
    i = 0;
    do
    {
        data = CommandBufferPtr[i];
        if (!count)
        {
            op = data >> 28;
            switch(op)
            {
                case NVE26E_CMD_METHOD_OPCODE_INCR:
                case NVE26E_CMD_METHOD_OPCODE_NONINCR:
                    count = data & 0xFFFF;
                    method = (data >> 16) & 0x0FFF;
                    break;
                //case NVE26E_CMD_METHOD_OPCODE_MASK:
                case NVE26E_CMD_METHOD_OPCODE_IMM:
                    //avpProcessMethod(ctx, (data >> 16) & 0xFFF, data & 0x0000FFFF);
                    if (method == NVE276_EXECUTE)
                        break;
                    AppMethodData[method - NVE276_PARAMETER_METHOD(0)] = data;
                    break;
                case NVE26E_CMD_METHOD_OPCODE_GATHER:
                    method = data;
                    count = 1;
                    break;
                //case NVE26E_CMD_METHOD_OPCODE_EXTEND:
                //case NVE26E_CMD_METHOD_OPCODE_CHDONE:
                case NVE26E_CMD_METHOD_OPCODE_SETCL:
                    if (data == 0x40)   // Ignore SetClass(Host1X)
                        break;
                    // Otherwise fall through to unsupported path
                default:
                    NvOsDebugPrintf("Host1x opcode %d is not implemented (0x%08X)\n", op, data);
            }
        }
        else
        {
            if (op == NVE26E_CMD_METHOD_OPCODE_GATHER)
            {
                //avpGatherMethods(ctx, (NvU32 *)data, method);
                //TVMR is not using any gather odcode.
            }
            else
            {
                //avpProcessMethod(ctx, method, data);
                if (method == NVE276_EXECUTE)
                    break;
                AppMethodData[method - NVE276_PARAMETER_METHOD(0)] = data;
            }
            if (op == NVE26E_CMD_METHOD_OPCODE_INCR)
            {
                method++;
            }
            count--;
        }
        i++;
        if (method == NVE276_EXECUTE)
            break;
    }while (i < Methods);
    // get app id
    data = CommandBufferPtr[i];
    app_id = data & 0xff;
    VideoCodecApp (app_id, AppMethodData);
    return NvSuccess;
}

NvError NvAvpGetSyncPointID(
    NvAvpHandle hAvp,
    NvU32 *pSyncPointID)
{
    return NvSuccess;
}

NvError NvAvpGetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 *pRate)
{
    return NvSuccess;
}

NvError NvAvpSetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 Rate,
    NvU32 *pFreqSet)
{
    return NvSuccess;
}

NvError NvAvpWakeAvp(
    NvAvpHandle hAvp)
{
    return NvSuccess;
}

NvError NvAvpForceClockStayOn(
    NvAvpHandle hAvp,
    NvU32 flag)
{
    return NvSuccess;
}

NvError NvAvpSetMinOnlineCpus(
    NvAvpHandle hAvp,
    NvU32 num)
{
    return NvSuccess;
}

NvError NvAvpMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 *MapAddr)
{
    return NvSuccess;
}

NvError NvAvpUnMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 MapAddr)
{
    return NvSuccess;
}
