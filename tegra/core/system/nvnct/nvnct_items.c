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


/*
 * Default values for NCT items :
 * may be different in every product,
 * however, below items are common for everyone.
 */
static nct_entry_type NctEntry[] = {
    { NCT_ID_SERIAL_NUMBER, {0,},
        .data.serial_number.sn="TEGRA00000000A123456", }, /* 20 characters */
    { NCT_ID_WIFI_MAC_ADDR, {0,},
        .data.u8={0x00, 0x04, 0x4B, 0xAC, 0x12, 0x34,}, },
    { NCT_ID_BT_ADDR, {0,},
        .data.u8={0x00, 0x04, 0x4B, 0xBD, 0x12, 0x34,}, },
    { NCT_ID_CM_ID, {0,},
        .data.cm_id.id=0x0000, },
    { NCT_ID_LBH_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_FACTORY_MODE, {0,},
        .data.u32={0,}, },
    { NCT_ID_RAMDUMP, {0,},
        .data.u32={0,}, },
    { NCT_ID_TEST, {0,},
        .data.u8={'T','E','S','T',}, },
    { NCT_ID_BOARD_INFO, {0,},
        .data.board_info =
        {
            .proc_board_id=0,
            .proc_sku=0,
            .proc_fab=0,
            .pmu_board_id=0,
            .pmu_sku=0,
            .pmu_fab=0,
            .display_board_id=0,
            .display_sku=0,
            .display_fab=0
        }, },
    { NCT_ID_GPS_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_LCD_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_ACCELEROMETER_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_COMPASS_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_GYROSCOPE_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_LIGHT_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_CHARGER_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_TOUCH_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_FUELGAUGE_ID, {0,},
        .data.lbh_id.id=0x0000, },
    { NCT_ID_DEBUG_CONSOLE_PORT_ID, {0,},
        .data.debug_port.port_id=0x0000, },
    { NCT_ID_BATTERY_MAKE, {0,},
        .data.battery_make.make="", }, /* 20 characters */
    { NCT_ID_BATTERY_COUNT, {0,},
        .data.battery_count.count=0x0000, },
    { NCT_ID_DDR_TYPE, {0,},
        .data.ddr_type.ddr_type_data =
        {
            {
                .mrr5 = 0,
                .mrr6 = -1,
                .mrr7 = -1,
                .mrr8 = -1,
                .index = 0,
            },
            {
                .mrr5 = 0,
                .mrr6 = -1,
                .mrr7 = -1,
                .mrr8 = -1,
                .index = 0,
            },
        },
    },
};

NvError NvNctBuildDefaultPart(nct_part_head_type *Header)
{
    NvError err;
    NvStorMgrFileHandle file;
    nct_part_head_type head;
    NvU32 bytes;
    NvU32 i, entryCount, crc = 0;
    nct_entry_type *entry;

    /* make header */
    NvOsMemset(&head, 0, sizeof(head));
    NvOsMemcpy(&head.magicId, NCT_MAGIC_ID, NCT_MAGIC_ID_LEN);
    head.vendorId = NCT_VENDOR_ID;
    head.productId = NCT_PRODUCT_ID;
    head.version = NCT_FORMAT_VERSION;
    head.revision = NCT_ID_END;

    /* Return default header info */
    if (Header)
        NvOsMemcpy(Header, &head, sizeof(head));

    err = NvStorMgrFileOpen(NCT_PART_NAME, NVOS_OPEN_WRITE, &file);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to open for NCT (e:0x%x)\n",
                        __func__, err);
        return  NvError_ResourceError;
    }

    /* Write default partition header */
    err = NvStorMgrFileWrite(file, &head, sizeof(head), &bytes);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: part header write failed (e:0x%x)\n",
                        __func__, err);
        (void)NvStorMgrFileClose(file);
        return  NvError_ResourceError;
    }

    /* Compute crc32 of data */
    entryCount = sizeof(NctEntry)/sizeof(nct_entry_type);
    NvOsDebugPrintf("%s: entryCount(%d)\n", __func__, entryCount);
    for (i=0; i<entryCount; i++)
    {
        entry = &NctEntry[i];
        crc = NvComputeCrc32(0, (NvU8 *)entry,
                            sizeof(nct_entry_type)-sizeof(entry->checksum));
        entry->checksum = crc;
        NvOsDebugPrintf("%s: cnt:%d, idx:%d, crc:0x%08x\n",
                        __func__, i, entry->index, entry->checksum);
    }

    /* Write default backup data */
    for (i=0; i<entryCount; i++)
    {
        entry = &NctEntry[i];

        err = NvStorMgrFileSeek(file, NCT_ENTRY_OFFSET +
                (entry->index * sizeof(nct_entry_type)), NvOsSeek_Set);
        if (err != NvSuccess)
        {
            NvOsDebugPrintf("%s: seek error (e:0x%x)\n", __func__, err);
            (void)NvStorMgrFileClose(file);
            return  NvError_ResourceError;
        }

        err = NvStorMgrFileWrite(file, entry, sizeof(nct_entry_type), &bytes);
        if (err != NvSuccess)
        {
            NvOsDebugPrintf("%s: part header write failed (e:0x%x)\n",
                            __func__, err);
            (void)NvStorMgrFileClose(file);
            return  NvError_ResourceError;
        }
    }

    NvOsDebugPrintf("%s: Done\n", __func__);
    (void)NvStorMgrFileClose(file);

    return NvSuccess;
}

