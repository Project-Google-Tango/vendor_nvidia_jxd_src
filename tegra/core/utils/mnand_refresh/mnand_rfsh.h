/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/*
 *  mnand_rfsh.h   Definitions for mNAND Refresh tool.
 */

#ifndef __MNAND_RFSH_H_INCLUDED
#define __MNAND_RFSH_H_INCLUDED

#define MNAND_TOTAL_BLOCKS      1200.0    /* 4 GB mNAND with 4 MB erase size or
                                             8 GB mNAND with 8 MB erase size */
#define MNAND_DAYS_TO_REFRESH   180
#define MNAND_SECS_TO_REFRESH   (MNAND_DAYS_TO_REFRESH * 24 * 3600)
#define MNAND_DESIRED_REFRESH_SPEED \
    ((MNAND_TOTAL_BLOCKS) / (MNAND_SECS_TO_REFRESH))
#define MAX_LINES_IN_FILE       250
#define DEFAULT_REFRESH_COUNT   (MNAND_TOTAL_BLOCKS / MNAND_DAYS_TO_REFRESH)
#define MNAND_RFSH_FINISHED     "/tmp/mnand_rfsh.finished"

/***************************************
 * Management of mNAND data extraction *
 ***************************************/
#define NUM_BLOCKS              0x10000
    /* Notice: If NUM_BLOCKS becomes different from 64K, then the
     * code needs to be carefully revisited */

typedef struct _mmcsd_age_info {
    uint16_t block_no[128];
    uint16_t erase_count[128];
} mmcsd_age_info;

typedef struct _mmcsd_type_info {
    uint16_t block_no[128];
    uint16_t block_type[128];
        #define MMCSD_BLOCK_TYPE_MLC 0
        #define MMCSD_BLOCK_TYPE_SLC 1
} mmcsd_type_info;

typedef struct _mmcsd_resfreshed_block_info {
    uint16_t block_no[256];
} mmcsd_refreshed_block_info;

typedef struct _mmcsd_gen_cmd_data {
    uint32_t argument;
    uint32_t response;
    union {
        uint8_t raw_data[512];
        uint8_t age_table_count;
        mmcsd_age_info age_info;
        mmcsd_type_info type_info;
        mmcsd_refreshed_block_info refreshed_block_info;
    };
} MMCSD_SCSI_PASSTHRU_DATA;

typedef struct _block_info {
    unsigned short erase_count;
    unsigned char block_type;
} block_info;

typedef struct _health_status_info {
    block_info block_info_table[NUM_BLOCKS];
    uint16_t last_refreshed_block;
} health_status_info;

typedef struct _info_node {
    struct _info_node *next;
    int index;                  /* simple node count */
    time_t date_time;
    uint64_t age;
} info_node;

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

/* MMCSD passtrough commands used */
#define MMC_SWITCH             6
#define MMC_SEND_EXT_CSD       8
#define MMC_GEN_CMD            56
#define MMC_SEND_STATUS        13

/* CMD56 defines */
#define DCMD_MMCSD_INDEX_BB_INFO              0x40
#define DCMD_MMCSD_INDEX_AGE_TBL_SIZE         0x41
#define DCMD_MMCSD_INDEX_AGE_RETRIEVE         0x42
#define DCMD_MMCSD_INDEX_ADDR_TYPE            0x43
#define DCMD_MMCSD_INDEX_AGE_SUMMARY_INFO     0x10
#define DCMD_MMCSD_INDEX_BAD_SPARE            0x08
#define DCMD_MMCSD_INDEX_REFRESH_BLOCK        0x44

#define DCMD_MMCSD_RD_ARGS(index, arg1, arg2) \
            (((arg1) & 0xff) << 8 | \
             ((arg2) & 0xff) << 16 | \
             ((index) << 1) | \
             0x01)

#define DCMD_MMCSD_RD(index)            DCMD_MMCSD_RD_ARGS((index), 0, 0)

/* CMD56 argument definitions */
#define DCMD_MMCSD_GEN_AGE_TBL_SIZE_ARG \
            DCMD_MMCSD_RD(DCMD_MMCSD_INDEX_AGE_TBL_SIZE)

#define DCMD_MMCSD_GEN_GET_AGE_TBL_ARG(tbl_no) \
            DCMD_MMCSD_RD_ARGS(DCMD_MMCSD_INDEX_AGE_RETRIEVE, (tbl_no), 0)

#define DCMD_MMCSD_GEN_GET_BLKTYPE_TBL_ARG(tbl_no) \
            DCMD_MMCSD_RD_ARGS(DCMD_MMCSD_INDEX_ADDR_TYPE, (tbl_no), 0)

#define DCMD_MMCSD_GEN_REFRESH_BLOCKS(count) \
            DCMD_MMCSD_RD_ARGS(DCMD_MMCSD_INDEX_REFRESH_BLOCK, (count), 0)

void extract_age_data(health_status_info *info, MMCSD_SCSI_PASSTHRU_DATA *data);
void extract_type_data(health_status_info *info, MMCSD_SCSI_PASSTHRU_DATA *data);
int extract_total_age(int fd, int block_type, uint64_t *total_age, int *blk_count);
int refresh_one_block(int fd, uint16_t *last_refreshed_block);
int extract_str_value(char *s, unsigned int start, unsigned int len);
int str_to_time(char *s, time_t *t);
unsigned int add_chars(char *s, int shift_start);
unsigned int calc_line_checksum(char *line);
int parse_line(char *line, info_node *node);
void free_info(info_node **head);
int count_info(info_node **head);
int same_day(time_t date_time1, time_t date_time2);
int add_info(info_node **head, time_t date_time, uint64_t age, int squash);
int load_info(char *file_name, info_node **head);
int node_to_str(info_node *node, char *p, int len);
int save_info(char *file_name, info_node **head, int max_entries);
void dump_info(info_node **head);
int calculate_blocks_to_refresh(info_node **head);
int usage(void);
void sig_handler(int sig_num);

#endif
