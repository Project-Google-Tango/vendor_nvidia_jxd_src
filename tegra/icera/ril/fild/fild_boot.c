/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup fild
 * @{
 */

/**
 * @file fild_boot.c FILD boot server.
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "fild.h"
#include "icera-util.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
/* Acquisition HSI client answer */
#define HSI_FILE_BLCK_ACCEPTED  ((BOOT_REQ_BLOCK_ACCEPT << HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)
#define HSI_FILE_BLCK_REJECTED  ((BOOT_REQ_BLOCK_REJECT << HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)

/* Acquisition UART client answer */
#define UART_FILE_BLCK_ACCEPTED  ((BOOT_REQ_BLOCK_ACCEPT << UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)
#define UART_FILE_BLCK_REJECTED  ((BOOT_REQ_BLOCK_REJECT << UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)

/* Coredump server HSI answer */
#define HSI_DEBUG_BLCK_ACCEPTED ((BOOT_REQ_DEBUG_ACCEPT << HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)
#define HSI_DEBUG_BLCK_REJECTED ((BOOT_REQ_DEBUG_REJECT << HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)

/* Coredump server HSI answer */
#define UART_DEBUG_BLCK_ACCEPTED ((BOOT_REQ_DEBUG_ACCEPT << UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)
#define UART_DEBUG_BLCK_REJECTED ((BOOT_REQ_DEBUG_REJECT << UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)

#define ALIGN32(val) ((val+0x3) & ~0x3)

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/
typedef enum
{
    ACCEPTED,
    REJECTED,
    FAILED
}ReqBlockStatus;

/**
 * Private boot config
 */
typedef struct
{
    /* boot & dbg channels */
    fildChannel channel[FILD_NUM_OF_BOOT_CHANNELS];

    /* boot */
    fildBootState state;
    fildBootFiles files;
    char *plat_config;
    int plat_config_len;
    unsigned int block_size;
    int use_bt3;
    fildShmCtx shm_ctx;
    int chip_type;
    int product_type;

    /* debug (crash dump, core dump...)*/
    char *root_dir;
    int coredump_enabled;
    unsigned int dbg_block_size;

    int align_data;
    int req_size_in_bytes;
    int msg_val_offset_in_bits;

    /* HIF */
    fildHifScheme hif;

} bootConfig;


/** Different file IDs used in protocol for RESPOPEN_EXT_VER:
 *  these files will be transmitted by FILD if compatibility
 *  version is >= VERSION_FILES_BUFFER
 *
 *  Files buffer also copied as is for shared memory boot.
 *
 *  This list is shared with BB. If BB doesn't know received
 *  ProtocolFileId, it is simply ignored. */
typedef enum
{
    PROTO_FILE_ID_0 = 0     /* None... */
    ,PROTO_FILE_ID_PLATCFG  /* platformConfig.xml */
    ,PROTO_FILE_ID_UNLOCK   /* unlock.bin */
} ProtocolFileId;

/* */
typedef struct
{
    ProtocolFileId id;
    unsigned int len;
} RespOpenItemHeader;

/* */
typedef struct
{
    unsigned int ver_compat; /** */
    unsigned int app_offset; /** */
    unsigned int buf_len;    /** */
} ShmBootConfigHeader;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/** FILD boot config */
static bootConfig fild_boot;

#ifndef FILD_DIRECT_IPC
/** Counter for debug during boot or coredump reception */
static int fild_boot_block_count;
#endif /* #ifndef FILD_DIRECT_IPC */

static int last_block = false;

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
#ifndef FILD_DIRECT_IPC
static unsigned int ComputeChecksum32(unsigned int* p, int lg)
{
    int i = 0;
    unsigned int chksum = 0;
    for(i = 0; i < lg/4; i++)
    {
        chksum ^= *(p++);
    }

    return chksum;
}

static unsigned int ComputeChecksum16(unsigned short * p, int lg)
{
    unsigned short chksum = 0;
    unsigned short * end = p + lg/2;

    while(p < end)
    {
        chksum ^= *p++;
    }

    return(unsigned int)chksum;
}

/**
 * Compute checksum on buffer.
 *
 * Regarding boot HIF settings, deliver checksum32 (HSI) or
 * checksum16 (UART)
 *
 * @param p    buffer to compute
 * @param lg   length of buffer in bytes.
 *
 * @return unsigned int checksum of buffer
 */
static unsigned int ComputeChecksum(void *p, int lg, int offset)
{
    int checksum = 0;
    switch(fild_boot.req_size_in_bytes)
    {
    case HSI_REQ_SIZE_IN_BYTES:
        checksum = ComputeChecksum32(((unsigned int *)p) + offset, lg);
        break;
    case UART_REQ_SIZE_IN_BYTES:
        checksum = ComputeChecksum16(((unsigned short *)p) + offset, lg);
        break;
    }
    return checksum;
}

/**
 * Close a boot channel if opened
 *
 * @param channel
 */
static void CloseBootChannel(int index)
{
    if(fild_boot.channel[index].handle != -1)
    {
        ALOGI("%s : Closing %s\n", __FUNCTION__, fild_boot.channel[index].name);
        fild_Close(fild_boot.channel[index].handle);
        fild_boot.channel[index].handle = -1;
        ALOGI("%s: %s closed.\n", __FUNCTION__, fild_boot.channel[index].name);
    }
}

/**
 * Read file header
 *
 * @param arch_hdr   storage buffer
 * @param arch_fd    file descriptor
 *
 * @return int
 */
static int GetArchiveHeader(tAppliFileHeader *arch_hdr, int arch_fd)
{
    int len = -1, ret;
    int tag_length_size = sizeof(arch_hdr->tag) + sizeof(arch_hdr->length);

    do
    {
        /* Read at the beginning of the file */
        ret = fild_Lseek(arch_fd, 0, SEEK_SET);
        if(ret == -1)
        {
            ALOGE("%s: seek error: %s\n", __FUNCTION__, fild_LastError());
            break;
        }

        /* 1st read tag/length */
        ret = fild_Read(arch_fd, arch_hdr, tag_length_size);
        if(ret != tag_length_size)
        {
            ALOGE("%s: tag/length read error: %s\n", __FUNCTION__, fild_LastError());
            break;
        }

        /* Read remaining of the header */
        ret = fild_Lseek(arch_fd, 0, SEEK_SET);
        if(ret == -1)
        {
            ALOGE("%s: seek error: %s\n", __FUNCTION__, fild_LastError());
            break;
        }

        ret = fild_Read(arch_fd, arch_hdr, arch_hdr->length);
        if(ret != (int)arch_hdr->length)
        {
            ALOGE("%s: read error: %s\n", __FUNCTION__, fild_LastError());
            break;
        }
        len = arch_hdr->length;

        int arch_id = GET_ARCH_ID(arch_hdr->file_id);
        ALOGD("Archive header, TAG      : 0x%x\n", arch_hdr->tag);
        ALOGD("Archive header, LENGTH   : %d bytes\n", arch_hdr->length);
        ALOGD("Archive header, FILE SIZE: %d bytes\n", arch_hdr->file_size);
        ALOGD("Archive header, FILE ID:   %d\n", arch_id);
    } while(0);

    return len;
}
#endif /* #ifndef FILD_DIRECT_IPC */

/**
 * Ensure blockSize is always aligned to a 32-bits boundary
 *
 * @param blockSize block size updated once 32-bits aligned
 * @param requiredBlockSize expected block size
 */
static void AlignBlockSize(unsigned int *blockSize, int requiredBlockSize)
{
    do
    {
        if(fild_boot.align_data && (requiredBlockSize & 0x3))
        {
            /* If requiredBlockSize is not aligned to a 32-bit boundary force it to be aligned and
               calculate the blockSize. */
            *blockSize = (requiredBlockSize & ~(0x3)) + 4;
            break;
        }

        if(!fild_boot.align_data && (requiredBlockSize & 0x1))
        {
            /* blocksize must at least be even number */
            *blockSize = requiredBlockSize + 1;
            break;
        }

        *blockSize = requiredBlockSize;
    } while(0);
}

/**
 * Prepare buffer sent with RESPOPEN_EXT_VER
 *
 * @param buffer NULL buffer (re)allocated regarding amount of
 *               data to transmit. By default, at this moment of
 *               th eboot, it is at least content of platform
 *               config file that is transmitted.
 *
 * @return int size of buffer.
 */
static int PrepareFilesBuffer(char **buffer, int bt2_version)
{
    (void) bt2_version;
    int buf_len = 0, fd, len, ret, item_len, offset, pad_len;
    char filename[MAX_FILENAME_LEN];
    RespOpenItemHeader header;

    /* Platform config is always OK at this moment... */
    len =fild_boot.plat_config_len;

    /** Buffer sent with RESPOPEN_EXT_VER comes with following
     *  format:
        --------------------------
        |         |    ID1       |
        | Header1 |--------------|
        |         |    LEN1      |
        |------------------------|
        | Buffer1 of (LEN1)*bytes|
        |------------------------|
        |  ....                  |
        |------------------------|
        |         |    IDX       |
        | HeaderX |--------------|
        |         |    LENX      |
        |------------------------|
        | BufferX of (LENX)*bytes|
        --------------------------
    */

    /** Put plat config in buffer */
    *buffer = malloc(sizeof(header) + len);

    // Put plat config data:
    memcpy(*buffer + sizeof(header), fild_boot.plat_config, len);
    buf_len = sizeof(header) + len;

    /* Add padding for next item 32-bit alignment if required */
    item_len = len;
    pad_len = 0;
    len = buf_len;
    buf_len = ALIGN32(len);
    if(buf_len != len)
    {
        /* Buffer len has been aligned */
        *buffer = realloc(*buffer, buf_len);

        /* Set 0 for padding... */
        pad_len = buf_len -len;
        memset(*buffer+len, 0, pad_len);
    }

    // Put plat config header:
    header.id = PROTO_FILE_ID_PLATCFG;
    header.len = item_len + pad_len;
    memcpy(*buffer, &header, sizeof(header));

    /** Check if unlock certificate is found in file system and
     *  put it in buffer */
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             fild_boot.root_dir, UNLOCK_FILE);
    if(fild_CheckFileExistence(filename) != -1)
    {
        len = fild_GetFileSize(filename);
        if(len == -1)
        {
            ALOGW("%s: fail to read unlock file. %s",
                  __FUNCTION__,
                  fild_LastError());
        }
        else
        {
            // Put content of file:
            fd = fild_Open(filename, O_RDONLY, 0);
            if(fd >= 0)
            {
                // Realloc buffer with file length:
                *buffer = realloc(*buffer, buf_len + sizeof(header) + len);

                ALOGD("%s: will transmit %s content\n",
                     __FUNCTION__,
                     filename);
                ret = fild_Read(fd,
                                    *buffer + buf_len + sizeof(header),
                                len);
                fild_Close(fd);
                if(ret == -1)
                {
                    ALOGE("%s: fail to read unlock file content. %s",
                          __FUNCTION__,
                          fild_LastError());
                }
                else
                {
                    offset = buf_len;
                    item_len = len;
                    buf_len +=  sizeof(header) + item_len;

                    /* Add padding for next item 32-bit alignment if required */
                    pad_len = 0;
                    len = buf_len;
                    buf_len = ALIGN32(len);
                    if(buf_len != len)
                    {
                        /* Buffer len has been aligned */
                        *buffer = realloc(*buffer, buf_len);

                        /* Set 0 for padding... */
                        pad_len = buf_len - len;
                        memset(*buffer+len, 0, pad_len);
                    }

                    // Put unlock certificate header:
                    header.id = PROTO_FILE_ID_UNLOCK;
                    header.len = item_len + pad_len;
                    memcpy(*buffer + offset, &header, sizeof(header));
                }
            }
            else
            {
                /** Will not transmit unlock certificate content... Not
                 *  fatal, but error reported in log. */
                ALOGE("%s: fail to open %s. %s",
                      __FUNCTION__,
                      filename,
                      fild_LastError());
            }
        }
    }

    return buf_len;
}

/**
 * Copy file content at a given addr in memory
 *
 * @param filename file name
 * @param dst      file destination in memory
 *
 * @return int -1 if NOK, > 0 if OK.
 */
static int CopyFileInMemory(char *filename, void *dst)
{
    int fd=-1, ret=-1, num, size=0, bytes_read, remaining_bytes;
    char *buf=NULL;
    char *dest = dst;

    ALOGD("%s: copying %s at 0x%x",
         __FUNCTION__,
         filename,
         (unsigned int)dst);

    /* Open file and get size */
    fd = fild_Open(filename, O_RDONLY, 0);
    if(fd == -1)
    {
        ALOGE("%s: fail to open [%s]. %s",
             __FUNCTION__,
             filename,
             fild_LastError());
        goto end;
    }
    size = fild_Lseek(fd, 0, SEEK_END);
    if(size < 0)
    {
        ALOGE("%s: fail to get file [%s] size. %s",
             __FUNCTION__,
             filename,
             fild_LastError());
        goto end;
    }
    fild_Lseek(fd, 0, SEEK_SET);

    /* Copy file in memory */
    remaining_bytes = size;
    num = 16384; /* 16K chunks */
    buf = malloc(num);
    while(remaining_bytes)
    {
        num = (num <=  remaining_bytes) ? num : remaining_bytes;
        bytes_read = fild_Read(fd, buf, num);
        if(num != bytes_read)
        {
            ALOGE("%s: fail to read %dbytes (read %d) in [%s]. %s",
                 __FUNCTION__,
                 num,
                 bytes_read,
                 filename,
                 fild_LastError());
            goto end;
        }
        memcpy(dest, buf, num);
        dest += num;
        remaining_bytes -= num;
    }
    ret = 1;

end:
    if(buf)
    {
        free(buf);
    }
    if(fd != -1)
    {
        fild_Close(fd);
    }
    return (ret == -1) ? ret: size;
}

#ifndef FILD_DIRECT_IPC
/**
 *  SendRESPOPEN
 *
 * 0xAA for start message, 0x73 to indicate server is opened
 * +
 * Block data size in bytes (64 to 65535). Recommended: 1024.
 * +
 * Archive header
 *
*/
static int SendRESPOPEN (int index, tAppliFileHeader *arch_hdr, unsigned int block_size, fildClient client)
{
    int ok = 0, bytes_written, len=0;
    unsigned int msg;
    int msg_size_in_bytes = fild_boot.req_size_in_bytes;
    char *buf=NULL;
    int handle = fild_boot.channel[index].handle;

    int bt2_version = fild_GetBt2CompatibilityVersion();

    if(fild_boot.plat_config &&
       (client == BOOT_CLIENT_BTX) &&
       (fild_boot.state == FILD_PRIMARY_BOOT))
    {
        /* Discussing with BT2: transmit BOOT_RESP_OPEN_EXT_VER */

        /* Extended RESPOPEN to indicate that message will also contain 2 additional fields after header:
           - 1 field to indicate a len
           - 1 block of aligned len "len" containing buffer of files...
        */
        len =  PrepareFilesBuffer(&buf, bt2_version);

        if(bt2_version)
        {
            ALOGD("%s %d: extended RESPOPEN with compatibility version 0x%x.\n",
                 __FUNCTION__,
                 index,
                 bt2_version);
            msg = ((BOOT_RESP_OPEN_EXT_VER << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);
        }
        else
        {
            ALOGD("%s %d: extended RESPOPEN.\n", __FUNCTION__, index);
            msg = ((BOOT_RESP_OPEN_EXT << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);
        }
    }
    else
    {
        /* Standard RESPOPEN */
        ALOGD("%s %d: standard RESPOPEN.\n", __FUNCTION__, index);
        msg = ((BOOT_RESP_OPEN << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);
    }

    if(fild_boot.align_data)
    {
        /* bootROM requires block size as a multiple of 32bits... */
        block_size = block_size/4;
    }

    do
    {
        if(!arch_hdr)
        {
            ALOGE("%s %d: No archive header.\n", __FUNCTION__, index);
            break;
        }

        /* Send server opened conf */
        bytes_written = fild_WriteOnChannel(handle, &msg, msg_size_in_bytes);
        if(bytes_written != msg_size_in_bytes)
        {
            ALOGE("%s %d: conf send failure: %d.\n",
                 __FUNCTION__,
                 index,
                 bytes_written);
            break;
        }

        /* Send block size ind */
        bytes_written = fild_WriteOnChannel(handle, &block_size, msg_size_in_bytes);
        if(bytes_written != msg_size_in_bytes)
        {
            ALOGE("%s %d: size send failure: %d.\n",
                 __FUNCTION__,
                 index,
                 bytes_written);
            break;
        }

        /* Send header content */
        bytes_written = fild_WriteOnChannel(handle, (char*)arch_hdr, ARCH_HEADER_BYTE_LEN);
        if(bytes_written != ARCH_HEADER_BYTE_LEN)
        {
            ALOGE("%s %d: header send failure: %d\n",
                 __FUNCTION__,
                 index,
                 bytes_written);
            break;
        }

        if(len)
        {
            /* Send files buffer len indication */
            bytes_written = fild_WriteOnChannel(handle, &len, msg_size_in_bytes);
            if(bytes_written != msg_size_in_bytes)
            {
                ALOGE("%s %d: files buffer len send failure: %d %d %d\n",
                     __FUNCTION__,
                     index,
                     bytes_written,
                     len,
                     msg_size_in_bytes);
                break;
            }

            /* Send files buffer */
            bytes_written = fild_WriteOnChannel(handle, buf, len);
            if(bytes_written != len)
            {
                ALOGE("%s %d: files buffer send failure: %d %d\n", __FUNCTION__,
                     index,
                     bytes_written,
                     len);
                break;
            }

            if(bt2_version)
            {
                /* Send version compatibility */
                bytes_written = fild_WriteOnChannel(handle, &bt2_version, sizeof(int));
                if(bytes_written != sizeof(int))
                {
                    ALOGE("%s %d: version compatibility send failure: %d.\n",
                         __FUNCTION__,
                         index,
                         bytes_written);
                    break;
                }
            }
        }

        ok = 1;
    } while(0);

    free(buf);

    return ok;
}

/**
 * WaitForREQBLOCK
 *
 * @param index
 *
 * @return int
 */
static ReqBlockStatus WaitForREQBLOCK (int index)
{
    int ok = 1;
    ReqBlockStatus status = FAILED;
    unsigned int message=0;
    int consecutive_readfailure = 0;
    int handle = fild_boot.channel[index].handle;

    ALOGD("%s (%d)\n", __FUNCTION__, index);

    do
    {
        ok = fild_Poll(handle, (char *)&message, fild_boot.req_size_in_bytes, 1000);
        if(ok == fild_boot.req_size_in_bytes)
        {
            ALOGD("%s %d:Got REQ_BLOCK 0x%x\n", __FUNCTION__, index, message);
            consecutive_readfailure = 0;
            switch(message)
            {
            case HSI_FILE_BLCK_REJECTED:
            case UART_FILE_BLCK_REJECTED:
                ALOGE("%s %d: REQ_BLOCK_REJECT\n", __FUNCTION__, index);
                status = REJECTED;
                break;

            case HSI_FILE_BLCK_ACCEPTED:
            case UART_FILE_BLCK_ACCEPTED:
                status = ACCEPTED;
                break;

            default:
                if (last_block && (index==FILD_PRIMARY_BOOT_CHANNEL))
                {
                    ALOGI("%s: invalid ACK on last block considered as BLOCK ACCEPT...", __FUNCTION__);
                    status = ACCEPTED;
                }
                break;
            }
            break;
        }
        else
        {
            if(ok==0)
            {
                ALOGE("%s: EOF on boot channel", __FUNCTION__);
                break;
            }
            else
            {
                consecutive_readfailure++;
                if(consecutive_readfailure == MAX_BLOCK_REJECT)
                {
                    /* Use the same const here as for block rejection: after 3 consecutive read failure on HIF,
                       stops... */
                    ALOGE("%s %d: read failure 3 consecutive times\n", __FUNCTION__, index);
                    break;
                }
            }
        }
    } while(1);

    return status;
}

/**
 * Handler called if timer set before sending RESPBLOCK fires
 * before REQBLOCK reception.
 *
 * @param num
 */
static void RespBlockTimer(void *args)
{
    (void) args;
    /** Stuck sending RESPBLOCK */
    ALOGE("%s: stuck sending RESPBLOCK.\n", __FUNCTION__);

    /** Close primary boot channel... */
    CloseBootChannel(FILD_PRIMARY_BOOT_CHANNEL);

    /** Cause FILD fatal exit to re-start it */
    fild_SemPost(&fild.exit_sem);
}

/**
 * SendRESPBLOCK
 *
 * @param handle
 * @param data
 * @param data_size
 *
 * @return int
 */
static int SendRESPBLOCK(int index, char *data, int data_size)
{
    int bytes_written;
    unsigned int msg = ((BOOT_RESP_BLOCK << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);
    int msg_size_in_bytes = fild_boot.req_size_in_bytes;
    unsigned int chksum;
    ReqBlockStatus status;
    int consecutive_reject = 0;
    int handle = fild_boot.channel[index].handle;

    fild_boot_block_count++;
    ALOGD("%s %d: %d %d\n", __FUNCTION__, index, fild_boot_block_count, data_size);

    /* Compute the checksum */
    chksum = 0;
    chksum = ComputeChecksum(data, data_size, 0);
    ALOGD("%s %d: block checksum: %d\n", __FUNCTION__, index, chksum);

    do
    {
        status = FAILED;

        /* Send RESPBLOCK conf */
        bytes_written = fild_WriteOnChannel(handle, &msg, msg_size_in_bytes);
        if(bytes_written != msg_size_in_bytes)
        {
            ALOGE("%s %d: conf send failure: %d\n", __FUNCTION__, index, bytes_written);
            break;
        }

        /* Send data block */
        bytes_written = fild_WriteOnChannel(handle, data, data_size);
        if(bytes_written != data_size)
        {
            ALOGE("%s %d: data failure: %d\n", __FUNCTION__, index, bytes_written);
            break;
        }

        /* Send data checksum */
        bytes_written = fild_WriteOnChannel(handle, &chksum, msg_size_in_bytes);
        if(bytes_written != msg_size_in_bytes)
        {
            ALOGE("%s %d: checksum send failure: %d\n", __FUNCTION__, index, bytes_written);
            break;
        }

        status = WaitForREQBLOCK(index);
        if(status == REJECTED)
        {
            /* Will try 2 more times to send rejected block... */
            consecutive_reject++;
            if(consecutive_reject == MAX_BLOCK_REJECT)
            {
                /* 3 consecutive rejects: stop here */
                ALOGE("%s %d: 3 consecutive failures.\n", __FUNCTION__, index);
                break;
            }
        }
        else
        {
            /* Stop here...: it is seither ACCEPTED or FAILED. */
            break;
        }
    }while(1);

    return status;
}

/**
 * SendPackedRESPBLOCK
 *
 * @param handle
 * @param data
 * @param total_bytes
 *
 * @return int
 */
static int SendPackedRESPBLOCK(int index, char *data, int data_size)
{
    int bytes_written;
    unsigned int msg = ((BOOT_RESP_BLOCK << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);
    int msg_size_in_bytes = fild_boot.req_size_in_bytes;
    unsigned int chksum;
    ReqBlockStatus status = FAILED;
    int consecutive_reject = 0;
    int packed_block_size = msg_size_in_bytes + data_size + msg_size_in_bytes; /* message + data + checksum */
    int handle = fild_boot.channel[index].handle;
    fildTimer timer;
    int ret, timer_ret = FILD_TIMER_NONE;

    fild_boot_block_count++;
    ALOGD("%s %d: %d %d\n", __FUNCTION__, index, fild_boot_block_count, data_size);

    /* Compute the checksum */
    chksum = 0;
    chksum = ComputeChecksum(data, data_size, 1);
    ALOGD("%s %d: block checksum: 0x%x\n", __FUNCTION__, index, chksum);

    /* Pack message in buf */
    memcpy(data, &msg, msg_size_in_bytes);

    /* Pack checksum in buf */
    memcpy(data + msg_size_in_bytes + data_size, &chksum, msg_size_in_bytes);

    if(index == FILD_PRIMARY_BOOT_CHANNEL)
    {
        /** We are about to send a RESPBLOCK. On primary boot
         *  channel, discussing with BROM, it could be a long
         *  blocking task: in case BROM went/goes in error
         *  before/during block writting, and since flow control is
         *  enabled, we might depend on BROM reading rate (3 *
         *  32bytes every second) to send entire block...
         *
         * Create a timer that will be armed before any write access.
         * The handler called if timer expires will simply close primary
         * boot channel, this to cause write failure and exit of
         * blocking writting action...
         *
         */
        timer_ret = fild_TimerCreate(&timer, RespBlockTimer);
        if(timer_ret == FILD_TIMER_NONE)
        {
            ALOGE("%s: Fail to create timer. %s\n", __FUNCTION__, fild_LastError());
        }
    }

    do
    {
        status = FAILED;

        if(timer_ret != FILD_TIMER_NONE)
        {
            /** Arm/re-arm a 2 seconds timer */
            ret = fild_TimerStart(timer, 2);
            if(ret == -1)
            {
                ALOGE("%s: Fail to start timer. %s\n", __FUNCTION__, fild_LastError());
            }
        }

        /* Send packed data block */
        bytes_written = fild_WriteOnChannel(handle, data, packed_block_size);
        if(bytes_written != packed_block_size)
        {
            ALOGE("%s %d: data send failure: %d.\n",
                 __FUNCTION__,
                 index,
                 bytes_written);
            break;
        }

        if(timer_ret != FILD_TIMER_NONE)
        {
            /** Write of block went well: stop timer */
            ret = fild_TimerStop(timer);
            if(ret == -1)
            {
                ALOGE("%s: Fail to stop timer. %s\n", __FUNCTION__, fild_LastError());
            }
        }

        status = WaitForREQBLOCK(index);
        if(status == REJECTED)
        {
            /* Will try 2 more times to send rejected block... */
            consecutive_reject++;
            if(consecutive_reject == MAX_BLOCK_REJECT)
            {
                /* 3 consecutive rejects: stop here */
                ALOGE("%s %d: 3 consecutive failures.\n", __FUNCTION__, index);
                break;
            }
        }
        else
        {
            /* Stop here...: it is seither ACCEPTED or FAILED. */
            break;
        }
    }while(1);

    if(timer_ret != FILD_TIMER_NONE)
    {
        fild_TimerDelete(timer);
    }

    return status;
}

/**
 * Get block of coredump data.
 *
 * @param handle
 * @param dump_buffer
 * @param block_size
 *
 * @return int 0 in case of failure, 1 if block received
 *         entirely.
 */
static int ReadREQDEBUGBLOCK(int handle, char *dump_buffer, unsigned int block_size)
{
    int ok = 0, bytes_read = 0, bytes_offset = 0, msg=0;
    int bytes_to_read = block_size;

    /* 1st wait for message tag */
    bytes_read = fild_Poll(handle, (char *)&msg, fild_boot.req_size_in_bytes, 2000);
    if(bytes_read > 0)
    {
        ok = (msg == ((BOOT_REQ_DEBUG_BLOCK << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START));
        if(!ok)
        {
            ALOGE("%s: invalid msg: 0x%x\n", __FUNCTION__, msg);
        }
        else
        {
            /* Then read block... */
            do
            {
                bytes_read = fild_Poll(handle, dump_buffer+bytes_offset, bytes_to_read, 2000);
                if(bytes_read > 0)
                {
                    bytes_to_read -= bytes_read;
                    bytes_offset += bytes_read;
                }
                else
                {
                    ALOGE("%s: data read failure\n", __FUNCTION__);
                    ok = 0;
                    break;
                }
            } while(bytes_to_read > 0);
        }
    }
    else
    {
        ALOGE("%s: start msg read failure. %d\n", __FUNCTION__, bytes_read);
    }

    return ok;
}

/**
 * Get coredump size indication.
 *
 * @param handle
 *
 * @return int 0 if not OK, 1 if BOOT_REQ_DEBUG_SIZE received
 *         successfully.
 */
static int ReadREQDEBUGSIZE(int handle, unsigned int *dump_size)
{
    int ok = 0, bytes_read, msg=0;

    do
    {
        /* Attempt to get message start and message tag. */
        bytes_read = fild_Poll(handle, (char *)&msg, fild_boot.req_size_in_bytes, 100);
        if(bytes_read != fild_boot.req_size_in_bytes)
        {
            ALOGE("%s: msg read failure: %d\n", __FUNCTION__, bytes_read);
            break;
        }

        /* Attempt to get the coredump size. */
        bytes_read = fild_Poll(handle, (char *)dump_size, sizeof(int), 100);
        if(bytes_read != sizeof(int))
        {
            ALOGE("%s: dump size read failure: %d\n", __FUNCTION__, bytes_read);
            break;
        }

        /* Check the message start and message tag */
        ok = (msg == ((BOOT_REQ_DEBUG_SIZE << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START));
        if(!ok)
        {
            ALOGE("%s: invalid msg [0x%x].\n", __FUNCTION__, msg);
        }

        ALOGD("%s: Got 0x%x message.\n", __FUNCTION__, msg);
        ALOGD("%s: Coredump size: 0x%x bytes\n", __FUNCTION__, *dump_size);
    } while(0);

    return ok;
}

/**
 * Indicate client that previous block of coredump data has been
 * rejected.
 *
 * @param handle
 *
 * @return int 0 in case of failure, 1 if message sent
 *         successfully.
 */
static int SendDEBUGBLOCKREJECT(int handle)
{
    int ok, bytes_written;
    unsigned int message = ((BOOT_REQ_DEBUG_BLOCK_REJECT << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);

    bytes_written = fild_WriteOnChannel(handle, &message, fild_boot.req_size_in_bytes);
    ok = (bytes_written == fild_boot.req_size_in_bytes) ? 1:0;
    if(!ok)
    {
        ALOGE("%s: send failure %d.\n", __FUNCTION__, bytes_written);
    }

    return ok;
}

/**
 * Indicate client that previous block of coredump data has been
 * accepted.
 *
 * @param handle
 *
 * @return int 0 in case of failure, 1 if message sent
 *         successfully.
 */
static int SendDEBUGBLOCKACCEPT(int handle)
{
    int ok, bytes_written;
    unsigned int message = ((BOOT_REQ_DEBUG_BLOCK_ACCEPT << fild_boot.msg_val_offset_in_bits) | BOOT_MSG_START);

    bytes_written = fild_WriteOnChannel(handle, &message, fild_boot.req_size_in_bytes);
    ok = (bytes_written == fild_boot.req_size_in_bytes) ? 1:0;
    if(!ok)
    {
        ALOGE("%s: send failure %d.\n", __FUNCTION__, bytes_written);
    }

    return ok;
}


/**
 * Indicate client if coredump is allowed and size to use for
 * block of coredump data transmission.
 *
 * @param handle
 *
 * @return int 0 in case of failure, 1 if message sent
 *         successfully.
 */
static int SendRESPDEBUG(int handle, unsigned int answer, unsigned int block_size)
{
    int ok = 1, bytes_written;

    ALOGD("%s: 0x%x.\n", __FUNCTION__, answer);

    do
    {
        /* Send server answer */
        bytes_written = fild_WriteOnChannel(handle, &answer, fild_boot.req_size_in_bytes);
        if(bytes_written != fild_boot.req_size_in_bytes)
        {
            ALOGE("%s: answer send failure: %d\n", __FUNCTION__, bytes_written);
            ok = 0;
            break;
        }

        /* Send block size ind */
        bytes_written = fild_WriteOnChannel(handle, &block_size, fild_boot.req_size_in_bytes);
        if(bytes_written != fild_boot.req_size_in_bytes)
        {
            ALOGE("%s: block size send failure: %d\n", __FUNCTION__, bytes_written);
            ok = 0;
            break;
        }
    } while(0);

    return ok;
}

/**
 * Get coredump reception confirmation message based on:
 *  - fild_boot.coredump_enabled value (0: disabled, 1: enabled)
 *  - HIF data alignement constraint.
 *
 * @return int the conf to send to client
 */
static int GetCoreDumpConf(void)
{
    int accepted = fild_boot.align_data ? HSI_DEBUG_BLCK_ACCEPTED:UART_DEBUG_BLCK_ACCEPTED;
    int rejected = fild_boot.align_data ? HSI_DEBUG_BLCK_REJECTED:UART_DEBUG_BLCK_REJECTED;
    unsigned int conf = fild_boot.coredump_enabled ? accepted:rejected;
    return conf;
}

/**
 * Receive coredump data from client block per block.
 *
 * @param handle            handle to opened device
 *                          communication channel
 * @param block_size        block size in bytes.
 *
 * @return int 1 on success, 0 on failure.
 */
static int ReceiveCoredump(int handle,
        unsigned int block_size)
{
    int ok = 1, len;
    unsigned int conf, dump_size=0, remaining_bytes;
    int dump_fd = -1;
    char *dump_buffer = NULL;
    int debug_blck_counter = 0;
    int consecutive_failure = 0;

    /* Just received a valid REQDEBUG from Icera's target */
    do
    {
        if(fild_boot.coredump_enabled)
        {
            dump_fd = fild_LogStart();
            if(dump_fd == -1)
            {
                /* File system is not OK for coredump reception: force coredump disabled */
                fild_boot.coredump_enabled = 0;
            }
        }

        /* Send coredump reception confirmation */
        conf = GetCoreDumpConf();
        ok = SendRESPDEBUG(handle, conf, block_size);
        if(!ok)
        {
            break;
        }

        if((conf == HSI_DEBUG_BLCK_REJECTED) || (conf == UART_DEBUG_BLCK_REJECTED))
        {
            ALOGI("%s: Coredump not allowed/possible.\n", __FUNCTION__);
            ok = 0;
            break;
        }

        /* Target is aware that coredump transmission can start and will send its size in REQMEMBLOCK */
        ok = ReadREQDEBUGSIZE(handle, &dump_size);
        if(!ok)
        {
            break;
        }
        dump_buffer = malloc(block_size);

        /* Start transfer */
        if(dump_fd != -1)
        {
            remaining_bytes = dump_size;
            while(remaining_bytes > 0)
            {
                ALOGD("%s: Read dbg block %d\n", __FUNCTION__, debug_blck_counter);
                if(remaining_bytes < block_size)
                {
                    /* We're about to receive last block of data */
                    AlignBlockSize(&block_size, remaining_bytes);
                }

                if(!ReadREQDEBUGBLOCK(handle, dump_buffer, block_size))
                {
                    consecutive_failure++;
                    if(SendDEBUGBLOCKREJECT(handle))
                    {
                        if(consecutive_failure >= MAX_BLOCK_REJECT)
                        {
                            ALOGE("%s: 3 read failures: %d %d\n", __FUNCTION__, debug_blck_counter, block_size);
                            ok = 0;
                            break;
                        }
                    }
                    else
                    {
                        ok = 0;
                        break;
                    }
                }
                else
                {
                    if(SendDEBUGBLOCKACCEPT(handle))
                    {
                        consecutive_failure = 0;
                        debug_blck_counter++;
                        remaining_bytes -= block_size;
                        fild_Write(dump_fd, dump_buffer, block_size);
                    }
                    else
                    {
                        ok = 0;
                        break;
                    }
                }
            }
        }
    } while(0);

    if(dump_buffer)
    {
        free(dump_buffer);
    }
    fild_LogStop(dump_fd);

    return ok;
}

/**
 * Transmit archive file to client block by block.
 *
 * @param index       handle to opened device communication
 *                    channel.
 * @param arch_name   path of file to transmit.
 * @param block_size  block size in bytes.
 * @param client      client type
 *
 * @return int 1 on success, 0 on failure
 */
static int TransmitArchive(int index,
                           char *arch_name,
                           unsigned int block_size,
                           fildClient client)
{
    int ok = 0;
    unsigned char *file_buf = NULL;
    int arch_fd = -1;
    ReqBlockStatus status = FAILED;

    /* Just received a valid REQOPEN from Icera's target: starts now archive transmission */
    while(1)
    {
        /* Open archive */
        arch_fd = fild_Open(arch_name, O_RDONLY, 0);
        if(arch_fd == -1)
        {
            ALOGE("%s %d: Fail to open file [%s]\n", __FUNCTION__, index, arch_name);
            break;
        }

        /*  Get archive header */
        tAppliFileHeader arch_hdr;
        int header_len = GetArchiveHeader(&arch_hdr, arch_fd);
        if(header_len < 0)
        {
            break;
        }

        /* Adapt block_size to arch_hdr.file_size */
        block_size = (block_size <= arch_hdr.file_size) ? block_size : arch_hdr.file_size;

        /* We will send only ARCH_HEADER_BYTE_LEN in RESPOPEN, not the full header */

        /* Remaining extended header if exists will be sent in 1st RESPBLOCK */
        int ext_hdr_len = arch_hdr.length - ARCH_HEADER_BYTE_LEN;
        int data_size = arch_hdr.file_size;
        int num_of_blocks = data_size / block_size;
        int remaining = data_size - (num_of_blocks * block_size);

        ALOGD("File size is [%d] bytes\n", arch_hdr.length + arch_hdr.file_size);
        ALOGD("Transmitted as this:\n");
        ALOGD("  - %d bytes of standard header in a RESPOPEN msg\n", ARCH_HEADER_BYTE_LEN);
        if(GET_ARCH_ID(arch_hdr.file_id) != ARCH_ID_BT2)
        {
            ALOGD("  - %d bytes of extended header in a RESPBLOCK\n", ext_hdr_len);
        }
        ALOGD("  - %d RESPBLOCK of %dbytes \n", num_of_blocks, block_size);
        if(remaining > 0)
        {
            ALOGD("  - 1 last RESPBLOCK of %dbytes \n", remaining);
        }
        fild_boot_block_count = 0;

        ok = SendRESPOPEN (index, &arch_hdr, block_size, client);
        if(!ok)
        {
            /* Client will certainly fail to read RESPOPEN and send another request while
               attempt number is less than MAX_BLOCK_REJECT */
            break;
        }

        if(GET_ARCH_ID(arch_hdr.file_id) != ARCH_ID_BT2)
        {
            /* 1st block sent is extended header only */
            unsigned int aligned_ext_hdr_len;
            ALOGD("%s %d: Sending extended header: %dbytes\n", __FUNCTION__, index, ext_hdr_len);
            AlignBlockSize(&aligned_ext_hdr_len, ext_hdr_len);
            status = SendRESPBLOCK(index, (char *)&arch_hdr.rfu[0], aligned_ext_hdr_len);
            if(status != ACCEPTED)
            {
                /* Client will certainly fail to read RESBLOCK and restart transaction with a new REQOPEN
                   while attempt number is less than MAX_BLOCK_REJECT */
                ok = 0;
                break;
            }
        }

        if (fild_Lseek(arch_fd, arch_hdr.length, SEEK_SET) == -1)
        {
            ok = 0;
            ALOGE("%s: Fail to seek in %s. %s",
                  __FUNCTION__,
                  arch_name,
                  fild_LastError());
            break;
        }

        int remaining_bytes = data_size, to_read;
        file_buf = malloc(fild_boot.req_size_in_bytes + block_size + fild_boot.req_size_in_bytes);
        while(remaining_bytes > 0)
        {
            /* Read the archive block by block and send via HIF */
            if(remaining_bytes < (int)block_size)
            {
                to_read = remaining_bytes;
                AlignBlockSize(&block_size, to_read);
                last_block = true;
            }
            else
            {
                to_read = block_size;
            }
            fild_Read(arch_fd, (char *)file_buf + fild_boot.req_size_in_bytes, to_read);

            status = SendPackedRESPBLOCK(index, (char *)file_buf, block_size);
            last_block = false;
            if(status != ACCEPTED)
            {
                /* Didn't manage to send successfully a RESPBLOCK, even re-send of rejected one
                   Let's see what will be next client request... */
                ALOGE("%s %d: Incomplete transmission.\n", __FUNCTION__, index);
                ok = 0;
                break;
            }
            else
            {
                remaining_bytes -= block_size;
            }
        }

        /* Transmission finished now */
        break;
    };

    if(arch_fd != -1)
    {
        fild_Close(arch_fd);
    }
    if(file_buf)
    {
        free(file_buf);
    }

    return ok;
}

/**
 * Handle client request on a given channel:
 *  - Archive transmission
 *  - Coredump reception
 *
 * @param index index of channel to monitor
 */
static void HandleClientRequest(int index)
{
    int ok = 0, numread, appli = 0, stop = 0;
    int handle = fild_boot.channel[index].handle;
    unsigned int req;
    char *path;
    int coredump = 0;
#ifdef ANDROID
    int tmp_coredump_enabled;
#endif

    ALOGI("\n[boot%d]:Will loop waiting for client request...\n", index);
    do
    {
        /*  Sit in a busy loop until we receive data on opened channel */
        req = 0;
        numread = fild_Read(handle, &req, fild_boot.req_size_in_bytes);
        if(numread == fild_boot.req_size_in_bytes)
        {
            ALOGD("[boot%d]:Got request: 0x%x\n", index, req);
            switch(req)
            {
            case HSI_BT2_ACQ_REQ:
            case UART_BT2_ACQ_REQ:
                /* BB reboot detected */
                fild_boot.state = FILD_PRIMARY_BOOT;

                ALOGI("[boot%d]:Start [%s] transmission.\n", index, fild_boot.files.bt2_path);

                /* Disable power control during the whole boot sequence */
                fild_DisableModemPowerControl();

                /* Disable LP0 during the whole boot sequence */
                fild_WakeLock();

                if((fild_boot.chip_type != ARCH_TAG_8060) &&
                   (fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].scheme == FILD_UART))
                {
                    /** For chip type after 8060, UART BROM boot protocol updated to
                     *  send PRODUCT TYPE right after REQOPEN. Read it...
                     *  TODO: act in consequence if required... */
                     numread = fild_Read(handle, &fild_boot.product_type, sizeof(int));
                     if(numread != sizeof(int))
                     {
                         ALOGE("[boot%d]:Fail to read product type.\n", index);
                         break;
                     }
                     else
                     {
                         ALOGI("[boot%d]: Product type - 0x%x .\n", index, fild_boot.product_type);
                     }
                }

                if(fild_IsFsUsed())
                {
                    /* Disconnect fs server */
                    fild_FsServerDisconnect();
                }

                if(fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].used)
                {
                    /** Close secondary boot channel: this will cause read/write
                     *  error, if 2ndary boot is on going, and cause restart of the
                     *  2ndary thread using this channel. */
                    CloseBootChannel(FILD_SECONDARY_BOOT_CHANNEL);
                }

                /* Start BT2 transmission */
                ok = TransmitArchive(index, fild_boot.files.bt2_path, fild_boot.block_size, BOOT_CLIENT_BROM);
                if(ok)
                {
                    ALOGI("[boot%d]:%s transmitted successfully.\n", index, fild_boot.files.bt2_path);
                }
                else
                {
                    ALOGI("[boot%d]:%s transmission failure.\n", index, fild_boot.files.bt2_path);
                }
                break;

            case HSI_APP_ACQ_REQ:
            case UART_APP_ACQ_REQ:
                /* Same message to handle either BT3 transmission by BT2
                   or
                   APP transmission by either BT2 or BT3.*/
                appli = 1;

                if((index == FILD_PRIMARY_BOOT_CHANNEL) && fild_boot.use_bt3)
                {
                    /* On primary boot channel, it is BT2 asking for BT3... */

                    /** BB will now transmit an appli that will
                     *  enumerate USB: unload/reload USB stack to permit device detection. */
                    fild_UnloadUsbStack();
                    fild_ReloadUsbStack();
                    /* Auto-suspend disabled waiting for USB device connect event */
                    fild_UsbHostAlwaysOnActive();

                    appli = 0;
                    ALOGI("[boot%d]:Start [%s] transmission.\n", index, fild_boot.files.bt3_path);
                    path = fild_boot.files.bt3_path;
                }
                else
                {
                    /* On any channel it is appli now required */
                    ALOGI("[boot%d]:Start %s transmission.\n", index, fild_boot.files.appli_path);
                    path = fild_boot.files.appli_path;
                }
                ok = TransmitArchive(index, path, fild_boot.block_size, BOOT_CLIENT_BTX);

                /* Indicate end of app transmit.
                   In case coredump previously transmitted, if 2ndary channel,
                   it indicates to break now from this loop...
                   Otherwise, at the end of the loop, if 2ndary channel and coredump==1
                   continue channel polling for app transmission */
                coredump = 0;

                if(ok)
                {
                    ALOGI("[boot%d]:%s transmitted successfully.\n", index, path);
                    if(appli)
                    {
#ifdef ANDROID
                        /* Start the RIL daemon */
                        fild_RilStart();
#endif

                        /* On any channel, boot is finished */
                        fild_RestoreModemPowerControl();
                        fild_WakeUnLock();
                        fild_boot.state = FILD_NO_BOOT;
                    }
                    else
                    {
                        /* BT3 just transmitted */
                        fild_boot.state = FILD_SECONDARY_BOOT;
                    }

                    /* Time to release secondary thread */
                    fild_SemPost(&fild.boot_sem);
                }
                else
                {
                    ALOGE("[boot%d]:%s transmission failure.\n", index, path);
                }
                break;

            case HSI_DEBUG_REQ:
            case UART_DEBUG_REQ:
#ifdef ANDROID
                /** Store current coredump enabled value in case it is
                 *  temporarily disabled later...  */
                tmp_coredump_enabled = fild_boot.coredump_enabled;
                if((index == FILD_PRIMARY_BOOT_CHANNEL) &&
                   (fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].scheme == FILD_UART))
                {
                    /** On UART channel, coredump reception can be disabled
                     *  through "persist.fild.uartcoredump" property */
                    char prop[PROPERTY_VALUE_MAX];
                    property_get("persist.fild.uartcoredump", prop, "enabled");
                    if(strcmp(prop, "enabled") != 0)
                    {
                        /* Temporary disable coredump reception over UART */
                        ALOGI("[boot%d]:Coredump reception over UART disabled\n", index);
                        fild_boot.coredump_enabled = 0;
                    }
                }
#endif
                if(fild_boot.coredump_enabled == 0)
                {
                    ALOGI("[boot%d]:Indicates coredump is disabled.\n", index);
                }
                else
                {
                    ALOGI("[boot%d]:Start coredump reception.\n", index);
                }
                coredump = 1;
                ok = ReceiveCoredump(handle,
                                     fild_boot.dbg_block_size);
                if(fild_boot.coredump_enabled)
                {
                    if(ok)
                    {
                        ALOGI("[boot%d]:Coredump received successfully.\n", index);
                    }
                    else
                    {
                        ALOGE("[boot%d]:Coredump reception failure.\n", index);
                    }
                }
#ifdef ANDROID
                /** Put back coredump enabled value in case it was previously
                 *  disabled... */
                fild_boot.coredump_enabled = tmp_coredump_enabled;
#endif
                break;

            default:
                ALOGD("[boot%d]:Unknown boot daemon request: 0x%x\n", index, req);
                break;
            }
        }
        else
        {
            /* This error maybe because channel has been closed... */
            ALOGE("%s: read error",__FUNCTION__);
            stop = 1;
        }

        if(fild_boot.channel[index].handle == -1)
        {
            /** Channel has been closed following error or BB
             *  assert/reset: quit endless loop */
            stop = 1;
        }

        if((index == FILD_SECONDARY_BOOT_CHANNEL) && (coredump == 0))
        {
            /** We've finished app transmission: quit endless loop. */
            stop = 1;
        }

        if(stop)
        {
            ALOGD("[boot%d]: Stop boot channel polling.\n", index);
            break;
        }
    } while(1);
}

/**
 * Boot from serial host interface
 *
 * @param channel
 */
static void SerialBoot(int channel)
{
    int ok=1, ret;

    /* Open boot channel */
    ALOGI("%s %d: Opening channel %s\n", __FUNCTION__, channel, fild_boot.channel[channel].name);
    do
    {
        fild_boot.channel[channel].handle = fild_Open(fild_boot.channel[channel].name, O_RDWR, 0);
        if(fild_boot.channel[channel].handle == -1)
        {
            /** wait 20ms before trying to open in order to avoid cpu
             *  load when channel not available */
            fild_Usleep(20000);
            ok = 0;
        }
        else
        {
            ALOGI("%s %d: %s channel opened.\n", __FUNCTION__, channel, fild_boot.channel[channel].name);
            ok = 1;
            if(fild_boot.channel[channel].scheme == FILD_UART)
            {
                ALOGD("%s %d: Configuring %s at %d\n", __FUNCTION__,
                     channel,
                     fild_boot.channel[channel].name,
                     fild_boot.channel[channel].speed);
                ret = fild_ConfigUart(fild_boot.channel[channel].handle,
                                      fild_boot.channel[channel].speed,
                                      fild_boot.channel[channel].fctrl);
                if(ret == -1)
                {
                    ALOGE("%s %d: Fail to config channel %s. %s.\n", __FUNCTION__, channel, fild_boot.channel[channel].name, fild_LastError());
                    CloseBootChannel(channel);
                    ok = 0;
                }
            }
        }
        if((channel == FILD_SECONDARY_BOOT_CHANNEL)
           && (fild_boot.state != FILD_SECONDARY_BOOT))
        {
            /** For secondary boot server: while trying to open boot
             *  channel, a BB reset has been detected. Skip following, go
             *  to wait on fild.boot_sem  */
            if(fild_boot.channel[channel].handle != -1)
            {
                CloseBootChannel(channel);
            }
            return;
        }
    } while(!ok);

    /* Start handling of boot channel:
       - endless loop if primary boot channel ran as server
       - single loop if secondary boot channel used once for acquisition */
    HandleClientRequest(channel);

    if(channel == FILD_SECONDARY_BOOT_CHANNEL)
    {
        CloseBootChannel(channel);
    }
    else
    {
        if(fild_boot.channel[channel].handle != -1)
        {
            /* Should never get there !!!!! */
            ALOGE("%s: FILD FATAL: boot server stopped.\n", __FUNCTION__);

            /* Send exit signal to main process... */
            fild_SemPost(&fild.exit_sem);
        }
        else
        {
            /** Channel has been closed after a call to CloseBootChannel:
             *  maybe a way of recovering from error on the channel. Do
             *  nothing here and return. */
        }
    }
    return;
}
#endif /* #ifndef FILD_DIRECT_IPC */

/**
 * Get offset of APP wrapped file in private memory window.
 *
 * @param filename
 *
 * @return unsigned int offset. 0 if fail to open file...
 */
static unsigned int ShmGetAppOffset(char *filename)
{
    int ret=0;
    int fd;
    tAppliFileHeader hdr;

    /* open archive */
    fd = fild_Open(filename, O_RDONLY, 0);
    if(fd >= 0)
    {
        /* read header's tag/length */
        fild_Read(fd, &hdr, offsetof(tAppliFileHeader, sign_type));

        /* get zip mode */
        if(GET_ZIP_MODE(hdr.file_id) == ZIP_MODE_NONE)
        {
            /** not compressed: copy at hdr entry address minus header
             *  length: appli will be XIP by BB... */
            ret = (hdr.entry_address & ARCH_ENTRY_ADDR_MASK) - hdr.length;
        }
        else
        {
            ALOGD("%s: compressed archive. 0x%x",
                 __FUNCTION__,
                 hdr.file_id);

            /** compressed archive: copy it at the end of private window
             *  minus place reserved for noninit... keep it 32bits
             *  aligned */
            ALOGD("%s: 0x%x, 0x%x, 0x%x, 0x%x",
                 __FUNCTION__,
                 fild_boot.shm_ctx.private_size,
                 hdr.file_size,
                 hdr.length,
                 fild_boot.shm_ctx.reserved);
            ret = fild_boot.shm_ctx.private_size -
                hdr.file_size -
                hdr.length -
                fild_boot.shm_ctx.reserved;
            ret = (ret & ~(0x3));
        }
        fild_Close(fd);

        ALOGD("%s: offset: 0x%x", __FUNCTION__, ret);
    }
    else
    {
        /* Should never get there... */
        ALOGE("%s: fail to open %s. %s",
              __FUNCTION__,
              filename,
              fild_LastError());
    }

    return ret;
}

/**
 * Push boot config in shm private area
 *
 * @param dst offset in shm.
 *
 * @return int new offset
 */
static int ShmPushBootConfig(void *dst)
{
    unsigned char *dest = dst;
    ShmBootConfigHeader boot_conf_hdr;
    char *config_buf=NULL;
    int ret;

    ALOGD("%s: pushing boot config at 0x%x",
         __FUNCTION__,
         (unsigned int)dst);

    /* push config header */
    boot_conf_hdr.ver_compat = fild_GetBt2CompatibilityVersion();
    boot_conf_hdr.app_offset = ShmGetAppOffset(fild_boot.files.appli_path);
    boot_conf_hdr.buf_len = PrepareFilesBuffer(&config_buf, boot_conf_hdr.ver_compat);
    memcpy(dest, &boot_conf_hdr, sizeof(ShmBootConfigHeader));
    ALOGD("%s: boot config header:\n"
         "     version:    0x%x\n"
         "     app offset: 0x%x\n"
         "     buf len:    0x%x",
         __FUNCTION__,
         boot_conf_hdr.ver_compat,
         boot_conf_hdr.app_offset,
         boot_conf_hdr.buf_len);

    /* pushing config buffer */
    dest += sizeof(ShmBootConfigHeader);
    memcpy(dest, config_buf, boot_conf_hdr.buf_len);
    free(config_buf);

    ret = boot_conf_hdr.app_offset;
    return ret;
}

/**
 * Push boot data in shm private area
 *
 * @return int 0 if OK, 1 if NOK
 */
static int ShmPushData(void)
{
    int offset, ans;
    unsigned char *addr = NULL;

    do
    {
        /** Push  BT2 in shm */
        offset = fild_boot.shm_ctx.secured;
        addr = fild_boot.shm_ctx.private_addr + offset;
        ans = CopyFileInMemory(fild_boot.files.bt2_path,
                               addr);
        if(ans < 0)
        {
            break;
        }
        offset += ans;

        /** Push boot config value in shm */
        addr = fild_boot.shm_ctx.private_addr + offset;
        ans = ShmPushBootConfig(addr);
        if(ans < 0)
        {
            ALOGE("%s: invalid app offset: %d",
                 __FUNCTION__,
                 ans);
            break;
        }

        /** Push APP in shm */
        addr = fild_boot.shm_ctx.private_addr + ans;
        ans = CopyFileInMemory(fild_boot.files.appli_path,
                               addr);
    } while(0);

    return (ans > 0) ? 0:1;
}

/**
 *  Get BB coredump from SHM...
 */
static void ShmGetCoredump(void)
{
    int fd=-1;
    int err=0;

    if(fild_boot.coredump_enabled)
    {
        fd = fild_LogStart();
        if(fd == -1)
        {
            /** File system is not OK for coredump reception: force
             *  coredump disabled  */
            fild_boot.coredump_enabled = 0;
        }
    }

    if(!fild_boot.coredump_enabled)
    {
        ALOGI("%s: Coredump not allowed/possible.\n", __FUNCTION__);
    }
    else
    {
        int bytes_written, size, remaining_bytes, bytes_copied, chunk;
        time_t start, current;
        fildIpcId ipc;
        unsigned char *offset;

        /** Set size to acquire for coredump: size is private + IPC
         *  windows size */
        size = fild_boot.shm_ctx.private_size +
        fild_boot.shm_ctx.ipc_size;
        remaining_bytes = size;
        bytes_copied = 0;
        chunk = 16384; // will copy coredump 16kB per 16kB in file system...

        start = fild_Time();
        while(remaining_bytes)
        {
            current = fild_Time();
            if((current - start) > 7)
            {
                /** We're about to reach BROM WDT expiracy: reset BB in order
                 *  to restart BROM WDT... */
                IceraModemPowerControl(MODEM_POWER_CYCLE, 1);

                /** Wait for next IPC message meaning BROM has restarted: no
                 *  check done on its content... */
                fild_ShmIpcWait(&ipc);
                start = fild_Time();
            }

            /** Next chunk value: */
            chunk = (remaining_bytes > chunk) ? chunk:remaining_bytes;

            /** Next offset to data: */
            if((unsigned int)bytes_copied < fild_boot.shm_ctx.ipc_size)
            {
                /** 1st copy IPC window */
                offset = fild_boot.shm_ctx.ipc_addr
                    + bytes_copied;
                if((unsigned int)(bytes_copied + chunk) > fild_boot.shm_ctx.ipc_size)
                {
                    /* Will copy last remaining bytes from IPC window */
                    chunk = fild_boot.shm_ctx.ipc_size - bytes_copied;
                }
            }
            else
            {
                /** Then copy private window content... */
                offset = fild_boot.shm_ctx.private_addr
                    + bytes_copied
                    - fild_boot.shm_ctx.ipc_size;
            }

            /** Write data in coredump file */
            bytes_written = fild_Write(fd,
                                       offset,
                                       chunk);
            if(bytes_written != chunk)
            {
                ALOGE("%s: failed to log BBC windows (%d/%d %d/%d). %s",
                      __FUNCTION__,
                      bytes_written,
                      chunk,
                      bytes_copied,
                      size,
                      fild_LastError());
                err=1;
                break;
            }

            bytes_copied+=chunk;
            remaining_bytes-=chunk;
        }
    }

    fild_LogStop(fd);

    if(err)
    {
        ALOGW("%s: coredump file may be incomplete/invalid", __FUNCTION__);
    }

    return;
}

/**
 *
 */
static void ShmIpcReadyTimer(void *args)
{
    (void) args;
    /* Didn't get IPC_READY in time: reset MDM... */
    ALOGE("%s: IPC_READY not received.", __FUNCTION__);
    IceraModemPowerControl(MODEM_POWER_CYCLE, 1);
}

/**
 * Boot from shared memory interface.
 */
static void ShmBoot(void)
{
   int err=0;
   fildIpcId ipc;
   fildTimer timer;
   int timer_ret = FILD_TIMER_NONE, ret;

   do
   {
       fild_ShmIpcWait(&ipc);
       switch(ipc)
       {
       case IPC_ID_BOOT_COLD_BOOT_IND:
           ALOGI("%s: Cold boot indication...", __FUNCTION__);
           break;

       case IPC_ID_BOOT_RESTART_FW_REQ:
           LogKernelTimeStamp();
           if (fild_boot.state == FILD_PRIMARY_BOOT)
           {
               /* We didn't even finish copy of f/w
               *  It may be a WDT from BROM...
                  At least no BT2 nor MDM has run: no need to get a coredump */
               ALOGE("%s: Early crash !", __FUNCTION__);
           }
           else
           {
               /* reboot after crash */
               ALOGI("%s: Get coredump.", __FUNCTION__);
               fild_boot.state = FILD_PRIMARY_BOOT;

               /* Stop waiting for IPC ready */
               if(timer_ret != FILD_TIMER_NONE)
               {
                   ret = fild_TimerStop(timer);
                   if(ret == -1)
                   {
                       ALOGE("%s: Fail to stop timer. %s\n", __FUNCTION__, fild_LastError());
                   }
               }

               /* Disable LP0 during the whole boot sequence */
               fild_WakeLock();

               /* Disable power control during the whole boot sequence */
               ALOGD("%s: disable modem power control.", __FUNCTION__);
               fild_DisableModemPowerControl();

               ShmGetCoredump();
           }
           /* no break, we continue with boot sequence */

       case IPC_ID_BOOT_FW_REQ:
           /* reboot */
           ALOGI("%s: Start boot.", __FUNCTION__);
           fild_boot.state = FILD_PRIMARY_BOOT;

           if(ipc == IPC_ID_BOOT_FW_REQ)
           {
               /** Disable LP0 and power control during the whole boot
                *  sequence.
                *  If ipc==IPC_ID_BOOT_RESTART_FW_REQ, it has already been
                *  done... */
               fild_WakeLock();

               ALOGD("%s: disable modem power control.", __FUNCTION__);
               fild_DisableModemPowerControl();
           }

           if(fild_IsFsUsed())
           {
               /* Disconnect fs server */
               ALOGD("%s: disconnect FS server.", __FUNCTION__);
               fild_FsServerDisconnect();
           }

           /* Push BB data in shm private area */
           err = ShmPushData();
           if(err)
           {
               ALOGE("%s: fail to push BB data in shm.", __FUNCTION__);
               break;
           }

           /* Notify BB */
           ALOGI("%s: Firmware ready", __FUNCTION__);
           fild_ShmIpcPost(IPC_ID_BOOT_FW_CONF);

           ALOGI("%s: End boot.", __FUNCTION__);

           /* Start waiting for IPC ready in parallel */
           if(timer_ret == FILD_TIMER_NONE)
           {
               timer_ret = fild_TimerCreate(&timer, ShmIpcReadyTimer);
               if (timer_ret == FILD_TIMER_NONE)
               {
                   ALOGE("%s: Fail to create timer. %s\n", __FUNCTION__, fild_LastError());
               }
           }
           if (timer_ret != FILD_TIMER_NONE)
           {
               /* Will wait for 40s */
               ret = fild_TimerStart(timer, 40);
               if(ret == -1)
               {
                   ALOGE("%s: Fail to start timer. %s\n", __FUNCTION__, fild_LastError());
               }
           }

           break;

       case IPC_ID_BOOT_FW_CONF:
           /* BB is booting... */
           ALOGD("%s: Boot on going...", __FUNCTION__);
           fild_boot.state = FILD_SECONDARY_BOOT;
           break;

       case IPC_ID_READY:
           /* BB has booted... */
           ALOGI("%s: boot successfull.", __FUNCTION__);
           fild_boot.state = FILD_NO_BOOT;

           /* Stop waiting for IPC ready */
           if(timer_ret != FILD_TIMER_NONE)
           {
               ret = fild_TimerStop(timer);
               if(ret == -1)
               {
                   ALOGE("%s: Fail to stop timer. %s\n", __FUNCTION__, fild_LastError());
               }
           }

#ifdef ANDROID
           /* Start the RIL daemon */
           fild_RilStart();
#endif

           /* Re-enable LP0 again... */
           fild_WakeUnLock();

           /* Restore power control */
           fild_RestoreModemPowerControl();

           /* Time to release secondary thread */
           fild_SemPost(&fild.boot_sem);
           break;

       case IPC_ID_BOOT_ERROR_BT2_HDR:
           ALOGE("%s: Invalid BT2 header.", __FUNCTION__);
           err=1;
           break;

       case IPC_ID_BOOT_ERROR_BT2_SIGN:
           ALOGE("%s: Fail to authenticate BT2.", __FUNCTION__);
           err=1;
           break;

       case IPC_ID_BOOT_ERROR_HWID:
           ALOGE("%s: Invalid HWID.", __FUNCTION__);
           err=1;
           break;

       case IPC_ID_BOOT_ERROR_APP_HDR:
           ALOGE("%s: Invalid APP header.", __FUNCTION__);
           err=1;
           break;

       case IPC_ID_BOOT_ERROR_APP_SIGN:
           ALOGE("%s: Fail to authenticate APP.", __FUNCTION__);
           err=1;
           break;

       case IPC_ID_BOOT_ERROR_INFLATE:
           ALOGE("%s: Fail to inflate APP.", __FUNCTION__);
           err=1;
           break;

       default:
           ALOGI("%s: Unknown IPC: 0x%x", __FUNCTION__, ipc);
       }

       if(err)
       {
           break;
       }
    } while(1);
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
fildBootState fild_GetBootState(void)
{
    return fild_boot.state;
}

void fild_SetBootState(fildBootState state)
{
    fild_boot.state = state;
}

int fild_BootServerInit(fildBoot *boot_cfg,
                        fildHif *hif)
{
    int err = 0;
    fild_boot.state = FILD_NO_BOOT;

    /** Init common boot init data  */
    fild_boot.files.bt2_path = malloc(MAX_FILENAME_LEN);
    strncpy(fild_boot.files.bt2_path, boot_cfg->files.bt2_path, MAX_FILENAME_LEN);
    fild_boot.files.appli_path = malloc(MAX_FILENAME_LEN);
    strncpy(fild_boot.files.appli_path, boot_cfg->files.appli_path, MAX_FILENAME_LEN);
    fild_boot.plat_config_len = boot_cfg->plat_config_len;
    fild_boot.plat_config = malloc(boot_cfg->plat_config_len);
    strncpy(fild_boot.plat_config, boot_cfg->plat_config, boot_cfg->plat_config_len);
    fild_boot.root_dir = malloc(PROPERTY_VALUE_MAX);
    strncpy(fild_boot.root_dir, boot_cfg->root_dir, PROPERTY_VALUE_MAX);
    if(boot_cfg->coredump_enabled)
    {
        fild_boot.coredump_enabled = 1;
        fild_LogInit(boot_cfg->coredump_dir, boot_cfg->max_coredump);
        ALOGD("Coredump dir: %s\n", boot_cfg->coredump_dir);
    }
    else
    {
        fild_boot.coredump_enabled = 0;
        ALOGD("Coredump disabled.\n");
    }
    fild_boot.hif = hif->scheme;

    /* Get BB chip type */
    int fd = fild_Open(fild_boot.files.bt2_path, O_RDONLY, 0);
    if(fd>=0)
    {
        fild_Read(fd, &fild_boot.chip_type, sizeof(int));
        fild_Close(fd);
    }

    /** Init specific HIF boot data */
    switch(fild_boot.hif)
    {
    case FILD_UART:
    case FILD_HSI:
        /* Get fildBoot data and store it in bootConfig */
        strncpy(fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].name,
                boot_cfg->primary_channel,
                MAX_FILENAME_LEN);
        ALOGD("Primary boot channel: %s\n", fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].name);
        if(boot_cfg->secondary_channel)
        {
            strncpy(fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].name,
                    boot_cfg->secondary_channel,
                    MAX_FILENAME_LEN);
            ALOGD("Secondary boot channel: %s\n", fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].name);
        }
        if(boot_cfg->use_bt3)
        {
            fild_boot.use_bt3 = boot_cfg->use_bt3;
            fild_boot.files.bt3_path = malloc(MAX_FILENAME_LEN);
            strncpy(fild_boot.files.bt3_path, boot_cfg->files.bt3_path, MAX_FILENAME_LEN);
        }
        fild_boot.block_size = boot_cfg->block_size;;
        fild_boot.dbg_block_size = boot_cfg->dbg_block_size;

        /* Init boot/dbg channels based on fildHif */
        if(fild_boot.hif == FILD_UART)
        {
            ALOGD("UART boot channel.\n");
            ALOGD("UART baudrate: %d\n", hif->baudrate);
            if(hif->fctrl)
            {
                ALOGD("H/W flow control enabled.\n");
            }
            fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].speed   = hif->baudrate;
            fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].fctrl   = hif->fctrl;
            fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].speed = hif->baudrate;
            fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].fctrl = hif->fctrl;
            fild_boot.align_data = 0;
            fild_boot.req_size_in_bytes = UART_REQ_SIZE_IN_BYTES;
            fild_boot.msg_val_offset_in_bits = UART_MSG_VAL_OFFSET_IN_BITS;
        }
        else
        {
            /* If not UART, then HSI... */
            ALOGD("HSI boot channel.\n");
            fild_boot.align_data = 1;
            fild_boot.req_size_in_bytes = HSI_REQ_SIZE_IN_BYTES;
            fild_boot.msg_val_offset_in_bits = HSI_MSG_VAL_OFFSET_IN_BITS;
        }
        fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].used = 1;
        fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].handle = -1;
        fild_boot.channel[FILD_PRIMARY_BOOT_CHANNEL].scheme = fild_boot.hif;
        if(boot_cfg->secondary_channel)
        {
            fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].used = 1;
            fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].handle = -1;
            fild_boot.channel[FILD_SECONDARY_BOOT_CHANNEL].scheme = fild_boot.hif;
        }

        /* Align different block size(s) */
        unsigned int required_block_size;
        required_block_size = boot_cfg->block_size;
        AlignBlockSize(&fild_boot.block_size, required_block_size);
        fild_boot.block_size &= FILD_MAX_BLOCK_SIZE;
        if(required_block_size != fild_boot.block_size)
        {
            ALOGW("Required block size (%d) aligned to %d\n", required_block_size, fild_boot.block_size);
        }
        ALOGD("Block size: %d\n", fild_boot.block_size);

        required_block_size = boot_cfg->dbg_block_size;
        AlignBlockSize(&fild_boot.dbg_block_size, required_block_size);
        fild_boot.dbg_block_size &= FILD_MAX_DBG_BLOCK_SIZE;
        if(required_block_size != fild_boot.dbg_block_size)
        {
            ALOGW("Required dbg block size (%d) aligned to %d\n", required_block_size, fild_boot.dbg_block_size);
        }
        ALOGD("Debug block size: %d\n", fild_boot.dbg_block_size);
        break;

    case FILD_SHM:
        memcpy(&fild_boot.shm_ctx, &boot_cfg->shm_ctx, sizeof(fildShmCtx));
        err = fild_ShmBootInit(&fild_boot.shm_ctx);
        break;

    case FILD_UNKNOWN:
        ALOGE("%s: unknown HIF scheme.", __FUNCTION__);
        err = 1;
    }

    return err;
}

void fild_BootServer(int channel)
{
    switch(fild_boot.hif)
    {
#ifndef FILD_DIRECT_IPC
        /** We don't need to link this when testing boot from SHM
         *  with software model */
    case FILD_UART:
    case FILD_HSI:
        SerialBoot(channel);
        break;
#endif
    case FILD_SHM:
        ShmBoot();
        /** Error during boot leads here: no need to
         *  continue. Send exit signal to main process... */
        fild_SemPost(&fild.exit_sem);
        break;
    default:
        ALOGE("%s: FILD FATAL should never get there...", __FUNCTION__);

            /* Send exit signal to main process... */
            fild_SemPost(&fild.exit_sem);
        break;
    }

    return;
}
