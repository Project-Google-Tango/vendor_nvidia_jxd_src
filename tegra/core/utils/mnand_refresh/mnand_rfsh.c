/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "mnand_rfsh.h"
#include "asm-generic/ioctl.h"

#define MMC_IOC_CMD             _IOWR(179, 0, mmc_ioc_cmd)

int s_verbosity = 0;

#define VERBOSE_PRINTF(level, ...) \
    do { \
        if (s_verbosity >= level) \
            printf(__VA_ARGS__); \
    } while (0)

#define BE2LE_16(a) (((a) >> 8) | ((a) << 8))
#define ARRSIZE(a) (sizeof(a) / sizeof((a)[0]))

void extract_age_data(health_status_info *info, MMCSD_SCSI_PASSTHRU_DATA *data)
{
    unsigned int i;
    unsigned short cur_block;
    unsigned short cur_block_age;

    for (i = 0; i < ARRSIZE(data->age_info.block_no); i++) {
        /* Physical block number */
        cur_block = BE2LE_16(data->age_info.block_no[i]);
        cur_block_age = BE2LE_16(data->age_info.erase_count[i]);
        if (cur_block && cur_block_age)
            /* Age information for current block */
            info->block_info_table[cur_block].erase_count = cur_block_age;
    }
}

void extract_type_data(health_status_info *info, MMCSD_SCSI_PASSTHRU_DATA *data)
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

int extract_total_age(int fd, int block_type, uint64_t *total_age, int *blk_count)
{
    MMCSD_SCSI_PASSTHRU_DATA data;
    health_status_info info;
    int res = 1;
    int i;
    uint64_t age_total = 0;
    int blocks = 0;
    int block_age_table_size = 0;
    char buf[512];
    mmc_ioc_cmd *mic;

    memset(&data, 0, sizeof(data));
    memset(&info, 0, sizeof(info));

    mic = calloc(1, sizeof(mmc_ioc_cmd));
    mic->data_ptr = (uint32_t ) &buf[0];
    mic->opcode = 56;
    mic->blksz = 512;
    mic->blocks = 1;
    /* Block ages */
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
            else
                goto fail;
        }
    } else
        goto fail;

    /* Block address type */
    for (i = 0; i < block_age_table_size; i++) {
        mic->arg = DCMD_MMCSD_GEN_GET_BLKTYPE_TBL_ARG(i);
        res = ioctl(fd, MMC_IOC_CMD, mic);
        if (res == 0) {
            memcpy(&data.type_info, buf, 512);
            extract_type_data(&info, &data);
        } else
            goto fail;
    }

    /* Add up ages for blocks of desired type */
    for (i = 0; i < NUM_BLOCKS; i++) {
        if (info.block_info_table[i].block_type == block_type
            && info.block_info_table[i].erase_count != 0) {
            age_total += info.block_info_table[i].erase_count;
            blocks++;
        }
    }
    if (blk_count)
        *blk_count = blocks;
    if (total_age)
        *total_age = age_total;
    res = 0;
fail:
    return res;
}

int refresh_one_block(int fd, uint16_t *last_refreshed_block)
{
    int res = 1;
    MMCSD_SCSI_PASSTHRU_DATA data;
    char buf[512];
    mmc_ioc_cmd *mic;

    memset(&data, 0, sizeof(data));
    mic = calloc(1, sizeof(mmc_ioc_cmd));
    mic->data_ptr = (uint32_t ) &buf[0];
    mic->opcode = 56;
    mic->blksz = 512;
    mic->blocks = 1;
    mic->arg = DCMD_MMCSD_GEN_REFRESH_BLOCKS(1);
    res = ioctl(fd, MMC_IOC_CMD, mic);
    if (res == 0) {
        if (last_refreshed_block)
            memcpy(&data.refreshed_block_info.block_no[0], buf, 2);
            *last_refreshed_block = BE2LE_16(data.refreshed_block_info.block_no[0]);
    } else {
        VERBOSE_PRINTF(0, "Failure to refresh blocks\n");
        goto fail;
    };
    res = 0;
fail:
    return res;
}

/**************************************
 * Management of historic information *
 **************************************/

/* Given string, extract decimal value from a substring */
int extract_str_value(char *s, unsigned int start, unsigned int len)
{
    char field[20];

    if (!s || start >= strlen(s))
        return 0;
    if (len >= sizeof(field))
        len = sizeof(field) - 1;
    memcpy(field, s + start, len);
    field[len] = '\0';
    return atoi(field);
}

/* Convert date/time string formatted as DDMMYYYYhhmmss to time_t */
int str_to_time(char *s, time_t *t)
{
    struct tm dt;
    int res = 1;

    if (strlen(s) != 14)
        return res; /* invalite date/time str */
    dt.tm_isdst = 0;
    dt.tm_mday = extract_str_value(s, 0, 2);
    dt.tm_mon = extract_str_value(s, 2, 2) - 1;
    dt.tm_year = extract_str_value(s, 4, 4) - 1900;
    dt.tm_hour = extract_str_value(s, 8, 2);
    dt.tm_min = extract_str_value(s, 10, 2);
    dt.tm_sec = extract_str_value(s, 12, 2);
    if (dt.tm_mday < 1 || dt.tm_mday > 31 ||
        dt.tm_mon < 0 || dt.tm_mon > 11 ||
        dt.tm_hour < 0 || dt.tm_hour > 23 ||
        dt.tm_min < 0 || dt.tm_min > 59 ||
        dt.tm_sec < 0 || dt.tm_sec > 59)
        return 1; /* invalid date/time */

    if (t)
        *t = mktime(&dt);
    return 0;
}

/*
 * Add all characters in string pointed by s shifting values to expand
 * result sum range.
 */
unsigned int add_chars(char *s, int shift_start)
{
    unsigned int sum = 0;
    int shift = shift_start;

    if (!s)
        return 0;
    while (*s) {
        sum += *s++ << shift;
        shift += 2;
    }
    return sum;
}

/*
 * Given a full line, add values of all character of its two first
 * comma-separated fields. Commas not included.
 */
unsigned int calc_line_checksum(char *line)
{
    char *line_bkp;
    char *p;
    unsigned int sum = 0;

    if (!line)
        return 0;
    line_bkp = malloc(strlen(line) + 1);
    if (!line_bkp)
        return 0;
    strcpy(line_bkp, line);
    p = strtok(line_bkp, ",");
    if (p) {
        sum += add_chars(p, 0);
        p = strtok(NULL, ",");
        if (p)
            sum += add_chars(p, 10);
    }
    free(line_bkp);
    return sum;
}

/* Parse a line from file into existing linked list node */
int parse_line(char *line, info_node *node)
{
    char *p;
    int res = 1;
    unsigned int expected_sum;
    unsigned int sum;

    if (!line || !node)
        return res;

    expected_sum = calc_line_checksum(line);
    p = strtok(line, ",");
    if (p) {
        str_to_time(p, &node->date_time);
        p = strtok(NULL, ",");
        if (p) {
            node->age = (uint64_t)atoll(p);
            p = strtok(NULL, ",");

            /*
             * If checksum exists in line, check it. If not (old file),
             * assume line is correct.
             */
            if (p) {
                sum = strtoul(p, NULL, 0);
                if (sum == expected_sum)
                    res = 0;
            }
            else
                res = 0;
        }
    }
    return res;
}

/* Free entire linked list, clean up head */
void free_info(info_node **head)
{
    info_node *node;

    if (!head)
        return;
    while (*head) {
        node = (*head)->next;
        free(*head);
        *head = node;
    }
}

/* Count items in list */
int count_info(info_node **head)
{
    int count = 0;
    info_node *node;

    if (!head || !(*head))
        return count;
    node = *head;
    do {
        count++;
        node = node->next;
    } while (node);
    return count;
}

/* Return 1 if two date/times have same date */
int same_day(time_t date_time1, time_t date_time2)
{
    struct tm *dt;
    struct tm dt1;
    struct tm dt2;

    dt = gmtime(&date_time1);
    dt1 = *dt;
    dt = gmtime(&date_time2);
    dt2 = *dt;
    return (dt1.tm_mday == dt2.tm_mday &&
        dt1.tm_mon == dt2.tm_mon &&
        dt1.tm_year == dt2.tm_year) ? 1 : 0;
}

/*
 * Add node to end of linked list. If squash == 1 and the last entry has
 * the same date, overwrite the last entry instead of adding new one.
 */
int add_info(info_node **head, time_t date_time, uint64_t age, int squash)
{
    info_node *node;
    info_node *next_node;

    if (!head)
        return 1;
    node = *head;

    /* find last item in list */
    while (node && node->next)
        node = node->next;

    /* create new node if needed and make node point at it */
    if (!node || !squash || !same_day(node->date_time, date_time)) {
        next_node = malloc(sizeof(info_node));
        if (!next_node)
            return 1;
        if (node)
            node->next = next_node;
        else
            *head = next_node;
        node = next_node;
    }
    node->date_time = date_time;
    node->age = age;
    node->next = NULL;
    node->index = count_info(head) - 1;
    return 0;
}

/* Empty linked list and load historic data from file into linked list. */
int load_info(char *file_name, info_node **head)
{
    FILE *fp;
    char buf[100];
    int res = 1;
    info_node node;
    info_node *new_node;
    info_node *cur_node = NULL;
    int index = 0;

    if (!head)
        return res;
    free_info(head);
    fp = fopen(file_name, "r");
    if (fp == NULL)
        return res;
    while (fgets(buf, sizeof(buf), fp)) {
        if (parse_line(buf, &node) == 0) {
            new_node = malloc(sizeof(info_node));
            if (!new_node) {
                res = 1;
                break;
            }
            *new_node = node;
            new_node->index = index++;
            new_node->next = NULL;
            if (cur_node)
                cur_node->next = new_node;
            else
                *head = new_node;
            cur_node = new_node;
            res = 0; /* at least one line of valid data was detected */
        }
    }
    fclose(fp);
    return res;
}

/* Convert linked list node to string pointed by p, up to len characters */
int node_to_str(info_node *node, char *p, int len)
{
    struct tm *dt;
    char aux[100];

    if (!node || !p)
        return 1;
    dt = gmtime(&node->date_time);
    if (!dt)
        return 1;
    strftime(p, len, "%d%m%Y%H%M%S,", dt);
    sprintf(aux, "%llu,", node->age);
    strncat(p, aux, len - strlen(p));
    sprintf(aux, "%u\n", calc_line_checksum(p));
    strncat(p, aux, len - strlen(p));
    return 0;
}

/*
 * Save historic linked list to file, up to max_entries. If list longer
 * than max_entries, only the list tail is saved.
 */
int save_info(char *file_name, info_node **head, int max_entries)
{
    int res = 1;
    FILE *fp;
    char buf[100];
    info_node *node;
    int total_count;

    if (!head)
        return res;
    fp = fopen(file_name, "w");
    if (fp == NULL)
        return res;
    node = *head;
    total_count = count_info(head);
    while (node) {
        /*
         * If no limit in number of nodes in file, write node.
         * If limit is set, but total list doesn't pass it, write node.
         * If limit set and list exceeds it, write only nodes from beginning
         *   and end of list, skipping nodes in the center.
         */
        if (max_entries == 0 ||
            total_count <= max_entries ||
            node->index >= total_count - max_entries) {
            if (node_to_str(node, buf, sizeof(buf)) == 0) {
                if (fputs(buf, fp) >= 0)
                    res = 0;
            }
        }
        node = node->next;
    }
    fclose(fp);
    return res;
}

/* Display linked list info (debug) */
void dump_info(info_node **head)
{
    info_node *node;
    char buf[100];

    printf("Info:\n");
    if (!head || !(*head))
        return;
    node = *head;
    while (node) {
        if (node_to_str(node, buf, sizeof(buf)) == 0)
            printf("%s", buf);
        node = node->next;
    }
}

/* Calculate how many blocks should be refreshed. */
int calculate_blocks_to_refresh(info_node **head)
{
    info_node *first;
    info_node *last;
    uint64_t target_age;
    char first_dt_str[100] = "Invalid";
    char last_dt_str[100] = "Invalid";
    struct tm *dt;

    if (!head || !(*head))
        return 0;
    /*
     * Use last entry and entry from 6 months ago in historic data to calculate
     * number of blocks to refresh.
     */
    last = first = *head;

    /*
     * Locate last entry (current time and age). At the same time, if any
     * out-of-order date is found, force "first" to point at it to ensure
     * only the last valid sequence of dates is used.
     */
    while (last->next) {
        if (difftime(last->next->date_time, last->date_time) < 0)
            first = last->next;
        last = last->next;
    }

    /* Locate entry 6 months ago, store in first */
    while (first->next &&
        difftime(last->date_time, first->next->date_time) > MNAND_SECS_TO_REFRESH)
        first = first->next;
    if (last == first)
        return DEFAULT_REFRESH_COUNT;

    dt = gmtime(&first->date_time);
    if (dt)
        strftime(first_dt_str, sizeof(first_dt_str), "%d %b %Y", dt);
    dt = gmtime(&last->date_time);
    if (dt)
        strftime(last_dt_str, sizeof(last_dt_str), "%d %b %Y", dt);
    VERBOSE_PRINTF(1, "Using data points (%s, %llu) and (%s, %llu)\n",
        first_dt_str, first->age, last_dt_str, last->age
    );
    target_age = first->age +
        MNAND_DESIRED_REFRESH_SPEED *
            difftime(last->date_time, first->date_time);

    return target_age > last->age ? target_age - last->age : 0;
}

int usage(void)
{
    printf("usage: mnand_rfsh -d <path to mnand device> [-t <date/time> | -T] -p <path-to-persist>\n\n"
           "   This tool attempts to ensure that the entire content of mNAND is refreshed within\n"
           "   6 months by checking the current total age of mNAND blocks and comparing with\n"
           "   the age 6 months before (or the oldest logged age). The tool issues the mNAND\n"
           "   refresh command as many times as needed to stay within the expected target.\n\n"
           "   -d <mnand_path>   Path to mnand device (for instance, /dev/mmcblk3)\n"
           "   -t <date/time>    Current date/time in format DDMMYYYYhhmmss\n"
           "                     If valid date/time not available, pass 0\n"
           "   -T                Use current system time\n"
           "   -p <path>         File name where tool can persist its information\n"
           "   -m <time>         Maximum time to run this tool (in seconds). Default is no limit (0)\n"
           "   -i <time>         Delay between refresh commands (in milliseconds). Default is 100ms\n"
           "   -v <verbosity>    Set verbosity level (0 - 2). Default 0.\n");
    return 1;
}

int s_stop = 0;

void sig_handler(int sig_num)
{
    s_stop = 1;
}

int main(int argc, char **argv)
{
    int fd;
    int res = 1;
    char devnode[40] = "\0";
    char date_time[40] = "\0";
    char file_path[200] = "\0";
    unsigned int max_time = 0;
    int blocks_to_refresh = 0;
    int blocks_refreshed = 0;
    int blk_count;
    uint16_t last_refreshed_block;
    int valid_stored_info = 1;
    int valid_date = 1;
    uint64_t cur_age;
    time_t cur_time;
    info_node *info_list = NULL;
    time_t start_time;
    unsigned int inter_rfsh_delay = 100;
    int opt;
    time_t t;

    printf("mNAND Refresh Tool\n");

    /* Remove node that notifies we are done */
    unlink(MNAND_RFSH_FINISHED);

    /* Extract command line parameters */
    if (argc < 2)
        return usage();

    while ((opt = getopt(argc, argv, "d:t:Tp:v:m:i:h")) != -1) {
        switch (opt) {
            case 'd':
                strcpy(devnode, optarg);
                break;
            case 't':
                strcpy(date_time, optarg);
                break;
            case 'T':
                t = time(NULL);
                struct tm *dt;
                dt = gmtime(&t);
                strftime(date_time, sizeof(date_time), "%d%m%Y%H%M%S", dt);
                break;
            case 'p':
                strcpy(file_path, optarg);
                break;
            case 'v':
                s_verbosity = strtoul(optarg, NULL, 0);
                break;
            case 'm':
                max_time = strtoul(optarg, NULL, 0);
                break;
            case 'i':
                inter_rfsh_delay = strtoul(optarg, NULL, 0);
                break;
            case 'h':
            default:
                usage();
                return -1;
        }
    }

    if (devnode[0] == '\0') {
        printf("mNAND path must be provided\n");
        return usage();
    }
    if (date_time[0] == '\0') {
        printf("Current date and time must be provided\n");
        return usage();
    }
    if (file_path[0] == '\0') {
        printf("Path to persistence file must be provided\n");
        return usage();
    }
    if (strcmp(date_time, "0") == 0)
        valid_date = 0; /* provided '-t 0' parameter; use defaults */
    else if (str_to_time(date_time, &cur_time)) {
        printf("Invalid date/time provided: %s\n", date_time);
        return usage();
    }

    /* Try to load info from existing file */
    if (load_info(file_path, &info_list)) {
        printf("Could not find valid stored info. Using defaults.\n");
        valid_stored_info = 0;
    } else
        VERBOSE_PRINTF(1, "Loaded %d data points from file\n", count_info(&info_list));

    /* Open mNAND device */
    fd = open(devnode, O_RDONLY);
    if (fd <= 0) {
        printf("Failed to access node. errno %d (%s)\n", errno, strerror(errno));
        return 1;
    }

    /* Register signal handlers for graceful termination */
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    /* Extract total MLC age from mNAND */
    res = extract_total_age(fd, MMCSD_BLOCK_TYPE_MLC, &cur_age, &blk_count);
    if (res)
        goto fail;
    VERBOSE_PRINTF(1, "%d MLC blocks. Total age before refresh: %llu.\n",
        blk_count, cur_age);

    /* Add to linked list */
    if (valid_date)
        res = add_info(&info_list, cur_time, cur_age, count_info(&info_list) > 1);

    /* Calculate number of blocks to refresh */
    if (!valid_stored_info || !valid_date)
        blocks_to_refresh = DEFAULT_REFRESH_COUNT;
    else
        blocks_to_refresh = calculate_blocks_to_refresh(&info_list);

    VERBOSE_PRINTF(1, "Blocks to refresh: %d\n", blocks_to_refresh);
    if (max_time)
        VERBOSE_PRINTF(1, "Time limit: %d sec\n", max_time);
    else
        VERBOSE_PRINTF(1, "No time limit\n");
    VERBOSE_PRINTF(1, "Delay between refreshes: %d ms\n", inter_rfsh_delay);

    /* Refresh blocks */
    VERBOSE_PRINTF(1, "Refresh started. Ctrl + C or slay command to abort early.\n");
    start_time = time(NULL);
    while (blocks_to_refresh-- &&
            !s_stop &&
            (max_time == 0 || difftime(time(NULL), start_time) < max_time)) {
        res = refresh_one_block(fd, &last_refreshed_block);
        if (res) {
            printf("Failed to refresh a block\n");
            goto fail;
        }
        VERBOSE_PRINTF(2, "Refreshed block %d\n", last_refreshed_block);
        blocks_refreshed++;
        usleep(inter_rfsh_delay*1000);
    }
    if (blocks_to_refresh > 0) {
        if (s_stop)
            VERBOSE_PRINTF(1, "Refresh process aborted.\n");
        else if (max_time)
            VERBOSE_PRINTF(1, "Refresh process exceeded %d sec.\n", max_time);
    }
    VERBOSE_PRINTF(1, "Refreshed %d blocks.\n", blocks_refreshed);

    if (blocks_refreshed) {
        /* Extract total MLC age from mNAND after refresh */
        res = extract_total_age(fd, MMCSD_BLOCK_TYPE_MLC, &cur_age, &blk_count);
        if (res)
            goto fail;
        VERBOSE_PRINTF(1, "%d MLC blocks. Total age after refresh: %llu.\n",
            blk_count, cur_age);

        /* Add to linked list. This should squash the last entry just added */
        if (valid_date)
            res = add_info(&info_list, cur_time, cur_age, count_info(&info_list) > 1);
    }

    /* Update file content (if valid date was provided) */
    if (valid_date) {
        res = save_info(file_path, &info_list, MAX_LINES_IN_FILE);
        if (res) {
            printf("Failed to save persistent data\n");
            goto fail;
        }
    }

    res = 0;

fail:
    if (res)
        printf("res = %d\n", res);
    close(fd);

    if (info_list)
        free_info(&info_list);

    /* Notify we are done */
    fd = open(MNAND_RFSH_FINISHED, O_CREAT | O_RDWR, 0644);
    if (fd >= 0)
        close(fd);

    return res;
}
