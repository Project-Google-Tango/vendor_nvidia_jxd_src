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

#if defined(__cplusplus)
extern "C"
{
#endif

#define NVBOOT_DEVICE_KEY_BYTES (4)


/*
 * NvBootFuseBootDevice -- Peripheral device where Boot Loader is stored
 *
 * AP15 A02 split the original NAND encoding in NAND x8 and NAND x16.
 * For backwards compatibility the original enum was maintained and 
 * is implicitly x8 and aliased with the explicit x8 enum.
 *
 * This enum *MUST* match the equivalent list in nvboot_devmgr.h for
 * all valid values and None, Undefined not present in nvboot_devmgr.h
 */
typedef enum
{
    NvBootFuseBootDevice_Sdmmc,
    NvBootFuseBootDevice_SnorFlash,
    NvBootFuseBootDevice_SpiFlash,
    NvBootFuseBootDevice_NandFlash,
    NvBootFuseBootDevice_NandFlash_x8  = NvBootFuseBootDevice_NandFlash,
    NvBootFuseBootDevice_NandFlash_x16 = NvBootFuseBootDevice_NandFlash,
    NvBootFuseBootDevice_MobileLbaNand,
    NvBootFuseBootDevice_MuxOneNand,
    NvBootFuseBootDevice_Sata,
    NvBootFuseBootDevice_BootRom_Reserved_Sdmmc3,   /* !!! this enum is strictly used by BootRom code only !!! */
    NvBootFuseBootDevice_Max, /* Must appear after the last legal item */
    NvBootFuseBootDevice_Force32 = 0x7fffffff
} NvBootFuseBootDevice;

/*
 * Definitions of device fuse fields
 */

/**
 * SATA configuration fuses
 */

/**
 * Bit 0: Selection of clock source for PLLE.
 *     0 = Use oscillator if 12MHz, otherwise divided PLLP
 *     1 = Always use divided PLLP
 */
#define SATA_DEVICE_CONFIG_0_PLLE_SOURCE_RANGE 0:0

/**
 * Bit 1: Spread Spectrum enable.
 *     0 = Do not enable spread spectrum
 *     1 = Enable spread spectrum
 */
#define SATA_DEVICE_CONFIG_0_SPREAD_SPECTRUM_RANGE 1:1

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
 * Fuse Bit 1: indicates the inserted card type.
 *      = 0 means the card is EMMC card.
 *      = 1 means the card is SD card.
 */
#define SDMMC_DEVICE_CONFIG_0_CARD_TYPE_RANGE 1:1
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
 * Fuse Bit 5: indicates the pinmux to select.
 *      = 0 means, use primary pinmux.
 *      = 1 means, use secondary pinmux.
 */
#define SDMMC_DEVICE_CONFIG_0_PINMUX_SELECTION_RANGE 5:5

/**
 * Fuse Bit 6: indicates the ddr mode to select.
 *      = 0 means, Normal mode.
 *      = 1 means, DDR mode.
 */
#define SDMMC_DEVICE_CONFIG_0_DDR_MODE_RANGE 6:6

/**
 * Fuse Bit 7: indicates the controller to select.
 *      = 0 means, sdmmc4
 *      = 1 means, sdmmc3.
 */
#define SDMMC_DEVICE_CONFIG_0_CNTRL_SELECTION_RANGE 7:7

/**
 * Fuse Bit 8: SDMMC4 pads voltage
 *      = 0 = 1.8v
 *      = 1 - 1.2v
 */
#define SDMMC_DEVICE_CONFIG_0_SDMMC4_PADS_VOLTAGE_RANGE 8:8

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
 * Bit 6 --> Used to correct Page Size received from read id info. 
 *      Values 0 and 1 indicate offsets 0 and 1 respectively. 
 *      This offset will be added to  page size value, which is in log2 scale. 
 *      So, Adding offset 1 makes page size to get multiplied by 2^1.
 */
#define NAND_DEVICE_CONFIG_0_PAGE_SIZE_OFFSET_RANGE 6:6
/**
 * Bit 7  --> Used to correct Block Size received from read id info. 
 *      Values 0 and 1 indicate offsets 0 and 1 respectively. 
 *      This offset will be added to  block size value, which is in log2 scale. 
 *      So, Adding offset 1 makes block size to get multiplied by 2^1.
 */
#define NAND_DEVICE_CONFIG_0_BLOCK_SIZE_OFFSET_RANGE 7:7
/**
 * Bits 8,9 --> Used for selecting the Main ECC Offset
 *       Values 0,1,2,3 indicate 4, 8, 12 and 16 bytes respectively.
 */
#define NAND_DEVICE_CONFIG_0_MAIN_ECC_OFFSET_RANGE 9:8
/**
 * Bit 10 --> Used for selecting the sector size for ECC computation.
 * 0 indicates 512 bytes and 1 indicates 1024 bytes
 */
#define NAND_DEVICE_CONFIG_0_BCH_SECTOR_SIZE_RANGE 10:10

/**
 * Bit 11 --> 1 indicates Error Free Nand is enabled and 0 indicates Error Free Nand is disabled.
 */
#define NAND_DEVICE_CONFIG_0_ENABLE_EF_NAND_RANGE 11:11
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
 * Bit 1 --> 0 for the muxed mode, 1 for the non-muxed mode.
 */
#define SNOR_DEVICE_CONFIG_0_ENABLE_NONMUX_RANGE 1:1

/**
 * MUX_ONE_NAND configuration fuses.
 */

/**
 * Bit 0 --> used for slecting the MUX_ONE_NAND type
 * 0 for selecting the  flex mux one nand and 1 for selecting the mux one nand
 */
#define MUXONENAND_DEVICE_CONFIG_0_DISABLE_FLEX_TYPE_RANGE 0:0
/**
 * Bit 1 --> used for selecting the interface type
 * 0 forselecting Muxed interface and 1 for selecting the NonMuxed interface
 */
#define MUXONENAND_DEVICE_CONFIG_0_DISABLE_MUX_INTERFACE_RANGE 1:1
/**
 * Bits 2,3 --> Used for page size selection. Values 00, 01, 10, and 11 indicate
 *      4K, 2K, 8K and rsvd respectively.
 */
#define MUXONENAND_DEVICE_CONFIG_0_PAGE_SIZE_RANGE 3:2
/**
 * Bits 4,5,6 --> Used for block size selection. Values 000, 001, 010, 011, 100, 101, 110 and 111
 * indicate  128K, 32K, 64K, 16K, 256K, 512K, 1024K and 2048K respectively.
 */
#define MUXONENAND_DEVICE_CONFIG_0_BLOCK_SIZE_RANGE 6:4


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_FUSE_H
