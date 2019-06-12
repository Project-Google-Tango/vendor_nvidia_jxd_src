/*
 * Copyright (c) 2006 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_fuse.h
 *
 * Defines the parameters and data structures related to fuses.
 */

#ifndef INCLUDED_NVBOOT_FUSE_H
#define INCLUDED_NVBOOT_FUSE_H

#include "nvcommon.h"
#if defined(__cplusplus)
extern "C"
{
#endif

#define NVBOOT_DEVICE_KEY_BYTES (4)

typedef struct NvBootECIDRec
{
    NvU32   ECID_0;
    NvU32   ECID_1;
    NvU32   ECID_2;
    NvU32   ECID_3;
} NvBootECID;

//DRF defines for ECID fields.

//Bootrom is defining 128 bits i.e. 4 x 32 bits, out of which
// a)100 bits make ECID for non RCM modes
// b)128 bits make ECID for RCM mode

//<Lot1:2><Wafer:6><X:9><Y:9><Reserved:6>
#define                 ECID_ECID0_0_RSVD1_RANGE        5:0
#define                 ECID_ECID0_0_Y_RANGE            14:6
#define                 ECID_ECID0_0_X_RANGE            23:15
#define                 ECID_ECID0_0_WAFER_RANGE        29:24
#define                 ECID_ECID0_0_LOT1_RANGE         31:30
//<Lot0:6><Lot1:26>
#define                 ECID_ECID1_0_LOT1_RANGE         25:0
#define                 ECID_ECID1_0_LOT0_RANGE         31:26
//<Fab:6><Lot0:26>
#define                 ECID_ECID2_0_LOT0_RANGE         25:0
#define                 ECID_ECID2_0_FAB_RANGE          31:26
//<operating mode:4><rcm version:16><Reserved:8><Vendor:4>
#define                 ECID_ECID3_0_VENDOR_RANGE       3:0
#define                 ECID_ECID3_0_RSVD2_RANGE        11:4
#define                 ECID_ECID3_0_RCM_VERSION_RANGE  27:12
#define                 ECID_ECID3_0_OPMODE_RANGE       31:28   // Valid for RCM mode only

/*
 * NvBootFuseBootDevice -- Peripheral device where Boot Loader is stored
 *
 * This enum *MUST* match the equivalent list in nvboot_devmgr.h for
 * all valid values and None, Undefined not present in nvboot_devmgr.h
 */
typedef enum
{
    NvBootFuseBootDevice_Sdmmc,
    NvBootFuseBootDevice_SnorFlash,
    NvBootFuseBootDevice_SpiFlash,
    NvBootFuseBootDevice_resvd_3,
    NvBootFuseBootDevice_resvd_4,
    NvBootFuseBootDevice_resvd_5,
    NvBootFuseBootDevice_Max, /* Must appear after the last legal item */
    NvBootFuseBootDevice_Force32 = 0x7fffffff
} NvBootFuseBootDevice;

/*
 * Definitions of device fuse fields
 */

/**
 * SDMMC configuration fuses.
 */

/**
 * Fuse Bit 0: indicates the data bus width.
 *      = 0 means the data bus width is 4 lines.
 *      = 1 means the data bus width is 8 lines.
 */
#define SDMMC_DEVICE_CONFIG_0_DATA_WIDTH_RANGE 0:0
/**
 * Fuse Bit 1: indicates the ddr mode to select.
 *      = 0 means, Normal mode.
 *      = 1 means, DDR mode.
 */
#define SDMMC_DEVICE_CONFIG_0_DDR_MODE_RANGE 1:1
/**
 * Fuse Bits 2,3: indicates the voltage range to use.
 *      = 00 means query voltage range from the card.
 *      = 01 means use high voltage range.
 *      = 10 means use dual voltage range.
 *      = 11 means use low voltage range.
 */
#define SDMMC_DEVICE_CONFIG_0_VOLTAGE_RANGE_RANGE 3:2
/**
 * Fuse Bit 4: indicates the Boot mode support.
 *      = 0 means the Boot mode is enabled.
 *      = 1 means the Boot mode is disabled.
 */
#define SDMMC_DEVICE_CONFIG_0_DISABLE_BOOT_MODE_RANGE 4:4
/**
 * Fuse Bit 5: SDMMC4 pads voltage
 *      = 0 = 1.8v
 *      = 1 - 1.2v
 */
#define SDMMC_DEVICE_CONFIG_0_SDMMC4_PADS_VOLTAGE_RANGE 5:5
/**
 * Fuse Bit 6,7,8: SDMMC4 Clock divider
 *  Option to control the clock divider by subtracting the value from fuses.
 *      = 0, means default clock divider 11
 *      = 1, means default clock divider 10
 *      = 2, means default clock divider 9
 *      = 3, means default clock divider 8
 *      = 4, means default clock divider 7
 *      = 5, means default clock divider 6
 *      = 6, means default clock divider 5
 *      = 7, means default clock divider 4
 *      = 8, means default clock divider 3
 *      = 9, means default clock divider 2
 */
#define SDMMC_DEVICE_CONFIG_0_SDMMC4_CLOCK_DIVIDER_RANGE 9:6
/**
 * Fuse Bit 9: SDMMC4 read MultiPage support
 *      = 0, Single Page     , 512Bytes size
 *      = 1, Multipage 2 page, 1K size
 *      = 2, Multipage 4 page, 2K size
 *      = 3, Multipage 8 page, 4K size
 *      = 4, Multipage 16 page, 8K size
 *      = 5, Multipage 32 page, 16K size
 */
#define SDMMC_DEVICE_CONFIG_0_SDMMC4_MULTI_PAGE_READ_RANGE 12:10

/**
 * NAND configuration fuses.
 */

/**
 * Bits 0,1,2 --> Used for ECC Selection Values 000, 001, 010, 011, 100, 101
 * indicate Discovery, Bch4, Bch8, Bch16, Bch24 and Ecc Off respectively.
 */
#define NAND_DEVICE_CONFIG_0_ECC_SELECT_RANGE 2:0
/**
 * Bits 3,4 --> Used for Data width selection. Values 00, 01, 10, and 11 indicate
 *      Discovery, 8-bit, 16-bit and rsvd respectively.
 * Use Discovery option only for >42nm Nand parts (ex: 52nm Nand)
 */
#define NAND_DEVICE_CONFIG_0_DATA_WIDTH_RANGE 4:3
/**
 * Bit 5    --> 0 indicates Onfi is Enabled and 1 indicated Onfi is disabled.
 */
#define NAND_DEVICE_CONFIG_0_DISABLE_ONFI_RANGE 5:5
/**
 * Bit 6 --> Used to correct Page/Block Size received from read id info.
 *      Values 0 and 1 indicate offsets 0 and 1 respectively.
 *      This offset will be added to  page/block size value, which is in log2 scale.
 *      So, Adding offset 1 makes page/block size to get multiplied by 2^1.
 */
#define NAND_DEVICE_CONFIG_0_PAGE_BLOCK_SIZE_OFFSET_RANGE 6:6
/**
 * Bits 7,8 --> Used for selecting the Main ECC Offset
 *       Values 0,1,2,3 indicate 4, 8, 12 and 16 bytes respectively.
 */
#define NAND_DEVICE_CONFIG_0_MAIN_ECC_OFFSET_RANGE 8:7
/**
 * Bit 9 --> Used for selecting the sector size for ECC computation.
 * 0 indicates 512 bytes and 1 indicates 1024 bytes
 */
#define NAND_DEVICE_CONFIG_0_BCH_SECTOR_SIZE_RANGE 9:9

/**
 * Bit 10,11 --> Used for selecting the EF Nand controller version
 * Values 00,01,10 and 11 indicate Disable EF, EF Nand 1.0, EF Nand 2.0 and EF Nand 3.0 respectively.
 */
#define NAND_DEVICE_CONFIG_0_EF_NAND_CTRL_VER_RANGE 11:10
/**
 * Bit 12, 13 --> Used for selecting the Toggle DDR mode
 * Values 00, 01, 10, and 11 indicate Discovery, Disable, Enable & rsvd respectively.
 * Use Discovery option only for 4x/3x/2x nm Nand parts.
 */
#define NAND_DEVICE_CONFIG_0_TOGGLE_DDR_RANGE 13:12


/**
 * SNOR configuration fuses.
 */

/**
 * Bit 0 --> 1 for the 32 bit mode, 0 for the 16 bit mode.
 */
#define SNOR_DEVICE_CONFIG_0_ENABLE_32BIT_RANGE 0:0


/**
 * SPI configuration fuses.
 */

/**
 * Bit 0 --> 0 = 2k page size, 1 = 16k page size
 */
#define SPI_DEVICE_CONFIG_0_PAGE_SIZE_2KOR16K_RANGE 0:0

/**
 * USBH configuration fuses.
 */

/**
 * Bit 0,1,2,3 -->  used for representing the root port number device is attached to the
 * usb host controller.
 */
#define USBH_DEVICE_CONFIG_0_ROOT_PORT_RANGE 3:0

/**
 * Bit 4 --> 0 = 2k page size, 1 = 16k page size
 */
#define USBH_DEVICE_CONFIG_0_PAGE_SIZE_2KOR16K_RANGE 4:4

/**
 * Bit 5,6,7--->  used to represent to program the OC pin or OC group this port.
 */
#define USBH_DEVICE_CONFIG_0_OC_PIN_RANGE 7:5

/**
 * Bit 8 --> 0 = pad 0, 1 = pad 1, used to represent tri-state of the associated vbus pad.
 */
#define USBH_DEVICE_CONFIG_0_VBUS_ENABLE_RANGE 8:8

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_FUSE_H
