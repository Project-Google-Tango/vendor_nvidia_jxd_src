/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "mnand_hs.h"
#include "asm-generic/ioctl.h"

#define MMC_IOC_CMD             _IOWR(179, 0, mmc_ioc_cmd)
#define MAX_BAD_BLOCKS_PER_DIE  250
#define NUM_DIES                16
#define NUM_BAD_BLOCK_TABLES    4
#define NUM_BLOCKS              0x10000
    /* Notice: If NUM_BLOCKS becomes different from 64K, then the
     * code needs to be carefully revisited */

typedef mmcsd_bad_block_info_element bad_blocks[MAX_BAD_BLOCKS_PER_DIE];
typedef struct _block_info {
    unsigned short erase_count;
    unsigned char block_type;
} block_info;

typedef struct _health_status_info {
    char manf_name[16];
    char prv[8];
    uint16_t bad_block_entries;
    bad_blocks bad_block_info[NUM_DIES];
    block_info block_info_table[NUM_BLOCKS];
} health_status_info;

#define BE2LE_16(a) (((a) >> 8) | ((a) << 8))
#define ARRSIZE(a) (sizeof(a) / sizeof((a)[0]))

void extract_bad_block_data(health_status_info *info, int die_nbr,
    MMCSD_GEN_CMD_DEVCTL_DATA *data);

void extract_age_data(health_status_info *info, MMCSD_GEN_CMD_DEVCTL_DATA *data);

void extract_type_data(health_status_info *info, MMCSD_GEN_CMD_DEVCTL_DATA *data);

void dump_per_block_info(health_status_info *info, int block_type);

void dump_info(health_status_info *info);

int usage(void);

void extract_bad_block_data(health_status_info *info, int die_nbr,
    MMCSD_GEN_CMD_DEVCTL_DATA *data)
{
    unsigned int i;
    mmcsd_bad_block_info_element *p;
    mmcsd_bad_block_info_element *pCur;

    if (info->bad_block_entries >= MAX_BAD_BLOCKS_PER_DIE)
        return; /* No more space for bad blocks on this die */
    for (i = 0; i < ARRSIZE(data->bb_info); i++) {
        p = &data->bb_info[i];
        if (!p->block_no && !p->page_no && !p->info && !p->bus)
            return;

        /* Copy valid bad block info */
        pCur = &info->bad_block_info[die_nbr][info->bad_block_entries++];
        *pCur = *p;

        /* Adjust for endianness */
        pCur->block_no = BE2LE_16(pCur->block_no);
        pCur->page_no = BE2LE_16(pCur->page_no);
        if (info->bad_block_entries == MAX_BAD_BLOCKS_PER_DIE)
            return; /* No more space for bad blocks on this die */
    }
}

void extract_age_data(health_status_info *info, MMCSD_GEN_CMD_DEVCTL_DATA *data)
{
    unsigned int i;
    unsigned short cur_block;
    unsigned short cur_block_age;

    for (i = 0; i < ARRSIZE(data->age_info.block_no); i++) {
        /* Physical block number */
        cur_block = BE2LE_16(data->age_info.block_no[i]);
        cur_block_age = BE2LE_16(data->age_info.erase_count[i]);
        if (cur_block && cur_block_age) {
            /* Age information for current block */
            if (info->block_info_table[cur_block].erase_count != 0)
                printf("Re-setting age for block %d\n", cur_block);
            info->block_info_table[cur_block].erase_count = cur_block_age;
        }
    }
}

void extract_type_data(health_status_info *info, MMCSD_GEN_CMD_DEVCTL_DATA *data)
{
    unsigned int i;
    unsigned short cur_block;
    unsigned short cur_block_type;

    for (i = 0; i < ARRSIZE(data->type_info.block_no); i++) {
        /* Physical block number */
        cur_block = BE2LE_16(data->type_info.block_no[i]);
        if (cur_block) {
            /* Type information for current block */
            cur_block_type = BE2LE_16(data->type_info.block_type[i]);
            info->block_info_table[cur_block].block_type = cur_block_type;
        }
    }
}

void dump_per_block_info(health_status_info *info, int block_type)
{
    int i;
    int cnt;
    uint16_t age_count[NUM_BLOCKS];

    memset(age_count, 0, sizeof(age_count));
    for (i = 0, cnt = 0; i < NUM_BLOCKS; i++) {
        if (info->block_info_table[i].block_type == block_type
            && info->block_info_table[i].erase_count != 0) {
            printf("%04X:%04d ", i, info->block_info_table[i].erase_count);
            cnt++;
            if ((cnt % 8) == 0)
                printf("\n");
            age_count[info->block_info_table[i].erase_count]++;
        }
    }
    printf("\n\nSummary:\n");
    for (i = 0; i < NUM_BLOCKS; i++)
        if (age_count[i])
            printf("%d %s block(s) with %d erase cycles.\n",
                age_count[i],
                block_type == MMCSD_BLOCK_TYPE_MLC ? "mlc" : "slc",
                i);
}

void dump_info(health_status_info *info)
{
    int i;
    int j;

    printf("Manufacturer's Name: %s\n", info->manf_name);

    printf("Product Revision: %s\n", info->prv);

    printf("BAD BLOCK DATA\n\n");
    for (i = 0; i < NUM_DIES; i++) {
        for (j = 0; j < MAX_BAD_BLOCKS_PER_DIE; j++) {
            if (info->bad_block_info[i][j].block_no) {
                printf("Bad Block at die %d, block %#x\n", i,
                    info->bad_block_info[i][j].block_no);
            }
        }
    }

    printf("PER-BLOCK DATA [MLC blocks only]\n\n");
    printf("Format: <Block Nbr>: <erase count>\n");
    dump_per_block_info(info, MMCSD_BLOCK_TYPE_MLC);

    printf("\nPER-BLOCK DATA [SLC blocks only]\n\n");
    printf("Format: <Block Nbr>: <erase count>\n");
    dump_per_block_info(info, MMCSD_BLOCK_TYPE_SLC);

    printf("\n");
}

int usage(void)
{
    printf("\nusage: ./mnand_hs -d <path to mnand device>\n\n"
           "   -d <mnand_path>   Path to mnand device (for instance, /dev/mmcblk3)\n\n");
    return 1;
}

int main(int argc, char **argv)
{
    int fd;
    int fd_temp;
    MMCSD_GEN_CMD_DEVCTL_DATA data;
    int i;
    int res;
    int block_age_table_size = 0;
    int die_nbr;
    int tbl_nbr;
    int opt;
    char errmsg_prepend[40];
    char device_stat[128];
    char file_name[128];
    char devnode[40] = "\0";
    health_status_info info;
    char buf[512];
    mmc_ioc_cmd *mic;
    struct stat st;
    bool dev = false;

    while ((opt = getopt(argc, argv, "d:h")) != -1) {
        switch (opt) {
            case 'd':
                strcpy(devnode, optarg);
                dev = true;
                break;
            case 'h':
            default:
                usage();
                return -1;
        }
    }

    if (!dev) {
        printf("\nError: please provide mnand device node via -d option\n");
        usage();
        return -1;
    }

    printf("\nmNAND Health and Status Info\n\n");

    memset(&info, 0, sizeof(info));
    fd = open(devnode, O_RDONLY);
    if (fd <= 0) {
        printf("Failed to access node. errno %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    /* Manufacturer's name and Product revision */
    for (i = 0; i <= 3; i++) {
        sprintf(device_stat, "/sys/devices/platform/sdhci-tegra.3/mmc_host/"
            "mmc%d/mmc%d:0001/", i, i);
        if (stat(device_stat, &st) == 0)
            break;
    }

    if (i > 3) {
        printf("Error: Can't find sys interfaces for name and prv of "
            "tegra-sdhci.3\n");
        close(fd);
        return -1;
    } else {
        sprintf(file_name, "%s%s", device_stat, "name");
        fd_temp = open(file_name, O_RDONLY);
        if (fd_temp < 0) {
            printf("Error: Can't open file:%s\n", file_name);
            close(fd);
            return -1;
        } else {
            if (-1 == read(fd_temp, info.manf_name, sizeof(info.manf_name))) {
                printf("Error: Can't read from file:%s\n", file_name);
                close(fd_temp);
                close(fd);
                return -1;
            }
            close(fd_temp);
        }

        sprintf(file_name, "%s%s", device_stat, "prv");
        fd_temp = open(file_name, O_RDONLY);
        if (fd_temp < 0) {
            printf("Error: Can't open file:%s\n", file_name);
            close(fd);
            return -1;
        } else {
            if (-1 == read(fd_temp, info.prv, sizeof(info.prv))) {
                printf("Error: Can't read from file:%s\n", file_name);
                close(fd_temp);
                close(fd);
                return -1;
            }
            close(fd_temp);
        }
    }

    mic = calloc(1, sizeof(mmc_ioc_cmd));
    mic->data_ptr = (uint32_t ) &buf[0];
    mic->opcode = 56;
    mic->blksz = 512;
    mic->blocks = 1;
    /* Retrieve bad block data for each die */
    for (die_nbr = 0; die_nbr < NUM_DIES; die_nbr++) {
        /* For each die, scan up to 4 tables */
        for (tbl_nbr = 0; tbl_nbr < NUM_BAD_BLOCK_TABLES; tbl_nbr++) {
            sprintf(errmsg_prepend, "Die %d, Table %d: ", die_nbr, tbl_nbr);
            mic->arg = DCMD_MMCSD_GEN_BB_INFO_ARG(die_nbr, tbl_nbr);
            res = ioctl(fd, MMC_IOC_CMD, mic);
            if (res != 0)
                break;
            for (i = 0; i < 64; i++) {
                memcpy(&data.bb_info[i].block_no, &buf[8*i], 2);
                memcpy(&data.bb_info[i].page_no, &buf[8*i+2], 2);
                data.bb_info[i].info = buf[8*i+4];
                data.bb_info[i].bus = buf[8*i+5];
            }
            extract_bad_block_data(&info, die_nbr, &data);
        }
    }

    /* Age */
    mic->arg = DCMD_MMCSD_GEN_AGE_TBL_SIZE_ARG;
    if (ioctl(fd, MMC_IOC_CMD, mic) == 0) {
        data.age_table_count = buf[0];
        block_age_table_size = data.age_table_count;
        for (i = 0; i < block_age_table_size; i++) {
            mic->arg = DCMD_MMCSD_GEN_GET_AGE_TBL_ARG(i);
            res = ioctl(fd, MMC_IOC_CMD, mic);
            if (res == 0) {
                memcpy(&data.age_info, buf, 512);
                extract_age_data(&info, &data);
            }
         }
    } else
        printf("res = %d, errstr %s\n", res, strerror(res));

    /* Block address type */
    for (i = 0; i < block_age_table_size; i++) {
        mic->arg = DCMD_MMCSD_GEN_GET_BLKTYPE_TBL_ARG(i);
        res = ioctl(fd, MMC_IOC_CMD, mic);
        if (res == 0) {
            memcpy(&data.type_info, buf, 512);
            extract_type_data(&info, &data);
        }
    }

    /* Dump it all */
    dump_info(&info);

    close(fd);

    return 0;
}
