/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nverror.h"
#include "fastboot.h"
#include "nvaboot.h"
#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvboot_bit.h"
#include "nvddk_blockdevmgr.h"
#include "nvassert.h"
#include "nvpartmgr.h"
#include "nvstormgr.h"
#include "crc32.h"
#include "nct.h"


typedef struct
{
    NvU32 NckBase;
    NvU32 NckSize;
    NvU32 PartId;
    NvU32 PartSize;
    NvU32 PartCRC;
    NvU32 BytesPerSector;
    NvPartInfo PartInfo;
} nct_info_type;

nct_info_type NctInfo;

static NvBool NvNctInitialized = NV_FALSE;
static nct_part_head_type NctHead;


NvU32 NvNctSetNCKCarveout(NvAbootHandle hAboot)
{
    NvU64 Base, Size;

    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Nck,
        &Base, &Size);
    NvOsDebugPrintf("NCK: Base(0x%x),Size(0x%x)\n",
        (NvU32)Base, (NvU32)Size);
    NctInfo.NckBase = (NvU32)Base;
    NctInfo.NckSize = (NvU32)Size;

    if (Size)
        return (NvU32)Base;
    else
    {
        NvOsDebugPrintf("NCK: Not Supported!\n");
        return 0;
    }
}

NvError NvNctCheckCRC(NvU32 baseAddr, NvU32 validLen)
{
    NvU32 ReadChunk = (1 << 24);
    NvError err = NvSuccess;
    NvU32 len = 0;
    NvU32 size;
    NvU8 *buf = NULL, *addr = (NvU8 *)baseAddr;
    NvU32 crc = 0, crc_nck;

    buf = NvOsAlloc(NV_MIN(ReadChunk, validLen));
    if (!buf)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    crc = 0;
    size = validLen;
    while (size)
    {
        len = (NvU32)NV_MIN(ReadChunk, size);
        NvOsMemcpy(buf, addr, len);
        crc = NvComputeCrc32(crc, buf, len);
        size -= len;
        addr += len;
    }
    NctInfo.PartCRC = crc;
    NvOsDebugPrintf("NCT: CRC:0x%08x (SZ:0x%x)\n", __func__, crc, validLen);

fail:
    if (buf)
        NvOsFree(buf);
    return err;
}

NvError NvNctCopyNCTtoNCK(NvU32 baseAddr, NvU32 validLen)
{
    NvU32 ReadChunk = (1 << 24);
    NvError err = NvSuccess;
    NvU32 len = 0, bytes = 0;
    NvU32 size;
    NvU8 *buf = NULL;
    NvStorMgrFileHandle rfile = NULL;

    if (validLen >= NctInfo.NckSize)
    {
        err = NvError_BadParameter;
        goto fail;
    }

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_READ, &rfile);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for NCT partition (e:0x%x)\n",
            __func__, err);
        err = NvError_FileOperationFailed;
        goto fail;
    }

    size = validLen;
    buf = (NvU8 *)baseAddr;
    while (size)
    {
        len = (NvU32)NV_MIN(ReadChunk, size);
        err = NvStorMgrFileRead(rfile, buf, len, &bytes);
        if (err != NvSuccess)
        {
            NvOsDebugPrintf("%s: failed to open for NCT partition (e:0x%x)\n",
                __func__, err);
            err = NvError_FileReadFailed;
            goto fail;
        }
        size -= len;
        buf += len;
    }

fail:
    if (rfile)
        NvStorMgrFileClose(rfile);
    return err;
}

NvBool NvNctIsInit(void)
{
    return NvNctInitialized;
}

NvError NvNctInitPart(void)
{
    NvError err, result;
    NvU32 bytes = 0;
    NvStorMgrFileHandle file;
    NvU32 partId;
    NvPartInfo partInfo;
    NvFsMountInfo minfo;
    NvDdkBlockDevInfo dinfo;
    NvDdkBlockDevHandle hDev = 0;

    if (NV_TRUE == NvNctInitialized)
        return    NvSuccess;

    err = NvPartMgrGetIdByName(NCT_PART_NAME, &partId);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to get id for NCT (e:0x%x)\n",
                        __func__, err);
        return  err;
    }
    err = NvPartMgrGetPartInfo(partId, &partInfo);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to get info for NCT (e:0x%x)\n",
                        __func__, err);
        return  err;
    }
    err = NvPartMgrGetFsInfo(partId, &minfo);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to get FsInfo for NCT (e:0x%x)\n",
                        __func__, err);
        return  err;
    }
    err = NvDdkBlockDevMgrDeviceOpen((NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open BlkDev for NCT (e:0x%x)\n",
                        __func__, err);
        return  err;
    }
    hDev->NvDdkBlockDevGetDeviceInfo(hDev, &dinfo);
    hDev->NvDdkBlockDevClose(hDev);

    NctInfo.PartId = partId;
    NvOsMemcpy(&NctInfo.PartInfo, &partInfo, sizeof(partInfo));
    NctInfo.BytesPerSector = dinfo.BytesPerSector;
    NctInfo.PartSize = (NvU32)partInfo.NumLogicalSectors *
                        NctInfo.BytesPerSector;
    NvOsDebugPrintf("NCT: PartId(%d),Size(0x%x)\n",
                    NctInfo.PartId, NctInfo.PartSize);

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_READ, &file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for NCT (e:0x%x)\n",
                        __func__, err);
        return  NvError_ResourceError;
    }

    err = NvStorMgrFileRead(file, &NctHead, sizeof(NctHead), &bytes);
    (void)NvStorMgrFileClose(file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: part header read failed (e:0x%x)\n",
                        __func__, err);
        return  NvError_ResourceError;
    }

    NvNctInitMutex();

    NvOsDebugPrintf("%s: Magic(0x%x),VID(0x%x),PID(0x%x),VER(%x.%x),REV(0x%x)\n",
        __func__,
        NctHead.magicId,
        NctHead.vendorId,
        NctHead.productId,
        (NctHead.version >> 16) & 0xFFFF,
        NctHead.version & 0xFFFF,
        NctHead.revision);

    if (NvOsMemcmp(&NctHead.magicId, NCT_MAGIC_ID, NCT_MAGIC_ID_LEN))
    {
        NvOsDebugPrintf("%s: magic ID error (0x%x/0x%x:%s)\n", __func__,
            NctHead.magicId, NCT_MAGIC_ID, NCT_MAGIC_ID);

        // build default NCT partition
        NvNctBuildDefaultPart(&NctHead);
    }

    NvNctInitialized = NV_TRUE;
    NvOsDebugPrintf("%s: Done\n", __func__);

    return NvSuccess;
}

NvError NvNctInitNCKPart(NvAbootHandle hAboot)
{
    NvU32 base, size, bytes = 0;

    if (NvNctInitialized != NV_TRUE)
        return NvError_NotInitialized;

    base = NvNctSetNCKCarveout(hAboot);
    if (!base)
    {
        return  NvError_ResourceError;
    }

    size = NCT_ENTRY_OFFSET + NCT_ID_END * sizeof(nct_entry_type);
    NvNctCopyNCTtoNCK(base, size);
    NvNctCheckCRC(base, size);

    return NvSuccess;
}

NvError NvNctUpdatePartHeader(nct_part_head_type *Header)
{
    NvStorMgrFileHandle file;
    nct_part_head_type head;
    NvU32 bytes = 0;
    NvError err;

    if (NvNctInitialized != NV_TRUE)
        return NvError_NotInitialized;

    NvOsMemcpy(&head, Header, sizeof(NctHead));

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_WRITE, &file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for NCT (e:0x%x)\n",
                        __func__, err);
        return    NvError_FileOperationFailed;
    }

    err = NvStorMgrFileWrite(file, &head, sizeof(head), &bytes);
    (void)NvStorMgrFileClose(file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to write part header (e:0x%x)\n",
                        __func__, err);
        return    NvError_FileWriteFailed;
    }

    return NvSuccess;
}

NvError NvNctUpdateRevision(NvU32 revision)
{
    NvStorMgrFileHandle file;
    nct_part_head_type head;
    NvU32 bytes = 0;
    NvError err;

    if (NvNctInitialized != NV_TRUE)
        return NvError_NotInitialized;

    if (revision > NctHead.revision)
    {
        NvOsMemcpy(&head, &NctHead, sizeof(NctHead));
        head.revision = revision;

        err = NvNctUpdatePartHeader(&head);
        if (err != NvSuccess)
        {
            NvOsDebugPrintf("%s: failed to update NCT part header (e:0x%x)\n",
                            __func__, err);
            return    err;
        }
    }

    return NvSuccess;
}
