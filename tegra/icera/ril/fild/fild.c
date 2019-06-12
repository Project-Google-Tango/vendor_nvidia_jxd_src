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
 * @file fild.c Icera Flash Interface Layer Daemon.
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "fild.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#include <getopt.h> /* getopt */
#include <stdlib.h> /* atoi */

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/

/** BB firmware modes used to select valid appli at boot time */
#define MODE_MDM 0
#define MODE_FT  1

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

enum
{
    FILD_NO_ERROR,
    FILD_ARGS_ERROR,
    FILD_FILE_SYSTEM_ERROR,
    FILD_BOOT_ERROR,
    FILD_SYSTEM_ERROR,
    FILD_FATAL_ERROR,
};

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/** Verbosity level used during debug */
int fild_verbosity = FILD_VERBOSE_VERBOSE;

/**
 * Buffers used to store user info transmitted to different boot
 * or fs threads.
 * Set here to free resources in generic function called asap...
 */
static char *bt2_path          = NULL;
static char *bt3_path          = NULL;
static char *appli_path        = NULL;
static char *plat_config       = NULL;
static int plat_config_len     = 0;
static char *root_dir          = NULL;
static char *coredump_dir      = NULL;
static char *fs_channel        = NULL;
static char *primary_channel   = NULL;
static char *secondary_channel = NULL;

/** FILD supported BT2 compatibility version */
static unsigned int bt2_compatibility_version = 0;

/** BBC extmem size */
static unsigned int bbc_extmem_size = 0;

/** Global FILD sync mechanisms */
fildSync fild;

#ifdef ANDROID
/** FILD power control state */
fildModemPowerControl fild_modem_power_control;
#endif

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

static void usage(void)
{
    ALOGI("\n");
    ALOGI("Usage:\n");
    ALOGI("fild <options> \n");
    ALOGI("   -h                         Display this help.\n");
    ALOGI("   -p <dev_name>              Primary boot channel name.\n");
    ALOGI("   -s <dev_name>              Secondary boot channel name.\n"
         "                              Alternative boot channel\n"
         "                              Can be used to communicate either with BT2 or BT3 when\n"
         "                               HIF is different than the one used with primary boot channel.\n");
    ALOGI("   -f <dev_name>              File system channel name.\n");
    ALOGI("   -d <root_dir>              File system root dir\n"
         "                              Default is %s.\n"
#ifdef ANDROID
         "                              Bypassed by read from gsm.modem.fild.rootdir property\n"
#endif
         ,DEFAULT_ICERA_ROOT_DIR);
    ALOGI("   -c <coredump_dir>          Coredump storage directory.\n"
         "                              Default is %s.\n"
         "                              Bypassed by read from persist.modem.logdir property\n",
         DEFAULT_LOGGING_DIR);
    ALOGI("   -b <block_size>            Size of blocks to send/receive boot data (in bytes).\n"
         "                              Default is %d and will be 32bits aligned.\n"
#ifdef ANDROID
         "                              Bypassed by read from gsm.modem.fild.blocksize property\n"
#endif
         ,DEFAULT_DATA_BLOCK_SIZE);
    ALOGI("   --dbg-blksize <block_size> Size of blocks to receive coredump data (in bytes).\n"
         "                              Default is %d and will be 32bits aligned.\n"
#ifdef ANDROID
         "                              Bypassed by read from gsm.modem.fild.dbgblocksize property\n"
#endif
         ,DEFAULT_DEBUG_BLOCK_SIZE);
    ALOGI("   --boot-only                Handle boot only\n");
    ALOGI("   --fs-only                  Handle file system only (for debug purpose)\n");
    ALOGI("   --no-coredump              Force modem full coredump disabled at start-up.\n"
         "                              Default is coredump enabled.\n"
#ifdef ANDROID
         "                              Bypassed by read from persist.modem.log property.\n"
#endif
        );
    ALOGI("   --version <version_num>    Version compatibility number used to align FILD and BB.\n"
         "                              Usually read in firmware used for boot.\n"
         "                              Option added here for FILD usage in fs-only\n");
    ALOGI("   -v <verbosity>             Only when built for debug, otherwise uses LOGx...\n");
    ALOGI("\n");
    ALOGI("HOST INTERFACE SETTINGS\n");
    ALOGI("   --scheme <hif_scheme>  HIF scheme to know if some alignment is required for RX/TX data.\n"
         "                          During boot sequence, protocol words might also be different.\n"
         "                          Available schemes:\n"
         "                           %d : UART (including USB as HIF...)\n"
         "                                No specific alignement is done.\n"
         "                           %d (default): HSI as HIF\n"
         "                                32bits alignment of data\n"
         "                                Different words for boot protocol.\n"
#ifndef WIN32
         "                           %d : Shared memory\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.hif property.\n"
#endif
#endif
         ,FILD_UART
         ,FILD_HSI
#ifndef WIN32
         ,FILD_SHM
#endif
        );
    ALOGI(" *) HSI:\n"
         "   -m <max_size>          Max file system data size per frame (in bytes).\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.maxframesize property\n"
#endif
        );
    ALOGI(" *) UART:\n"
         "   --baudrate <rate>      Indicate baudrate to configure UART channel.\n"
         "                          Default is %d.\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.baudrate property.\n"
#endif
         , DEFAULT_UART_BAUDRATE);
    ALOGI("   --no-fc                Disable hardware flow control.\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.flowcontrol property.\n"
#endif
        );
    ALOGI("   --with-bt3             Tertiary boot application is used for final application acquisition.\n"
         "                          Supported only for UART boot sessions...\n");
    ALOGI("   --hsic                To indicate specific usage of HSIC as USB host interface.\n");
#ifndef WIN32
    ALOGI(" *) SHM:\n"
         "   --shm-priv <path>      Path to shm private memory device folder.\n"
         "                          Default is %s\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.shm.privif property\n"
#endif
         "   --shm-ipc <path>       Path to shm ipc memory device folder.\n"
         "                          Default is %s\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.shm.ipcif property\n"
#endif
         "   --shm-sysfs <path>     Path to shm sysfs folder.\n"
         "                          Default is %s\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.shm.sysfsif property.\n"
#endif
         "   --shm-reserved <size>  Reserved memory area at the top of private memory window.\n"
         "                          Default is 0x%x bytes\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.shm.reserved property.\n"
#endif
         "   --shm-secured <size>   Reserved secured zone at the bottom of private memory window.\n"
         "                          Default is 0x%x bytes\n"
#ifdef ANDROID
         "                          Bypassed by read from gsm.modem.fild.shm.secured property.\n"
#endif
         ,DEFAULT_SHM_PRIV_IF
         ,DEFAULT_SHM_IPC_IF
         ,DEFAULT_SHM_SYSFS_IF
         ,(unsigned int)strtoul(DEFAULT_SHM_RESERVED, NULL, 16)
         ,(unsigned int)strtoul(DEFAULT_SHM_SECURED, NULL, 16));
#endif
    ALOGI("\n");
}

/**
 * Free a buffer resource
 *
 * @param buf
 */
static void FreeBuffer(char *buf)
{
    if(buf)
    {
        free(buf);
    }
}

/**
 * Free FILD resources used at start-up and useless as soon as
 * threads are running...
 *
 */
static void FreeResources(void)
{
    FreeBuffer(bt2_path);
    FreeBuffer(bt3_path);
    FreeBuffer(appli_path);
    FreeBuffer(root_dir);
    FreeBuffer(coredump_dir);
}

/**
 * Get string value between 2 tags in an xml buffer
 *
 * @param xml   buffer of xml value
 * @param start 1st start tag to consider
 * @param end   2nd end tag to consider
 *
 * @return char* string value between 2 tags (buffer to be
 *         released or not by user)
 */
static char *GetTagStrValue(char *xml, char *start, char *end)
{
    char *tmp1, *tmp2, *tag=NULL;
    int len;
    tmp1 = strstr(xml, start); /* pointer right before start tag */
    tmp2 = strstr(xml, end);   /* pointer right before end tag */
    if(tmp1 && tmp2)
    {
        tmp1 += strlen(start); /* pointer right after start tag */
        len = tmp2 - tmp1;
        if(len>0)
        {
            tag = malloc(len+1);
            strncpy(tag, tmp1, len);
            tag[len] = '\0';
        }
    }
    return tag;
}

/**
 * Get offset of BT2 trailer in archive.
 *
 * For 8060 archive, old scheme is applied and trailer offset is
 * found thanks to the BT2 boot map stored right after the
 * archive header.
 * For other archives (9040 and upper...), trailer is found at
 * fix offset (see details below)
 *
 *
 * @param fd        BT2 archive file descriptor
 * @param arch_hdr  Pointer to BT2 archive header.
 *
 * @return unsigned int 0 if failure reading file, offset of
 *         trailer in file if OK.
 */
static unsigned int GetBt2TrailerOffset(int fd,
                               tAppliFileHeader *arch_hdr)
{
    unsigned int offset=0, bytes_read;

    if(arch_hdr->tag == ARCH_TAG_8060)
    {
        /**
         * On 8060 platforms, 1st version of BT2 trailer came with
         * following format:
         *
         *    ----------------------------------
         *   | Ext trailer magic: 4bytes        |
         *    ----------------------------------
         *   | Ext trailer xml data size: 4bytes|
         *    ----------------------------------
         *   | Ext trailer xml data: len of xml |
         *    ----------------------------------
         *
         *  It can be accessed reading BT2 boot map embedded right after
         *  header to know where to find it in BT2 binary (e.g on top of
         *  keyID/NONCE/RSASIG data...)
         */
        ArchBt2BootMap boot_map;
        if (fild_Lseek(fd, arch_hdr->length, SEEK_SET) == -1)
        {
            ALOGE("%s: Fail to seek in BT2 file. %s",
                  __FUNCTION__,
                  fild_LastError());
            goto end;
        }
        bytes_read = fild_Read(fd, &boot_map, sizeof(boot_map));
        if(bytes_read != sizeof(boot_map))
        {
            ALOGE("%s: Fail to read BT2 boot map. %s",
                 __FUNCTION__,
                 fild_LastError());
            goto end;
        }
        offset = arch_hdr->length + (boot_map.dmem_load_addr & ~DMEM_ADDR_MASK) + boot_map.imem_size;
    }
    else
    {
        /**
         * Last version of BT2 trailer come with following format:
         *
         *    ----------------------------------
         *   | Ext trailer magic: 4bytes        |
         *    ----------------------------------
         *   | Ext trailer xml data size: 4bytes|
         *    ----------------------------------
         *   | Ext trailer xml data: len of xml |
         *   | (32bits aligned with padding)    |
         *    ----------------------------------
         *   | Ext trailer size: 4bytes         |
         *    ----------------------------------
         *
         *  Stored on top of keyID/NONCE/RSASIG
         */
        int trailer_size = 0;
        int trailer_size_offset = arch_hdr->length
                                  + arch_hdr->file_size
                                  - RSA_SIGNATURE_SIZE
                                  - NONCE_SIZE
                                  - KEY_ID_BYTE_LEN
                                  - sizeof(int);
        if (fild_Lseek(fd,trailer_size_offset,SEEK_SET) == -1)
        {
            ALOGE("%s: Fail to seek in BT2 file. %s",
                  __FUNCTION__,
                  fild_LastError());
            goto end;
        }
        bytes_read = fild_Read(fd, &trailer_size, sizeof(int));
        if(bytes_read != sizeof(int))
        {
            ALOGE("%s: Fail to read BT2 trailer size", __FUNCTION__);
            goto end;
        }
        offset = trailer_size_offset - trailer_size + sizeof(int);
    }

end:
    return offset;
}

/**
 * Display tags common to ext header & footer from a given
 * buffer of data supposed to be xml data.
 *
 * @param data
 */
static void DisplayExtendedHeaderOrFooter(char *data)
{
    char *tag = NULL;

    tag = GetTagStrValue(data, "<compatibleHwPlatPattern>", "</compatibleHwPlatPattern>");
    if(tag)
    {
        ALOGD("   hwplat: %s", tag);
        free(tag);
    }
    tag = GetTagStrValue(data, "<changelist>", "</changelist>");
    if(tag)
    {
        ALOGD("   changelist: %s", tag);
        free(tag);
    }
    tag = GetTagStrValue(data, "<modified>", "</modified>");
    if(tag)
    {
        ALOGD("   modified: %s", tag);
        free(tag);
    }
    return;
}

/**
 * Read tags value in BT2 trailer.
 * Update static bt2_compatibility_version and bbc_extmem_size with value found.
 * bt2_compatibility_version is never updated with a value
 * higher than LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY.
 *
 * If flag is not found, bt2_compatibility_version is set to 0
 * value.
 * If flag is not found, bbc_extmem_size is set to 0
 * value.
 *
 * @param fd
 * @param arch_hdr
 *
 * @return int
 */
static int ReadBt2Trailer(int fd, tAppliFileHeader *arch_hdr)
{
    int ok = 0, bytes_read;
    ArchBt2ExtTrailerInfo trailer;
    char *trailer_data = NULL;
    unsigned int offset=0;

    offset = GetBt2TrailerOffset(fd, arch_hdr);
    if(offset == 0)
    {
        /** Failure reading in BT2 file... */
        goto end;
    }

    /* Check offset is relevant regarding file size... */
    if(offset > (arch_hdr->length + arch_hdr->file_size))
    {
        ALOGE("%s: invalid trailer offset: 0x%x", __FUNCTION__, offset);
        goto end;
    }

    /* Read BT2 trailer magic & size */
    if (fild_Lseek(fd, offset, SEEK_SET) == -1)
    {
        ALOGE("%s: Fail to seek in BT2 file. %s",
              __FUNCTION__,
              fild_LastError());
        goto end;
    }
    bytes_read = fild_Read(fd, &trailer, sizeof(ArchBt2ExtTrailerInfo));
    if(bytes_read != sizeof(ArchBt2ExtTrailerInfo))
    {
        ALOGE("%s: Fail to read BT2 trailer magic and size.",__FUNCTION__);
        goto end;
    }
    if(trailer.magic != ARCH_TAG_BT2_TRAILER)
    {
        ALOGE("%s: Invalid trailer magic 0x%x",
              __FUNCTION__,
              trailer.magic);
        goto end;
    }

    /* Read trailer data */
    trailer_data = malloc(trailer.size);
    bytes_read = fild_Read(fd, trailer_data, trailer.size);
    if(bytes_read != (int)trailer.size)
    {
        ALOGE("%s: Fail to read BT2 trailer data. %s",
              __FUNCTION__,
              fild_LastError());
        goto end;
    }

    /** Display content of extended trailer */
    DisplayExtendedHeaderOrFooter(trailer_data);

    /** Extract version compatibility */
    char *tag = NULL;
    unsigned int version = 0;
    tag = GetTagStrValue(trailer_data, "<versionCompatibility>", "</versionCompatibility>");
    if(tag)
    {
        version = atoi(tag);
        free(tag);
        ALOGI("BT2 compatibility version: 0x%x\n", version);
    }
    else
    {
        /* Could be old BT2... */
        ALOGD("%s: No version compatibility info in BT2 trailer.\n",
             __FUNCTION__);
    }

    ALOGI("FILD compatibility version: 0x%x\n", LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY);
    if(version)
    {
        /* Update FILD compatibility version */
        bt2_compatibility_version = (version > LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY) ?
                                    LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY : version;
    }
    ALOGI("FILD compatibility version used: 0x%x\n", bt2_compatibility_version);

    /** Extract BB extmem size */
    tag = GetTagStrValue(trailer_data, "<extmemSize>", "</extmemSize>");
    if(tag)
    {
        bbc_extmem_size = atoi(tag);
        free(tag);
        ALOGI("BBC extmem size: 0x%x\n", bbc_extmem_size);
    }
    else
    {
        /* Could be old BT2... */
        ALOGW("%s: No BBC extmem size info found in BT2 trailer.\n",
             __FUNCTION__);
    }
    ok = 1;

end:
    if(trailer_data)
    {
        free(trailer_data);
    }
    return ok;
}

/**
 * Check wrapped file content
 *
 *  1- check header validity:
 *     - arch ID matching
 *     - TODO: arch tag, checksum, extended header,...
 *  2- check file coherency based on header content:
 *     - file size
 *     - format (TODO: signature, keyID, ...)
 *
 * @param filename full file path in file system
 * @param arch_id  expected arch ID
 *
 * @return int 1 if check OK, 0 if KO
 */
static int CheckWrappedFileContent(char *filename, ArchId arch_id)
{
    int fd, bytes_read, ok=0;
    tAppliFileHeader arch_hdr;
    char *ext_hdr = NULL;

    /* Open file to check: file existence already done... */
    fd = fild_Open(filename, O_RDONLY, 0);
    if(fd == -1)
    {
        ALOGE("%s: Fail to open %s. %s",
              __FUNCTION__,
              filename,
              fild_LastError());
        goto end;
    }
    do
    {
        /* Read file header */
        bytes_read = fild_Read(fd, &arch_hdr, ARCH_HEADER_BYTE_LEN);
        if(bytes_read != ARCH_HEADER_BYTE_LEN)
        {
            ALOGE("%s: Fail to read [%s] header: %d : %s\n", __FUNCTION__, filename, bytes_read, fild_LastError());
            break;
        }

        /**
         *  Check file content:
         *   - arch ID
         *   - file size
         *   - TODO...
         */
        /* arch ID */
        if((arch_hdr.file_id & ARCH_ID_MASK) != arch_id)
        {
            ALOGE("%s: [%s] Invalid arch ID %d in header instead of %d.",
                 __FUNCTION__,
                 filename,
                 (arch_hdr.file_id & ARCH_ID_MASK),
                 arch_id);
            break;
        }

        /* file size */
        fild_Lseek(fd, 0, SEEK_SET);
        unsigned int file_size = fild_Lseek(fd, 0, SEEK_END);
        fild_Lseek(fd, 0, SEEK_SET);
        if((arch_hdr.file_size + arch_hdr.length) != file_size)
        {
            ALOGE("%s: [%s] Invalid file size %d instead of %d in header.",
                 __FUNCTION__,
                 filename,
                 file_size,
                 (arch_hdr.file_size + arch_hdr.length));
            break;
        }

        if(arch_id == ARCH_ID_BT2)
        {
            if(ReadBt2Trailer(fd, &arch_hdr) != 1)
            {
                break;
            }
        }

        /* Display ext hdr info for some files... */
        if(((arch_id == ARCH_ID_APP) ||
            (arch_id == ARCH_ID_IFT) ||
            (arch_id == ARCH_ID_BT3)) &&
           (arch_hdr.tag != ARCH_TAG_8060) &&
           (fild_GetBt2CompatibilityVersion() >= VERSION_XML_EXTHDR))
        {
            /* Read extended header */
            bytes_read = fild_Lseek(fd, ARCH_HEADER_BYTE_LEN, SEEK_SET);
            if(bytes_read == -1)
            {
                ALOGE("%s: fail to seek in %s. %s",
                      __FUNCTION__,
                      filename,
                      fild_LastError());
                break;
            }
            ext_hdr = malloc(arch_hdr.length - ARCH_HEADER_BYTE_LEN);
            bytes_read = fild_Read(fd, ext_hdr, arch_hdr.length - ARCH_HEADER_BYTE_LEN);
            if(bytes_read != ((int)arch_hdr.length - ARCH_HEADER_BYTE_LEN))
            {
                ALOGE("%s: Fail to read [%s] ext. header: %d/%d : %s\n",
                      __FUNCTION__,
                      filename,
                      bytes_read,
                      arch_hdr.length - ARCH_HEADER_BYTE_LEN,
                      fild_LastError());
                break;
            }

            /** Display content of extended header */
            DisplayExtendedHeaderOrFooter(ext_hdr);
        }

        /* Set ok if end of tests reached... */
        ok = 1;
    } while(0);

    free(ext_hdr);
    fild_Close(fd);

end:
    return ok;
}

/**
 * Some checks in AP file system:
 *  - look for BT2, IFT and MDM applications
 *  - look for calibration files
 *  - look for platform config file
 *  - look for properties: gsm.modem.fild.mode
 *
 * Report error or set bt2, possible bt3 and appli paths and
 * config string for boot sequence.
 *
 * @param root_dir
 * @param use_bt3
 *
 * @return int application mode (MODE_MDM or MODE_FT) or -1 if error
 */
static int FileSystemVerify(int use_bt3)
{
    int ok = 1;

    int mdm_found = 1;
    int ift_found = 1;
    int plat_cfg_found = 0;
    int fw_mode = MODE_MDM;

    char bt2_usual_name[MAX_FILENAME_LEN];
    char bt3_usual_name[MAX_FILENAME_LEN];
    char mdm_usual_name[MAX_FILENAME_LEN];
    char ift_usual_name[MAX_FILENAME_LEN];
    char filename[MAX_FILENAME_LEN];

#ifdef ANDROID
    char prop[PROPERTY_VALUE_MAX];
#endif

    /* Check for BT2 binary */
    snprintf(bt2_usual_name,
             sizeof(bt2_usual_name),
             "%s%s",
             root_dir, USUAL_MODEM_BT2);
    if(fild_CheckFileExistence(bt2_usual_name) == -1)
    {
        ALOGE("%s: %s - Not found.\n", __FUNCTION__, bt2_usual_name);
        ok = 0;
    }
    else
    {
        ALOGI("BT2: %s", bt2_usual_name);
        ok = CheckWrappedFileContent(bt2_usual_name, ARCH_ID_BT2);
    }

    if(use_bt3)
    {
        /* Check for BT3 binary */
        snprintf(bt3_usual_name,
                 sizeof(bt3_usual_name),
                 "%s%s",
                 root_dir, USUAL_MODEM_BT3);
        if(fild_CheckFileExistence(bt3_usual_name) == -1)
        {
            ALOGE("%s: %s - Not found.\n", __FUNCTION__, bt3_usual_name);
            ok = 0;
        }
        else
        {
            ALOGI("BT3: %s", bt3_usual_name);
            ok = CheckWrappedFileContent(bt3_usual_name, ARCH_ID_BT3);
            if(!ok)
            {
                /* Check old arch ID... */
                ok = CheckWrappedFileContent(bt3_usual_name, ARCH_ID_LDR);
            }

        }
    }

    /* Check for MDM binary */
    snprintf(mdm_usual_name,
             sizeof(mdm_usual_name),
             "%s%s",
             root_dir, USUAL_MODEM_MDM);
    if(fild_CheckFileExistence(mdm_usual_name) == -1)
    {
        mdm_found = 0;
        ALOGW("%s - Not found.\n", mdm_usual_name);
    }

    /* Check for IFT binary */
    snprintf(ift_usual_name,
             sizeof(ift_usual_name),
             "%s%s",
             root_dir, USUAL_MODEM_IFT);
    if(fild_CheckFileExistence(ift_usual_name) == -1)
    {
        ift_found = 0;
        ALOGW("%s - Not found.\n", ift_usual_name);
    }

    /* Check for platform config file */
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, PLATFORM_CONFIG_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        /* No platformConfig to use: check if one can be found in system image */
        char filename2[MAX_FILENAME_LEN];
        snprintf(filename2,
                 sizeof(filename2),
                 "%s%s",
                 root_dir,
                 SYSTEM_PLATFORM_CONFIG_FILE);
        if(fild_CheckFileExistence(filename2) == -1)
        {
            ALOGE("%s: Neither [%s] nor [%s] found.\n",
                  __FUNCTION__,
                  filename,
                  filename2);
            ok = 0;
        }
        else
        {
            /** Copy SYSTEM_PLATFORM_CONFIG_FILE to
             *  PLATFORM_CONFIG_FILE and re-start config_modem
             *  service in charge of populating platform config with
             *  some AP settings on some platforms */
            if (fild_Copy(filename2, filename) == -1)
            {
                ALOGE("%s: fail to copy [%s] to [%s]. %s",
                      __FUNCTION__,
                      filename2,
                      filename,
                      fild_LastError());
                snprintf(filename,
                         sizeof(filename),
                         "%s%s",
                         root_dir,
                         SYSTEM_PLATFORM_CONFIG_FILE);
                ALOGW("%s: use default [%s].", __FUNCTION__, filename);
            }
            else
            {
#ifdef ANDROID
                /** Service used to populate platform config file with some AP
                 *  settings didn't run properly since platform config  wasn't
                 *  found: start it now. */
                property_set("gsm.modem.config", "1");
#endif
            }
            plat_cfg_found = 1;
        }
    }
    else
    {
        plat_cfg_found = 1;
    }

    if (plat_cfg_found)
    {
        /* Get PLATFORM_CONFIG_FILE content... */
        int fd = fild_Open(filename, O_RDONLY, 0);
        if(fd == -1)
        {
            ALOGE("%s: Fail to open %s. %s\n", __FUNCTION__, filename, fild_LastError());
            ok = 0;
        }
        else
        {
            /* Read platform config file */
            int size = fild_Lseek(fd, 0, SEEK_END);
            if(size > 0)
            {
                fild_Lseek(fd, 0, SEEK_SET);
                char *buf = malloc(size+1);
                if(buf)
                {
                    fild_Read(fd, buf, size);

                    if(fild_GetBt2CompatibilityVersion() < VERSION_FULL_PLATCFG)
                    {
                        /* Extract platform config value: the char contained between <hwPlat>[...]</hwPlat> */
                        plat_config = GetTagStrValue(buf, "<hwPlat>", "</hwPlat>");
                        if(plat_config)
                        {
                            if(strlen(plat_config) <= MAX_PLATCFG_LEN)
                            {
                                ALOGI("Platform configuration: %s", plat_config);
                            }
                            else
                            {
                                ALOGE("%s: Platform config indication is too long (%d). Not supported (%d).",
                                      __FUNCTION__,
                                      strlen(plat_config),
                                      MAX_PLATCFG_LEN);
                            }
                            plat_config_len = strlen(plat_config) + 1;
                        }
                        free(buf);
                    }
                    else
                    {
                        /* Transmit entire platform config content as is */
                        plat_config = buf;
                        plat_config[size] = '\0';
                        plat_config_len = size+1;
                        ALOGI("Platform configuration: (entire XML file)\n%s", plat_config);
                    }
                }
                else
                {
                    ALOGE("Platform config buffer alloc failure !!!");
                    ok = 0;
                }
            }
            else
            {
                ALOGE("%s: [%s] with invalid content.",
                     __FUNCTION__,
                     filename);
                ok = 0;
            }
            fild_Close(fd);
        }
    }

    /* Check for calibration file(s) */
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, CALIBRATION_0_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        ALOGI("%s not found.\n", filename);
        snprintf(filename,
                 sizeof(filename),
                 "%s%s",
                 root_dir, CALIBRATION_1_FILE);
        if(fild_CheckFileExistence(filename) == -1)
        {
            /* No cal found: force FT appli start-up */
            fw_mode = MODE_FT;
            ALOGI("%s not found.\n", filename);
        }
    }

    /* Check some data file existence... */
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, DEVICE_CONFIG_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        ALOGW("%s - Not found.\n", filename);
    }
    else
    {
        /* Check content but not source of error, just to have trace... */
        CheckWrappedFileContent(filename, ARCH_ID_DEVICECFG);
    }
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, PRODUCT_CONFIG_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        ALOGW("%s - Not found.\n", filename);
    }
    else
    {
        /* Check content but not source of error, just to have trace... */
        CheckWrappedFileContent(filename, ARCH_ID_PRODUCTCFG);
    }
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, AUDIO_CONFIG_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        ALOGW("%s - Not found.\n", filename);
    }
    else
    {
        /* Check content but not source of error, just to have trace... */
        CheckWrappedFileContent(filename, ARCH_ID_AUDIOCFG);
    }
    snprintf(filename,
             sizeof(filename),
             "%s%s",
             root_dir, IMEI_FILE);
    if(fild_CheckFileExistence(filename) == -1)
    {
        ALOGW("%s - Not found.\n", filename);
    }
    else
    {
        /* Check content but not source of error, just to have trace... */
        CheckWrappedFileContent(filename, ARCH_ID_IMEI);
    }

#ifdef ANDROID
    /* Look for property to eventually select appli path bypassing default behavior */
    int res;
    res = property_get("gsm.modem.fild.mode", prop, "");
    if(res)
    {
        /* Get possible different boot mode in properties */
        if(strcmp(prop, "none"))
        {
            if(!strcmp(prop, "mdm") && mdm_found)
            {
                /* Forced to MDM in properties */
                ALOGI("MDM mode from gsm.modem.fild.mode property\n");
                fw_mode = MODE_MDM;
            }

            if(!strcmp(prop, "ift") && ift_found)
            {
                /* Forced to IFT in properties */
                ALOGI("IFT mode from gsm.modem.fild.mode property\n");
                fw_mode = MODE_FT;
            }

            /* Property used for mode switching, not as persistent setting,
               let's disable propoerty value */
            property_set("gsm.modem.fild.mode", "none");
        }
    }
#endif

    if(ok)
    {
        do
        {
            /* Set bt2 path */
            bt2_path = malloc(MAX_FILENAME_LEN);
            snprintf(bt2_path, MAX_FILENAME_LEN, "%s", bt2_usual_name);
            if(use_bt3)
            {
                /* Set bt3 path */
                bt3_path = malloc(MAX_FILENAME_LEN);
                snprintf(bt3_path, MAX_FILENAME_LEN, "%s", bt3_usual_name);
            }

            /* Set appli path */
            if(fild_GetBt2CompatibilityVersion() >= VERSION_IFT_IN_MDM)
            {
                /* Check fw_mode: whatever it is we need to transmit ARCH_ID_APP */
                if(fw_mode == MODE_FT)
                {
#ifdef ANDROID
                    /* A sys prop may allow to override this default behaviour */
                    property_get("gsm.modem.fild.forceiftapp", prop, "disabled");
                    if(strcmp(prop, "enabled")==0)
                    {
                        /*  Forced to IFT in properties */
                        ALOGI("Force IFT usage from gsm.modem.fild.forceiftapp property\n");
                        fw_mode = MODE_FT;
                    }
                    else
#endif
                    {
                        ALOGD("IFT mode will be handled by MDM application.");
                        fw_mode = MODE_MDM;
                    }
                }
            }
            if(fw_mode == MODE_MDM)
            {
                if(mdm_found)
                {
                    ALOGI("APP: %s", mdm_usual_name);
                    ok = CheckWrappedFileContent(mdm_usual_name, ARCH_ID_APP);
                    if(!ok)
                    {
                        break;
                    }
                    appli_path = malloc(MAX_FILENAME_LEN);
                    snprintf(appli_path, MAX_FILENAME_LEN, "%s", mdm_usual_name);
                }
                else
                {
                    ALOGE("%s: No MDM appli found in file system.\n", __FUNCTION__);
                    ok = 0;
                    break;
                }
            }
            else if(fw_mode == MODE_FT)
            {
                if(ift_found)
                {
                    ALOGI("IFT: %s", ift_usual_name);
                    ok = CheckWrappedFileContent(ift_usual_name, ARCH_ID_IFT);
                    if(!ok)
                    {
                        break;
                    }
                    appli_path = malloc(MAX_FILENAME_LEN);
                    snprintf(appli_path, MAX_FILENAME_LEN, "%s", ift_usual_name);
                }
                else
                {
                    ALOGW("%s: No FT appli found in file system. Will start MDM appli.\n", __FUNCTION__);
                    if(mdm_found)
                    {
                        ALOGI("APP: %s", mdm_usual_name);
                        ok = CheckWrappedFileContent(mdm_usual_name, ARCH_ID_APP);
                        if(!ok)
                        {
                            break;
                        }
                        appli_path = malloc(MAX_FILENAME_LEN);
                        snprintf(appli_path, MAX_FILENAME_LEN, "%s", mdm_usual_name);
                        fw_mode = MODE_MDM;
                    }
                    else
                    {
                        ALOGE("%s: No MDM appli found in file system anymore...\n", __FUNCTION__);
                        ok = 0;
                        break;
                    }
                }
            }
        } while(0);
    }

    if (ok)
    {
        /** Check if f/w update done since next FILD start. If yes
         *  then may do some specific processing before starting
         *  modem */
        fild_CheckFwUpdateDone();
    }

    return ok ? fw_mode : -1;
}

/**
 * Primary thread routine.
 *
 * This routine is always used to handle a boot server.
 *
 * @param arg
 *
 * @return void*
 */
static void *PrimaryThread(void *arg)
{
    (void) arg;
    fild_SemWait(&fild.start_sem);
    do
    {
        fild_BootServer(FILD_PRIMARY_BOOT_CHANNEL);
    } while(1);
    return NULL;
}

/**
 * Secondary thread routine.
 *
 * This routine can be used to handle either boot or file system
 * server(s), both or separately.
 *
 * @param arg
 *
 * @return void*
 */
static void *SecondaryThread(void *arg)
{
    (void) arg;
    fildBootState boot;

    do
    {
        fild_SemWait(&fild.boot_sem);
        boot = fild_GetBootState();
        switch(boot)
        {
        case FILD_NO_BOOT:
            /* OK to start a FS server */
            fild_FsServer();
            break;

        case FILD_SECONDARY_BOOT:
            /* OK to start a boot server */
            fild_BootServer(FILD_SECONDARY_BOOT_CHANNEL);
            break;

        default:
            break;
        }
    } while(1);

    return NULL;
}

#ifdef ANDROID
static void parse_args(char *args, int *argc, char **argv)
{
    int space, arg=0;
    int count = 0;
    char *s = args;

    /* No arg found */
    *argc = count;

    /* Parse args */
    while(*s)
    {
        space = isspace(*s);
        if(space)
        {
            /* Space found, not an arg, will continue.. */
            if(argv && arg)
            {
                /* Found space after an arg, terminate previous one stored in argv */
                *s = '\0';
            }
            /* Indicate arg not found */
            arg = 0;
        }else
        {
            /* Parsing an arg */
            if(!arg)
            {
                /* 1st char of arg: indicate arg found */
                arg = 1;

                /* increment argc */
                count++;

                if(argv)
                {
                    /* Store pointer in argv */
                    argv[count] = s;
                }
            }
        }
        s++;
    }

    *argc = count;

    return;
}

static char **make_argv(char *args, int *argc)
{
    char **argv = NULL;
    char *tok;
    char *s = args;
    int count=0;

    /* 1st parse to count num of args */
    parse_args(args, &count, NULL);
    if(count)
    {
        /* Then parse to get args strings: "count + 1" to keep original argv[0] */
        argv = malloc((count+1)*sizeof(*argv));
        parse_args(args, &count, argv);
    }

    *argc = count+1;
    return argv;
}
#endif

static int main_wrapped(int argc, char *argv[])
{
    int showhelp = 0, i, ret = FILD_NO_ERROR;
    int opt, long_val, long_opt_index = 0;
    fildThread primary_thread;
    fildThread secondary_thread;

    /* Default params values: */
    unsigned int block_size     = DEFAULT_DATA_BLOCK_SIZE;
    unsigned int dbg_block_size = DEFAULT_DEBUG_BLOCK_SIZE;
    int max_frame_size          = DEFAULT_FRAME_SIZE_LIMIT;
    int boot_only               = 0;
    int fs_only                 = 0;
    int coredump_enabled        = 1;
    int max_coredump            = 0;
    int baudrate                = DEFAULT_UART_BAUDRATE;
    int hwflow_control          = 1;
    int use_bt3                 = 0;
    fildHifScheme hif_scheme    = FILD_HSI;
    unsigned int ver_compat     = 0;

    int is_hsic                 = 0;
#ifndef WIN32
    char shmprivif[PROPERTY_VALUE_MAX];
    char shmipcif[PROPERTY_VALUE_MAX];
    char shmsysfsif[PROPERTY_VALUE_MAX];
    char shmreservedstr[PROPERTY_VALUE_MAX];
    int shm_reserved = 0;
    char shmsecuredstr[PROPERTY_VALUE_MAX];
    int shm_secured = 0;
#endif
    char forward[PROPERTY_VALUE_MAX];

    struct option long_options[] = {
        /* long option array, where "name" is case sensitive*/
        {"help", 0, NULL, 'h'},             /* --help or -h will do the same... */
        {"boot-only", 0, &long_val, 'b'},   /* --boot-only  */
        {"fs-only", 0, &long_val, 'f'},     /* --fs-only  */
        {"no-coredump", 0, &long_val, 'c'},    /* --no-coredump  */
        {"scheme", 1, &long_val, 's'},      /* --scheme <hif_scheme> */
        {"baudrate", 1, &long_val, 'r'},    /* --baudrate <rate>  */
        {"no-fc", 0, &long_val, 'n'},       /* --no-fc  */
        {"with-bt3", 0, &long_val, 't'},    /* --with-bt3  */
        {"dbg-blksize", 1, &long_val, 'd'}, /* --dbg-blksize <block size>  */
        {"version", 1, &long_val, 'v'},     /* --version <version number>  */
        {"hsic", 0, &long_val, 'u'},        /* --hsic  */
#ifndef WIN32
        {"shm-priv", 1, &long_val, 'p'},    /* --shm-priv <path> */
        {"shm-ipc", 1, &long_val, 'i'},     /* --shm-ipc <path> */
        {"shm-sysfs", 1, &long_val, 'y'},   /* --shm_sysfs <path> */
        {"shm-reserved", 1, &long_val, 'm'},/* --shm-reserved <size> */
        {"shm-secured", 1, &long_val, 'z'}, /* --shm-secured <size> */
#endif
        {0, 0, 0, 0}                        /* usual array terminating item */
    };

    ALOGI("\nStarting FILD...\n");

#ifdef ANDROID
    fild_RilStartEnable();
    umask(0);

    char fild_args[PROPERTY_VALUE_MAX];
    int newArgc=0;
    char **newArgv=NULL;
    property_get("gsm.modem.fild.args", fild_args, "");
    if(strlen(fild_args) != 0)
    {
        /* Bypass cmd line args with system property */
        ALOGI("Cmd line args to be bypassed with: %s", fild_args);
        newArgv = make_argv(fild_args, &newArgc);
        if(newArgc > 1)
        {
            newArgv[0] = argv[0];
            argv = newArgv;
            argc = newArgc;
        }
    }
#endif

    /* Parse cmd line args */
    opterr = 0;
    while((opt = getopt_long(argc, argv, "hb:c:d:f:m:p:s:v:", long_options, &long_opt_index)) != -1)
    {
        switch(opt)
        {
        case 'b':
            block_size = atoi(optarg);
            break;
        case 'c':
            coredump_dir = malloc(PROPERTY_VALUE_MAX);
            snprintf(coredump_dir, strlen(optarg)+1, "%s", optarg);
            break;
        case 'd':
            root_dir = malloc(PROPERTY_VALUE_MAX);
            snprintf(root_dir, strlen(optarg)+1, "%s", optarg);
            break;
        case 'f':
            fs_channel = optarg;
            break;
        case 'm':
            max_frame_size = atoi(optarg);
            break;
        case 'p':
            primary_channel = optarg;
            break;
        case 's':
            secondary_channel = optarg;
            break;
        case 'v':
            fild_verbosity = atoi(optarg);
            break;
        case 0: /* a "long" option */
            switch(long_val)
            {
            case 'b': /* --boot-only */
                boot_only = 1;
                break;
            case 'c': /* --no-coredump */
                coredump_enabled = 0;
                break;
            case 'd': /* --dbg-blksize */
                dbg_block_size = atoi(optarg);
                break;
            case 'f': /* --fs-only */
                fs_only = 1;
                break;
            case 'n': /* --no-fc */
                hwflow_control = 0;
                break;
            case 't': /* --with-bt3 */
                use_bt3 = 1;
                break;
            case 'r': /* --baudrate <rate> */
                baudrate = atoi(optarg);
                break;
            case 's': /* --scheme <hif_scheme> */
                hif_scheme = atoi(optarg);
                break;
            case 'v': /* --version <version number> */
                ver_compat=strtoul(optarg, NULL, 16);
                break;
            case 'u': /* --hsic */
                is_hsic = 1;
                break;
#ifndef WIN32
            case 'p': /* --shm-priv <path> */
                snprintf(shmprivif, strlen(optarg)+1, "%s", optarg);
                break;
            case 'i': /* --shm-ipc <path> */
                snprintf(shmipcif, strlen(optarg)+1, "%s", optarg);
                break;
            case 'y': /* --shm-sysfs <path> */
                snprintf(shmsysfsif, strlen(optarg)+1, "%s", optarg);
                break;
            case 'm': /* --shm-reserved <size> */
                snprintf(shmreservedstr, strlen(optarg)+1, "%s", optarg);
                shm_reserved = 1;
                break;
            case 'z': /* --shm-secured <size> */
                snprintf(shmsecuredstr, strlen(optarg)+1, "%s", optarg);
                shm_secured = 1;
                break;
#endif
            }
            break;
        case 'h':
            showhelp = 1;
            break;
        default: /* '?' */
            ALOGE("Unknown option\n");
            showhelp = 1;
            break;
        }
    }

#ifdef ANDROID
    if(newArgc > 1)
    {
        /* argv bypassed by system properties: free allocated ressource */
        free(newArgv);
    }

    /* Check system properties that can overwrite cmd line args or some default values. */
    char prop[PROPERTY_VALUE_MAX];
    char prop2[PROPERTY_VALUE_MAX];
    if(root_dir)
    {
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("gsm.modem.fild.rootdir", prop, root_dir);
        strcpy(root_dir, prop);
    }
    else
    {
        /* Either there's a system property, or we use default root dir value */
        root_dir = malloc(PROPERTY_VALUE_MAX);
        property_get("gsm.modem.fild.rootdir", root_dir, DEFAULT_ICERA_ROOT_DIR);
    }

    if(coredump_dir)
    {
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("persist.modem.logdir", prop, coredump_dir);
        strcpy(coredump_dir, prop);
    }
    else
    {
        /* Either there's a system property, or we use default coredump dir value */
        coredump_dir = malloc(PROPERTY_VALUE_MAX);
        property_get("persist.modem.logdir", coredump_dir, DEFAULT_LOGGING_DIR);
    }

    if(coredump_enabled)
    {
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("persist.modem.log", prop, "enabled");
    }
    else
    {
        /* Either there's a system property, or coredump is disabled */
        property_get("persist.modem.log", prop, "disabled");
    }
    if(strcmp(prop, "enabled") == 0)
    {
        coredump_enabled = 1;
    }
    else
    {
        coredump_enabled = 0;
    }

    snprintf(prop2, sizeof(prop2), "%d", max_frame_size);
    property_get("gsm.modem.fild.maxframesize", prop, prop2);
    max_frame_size = atoi(prop);

    snprintf(prop2, sizeof(prop2), "%d", block_size);
    property_get("gsm.modem.fild.blocksize", prop, prop2);
    block_size = atoi(prop);

    snprintf(prop2, sizeof(prop2), "%d", dbg_block_size);
    property_get("gsm.modem.fild.dbgblocksize", prop, prop2);
    dbg_block_size = atoi(prop);

    snprintf(prop2, sizeof(prop2), "%d", hif_scheme);
    property_get("gsm.modem.fild.hif", prop, prop2);
    hif_scheme = atoi(prop);

    property_get("gsm.modem.fild.maxcoredump", prop, DEFAULT_MAX_NUM_OF_CORE_DUMP);
    max_coredump = atoi(prop);

    if(hif_scheme == FILD_UART)
    {
        snprintf(prop2, sizeof(prop2), "%d", baudrate);
        property_get("gsm.modem.fild.baudrate", prop, prop2);
        baudrate = atoi(prop);

        if(!hwflow_control)
        {
            /* hwflow_control disabled on cmd line: do not overwrite if no system property */
            property_get("gsm.modem.fild.flowcontrol", prop, "disabled");
        }
        else
        {
            /* apply property if exists or set default to "enabled" */
            property_get("gsm.modem.fild.flowcontrol", prop, "enabled");
        }
        if(strcmp(prop, "enabled") == 0)
        {
            hwflow_control = 1;
        }
    }

    if(hif_scheme == FILD_SHM)
    {
        property_get("gsm.modem.fild.shm.privif", shmprivif, DEFAULT_SHM_PRIV_IF);
        property_get("gsm.modem.fild.shm.ipcif", shmipcif, DEFAULT_SHM_IPC_IF);
        property_get("gsm.modem.fild.shm.sysfsif", shmsysfsif, DEFAULT_SHM_SYSFS_IF);
        if(shm_reserved)
        {
            /* Bypass or given arg */
            property_get("gsm.modem.fild.shm.reserved", shmreservedstr, shmreservedstr);
        }
        else
        {
            /* Bypass or default */
            property_get("gsm.modem.fild.shm.reserved", shmreservedstr, DEFAULT_SHM_RESERVED);
        }
        if(shm_secured)
        {
            /* Bypass or given arg */
            property_get("gsm.modem.fild.shm.secured", shmsecuredstr, shmsecuredstr);
        }
        else
        {
            /* Bypass or default */
            property_get("gsm.modem.fild.shm.secured", shmsecuredstr, DEFAULT_SHM_SECURED);
        }
    }

    /* Store gsm.modem.powercontrol at init: to be restored at end of boot... */
    fild_modem_power_control=malloc(PROPERTY_VALUE_MAX);
    property_get("gsm.modem.powercontrol", fild_modem_power_control, "enabled");

    /* Get forward args */
    property_get("gsm.modem.fild.forward", forward, "");
#endif

    /* Check params validity */
    if(hif_scheme != FILD_SHM)
    {
        if((boot_only || !fs_only) && !primary_channel)
        {
            ALOGE("No primary boot channel indicated.\n");
            showhelp = 1;
        }
    }
    if((fs_only || !boot_only) && !fs_channel)
    {
        ALOGE("No file system channel indicated.\n");
        showhelp = 1;
    }
    if(fs_only && boot_only)
    {
        ALOGE("Cannot be fs only AND boot only, make a choice...\n");
        showhelp = 1;
    }
    if(primary_channel && secondary_channel)
    {
        if(strcmp(primary_channel, secondary_channel) == 0)
        {
            secondary_channel = NULL;
        }
    }
    if(hif_scheme == FILD_UART)
    {
        int found = 0;
        for(i=0; fild_baudrate[i]>0; i++)
        {
            if(fild_baudrate[i] == baudrate)
            {
                found = 1;
            }
        }
        if(!found)
        {
            ALOGE("Invalid UART baudrate: %d\n", baudrate);
            ALOGE("Supported baudrates:\n");
            for(i=0; fild_baudrate[i]>0; i++)
            {
                ALOGE("%d ", fild_baudrate[i]);
            }
            ALOGE("\n\n");
            showhelp = 1;
        }
    }
    if(use_bt3 && !secondary_channel)
    {
        ALOGE("No secondary boot channel indicated.\n");
        showhelp = 1;
    }
    if(!root_dir)
    {
        ALOGE("No root directory indicated.\n");
        showhelp = 1;
    }
    if(!coredump_dir && coredump_enabled)
    {
        ALOGE("No coredump dir indicated.\n");
        showhelp = 1;
    }

    do
    {
        if(showhelp)
        {
            usage();
            ret = FILD_ARGS_ERROR;
            break;
        }

        /* Check file system and get firmware mode */
        int fw_mode = FileSystemVerify(use_bt3);
        if(fw_mode >= 0)
        {
            if(fw_mode == MODE_FT)
            {
                if(hif_scheme == FILD_UART)
                {
                    if(bt2_compatibility_version >= VERSION_IFT_FS_ACCESS)
                    {
                        /* FS access supported for factory tests application */
                        ALOGD("FILD with IFT FS access.\n");
                    }
                    else
                    {
                        /* Factory tests application doesn't use file system and might use
                            file system channel for AT cmds...
                           Force FILD to work as boot_only daemon. */
                        ALOGI("FILD boot only.\n");
                        boot_only = 1;
                    }
                }
            }
        }
        else
        {
            if(!fs_only)
            {
                ALOGE("Invalid file system content: no boot possible.\n");
                ret = FILD_FILE_SYSTEM_ERROR;
            break;
            }
            else
            {
                ALOGW("Invalid file system content may cause problems");
            }
        }

        /* Init FILD HIF infos */
        fildHif hif;
        hif.scheme   = hif_scheme;
        hif.baudrate = baudrate;
        hif.fctrl    = hwflow_control;

        /* Init FILD sync sems... */
        fild_SemInit(&fild.start_sem, 0, 0);
        fild_SemInit(&fild.exit_sem, 0, 0);
        fild_SemInit(&fild.boot_sem, 0, 0);

        /* Init wake lock framework */
        fild_WakeLockInit();

        if(hif.scheme == FILD_UART)
        {
            /* Init USB stack framework */
            fild_UsbStackInit();
        }

        /* Init file system feature */
        if(!boot_only)
        {
            fildFs fs;
            fs.channel        = fs_channel;
            fs.inbox          = root_dir;
            fs.max_frame_size = max_frame_size;
            fs.is_hsic        = is_hsic;
            fild_FsServerInit(&fs, &hif);
        }

        /* Init forward feature */
        if (strlen(forward))
        {
            fild_UartForwardInit(forward);
        }

        /* Init boot feature */
        if(!fs_only)
        {
            fildBoot boot;
            boot.primary_channel   = primary_channel;
            boot.secondary_channel = secondary_channel;
            boot.files.bt2_path    = bt2_path;
            boot.files.bt3_path    = bt3_path;
            boot.files.appli_path  = appli_path;
            boot.plat_config       = plat_config;
            boot.plat_config_len   = plat_config_len;
            boot.block_size        = block_size;
            boot.use_bt3           = use_bt3;
            boot.root_dir          = root_dir;
            boot.coredump_dir      = coredump_dir;
            boot.coredump_enabled  = coredump_enabled;
            boot.dbg_block_size    = dbg_block_size;
            boot.max_coredump      = max_coredump;
#ifndef WIN32
            strncpy(fild_shm_priv_dev_name, shmprivif, MAX_DIRNAME_LEN);
            strncpy(fild_shm_ipc_dev_name, shmipcif, MAX_DIRNAME_LEN);
            strncpy(fild_shm_sysfs_name, shmsysfsif, MAX_DIRNAME_LEN);
            boot.shm_ctx.reserved = strtoul(shmreservedstr, NULL, 16);
            boot.shm_ctx.secured  = strtoul(shmsecuredstr, NULL, 16);
            /** Indicate BBC extmem size.
                If found in BT2 trailer and not to be bypassed by any cmd line
                arg or system property */
            boot.shm_ctx.extmem_size = (bbc_extmem_size && !shm_reserved) ? bbc_extmem_size:0;
#endif

#ifdef ANDROID
            /* "Power off" modem while initialising boot  */
            IceraModemPowerControlConfig();
            IceraModemPowerControl(MODEM_POWER_OFF, 1);
#endif
            if(fild_BootServerInit(&boot, &hif))
            {
                ALOGE("Boot server init failure");
                ret = FILD_BOOT_ERROR;
                break;
            }
        }

        /* Create threads */
        if(!fs_only)
        {
            /* Create at least a primary server to handle boot */
            if(fild_ThreadCreate(&primary_thread,
                                 PrimaryThread,
                                 NULL) != 0)
            {
                ALOGE("Fail to start boot thread.\n");
                ret = FILD_SYSTEM_ERROR;
                break;
            }
        }
        if((secondary_channel || fs_only) ||
        ((hif.scheme == FILD_SHM) && (fs_channel)))
        {
            /* Create secondary thread */
            if(fild_ThreadCreate(&secondary_thread,
                                 SecondaryThread,
                                 NULL) != 0)
            {
                ALOGE("Fail to start secondary thread.\n");
                ret = FILD_SYSTEM_ERROR;
                break;
            }
        }

        /* Start threads */
        if(fs_only)
        {
            if(ver_compat)
            {
                ALOGI("FILD compatibility version used from cmd line: 0x%x\n", ver_compat);
                bt2_compatibility_version = ver_compat;
            }
            fild_SetBootState(FILD_NO_BOOT);

            /** Post on boot_sem to allow standalone usage of fs
             *  server for debug purpose. */
            fild_SemPost(&fild.boot_sem);
        }
        else
        {

            if(boot_only)
            {
                /** Set FILD fs state to unused for a standalone usage of
                 *  boot server for debug purpose */
                fild_SetFsUnused();
            }

#ifdef ANDROID
            /* "Power on" modem */
            IceraModemPowerControl(MODEM_POWER_ON, 1);

            /* Stop the RIL daemon for now (started on IPC ready, if allowed) */
            fild_RilStop();
            if((fw_mode == MODE_FT) || boot_only) {
                fild_RilStartDisable();
            }
#endif

            /** Post on start_sem to start primary boot thread  */
            fild_SemPost(&fild.start_sem);
        }

        /* Wait for possible exit signal */
        fild_SemWait(&fild.exit_sem);

        ret = FILD_FATAL_ERROR;

    } while(0);

    /* Free resources in case not previoulsy done due to error case... */
    FreeResources();

    return ret;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int fild_GetBt2CompatibilityVersion(void)
{
    return bt2_compatibility_version;
}


int main(int argc, char *argv[])
{
    int res;
    fildSem sleep_sem;

    /* call main function */
    res = main_wrapped(argc, argv);

    if(res != FILD_FATAL_ERROR)
    {
        /* An error occurred : stay here indefinitely */
        fild_SemInit(&sleep_sem, 0, 0);
        ALOGE("FILD initialized with error %d - sleeping...\n", res);
        fild_SemWait(&sleep_sem);
    }
    else
    {
        ALOGE("FILD Fatal Error");
    }

    return res;
}
