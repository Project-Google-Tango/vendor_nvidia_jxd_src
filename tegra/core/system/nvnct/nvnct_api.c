/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
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


#define USE_CRC32_IN_NCT    1

NvOsMutexHandle g_hMutexNct = NULL;

static NvBool NctErrOut = NV_FALSE;

NvError NvNctInitMutex(void)
{
    NvError err = NvSuccess;

    if (!g_hMutexNct)
        err = NvOsMutexCreate(&g_hMutexNct);

    return err;
}

NvError NvNctReadItem(NvU32 index, nct_item_type *buf)
{
    NvError err;
    NvStorMgrFileHandle file;
    nct_entry_type entry;
    NvU32 crc = 0;
    NvU32 bytes;

    if (NV_TRUE != NvNctIsInit())
    {
        if (NctErrOut != NV_TRUE)
        {
            NvOsDebugPrintf("%s: Nct partition is not initialized \n",
                __func__);
            NctErrOut = NV_TRUE;
        }
        return NvError_NotInitialized;
    }

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_READ, &file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for nct partition (e:0x%x)\n",
            __func__, err);
        return  NvError_FileOperationFailed;
    }

    err = NvStorMgrFileSeek(file, NCT_ENTRY_OFFSET +
            (index * sizeof(nct_entry_type)), NvOsSeek_Set);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: seek error (e:0x%x)\n", __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_FileOperationFailed;
    }

    NvOsMutexLock(g_hMutexNct);
    err = NvStorMgrFileRead(file, &entry, sizeof(nct_entry_type), &bytes);
    NvOsMutexUnlock(g_hMutexNct);
    if (bytes != sizeof(nct_entry_type) || err != NvSuccess)
    {
        NvOsDebugPrintf("%s: read failed (e:0x%x)\n", __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_FileReadFailed;
    }

#if USE_CRC32_IN_NCT
    // For an entry with unpopulated data in nct, checksum will be zero, so
    // discarding NvComputeCrc32 for that entry and returning.
    if(entry.checksum == 0)
    {
       (void)NvStorMgrFileClose(file);
       NvOsMemset(buf, 0x0, sizeof(nct_item_type));
       return NvError_InvalidState;
    }
    crc = NvComputeCrc32(0, (NvU8 *)&entry,
              sizeof(nct_entry_type)-sizeof(entry.checksum));
    if (crc != entry.checksum)
    {
        NvOsDebugPrintf("%s: checksum(%x/%x)\n", __func__,
            crc, entry.checksum);
        (void)NvStorMgrFileClose(file);
        return NvError_InvalidState;
    }
#endif
    if (index != entry.index)
    {
        NvOsDebugPrintf("%s: index(%x/%x)\n", __func__,
            index, entry.index);
        (void)NvStorMgrFileClose(file);
        return NvError_InvalidState;
    }

    (void)NvStorMgrFileClose(file);
    NvOsMemcpy(buf, &entry.data, sizeof(nct_item_type));

    return NvSuccess;
}

NvError NvNctWriteItem(NvU32 index, nct_item_type *buf)
{
    NvError err;
    NvStorMgrFileHandle file;
    nct_entry_type entry;
    NvU32 crc = 0;
    NvU32 bytes;

    if (NV_TRUE != NvNctIsInit())
        return NvError_NotInitialized;

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_WRITE, &file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for nct partition (e:0x%x)\n",
            __func__, err);
        return  NvError_FileOperationFailed;
    }

    err = NvStorMgrFileSeek(file, NCT_ENTRY_OFFSET +
            (index * sizeof(nct_entry_type)), NvOsSeek_Set);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: seek error (e:0x%x)\n", __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_FileOperationFailed;
    }

    NvOsMemset(&entry, 0, sizeof(entry));
    entry.index = index;
    NvOsMemcpy(&entry.data, buf, sizeof(nct_item_type));
#if USE_CRC32_IN_NCT
    crc = NvComputeCrc32(0, (NvU8 *)&entry,
                        sizeof(nct_entry_type)-sizeof(entry.checksum));
    entry.checksum = crc;
#else
    entry.checksum = 0;
#endif

    NvOsMutexLock(g_hMutexNct);
    err = NvStorMgrFileWrite(file, &entry, sizeof(nct_entry_type), &bytes);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: write failed (e:0x%x)\n", __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_FileWriteFailed;
    }
    err = NvNctUpdateRevision(entry.index);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: part header update failed (e:0x%x)\n",
                        __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_FileWriteFailed;
    }
    NvOsMutexUnlock(g_hMutexNct);

    (void)NvStorMgrFileClose(file);

    return NvSuccess;
}

NvU32 NvNctGetItemType( NvU32 index )
{
    switch( index )
    {
        case NCT_ID_SERIAL_NUMBER:
            return 0x80; // String
        case NCT_ID_WIFI_MAC_ADDR:
        case NCT_ID_BT_ADDR:
            return 0x1A; // 1byte data array
        case NCT_ID_CM_ID:
        case NCT_ID_LBH_ID:
            return 0x20; // 2byte data
        case NCT_ID_FACTORY_MODE:
        case NCT_ID_RAMDUMP:
            return 0x40; // 4byte data
        default:
            return 0xFFFF;
    }
}
