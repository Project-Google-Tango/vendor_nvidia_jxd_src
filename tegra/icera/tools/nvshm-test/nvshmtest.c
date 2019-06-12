/*
 *  Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 *  NVIDIA Corporation and its licensors retain all intellectual property
 *  and proprietary rights in and to this software, related documentation
 *  and any modifications thereto.  Any use, reproduction, disclosure or
 *  distribution of this software and related documentation without an express
 *  license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/inotify.h>
#include <poll.h>
#include <sys/mman.h>

#define POLL_CMD 1
#define NOTIFY_CMD 2
#define READ_CMD 3
#define WRITE_CMD 4
#define CONF_CMD 5
#define CHECK_CMD 6

#define SYSFS_PATH "/sys/devices/platform"
#define PATH_SIZE 64
#define BUF_SIZE 64

/* Shared memory fixed offsets */

#define NVSHM_IPC_BASE (0x0)              /* IPC mailbox base offset */
#define NVSHM_IPC_MAILBOX (0x0)           /* IPC mailbox offset */
#define NVSHM_IPC_RETCODE (0x4)           /* IPC mailbox return code offset */
#define NVSHM_IPC_SIZE (4096)             /* IPC mailbox region size */
#define NVSHM_CONFIG_OFFSET (8)           /* shared memory config offset */
#define NVSHM_MAX_CHANNELS_2_0 (12)       /* Maximum number of channels */
#define NVSHM_CHAN_NAME_SIZE (27)         /* max channel name size in chars */

/* Versions: */
#define NVSHM_MAJOR(v) (v >> 16)
#define NVSHM_MINOR(v) (v & 0xFFFF)
/** Version 1.3 is supported. Keep until modem image has reached v2.x in main */
#define NVSHM_CONFIG_VERSION_1_3 (0x00010003)
/** Version with statistics export support, otherwise compatible with v1.3 */
#define NVSHM_CONFIG_VERSION (0x00020001)

/* NVSHM serial number size in bytes */
#define NVSHM_SERIAL_BYTE_SIZE 20

#define NVSHM_IPC_BB_BASE (0x8C000000)
#define NVSHM_PRIV_BB_BASE (0x88000000)
#define NVSHM_BBC_SHM_SIZE (128*1024*1024)

/* This bit define cacheness on BBC */
#define BBC_CACHED (0x20000000)
#define NVSHM_AP_POOL_ID (128) /* IOPOOL ID - use 128-255 for AP */
#define ADDR_OUTSIDE(addr, base, size) (((unsigned long)(addr)		\
					 < (unsigned long)(base)) ||	\
					((unsigned long)(addr)		\
					 > ((unsigned long)(base) +	\
					    (unsigned long)(size))))

#define AP_CONVERT(base, iob) ((void *)((int)iob - ((int)base) + NVSHM_IPC_BB_BASE))
#define BB_CONVERT(base, iob) ((void *)(((int)iob & ~BBC_CACHED) + ((int)base) - NVSHM_IPC_BB_BASE))

/**
 * Channel type
 */
enum nvshm_chan_type {
	NVSHM_CHAN_UNMAP = 0,
	NVSHM_CHAN_TTY,
	NVSHM_CHAN_LOG,
	NVSHM_CHAN_NET,
	NVSHM_CHAN_RPC,
};

struct nvshm_chan_map {
	int type;
	char name[NVSHM_CHAN_NAME_SIZE+1];
};

struct nvshm_config {
	int version;
	int shmem_size;
	int region_ap_desc_offset;
	int region_ap_desc_size;
	int region_bb_desc_offset;
	int region_bb_desc_size;
	int region_ap_data_offset;
	int region_ap_data_size;
	int region_bb_data_offset;
	int region_bb_data_size;
	int queue_ap_offset;
	int queue_bb_offset;
	union {
		struct {
			int chan_count;
			int chan_map_offset;
			char serial[NVSHM_SERIAL_BYTE_SIZE];
			int region_dxp1_stats_offset;
			int region_dxp1_stats_size;
			int guard;
		} v3;
		struct {
			struct nvshm_chan_map chan_map[NVSHM_MAX_CHANNELS_2_0];
			char serial[NVSHM_SERIAL_BYTE_SIZE];
			int region_dxp1_stats_offset;
			int region_dxp1_stats_size;
			int guard;
		} v2;
	} compat;
};

/** NVSHM_IPC mailbox messages ids */
enum nvshm_ipc_mailbox
{
	/** Boot status */
	NVSHM_IPC_BOOT_COLD_BOOT_IND=0x01,  /** AP2BROM: Cold boot indication */
	NVSHM_IPC_BOOT_FW_REQ,              /** BROM2AP: BROM request F/W at cold boot */
	NVSHM_IPC_BOOT_RESTART_FW_REQ,      /** BROM2AP: BROM request F/W after crash
				       * (e.g NVSHM_IPC_BOOT_COLD_BOOT_IND not found in mailbox) */
	NVSHM_IPC_BOOT_FW_CONF,             /** AP2BROM: F/W ready confirmation */
	NVSHM_IPC_READY,                    /** BB2AP: runtime NVSHM_IPC state after successfull boot */

	/** Boot errors */
	NVSHM_IPC_BOOT_ERROR_BT2_HDR=0x1000,/** BROM2AP: BROM found invalid BT2 header */
	NVSHM_IPC_BOOT_ERROR_BT2_SIGN,      /** BROM2AP: BROM failed to SHA1-RSA authenticate BT2 */
	NVSHM_IPC_BOOT_ERROR_HWID,          /** BT22AP:  BT2 found invalid H/W ID */
	NVSHM_IPC_BOOT_ERROR_APP_HDR,       /** BT22AP:  BT2 found invalid APP header */
	NVSHM_IPC_BOOT_ERROR_APP_SIGN,      /** BT22AP:  BT2 failed to SHA1-RSA authenticate APP */
	NVSHM_IPC_BOOT_ERROR_UNLOCK_HEADER, /** BT22AP:  BT2 found invalid unlock cert. header */
	NVSHM_IPC_BOOT_ERROR_UNLOCK_SIGN,   /** BT22AP:  BT2 failed to SHA1-RSA authenticate unlock cert. */
	NVSHM_IPC_BOOT_ERROR_UNLOCK_PCID,   /** BT22AP:  BT2 found invalid PCID in unlock cert. */

	NVSHM_IPC_MAX_MSG=0xFFFF            /** Max ID for NVSHM_IPC mailbox message */
};

struct nvshm_iobuf {
	/* Standard iobuf part - This part is fixed and cannot be changed */
	unsigned char   *npdu_data;
	unsigned short   length;
	unsigned short   data_offset;
	unsigned short   total_length;
	unsigned char ref;
	unsigned char pool_id;
	struct nvshm_iobuf *next;
	struct nvshm_iobuf *sg_next;
	unsigned short flags;
	unsigned short _size;
	void *_handle;
	unsigned int _reserved;

	/* Extended iobuf - This part is not yet fixed (under spec/review) */
	struct nvshm_iobuf *qnext;
	int chan;
	int qflags;
	int _reserved1;
	int _reserved2;
	int _reserved3;
	int _reserved4;
	int _reserved5;
};

static long ipc_size = 0;
static long priv_size = 0;

static void print_usage(void) {
	fprintf(stderr, "Mutual exclusive commands:\n\
\t -p poll sysfs\n\
\t -n notify sysfs\n\
\t -r <size> read 32/16/8 bits\n\
\t -w <size> 32/16/8 bits write\n\
\t -d dump configuration\n\
\t -a dump all iobufs, even if not broken\n\
\t -c check IPC region information\n\
\nIPC source:\n\
\t -i <instance>\n\
\t -f <name> coredump file\n\
\nOptions:\n\
\t -m <memory:priv/ipc>\n\
\t -o offset (in hex)\n\
\t -v value (in hex)\n\
\t -h help\n");
}

static int poll_cmd(int instance) {
	struct pollfd fds;
	char buf[BUF_SIZE];
	char fname[PATH_SIZE];
	int ret;

	memset(&fds, 0, sizeof(fds));
	snprintf(fname, sizeof(fname)-1,"%s/tegra_bb.%01d/status", SYSFS_PATH, instance);
	while (1) {

		fds.fd = open(fname, O_RDONLY);
		if (fds.fd == -1) {
			fprintf(stderr, "Could not open %s\n", fname);
			return -1;
		}
		ret = read(fds.fd, buf, sizeof(buf));
		fprintf(stdout,"READ %s\n", buf);
		fds.events = POLLPRI | POLLERR;
		ret = poll(&fds, 1, -1);
		if (ret > 0) {
			if (fds.revents & POLLPRI) {
				fprintf(stdout, "POLLPRI event received\n");
			}
			if (fds.revents & POLLERR) {
				fprintf(stdout, "POLLERR event received\n");
			}
		}
		close(fds.fd);
	}
	return 0;
}

static int notify_cmd(int instance, const char *value) {
	int check_value, fd, ret;
	char fname[PATH_SIZE];
	char buf[BUF_SIZE];

	if (sscanf(value,"%x", &check_value) != 1) {
		fprintf(stderr, "Wrong value given: %s\n", value);
		return -1;
	}
	snprintf(buf,sizeof(buf)-1,"%d\n", check_value);
	snprintf(fname, sizeof(fname)-1, "%s/tegra_bb.%01d/status", SYSFS_PATH, instance);
	fd = open(fname, O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "Could not open %s\n", fname);
		return -1;
	}
	ret = write(fd, buf, strlen(buf));
	close(fd);
	return ret;
}

static int read_cmd(int instance, const char *memory, const char *offset, int size) {
	int check_offset, fd;
	char fname[PATH_SIZE];
	int length;

	if (sscanf(offset,"%x", &check_offset) != 1) {
		fprintf(stderr, "Wrong offset given: %s\n", offset);
		return -1;
	}

	switch(size) {
	case 32:
		if (check_offset & 0x3) {
			fprintf(stderr, "Alignement error requesting access in %d bit at offset 0x%x\n", size, check_offset);
			return -1;
		}
		break;
	case 16:
		if (check_offset & 0x1) {
			fprintf(stderr, "Alignement error requesting access in %d bit at offset 0x%x\n", size, check_offset);
			return -1;
		}
		break;
	case 8:
		break;
	default:
		fprintf(stderr, "Wrong access size given: %d\n", size);
		return -1;
	}

	if (strcmp(memory, "priv") == 0) {
		snprintf(fname, sizeof(fname)-1, "/dev/tegra_bb_priv%01d", instance);
		length = priv_size;
	} else {
		if (strcmp(memory, "ipc") == 0) {
			snprintf(fname, sizeof(fname)-1, "/dev/tegra_bb_ipc%01d", instance);
		length = ipc_size;
		} else {
			fprintf(stderr, "Wrong memory type given %s\n",memory);
			return -1;
		}
	}

	fd = open(fname, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Could not open %s\n", fname);
		return -1;
	}
	switch (size) {
	case 32:
	{
		volatile unsigned int *addr = NULL;
		addr = (unsigned int *)mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
		fprintf(stdout, "32 bit Read@0x%x = 0x%08x\n", check_offset, addr[check_offset/4]);
		munmap((void*)addr, length);
	}
	break;
	case 16:
	{
		volatile unsigned short *addr = NULL;
		addr = (unsigned short *)mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
		fprintf(stdout, "16 bit Read@0x%x = 0x%04x\n", check_offset, addr[check_offset/2]);
		munmap((void*)addr, length);
	}
	break;
	case 8:
	{
		volatile unsigned char *addr = NULL;
		addr = (unsigned char *)mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
		fprintf(stdout, "8 bit Read@0x%x = 0x%02x\n", check_offset, addr[check_offset]);
		munmap((void*)addr, length);
	}
	break;
	}
	close(fd);
	return 0;
}

static int write_cmd(int instance, const char *memory, const char *offset, const char *value, int size) {
	int check_offset, check_value, fd;
	char fname[PATH_SIZE];
	int length;

	if (sscanf(offset,"%x", &check_offset) != 1) {
		fprintf(stderr, "Wrong offset given: %s\n", offset);
		return -1;
	}
	if (sscanf(value,"%x", &check_value) != 1) {
		fprintf(stderr, "Wrong value given: %s\n", value);
		return -1;
	}

	switch(size) {
	case 32:
		if (check_offset & 0x3) {
			fprintf(stderr, "Alignement error requesting access in %d bit at offset 0x%x\n", size, check_offset);
			return -1;
		}
		break;
	case 16:
		if (check_offset & 0x1) {
			fprintf(stderr, "Alignement error requesting access in %d bit at offset 0x%x\n", size, check_offset);
			return -1;
		}
		if (check_value & ~0xffff) {
			fprintf(stderr, "value %x exceed 16 bit\n", check_value);
			return -1;
		}
		break;
	case 8:
		if (check_value & ~0xff) {
			fprintf(stderr, "value %x exceed 8 bit\n", check_value);
			return -1;
		}
		break;
	default:
		fprintf(stderr, "Wrong access size given: %d\n", size);
		return -1;
	}

	if (strcmp(memory, "priv") == 0) {
		snprintf(fname, sizeof(fname)-1, "/dev/tegra_bb_priv%01d", instance);
		length = priv_size;
	} else {
		if (strcmp(memory, "ipc") == 0) {
			snprintf(fname, sizeof(fname)-1, "/dev/tegra_bb_ipc%01d", instance);
			length = ipc_size;
		} else {
			fprintf(stderr, "Wrong memory type given %s\n",memory);
			return -1;
		}
	}

	fd = open(fname, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Could not open %s\n", fname);
		return -1;
	}
	fprintf(stderr, "%s:L%d\n", __FUNCTION__, __LINE__);
	switch (size) {
	case 32:
	{
		volatile unsigned int *addr = NULL;
		addr = (unsigned int *)mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);
		addr[check_offset/4] = check_value;
		fprintf(stdout, "32 bit Write@0x%x = 0x%08x\n", check_offset, check_value);
		munmap((void*)addr, length);
	}
	break;
	case 16:
	{
		volatile unsigned short *addr = NULL;
		addr = (unsigned short *)mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);
		addr[check_offset/2] = check_value & 0xFFFF;
		fprintf(stdout, "16 bit Write@0x%x = 0x%04x\n", check_offset, check_value);
		munmap((void*)addr, length);
	}
	break;
	case 8:
	{
		volatile unsigned char *addr = NULL;
		addr = (unsigned char *)mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);
		addr[check_offset] = check_value & 0xFF;
		fprintf(stdout, "8 bit Write@0x%x = 0x%04x\n", check_offset, check_value);
		munmap((void*)addr, length);
	}
	break;
	}
	close(fd);
	return 0;
}

static int conf_cmd(int instance, char* filename) {
	int fd;
	char fname[PATH_SIZE];
	struct nvshm_config *conf = NULL;
	int chan;
	void *addr;
	int phys_addr;

	if (filename == NULL) {
		printf("read from instance %d\n", instance);
		snprintf(fname, sizeof(fname)-1,"/dev/tegra_bb_ipc%01d", instance);
	} else {
		printf("read from file %s\n", filename);
		strncpy(fname, filename, sizeof(fname)-1);
	}

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s\n", fname);
		return -2;
	}

	addr = mmap(NULL, ipc_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!addr) {
		fprintf(stderr, "Could not map %s\n", fname);
		return -3;
	}

	conf = (struct nvshm_config *)((unsigned long)addr + NVSHM_CONFIG_OFFSET);
	fprintf(stdout, "NVHSM config:\n");
	phys_addr = NVSHM_IPC_BB_BASE;
	fprintf(stdout, "\tversion =\t\t\t0x%08x\t\t[0x%08x]\n", conf->version, phys_addr);
	phys_addr += conf->shmem_size;
	fprintf(stdout, "\tshmem_size =\t\t\t0x%08x (%dMiB)\t[0x%08x]\n", conf->shmem_size, conf->shmem_size/(1024*1024), phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->region_ap_desc_offset;
	fprintf(stdout, "\tregion_ap_desc_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->region_ap_desc_offset, phys_addr);
	phys_addr += conf->region_ap_desc_size;
	fprintf(stdout, "\tregion_ap_desc_size =\t\t%d (%d desc)\t[0x%08x]\n", conf->region_ap_desc_size, conf->region_ap_desc_size/64, phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->region_bb_desc_offset;
	fprintf(stdout, "\tregion_bb_desc_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->region_bb_desc_offset, phys_addr);
	phys_addr += conf->region_bb_desc_size;
	fprintf(stdout, "\tregion_bb_desc_size =\t\t%d (%d desc)\t[0x%08x]\n", conf->region_bb_desc_size, conf->region_bb_desc_size/64, phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->region_ap_data_offset;
	fprintf(stdout, "\tregion_ap_data_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->region_ap_data_offset, phys_addr);
	phys_addr += conf->region_ap_data_size;
	fprintf(stdout, "\tregion_ap_data_size =\t\t%d (%dMB)\t\t[0x%08x]\n", conf->region_ap_data_size, conf->region_ap_data_size/(1024*1024), phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->region_bb_data_offset;
	fprintf(stdout, "\tregion_bb_data_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->region_bb_data_offset, phys_addr);
	phys_addr += conf->region_bb_data_size;
	fprintf(stdout, "\tregion_bb_data_size =\t\t%d (%dMB)\t\t[0x%08x]\n", conf->region_bb_data_size, conf->region_bb_data_size/(1024*1024), phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->queue_ap_offset;
	fprintf(stdout, "\tqueue_ap_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->queue_ap_offset, phys_addr);
	phys_addr = NVSHM_IPC_BB_BASE + conf->queue_bb_offset;
	fprintf(stdout, "\tqueue_bb_offset =\t\t0x%08x\t\t[0x%08x]\n", conf->queue_bb_offset, phys_addr);

	/* Conf 2/3 split */
	if (NVSHM_MAJOR(conf->version) <= 2) {
                phys_addr = NVSHM_IPC_BB_BASE + conf->compat.v2.region_dxp1_stats_offset;
                fprintf(stdout, "\tregion_dxp1_stats_offset =\t0x%08x\t\t[0x%08x]\n", conf->compat.v2.region_dxp1_stats_offset, phys_addr);
                phys_addr += conf->compat.v2.region_dxp1_stats_size;
                fprintf(stdout, "\tregion_dxp1_stats_size =\t%d\t\t\t[0x%08x]\n", conf->compat.v2.region_dxp1_stats_size, phys_addr);
                fprintf(stdout, "\n");

                for (chan=0; chan < NVSHM_MAX_CHANNELS_2_0; chan++) {
                        switch (conf->compat.v2.chan_map[chan].type) {
                        case NVSHM_CHAN_TTY:
                                fprintf(stdout, "\tchannel #%d mapped as TTY\n", chan);
                                break;
                        case NVSHM_CHAN_LOG:
                                fprintf(stdout, "\tchannel #%d mapped as LOG\n", chan);
                                break;
                        case NVSHM_CHAN_NET:
                                fprintf(stdout, "\tchannel #%d mapped as NET\n", chan);
                                break;
                        case NVSHM_CHAN_RPC:
                                fprintf(stdout, "\tchannel #%d mapped as RPC\n", chan);
                                break;
                        case NVSHM_CHAN_UNMAP:
                        default:
                                break;
                        }
                }
	} else {
                struct nvshm_chan_map *map;
                int channel_count;

                phys_addr = NVSHM_IPC_BB_BASE + conf->compat.v3.region_dxp1_stats_offset;
                fprintf(stdout, "\tregion_dxp1_stats_offset =\t0x%08x\t\t[0x%08x]\n", conf->compat.v3.region_dxp1_stats_offset, phys_addr);
                phys_addr += conf->compat.v3.region_dxp1_stats_size;
                fprintf(stdout, "\tregion_dxp1_stats_size =\t%d\t\t\t[0x%08x]\n", conf->compat.v3.region_dxp1_stats_size, phys_addr);
                fprintf(stdout, "\n");

                channel_count = conf->compat.v3.chan_count;
                fprintf(stdout, "\tchannel_count =\t%d\n", channel_count);
                map = (struct nvshm_chan_map *)((unsigned long)addr + conf->compat.v3.chan_map_offset);
                for (chan=0; chan < channel_count; chan++) {
                        switch (map[chan].type) {
                        case NVSHM_CHAN_TTY:
                                fprintf(stdout, "\tchannel #%d mapped as TTY\n", chan);
                                break;
                        case NVSHM_CHAN_LOG:
                                fprintf(stdout, "\tchannel #%d mapped as LOG\n", chan);
                                break;
                        case NVSHM_CHAN_NET:
                                fprintf(stdout, "\tchannel #%d mapped as NET\n", chan);
                                break;
                        case NVSHM_CHAN_RPC:
                                fprintf(stdout, "\tchannel #%d mapped as RPC\n", chan);
                                break;
                        case NVSHM_CHAN_UNMAP:
                        default:
                                break;
                        }
                }
        }

	munmap(conf, sizeof(struct nvshm_config));
	close(fd);
	return 0;
}

static void get_mem_size(int instance)
{
#ifdef ANDROID
	int fd;
	char buf[16];
	char fname[PATH_SIZE];
	int ret;

	if (instance < 0 )
		instance = 0;

	snprintf(fname, sizeof(fname)-1,"%s/tegra_bb.%01d/ipc_size", SYSFS_PATH, instance);
	fd = open(fname, O_RDONLY);
	ret = read(fd, buf, 16);
	if (ret)
		sscanf(buf, "%ld", &ipc_size);
	close(fd);

	snprintf(fname, sizeof(fname)-1,"%s/tegra_bb.%01d/priv_size", SYSFS_PATH, instance);
	fd = open(fname, O_RDONLY);
	ret = read(fd, buf, 16);
	if (ret)
		sscanf(buf, "%ld", &priv_size);
	fprintf(stdout,"SHM memory region sizes: IPC=%ldMB Priv=%ldMB\n", ipc_size/(1024*1024), priv_size/(1024*1024));
	close(fd);
#else
	(void) instance;
	ipc_size = 8*1024*1024;
	priv_size = 64*1024*1024;
#endif
}

static void dump_iob(void *base, struct nvshm_iobuf *iob)
{
	fprintf(stdout,"iobuf (%p) dump:\n", AP_CONVERT(base, iob));
	fprintf(stdout,"\t data      = %p\n", iob->npdu_data);
	fprintf(stdout,"\t length    = %d\n", iob->length);
	fprintf(stdout,"\t offset    = %d\n", iob->data_offset);
	fprintf(stdout,"\t total_len = %d\n", iob->total_length);
	fprintf(stdout,"\t ref       = %d\n", iob->ref);
	fprintf(stdout,"\t pool_id   = %d\n", iob->pool_id);
	fprintf(stdout,"\t next      = %p\n", iob->next);
	fprintf(stdout,"\t sg_next   = %p\n", iob->sg_next);
	fprintf(stdout,"\t flags     = 0x%x\n", iob->flags);
	fprintf(stdout,"\t _size     = %d\n", iob->_size);
	fprintf(stdout,"\t _handle   = %p\n", iob->_handle);
	fprintf(stdout,"\t _reserved = 0x%x\n", iob->_reserved);
	fprintf(stdout,"\t qnext     = %p\n", iob->qnext);
	fprintf(stdout,"\t chan      = 0x%x\n", iob->chan);
	fprintf(stdout,"\t qflags    = 0x%x\n\n", iob->qflags);
}

static int check_cmd(int instance, char* filename, int dump_all_iobufs)
{
	char fname[PATH_SIZE];
	struct nvshm_config *conf = NULL;
	void *addr;
	struct nvshm_iobuf *iob;
	int fd, chan, ndesc, desc, free_d, ipc_use = 0, in_queue = 0, ret;

	ret = conf_cmd(instance, filename);
	if (ret < 0)
		return ret;

	if (filename == NULL) {
		snprintf(fname, sizeof(fname)-1,"/dev/tegra_bb_ipc%01d", instance);
	} else {
		strncpy(fname, filename, sizeof(fname)-1);
	}

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s\n", fname);
		return -2;
	}

	addr = mmap(NULL, ipc_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!addr) {
		fprintf(stderr, "Could not map %s\n", fname);
		return -3;
	}

	conf = (struct nvshm_config *)((unsigned long)addr + NVSHM_CONFIG_OFFSET);
	if ((conf->region_ap_desc_offset % (sizeof(struct nvshm_iobuf)/2)) == 0) {
                int *chan_use;
                int chan_count;

                if (NVSHM_MAJOR(conf->version) <= 2)
                        chan_count = NVSHM_MAX_CHANNELS_2_0;
                else
                        chan_count =  conf->compat.v3.chan_count;

                chan_use = malloc(sizeof(int) * chan_count);
                memset(chan_use, 0, sizeof(chan_use));
		ndesc = conf->region_ap_desc_size / sizeof(struct nvshm_iobuf);
		fprintf(stdout, "\nChecking %d AP descriptors\n", ndesc);
		iob = (struct nvshm_iobuf *)((long)addr + conf->region_ap_desc_offset);
		free_d = ndesc;

		for (desc = 0; desc < ndesc; desc++) {
			int dump = 0;
			if (iob[desc].pool_id < NVSHM_AP_POOL_ID) {
				fprintf(stdout, "Error: pool_id invalid (%d)\n", iob[desc].pool_id);
				dump = 1;
			}
			if (iob[desc].qnext)
				in_queue++;
			if (iob[desc].ref) {
				if (ADDR_OUTSIDE(iob[desc].npdu_data, NVSHM_IPC_BB_BASE + conf->region_ap_data_offset, conf->region_ap_data_size)) {
					fprintf(stdout, "Error: data outside SHM region\n");
					dump = 1;
				}
				if (ADDR_OUTSIDE(iob[desc].npdu_data + iob[desc].data_offset, iob[desc].npdu_data, iob[desc].total_length)) {
					fprintf(stdout, "Error: offset outside data\n");
					dump = 1;
				}
				if (ADDR_OUTSIDE(iob[desc].npdu_data + iob[desc].data_offset + iob[desc].length, iob[desc].npdu_data, iob[desc].total_length)) {
					fprintf(stdout, "Error: offset+length outside data\n");
					dump = 1;
				}
				free_d--;
				chan_use[iob[desc].chan]++;
			}
			if (dump)
				dump_iob(addr, &iob[desc]);
		}
		fprintf(stdout, "\t%d free descriptors\n", free_d);
		if (ndesc - free_d)
			for (chan = 0; chan < chan_count; chan++)
				if (chan_use[chan])
					fprintf(stdout,"\tChannel #%d - %d descriptors\n", chan, chan_use[chan]);
                free(chan_use);
	} else {
		fprintf(stdout, "region_ap_desc_offset (0x%x) not %d bytes aligned\n", conf->region_ap_desc_offset, sizeof(struct nvshm_iobuf)/2);
	}

	if ((conf->region_bb_desc_offset % (sizeof(struct nvshm_iobuf)/2)) == 0) {
		int *chan_use;
                int chan_count;

                if (NVSHM_MAJOR(conf->version) <= 2)
                        chan_count = NVSHM_MAX_CHANNELS_2_0;
                else
                        chan_count =  conf->compat.v3.chan_count;

                chan_use = malloc(sizeof(int) * chan_count);

		memset(chan_use, 0, sizeof(chan_use));
		ndesc = conf->region_bb_desc_size / sizeof(struct nvshm_iobuf);
		fprintf(stdout, "\nChecking %d BBC descriptors\n", ndesc);
		iob = (struct nvshm_iobuf *)((long)addr + conf->region_bb_desc_offset);
		free_d = ndesc;

		for (desc = 0; desc < ndesc; desc++) {
			int dump = 0;
			if (iob[desc].pool_id >= NVSHM_AP_POOL_ID) {
				fprintf(stdout, "Error: pool_id invalid (%d)\n", iob[desc].pool_id);
				dump = 1;
			}
			if (iob[desc].qnext)
				in_queue++;
			if (iob[desc].ref) {
				if (ADDR_OUTSIDE((int)iob[desc].npdu_data & ~BBC_CACHED, NVSHM_PRIV_BB_BASE, NVSHM_BBC_SHM_SIZE)) {
					fprintf(stdout, "Warning: data outside IPC region\n");
					dump = 1;
				}
				if (!ADDR_OUTSIDE((int)iob[desc].npdu_data & ~BBC_CACHED, NVSHM_PRIV_BB_BASE + conf->region_bb_data_offset, conf->region_bb_data_size)) {
					ipc_use++;
				}
				free_d--;
				chan_use[iob[desc].chan]++;
			}
			if (dump || dump_all_iobufs)
				dump_iob(addr, &iob[desc]);
		}
		fprintf(stdout, "\t%d free descriptors\n", free_d);
		fprintf(stdout, "\t%d IPC descriptors\n", ipc_use);
		fprintf(stdout, "\t%d non IPC descriptors\n", ndesc-ipc_use-free_d);
		if (ndesc - free_d)
			for (chan = 0; chan < chan_count; chan++)
				if (chan_use[chan])
					fprintf(stdout,"\tChannel #%d - %d descriptors\n", chan, chan_use[chan]);
                free(chan_use);
	} else {
		fprintf(stdout, "region_bb_desc_offset (0x%x) not %d bytes aligned\n", conf->region_bb_desc_offset, sizeof(struct nvshm_iobuf)/2);
	}

	fprintf(stdout, "\nChecking DL queue\n");
	iob = (struct nvshm_iobuf *)((int)addr + conf->queue_bb_offset);
	fprintf(stdout, "DL guard head = 0x%x\n",  conf->queue_bb_offset + NVSHM_IPC_BB_BASE);
	dump_iob(addr, iob);

	if (!iob->qnext) {
		fprintf(stdout, "\tDL queue is empty\n");
	} else {
		struct nvshm_iobuf *bb_iob = iob->qnext;
		int qfree = 0, qsize = 0, qinsert = 0;

		while (bb_iob) {
			struct nvshm_iobuf *leaf, *bb_leaf, *sleaf, *bb_sleaf;

			iob = BB_CONVERT(addr, bb_iob);
			bb_leaf = bb_iob;
			qinsert++;
			while (bb_leaf) {
				leaf = BB_CONVERT(addr, bb_leaf);
				bb_sleaf = bb_leaf;
				while (bb_sleaf) {
					sleaf = BB_CONVERT(addr, bb_sleaf);
					qsize++;
					if (iob->pool_id >= NVSHM_AP_POOL_ID)
						qfree++;
					bb_sleaf = sleaf->sg_next;
				}
				bb_leaf = leaf->next;
			}
			bb_iob = iob->qnext;
		}
		fprintf(stdout, "\tDL queue contain %d iobuf (%d pending free) in %d elements\n", qsize, qfree, qinsert);
		in_queue -= qsize;
	}

	fprintf(stdout, "\nChecking UL queue\n");
	iob = (struct nvshm_iobuf *)((int)addr + (conf->queue_ap_offset & ~BBC_CACHED));
	fprintf(stdout, "UL guard head = 0x%x\n", (conf->queue_ap_offset & ~BBC_CACHED) + NVSHM_IPC_BB_BASE);
	dump_iob(addr, iob);

	if (!iob->qnext) {
		fprintf(stdout, "\tUL queue is empty\n");
	} else {
		struct nvshm_iobuf *bb_iob = iob->qnext;
		int qfree = 0, qsize = 0, qinsert = 0;

		while (bb_iob) {
			struct nvshm_iobuf *leaf, *bb_leaf, *sleaf, *bb_sleaf;

			iob = BB_CONVERT(addr, bb_iob);
			bb_leaf = bb_iob;
			qinsert++;
			while (bb_leaf) {
				leaf = BB_CONVERT(addr, bb_leaf);
				bb_sleaf = bb_leaf;
				while (bb_sleaf) {
					sleaf = BB_CONVERT(addr, bb_sleaf);
					qsize++;
					if (iob->pool_id < NVSHM_AP_POOL_ID)
						qfree++;
					bb_sleaf = sleaf->sg_next;
				}
				bb_leaf = leaf->next;
			}
			bb_iob = iob->qnext;
		}
		fprintf(stdout, "\tUL queue contain %d iobuf (%d pending free) in %d elements\n", qsize, qfree, qinsert);
		in_queue -= qsize;
	}

	if (in_queue)
		fprintf(stdout, "%d elements not found in UL/DLqueues\n", in_queue);
	else
		fprintf(stdout, "No rogue elements found\n");
	return 0;
}

int main(int argc, char *argv[]) {
	int opt;
	int cmd = 0;
	int instance = -1;
	int size = 0;
	char *filename = NULL;
	char *memory = NULL;
	char *offset = NULL;
	char *value = NULL;
	int dump_all_iobufs = 0;
	int ret = -1;

	while (-1 != (opt = getopt(argc, argv, "pnhcadf:i:r:w:m:o:v:"))) {
		switch (opt) {
		case 'p':
			if (cmd)
				goto usage;
			cmd = POLL_CMD;
			break;
		case 'n':
			if (cmd)
				goto usage;
			cmd = NOTIFY_CMD;
			break;
		case 'r':
			if (cmd)
				goto usage;
			cmd = READ_CMD;
			if (sscanf(optarg, "%d", &size) != 1)
				goto usage;
			break;
		case 'w':
			if (cmd)
				goto usage;
			cmd = WRITE_CMD;
			if (sscanf(optarg, "%d", &size) != 1)
				goto usage;
			break;
		case 'd':
			if (cmd)
				goto usage;
			cmd = CONF_CMD;
			break;
		case 'a':
			dump_all_iobufs = 1;
			break;
		case 'c':
			if (cmd)
				goto usage;
			cmd = CHECK_CMD;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'i':
#ifdef ANDROID
			if (sscanf(optarg, "%d", &instance) != 1)
				goto usage;
#else
			fprintf(stderr,"-i option not supported\n");
			goto usage;
#endif
			break;
		case 'm':
			memory = optarg;
			break;
		case 'o':
			offset = optarg;
			break;
		case 'v':
			value = optarg;
			break;
		case 'h':
		default:
			goto usage;
		}
	}
#ifdef ANDROID
	if ((instance >= 0) && (filename != NULL)) {
		fprintf(stderr, "Cannot request -i and -f sources together\n");
		goto usage;
	}

	if ((instance < 0) && (filename == NULL)) {
		fprintf(stderr, "Please specify either -i or -f source\n");
		goto usage;
	}
#else
	if (filename == NULL) {
		fprintf(stderr, "No source file specified\n");
		goto usage;
	}
#endif
	/* Memory sizes are used for coredump split */
	get_mem_size(instance);

	if (!ipc_size || !priv_size) {
		fprintf(stderr,"Could not read SHM regions sizes\n");
		return -1;
	}

	switch(cmd) {
	case POLL_CMD:
		poll_cmd(instance);
		break;
	case NOTIFY_CMD:
		if ((value == NULL))
			goto usage;
		ret = notify_cmd(instance, value);
		break;
	case READ_CMD:
		if ((memory == NULL) || (offset == NULL))
			goto usage;
		ret = read_cmd(instance, memory, offset, size);
		break;
	case WRITE_CMD:
		if ((memory == NULL) || (offset == NULL)|| (value == NULL))
			goto usage;
		ret = write_cmd(instance, memory, offset, value, size);
		break;
	case CONF_CMD:
		ret = conf_cmd(instance, filename);
		break;
	case CHECK_CMD:
		ret = check_cmd(instance, filename, dump_all_iobufs);
		break;
	default:
		fprintf(stderr,"Unknown command\n");
	}
	return ret;
usage:
	print_usage();
	return ret;
}
