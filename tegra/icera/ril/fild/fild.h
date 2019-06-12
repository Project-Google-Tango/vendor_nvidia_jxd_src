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
 * @file fild.h Flash Interface Layer daemon header.
 *
 */

#ifndef FILD_H
#define FILD_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "fild_port.h"
#include "obex_file.h" /* for dir operations types... */

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
/* Max len in bytes for platform config string... */
#define MAX_PLATCFG_LEN 128

/* Misc macros to avoid some target code dependencies...: */
#define ARCH_RFU_FIELD_SZ       0x120
#define ARCH_HEADER_BYTE_LEN    32
#define ARCH_ID_MASK            0x00FFFFFF
#define GET_ARCH_ID(x)          ((x) & ARCH_ID_MASK)
#define ARCH_TAG_BT2_TRAILER    0x1CEB72AB
#define ZIP_MODE_NONE           0
#define ZIP_MODE_MASK           0xFF000000
#define ZIP_MODE_REQ_SHIFT      24
#define GET_ZIP_MODE(x)         ((char)(((x) & ZIP_MODE_MASK) >> ZIP_MODE_REQ_SHIFT))
#define ARCH_ENTRY_ADDR_MASK    0x00FFFFFF
#define RSA_SIGNATURE_SIZE      256
#define NONCE_SIZE              256
#define KEY_ID_BYTE_LEN         4
#define ARCH_TAG_8060           0x1CE8060A
#define DMEM_ADDR_MASK          0x40000000

/* Max data block size for apppli transmission to BB:
   - no more than 0xFFFF (BROM limitation)
   - odd and 32-bit aligned
*/
#define FILD_MAX_BLOCK_SIZE 0xFFF8

/** Max data block size for coredump reception from BB */
#define FILD_MAX_DBG_BLOCK_SIZE 0x8000

/* Default data block size */
#define DEFAULT_DATA_BLOCK_SIZE FILD_MAX_BLOCK_SIZE

/* Default debug (coredump) block size */
#define DEFAULT_DEBUG_BLOCK_SIZE FILD_MAX_DBG_BLOCK_SIZE

/* Default HLP frame max size */
#define DEFAULT_FRAME_SIZE_LIMIT -1 /* no limit by default... */

/* Default max num of coredump files */
#define DEFAULT_MAX_NUM_OF_CORE_DUMP "10"

/* Default UART baudrate */
#define DEFAULT_UART_BAUDRATE 3500000

/* Boot protocol messages. */
#define BOOT_MSG_START                (0x00aa)
#define BOOT_REQ_OPEN                 (0x007f)
#define BOOT_REQ_ARCH                 (0x007c)
#define BOOT_REQ_DEBUG                (0x007b)
#define BOOT_REQ_DEBUG_ACCEPT         (0x0061)
#define BOOT_REQ_DEBUG_REJECT         (0x0069)
#define BOOT_REQ_DEBUG_SIZE           (0x0076)
#define BOOT_REQ_DEBUG_BLOCK          (0x0002)
#define BOOT_REQ_DEBUG_BLOCK_ACCEPT   (0x0051)
#define BOOT_REQ_DEBUG_BLOCK_REJECT   (0x0059)
#define BOOT_RESP_OPEN                (0x0073)
#define BOOT_RESP_OPEN_EXT            (0x0074)
#define BOOT_RESP_OPEN_EXT_VER        (0x0075)
#define BOOT_RESP_BLOCK               (0x0000)
#define BOOT_REQ_BLOCK_ACCEPT         (0x0001)
#define BOOT_REQ_BLOCK_REJECT         (0x0009)

/* Boot client HSI request */
#define HSI_REQ_SIZE_IN_BYTES 4         /** Size of any HSI protocol message: 32 bits.
                                         * 16 lsb bits to contain BOOT_MSG_START, 16 msb to contain value in any upper message */
#define HSI_MSG_VAL_OFFSET_IN_BITS  16  /** Offset of value in message: 16 bits */
#define HSI_BT2_ACQ_REQ ((BOOT_REQ_OPEN <<  HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)   /** REQOPEN sent by boot ROM... */
#define HSI_APP_ACQ_REQ ((BOOT_REQ_ARCH <<  HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)   /** REQARCH sent by BT2... */
#define HSI_DEBUG_REQ   ((BOOT_REQ_DEBUG << HSI_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)   /** REQDEBUG sent by BT2... */

/* Boot client UART request */
#define UART_REQ_SIZE_IN_BYTES 2         /** Size of any UART protocol message: 16 bits.
                                         * 8 lsb bits to contain BOOT_MSG_START, 8 following bits to contain value in any upper message */
#define UART_MSG_VAL_OFFSET_IN_BITS  8  /** Offset of value in message: 8 bits */
#define UART_BT2_ACQ_REQ ((BOOT_REQ_OPEN <<  UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)     /** REQOPEN sent by boot ROM... */
#define UART_APP_ACQ_REQ ((BOOT_REQ_ARCH <<  UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)     /** REQARCH sent by BT2... */
#define UART_DEBUG_REQ   ((BOOT_REQ_DEBUG << UART_MSG_VAL_OFFSET_IN_BITS) | BOOT_MSG_START)     /** REQDEBUG sent by BT2... */

/* Max consecutive acquisition (or coredump) protocol failures */
#define MAX_BLOCK_REJECT 3

/* File system paths */
#define ICERA_APP         "/app"                /** To store apps at 1st boot or during F/W update */
#define ICERA_DATA        "/data"               /** To store any data file, contains subfolders */
#define ICERA_DATA_DEBUG  ICERA_DATA"/debug"    /** To store crash dump generated by modem application or coredump */
#define ICERA_DATA_MODEM  ICERA_DATA"/modem"    /** To store files created by modem application */
#define ICERA_DATA_FACT   ICERA_DATA"/factory"  /** To store calib, imei, and any factory data file like device config */
#define ICERA_DATA_CONFIG ICERA_DATA"/config"   /** To store wrapped config files like audio config, product config */

#define USUAL_MODEM_MDM   ICERA_APP"/modem.wrapped"          /** Modem app */
#define USUAL_MODEM_IFT   ICERA_APP"/factory_tests.wrapped"  /** Factory test app */
#define USUAL_MODEM_BT2   ICERA_APP"/secondary_boot.wrapped" /** Secondary boot app */
#define USUAL_MODEM_BT3   ICERA_APP"/tertiary_boot.wrapped"  /** Tertiary boot app */

#define CALIBRATION_0_FILE   ICERA_DATA_FACT"/calibration_0.bin"    /** Calibration file */
#define CALIBRATION_1_FILE   ICERA_DATA_FACT"/calibration_1.bin"    /** Calibration file back-up copy */
#define PLATFORM_CONFIG_FILE ICERA_DATA_FACT"/platformConfig.xml"   /** Platform config xml file used for modem config */
#define SYSTEM_PLATFORM_CONFIG_FILE ICERA_DATA_CONFIG"/platformConfig.xml"   /** Platform config xml file embedded in system gang image */
#define DEVICE_CONFIG_FILE   ICERA_DATA_FACT"/deviceConfig.bin"     /** Wrapped file containing device customization data */
#define IMEI_FILE            ICERA_DATA_FACT"/imei.bin"             /** Wrapped file containing device IMEI */
#define UNLOCK_FILE          ICERA_DATA_FACT"/unlock.bin"           /** Wrapped unlock certificate generated by Robusta */
#define PRODUCT_CONFIG_FILE  ICERA_DATA_CONFIG"/productConfig.bin"  /** Wrapped file containing product customization data */
#define AUDIO_CONFIG_FILE    ICERA_DATA_CONFIG"/audioConfig.bin"    /** Wrapped file containing audio parameters */

#define FILD_TIMER_NONE -1  /** No timer created */

#define FILE_BLOCK 8192 /** Used for fild_Copy to consider data file block per block */

/**
 *  Dated files stuff
 *
 *  Dated file is always with following format:
 *  <prefix><date>.<extension>
 *  This to allow easier generic parsing of files in different
 *   fs directories.
 *
 */
#define FILE_DATE_FORMAT   "%08d_%06d"  /** yyyymmdd_hhmmss */
#define MAX_TIME_LENGTH    15           /** yyyymmdd_hhmmss */
#define MAX_YYYYMMDD_VALUE 99991231     /* 31st of December 9999: I won't certainly be here to solve 1st of jan 10000 bug.... */
#define MAX_HHMM_VALUE     2359         /* 11:59 PM */

/** Secured zone size at the bottom of private memory window */
#define DEFAULT_SHM_SECURED   "0x1020" /* ((4 * 1024) + 32) */

/** 9MB reserved by default at the top of private memory
 * window in order to avoid overwrite of BB noninit when
 * copying zipped modem application at boot time.
 * This default value may be overwritten by a tag value found
 * in BT2 XML trailer or by cmd line arg or by a system property */
#define DEFAULT_SHM_RESERVED  "0x900000"

/** BT2 version compatibility that may be used to align BB & AP
 *  behavior.
 *
 *  Another version number is read in BT2 transmitted
 *  trailer.
 *  If this version number is higher than
 *  LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY, it is
 *  LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY transmitted to
 *  BT2 during app transmission.
 *  If no version number is found, version is considered to be 0
 *  value.
 *
 *  0 value is never transmitted.
 *
 *  BB knows a version is sent if it is RESPOPEN_EXT_VER
 *  transmitted to answer a REQOPEN.
 *
 *  Version number is a 32 bits hexadecimal value with 24 lsb
 *  bits will be used to define major, minor & patch numbers,
 *  8bits per number.
 *
 */
#define VERSION_IFT_FS_ACCESS 0x00010000 /** 1.0.0: FS access when running IFT c.f nvbug 906891 */
#define VERSION_FILES_BUFFER  0x00010100 /** 1.1.0: RESPOPEN enh with buffer of files c.f nvbug 911103 */
#define VERSION_DIR_OPS       0x00010200 /** 1.2.0: OBEX dir operations capabilities */
#define VERSION_XML_EXTHDR    0x00010300 /** 1.3.0: XML extended headers */
#define VERSION_IFT_IN_MDM    0x00010400 /** 1.4.0: IFT in MDM: always transmit modem.wrapped to BB */
#define VERSION_FULL_PLATCFG  0x00010500 /** 1.5.0: Entire platform config file is transmitted. */
#define VERSION_SHM_OBEX_NOP  0x00010600 /** 1.6.0: SHM: single NUL byte message at FS channel open */

#define LATEST_SUPPORTED_BT2_VERSION_COMPATIBILITY VERSION_SHM_OBEX_NOP

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/**
 *  Archive Id constants
 *
 *  Written in file header for identification by embedded
 *  applications.
 *
 */
typedef enum
{
    ARCH_ID_APP = 0
    ,ARCH_ID_BT2
    ,ARCH_ID_IFT
    ,ARCH_ID_LDR
    ,ARCH_ID_IMEI
    ,ARCH_ID_CUSTCFG
    ,ARCH_ID_RESERVED1
    ,ARCH_ID_RESERVED2
    ,ARCH_ID_AUDIOCFG
    ,ARCH_ID_RESERVED3
    ,ARCH_ID_PLATCFG
    ,ARCH_ID_RESERVED4
    ,ARCH_ID_UNLOCK
    ,ARCH_ID_RESERVED5
    ,ARCH_ID_RESERVED6
    ,ARCH_ID_RESERVED7
    ,ARCH_ID_DEVICECFG
    ,ARCH_ID_PRODUCTCFG
    ,ARCH_ID_RESERVED8
    ,ARCH_ID_RESERVED9
    ,ARCH_ID_RESERVED10
    ,ARCH_ID_BT3
}ArchId;

/**
 * TLV encoded file header
 */
typedef struct
{
    unsigned int  tag;                                  /** DXP Tag: 0x1CE8040A or 0x1CE8060A */
    unsigned int  length;                               /** length of the whole File Header */

    /* Data field */
    unsigned int  file_size;                            /** size of file + size of signature */
    unsigned int  entry_address;                        /** application entry point in RAM */
    unsigned int  file_id;                              /** file identifier */
    unsigned int  sign_type;                            /** Signature type: 0=SHA1, 1=SHA1+RSA */
    unsigned int  checksum;                             /** checksum on TLV including Tag and Length */
    unsigned int  key_index;                            /** ICE-OEM key index used for acrchive signature */
    unsigned char rfu[ARCH_RFU_FIELD_SZ];               /** RFU + Padding (%32) */
} tAppliFileHeader;

/**
 * Secondary boot archive map in memory.
 *
 * Used to access BT2 trailer when BT2 header
 * is not available.
 */
typedef struct
{
    unsigned int imem_start_addr; /* start addr to copy BT2 code in IMEM */
    unsigned int dmem_load_addr;  /* start addr to load BT2 code from DMEM */
    unsigned int imem_size;       /* sizeof BT2 code */
} ArchBt2BootMap;

/**
 * Secondary boot extended trailer infos
 */
typedef struct
{
    unsigned int magic;   /* ARCH_TAG_BT2_TRAILER */
    unsigned int size;    /* Size of data */
} ArchBt2ExtTrailerInfo;

/**
 * Different HIF scheme supported by FILD
 */
typedef enum
{
    FILD_UART,
    FILD_HSI,
    FILD_SHM,
    FILD_UNKNOWN
}fildHifScheme;

/**
 * Different FILD boot channels indexes
 */
typedef enum
{
    FILD_PRIMARY_BOOT_CHANNEL,
    FILD_SECONDARY_BOOT_CHANNEL,
    FILD_NUM_OF_BOOT_CHANNELS
}fildBootChannelIdx;

/**
 * Different FILD's boot client types
 */
typedef enum
{
    BOOT_CLIENT_BROM,   /* Current FILD's boot client is BROM */
    BOOT_CLIENT_BTX,    /* Current FILD's boot client is BT2 or BT3 */
    BOOT_CLIENT_INVALID
}fildClient;

/**
 * FILD channel attributes
 */
typedef struct
{
    int used;                        /** Set at init if channel is required and then used */
    int handle;                      /** Handle returned by fild_Open */
    char name[MAX_FILENAME_LEN];     /** Name of the channel */
    fildHifScheme scheme;            /** HIF scheme */
    int speed;                       /** For UART only: baudrate. */
    int fctrl;                       /** For UART only: h/w flow control enabled (1) or not (0) */
} fildChannel;

/**
 * FILD possible boot states
 */
typedef enum
{
    FILD_NO_BOOT,        /** No boot action on going: OK for fs. */
    FILD_PRIMARY_BOOT,   /** Primary server currently used for boot */
    FILD_SECONDARY_BOOT, /** Secondary server currently used for boot */
} fildBootState;

/**
 * FILD sync mechanisms for the different threads.
 */
typedef struct
{
    fildSem start_sem;           /** Sem used to lock primary boot thread until init complete */
    fildSem exit_sem;            /** Sem used by main process for wait after all threads are started.
                                     Other threads might post on this sem to force main process exit and daemon re-start */
    fildSem boot_sem;            /** Sem to sync with end of boot: blocking for 2ndary server(s) such as 2ndary boot server
                                     or file system server.*/
} fildSync;

/**
 * FILD HIF infos.
 * Different HIF scheme supported with different data alignment
 * constraints.
 */
typedef struct
{
    fildHifScheme scheme;

    /* FILD_UART */
    int baudrate;
    int fctrl;
} fildHif;

/**
 * Path to files used for boot
 */
typedef struct
{
    char *bt2_path;
    char *bt3_path;
    char *appli_path;
} fildBootFiles;

typedef struct
{
    unsigned char *private_addr;    /** start addr of mapped private window */
    unsigned int private_size;      /** size of priv window */

    unsigned char *ipc_addr;        /** start addr of mapped ipc window */
    unsigned int ipc_size;          /** size of ipc window */

    unsigned int reserved;          /** size of reserved area (e.g bb noninit memory) */
    unsigned int secured;           /** size of secured area */
    unsigned int extmem_size;       /** size of bb extmem size (e.g should be (private_size-reserved)) */
}fildShmCtx;

/**
 * Boot config
 */
typedef struct
{
    /* boot & dbg channels */
    char *primary_channel;
    char *secondary_channel;

    /* boot */
    fildBootFiles files;
    char *plat_config;
    int plat_config_len;
    int block_size;
    int use_bt3;
    fildShmCtx shm_ctx;

    /* debug (crash dump, core dump...)*/
    char *root_dir;
    char *coredump_dir;
    int coredump_enabled;
    int dbg_block_size;
    int max_coredump;
} fildBoot;

/**
 * FILD FS config
 */
typedef struct
{
    char *channel;       /* serial channel used for comms */
    char *inbox;         /* OBEX FS inbox (e.g root dir) */
    int max_frame_size;  /* serial max size of data messages */
    int is_hsic;         /* serial HSIC link used */
} fildFs;

/**
 * ID used for messages in IPC mailbox
 */
typedef enum
{
    /** Boot status */
	IPC_ID_BOOT_COLD_BOOT_IND=0x01,  /** AP2BROM: Cold boot indication */
	IPC_ID_BOOT_FW_REQ,              /** BROM2AP: BROM request F/W at cold boot */
	IPC_ID_BOOT_RESTART_FW_REQ,      /** BROM2AP: BROM request F/W after crash
				                   * (e.g IPC_BOOT_COLD_BOOT_IND not found in mailbox) */
	IPC_ID_BOOT_FW_CONF,             /** AP2BROM: F/W ready confirmation */
	IPC_ID_READY,                    /** BB2AP: runtime IPC state after successfull boot */

	/** Boot errors */
	IPC_ID_BOOT_ERROR_BT2_HDR=0x1000,/** BROM2AP: BROM found invalid BT2 header */
	IPC_ID_BOOT_ERROR_BT2_SIGN,      /** BROM2AP: BROM failed to SHA1-RSA authenticate BT2 */
	IPC_ID_BOOT_ERROR_HWID,          /** BT22AP:  BT2 found invalid H/W ID */
	IPC_ID_BOOT_ERROR_APP_HDR,       /** BT22AP:  BT2 found invalid APP header */
	IPC_ID_BOOT_ERROR_APP_SIGN,      /** BT22AP:  BT2 failed to SHA1-RSA authenticate APP */
    IPC_ID_BOOT_ERROR_INFLATE,       /** BT22AP:  BT2 failed to inflate APP */

	IPC_ID_MAX_MSG=0xFFFF            /** Max ID for IPC mailbox message */
} fildIpcId;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/
/**
 * Exhaustive list of supported UART baudrates.
 *
 * Of course, not all of them are to be used for data
 * transfer...
 *
 */
extern const int fild_baudrate[];

/** FILD sync mechanisms */
extern fildSync fild;

/** FILD system property to enable/disable/... modem power
 *  control... */
extern fildModemPowerControl fild_modem_power_control;


/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
/**
 * Return current supported BT2 compatibility version.
 *
 *
 * @return int
 */
int fild_GetBt2CompatibilityVersion(void);

/**
 * Handle OBEX file system server over FILD_FS_CHANNEL.
 *
 */
void fild_FsServer(void);

/**
 * Disconnect OBEX file system server.
 *
 * Used by boot thread, in multi-threaded environment, to
 * re-start file system server after a reboot/reset is detected.
 *
 */
void fild_FsServerDisconnect(void);

/**
 * Init OBEX file system server.
 * Don't start server.
 * To be called only once at init.
 *
 */
void fild_FsServerInit(fildFs *fs_cfg,
                       fildHif *hif);

/**
 * Check if FILD fs is used...
 * Used by boot thread to know if specific action is required
 * during a reboot.
 *
 * @return int 1 if FILD fs is used, 0 if not
 */
int fild_IsFsUsed(void);

/**
 * Inidcate FILD fs is not used for future check.
 * For debug purpose when using FILD for boot only or when
 * starting IFT mode where FILD fs is not supported.
 *
 */
void fild_SetFsUnused(void);

/**
 * Handle boot server over boot channel
 *
 * @param channel FILD_PRIMARY_BOOT_CHANNEL or
 *                FILD_SECONDARY_BOOT_CHANNEL
 */
void fild_BootServer(int channel);

/**
 * Init boot server(s) environment.
 * Don't start any server.
 * To be called once at init.
 *
 * @return 0 if OK, 1 if err.
 *
 */
int fild_BootServerInit(fildBoot *boot_cfg,
                         fildHif *hif);

/**
 *  Get boot state
 *  Used by secondary boot server or fs server to sync with
 *  boot/reboot while accessing boot/fs channel.
 *
 * @return fildBootState
 */
fildBootState fild_GetBootState(void);

/**
 * Set boot state:
 *  - for debug prupose while in boot_only
 *  - when ift mode while in boot_only
 *  -...
 *
 * @param state
 */
void fild_SetBootState(fildBootState state);

/**************************************************
 * File system or device access
 *************************************************/
/**
 * Open a file or a device channel.
 *
 * examples:
 * fild_Open(filename, O_CREAT | O_WRONLY, S_IREAD | S_IWRITE)
 * will create a file with read and write access grant (or will
 * simply open it if it already exists) in write only mode.
 *
 * fild_Open(channelname, O_RDWR, 0) will open a channel whose
 * path in system is "channelname" for read/write access.
 *
 *
 *
 * @param pathname  path to file or device to open
 * @param flags     open time flags: O_RDONLY, O_WRONLY, O_RDWR,
 *                  O_CREAT...
 * @param mode      access permission (only when creating a
 *                  file...): S_IREAD, S_WRITE
 *
 * @return int -1 on failure (and errno is set appropriately), 0
 *         or greater on success. Successfull return value known
 *         as file descriptor.
 */
int fild_Open(const char *pathname, int flags, int mode);

/**
 * Close a file or a device.
 *
 * @param fd  file descriptor get from a successfull call to
 *            fild_Open
 *
 * @return int -1 on failure, 0 on success and errno is set
 *         appropriately.
 */
int fild_Close(int fd);

/**
 * Read from a file descriptor.
 *
 * @param fd     file descriptor get from a successfull call to
 *               fild_Open.
 * @param buf    destination buffer of the read.
 * @param count  length to read in bytes.
 *
 * @return int   num of read bytes on success, -1 on failure and
 *         errno is set appropriately.
 */
int fild_Read(int fd, void *buf, int count);

/**
 * Write to a file descriptor.
 *
 * @param fd    file descriptor get from a successfull call to
 *              fild_Open.
 * @param buf   src buffer of the write.
 * @param count length to write in bytes.
 *
 * @return int  num of written bytes on success, -1 on failure
 *         and errno is set appropriately.
 */
int fild_Write(int fd, const void *buf, int count);

/**
 * Write a given amount of data on an opened device channel
 * (boot channel, file system channel).
 *
 * @param handle   device channel descriptor get from a
 *                 successfull call to fild_Open
 * @param buf      src buffer of data to write on channel
 * @param count    length to write in bytes
 *
 * @return int     num of written bytes on success, -1 on
 *         failure and errno is set appropiately.
 */
int fild_WriteOnChannel(int handle, const void *buf, int count);

/**
 * Set current position in file.
 *
 * @param fd      file descriptor get from a successfull call to
 *                fild_Open.
 * @param offset  new position in file.
 * @param whence  directive to apply offset.
 *
 * @return int current offset in file on success, -1 on failure
 *         and errno is set appropriately.
 */
int fild_Lseek(int fd, int offset, int whence);

/**
 * Copy file.
 *
 * @param src  path of source to copy
 * @param dst  path of destination to be a copy or source. If
 *             dst exists, it is replaced with content of src.
 *             If dst doesn't exist, it is created.
 *
 * @return int 0 on success, -1 on failure.
 */
int fild_Copy(const char *src, const char *dst);

/**
 * Poll for read data on a device channel.
 *
 * @param fd       file descriptor get from a successfull call
 *                 to fild_Open.
 * @param buf      destination buffer of the read.
 * @param count    length to read in bytes.
 * @param timeout  time to wait for data (if -1, no timeout)
 *
 * @return int num of read bytes in on success, -1 on failure.
 */
int fild_Poll(int fd, char *buf, int count, int timeout);

/**
 * Get last error message.
 *
 * @return char* string of last error message.
 */
char *fild_LastError(void);

/**
 * Rename a file.
 *
 * @param oldpath
 * @param newpath
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately.
 */
int fild_Rename(const char *oldpath, const char *newpath);

/**
 * Remove a file
 *
 * @param pathname
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately.
 */
int fild_Remove(const char *pathname);

/**
 * Truncate a file.
 *
 * @param pathname  path to file to truncate.
 * @param offset    length of truncated file in bytes.
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately.
 */
int fild_Truncate(const char *pathname, int offset);

/**
 * Check if a file exist in file system.
 * (try to open it to know if exists...)
 *
 * @param pathname
 *
 * @return int -1 if doesn't exist, >= 0 if exist
 */
int fild_CheckFileExistence(const char *pathname);

/**
 *  Get file size.
 *
 * @param pathname
 *
 * @return int size of file, -1 on failure and errno is set
 *         appropriately.
 */
int fild_GetFileSize(const char *pathname);

/**
 * Get path infos
 *
 * @param path path name
 * @param buf filled with path infos on success (must not be
 *            NULL)
 *
 * @return int 0 on success, -1 on failure and errno set
 *         appropriately.
 */
int fild_Stat(const char *path, obex_Stat *buf);

/**
 * Open a directory
 *
 * @param name dir name
 *
 * @return obexDir* handle to directory on success or NULL on
 *         failure and errno set appropriately
 */
obex_Dir* fild_OpenDir(const char *name);

/**
 * Close a directory
 *
 * @param dir dir handle returned by fild_OpenDir
 *
 * @return int  0 on success, -1 on failure and errno set
 *         appropriately
 */
int fild_CloseDir(obex_Dir *dir);

/**
 * Read directory content
 *
 * @param dir dir handle returned by fild_openDir
 *
 * @return obexDirEnt* pointer to dir entry or NULL if no entry
 *         found or if failure and errno set appropriately
 */
obex_OsDirEnt *fild_ReadDir(obex_Dir *dir);

/**
 * Create a directory
 *
 * @param name dir name
 * @param mode dir rights
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately
 */
int fild_Mkdir(const char *pathname, int mode);

/**************************************************
 * Shared memory
 *************************************************/
/**
 * Init SHM for boot handling
 *
 * @param *shm_ctx shm ctx
 *
 * @return int 0 if OK, 1 if NOK
 */
int fild_ShmBootInit(fildShmCtx *shm_ctx);

/**
 * Wait for IPC
 * Blocks until a new IPC is posted in IPC mailbox.
 *
 * @param ipc_id updated with new IPC message
 */
void fild_ShmIpcWait(fildIpcId *ipc_id);

/**
 * Post IPC message in IPC mailbox.
 *
 * @param ipc IPC id.
 */
void fild_ShmIpcPost(fildIpcId ipc);

/**************************************************
 * Modem power control
 *************************************************/

/**
 * Disable any FILD external modem control.
 *
 * During boot sequence, FILD can forbid any external daemon to
 * perform any power cycle on modem hardware.
 *
 */
void fild_DisableModemPowerControl(void);

/**
 * Restore original modem power control found at start-up.
 *
 */
void fild_RestoreModemPowerControl(void);

/**************************************************
 * RIL control
 *************************************************/

/**
 * Start RIL daemon
 * May be RIL test daemon when ril.testmode property is set
 *
 */
void fild_RilStart(void);

/**
 * Stop RIL and RIL test daemons
 *
 */
void fild_RilStop(void);

/**
 * Enable RIL daemon start by fild_RilStart()
 *
 */
void fild_RilStartEnable(void);

/**
 * Disable RIL daemon start by fild_RilStart()
 *
 */
void fild_RilStartDisable(void);

/**************************************************
 * Threads...
 *************************************************/

/**
 * Create a thread..
 *
 * @param thread         thread handler
 * @param start_routine  thread routine
 * @param arg            pointer to args passed to thread
 *                       routine.
 *
 * @return int 0 on success, value < 0 on failure.
 */
int fild_ThreadCreate(fildThread *thread, void *(*start_routine)(void *), void *arg);

/**
 * Init semaphore
 *
 * @param sem     pointer to existing fildSem
 * @param shared  is sem shared bewteen processes. (0 is enough
 *                to share sem between threads of a same
 *                process.)
 * @param value   init sem token value
 *
 * @return int    0 on success, -1 on failure and errno is set
 *                appropriately.
 */
int fild_SemInit(fildSem *sem, int shared, int value);

/**
 * Post sem token.
 *
 * Might "unlock" thread or process waiting for sem token.
 *
 * @param sem  pointer to initialised fildSem
 *
 * @return int 0 on success, -1 on failure and errno is set
 *             appropriately.
 */
int fild_SemPost(fildSem *sem);

/**
 * Wait for sem token.
 *
 * "Locking" call if num of token == 0
 *
 * @param sem  pointer to initialised fildSem
 *
 * @return int 0 on success, -1 on failure and errno is set
 *             appropriately.
 */
int fild_SemWait(fildSem *sem);

/**************************************************
 * Timers...
 *************************************************/
/**
 * Create disarmed timer.
 *
 * fildTimer *t initialised for future usage.
 *
 * @param t
 * @param handler
 *
 * @return int 0 if OK, -1 in case of failure and errno is set
 *         appropriately.
 */
int fild_TimerCreate(fildTimer *t, fildTimerHandler handler);

/**
 * Arm timer for a given amount of second(s)
 *
 * @param t
 * @param secs
 *
 * @return int 0 if OK, -1 in case of failuer and errno is set
 *         appropriately.
 */
int fild_TimerStart(fildTimer t, int secs);

/**
 * Disarm timer.
 *
 * @param t
 *
 * @return int 0 if OK, -1 in case of failure and errno is set
 *         appropriately.
 */
int fild_TimerStop(fildTimer t);

/**
 * Disarm (if armed) and delete timer.
 *
 * @param t
 *
 * @return int 0 if OK, -1 in case of failure and errno is set
 *         appropriately.
 */
int fild_TimerDelete(fildTimer t);

/**************************************************
 * UART
 *************************************************/

/**
 * Configure opened UART channel.
 *
 * @param com_hdl handle of opened UART channel
 * @param rate UART baudrate.
 * @param fctrl UART fctrl
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately.
 */
int fild_ConfigUart(int com_hdl, int rate, int fctrl);

/**************************************************
 * Misc...
 *************************************************/
/**
 * Sleep required amount of time in second(s)
 *
 * @param seconds
 */
void fild_Sleep(unsigned int seconds);

/**
 * Sleep required amount of time in microseconds
 *
 * @param useconds
 */
void fild_Usleep(unsigned int useconds);

/**
 * Return current time in seconds since 1st of January 1970.
 *
 * @return unsigned int
 */
unsigned int fild_Time(void);

/**
 * Init LP0 hamdling
 */
void fild_WakeLockInit(void);

/**
 * Forbids platform LP0
 */
void fild_WakeLock(void);

/**
 * Allows platform LP0
 */
void fild_WakeUnLock(void);

/**
 * Init MDM USB stack handling
 */
void fild_UsbStackInit(void);

/**
 * Unload MDM USB stack
 */
void fild_UnloadUsbStack(void);

/**
 * Reload MDM USB stack
 */
void fild_ReloadUsbStack(void);

/**
 * USB host controller always-on (no autosuspend)
 */
void fild_UsbHostAlwaysOnActive(void);

/**
 * USB host controller always-on disabled (autosuspend)
 */
void fild_UsbHostAlwaysOnInactive(void);

/**
 * Init modem log storage env
 *
 * @param root_dir where to store any data
 * @param limit since data is stored in dated dir, set a limit
 *              for the number of dated dir to keep in file system
 */
void fild_LogInit(char *root_dir, int limit);

/**
 * Prepare storing of data logging:
 * - limit handling to create a new dated dir
 * - open coredump file in new dated dir
 *
 * @return file descriptor of opened coredump file, -1 if NOK
 */
int fild_LogStart(void);

/**
 * Store coredump data:
 * - close coredump file
 * - append modem logging data in dated dir if exist
 * - call icera-crashlogs to append AP logs (and more if needed...)
 */
void fild_LogStop(int fd);

/**
 * Init UART <--> MDM port forwarding
 *
 * A given MDM serial channel (in general AT channel) can be forwarded to
 * AP UART channel.
 *
 * @param arg expected is a string with the following format:
 *            "uart:<uart_device_name> mdm:<mdm_device_name>"
 *
 */
void fild_UartForwardInit(void *arg);

/** Check wether or not f/w was updated since last FILD
 *  start. If yes then act in consequence:
 *   - remove NRAM2 files except MEP file...
 *
 *  Check consists in comparing list of hash of files with
 *  corresponding files' hashes in local file system. If list
 *  not found or invalid, consider a f/w was updated.
 *
 *  Since today consequence of f/w update detection is "only" to
 *  remove NRAM2 files, then "only" check on MDM file will be
 *  done.
 *
 *  TODO: put in place check of all existing .wrapped files
 *  if required... */
void fild_CheckFwUpdateDone(void);
#endif /* FILD_H */
