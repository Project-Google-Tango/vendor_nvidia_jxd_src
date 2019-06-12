/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __MNAND_HS_H_INCLUDED
#define __MNAND_HS_H_INCLUDED

typedef struct _mmcsd_bad_block_info_element {
    uint16_t block_no;
    uint16_t page_no;
    uint8_t info;
        #define MMCSD_BB_INFO_ERASE_FAILED_1    0x01
        #define MMCSD_BB_INFO_ERASE_FAILED_2    0x10
        #define MMCSD_BB_INFO_PGM_FAILED_1      0x02
        #define MMCSD_BB_INFO_PGM_FAILED_2      0x11
    uint8_t bus;
        #define MMCSD_BB_INFO_BUS_LOW    0x01
        #define MMCSD_BB_INFO_BUS_HIGH   0x02
    uint16_t rsvd;
} mmcsd_bad_block_info_element;

typedef mmcsd_bad_block_info_element mmcsd_bad_block_info[64];

typedef struct _mmcsd_age_info {
    uint16_t block_no[128];
    uint16_t erase_count[128];
} mmcsd_age_info;

typedef struct _mmcsd_type_info {
    uint16_t block_no[128];
    uint16_t block_type[128];
        #define MMCSD_BLOCK_TYPE_MLC    0
        #define MMCSD_BLOCK_TYPE_SLC    1
} mmcsd_type_info;

typedef struct _mmcsd_gen_cmd_data {
    uint32_t argument;
    uint32_t response;
    union {
        uint8_t raw_data[512];
        uint8_t age_table_count;
        mmcsd_bad_block_info bb_info;
        mmcsd_age_info age_info;
        mmcsd_type_info type_info;
    };
} MMCSD_GEN_CMD_DEVCTL_DATA;

typedef struct _mmc_ioc_cmd {
    /* Implies direction of data.  true = write, false = read */
    int write_flag;

    /* Application-specific command.  true = precede with CMD55 */
    int is_acmd;

    uint32_t opcode;
    uint32_t arg;
    uint32_t response[4];  /* CMD response */
    unsigned int flags;
    unsigned int blksz;
    unsigned int blocks;

    /*
     * Sleep at least postsleep_min_us useconds, and at most
     * postsleep_max_us useconds *after* issuing command.  Needed for
     * some read commands for which cards have no other way of indicating
     * they're ready for the next command (i.e. there is no equivalent of
     * a "busy" indicator for read operations).
     */
    unsigned int postsleep_min_us;
    unsigned int postsleep_max_us;

    /*
     * Override driver-computed timeouts.  Note the difference in units!
     */
    unsigned int data_timeout_ns;
    unsigned int cmd_timeout_ms;

    /*
     * For 64-bit machines, the next member, ``__u64 data_ptr``, wants to
     * be 8-byte aligned.  Make sure this struct is the same size when
     * built for 32-bit.
     */
    uint32_t __pad;

    /* DAT buffer */
    uint32_t data_ptr;

    /* This is required just so that the size of this structure becomes same
     * as the corresponding structure in the kernel. This lets us generate the
     * same number for MMC_IOC_CMD as in the kernel.
     */
    uint32_t __pad2;
} mmc_ioc_cmd;

#define DCMD_MMCSD_GEN_BB_INFO_ARG(die_no, tbl_no)    \
            ((((die_no) & 0xff) << 8) | (((tbl_no) & 0xff) << 16) | 0x81)
#define DCMD_MMCSD_GEN_AGE_TBL_SIZE_ARG              0x83
#define DCMD_MMCSD_GEN_GET_AGE_TBL_ARG(tbl_no)       (((tbl_no) << 8) | 0x85)
#define DCMD_MMCSD_GEN_GET_BLKTYPE_TBL_ARG(tbl_no)   (((tbl_no) << 8) | 0x87)

#endif
