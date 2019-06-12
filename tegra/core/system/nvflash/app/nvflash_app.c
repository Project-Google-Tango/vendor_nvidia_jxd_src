/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvutil.h"
#include "nvcommon.h"
#include "nvapputil.h"
#include "nv3p.h"
#include "nv3pserver.h"
#include "nvflash_usage.h"
#include "nvflash_version.h"
#include "nvflash_configfile.h"
#include "nvflash_commands.h"
#include "nvflash_verifylist.h"
#include "nvflash_util.h"
#include "nvusbhost.h"
#include "nvboothost.h"
#include "nvboothost_t1xx.h"
#include "nvboothost_t12x.h"
#include "nvflash_parser.h"
#include "nvddk_fuse.h"
#include "nvpartmgr_defs.h"
#include "nvbuildbct.h"
#include "nvbdktest.h"
#include "nvnct.h"
#include "nvbct.h"
#include "nvflash_fuse_bypass.h"

#ifdef NVHOST_QUERY_DTB
#include "nvgetdtb_platform.h"
#endif

#ifdef NV_EMBEDDED_BUILD
#include "nvimager.h"
#endif

#if NVOS_IS_WINDOWS || NVOS_IS_LINUX
#include <stdio.h>
#endif

/* don't connect to the bootrom if this is set. */
#define NV_FLASH_BOOTROM_MODE       (0)
#define NV_FLASH_STAND_ALONE        (1)

/* dangerous command, must edit source code to enable it */
#define NV_FLASH_HAS_OBLITERATE     (0)

#define NV_FLASH_PARTITION_NAME_LENGTH 4
#define NVFLASH_TEMP_STRING_SIZE 50

/* Size of a max test result string buffer */
#define BDKTEST_RESULT_BUFFER_SIZE (1024 * 32)

#define DATA_CHUNK_SIZE_1MB (1024 * 1024)

/* File extension for back up files. */
#define BACKUP_FILE_EXTENSION ".bin"

/* File extension for bct file */
#define BCT_CFG_FILE_EXTENSION ".cfg"

/* option of --runbdktest */
#define BDKTESTOPTIONS_STOPONFAIL_BIT   0
#define BDKTESTOPTIONS_LOGFORMAT_BIT    1
#define BDKTESTOPTIONS_TIMEOUT_BIT      5
#define BDKTESTOPTIONS_RESET_BIT        13

/* Download partitions account for 88% of totoal download time.
   It is measured on Dalmore.
 */
#define NVFLASH_DOWNLOADPARTITION_RATIO 88

#ifndef DEBUG_VERIFICATION
#define DEBUG_VERIFICATION 0
#define DEBUG_VERIFICATION_PRINT(x) \
    do { \
        if (DEBUG_VERIFICATION) \
        { \
            NvAuPrintf x; \
        } \
    } while(0)
#endif

#define VERIFY( exp, code ) \
    if( !(exp) ) { \
        code; \
    }
#ifndef NVHOST_QUERY_DTB
NvU8 s_MiniLoader_t30[] =
{
    #include "nvflash_miniloader_t30.h"
};

NvU8 s_MiniLoader_t11x[] =
{
    #include "nvflash_miniloader_t11x.h"
};

NvU8 s_MiniLoader_t12x[] =
{
#ifdef NVTBOOT_EXISTS
    #include "nvtboot_t124.h"
#else
    #include "nvflash_miniloader_t12x.h"
#endif
};
#else
NvU8 s_Dtb_Miniloader[] =
{
    #include "nvgetdtb_miniloader.h"
};
#endif

/* Structre to create the list of partitions which are to be backed
 * up before formatting and restored after creating partitions.
 */
typedef struct NvFlashBackupPartitionRec
{
    NvFlashPartition *partition;
    struct NvFlashBackupPartitionRec *next;
}NvFlashBackupPartition;

NctCustInfo s_NctCustInfo;
NvFlashBackupPartition *s_dPartition     = NULL;
NvFlashBackupPartition *s_dPartitionList = NULL;

Nv3pSocketHandle s_hSock;
NvFlashDevice *s_Devices;
NvU32 s_nDevices;
NvBool s_Resume;
NvBool s_Quiet;
NvOsSemaphoreHandle s_FormatSema = 0;
NvBool s_FormatSuccess = NV_TRUE;
static Nv3pPartitionInfo *s_pPartitionInfo = NULL;
NvBool s_bIsFormatPartitionsNeeded;
static NvU32 OpMode;
Nv3pCmdGetPlatformInfo s_PlatformInfo;
Nv3pBoardDetails g_BoardDetails;
NvU32 s_NvflashOprMode = NV_FLASH_BOOTROM_MODE;
static NvU32 s_PartitionId = 0;
static NvOsSystemTime StartTime;
static NvOsSystemTime EndTime;
static NvBool s_IsNCTPartInCommandline = NV_TRUE;
const char *s_NCTPartName = "NCT";
extern NvFuseBypassInfo s_FuseBypassInfo;
static NvU64 total_image_size = 0;
NvBool s_ReportFlashProgress;

NvError nvflash_start_pseudoserver(void);
void nvflash_exit_pseudoserver(void);
void PseudoServer(void* args);
void nvflash_rcm_open( NvUsbDeviceHandle* hUsb, NvU32 DevInstance );
static NvBool
nvflash_rcm_read_uid( NvUsbDeviceHandle hUsb, void *uniqueId, NvU32 DeviceId);
NvBool nvflash_rcm_send( NvUsbDeviceHandle hUsb, NvU8 *msg, NvU32* response );
void nvflash_rcm_close(NvUsbDeviceHandle hUsb);
NvBool nvflash_connect(void);
NvBool nvflash_configfile( const char *filename );
NvBool nvflash_dispatch( void );
NvBool nvflash_sendfile( const char *filename, NvU64 NoOfBytes);

static NvBool nvflash_cmd_create( void );
static NvBool nvflash_cmd_settime( void );
static NvBool nvflash_cmd_formatall( void );
static NvBool nvflash_query_partition_type(NvU32 PartId, NvU32 *PartitionType);
static NvBool nvflash_cmd_getbct( const char *bct_file );
static NvBool nvflash_cmd_getbit( NvFlashCmdGetBit *a);
static NvBool nvflash_cmd_dumpbit( NvFlashCmdDumpBit * a );
static NvBool nvflash_cmd_setboot( NvFlashCmdSetBoot *a );

static NvBool nvflash_odmcmd_VerifySdram(NvFlashCmdSetOdmCmd *a);
static NvBool nvflash_cmd_bootloader( const char *filename );
static NvBool nvflash_cmd_go( void );
static NvBool nvflash_cmd_read( NvFlashCmdRead *a );
static NvBool nvflash_cmd_read_raw( NvFlashCmdRawDeviceReadWrite *a );
static NvBool nvflash_cmd_write_raw( NvFlashCmdRawDeviceReadWrite *a );
static NvBool nvflash_cmd_getpartitiontable(NvFlashCmdGetPartitionTable *a,
                                            Nv3pPartitionInfo **pPartitionInfo);
static NvBool nvflash_cmd_setodmcmd( NvFlashCmdSetOdmCmd*a );
static NvBool nvflash_cmd_sync( void );
static NvBool nvflash_cmd_odmdata( NvU32 odm_data );
static NvBool nvflash_cmd_setbootdevicetype( const NvU8 *devType );
static NvBool nvflash_cmd_setbootdeviceconfig( const NvU32 devConfig );
static NvBool nvflash_cmd_formatpartition( Nv3pCommand cmd, NvU32 PartitionNumber);
static NvBool nvflash_enable_verify_partition(NvU32 PartitionId );
static NvBool nvflash_verify_partitions(void);
static NvBool nvflash_send_verify_command(NvU32 PartitionId);
#ifndef NVHOST_QUERY_DTB
static NvBool nvflash_check_verify_option_args(void);
#endif
static NvBool nvflash_check_skippart(char *name);
static NvBool nvflash_check_skiperror(void);
static NvBool nvflash_match_file_ext(const char *filename, const char *match);
static NvBool nvflash_get_partlayout(NvBool ReadBctAtDevice);
static NvU32 nvflash_get_partIdByName(const char *pStr, NvBool ReadBctAtDevice);
static char* nvflash_get_partNameById(NvU32 PartitionId, NvBool ReadBctAtDevice);
static NvBool nvflash_cmd_runbdktests(NvFlashCmdBDKTest *a);

static NvU32 nvflash_read_devid(NvUsbDeviceHandle hUsb);
void nvflash_buildbct(void);
static NvBool nvflash_cmd_symkeygen(NvFlashCmdSymkeygen *a);
static NvBool nvflash_cmd_dfuseburn(NvFlashCmdDFuseBurn *a);
static NvError nvflash_data_send(NvU64 bytesLeftToTransfer, NvU8 *buf);
static NvBool nvflash_odmcmd_updateucfw(NvFlashCmdSetOdmCmd *a);
static NvBool nvflash_odmcmd_upgradedevicefirmware(NvFlashCmdSetOdmCmd *a);
static NvBool nvflash_backup_partitions(void);
static NvBool nvflash_remove_backuped_partitions(void);
static NvBool nvflash_odmcmd_tegratabfusekeys(NvFlashCmdSetOdmCmd *a);
static NvBool nvflash_odmcmd_tegratabverifyfuse(NvFlashCmdSetOdmCmd *a);
#if ENABLE_NVDUMPER
static NvBool nvflash_cmd_nvtbootramdump( NvFlashCmdNvtbootRAMDump *a );
#endif

/* Downloads single partition
 *
 * @param pDownloadArg Argument containing the partition
 *                     id and filename to download
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
static NvBool
NvFlashDownloadPartition(
    NvFlashCmdDownload *pDownloadArg);

/* Updates the specified section of device bct with the value
 * present in specified bct file.
 *
 * @param pBctFile Bct file
 * @param pBctSection Section of bct which is to be updated
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
static NvBool
nvflash_cmd_updatebct(
    const char *pBctFile,
    const char *pBctSection);

/* Downloads the file to a partition
 *
 * @param pDownloadArg Pointer to structure containing
 *                     information for download command
 * @param PartitionType Type of the partition
 * @param MaxSize Maximum size of the partition
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
static NvBool
nvflash_cmd_download(
    NvFlashCmdDownload *pDownloadArg,
    NvU32 PartitionType,
    NvU64 MaxSize);

/* Replaces/sets the specified bct as device's iram bct
 *
 * @param pSetBctArg Pointer to structure containing information to set bct
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
static NvBool nvflash_cmd_setbct(const NvFlashBctFiles *pSetBctArg);

/* Reads the data sent by device and stores into file
 *
 * @param pFileName Name of the file
 * @param Size Expected size of data
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
static NvBool nvflash_recvfile(const char *pFileName, NvU64 Size);

/**
 * Converts the Nv3pStatus enum to string
 *
 * @param s nv3p status
 *
 * @returns String
 */
char* nvflash_status_string(Nv3pStatus s);

/**
 * Waits for status from nv3pserver or nvml server
 *
 * @returns NV_TRUE if no errors else NV_FALSE
 */
NvBool nvflash_wait_status(void);

/**
 * Retrieves the board details if common cfg or sku bypass
 * is used.
 *
 * @returns NV_TRUE if no errors else NV_FALSE
 */
static NvBool NvFlashRetrieveBoardDetails(void);

#if NV_FLASH_HAS_OBLITERATE
static NvBool nvflash_cmd_obliterate( void );
#endif
static void nvflash_thread_execute_command(void *args);
static NvBool
ConvertPartitionType(NvPartMgrPartitionType PartNvPartMgr,
                     Nv3pPartitionType *PartNv3p);
static void nvflash_report_progress(NvU32 size);

static NvBool nvflash_ParseConfigTable(
    const char *config_file, NvU32 partition_id, NvU64 partition_size);

typedef struct NvBdkCallbackArgRec
{
    Nv3pCmdRunBDKTest bdktest_params;
    NvU8 *out_buffer;
    NvBool out_format;
    NvBool testFailed;
    NvOsFileHandle hFile;
} NvBdkCallbackArg;

typedef struct NvConfigTableCallbackArgRec
{
    nct_part_head_type *Header;
    NvU8 *NctEntry;
    NvU32 EntrySize;
    NvU32 offset;
    NvBool Found;
} NvConfigTableCallbackArg;

typedef struct Nv3pPseudoServerThreadArgsRec
{
    NvBool IsThreadRunning;
    NvBool ExitThread;
    NvOsThreadHandle hThread;
} Nv3pPseudoServerThreadArgs;

static Nv3pPseudoServerThreadArgs s_gPseudoServerThread;


void nvflash_report_progress( NvU32 size )
{
    static NvU32 nvflash_progress_bar = 0;
    static NvU64 sentToBytes = 0;
    static NvU32 progress = 0;

    if (!s_ReportFlashProgress)
        return;

    if (!total_image_size)
        return;

    if( size == 0xFFFFFFFF)
        sentToBytes = total_image_size;
    else
        sentToBytes += size;
    progress = (NvU32)((sentToBytes*100)/(total_image_size));
    if( total_image_size )
    {
        if(( progress - nvflash_progress_bar) > 0)
        {
            nvflash_progress_bar = progress;
            if (nvflash_progress_bar > 100)
                nvflash_progress_bar = 100;
            NvAuPrintf("\t[PROGRESS:%d]\n", nvflash_progress_bar);
#if NVOS_IS_WINDOWS || NVOS_IS_LINUX
            fflush(stdout);
#endif
        }
    }
}

void PseudoServer(void* args)
{
#if NVODM_ENABLE_SIMULATION_CODE==1
    Nv3pServer();
#endif
}

NvError nvflash_start_pseudoserver(void)
{
#if NVODM_ENABLE_SIMULATION_CODE
    return NvOsThreadCreate(
        PseudoServer,
        (void *)&s_gPseudoServerThread,
        &s_gPseudoServerThread.hThread);
#else
    return NvError_NotImplemented;
#endif
}

void nvflash_exit_pseudoserver(void)
{
    NvOsThreadJoin(s_gPseudoServerThread.hThread);
}

static NvBool nvflash_odmcmd_fuelgaugefwupgrade(NvFlashCmdSetOdmCmd *a);

/**
 * This sends a download-execute message to the bootrom. This may only
 * be called once.
 */
void
nvflash_rcm_open( NvUsbDeviceHandle* hUsb,  NvU32 DevInstance )
{
    NvError e = NvSuccess;
    NvBool bWait;

    /* might want to wait for a usb device to be plugged in */
    e = NvFlashCommandGetOption( NvFlashOption_Wait, (void *)&bWait );
    VERIFY( e == NvSuccess, return );

    do {
        e = NvUsbDeviceOpen( DevInstance, hUsb );
        if( e == NvSuccess )
        {
            break;
        }
        if( bWait )
        {
            NvOsSleepMS( 1000 );
        }
    } while( bWait );

    if(e != NvSuccess)
    {
        if(e == NvError_AccessDenied)
            NvAuPrintf("Permission Denied\n");
        else
            NvAuPrintf("USB device not found\n");
        }
}

NvBool NvFlashRetrieveBoardDetails(void)
{
    NvBool IsCommonCfg = NV_FALSE;
    NvError e = NvSuccess;
    const NvFlashBctFiles *pBctFile = NULL;
    NvFlashFuseBypass FusebypassOpt;
    const char *pErrStr = NULL;

    /* Check if bct is common bct cfg or not */
    e = NvFlashCommandGetOption(NvFlashOption_Bct, (void *)&pBctFile);
    VERIFY(e == NvSuccess, goto fail);

    if(pBctFile->BctOrCfgFile)
        IsCommonCfg = nvflash_check_cfgtype(pBctFile->BctOrCfgFile);

    /* Retrieve the fusebypass options */
    e = NvFlashCommandGetOption(NvFlashOption_FuseBypass,
                                (void *)&FusebypassOpt);
    VERIFY(e == NvSuccess, pErrStr = "Fuse bypass option failed"; goto fail);

    if (!IsCommonCfg && (FusebypassOpt.pSkuId == NULL ||
        NvFlashIStrcmp(FusebypassOpt.pSkuId, "0") == 0))
    {
        goto clean;
    }

    if (IsCommonCfg || FusebypassOpt.pSkuId != NULL)
    {
        NvU32 Size = 0;
        NvU32 BoardId = 0;
        NvBool IsAutoFuseBypass = NV_FALSE;

        /* Retrieve the speedo/iddq values from board */
        e = Nv3pCommandSend(s_hSock, Nv3pCommand_GetBoardDetails,
                            &Size, 0);
        VERIFY(e == NvSuccess,
               pErrStr = "Error: Unable to retrieve board details"; goto fail);

        VERIFY (Size == sizeof(g_BoardDetails), e = NvError_BadParameter;
                pErrStr = "Error: Board details Size mismatch"; goto fail);

        e = Nv3pDataReceive(s_hSock, (NvU8 *)&g_BoardDetails,
                            sizeof(g_BoardDetails), &Size, 0);
        VERIFY(e == NvSuccess,
               pErrStr = "Error: Unable to read the board details"; goto fail);

        VERIFY(nvflash_wait_status(),
               pErrStr = "Error: Unable to retrieve board details";
               e = NvError_BadParameter; goto fail);

        BoardId = g_BoardDetails.BoardInfo.BoardId;

        if (FusebypassOpt.pSkuId)
            IsAutoFuseBypass = !NvFlashIStrcmp(FusebypassOpt.pSkuId, "auto");

        if ((BoardId == 0 || BoardId == 0xFFFF) &&
            (IsCommonCfg || IsAutoFuseBypass))
        {
            NvAuPrintf("ERROR: Board ID has not been programmed "
                       "- return board for programming");
            goto fail;
        }
    }

    /* If fuse bypass config is given then update the sku type
     * which is being selected. Required for common BCT cfg
     * feature.
     */
    e = NvFlashParseFusebypass();
    if (e != NvSuccess)
        goto fail;

clean:
    return NV_TRUE;
fail:
    if (pErrStr)
        NvAuPrintf("%s\n", pErrStr);

    return NV_FALSE;
}


static NvBool
nvflash_rcm_read_uid( NvUsbDeviceHandle hUsb, void *uniqueId, NvU32 DeviceId)
{
    NvU32 len;
    NvError e;
    Nv3pChipUniqueId uid;
    NvU32 Size = 0;
    const char *err_str = 0;

    NvOsMemset((void *)&uid, 0, sizeof(uid));

    /* the bootrom sends back the chip's uid, read it. */
    if (DeviceId == 0x30)
        Size = sizeof(NvU64);
    else
        Size = sizeof(uid);

    e = NvUsbDeviceRead( hUsb, &uid, Size, &len );
    VERIFY( e == NvSuccess, err_str = "uid read failed"; goto fail );
    VERIFY( len == Size,
        err_str = "uid read failed (truncated)"; goto fail );
    *(Nv3pChipUniqueId *)uniqueId = uid;
    return NV_TRUE;
fail:
    if( err_str )
    {
        NvAuPrintf( "%s\n", err_str );
    }
    return NV_FALSE;
}

NvBool
nvflash_rcm_send( NvUsbDeviceHandle hUsb, NvU8 *msg, NvU32* response )
{
    NvBool b;
    NvError e;
    NvU32 resp = 0;
    NvU32 len;
    const char *err_str = 0;

    len = NvBootHostRcmGetMsgLength( msg );
    e = NvUsbDeviceWrite( hUsb, msg, len );
    VERIFY( e == NvSuccess,
        err_str = "Command send failed (usb write failed)";
        goto fail );

    /* get the bootrom response */
    e = NvUsbDeviceRead( hUsb, &resp, sizeof(resp), &len );
    VERIFY( e == NvSuccess,
        err_str = "miniloader download failed (response read failed)";
        goto fail );

    VERIFY( len == sizeof(resp),
        err_str = "miniloader download failed (response truncated)";
        goto fail );

    b = NV_TRUE;
    *response = resp;
    goto clean;

fail:
    if (err_str)
    {
        NvAuPrintf( "%s\n", err_str );
    }

    b = NV_FALSE;

clean:
    if (msg)
        NvOsFree(msg);
    return b;
}

NvU32
nvflash_read_devid(NvUsbDeviceHandle hUsb)
{
    NvU32 DevId = 0;
    NvU32 NvDeviceId = 0x00;

    if (NvUsbDeviceReadDevId(hUsb,&DevId) ==  NvSuccess)
    {
        NvDeviceId = DevId & 0x00FF;
    }
    return NvDeviceId;
}

void nvflash_rcm_close(NvUsbDeviceHandle hUsb)
{
    NvUsbDeviceClose( hUsb );
}

NvBool
nvflash_parse_blob(const char *BlobFile, NvU8 **pMsgBuffer, NvU32 HeaderType, NvU32 *length)
{
    NvBool b;
    NvError e;
    NvOsFileHandle hFile = 0;
    NvFlashBlobHeader header;
    size_t bytes;
    char *err_str = 0;

    // read the blob data to extract info like RCM messages, bl hash etc
    e = NvOsFopen(BlobFile, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    for(;;)
    {
        e = NvOsFread(hFile, &header, sizeof(header), &bytes);
        VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);
        if ((NvU32)header.BlobHeaderType == HeaderType)
            break;
        NvOsFseek(hFile, header.length, NvOsSeek_Cur);
    }
    *pMsgBuffer = NvOsAlloc(header.length);
    if (!*pMsgBuffer) {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    if (length)
    *length = header.length;
    e = NvOsFread(hFile, *pMsgBuffer, header.length, &bytes);
    VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);
    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;
    if (err_str && HeaderType != NvFlash_mb)
        NvAuPrintf("%s NvError 0x%x\n", err_str , e);
    if (*pMsgBuffer)
        NvOsFree(*pMsgBuffer);
clean:
    NvOsFclose(hFile);
    return b;
}

/*
 * Callback function for updating configuration table.
 *
 * rec: Vector of <key, value> pairs parsed by parser
 * aux: Callback argument having Configuration table structure which is to be filled
 *      and a boolean value indicating whether the valid sys info entry is found
 *      or not.
 *
 * return: Status to parser whether to continue or stop parsing
 *         and any error.
 */
static NvFlashParserStatus
nvflash_configtable_parser_callback(NvFlashVector *rec, void *aux)
{
    NvU32 i = 0;
    NvFlashParserStatus ret    = P_CONTINUE;
    NvU32 nPairs               = rec->n;
    NvBool status              = NV_FALSE;
    NvFlashSectionPair *Pairs  = rec->data;
    const char *err_str        = NULL;
    NvBool unknown_token       = NV_FALSE;
    NvError e                  = 0;
    NvConfigTableCallbackArg *a = (NvConfigTableCallbackArg *) aux;
    nct_entry_type *data = NULL;
    nct_entry_type *Ptr = NULL;
    NvU32 dataType = 0;
    NvU32 idx;
    NvU32 entry_array_index = 0;

    /**
    //-----------------------------------------------
    // comment
    // tag info
    // 0x10 : 1byte data , 0x1A : 1byte data array
    // 0x20 : 2bytes data, 0x2A : 2bytes data array
    // 0x40 : 4bytes data, 0x4A : 4bytes data array
    // 0x80 : string     , 0x8A : string array
    //-----------------------------------------------
    **/

    status = a->Found;
    for(i = 0; i < nPairs; i++)
    {
        e = 0;
        switch(Pairs[i].key[0])
        {
            case 'v':
                if(NvOsStrcmp(Pairs[i].key, "vid") == 0)
                {
                    a->Header->vendorId = (NvU32)NvUStrtoul (Pairs[i].value,
                        NULL, 0);
                }
                else if(NvOsStrcmp(Pairs[i].key, "version") == 0)
                {
                    if(status == NV_TRUE)
                    {
                        ret = P_STOP;
                        goto end;
                    }
                    a->Header->version = (NvU32)NvUStrtoul (Pairs[i].value,
                        NULL, 0);
                    if(a->Header->version == 0x00010000)
                        status = NV_TRUE;
                }
                break;
            case 'p':
                if(NvOsStrcmp(Pairs[i].key, "pid") == 0)
                {
                    a->Header->productId = (NvU32)NvUStrtoul (Pairs[i].value,
                        NULL, 0);
                }
                break;
            case 'r':
                if(NvOsStrcmp(Pairs[i].key, "revision") == 0)
                {
                    a->Header->revision = (NvU32)NvUStrtoul (Pairs[i].value,
                        NULL, 0);
                }
                break;
            case 'o': // offset
                if(status == NV_FALSE)
                {
                    goto end;
                }
                if(NvOsStrcmp(Pairs[i].key, "offset") == 0)
                {
                    data = NvOsAlloc(sizeof(nct_entry_type) * (a->Header->revision + 1));
                    NvOsMemset(data, 0, sizeof(nct_entry_type) * (a->Header->revision + 1));
                    a->NctEntry = (NvU8 *)data;
                    a->EntrySize = sizeof(nct_entry_type) * (a->Header->revision + 1);
                    Ptr = data;
                    a->offset = (NvU32)NvUStrtoul (Pairs[i].value,
                        NULL, 0);
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;
            case 'n':
                if(NvOsStrcmp(Pairs[i].key, "name") == 0)
                    entry_array_index = 0;
                break;
            case 'i':
                if(status == NV_FALSE)
                {
                    goto end;
                }
                if(NvOsStrcmp(Pairs[i].key, "idx") == 0)
                {
                    idx = (NvU32)NvUStrtoul(Pairs[i].value, NULL, 0);
                    Ptr = (nct_entry_type *)(a->NctEntry + sizeof(nct_entry_type)* idx);
                    Ptr->index = idx;
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;
            case 't': // not sure it is needed actually.
                if(status == NV_FALSE)
                {
                    goto end;
                }
                if(NvOsStrcmp(Pairs[i].key, "tag") == 0)
                {
                    dataType = (NvU32)NvUStrtoul(Pairs[i].value, NULL, 0);
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;
            case 'd':
                if(status == NV_FALSE)
                {
                    goto end;
                }
                if(NvOsStrcmp(Pairs[i].key, "data") == 0)
                {
                    void * tmp = NULL;
                    switch(dataType)
                    {
                        case NCT_TAG_1B_SINGLE:
                            tmp = NvOsAlloc(sizeof(nct_item_type));
                            NvOsMemset(tmp, 0, sizeof(nct_item_type));
                            *(NvU8*)tmp = (NvU8)NvUStrtoul(Pairs[i].value, NULL, 0);
                            Ptr->data = *(nct_item_type*)tmp;
                            break;
                        case NCT_TAG_2B_SINGLE:
                            tmp = NvOsAlloc(sizeof(nct_item_type));
                            NvOsMemset(tmp, 0, sizeof(nct_item_type));
                            *(NvU16*)tmp = (NvU16)NvUStrtoul(Pairs[i].value, NULL, 0);
                            Ptr->data = *(nct_item_type*)tmp;
                            break;
                        case NCT_TAG_4B_SINGLE:
                            tmp = NvOsAlloc(sizeof(nct_item_type));
                            NvOsMemset(tmp, 0, sizeof(nct_item_type));
                            *(NvU32*)tmp = NvUStrtoul(Pairs[i].value, NULL, 0);
                            Ptr->data = *(nct_item_type*)tmp;
                            break;
                        case NCT_TAG_1B_ARRAY:
                            tmp = NvOsAlloc(1);
                            NvOsMemset(tmp, 0, 1);
                            *(NvU8*)tmp = (NvU8)NvUStrtoul(Pairs[i].value, NULL, 0);
                            *((NvU8*)&(Ptr->data) + entry_array_index) = *(NvU8*)tmp;
                            entry_array_index++;
                            break;
                        case NCT_TAG_2B_ARRAY:
                            tmp = NvOsAlloc(2);
                            NvOsMemset(tmp, 0, 2);
                            *(NvU16*)tmp = (NvU16)NvUStrtoul(Pairs[i].value, NULL, 0);
                            *((NvU16*)&(Ptr->data) + entry_array_index) = *(NvU16*)tmp;
                            entry_array_index++;
                            break;
                        case NCT_TAG_4B_ARRAY:
                            tmp = NvOsAlloc(4);
                            NvOsMemset(tmp, 0, 4);
                            *(NvU32*)tmp = NvUStrtoul(Pairs[i].value, NULL, 0);
                            *((NvU32*)&(Ptr->data) + entry_array_index) = *(NvU32*)tmp;
                            entry_array_index++;
                            break;
                        case NCT_TAG_STR_SINGLE:
                            NvOsStrcpy((char*)&Ptr->data, Pairs[i].value);
                            break;
                        case NCT_TAG_STR_ARRAY:
                            break;
                    }
                    NvOsFree(tmp);
                    Ptr->checksum = nvflash_crc32(0, (NvU8 *)Ptr,
                                        sizeof(nct_entry_type)-sizeof(Ptr->checksum));
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;
            default:
                unknown_token = NV_TRUE;
                goto end;
        }

        if(e != NvSuccess)
        {
            ret = P_ERROR;
            goto end;
        }
    }

end:
    if(unknown_token)
    {
        NvAuPrintf("Invalid token in config file: %s\n", Pairs[i].key);
        ret = P_ERROR;
    }

    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
        ret = P_ERROR;
    }

    a->Found = status;

    return ret;
}

NvBool
nvflash_connect(void)
{
    NvBool b = NV_TRUE;
    NvError e = NvSuccess;
    NvU8 *msg_buff = 0;
    NvFlashRcmDownloadData download_arg;
    const char *err_str = 0;
    Nv3pCommand cmd;
    NvUsbDeviceHandle hUsb = 0;
    Nv3pChipUniqueId uniqueId;
    NvU32 response = 0;
    NvU32 DeviceId = 0;
    NvU8* pMiniLoader = 0;
    NvU32 MiniLoaderSize = 0;
    const char* pBootDevStr = 0;
    const char* pChipNameStr = 0;
    const char* str = 0;
    Nv3pTransportMode TransportMode = Nv3pTransportMode_Usb;
    NvU32 devinstance = 0;
    const char* blob_file;
    NvU32 DevId = 0;
    NvBool SkipAutoDetect = NV_FALSE;
    const NvFlashBctFiles *pBctFile = NULL;
    NvConfigTableCallbackArg callback_arg;
    NvOsFileHandle hFile = 0;
    nct_part_head_type Header;
    NvU8 *NctEntry = 0;
    NvBool IsBctSet = NV_FALSE;
    NvFlashNCTPart *pNCTPart = NULL;
    const char *pAppletFile = NULL;
    NvOsFileHandle hAppletFile = 0;
    NvOsStatType AppletFileStat;
    NvU32 ReadBytes = 0;
    NvU32 i = 0;

    NvFlashCommandGetOption(NvFlashOption_TransportMode,(void*)&TransportMode);

#if NVODM_BOARD_IS_FPGA
    if(TransportMode == Nv3pTransportMode_Jtag)
    {
        NvU32 EmulDevId = 0;
        NvFlashCommandGetOption(NvFlashOption_EmulationDevId,(void*)&EmulDevId);
        nvflash_set_devid(EmulDevId);
    }
    else
    {
        TransportMode = Nv3pTransportMode_Usb;
    }
#endif

    //default transport mode is USB
    if(TransportMode == Nv3pTransportMode_default)
        TransportMode = Nv3pTransportMode_Usb;

    //fix me :remove this once the BR BINARY
     //and valid BCT are available for T124
     NvFlashCommandGetOption(NvFlashOption_EmulationDevId,(void*)&DevId);
     if (DevId == 0x40)
     {
         nvflash_set_devid(DevId);
         //function call to create a dummy BCT
         //will be removed once valid BCT is available
         nvflash_get_bct_size();
     }
     /* don't try to connect to the bootrom in resume mode */

    if (TransportMode == Nv3pTransportMode_Usb && (DevId != 0x40))
    {
        NvFlashCommandGetOption(NvFlashOption_DevInstance,(void*)&devinstance);
        nvflash_rcm_open(&hUsb, devinstance);
        VERIFY( hUsb, goto fail );
        // obtain the product id
        DeviceId = nvflash_read_devid(hUsb);
        nvflash_set_devid(DeviceId);
        if (s_Resume)
            nvflash_rcm_close(hUsb);
    }
    /* don't try to connect to the bootrom in resume mode */
    if (!s_Resume && (TransportMode == Nv3pTransportMode_Usb) && (DevId != 0x40))
    {
        NvDdkFuseOperatingMode op_mode;

        op_mode = NvDdkFuseOperatingMode_NvProduction;

        b = nvflash_rcm_read_uid(hUsb, &uniqueId, DeviceId);
        VERIFY(b, goto fail);
#ifndef NVHOST_QUERY_DTB
        NvAuPrintf("chip uid from BR is: 0x%.8x%.8x%.8x%.8x\n",
                                uniqueId.ecid_3,uniqueId.ecid_2,
                                uniqueId.ecid_1,uniqueId.ecid_0);
#endif
        // --blob option need to be used following nvflash v1.1.51822
        NvFlashCommandGetOption(NvFlashOption_Blob, (void *)&blob_file);
        VERIFY(e == NvSuccess, goto fail);

        if (blob_file)
        {
            //RCM message comes from nvsecuretool utility in a blob
            op_mode = NvDdkFuseOperatingMode_OdmProductionSecure;
            b = nvflash_parse_blob(blob_file,
                    &msg_buff, NvFlash_EnableJtag,NULL);
            if (b)
            {
                if (s_NvflashOprMode == NV_FLASH_BOOTROM_MODE)
                {
#ifndef NVHOST_QUERY_DTB
                    NvAuPrintf("Sending Enable JTAG command\n");
#endif
                    b = nvflash_rcm_send(hUsb, msg_buff, &response);
                    VERIFY(b, goto fail);
                }
            }
        }
        if (blob_file)
        {
            // odm secure mode, RCM message comes from sbktool utility in a blob
            op_mode = NvDdkFuseOperatingMode_OdmProductionSecure;
            b = nvflash_parse_blob(blob_file,
                    &msg_buff, NvFlash_QueryRcmVersion,NULL);
            VERIFY(b,
                err_str = "query rcm version parse from blob failed";
                goto fail);
        }
        else
        {
            // nvprod mode, RCM message is prepared
            if (DeviceId == 0x30)
            {
                e = NvBootHostRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_QueryRcmVersion,
                    NULL, (NvU8 *)NULL, 0,
                    0, NULL,
                    op_mode, NULL, &msg_buff);
            }
            else if (DeviceId ==0x40)
            {
                e = NvBootHostT12xRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_QueryRcmVersion,
                    NULL, (NvU8 *)NULL, 0,
                    0, NULL,
                    op_mode, NULL, &msg_buff);
            }
            else
            {
                e = NvBootHostT1xxRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_QueryRcmVersion,
                    NULL, (NvU8 *)NULL, 0,
                    0, NULL,
                    op_mode, NULL, &msg_buff);
            }

            VERIFY(e == NvSuccess, err_str = "miniloader message create failed";
                goto fail);
        }

        if (s_NvflashOprMode == NV_FLASH_BOOTROM_MODE) {
            b = nvflash_rcm_send(hUsb, msg_buff, &response);
#ifndef NVHOST_QUERY_DTB
            NvAuPrintf("rcm version 0X%x\n", response);
#endif
            VERIFY(b, goto fail);
            //e = NvBootHostRcmSetVersionInfo(response);
            //VERIFY( e == NvSuccess, err_str = "Unsupported RCM version";
                //goto fail );
        }

        msg_buff = 0;

        if (blob_file)
        {
            // odm secure mode, RCM message comes from sbktool utility in a blob
            b = nvflash_parse_blob(blob_file,
                    &msg_buff, NvFlash_SendMiniloader,NULL);
            VERIFY(b,
                err_str = "download execute command parse from blob failed";
                goto fail);
        }
        else
        {
            // nvprod mode, RCM message is prepared
            // FIXME: figure this out at runtime
            #define NVFLASH_MINILOADER_ENTRY_32K 0x40008000
            #define NVFLASH_MINILOADER_ENTRY_40K 0x4000A000
            #define NVFLASH_MINILOADER_ENTRY_56K 0x4000E000

#ifndef NVHOST_QUERY_DTB
            if (DeviceId == 0x30)
            {
                pMiniLoader = s_MiniLoader_t30;
                MiniLoaderSize = sizeof(s_MiniLoader_t30);
                /* download the miniloader to the bootrom */
                download_arg.EntryPoint = NVFLASH_MINILOADER_ENTRY_40K;
            }
            else if (DeviceId == 0x35)
            {
                pMiniLoader = s_MiniLoader_t11x;
                MiniLoaderSize = sizeof(s_MiniLoader_t11x);
                /* download the miniloader to the bootrom */
                download_arg.EntryPoint = NVFLASH_MINILOADER_ENTRY_56K;
            }
            else if (DeviceId == 0x40)
            {
                pMiniLoader = s_MiniLoader_t12x;
                MiniLoaderSize = sizeof(s_MiniLoader_t12x);
                /* download the miniloader to the bootrom */
                download_arg.EntryPoint = NVFLASH_MINILOADER_ENTRY_56K;
            }
            else
            {
                b = NV_FALSE;
                NvAuPrintf("UnKnown device found\n");
                goto fail;
            }
#else
            pMiniLoader = s_Dtb_Miniloader;
            MiniLoaderSize = sizeof(s_Dtb_Miniloader);
            if (DeviceId == 0x30)
                download_arg.EntryPoint = NVFLASH_MINILOADER_ENTRY_40K;
            else
                download_arg.EntryPoint = NVFLASH_MINILOADER_ENTRY_56K;
#endif

            NvFlashCommandGetOption(NvFlashOption_AppletFile,
                (void*)&pAppletFile);
            if (pAppletFile)
            {
                pMiniLoader = NULL;
                e = NvOsFopen(pAppletFile, NVOS_OPEN_READ, &hAppletFile);
                VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

                e = NvOsFstat(hAppletFile, &AppletFileStat);
                VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);

                MiniLoaderSize = AppletFileStat.size;

                pMiniLoader = NvOsAlloc(MiniLoaderSize);

                if (!pMiniLoader)
                {
                    e = NvError_InsufficientMemory;
                    err_str = "file stat failed";
                    goto fail;
                }

                e = NvOsFread(hAppletFile, pMiniLoader,
                                MiniLoaderSize, &ReadBytes);
                VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);
            }

            if (DeviceId == 0x30)
            {
                e = NvBootHostRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_DownloadExecute,
                    NULL, (NvU8 *)&download_arg, sizeof(download_arg),
                    MiniLoaderSize, pMiniLoader,
                    op_mode, NULL, &msg_buff);
            }
            else if (DeviceId == 0x40)
            {
               e = NvBootHostT12xRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_DownloadExecute,
                    NULL, (NvU8 *)&download_arg, sizeof(download_arg),
                    MiniLoaderSize, pMiniLoader,
                    op_mode, NULL, &msg_buff);
            }
            else
            {
                e = NvBootHostT1xxRcmCreateMsgFromBuffer(
                    NvFlashRcmOpcode_DownloadExecute,
                    NULL, (NvU8 *)&download_arg, sizeof(download_arg),
                    MiniLoaderSize, pMiniLoader,
                    op_mode, NULL, &msg_buff);
            }

            VERIFY(e == NvSuccess, err_str = "miniloader message create failed";
                goto fail);

            #undef NVFLASH_MINILOADER_ENTRY_32K
            #undef NVFLASH_MINILOADER_ENTRY_40K
            #undef NVFLASH_MINILOADER_ENTRY_56K
        }

        if (s_NvflashOprMode == NV_FLASH_BOOTROM_MODE) {
            b = nvflash_rcm_send(hUsb, msg_buff, &response);
            VERIFY(b, goto fail);
        }
        nvflash_rcm_close(hUsb);

    }

    e = NvOsGetSystemTime(&StartTime);
    VERIFY(e == NvSuccess, err_str = "Getting system time failed";
        goto fail);

    e = Nv3pOpen( &s_hSock, TransportMode, devinstance);
    VERIFY( e == NvSuccess, err_str = "connection failed"; goto fail );

    // FIXME: add support for platform info into bootloader?
    if( s_Resume || (TransportMode == Nv3pTransportMode_Sema))
    {
        b = NV_TRUE;
        goto clean;
    }

#ifdef NVHOST_QUERY_DTB
    e = nvgetdtb_read_boardid();
    VERIFY(e == NvSuccess, err_str = "ReadBoardId failed"; goto fail);
    b = NV_TRUE;
    goto clean;
#endif

    /* Check if BoardID read should be skipped by miniloader */
    e = NvFlashCommandGetOption(
        NvFlashOption_SkipAutoDetect, (void*)&SkipAutoDetect);
    VERIFY(e == NvSuccess, goto fail);

    /* Check if it is common bct or single bct */
    e = NvFlashCommandGetOption(NvFlashOption_Bct, (void *)&pBctFile);
    VERIFY(e == NvSuccess, goto fail);

    if(pBctFile->BctOrCfgFile)
    {
        b = nvflash_check_cfgtype(pBctFile->BctOrCfgFile);
        if (!b)
            SkipAutoDetect = NV_TRUE;
    }

    s_PlatformInfo.SkipAutoDetect = SkipAutoDetect;

    if (SkipAutoDetect)
        NvAuPrintf("Skipping BoardID read at miniloader level\n");


    /* get the platform info */
    cmd = Nv3pCommand_GetPlatformInfo;
    e = Nv3pCommandSend( s_hSock, cmd, (NvU8 *)&s_PlatformInfo, 0 );

    b = nvflash_wait_status();
    VERIFY( b, err_str = "unable to retrieve platform info ";
        goto fail );

    VERIFY( s_PlatformInfo.BootRomVersion == 1, err_str = "unknown bootrom version";
        goto fail );


    // NCT related flow
    e = NvFlashCommandGetOption(NvFlashOption_NCTPart, (void *)&pNCTPart);
    if (e != NvSuccess)
    {
        NvAuPrintf("%s: error getting NCT part info.\n", __FUNCTION__);
        goto fail;
    }
    if (!pNCTPart->nctfilename)
        s_IsNCTPartInCommandline = NV_FALSE;

    e = NvFlashCommandGetOption(NvFlashOption_SetBct,
                                (void *)&IsBctSet);
    if (IsBctSet && s_IsNCTPartInCommandline)
    {
        NvU32 BoardInfoOffset = 0;

        e = NvOsFopen(pNCTPart->nctfilename, NVOS_OPEN_READ, &hFile);

        if (e != NvSuccess)
        {
            NvAuPrintf("Cannot open NCT file %s, error code %x\n",
                        pNCTPart->nctfilename, e);
            goto fail;
        }

        NvAuPrintf("NCT file %s given in commandline\n", pNCTPart->nctfilename);
        /* Temporary parser call to get BoardInfo from NCT */
        NvOsMemset(&callback_arg, 0, sizeof(callback_arg));
        NvOsMemset(&Header, 0, sizeof(nct_part_head_type));
        NvOsMemcpy(&Header.magicId, NCT_MAGIC_ID, sizeof(NCT_MAGIC_ID));
           callback_arg.Header = &Header;

        NvParseCaseSensitive(NV_TRUE);
        e = nvflash_parser(hFile, &nvflash_configtable_parser_callback,
                           NULL, &callback_arg);

        if(hFile)
            NvOsFclose(hFile);

        NctEntry = callback_arg.NctEntry;

        BoardInfoOffset = (NCT_ENTRY_SIZE * NCT_ID_BOARD_INFO) +
                                                NCT_ENTRY_DATA_OFFSET;
        NctEntry += BoardInfoOffset;
        s_NctCustInfo.NCTBoardInfo.proc_board_id =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.proc_sku =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.proc_fab =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.pmu_board_id =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.pmu_sku =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.pmu_fab =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.display_board_id =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.display_sku =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));
        NctEntry+=4;
        s_NctCustInfo.NCTBoardInfo.display_fab =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));

        NctEntry+=4;

        NctEntry = callback_arg.NctEntry;
        BoardInfoOffset = (NCT_ENTRY_SIZE * NCT_ID_BATTERY_COUNT) +
                                                NCT_ENTRY_DATA_OFFSET;
        NctEntry += BoardInfoOffset;
        s_NctCustInfo.BatteryCount.count =
                                    ((*(NctEntry+1))<<8)|(*(NctEntry));

        NctEntry = callback_arg.NctEntry;
        BoardInfoOffset = (NCT_ENTRY_SIZE * NCT_ID_BATTERY_MAKE) +
                                                NCT_ENTRY_DATA_OFFSET;
        NctEntry += BoardInfoOffset;
        i = 0;
        while(i < sizeof(s_NctCustInfo.BatteryMake))
        {
            s_NctCustInfo.BatteryMake.make[i]= *(NctEntry+i);
            i++;
        }

        NctEntry = callback_arg.NctEntry;
        BoardInfoOffset = (NCT_ENTRY_SIZE * NCT_ID_DDR_TYPE) +
                                                NCT_ENTRY_DATA_OFFSET;
        NctEntry += BoardInfoOffset;
        i = 0;
        while (i < sizeof(s_NctCustInfo.DDRType)/sizeof(s_NctCustInfo.DDRType.ddr_type_data[0]))
        {
               s_NctCustInfo.DDRType.ddr_type_data[i].mrr5 =
                                                       ((*(NctEntry+1))<<8)|(*(NctEntry));
               NctEntry+=2;
               s_NctCustInfo.DDRType.ddr_type_data[i].mrr6 =
                                                       ((*(NctEntry+1))<<8)|(*(NctEntry));
               NctEntry+=2;
               s_NctCustInfo.DDRType.ddr_type_data[i].mrr7 =
                                                       ((*(NctEntry+1))<<8)|(*(NctEntry));
               NctEntry+=2;
               s_NctCustInfo.DDRType.ddr_type_data[i].mrr8 =
                                                       ((*(NctEntry+1))<<8)|(*(NctEntry));
               NctEntry+=2;
               s_NctCustInfo.DDRType.ddr_type_data[i].index =
                                                       ((*(NctEntry+1))<<8)|(*(NctEntry));
               NctEntry+=2;
               i++;
        }

        // There can be a case that NCT and EEPROM both present. In that case don't overwrite with NCT info.
        if (!s_PlatformInfo.BoardId.BoardNo)
        {
            s_PlatformInfo.BoardId.BoardNo =
                                s_NctCustInfo.NCTBoardInfo.proc_board_id;
            s_PlatformInfo.BoardId.BoardFab =
                                s_NctCustInfo.NCTBoardInfo.proc_fab;
            s_PlatformInfo.BoardId.SkuType =
                                s_NctCustInfo.NCTBoardInfo.proc_sku;
        }
    }

// Convert chip sku to chip name as per chip id
    if (s_PlatformInfo.ChipId.Id == 0x30)
    {
        switch (s_PlatformInfo.ChipSku)
        {
            case Nv3pChipName_Development: pChipNameStr = "development"; break;
            default: pChipNameStr = "unknown"; break;
        }
    }
    else if (s_PlatformInfo.ChipId.Id == 0x35)
    {
        switch (s_PlatformInfo.ChipSku)
        {
            case Nv3pChipName_Development: pChipNameStr = "development"; break;
            default: pChipNameStr = "unknown"; break;
        }
    }
    else
    {
        pChipNameStr = "unknown";
    }

    NvAuPrintf(
    "System Information:\n"
    "   chip name: %s\n"
    "   chip id: 0x%x major: %d minor: %d\n"
    "   chip sku: 0x%x\n"
    "   chip uid: 0x%.8x%.8x%.8x%.8x\n"
    "   macrovision: %s\n"
    "   hdcp: %s\n"
    "   jtag: %s\n"
    "   sbk burned: %s\n"
    "   board id: %d\n"
    "   warranty fuse: %d\n",
    pChipNameStr,
    s_PlatformInfo.ChipId.Id, s_PlatformInfo.ChipId.Major, s_PlatformInfo.ChipId.Minor,
    s_PlatformInfo.ChipSku,
    (NvU32)(s_PlatformInfo.ChipUid.ecid_3), (NvU32)(s_PlatformInfo.ChipUid.ecid_2),
    (NvU32)(s_PlatformInfo.ChipUid.ecid_1),(NvU32)(s_PlatformInfo.ChipUid.ecid_0),
    (s_PlatformInfo.MacrovisionEnable) ? "enabled" : "disabled",
    (s_PlatformInfo.HdmiEnable) ? "enabled" : "disabled",
    (s_PlatformInfo.JtagEnable) ? "enabled" : "disabled",
    (s_PlatformInfo.SbkBurned) ? "true" : "false",
    s_PlatformInfo.BoardId.BoardNo,
    s_PlatformInfo.WarrantyFuse);

    if( s_PlatformInfo.DkBurned == Nv3pDkStatus_NotBurned )
        str = "false";
    else if ( s_PlatformInfo.DkBurned == Nv3pDkStatus_Burned )
        str = "true";
    else
        str = "unknown";
    NvAuPrintf( "   dk burned: %s\n", str );

    switch( s_PlatformInfo.SecondaryBootDevice ) {
    case Nv3pDeviceType_Nand: pBootDevStr = "nand"; break;
    case Nv3pDeviceType_Emmc: pBootDevStr = "emmc"; break;
    case Nv3pDeviceType_Spi: pBootDevStr = "spi"; break;
    case Nv3pDeviceType_Ide: pBootDevStr = "ide"; break;
    case Nv3pDeviceType_Nand_x16: pBootDevStr = "nand_x16"; break;
    case Nv3pDeviceType_Usb3: pBootDevStr = "usb3"; break;
    case Nv3pDeviceType_Sata: pBootDevStr = "sata"; break;
    case Nv3pDeviceType_Snor: pBootDevStr = "snor"; break;
    default: pBootDevStr = "unknown"; break;
    }
    NvAuPrintf( "   boot device: %s\n", pBootDevStr );

    NvAuPrintf(
"   operating mode: %d\n"
"   device config strap: %d\n"
"   device config fuse: %d\n"
"   sdram config strap: %d\n",
    s_PlatformInfo.OperatingMode, s_PlatformInfo.DeviceConfigStrap,
    s_PlatformInfo.DeviceConfigFuse, s_PlatformInfo.SdramConfigStrap );

    NvAuPrintf( "\n" );
    OpMode = s_PlatformInfo.OperatingMode;

    nvflash_set_platforminfo(&s_PlatformInfo);
    NvAuPrintf("RCM communication completed\n");

    b = NvFlashRetrieveBoardDetails();
    VERIFY(b, goto fail);

    b = NV_TRUE;
    goto clean;

fail:
    if( err_str )
    {
        NvAuPrintf( "%s NvError 0x%x\n", err_str , e);
    }

    b = NV_FALSE;

clean:

    if (hAppletFile)
        NvOsFclose(hAppletFile);

    if (pMiniLoader && pAppletFile)
        NvOsFree(pMiniLoader);

    return b;
}

NvBool
nvflash_configfile( const char *filename )
{
    NvBool b;
    NvError e;
    NvFlashConfigFileHandle hCfg = 0;
    NvFlashDevice *dev = 0;
    NvU32 nDev = 0;
    const char *err_str = 0;

    e = NvFlashConfigFileParse( filename, &hCfg );
    VERIFY( e == NvSuccess,
        err_str = NvFlashConfigGetLastError(); goto fail );

    e = NvFlashConfigListDevices( hCfg, &nDev, &dev );
    VERIFY( e == NvSuccess,
        err_str = NvFlashConfigGetLastError(); goto fail );

    VERIFY( nDev, goto fail );
    VERIFY( dev, goto fail );

    s_Devices = dev;
    s_nDevices = nDev;

    b = NV_TRUE;
    goto clean;

fail:
    if( err_str )
    {
        NvAuPrintf( "nvflash configuration file error: %s\n", err_str );
    }

    b = NV_FALSE;

clean:
    NvFlashConfigFileClose( hCfg );
    return b;
}

/*
* nvflash_sendfile: send data present in file "filename" to nv3p server.
* If NoOfBytes is valid (>0 && <file_size) then only those many number
* of bytes will be sent.
*/
NvBool
nvflash_sendfile( const char *filename, NvU64 NoOfBytes)
{
    NvBool b;
    NvError e;
    NvOsFileHandle hFile = 0;
    NvOsStatType stat;
    NvU8 *buf = 0;
    NvU32 size;
    NvU64 total;
    size_t bytes;
    NvU64 count;
    char *spinner = "-\\|/";
    NvU32 spin_idx = 0;
    char *err_str = 0;

    NvAuPrintf( "sending file: %s\n", filename );

    e = NvOsFopen( filename, NVOS_OPEN_READ, &hFile );
    VERIFY( e == NvSuccess, err_str = "file open failed"; goto fail );

    e = NvOsFstat( hFile, &stat );
    VERIFY( e == NvSuccess, err_str = "file stat failed"; goto fail );

    total = stat.size;
    if(NoOfBytes && (NoOfBytes < stat.size))
    {
        total = NoOfBytes;
    }
    buf = NvOsAlloc( NVFLASH_DOWNLOAD_CHUNK );
    VERIFY( buf, err_str = "buffer allocation failed"; goto fail );

    count = 0;
    while( count != total )
    {
        size = (NvU32) NV_MIN( total - count, NVFLASH_DOWNLOAD_CHUNK );

        e = NvOsFread( hFile, buf, size, &bytes );
        VERIFY( e == NvSuccess, err_str = "file read failed"; goto fail );
        VERIFY( bytes == size, goto fail );

        e = Nv3pDataSend( s_hSock, buf, size, 0 );
        VERIFY( e == NvSuccess, err_str = "data send failed"; goto fail );

        count += size;
        if( !s_Quiet )
        {
            NvAuPrintf( "\r%c %llu/%llu bytes sent", spinner[spin_idx],
                count, total );
            spin_idx = (spin_idx + 1) % 4;
        }
        nvflash_report_progress(size);
    }
    NvAuPrintf( "\n%s sent successfully\n", filename );

    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;
    if( err_str )
    {
        NvAuPrintf( "%s NvError 0x%x\n", err_str , e);
    }

clean:
    NvOsFclose( hFile );
    NvOsFree( buf );
    return b;
}

NvBool nvflash_recvfile(const char *pFileName, NvU64 Size)
{
    NvBool Ret = NV_TRUE;
    NvError e = NvSuccess;
    NvU32 Bytes = 0;
    NvU64 BytesRecieved = 0;
    NvU8 *pBuffer = NULL;
    char *pErrStr = NULL;
    char Spinner[] = "-\\|/";
    NvU32 SpinIndex = 0;
    NvOsFileHandle hFile = NULL;
    NvBool FileCreateFailed = NV_FALSE;

    #define NVFLASH_UPLOAD_CHUNK (1024 * 2)

    NvAuPrintf("Receiving file: %s, expected size: %Lu bytes\n",
        pFileName, Size);

    e = NvOsFopen(pFileName, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile);
    if (e != NvSuccess)
    {
        Ret = NV_FALSE;
        NvAuPrintf("Failed to open file \"%s\" for write. NvError 0x%x\n"
                   "Continuing to receive data without saving.\n",
                   pFileName, e);
        FileCreateFailed = NV_TRUE;
    }

    pBuffer = NvOsAlloc(NVFLASH_UPLOAD_CHUNK);
    VERIFY(pBuffer != NULL, pErrStr = "buffer allocation failure"; goto fail);

    while (BytesRecieved < Size)
    {
        e = Nv3pDataReceive(s_hSock, pBuffer, NVFLASH_UPLOAD_CHUNK, &Bytes, 0);
        VERIFY(e == NvSuccess, pErrStr = "data receive failure"; goto fail);

        if (FileCreateFailed == NV_FALSE)
        {
            e = NvOsFwrite(hFile, pBuffer, Bytes);

            if (e != NvSuccess)
            {
                Ret = NV_FALSE;
                NvAuPrintf("Failed to write to file \"%s\". NvError 0x%x\n"
                           "Continuing to receive data without saving.\n",
                           pFileName, e);
                FileCreateFailed = NV_TRUE;
            }
        }

        BytesRecieved += Bytes;

        if(!s_Quiet)
        {
            NvAuPrintf("\r%c %Lu/%Lu bytes received", Spinner[SpinIndex],
                       BytesRecieved, Size);
            SpinIndex = (SpinIndex + 1) % 4;
        }
    }

    NvAuPrintf("\n");
    if (FileCreateFailed == NV_FALSE)
        NvAuPrintf("file received successfully\n");

    #undef NVFLASH_UPLOAD_CHUNK

    goto clean;

fail:
    Ret = NV_FALSE;
    if (pErrStr)
    {
        NvAuPrintf("%s NvError 0x%x\n", pErrStr, e);
    }

clean:
    if (hFile != NULL)
        NvOsFclose(hFile);
    if (pBuffer != NULL)
        NvOsFree(pBuffer);

    return Ret;
}

char *
nvflash_status_string( Nv3pStatus s )
{
    switch( s ) {
#define NV3P_STATUS(_name_, _value_, _desc_) \
    case Nv3pStatus_##_name_ : return _desc_;
    #include "nv3p_status.h"
#undef NV3P_STATUS
    default:
        return "unknown";
    }
}

NvBool
nvflash_wait_status( void )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdStatus *status_arg = 0;

    e = Nv3pCommandReceive( s_hSock, &cmd, (void **)&status_arg, 0 );
    VERIFY( e == NvSuccess, goto fail );
    VERIFY( cmd == Nv3pCommand_Status, goto fail );

    e = Nv3pCommandComplete( s_hSock, cmd, status_arg, 0 );
    VERIFY( e == NvSuccess, goto fail );

    VERIFY( status_arg->Code == Nv3pStatus_Ok, goto fail );
    return NV_TRUE;

fail:
    if( status_arg )
    {
        char *str;
        str = nvflash_status_string( status_arg->Code );
        NvAuPrintf(
            "bootloader status: %s (code: %d) message: %s flags: %d\n",
            str, status_arg->Code, status_arg->Message, status_arg->Flags );
    }
    return NV_FALSE;
}

void nvflash_thread_execute_command(void *args)
{
        NvError e = NvSuccess;
        NvFlashDevice *d;
        NvFlashPartition *p;
        NvU32 i,j;
        NvBool CheckStatus = NV_TRUE;
        const char *PartitionName = NULL;
        Nv3pNonBlockingCmdsArgs* pThreadArgs = (Nv3pNonBlockingCmdsArgs*)args;
        Nv3pCmdFormatPartition* pFormatArgs =
                                    (Nv3pCmdFormatPartition*)pThreadArgs->CmdArgs;
        if(pThreadArgs->cmd == Nv3pCommand_FormatAll)
        {
            NvAuPrintf( "Formatting all partitions please wait.. " );
            e = Nv3pCommandSend( s_hSock,
                pThreadArgs->cmd, pThreadArgs->CmdArgs, 0 );
        }
        else if(pFormatArgs->PartitionId == (NvU32)-1)
        {
            CheckStatus = NV_FALSE;
            d = s_Devices;
            /* format all the partitions */
            for( i = 0; i < s_nDevices; i++, d = d->next )
            {
                p = d->Partitions;
                for( j = 0; j < d->nPartitions; j++, p = p->next )
                {
                    if (nvflash_check_skippart(p->Name))
                    {
                        /* This partition must be skipped from formatting procedure */
                        NvAuPrintf("Skipping Formatting partition %d %s\n",p->Id, p->Name);
                        continue;
                    }
                    NvAuPrintf( "\bFormatting partition %d %s please wait.. ", p->Id, p->Name );
                    pFormatArgs->PartitionId = p->Id;
                    e = Nv3pCommandSend( s_hSock,
                        pThreadArgs->cmd, pThreadArgs->CmdArgs, 0 );
                    if( e != NvSuccess)
                        break;
                    s_FormatSuccess = nvflash_wait_status();
                    if( !s_FormatSuccess)
                        break;
                    NvAuPrintf( "%s\n", s_FormatSuccess ? "done!" : "FAILED!" );
                }
                if( (e != NvSuccess) || !s_FormatSuccess)
                    break;
            }
        }
        else
        {
            PartitionName =
                nvflash_get_partNameById(pFormatArgs->PartitionId, NV_FALSE);
            NvAuPrintf( "Formatting partition %d %s please wait.. ",
                       pFormatArgs->PartitionId, PartitionName);
            e = Nv3pCommandSend( s_hSock,
                pThreadArgs->cmd, pThreadArgs->CmdArgs, 0 );
        }

        if(CheckStatus)
        {
            if( e == NvSuccess )
            {
                s_FormatSuccess = nvflash_wait_status();
            }
            else
            {
                s_FormatSuccess = NV_FALSE;
                NvAuPrintf("Command Execution failed cmd %d, error 0x%x \n",pThreadArgs->cmd,e);
            }
        }
        NvOsSemaphoreSignal(s_FormatSema);
}

NvBool
nvflash_cmd_formatpartition( Nv3pCommand cmd, NvU32 PartitionNumber)
{
    Nv3pCmdFormatPartition args;
    NvError e;
    NvBool b = NV_TRUE;
    char *spinner = "-\\|/-";
    NvU32 spin_idx = 0;
    NvOsThreadHandle thread = 0;
    Nv3pNonBlockingCmdsArgs ThreadArgs;

    if((cmd != Nv3pCommand_FormatAll) && (cmd != Nv3pCommand_FormatPartition))
        return NV_FALSE;

    e = NvOsSemaphoreCreate(&s_FormatSema, 0);
    if (e != NvSuccess)
        return NV_FALSE;

    s_FormatSuccess = NV_FALSE;
    args.PartitionId = PartitionNumber;
    ThreadArgs.cmd = cmd;
    ThreadArgs.CmdArgs = (void *)&args;
    e = NvOsThreadCreate(
            nvflash_thread_execute_command,
            (void *)&ThreadArgs,
            &thread);
    if( e == NvSuccess )
    {
        do
        {
            if(!s_Quiet)
            {
                NvAuPrintf( "%c\b", spinner[spin_idx]);
                spin_idx = (spin_idx + 1) % 5;
            }
            e = NvOsSemaphoreWaitTimeout(s_FormatSema, 100);
        } while (e != NvSuccess);

        NvAuPrintf( "%s\n", s_FormatSuccess ? "done!" : "FAILED!" );
        NvOsThreadJoin(thread);
    }

    NvOsSemaphoreDestroy(s_FormatSema);
    s_FormatSema = 0;

    if(!s_FormatSuccess)
        b = NV_FALSE;

    return b;
}

/** per-command implementation */
static NvBool
nvflash_cmd_settime( void )
{
    Nv3pCommand cmd;
    Nv3pCmdSetTime arg;
    NvBool b= NV_FALSE;
    NvError e = NvSuccess;
    NvOsMemset(&arg, 0, sizeof(Nv3pCmdSetTime));
    NvOsGetSystemTime((NvOsSystemTime*)&arg);
    cmd = Nv3pCommand_SetTime;
    NvAuPrintf("Setting current time %d seconds with epoch time as base\r\n",arg.Seconds);
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    b = nvflash_wait_status();
    VERIFY( b, goto fail );

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

NvBool
nvflash_check_skippart(char *name)
{
    NvBool res = NV_FALSE;    /* partition never skipped by default */
    NvError e;
    NvFlashSkipPart *skipped=NULL;
    NvU32 i = 0;

    do
    {
        e = NvFlashCommandGetOption(NvFlashOption_SkipPartition, (void *)&skipped);
        if (e != NvSuccess)
        {
            NvAuPrintf("%s: error getting skip part info.\n", __FUNCTION__);
            break;
        }

        if (skipped)
        {
            for (i = 0; i < skipped->number; i++)
            {
                if (!NvOsStrcmp(skipped->pt_name[i], (const char *)name))
                {
                    /* Found partition name in skip list, exit loop and return info */
                    res = NV_TRUE;
                    break;
                }
            }
        }
        else
        {
            NvAuPrintf("%s: error invalid skip part info\n", __FUNCTION__);
        }

    } while(0);

    return res;
}

static NvBool nvflash_check_skiperror(void)
{
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    const char *err_str = 0;
    NvU32 i, j;
    NvU32 SizeMultiple = 0, TotalCardSectors = 0, TotalPartitions = 0;
    NvU32 PartIndex = 0, nSkipPart = 0;
    NvU64 TotalSize = 0;

    Nv3pCommand cmd = 0;
    Nv3pCmdGetDevInfo DevInfo;
    NvFlashSkipPart *pSkipped = NULL;
    NvFlashDevice *pDevices = NULL;
    NvFlashPartition *pPartitions = NULL;
    NvFlashCmdGetPartitionTable *a = NULL;
    Nv3pPartitionTableLayout *pPartitionInfoLayout = NULL;
    Nv3pPartitionInfo *pPartitionInfo = NULL;

    // Retrieving skip list
    e = NvFlashCommandGetOption(NvFlashOption_SkipPartition, (void *)&pSkipped);
    // If --skip is not present , then return true & don't proceed with error check
    if (pSkipped->number == 0)
    {
        return NV_TRUE;
    }

    // Set this flag, if skip is present, to not to delete entire device. Needs
    // to do format partitions individually
    s_bIsFormatPartitionsNeeded = NV_TRUE;

    // CHECK 2: If there is a typo in skip list,
    // give a warning for the unknown partition in skip list
    pDevices = s_Devices;
    // Searching for skip partition name in partition table
    for (i = 0; i < s_nDevices; i++, pDevices = pDevices->next)
    {
        pPartitions = pDevices->Partitions;
        for (j = 0; j < pDevices->nPartitions; j++, pPartitions = pPartitions->next)
        {
            if (nvflash_check_skippart(pPartitions->Name))
                nSkipPart++;
        }
    }
    if (nSkipPart != pSkipped->number)
        NvAuPrintf("Warning: Unknown partition(s) in the skip list.\n");

    // Get the BlockDevInfo info
    NvOsMemset(&DevInfo, 0, sizeof(Nv3pCmdGetDevInfo));
    cmd = Nv3pCommand_GetDevInfo;
    e = Nv3pCommandSend(s_hSock, cmd, (NvU8 *)&DevInfo, 0);
    VERIFY(e == NvSuccess, err_str = "Failed to complete GetDevInfo command";b = NV_FALSE;
        goto fail);

    b = nvflash_wait_status();
    VERIFY(b, err_str = "Unable to retrieve device info"; b = NV_FALSE; goto fail);

    SizeMultiple = DevInfo.BytesPerSector * DevInfo.SectorsPerBlock;
    TotalCardSectors = DevInfo.SectorsPerBlock * DevInfo.TotalBlocks;

    // Calculating the total number of partitions and allocating memory to pPartitionInfoLayout
    // which will store the new partition layout
    pDevices = s_Devices;
    for (i = 0 ; i < s_nDevices ; i++, pDevices = pDevices->next)
    {
        pPartitions = pDevices->Partitions;
        for (j = 0 ; j < pDevices->nPartitions ; j++, pPartitions = pPartitions->next)
            TotalPartitions++;
    }
    pPartitionInfoLayout = NvOsAlloc(TotalPartitions * sizeof(Nv3pPartitionTableLayout));
    if (!pPartitionInfoLayout)
    {
        err_str = "Insufficient system memory";
        b = NV_FALSE;
        goto fail;
    }
    NvOsMemset(pPartitionInfoLayout, 0, TotalPartitions * sizeof(Nv3pPartitionTableLayout));

    a = NvOsAlloc(sizeof(NvFlashCmdGetPartitionTable));
    if (!a)
    {
        err_str = "Insufficient system memory";
        b = NV_FALSE;
        goto fail;
    }

    NvOsMemset(a, 0, sizeof(NvFlashCmdGetPartitionTable));

    // Plotting the new partition table layout & filling pPartitonTableLayout
    pDevices = s_Devices;
    for (i = 0 ; i < s_nDevices ; i++, pDevices = pDevices->next)
    {
        pPartitions = pDevices->Partitions;
        for (j = 0 ; j < pDevices->nPartitions ; j++, pPartitions = pPartitions->next)
        {
            NvU64 PartitionSize = 0;
            if (pPartitions->AllocationAttribute == 0x808)
            {
                // Temporary storing UDA info
                PartitionSize = nvflash_util_roundsize(pPartitions->Size, SizeMultiple);
                TotalSize += PartitionSize;

                pPartitions = pPartitions->next;j++;
                // GPT info with size calculated using TotalCardSize
                pPartitionInfoLayout[PartIndex].StartLogicalAddress  =
                    pPartitionInfoLayout[PartIndex-1].StartLogicalAddress
                    + pPartitionInfoLayout[PartIndex-1].NumLogicalSectors;
                pPartitionInfoLayout[PartIndex].NumLogicalSectors =
                    (NvU32)(TotalCardSectors -(TotalSize / DevInfo.BytesPerSector));
                PartIndex++;

                // Filling UDA info
                pPartitionInfoLayout[PartIndex].StartLogicalAddress =
                    pPartitionInfoLayout[PartIndex-1].StartLogicalAddress
                    + pPartitionInfoLayout[PartIndex-1].NumLogicalSectors;
                pPartitionInfoLayout[PartIndex].NumLogicalSectors =
                    (NvU32)(PartitionSize / DevInfo.BytesPerSector);

            }
            else
            {
                if (PartIndex == 0)
                    pPartitionInfoLayout[PartIndex].StartLogicalAddress = 0;
                else
                {
                    pPartitionInfoLayout[PartIndex].StartLogicalAddress =
                        pPartitionInfoLayout[PartIndex-1].StartLogicalAddress
                        + pPartitionInfoLayout[PartIndex-1].NumLogicalSectors;
                }
                PartitionSize = pPartitions->Size;
                PartitionSize = nvflash_util_roundsize(PartitionSize, SizeMultiple);
                TotalSize += PartitionSize;
                pPartitionInfoLayout[PartIndex].NumLogicalSectors =
                    (NvU32)(PartitionSize / DevInfo.BytesPerSector);
            }
            // Store PT partition attributes for loading partition table in case of skip.
            if (!NvOsStrcmp(pPartitions->Name, "PT"))
            {
                a->NumLogicalSectors = pPartitionInfoLayout[PartIndex].NumLogicalSectors;
                a->StartLogicalSector = pPartitionInfoLayout[PartIndex].StartLogicalAddress;
                a->filename = NULL;
            }
            PartIndex++;
        }
    }

    if (!a->NumLogicalSectors)
    {
        err_str = "PT partition absent in the cfg file";
        b = NV_FALSE;
        goto fail;
    }

    // CHECK 3: If skip is being used on a formatted device
    // Retrieving stored partition table in the device.

    // Note: Partiontable attributes(NumLogicalSectors & StartLogicalSector) are calculated
    // from the host config file which are used/sent to load partition table present on the device.
    // If these attributes mismatch, then it will error out.
    a->ReadBctFromSD = NV_FALSE;
    b = nvflash_cmd_getpartitiontable(a, &pPartitionInfo);
    VERIFY(b, err_str = "Unable to retrieve Partition table from device. Partition\
                         table attributes mismatch"; goto fail);
    b = nvflash_wait_status();
    VERIFY(b, goto fail);
    // CHECK 4: If there is change in partition layout.
    for (i = 0 ; i < TotalPartitions ; i++)
    {
        if (nvflash_check_skippart((char*)pPartitionInfo[i].PartName))
        {
            if ((pPartitionInfo[i].StartLogicalAddress !=
                pPartitionInfoLayout[i].StartLogicalAddress) ||
                (pPartitionInfo[i].NumLogicalSectors !=
                pPartitionInfoLayout[i].NumLogicalSectors))
            {
                NvAuPrintf("Partition boundaries mismatch for %s partition\n",
                           pPartitionInfo[i].PartName);
                b = NV_FALSE;
                goto fail;
            }
        }
        else
            continue;
    }

fail:
    if (a)
        NvOsFree(a);
    if (pPartitionInfo)
        NvOsFree(pPartitionInfo);
    if (pPartitionInfoLayout)
        NvOsFree(pPartitionInfoLayout);
    if (e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    return b;
}


static NvBool
nvflash_cmd_deleteall(void)
{
    Nv3pCmdStatus *status_arg = 0;
    Nv3pCommand cmd = 0;
    NvError e;
    NvAuPrintf("deleting device partitions\n");

    /* delete everything on the device before creating the partitions */
    cmd = Nv3pCommand_DeleteAll;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, 0, 0)
    );

    e = Nv3pCommandReceive(s_hSock, &cmd, (void **)&status_arg, 0);
    VERIFY(e == NvSuccess, goto fail);
    VERIFY(cmd == Nv3pCommand_Status, goto fail);

    e = Nv3pCommandComplete(s_hSock, cmd, status_arg, 0);
    VERIFY(e == NvSuccess, goto fail);

    VERIFY((status_arg->Code == Nv3pStatus_Ok ||
               status_arg->Code == Nv3pStatus_NotSupported), goto fail);

    if (status_arg->Code == Nv3pStatus_NotSupported)
        s_bIsFormatPartitionsNeeded = NV_TRUE;
    return NV_TRUE;
fail:
    return NV_FALSE;
}

/* Reads to partition, which are to be backed up, in <partition_name>.bin and
 * updates the filename of partition structure with <partition_name>.bin to
 * restore them back in create command.
 */
NvBool nvflash_backup_partitions(void)
{
    NvU32 i;
    NvU32 j;
    NvBool b  = NV_TRUE;
    NvError e = NvSuccess;
    const char *err_str = NULL;
    NvFlashDevice *d    = NULL;
    NvFlashPartition *p = NULL;
    NvFlashBackupPartition *bPartition     = NULL;
    NvFlashBackupPartition *bPartitionList = NULL;
    Nv3pTransportMode TransportMode;
    NvU32 partitionNameLength = MAX_PARTITION_NAME_LENGTH +
                                sizeof(NvU32) * 2 + 1 +
                                NvOsStrlen(BACKUP_FILE_EXTENSION);
    NvFlashNCTPart *pNCTPart = NULL;


    // Check if NCT is supplied as part of commandline to determine whether or not
    // to take a backup of NCT.
    e = NvFlashCommandGetOption(NvFlashOption_NCTPart, (void *)&pNCTPart);
    if (e != NvSuccess)
    {
        NvAuPrintf("%s: error getting NCT part info.\n", __FUNCTION__);
        goto fail;
    }

    /* Create the list of partitions which are to be backed up. */
    d = s_Devices;
    for(i = 0; i < s_nDevices; i++, d = d->next)
    {
        p = d->Partitions;
        for(j = 0; j < d->nPartitions; j++, p = p->next)
        {
            if(p->PartitionAttribute &
               (1 << NvPartMgrPartitionAttribute_Preserve))
            {
                if ((NvOsStrcmp(p->Name, s_NCTPartName) == 0 && pNCTPart->nctfilename))
                {
                    NvAuPrintf("Overriding NCT partition with supplied NCT file\n");
                    // TODO : Query vendor or pid & compare with supplied to warn the user
                }
                else
                {
                    // Take backup of NCT only if new NCT is not supplied
                    bPartition = NvOsAlloc(sizeof(*bPartition));
                    VERIFY(bPartition != NULL, goto fail);
                    bPartition->partition = p;
                    bPartition->next = bPartitionList;
                    bPartitionList = bPartition;
                }
            }
        }
    }

    e = NvFlashCommandGetOption(NvFlashOption_TransportMode,
                                (void *)&TransportMode);
    VERIFY(e == NvSuccess, goto fail);

    if(bPartitionList == NULL || TransportMode == Nv3pTransportMode_Sema)
        goto clean;

    /* Deallocate s_pPartitionInfo to make sure that it will get loaded
     * with appropriate partition information read from device.
     */
    if(s_pPartitionInfo)
    {
        NvOsFree(s_pPartitionInfo);
        s_pPartitionInfo = NULL;
    }
    /* If list of partitions which are to be backed up is not empty then read
     * the partition table from device in nvflash_get_partIdByName().
     */
    bPartition = bPartitionList;
    for(; bPartition != NULL; bPartition = bPartition->next)
    {
        NvFlashCmdRead read_part;
        char *filename = NULL;
        if(bPartition->partition->Filename != NULL)
        {
            NvAuPrintf("Not creating back up of %s, using %s\n",
                       bPartition->partition->Name,
                       bPartition->partition->Filename);
            continue;
        }

        NvAuPrintf("Taking backup of %s\n", bPartition->partition->Name);
        NvOsMemset(&read_part, 0, sizeof(read_part));
        filename = NvOsAlloc(sizeof(char) * partitionNameLength);
        if(filename == NULL)
            goto fail;

        NvOsSnprintf(filename, partitionNameLength, "%x_%s%s",
                     (s_PlatformInfo.ChipUid.ecid_0),
                     bPartition->partition->Name, BACKUP_FILE_EXTENSION);

        read_part.PartitionID =
                nvflash_get_partIdByName(bPartition->partition->Name, NV_TRUE);

        read_part.filename = filename;
        if(read_part.PartitionID != 0)
        {
            b = nvflash_cmd_read(&read_part);
            /* Fail if the partition is present on device and could not
             * read the to file.
             */
            if(b == NV_FALSE)
            {
                NvOsFree(filename);
                filename = NULL;
                NvAuPrintf("Error while reading the partition %s\n",
                            bPartition->partition->Name);
                nvflash_wait_status();
                goto fail;
            }
        }
        else
        {
            NvOsFree(filename);
            filename = NULL;
            /* Get status if failed while retrieving the PT from device.
             */
            if(s_pPartitionInfo == NULL)
            {
                nvflash_wait_status();
                NvAuPrintf("Partition table does not exist on device\n");
                NvAuPrintf("Skipping backup of partitions\n");
                goto clean;
            }

            b = NV_FALSE;
            NvAuPrintf("%s partition does not exist on device\n",
                       bPartition->partition->Name);
        }

        if(!b)
        {
            NvAuPrintf("Warning: Update the partition %s with appropriate "
                       "data if needed\n", bPartition->partition->Name);
            continue;
        }
        nvflash_wait_status();
        /* Set partition file name so that create will download this file
         * to partition.
         */
        bPartition->partition->Filename = filename;

        if (s_dPartition == NULL)
        {
            s_dPartition = NvOsAlloc(sizeof(*s_dPartition));
            s_dPartitionList = s_dPartition;
        }
        else
        {
            s_dPartition->next = NvOsAlloc(sizeof(*s_dPartition));
            s_dPartition = s_dPartition->next;
        }
        s_dPartition->partition = bPartition->partition;
        s_dPartition->next = NULL;
        NvAuPrintf("Taking filename to delete %s\n",
                s_dPartition->partition->Filename);
    }
    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;
    NvAuPrintf("Skipping backup of partitions\n");
clean:
    /* Deallocate s_pPartitionInfo to make sure that it will get loaded
     * with appropriate data for any other command/functionality.
     */
    if(s_pPartitionInfo)
    {
        NvOsFree(s_pPartitionInfo);
        s_pPartitionInfo = NULL;
    }
    if(err_str)
        NvAuPrintf("%s\n", err_str);

    if(bPartitionList != NULL && TransportMode != Nv3pTransportMode_Sema)
    {
        char *cfg_filename = NULL;
        e = NvFlashCommandGetOption(NvFlashOption_ConfigFile,
                                    (void *)&cfg_filename);
        if(e == NvSuccess && b == NV_TRUE)
            NvAuPrintf("Continuing create using %s\n", cfg_filename);
    }

    while(bPartitionList)
    {
        bPartition = bPartitionList;
        bPartitionList = bPartitionList->next;
        NvOsFree(bPartition);
    }
    return b;
}

/* Removes to partition files, which are backed up by
 * nvflash_backup_partitions()
 */
NvBool nvflash_remove_backuped_partitions(void)
{
    NvBool b  = NV_TRUE;

    /* If list of partitions which are to be backed up is not empty
     * then remove the partition files.
     */
    s_dPartition = s_dPartitionList;
    for(; s_dPartition != NULL; s_dPartition = s_dPartition->next)
    {
        NvAuPrintf("Removing file %s\n",
                s_dPartition->partition->Filename);
        NvOsFremove(s_dPartition->partition->Filename);
    }
    b = NV_TRUE;
    goto clean;

clean:
    while(s_dPartitionList)
    {
        s_dPartition = s_dPartitionList;
        s_dPartitionList = s_dPartitionList->next;
        NvOsFree(s_dPartition);
    }
    return b;
}

static NvBool NvFlashFlashAllPartitions(void)
{
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvFlashDevice *pDevice = NULL;
    NvFlashPartition *pPartition = NULL;
    NvU32 i;
    NvU32 j;
    NvFlashFuseBypass FuseBypassData;
    NvFlashCmdDownload DownloadCmd;
    const char *pDtbFile = NULL;
    NvFlashNCTPart *pNCTPart = NULL;
    NvBool SkuBypassed = NV_FALSE;

    e = NvFlashCommandGetOption(NvFlashOption_FuseBypass,
                                (void *)&FuseBypassData);
    VERIFY(e == NvSuccess, goto fail);

    // Check if NCT is supplied as part of commandline to
    // determine whether or not to take a backup of NCT.
    e = NvFlashCommandGetOption(NvFlashOption_NCTPart, (void *)&pNCTPart);
    if (e != NvSuccess)
    {
        NvAuPrintf("%s: error getting NCT part info.\n", __FUNCTION__);
        goto fail;
    }

    pDevice = s_Devices;
    /* download all of the partitions with file names specified */
    for (i = 0; i < s_nDevices; i++, pDevice = pDevice->next )
    {
        pPartition = pDevice->Partitions;
        for (j = 0; j < pDevice->nPartitions; j++, pPartition = pPartition->next)
        {
            if (pPartition->Type == Nv3pPartitionType_FuseBypass)
            {
                /* If no skus are to be bypassed then make filename NULL
                 * to avoid download of fusebypass partition
                 */
                if (FuseBypassData.pSkuId == NULL ||
                   s_FuseBypassInfo.sku_type == 0)
                {
                    if (pPartition->Filename)
                        NvOsFree(pPartition->Filename);
                    pPartition->Filename = NULL;
                }
                /* If config file is present in flash.cfg and is also given via
                 * command option then replace the config file with user config
                 * file.
                 */
                else if (FuseBypassData.override_config_file)
                {
                    if (pPartition->Filename)
                        NvOsFree(pPartition->Filename);
                    pPartition->Filename = (char *)FuseBypassData.config_file;
                }
                else if (pPartition->Filename == NULL)
                {
                    pPartition->Filename = (char *)FuseBypassData.config_file;
                    FuseBypassData.override_config_file = NV_TRUE;
                }
            }

            e = NvFlashCommandGetOption(NvFlashOption_DtbFileName,
                                        (void *)&pDtbFile);
            VERIFY(e == NvSuccess, goto fail);

            if (pDtbFile && !NvOsStrcmp(pPartition->Name, "DTB"))
            {
                if (NvOsStrcmp(pDtbFile, "unknown"))
                {
                    if (pPartition->Filename)
                        NvOsFree(pPartition->Filename);
                    pPartition->Filename = NvOsAlloc(NvOsStrlen(pDtbFile) + 1);
                    NvOsStrcpy(pPartition->Filename, pDtbFile);
                }
            }

            if ((!pPartition->Filename &&
                NvOsStrcmp(pPartition->Name, s_NCTPartName)) ||
                pPartition->Type == Nv3pPartitionType_Bct ||
                pPartition->Type == Nv3pPartitionType_PartitionTable)
            {
                /* not allowed to download bcts or partition tables
                 * here.
                 */
                continue;
            }

            if (NvOsStrcmp(pPartition->Name, s_NCTPartName) == 0)
            {
                /* Check if NCT file is given as argument */
                if (pNCTPart->nctfilename)
                    DownloadCmd.filename = (char *)pNCTPart->nctfilename;

                /* Check if NCT filename is present in flash.cfg */
                else if (pPartition->Filename)
                    DownloadCmd.filename = pPartition->Filename;

                /* NCT partition present, but no input is given */
                else
                {
                    NvAuPrintf("Warning: No NCT file present. "
                               "Skip NCT partition download to continue.\n");
                    continue;
                }
            }
            else
                DownloadCmd.filename = pPartition->Filename;

            if (pPartition->Type == Nv3pPartitionType_FuseBypass &&
                nvflash_match_file_ext(DownloadCmd.filename, "txt"))
            {
                b = NvFlashSendFuseBypassInfo(pPartition->Id, pPartition->Size);
                SkuBypassed = b;
                if (FuseBypassData.override_config_file)
                    pPartition->Filename = NULL;
            }
            else if (pPartition->Type == Nv3pPartitionType_ConfigTable &&
                     nvflash_match_file_ext(DownloadCmd.filename, "txt"))
            {
                b = nvflash_ParseConfigTable(DownloadCmd.filename,
                                             pPartition->Id, pPartition->Size);
            }
            else
            {
                DownloadCmd.PartitionID = pPartition->Id;
                b = NvFlashDownloadPartition(&DownloadCmd);
            }

            VERIFY(b, goto fail);

            b = nvflash_wait_status();
            VERIFY(b, goto fail);
        }

    }

    /* If Sku bypass information is sent to device then print
     * which sku is bypassed
     */
    if (SkuBypassed && FuseBypassData.pSkuId != NULL)
    {
        NvAuPrintf("Completed flash of SKU: %s\n",
                   NvFlashGetSkuName(s_FuseBypassInfo.sku_type));
    }
    /* If sku bypass information is not sent to device though sku
     * bypass option is provided in command line with non zero sku
     * and device is a pre-production then print warning
     */
    else if (!SkuBypassed && FuseBypassData.pSkuId != NULL &&
             s_FuseBypassInfo.sku_type != 0)
    {
        NvAuPrintf("Warning: Fuse bypass partition is not present, "
                   "could not bypass SKU: %s\n",
                   NvFlashGetSkuName(s_FuseBypassInfo.sku_type));
    }

    goto clean;

fail:
    b = NV_FALSE;

clean:
    return b;
}

static NvBool
nvflash_cmd_create(void)
{
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvU32 i, j, len;
    NvFlashDevice *d;
    NvFlashPartition *p;
    Nv3pCommand cmd = 0;
    Nv3pCmdSetDevice dev_arg;
    Nv3pCmdCreatePartition part_arg;
    Nv3pCmdStartPartitionConfiguration part_cfg;
    NvU32 nPartitions;
    NvU32 odm_data;
    NvBool verifyPartitionEnabled;
    const NvFlashBctFiles *bct_files;
    char *err_str = 0;
    NvFlashFuseBypass fuseBypass_data;
    NvFlashNCTPart *pNCTPart = NULL;
    NvBool SkuBypassed = NV_FALSE;
    const char *Dtb_File = NULL;

    e = NvOsGetSystemTime(&StartTime);
    VERIFY(e == NvSuccess, err_str = "Getting system time failed";
        goto fail);

    s_bIsFormatPartitionsNeeded = NV_FALSE;

    e = NvFlashCommandGetOption(NvFlashOption_FuseBypass,
                                (void *)&fuseBypass_data);
    VERIFY(e == NvSuccess, goto fail);

    if (s_Resume)
    {
        // need to send bct and odmdata for --create in resume mode
        e = NvFlashCommandGetOption(NvFlashOption_Bct, (void *)&bct_files);
        VERIFY(e == NvSuccess, goto fail);
        VERIFY(bct_files->BctOrCfgFile, err_str = "bct file required for this command"; goto fail);
        b = nvflash_cmd_setbct(bct_files);
        VERIFY(b, err_str = "setbct failed"; goto fail);
        b = nvflash_wait_status();
        VERIFY(b, goto fail);    /* count all the partitions for all devices */
        e = NvFlashCommandGetOption(NvFlashOption_OdmData, (void *)&odm_data);
        VERIFY(e == NvSuccess, goto fail);
        if(odm_data)
        {
            b = nvflash_cmd_odmdata(odm_data);
            VERIFY( b, err_str = "odm data failure"; goto fail );
        }
    }

    // Check for skip and do the errorcheck using this function.
    b = nvflash_check_skiperror();
    VERIFY(b, err_str = "Skip error check handler failed"; goto fail);

    d = s_Devices;
    nPartitions = 0;
    for (i = 0; i < s_nDevices; i++, d = d->next)
    {
        nPartitions += d->nPartitions;
    }

    part_cfg.nPartitions = nPartitions;
    cmd = Nv3pCommand_StartPartitionConfiguration;
    e = Nv3pCommandSend( s_hSock,
        cmd, &part_cfg, 0 );
    VERIFY( e == NvSuccess, goto fail );

    b = nvflash_wait_status();
    VERIFY( b, err_str="unable to do start partition"; goto fail );

    d = s_Devices;
    for( i = 0; i < s_nDevices; i++, d = d->next )
    {
        NvAuPrintf( "setting device: %d %d\n", d->Type, d->Instance );

        cmd = Nv3pCommand_SetDevice;
        dev_arg.Type = d->Type;
        dev_arg.Instance = d->Instance;
        e = Nv3pCommandSend( s_hSock, cmd, (NvU8 *)&dev_arg, 0 );
        VERIFY( e == NvSuccess, goto fail );

        b = nvflash_wait_status();
        VERIFY( b, err_str="unable to do set device"; goto fail );

        // delete all the partitions by erasing the whole device contents
        if (s_bIsFormatPartitionsNeeded != NV_TRUE)
        {
            b = nvflash_cmd_deleteall();
            VERIFY(b == NV_TRUE, err_str = "deleteall failed"; goto fail);
        }

        p = d->Partitions;
        for( j = 0; j < d->nPartitions; j++, p = p->next )
        {
            cmd = Nv3pCommand_CreatePartition;

            NvAuPrintf( "creating partition: %s\n", p->Name );

            len = NV_MIN( NV3P_STRING_MAX - 1, NvOsStrlen( p->Name ) );
            NvOsMemcpy( part_arg.Name, p->Name, len );
            part_arg.Name[len] = 0;
            part_arg.Size = p->Size;
            part_arg.Address = p->StartLocation;
            part_arg.Id = p->Id;
            part_arg.Type = p->Type;
            part_arg.FileSystem = p->FileSystemType;
            part_arg.AllocationPolicy = p->AllocationPolicy;
            part_arg.FileSystemAttribute = p->FileSystemAttribute;
            part_arg.PartitionAttribute = p->PartitionAttribute;
            part_arg.AllocationAttribute = p->AllocationAttribute;
            part_arg.PercentReserved = p->PercentReserved;
#ifdef NV_EMBEDDED_BUILD
            part_arg.IsWriteProtected = p->IsWriteProtected;
#endif
            e = Nv3pCommandSend( s_hSock, cmd, (NvU8 *)&part_arg, 0 );
            VERIFY( e == NvSuccess, goto fail );

            b = nvflash_wait_status();
            VERIFY( b, err_str="unable to do create partition"; goto fail );
        }
    }

    // Start to report status information
    nvflash_report_progress(0);

    cmd = Nv3pCommand_EndPartitionConfiguration;
    e = Nv3pCommandSend( s_hSock,
        cmd, 0, 0 );
    VERIFY( e == NvSuccess, goto fail );

    b = nvflash_wait_status();
    VERIFY( b, err_str="unable to do end partition"; goto fail );

    if (s_bIsFormatPartitionsNeeded == NV_TRUE)
    {
        /* Format partitions */
        b = nvflash_cmd_formatpartition(Nv3pCommand_FormatPartition, -1);
        VERIFY(b, goto fail);
    }

    // Check if NCT is supplied as part of commandline to determine whether or not
    // to take a backup of NCT.
    e = NvFlashCommandGetOption(NvFlashOption_NCTPart, (void *)&pNCTPart);
    if (e != NvSuccess)
    {
        NvAuPrintf("%s: error getting NCT part info.\n", __FUNCTION__);
        goto fail;
    }

    d = s_Devices;
    /* get the total size of the files to be downloaded to device */
    for( i = 0; i < s_nDevices; i++, d = d->next )
    {
        NvOsStatType stat;
        p = d->Partitions;
        for( j = 0; j < d->nPartitions; j++, p = p->next )
        {
            if( !p->Filename ||
                p->Type == Nv3pPartitionType_Bct ||
                p->Type == Nv3pPartitionType_PartitionTable ||
                nvflash_check_skippart(p->Name))
            {
                /* not allowed to download bcts or partition tables
                 * here.
                 */
                continue;
            }

            NvOsStat( p->Filename, &stat );
            total_image_size += stat.size;
        }
    }

    d = s_Devices;
    /* download all of the partitions with file names specified */
    for( i = 0; i < s_nDevices; i++, d = d->next )
    {
        p = d->Partitions;
        for( j = 0; j < d->nPartitions; j++, p = p->next )
        {
            /* hack up a command to re-use the helper function */
            NvFlashCmdDownload dp;

            if(p->Type == Nv3pPartitionType_FuseBypass)
            {
                /* If no skus are to be bypassed then make filename NULL
                 * to avoid download of fusebypass partition
                 */
                if(fuseBypass_data.pSkuId == NULL ||
                   s_FuseBypassInfo.sku_type == 0)
                {
                    if(p->Filename)
                        NvOsFree(p->Filename);
                    p->Filename = NULL;
                }
                /* If config file is present in flash.cfg and is also given via
                 * command option then replace the config file with user config
                 * file.
                 */
                 else if(fuseBypass_data.override_config_file)
                 {
                        if(p->Filename)
                            NvOsFree(p->Filename);
                        p->Filename = (char *)fuseBypass_data.config_file;
                 }
                 else if(p->Filename == NULL)
                 {
                    p->Filename = (char *)fuseBypass_data.config_file;
                    fuseBypass_data.override_config_file = NV_TRUE;
                 }
            }

            e = NvFlashCommandGetOption(
                    NvFlashOption_DtbFileName, (void *)&Dtb_File);
            VERIFY( e == NvSuccess, goto fail );

            if (Dtb_File && !NvOsStrcmp(p->Name, "DTB"))
            {
                if (NvOsStrcmp(Dtb_File, "unknown"))
                {
                    if(p->Filename)
                        NvOsFree(p->Filename);
                    p->Filename = NvOsAlloc(NvOsStrlen(Dtb_File) + 1);
                    NvOsStrcpy(p->Filename, Dtb_File);
                }
            }

            if( (!p->Filename && NvOsStrcmp(p->Name, s_NCTPartName)) ||
                p->Type == Nv3pPartitionType_Bct ||
                p->Type == Nv3pPartitionType_PartitionTable ||
                nvflash_check_skippart(p->Name))
            {
                /* not allowed to download bcts or partition tables
                 * here.
                 */
                continue;
            }

            e = NvFlashCommandGetOption(NvFlashOption_VerifyEnabled,
                (void *)&verifyPartitionEnabled);
            VERIFY( e == NvSuccess, goto fail );
            if (verifyPartitionEnabled)
            {
                // Check if partition data needs to be verified. If yes, send
                // an auxiliary command to indicate this to the device side
                // code.
                b = nvflash_enable_verify_partition(p->Id);
                VERIFY( b, goto fail );
            }
            dp.PartitionID = p->Id;

            if (NvOsStrcmp(p->Name, s_NCTPartName) == 0)
            {
                /* Check if NCT file is given as argument */
                if (pNCTPart->nctfilename)
                    dp.filename = (char *)pNCTPart->nctfilename;

                /* Check if NCT filename is present in flash.cfg */
                else if (p->Filename)
                    dp.filename = p->Filename; // FIXME: consistent naming

                /* NCT partition present, but no input is given */
                else
                {
                    NvAuPrintf("Warning: No NCT file present. "
                               "Skip NCT partition download to continue.\n");
                    continue;
                }
            }
            else
                dp.filename = p->Filename; // FIXME: consistent naming

            // if allocation attribute is 0x808 p->size is not the
            //actual allocated partition size .In that case query it
            //from device
            if (p->AllocationAttribute == 0x808)
                p->Size = 0;

            if(p->Type == Nv3pPartitionType_FuseBypass &&
               nvflash_match_file_ext(dp.filename, "txt"))
            {
                b = NvFlashSendFuseBypassInfo(p->Id, p->Size);
                SkuBypassed = b;
                if(fuseBypass_data.override_config_file)
                    p->Filename = NULL;
            }
            else if(p->Type == Nv3pPartitionType_ConfigTable &&
                    nvflash_match_file_ext(dp.filename, "txt"))
            {
                b = nvflash_ParseConfigTable(dp.filename, p->Id, p->Size);
            }
            else
            {
                b = nvflash_cmd_download(&dp, p->Type, p->Size);
            }

            VERIFY( b, goto fail );

            b = nvflash_wait_status();
            VERIFY( b, err_str="failed to verify partition"; goto fail );
        }
    }

    e = NvOsGetSystemTime(&EndTime);
    VERIFY(e == NvSuccess, err_str = "Getting system time failed";
        goto fail);

    NvAuPrintf("Create, format and download  took %d Secs\n",
        (EndTime.Seconds - StartTime.Seconds));

    /* If Sku bypass information is sent to device then print
     * which sku is bypassed
     */
    if (SkuBypassed && fuseBypass_data.pSkuId != NULL)
    {
        NvAuPrintf("Completed flash of SKU: %s\n",
                   NvFlashGetSkuName(s_FuseBypassInfo.sku_type));
    }
    /* If sku bypass information is not sent to device though sku
     * bypass option is provided in command line with non zero sku
     * and device is a pre-production then print warning
     */
    else if (!SkuBypassed && fuseBypass_data.pSkuId != NULL &&
             s_FuseBypassInfo.sku_type != 0)
    {
        NvAuPrintf("Warning: Fuse bypass partition is not present, "
                   "could not bypass SKU: %s\n",
                   NvFlashGetSkuName(s_FuseBypassInfo.sku_type));
    }

    b = NV_TRUE;
    nvflash_report_progress(0xFFFFFFFF); // download done
    goto clean;

fail:
    b = NV_FALSE;
    if (err_str)
    {
        NvAuPrintf( "%s\n", err_str);
    }
    if (e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);

clean:
    return b;
}

#if NV_FLASH_HAS_OBLITERATE
static NvBool
nvflash_cmd_obliterate( void )
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    NvFlashDevice *d;
    Nv3pCmdSetDevice dev_arg;
    NvU32 i;

    /* for each device in the config file, send a set-device, then an
     * obliterate
     */

    d = s_Devices;
    for( i = 0; i < s_nDevices; i++, d = d->next )
    {
        NvAuPrintf( "setting device: %d %d\n", d->Type, d->Instance );

        cmd = Nv3pCommand_SetDevice;
        dev_arg.Type = d->Type;
        dev_arg.Instance = d->Instance;
        e = Nv3pCommandSend( s_hSock, cmd, (NvU8 *)&dev_arg, 0 );
        VERIFY( e == NvSuccess, goto fail );

        b = nvflash_wait_status();
        VERIFY( b, goto fail );

        NvAuPrintf( "issuing obliterate\n" );

        cmd = Nv3pCommand_Obliterate;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend( s_hSock, cmd, 0, 0 )
        );

        b = nvflash_wait_status();
        VERIFY( b, goto fail );
    }

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}
#endif

static NvBool
nvflash_cmd_formatall( void )
{
    NvBool b;
    b = nvflash_cmd_formatpartition(Nv3pCommand_FormatAll, -1);
    VERIFY( b, goto fail );

    return NV_TRUE;
fail:
    NvAuPrintf( "\r\nformatting failed\n");
    return NV_FALSE;
}

static NvBool
nvflash_cmd_setnvinternal( NvFlashCmdNvPrivData *a )
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_NvPrivData;
    Nv3pCmdNvPrivData arg;
    NvOsStatType stat;

    e = NvOsStat( a->filename, &stat );
    if( e != NvSuccess )
    {
        NvAuPrintf( "file not found: %s\n", a->filename );
        goto fail;
    }

    arg.Length = (NvU32)stat.size;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    b = nvflash_sendfile( a->filename, 0 );
    return b;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n",cmd,e);
    return NV_FALSE;
}

static NvBool
ConvertPartitionType(NvPartMgrPartitionType PartNvPartMgr,
                     Nv3pPartitionType *PartNv3p)
{
    switch (PartNvPartMgr)
    {
        case NvPartMgrPartitionType_Bct:
            *PartNv3p = Nv3pPartitionType_Bct;
            break;

        case NvPartMgrPartitionType_Bootloader:
            *PartNv3p = Nv3pPartitionType_Bootloader;
            break;

        case NvPartMgrPartitionType_BootloaderStage2:
            *PartNv3p = Nv3pPartitionType_BootloaderStage2;
            break;

        case NvPartMgrPartitionType_PartitionTable:
            *PartNv3p = Nv3pPartitionType_PartitionTable;
            break;

        case NvPartMgrPartitionType_NvData:
            *PartNv3p = Nv3pPartitionType_NvData;
            break;

        case NvPartMgrPartitionType_Data:
            *PartNv3p = Nv3pPartitionType_Data;
            break;

        case NvPartMgrPartitionType_Mbr:
            *PartNv3p = Nv3pPartitionType_Mbr;
            break;

        case NvPartMgrPartitionType_Ebr:
            *PartNv3p = Nv3pPartitionType_Ebr;
            break;
        case NvPartMgrPartitionType_GP1:
            *PartNv3p = Nv3pPartitionType_GP1;
            break;
        case NvPartMgrPartitionType_GPT:
            *PartNv3p = Nv3pPartitionType_GPT;
            break;
        case NvPartMgrPartitionType_Os:
            *PartNv3p = Nv3pPartitionType_Os;
            break;
        case NvPartMgrPartitionType_FuseBypass:
            *PartNv3p = Nv3pPartitionType_FuseBypass;
            break;
        case NvPartMgrPartitionType_ConfigTable:
            *PartNv3p = Nv3pPartitionType_ConfigTable;
            break;
        case NvPartMgrPartitionType_WB0:
            *PartNv3p = Nv3pPartitionType_WB0;
            break;

        default:
            return NV_FALSE;;
    }

    return NV_TRUE;
}

static NvBool
nvflash_query_partition_type(NvU32 PartId, NvU32 *PartitionType)
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_Force32;
    Nv3pCmdQueryPartition query_part;

    query_part.Id = PartId;
    cmd = Nv3pCommand_QueryPartition;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &query_part, 0)
    );

    /* wait for status */
    b = nvflash_wait_status();
    VERIFY(b, goto fail);
    b = ConvertPartitionType((NvPartMgrPartitionType) query_part.PartType,
                             (Nv3pPartitionType *) PartitionType);
    VERIFY(b, goto fail);
    return b;

fail:
    return NV_FALSE;
}

static NvBool nvflash_cmd_setbct(const NvFlashBctFiles *pSetBctArg)
{
    NvError e = NvSuccess;
    NvBool Ret = NV_TRUE;
    NvBool SkipSync = NV_FALSE;
    Nv3pCommand Command = Nv3pCommand_DownloadBct;
    Nv3pCmdDownloadBct Arg;
    NvOsStatType FileStat;
    NvFlashDevice *pDevice = NULL;
    NvFlashPartition *pPartition = NULL;
    NvS32 StartExtension;
    char *pCfgFile = NULL;
    NvU32 BctSize = 0;
    NvU8 *pBctBuffer = NULL;
    size_t ReadBytes = 0;
    NvOsFileHandle hFile = 0;
    char *pBctFile = (char *)pSetBctArg->BctOrCfgFile;
#if !defined(NV_EMBEDDED_BUILD) && !defined(NVHOST_QUERY_DTB)
    NvBctHandle hBct = NULL;
    NvU8 *pCustomerData = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvStorageBootDevice BootDevice;
    NvBctAuxInfo *pAuxInfo = NULL;
#endif
    StartExtension = NvOsStrlen(pBctFile);
    StartExtension -= NvOsStrlen(BCT_CFG_FILE_EXTENSION);
    StartExtension = StartExtension > 0 ? StartExtension : 0;

    if (NvFlashIStrcmp(pBctFile + StartExtension, BCT_CFG_FILE_EXTENSION) == 0)
    {
        const char *pBlobFile = NULL;
        NvFlashBctParseInfo BctParseInfo;
        NvBoardInfo *pBoardInfo = &(g_BoardDetails.BoardInfo);

        e = NvFlashCommandGetOption(NvFlashOption_Blob, (void **)&pBlobFile);
        VERIFY(e == NvSuccess, goto fail);

        if (pBlobFile != NULL)
        {
            NvAuPrintf("BCT configuration file is not supported "
                       "for the secure devices\n");
            e = NvError_NotSupported;
            goto fail;
        }

        BctParseInfo.BoardId = pBoardInfo->BoardId;
        BctParseInfo.BoardFab = pBoardInfo->Fab;
        BctParseInfo.BoardSku = pBoardInfo->Sku;
        BctParseInfo.MemType = pBoardInfo->MemType;
        BctParseInfo.SkuType = g_BoardDetails.ChipSku;

        e = NvFlashCommandGetOption(NvFlashOption_Freq,
                                    (void *)&BctParseInfo.MemFreq);
        VERIFY(e == NvSuccess, goto fail);

        // Use nvbuildbct library to store the bct into buffer
        NV_CHECK_ERROR_CLEANUP(
            nvflash_util_buildbct(pBctFile, NULL, &BctParseInfo,
                                  &pBctBuffer, &BctSize)
        );
        // Make pointer NULL so that buffer will be sent instead of file
        pBctFile = NULL;

        VERIFY(pBctBuffer == NULL, e = NvError_NotInitialized);
    }
    else
    {
        VERIFY(pBctFile == NULL, e = NvError_NotInitialized);
        e = NvOsStat(pBctFile, &FileStat);
        BctSize = (NvU32)FileStat.size;
        if (e != NvSuccess)
        {
            NvAuPrintf("file not found: %s\n", pBctFile);
            goto fail;
        }
        e = NvOsFopen(pBctFile, NVOS_OPEN_READ, &hFile);
        if (e != NvSuccess)
        {
            NvAuPrintf("file open failed: %s\n", pBctFile);
            goto fail;
        }
        pBctBuffer = NvOsAlloc(BctSize);
        VERIFY(pBctBuffer == NULL, e = NvError_InsufficientMemory);
        NvOsMemset(pBctBuffer, 0x0, BctSize);
        e = NvOsFread(hFile, pBctBuffer, (NvU32)BctSize, &ReadBytes);
        if (e != NvSuccess)
        {
            NvAuPrintf("file read failed: %s\n", pBctFile);
            goto fail;
        }
    }

    /* Fill BCT Customer Data with parsed NCT info. */
    if (s_IsNCTPartInCommandline)
        nvflash_set_nctinfoinbct(s_NctCustInfo, pBctBuffer);

    e = NvFlashCommandGetOption(NvFlashOption_SkipSync, (void *)&SkipSync);
    VERIFY(e == NvSuccess, goto fail);

    if (!SkipSync)
    {
        NvU32 j;

        e = NvFlashCommandGetOption(NvFlashOption_ConfigFile,
                                    (void **)&pCfgFile);
        if (pCfgFile == NULL)
        {
            NvAuPrintf("--configfile needed for --setbct \n");
            e = NvError_BadParameter;
            goto fail;
        }

        pDevice = s_Devices;
        pPartition = pDevice->Partitions;

        for (j = 0; j < pDevice->nPartitions; j++)
        {
            if (NvOsStrcmp(pPartition->Name, "BCT") == 0)
                break;
            pPartition = pPartition->next;
        }
        if (j == pDevice->nPartitions)
        {
            NvAuPrintf("No Bct Partition\n");
            goto fail;
        }

        if (BctSize > pPartition->Size)
        {
            NvAuPrintf("%s is too large for partition\n", pPartition->Name);
            goto fail;
        }
    }

#if !defined(NV_EMBEDDED_BUILD) && !defined(NVHOST_QUERY_DTB)
    if (s_Devices->next)
        pDevice = s_Devices->next;
    else
        pDevice = s_Devices;

    switch(pDevice->Type)
    {
        case Nv3pDeviceType_Emmc:
            BootDevice = NvStorageBootDevice_Sdmmc;
            break;

        case Nv3pDeviceType_Sata:
            BootDevice = NvStorageBootDevice_Sata;
            break;

        default:
            e = NvError_BadParameter;
            goto fail;
    }

    Size = BctSize;

    NV_CHECK_ERROR_CLEANUP(
            NvBctInit(&Size, pBctBuffer, &hBct));

    Size = 0;

    NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hBct,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                NULL));

    pCustomerData = NvOsAlloc(Size);

    NV_CHECK_ERROR_CLEANUP(!pCustomerData);

    NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hBct,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                pCustomerData));

    pAuxInfo = (NvBctAuxInfo*)pCustomerData;

    pAuxInfo->BootDevice = BootDevice;

    pAuxInfo->Instance = pDevice->Instance;

    NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(
                hBct,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                pCustomerData));
#endif
    Arg.Length = BctSize;

    Command = Nv3pCommand_DownloadBct;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, Command, &Arg, 0)
    );

    e = Nv3pDataSend(s_hSock, pBctBuffer, BctSize, 0);
    Ret = (e == NvSuccess);

    if (!Ret)
    {
        NvAuPrintf("Sending BCT failed\n");
        goto fail;
    }
    NvAuPrintf("BCT sent successfully\n");

    goto clean;

fail:
    Ret = NV_FALSE;
    if (e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n", Command, e);
clean:
    if (pBctBuffer != NULL)
        NvOsFree(pBctBuffer);

    if (hFile != NULL)
        NvOsFclose(hFile);

#if !defined(NV_EMBEDDED_BUILD) && !defined(NVHOST_QUERY_DTB)
    if (pCustomerData)
        NvOsFree(pCustomerData);

    if (hBct)
        NvBctDeinit(hBct);
#endif
    return Ret;
}

static NvBool nvflash_odmcmd_tegratabfusekeys(NvFlashCmdSetOdmCmd *a)
{
        NvBool  b = NV_TRUE;
        NvError e = NvSuccess;
        Nv3pCommand cmd;
        Nv3pCmdOdmCommand arg;
        NvOsStatType stat;
        char *err_str = NULL;

        e = NvOsStat(a->odmExtCmdParam.tegraTabFuseKeys.filename, &stat);
        if (e != NvSuccess)
        {
            err_str = "Getting Stats for blob file Failed";
            goto fail;
        }
        arg.odmExtCmdParam.tegraTabFuseKeys.FileLength = (NvU32) stat.size;

        cmd = Nv3pCommand_OdmCommand;
        arg.odmExtCmd = Nv3pOdmExtCmd_TegraTabFuseKeys;

        e = Nv3pCommandSend(s_hSock, cmd, &arg, 0);
        if (e != NvSuccess)
        {
            err_str = "Nv3pCommandSend failed";
            goto fail;
        }

        // send hex file to device
        b = nvflash_sendfile(a->odmExtCmdParam.tegraTabFuseKeys.filename, 0);
        VERIFY(b, err_str = "sending blob file failed"; goto fail);

        return NV_TRUE;

    fail:
        if (err_str)
        {
            NvAuPrintf("TegraTabFuseKeys: %s\n", err_str);
        }
        return NV_FALSE;
}

static NvBool nvflash_odmcmd_tegratabverifyfuse(NvFlashCmdSetOdmCmd *a)
{
        NvBool  b = NV_TRUE;
        NvError e = NvSuccess;
        Nv3pCommand cmd;
        Nv3pCmdOdmCommand arg;
        NvOsStatType stat;
        char *err_str = NULL;

        e = NvOsStat(a->odmExtCmdParam.tegraTabFuseKeys.filename, &stat);
        if (e != NvSuccess)
        {
            err_str = "Getting Stats for blob file Failed";
            goto fail;
        }
        arg.odmExtCmdParam.tegraTabFuseKeys.FileLength = (NvU32) stat.size;

        cmd = Nv3pCommand_OdmCommand;
        arg.odmExtCmd = Nv3pOdmExtCmd_TegraTabVerifyFuse;

        e = Nv3pCommandSend(s_hSock, cmd, &arg, 0);
        if (e != NvSuccess)
        {
            err_str = "Nv3pCommandSend failed";
            goto fail;
        }

        // send hex file to device
        b = nvflash_sendfile(a->odmExtCmdParam.tegraTabFuseKeys.filename, 0);
        VERIFY(b, err_str = "sending blob file failed"; goto fail);

        return NV_TRUE;

    fail:
        if (err_str)
        {
            NvAuPrintf("TegraTabFuseKeys: %s\n", err_str);
        }
        return NV_FALSE;
}

NvBool
nvflash_cmd_updatebct(
    const char *pBctFile,
    const char *pBctSection)
{
    NvError e = NvSuccess;
    NvBool Ret = NV_TRUE;
    Nv3pCommand Cmd = Nv3pCommand_UpdateBct;
    Nv3pCmdUpdateBct UpdateBctArg = {0};
    NvOsStatType FileStat;
    NvU32 BctSize = 0;
    NvS32 StartExtension = 0;
    NvU8 *pBctBuffer = NULL;

    if (pBctFile == NULL || pBctSection == NULL)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    StartExtension = NvOsStrlen(pBctFile);
    StartExtension -= NvOsStrlen(BCT_CFG_FILE_EXTENSION);
    StartExtension = StartExtension > 0 ? StartExtension : 0;

    if (NvFlashIStrcmp(pBctFile + StartExtension, BCT_CFG_FILE_EXTENSION) == 0)
    {
        const char *pBlobFile = NULL;
        NvFlashBctParseInfo BctParseInfo;
        NvBoardInfo *pBoardInfo = &(g_BoardDetails.BoardInfo);

        e = NvFlashCommandGetOption(NvFlashOption_Blob, (void **)&pBlobFile);
        VERIFY(e == NvSuccess, goto fail);

        if (pBlobFile != NULL)
        {
            NvAuPrintf("BCT configuration file is not supported for "
                       "updating the secure device BCT\n");
            e = NvError_NotSupported;
            goto fail;
        }

        BctParseInfo.BoardId = pBoardInfo->BoardId;
        BctParseInfo.BoardFab = pBoardInfo->Fab;
        BctParseInfo.BoardSku = pBoardInfo->Sku;
        BctParseInfo.MemType = pBoardInfo->MemType;
        BctParseInfo.SkuType = g_BoardDetails.ChipSku;

        e = NvFlashCommandGetOption(NvFlashOption_Freq,
                                    (void *)&BctParseInfo.MemFreq);
        VERIFY(e == NvSuccess, goto fail);

        // Use nvbuildbct library to store the bct into buffer
        NV_CHECK_ERROR_CLEANUP(
            nvflash_util_buildbct(pBctFile, NULL, &BctParseInfo,
                                  &pBctBuffer, &BctSize)
        );
        // Make pointer NULL so that buffer will be sent instead of file
        pBctFile = NULL;

        VERIFY(pBctBuffer == NULL, e = NvError_NotInitialized);
    }

    if (pBctFile)
    {
        e = NvOsStat(pBctFile, &FileStat);
        BctSize = (NvU32)FileStat.size;
        if (e != NvSuccess)
        {
            NvAuPrintf("file not found: %s\n", pBctFile);
            goto fail;
        }
    }

    UpdateBctArg.BctSection = Nv3pUpdatebctSectionType_None;
    /* verify whether we are asked to update a
     * valid/supported bct section or not
     */
    if (!NvOsStrcmp("SDRAM", pBctSection))
        UpdateBctArg.BctSection = Nv3pUpdatebctSectionType_Sdram;
    else if (!NvOsStrcmp("DEVPARAM", pBctSection))
        UpdateBctArg.BctSection = Nv3pUpdatebctSectionType_DevParam;
    else if (!NvOsStrcmp("BOOTDEVINFO", pBctSection))
        UpdateBctArg.BctSection = Nv3pUpdatebctSectionType_BootDevInfo;
    else if (!NvFlashIStrcmp("BLINFO", pBctSection))
        UpdateBctArg.BctSection = Nv3pUpdatebctSectionType_BlInfo;

    if (UpdateBctArg.BctSection == Nv3pUpdatebctSectionType_None)
    {
        NvAuPrintf("no section found to update bct\r\n");
        e = NvError_BadParameter;
        goto fail;
    }

    UpdateBctArg.PartitionId = s_PartitionId;
    UpdateBctArg.Length = BctSize;

    Cmd = Nv3pCommand_UpdateBct;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, Cmd, &UpdateBctArg, 0)
    );

    /* send bct to server */
    if (pBctFile != NULL)
    {
        Ret = nvflash_sendfile(pBctFile, 0);
    }
    else
    {
        e = Nv3pDataSend(s_hSock, pBctBuffer, BctSize, 0);
        Ret = (e == NvSuccess);
        if (Ret)
            NvAuPrintf("BCT sent successfully\n");
    }

    goto clean;

fail:
    Ret = NV_FALSE;
    if (e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n", Cmd, e);

clean:
    if (pBctBuffer != NULL)
        NvOsFree(pBctBuffer);

    return Ret;
}

static NvBool
nvflash_cmd_getbct( const char *bct_file )
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdGetBct arg;
    NvOsFileHandle hFile = 0;
    NvU8 *buf = 0;
    NvU32 size;

    NvAuPrintf( "retrieving bct into: %s\n", bct_file );

    cmd = Nv3pCommand_GetBct;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    b = nvflash_wait_status();
    VERIFY( b, goto fail );
    size = arg.Length;
    buf = NvOsAlloc( size );
    VERIFY( buf, goto fail );

    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataReceive( s_hSock, buf, size, 0, 0 )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFopen( bct_file, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( hFile, buf, size )
    );

    NvAuPrintf( "%s received successfully\n", bct_file );

    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;
    if(e != NvSuccess)
        NvAuPrintf("Failed sending command %d NvError %d",cmd,e);

clean:
    NvOsFclose( hFile );
    NvOsFree( buf );
    return b;
}

static NvBool
nvflash_cmd_getbit( NvFlashCmdGetBit *a)
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdGetBct arg;
    NvOsFileHandle hFile = 0;
    NvU8 *buf = 0;
    NvU32 size;
    const char *bit_file = a->filename;

    NvAuPrintf( "retrieving bit into: %s\n", bit_file );

    cmd = Nv3pCommand_GetBit;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    size = arg.Length;
    buf = NvOsAlloc( size );
    VERIFY( buf, goto fail );
    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataReceive( s_hSock, buf, size, 0, 0 )
    );
    NV_CHECK_ERROR_CLEANUP(
        NvOsFopen( bit_file, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile )
    );
    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( hFile, buf, size )
    );

    NvAuPrintf( "%s received successfully\n", bit_file );
    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;

clean:
    NvOsFclose( hFile );
    NvOsFree( buf );
    return b;
}

static NvBool
nvflash_cmd_dumpbit( NvFlashCmdDumpBit * a )
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdGetBct arg;
    NvU8 *buf = 0;
    NvU32 size;

    cmd = Nv3pCommand_GetBit;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    size = arg.Length;
    buf = NvOsAlloc( size );
    VERIFY( buf, goto fail );
    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataReceive( s_hSock, buf, size, 0, 0 )
    );

    NV_CHECK_ERROR_CLEANUP(
        nvflash_util_dumpbit(buf, a->DumpbitOption)
    );
    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;

clean:
    NvOsFree( buf );
    return b;
}

static NvBool
nvflash_cmd_setboot( NvFlashCmdSetBoot *a )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdSetBootPartition arg;

    arg.Id = a->PartitionID;
    // FIXME: remove the rest of the command parameters
    arg.LoadAddress = 0;
    arg.EntryPoint = 0;
    arg.Version = 1;
    arg.Slot = 0;

    cmd = Nv3pCommand_SetBootPartition;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    return NV_TRUE;
fail:
    return NV_FALSE;
}

static NvBool
nvflash_match_file_ext( const char *filename, const char *match )
{
    const char *ext;
    const char *tmp;
    tmp = filename;
    do
    {
        /* handle pathologically-named files, such as blah.<match>.txt */
        ext = NvUStrstr( tmp, match );
        if( ext && NvOsStrcmp( ext, match ) == 0 )
        {
            return NV_TRUE;
        }
        tmp = ext;
    } while( ext );

    return NV_FALSE;
}

static NvBool
NvFlashDownloadPartition(
    NvFlashCmdDownload *pDownloadArg)
{
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvBool VerifyPartitionEnabled;
    const char *pErrStr = NULL;
    NvU32 PartitionType;
    char *pBlobFile = NULL;
    const char *pBctSection = NULL;
    const NvFlashBctFiles *pBct;

    if (pDownloadArg->PartitionID == 0 && pDownloadArg->PartitionName == NULL)
        return NV_FALSE;

    // If input argument is a partition name string ,
    // then retrieve the partition id.
    if (!pDownloadArg->PartitionID)
    {
        pDownloadArg->PartitionID =
            nvflash_get_partIdByName(pDownloadArg->PartitionName, NV_FALSE);
        VERIFY(pDownloadArg->PartitionID,
               pErrStr = "partition not found in partition table"; goto fail);
    }

    b = nvflash_query_partition_type(pDownloadArg->PartitionID, &PartitionType);
    VERIFY(b, pErrStr = "Error querying partition type"; goto fail);

    e = NvFlashCommandGetOption(NvFlashOption_VerifyEnabled,
                                (void *)&VerifyPartitionEnabled);
    VERIFY(e == NvSuccess, goto fail);


    if (VerifyPartitionEnabled)
    {
        // Check if partition data needs to be verified. If yes, send
        // an auxiliary command to indicate this to the device side
        // code.
        b = nvflash_enable_verify_partition(pDownloadArg->PartitionID);
        VERIFY(b,
               pErrStr = "Error marking Partition for verification"; goto fail);
    }

    if (PartitionType == Nv3pPartitionType_Bootloader ||
        PartitionType == Nv3pPartitionType_BootloaderStage2)
    {
        e = NvFlashCommandGetOption(NvFlashOption_Blob, (void*)&pBlobFile);
        VERIFY(e == NvSuccess, goto fail);

        e = NvFlashCommandGetOption(NvFlashOption_UpdateBct,
                                    (void *)&pBctSection);
        VERIFY(e == NvSuccess, goto fail);

        e = NvFlashCommandGetOption(NvFlashOption_Bct, (void *)&pBct);
        VERIFY(e == NvSuccess, goto fail);

        if (pBlobFile)
        {
            if (pBctSection)
            {
                if (!NvFlashIStrcmp(pBctSection, "BLINFO"))
                {
                    s_PartitionId = pDownloadArg->PartitionID;
                    b = nvflash_cmd_updatebct(pBct->BctOrCfgFile, pBctSection);
                    VERIFY(b, pErrStr = "updatebct failed"; goto fail);
                    b = nvflash_wait_status();
                    VERIFY(b, pErrStr = "bootloader error"; goto fail);
                }
                else
                {
                    b = NV_FALSE;
                    pErrStr = "only BLINFO supported with download command";
                    goto fail;
                }
            }
            else
            {
                b = NV_FALSE;
                pErrStr = "Updatebct command is required";
                goto fail;
            }
        }

    }

    b = nvflash_cmd_download(pDownloadArg, PartitionType, 0);

    VERIFY(b, pErrStr = "partition download failed"; goto fail);

    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;

clean:
    if (pErrStr != NULL)
        NvAuPrintf("%s\n", pErrStr);

    return b;
}

static NvBool
nvflash_cmd_download(
    NvFlashCmdDownload *pDownloadArg,
    NvU32 PartitionType,
    NvU64 MaxSize)
{
    NvError e = NvSuccess;
    NvBool Ret = NV_TRUE;
    NvBool NeedPreProcessing = NV_FALSE;
    NvU32 BlockSize = 0;
    NvU64 SizeToSend = 0;
    NvU32 PadSize = 0;
    char *pFileName = NULL;
    NvU8  *Wb0Buf = NULL;
    NvU32 Wb0Codelength;
    NvU8 *pPadBuf = NULL;
    const char *pErrStr = NULL;
    NvOsStatType FileStat;
    Nv3pCommand Command = Nv3pCommand_Force32;
    Nv3pCmdDownloadPartition Arg;
    Nv3pCmdQueryPartition QueryPartCmd;
    NvFlashCmdDiskImageOptions *pDiskImg = NULL;
    NvFlashPreprocType PreProcessingType = (NvFlashPreprocType)0;

    pFileName = (char *)pDownloadArg->filename;

    e = NvOsStat(pFileName, &FileStat);
    SizeToSend = FileStat.size;

    if (e != NvSuccess)
    {
        NvAuPrintf("file not found: %s\n", pFileName);
        goto fail;
    }

    /* check for files that need preprocessing */
    if (nvflash_match_file_ext(pFileName, ".dio"))
    {
        NeedPreProcessing = NV_TRUE;
        PreProcessingType = NvFlashPreprocType_DIO;
    }
    else if (nvflash_match_file_ext(pFileName, ".store.bin"))
    {
        NeedPreProcessing = NV_TRUE;
        PreProcessingType = NvFlashPreprocType_StoreBin;

        /* only need block size for .store.bin */
        NV_CHECK_ERROR_CLEANUP(
            NvFlashCommandGetOption(NvFlashOption_DiskImgOpt,
                                    (void **)&pDiskImg)
        );

        if (pDiskImg != NULL)
            BlockSize = pDiskImg->BlockSize;
    }

    if (NeedPreProcessing)
    {
        Ret = nvflash_util_preprocess(pFileName, PreProcessingType, BlockSize,
                                      &pFileName);
        VERIFY(Ret, goto fail);

        e = NvOsStat(pFileName, &FileStat);
        if (e != NvSuccess)
        {
            NvAuPrintf("file not found: %s\n", pFileName);
            goto fail;
        }
    }

    if (PartitionType == Nv3pPartitionType_ConfigTable &&
        nvflash_match_file_ext(pDownloadArg->filename, "txt"))
    {
        NvU64 PartSize = 0;
        Ret = nvflash_ParseConfigTable(pDownloadArg->filename,
                pDownloadArg->PartitionID, PartSize);
        VERIFY(Ret, goto fail );
        goto clean;
    }
    else
    {
        /* if Download partition is through --download command,
        * find whether file to be downloaded is greater than partition size.
        */
        if (!MaxSize)
        {
            QueryPartCmd.Id = pDownloadArg->PartitionID;
            Command = Nv3pCommand_QueryPartition;
            NV_CHECK_ERROR_CLEANUP(
                Nv3pCommandSend(s_hSock, Command, &QueryPartCmd, 0)
            );

            /* wait for status */
            Ret = nvflash_wait_status();
            VERIFY(Ret, pErrStr = "failed to query partition"; goto fail);
            MaxSize = QueryPartCmd.Size;
            PartitionType = (NvU32)QueryPartCmd.PartType;
        }

        if (PartitionType == Nv3pPartitionType_Bootloader ||
            PartitionType == Nv3pPartitionType_BootloaderStage2)
        {
            Ret = nvflash_align_bootloader_length(pDownloadArg->filename,
                                                  &PadSize, &pPadBuf);
            VERIFY(Ret, pErrStr = "padding to booloader failed"; goto fail);
            SizeToSend += PadSize;
        }

        /* Warmboot code signing for PKC, SBK mode is done by nvsecuretool */
        if ( (PartitionType == Nv3pPartitionType_WB0) &&
             ((OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC) &&
              (OpMode != NvDdkFuseOperatingMode_OdmProductionSecure)))
        {
            if (pDownloadArg->filename != NULL)
               Ret = nvflash_sign_wb0(pDownloadArg->filename, &Wb0Codelength,
                                   &Wb0Buf);
            else
            {
               NvAuPrintf("Warmboot File not found \n");
               goto clean;
            }

            if (Ret == NV_TRUE)
               NvAuPrintf("Warmboot code signing done \n");
            else
            {
               NvAuPrintf("Warmboot code signing failed \n");
               goto clean;
            }

            Command = Nv3pCommand_DownloadPartition;
            Arg.Id = pDownloadArg->PartitionID;
            Arg.Length = Wb0Codelength;
            e = Nv3pCommandSend(s_hSock, Command, &Arg, 0);
            if (e != NvSuccess)
            {
               NvAuPrintf("WB0 partition download command sending failed \n");
               goto clean;
            }

            NvAuPrintf("Sending Signed Warmboot code of length %d\n",
                        Wb0Codelength);
            e = Nv3pDataSend(s_hSock, Wb0Buf, Wb0Codelength, 0);
            if (e == NvSuccess)
               NvAuPrintf("Signed Warmboot code sent successfully \n");
            else
               NvAuPrintf("Signed Warmboot code sending failed \n");
            goto clean;
        }

        if ((MaxSize) && (SizeToSend > MaxSize))
        {
            NvAuPrintf("%s is too large for partition\n", pFileName);
            Ret = NV_FALSE;
            goto fail;
        }

        Command = Nv3pCommand_DownloadPartition;
        Arg.Id = pDownloadArg->PartitionID;
        Arg.Length = SizeToSend;

        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, Command, &Arg, 0)
        );
        // send all the data present in filename to  server
        Ret = nvflash_sendfile(pFileName, 0);
        VERIFY(Ret, goto fail);

        if (pPadBuf != NULL)
        {
            e = Nv3pDataSend(s_hSock, pPadBuf, PadSize, 0);
            VERIFY(e == NvSuccess, pErrStr = "data send failed"; goto fail);
        }

        Ret = NV_TRUE;
        goto clean;
    }

fail:
    Ret = NV_FALSE;
    if (pErrStr)
        NvAuPrintf("%s\n", pErrStr);
    if (e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n", Command, e);
clean:
    if (pPadBuf)
        NvOsFree(pPadBuf);
    if (Wb0Buf)
        NvOsFree(Wb0Buf);
    return Ret;
}

static NvBool
nvflash_odmcmd_VerifySdram(NvFlashCmdSetOdmCmd *a)
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdOdmCommand arg;
    NvBool b =NV_FALSE;

    cmd = Nv3pCommand_OdmCommand;
    arg.odmExtCmd = Nv3pOdmExtCmd_VerifySdram;
    arg.odmExtCmdParam.verifySdram.Value =
                                        a->odmExtCmdParam.verifySdram.Value;

    if (arg.odmExtCmdParam.verifySdram.Value == 0)
        NvAuPrintf("SdRam Verification :SOFT TEST\n");
    else if (arg.odmExtCmdParam.verifySdram.Value == 1)
        NvAuPrintf("SdRam Verification :STRESS TEST\n");
    else if (arg.odmExtCmdParam.verifySdram.Value == 2)
        NvAuPrintf("SdRam Verification :BUS TEST\n");
    else
    {
        NvAuPrintf("Valid parameters for verifySdram extn command are");
        NvAuPrintf(" 0- softest    1- stress test \n");
        return NV_FALSE;
    }
    NvAuPrintf("Please wait\n");
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);
    NvAuPrintf("Sdram Verification successfull\n");
    NvAuPrintf("Size verified is  :%u MB\n",arg.Data / (1024 * 1024));
    return b;

fail:
    NvAuPrintf("Sdram Verification failed\n");
    return NV_FALSE;
}

static NvBool
nvflash_odmcmd_limitedpowermode( void )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdOdmCommand arg;
    NvBool b =NV_FALSE;

    cmd = Nv3pCommand_OdmCommand;
    arg.odmExtCmd = Nv3pOdmExtCmd_LimitedPowerMode;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);
    NvAuPrintf("Limited Power Mode Set successfully\n");
fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return b;
}

static NvBool
nvflash_cmd_bootloader( const char *filename )
{
    NvBool b;
    NvError e;
    NvError SetEntry;
    Nv3pCommand cmd;
    Nv3pCmdDownloadBootloader arg;
    NvOsStatType stat;
    NvU32 odm_data;
    NvU8 *devType;
    NvU32 devConfig;
    NvBool FormatAllPartitions;
    NvBool IsBctSet;
    const NvFlashBctFiles *bct_files;
    char *blob_filename;
    char *err_str = 0;
    NvBool IsVerifySdramSet;
    NvBool IsLimitedPowerMode;

    /* need to send odm data, if any, before loading the bootloader */
    e = NvFlashCommandGetOption( NvFlashOption_OdmData,
        (void *)&odm_data );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBootDevType,
        (void *)&devType );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBootDevConfig,
        (void *)&devConfig );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_FormatAll,
        (void *)&FormatAllPartitions );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBct,
        (void *)&IsBctSet );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption(NvFlashOption_SetOdmCmdVerifySdram,
        (void *)&IsVerifySdramSet);
    VERIFY(e == NvSuccess, goto fail);

    e = NvFlashCommandGetOption(NvFlashOption_OdmCmdLimitedPowerMode,
        (void *)&IsLimitedPowerMode);
    VERIFY(e == NvSuccess, goto fail);

    if( IsBctSet )
    {
        e = NvFlashCommandGetOption( NvFlashOption_Bct,
        (void *)&bct_files );
        VERIFY( e == NvSuccess, goto fail );
        VERIFY(bct_files->BctOrCfgFile, err_str = "bct file required for this command"; goto fail);
        b = nvflash_cmd_setbct(bct_files);
        VERIFY( b, err_str = "setbct failed"; goto fail );
        b = nvflash_wait_status();
        VERIFY( b, goto fail );
    }

    if (IsVerifySdramSet)
    {
        NvFlashCmdSetOdmCmd a;
        e = NvFlashCommandGetOption( NvFlashOption_OdmCmdVerifySdramVal,
            (void *)&a.odmExtCmdParam.verifySdram.Value);
        VERIFY(e == NvSuccess, goto fail);

        if (!IsBctSet)
        {
            NvAuPrintf("--setbct required to verify Sdram\n");
            goto fail;
        }


        b = nvflash_odmcmd_VerifySdram(&a);
        VERIFY(b, goto fail);
    }

    if( (odm_data || devType || devConfig) && s_Resume )
    {
        char *str = 0;
        if( odm_data ) str = "odm data";
        if( devType ) str = "device type";
        if( devConfig ) str = "device config";

        NvAuPrintf( "warning: %s may not be specified with a resume\n", str );
        return NV_FALSE;
    }

    if( odm_data )
    {
        b = nvflash_cmd_odmdata( odm_data );
        VERIFY( b, err_str = "odm data failure"; goto fail );
    }

    if( devType )
    {
        b = nvflash_cmd_setbootdevicetype( devType );
        VERIFY( b, err_str = "boot devive type data failure"; goto fail );
    }

    if( devConfig )
    {
        b = nvflash_cmd_setbootdeviceconfig( devConfig );
        VERIFY( b, err_str = "boot device config data failure"; goto fail );
    }

    if( IsLimitedPowerMode )
    {
        b = nvflash_odmcmd_limitedpowermode();
        VERIFY( b, err_str = "limited power mode set failure"; goto fail );
    }

    e = NvOsStat( filename, &stat );
    if( e != NvSuccess )
    {
        NvAuPrintf( "file not found: %s\n", filename );
        goto fail;
    }

    NvAuPrintf( "downloading bootloader -- " );

    cmd = Nv3pCommand_DownloadBootloader;
    arg.Length = stat.size;

    nvflash_get_bl_loadaddr(&arg.EntryPoint, &arg.Address);

    /* check if entryPoint and address are provided thru command line */
    SetEntry = NvFlashCommandGetOption( NvFlashOption_EntryAndAddress,
        (void *)&arg );

    NvAuPrintf( "load address: 0x%x entry point: 0x%x\n",
        arg.Address, arg.EntryPoint );

    e = Nv3pCommandSend( s_hSock, cmd, &arg, 0 );
    VERIFY( e == NvSuccess, err_str = "download command failed"; goto fail );
    b = nvflash_wait_status();
    VERIFY( b, goto fail );

    // send command line bl hash to miniloader to validate it in secure mode
    e = NvFlashCommandGetOption(NvFlashOption_Blob, (void*)&blob_filename);
    VERIFY(e == NvSuccess, goto fail);
    if (blob_filename &&
        OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        NvU8 *blhash_buff = 0;

        // parse blob to retrieve command line bl hash and send it to miniloader
        b = nvflash_parse_blob(blob_filename, &blhash_buff, NvFlash_BlHash,NULL);
        VERIFY(b, err_str = "bootloader hash extraction from blob failed";
            goto fail);

        e = Nv3pDataSend(s_hSock, blhash_buff, NV3P_AES_HASH_BLOCK_LEN, 0);
        VERIFY(e == NvSuccess, err_str = "data send failed"; goto fail);
        if (blhash_buff)
            NvOsFree(blhash_buff);
    }
    else if (blob_filename &&
        OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU8 *blhash_buff = 0;

        // parse blob to retrieve command line bl hash and send it to miniloader
        b = nvflash_parse_blob(blob_filename, &blhash_buff, NvFlash_BlHash,NULL);
        VERIFY(b, err_str = "bootloader hash extraction from blob failed";
            goto fail);

        e = Nv3pDataSend(s_hSock, blhash_buff, RSA_KEY_SIZE, 0);
        VERIFY(e == NvSuccess, err_str = "data send failed"; goto fail);
        if (blhash_buff)
            NvOsFree(blhash_buff);
    }

    //send all the data present in filename to  server
    b = nvflash_sendfile( filename, 0 );
    VERIFY( b, goto fail );

    if ( SetEntry == NvSuccess ) {
        NvAuPrintf( "bootloader downloaded successfully\n" );
        return NV_TRUE;
    }

    /* wait for the bootloader to come up */
    NvAuPrintf( "waiting for bootloader to initialize\n" );
    b = nvflash_wait_status();
    VERIFY( b, err_str = "bootloader failed"; goto fail );

    NvAuPrintf( "bootloader downloaded successfully\n" );

    if ( FormatAllPartitions )
    {
        VERIFY( !IsBctSet, err_str = "--setbct not required for formatting partitions"; goto fail );
        b = nvflash_cmd_formatall();
        VERIFY( b, err_str = "format-all failed\n"; goto fail );
    }

    return NV_TRUE;

fail:
    if( err_str )
    {
        NvAuPrintf( "%s NvError 0x%x\n", err_str , e );
    }
    return NV_FALSE;
}

NvBool
nvflash_odmcmd_updateucfw(NvFlashCmdSetOdmCmd *a)
{
    NvBool  b = NV_TRUE;
    NvError e = NvSuccess;
    Nv3pCommand cmd;
    Nv3pCmdOdmCommand arg;
    NvOsStatType stat;
    char *err_str = NULL;

    e = NvOsStat(a->odmExtCmdParam.updateuCFw.filename, &stat);
    if (e != NvSuccess)
    {
        err_str = "Getting Stats for hex file Failed";
        goto fail;
    }
    arg.odmExtCmdParam.updateuCFw.FileSize = (NvU32) stat.size;

    cmd = Nv3pCommand_OdmCommand;
    arg.odmExtCmd = Nv3pOdmExtCmd_UpdateuCFw;

    e = Nv3pCommandSend(s_hSock, cmd, &arg, 0);
    if (e != NvSuccess)
    {
        err_str = "Nv3pCommandSend failed";
        goto fail;
    }

    // send hex file to device
    b = nvflash_sendfile(a->odmExtCmdParam.updateuCFw.filename, 0);
    VERIFY(b, err_str = "sending hex file failed"; goto fail);

    return NV_TRUE;

fail:
    if (err_str)
    {
        NvAuPrintf("UpdateuCFw: %s\n", err_str);
    }
    return NV_FALSE;
}

NvBool
nvflash_odmcmd_fuelgaugefwupgrade(NvFlashCmdSetOdmCmd *a)
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdOdmCommand arg;
    NvOsStatType stat;
    char *err_str = NULL;

    e = NvOsStat(a->odmExtCmdParam.fuelGaugeFwUpgrade.filename1, &stat);
    if (e != NvSuccess)
    {
        err_str = "Getting Stats for file1 Failed";
        goto fail;
    }

    arg.odmExtCmdParam.fuelGaugeFwUpgrade.FileLength1 = stat.size;

    if (a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2 != NULL)
    {
       e = NvOsStat(a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2, &stat);
       if (e != NvSuccess)
       {
           err_str = "Getting Stats for file2 Failed";
           goto fail;
       }

       arg.odmExtCmdParam.fuelGaugeFwUpgrade.FileLength2 = stat.size;
    }
    else
    {
       arg.odmExtCmdParam.fuelGaugeFwUpgrade.FileLength2 = 0;
    }

    cmd = Nv3pCommand_OdmCommand;
    arg.odmExtCmd = Nv3pOdmExtCmd_FuelGaugeFwUpgrade;

    e = Nv3pCommandSend(s_hSock, cmd, &arg, 0);
    if (e != NvSuccess)
    {
        err_str = "Nv3pCommandSend failed";
        goto fail;
    }
    // send all the data present in filename to server
    b = nvflash_sendfile(a->odmExtCmdParam.fuelGaugeFwUpgrade.filename1, 0);
    VERIFY(b, err_str = "sending file1 failed"; goto fail);

    if (a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2 != NULL)
    {
       b = nvflash_sendfile(a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2, 0);
       VERIFY(b, err_str = "sending file2 failed"; goto fail);
    }

    return NV_TRUE;

fail:
    if (err_str)
    {
        NvAuPrintf("FuelGaugeFwUpgrade: %s\n", err_str);
    }
    return NV_FALSE;
}

NvBool
nvflash_odmcmd_upgradedevicefirmware(NvFlashCmdSetOdmCmd *a)
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdOdmCommand arg;
    NvOsStatType stat;
    char *err_str = NULL;

    e = NvOsStat(a->odmExtCmdParam.upgradedevicefirmware.filename, &stat);
    if (e != NvSuccess)
    {
        err_str = "Getting Stats for file Failed";
        goto fail;
    }

    NvAuPrintf("File size is %d\n", stat.size);

    arg.odmExtCmdParam.upgradeDeviceFirmware.FileLength = stat.size;
    cmd = Nv3pCommand_OdmCommand;
    arg.odmExtCmd = Nv3pOdmExtCmd_UpgradeDeviceFirmware;

    e = Nv3pCommandSend(s_hSock, cmd, &arg, 0);
    if (e != NvSuccess)
    {
        err_str = "Nv3pCommandSend failed";
        goto fail;
    }
    // send all the data present in filename to server
    b = nvflash_sendfile(a->odmExtCmdParam.upgradedevicefirmware.filename, 0);
    VERIFY(b, err_str = "sending file failed"; goto fail);
    return NV_TRUE;

fail:
    if (err_str)
    {
        NvAuPrintf("UpgradeDeviceFirwmare: %s\n", err_str);
    }
    return NV_FALSE;
}

static NvBool
nvflash_cmd_go( void )
{
    NvError e;
    Nv3pCommand cmd;

    cmd = Nv3pCommand_Go;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, 0, 0 )
    );

    return NV_TRUE;
fail:
    return NV_FALSE;
}

static NvBool
nvflash_cmd_read( NvFlashCmdRead *a )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdReadPartition read_part;
    const char *err_str = 0;

    /* read the whole partition */
    read_part.Id = a->PartitionID;
    read_part.Offset = 0;
    cmd = Nv3pCommand_ReadPartition;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &read_part, 0 )
    );
    if(read_part.Length == 0)
    {
        err_str = "Partition not found\n";
        goto fail;
    }
    return nvflash_recvfile( a->filename, read_part.Length );

fail:
    if( err_str )
    {
        NvAuPrintf( "%s ", err_str);
    }
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_read_raw( NvFlashCmdRawDeviceReadWrite *a )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdRawDeviceAccess raw_read_dev;
    const char *err_str = 0;

    raw_read_dev.StartSector = a->StartSector;
    raw_read_dev.NoOfSectors = a->NoOfSectors;
    cmd = Nv3pCommand_RawDeviceRead;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &raw_read_dev, 0 )
    );
    NvAuPrintf("Bytes to receive %Lu\n", raw_read_dev.NoOfBytes);
    return nvflash_recvfile(a->filename, raw_read_dev.NoOfBytes);

fail:
    if( err_str )
    {
        NvAuPrintf( "%s ", err_str);
    }
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_write_raw( NvFlashCmdRawDeviceReadWrite *a )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdRawDeviceAccess raw_write_dev;
    const char *err_str = 0;
    NvOsStatType stat;

    raw_write_dev.StartSector = a->StartSector;
    raw_write_dev.NoOfSectors = a->NoOfSectors;
    cmd = Nv3pCommand_RawDeviceWrite;

    e = NvOsStat( a->filename, &stat );
    if( e != NvSuccess )
    {
        NvAuPrintf( "file not found: %s\n", a->filename );
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &raw_write_dev, 0 )
    );

    if( (raw_write_dev.NoOfBytes) && (stat.size < raw_write_dev.NoOfBytes) )
    {
        NvAuPrintf( "%s is too small for writing %Lu bytes\n", a->filename, raw_write_dev.NoOfBytes);
        return NV_FALSE;
    }

    //send raw_write_dev.NoOfBytes present in filename to  server
    return nvflash_sendfile(a->filename, raw_write_dev.NoOfBytes);

fail:
    if( err_str )
    {
        NvAuPrintf( "%s ", err_str);
    }
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n", cmd, e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_getpartitiontable(NvFlashCmdGetPartitionTable *a, Nv3pPartitionInfo **pPartitionInfo)
{
    NvError e = 0;
    Nv3pCommand cmd;
    Nv3pCmdReadPartitionTable arg;

    NvU32 bytes;
    char* buff = 0;
    char *err_str = 0;
    NvOsFileHandle hFile = 0;
    NvU32 MaxPartitionEntries = 0;
    NvU32 Entry = 0;

    #define MAX_STRING_LENGTH 256
    buff = NvOsAlloc(MAX_STRING_LENGTH);

    // These values will be non zero in case of call from skip error check.
    arg.NumLogicalSectors = a->NumLogicalSectors;
    arg.StartLogicalSector = a->StartLogicalSector;
    arg.ReadBctFromSD = a->ReadBctFromSD;

    cmd = Nv3pCommand_ReadPartitionTable;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );

    MaxPartitionEntries = (NvU32) arg.Length / sizeof(Nv3pPartitionInfo);
    if(MaxPartitionEntries > MAX_GPT_PARTITIONS_SUPPORTED)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    *pPartitionInfo = NvOsAlloc((NvU32)arg.Length);
    if(!(*pPartitionInfo))
        NV_CHECK_ERROR_CLEANUP(NvError_InsufficientMemory);

    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataReceive(s_hSock, (NvU8*)(*pPartitionInfo), (NvU32)arg.Length, &bytes, 0)
    );

    if (a->filename) // will be NULL in case of call from skip error check
    {
        e = NvOsFopen(a->filename, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile);
        VERIFY(e == NvSuccess, err_str = "file create failed"; goto fail);

        NvOsMemset(buff, 0, MAX_STRING_LENGTH);
        for (Entry = 0 ; Entry < MaxPartitionEntries ; Entry++)
        {
            NvOsSnprintf(buff,
                                    MAX_STRING_LENGTH,
                                    "PartitionId=%d\r\nName=%s\r\n"
                                    "DeviceId=%d\r\n"
                                    "StartSector=%d\r\nNumSectors=%d\r\n"
                                    "BytesPerSector=%d"
                                    "\r\n\r\n\r\n",
                                    (*pPartitionInfo)[Entry].PartId,
                                    (*pPartitionInfo)[Entry].PartName,
                                    (*pPartitionInfo)[Entry].DeviceId,
                                    (*pPartitionInfo)[Entry].StartLogicalAddress,
                                    (*pPartitionInfo)[Entry].NumLogicalSectors,
                                    (*pPartitionInfo)[Entry].BytesPerSector
                            );
            e = NvOsFwrite(hFile, buff, NvOsStrlen(buff));
            VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);
        }
        NvAuPrintf("Succesfully updated partition table information to %s\n", a->filename);
    }
fail:
    if (hFile)
        NvOsFclose(hFile);
    if (buff)
        NvOsFree(buff);
    if (a->filename && *pPartitionInfo) // Deallocation when no skip error check.
        NvOsFree(*pPartitionInfo);
    if (err_str)
    {
        NvAuPrintf("%s NvError 0x%x\n", err_str , e);
    }
    if (e != NvSuccess)
    {
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
nvflash_cmd_sync( void )
{
    NvError e;
    Nv3pCommand cmd;

    cmd = Nv3pCommand_Sync;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, 0, 0 )
    );

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_odmdata( NvU32 odm_data )
{
    NvError e;
    Nv3pCommand cmd;
    Nv3pCmdOdmOptions arg;
    NvBool b;

    cmd = Nv3pCommand_OdmOptions;
    arg.Options = odm_data;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    b = nvflash_wait_status();
    VERIFY( b, goto fail );

    NvAuPrintf( "odm data: 0x%x\n", odm_data );

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_setbootdevicetype( const NvU8 *devType )
{
    NvError e = NvSuccess;
    Nv3pCommand cmd = Nv3pCommand_SetBootDevType;
    Nv3pCmdSetBootDevType arg;
    Nv3pDeviceType DeviceType;
    NvBool b;

    // convert user defined string to nv3p device
    if (!NvOsStrncmp((const char *)devType, "nand_x8", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Nand;
    }
    else if (!NvOsStrncmp((const char *)devType, "nand_x16", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Nand_x16;
    }
    else if (!NvOsStrncmp((const char *)devType, "emmc", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Emmc;
    }
    else if (!NvOsStrncmp((const char *)devType, "spi", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Spi;
    }
    else if (!NvOsStrncmp((const char *)devType, "usb3", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Usb3;
    }
    else if (!NvOsStrncmp((const char *)devType, "sata", NV3P_STRING_MAX))
    {
        DeviceType = Nv3pDeviceType_Sata;
    }
    else
    {
        NvAuPrintf("Invalid boot device type: %s\n", devType);
        goto fail;
    }
    arg.DevType = DeviceType;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_setbootdeviceconfig( const NvU32 devConfig )
{
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_SetBootDevConfig;
    Nv3pCmdSetBootDevConfig arg;
    NvBool b;

    arg.DevConfig = devConfig;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_setodmcmd( NvFlashCmdSetOdmCmd *a )
{
    NvError e = NvSuccess;
    Nv3pCommand cmd = Nv3pCommand_Force32;
    Nv3pCmdOdmCommand arg;
    NvBool b;
    NvU8 *buf = NULL;
    NvOsStatType stat;

    switch( a->odmExtCmd ) {
    case NvFlashOdmExtCmd_FuelGaugeFwUpgrade:
        return  nvflash_odmcmd_fuelgaugefwupgrade(a);

    case NvFlashOdmExtCmd_UpgradeDeviceFirmware:
        return nvflash_odmcmd_upgradedevicefirmware(a);

    case NvFlashOdmExtCmd_GetTN_ID:
        cmd = Nv3pCommand_OdmCommand;
        arg.odmExtCmd = Nv3pOdmExtCmd_Get_Tnid;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
        );
        if (a->odmExtCmdParam.tnid_file.filename)
        {
            b = nvflash_recvfile(a->odmExtCmdParam.tnid_file.filename,
               arg.odmExtCmdParam.tnid_info.length);
            VERIFY(b, goto fail);
        }
        else
        {
            NvU32 Bytes = 0;
            NvU8 *pBuffer = NULL;

            pBuffer = NvOsAlloc(arg.odmExtCmdParam.tnid_info.length);
            VERIFY(pBuffer != NULL, goto fail);
            e = Nv3pDataReceive(s_hSock, pBuffer,
                    (arg.odmExtCmdParam.tnid_info.length), &Bytes, 0);
            VERIFY(e == NvSuccess, goto fail);
            NvAuPrintf("TNID %2X%02X%02X%02X\n", *pBuffer, *(pBuffer+1),
                    *(pBuffer+2), *(pBuffer+3));
            if (pBuffer != NULL)
                NvOsFree(pBuffer);
        }
        break;
    case NvFlashOdmExtCmd_VerifyT_id:
        cmd = Nv3pCommand_OdmCommand;
        arg.odmExtCmd = Nv3pOdmExtCmd_Verify_Tid;
        e = NvOsStat( a->odmExtCmdParam.tnid_file.filename, &stat);
        if( e != NvSuccess )
        {
            NvAuPrintf( "file not found: %s\n",
                        a->odmExtCmdParam.tnid_file.filename);
            goto fail;
        }
        arg.odmExtCmdParam.tnid_info.length = (NvU32)stat.size;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend( s_hSock, cmd, &arg, 0)
        );
        b = nvflash_sendfile( a->odmExtCmdParam.tnid_file.filename, 0);
        buf = NvOsAlloc(arg.odmExtCmdParam.tnid_info.length);
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( s_hSock, buf,
                arg.odmExtCmdParam.tnid_info.length, 0, 0));
        NvAuPrintf("status: %s \n",buf);
        break;
    case NvFlashOdmExtCmd_UpdateuCFw:
        VERIFY(
            nvflash_odmcmd_updateucfw(a), goto fail
        );
        break;
    case NvFlashOdmExtCmd_LimitedPowerMode:
        NvAuPrintf("Limited Power On - nothing to do\n");
        break;
    case NvFlashOdmExtCmd_TegraTabFuseKeys:
        VERIFY(
            nvflash_odmcmd_tegratabfusekeys(a), goto fail
        );
        break;
    case NvFlashOdmExtCmd_TegraTabVerifyFuse:
        VERIFY(
            nvflash_odmcmd_tegratabverifyfuse(a), goto fail
        );
        break;
    default:
        return NV_FALSE;
    }
    return NV_TRUE;
fail:
    if (buf)
        NvOsFree(buf);
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_fusewrite(const char *config_file)
{
    Nv3pCommand cmd;
    NvOsFileHandle hFile = 0;
    NvFuseWriteCallbackArg callback_arg;
    NvError e = NvSuccess;
    NvBool b  = NV_TRUE;
    const char *err_str = NULL;
    NvU32 size;
    NvU8 *buf = 0;
    Nv3pCmdFuseWrite arg;
    NvOsMemset(&callback_arg, 0, sizeof(callback_arg));
    e = NvOsFopen(config_file, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, err_str = "Fuse file open failed"; goto fail);
    e = nvflash_parser(hFile, &nvflash_fusewrite_parser_callback,
                       NULL, &callback_arg);
    VERIFY(e == NvSuccess, err_str = "Fuse file parsing failed"; goto fail);
    size = sizeof(callback_arg.FuseInfo);
    buf = NvOsAlloc(size);
    VERIFY(buf, err_str = "buffer allocation failed"; goto fail);
    NvOsMemcpy(buf, &(callback_arg.FuseInfo), size);
    cmd = Nv3pCommand_FuseWrite;
    arg.Length = size;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );
    e = nvflash_data_send(size, buf);
    VERIFY(e == NvSuccess, err_str = "Fuse data send failed"; goto fail);
    NvAuPrintf( "\nFuse details downloaded successfully\n" );
fail:
    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
        b = NV_FALSE;
    }
    if(hFile)
        NvOsFclose(hFile);
    if(buf)
        NvOsFree(buf);
    return b;
}

/*
 * Stores the configuration table on specified partition
 *
 * config_file: File containing configuration table about boards
 * partition id: Id of the partition where fuse info is to be stored
 * partition size: Size of the partition
 *
 * Returns: True if success else false
 */
static NvBool
nvflash_ParseConfigTable(
    const char *config_file, NvU32 partition_id, NvU64 partition_size)
{
    NvBool b  = NV_TRUE;
    NvError e = NvSuccess;
    const char *err_str = NULL;
    Nv3pCmdDownloadPartition arg;
    Nv3pCommand cmd;
    Nv3pCmdQueryPartition query_part;
    NvU32 EntrySize = 0;
    NvU8 *NctEntry = 0;
    NvOsFileHandle hFile = 0;
    NvConfigTableCallbackArg callback_arg;
    NvU8 *LocalBuffer = NULL;
    NvU32 Length = 0;
    nct_part_head_type Header;

    NvOsMemset(&callback_arg, 0, sizeof(callback_arg));
    NvOsMemset(&Header, 0, sizeof(nct_part_head_type));
    NvOsMemcpy(&Header.magicId, NCT_MAGIC_ID, sizeof(NCT_MAGIC_ID));
    callback_arg.Header = &Header;

    e = NvOsFopen(config_file, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, err_str = "ConfigurationTable file open failed"; goto fail);

    NvParseCaseSensitive(NV_TRUE);
    e = nvflash_parser(hFile, &nvflash_configtable_parser_callback,
                       NULL, &callback_arg);
    VERIFY(e == NvSuccess, err_str = "CongifurationTable file parsing failed"; goto fail);
    VERIFY(callback_arg.Found == NV_TRUE, err_str = "No matching ConfigurationTable";
                                 goto fail);
    ////////////////////////////////////////////////////////////////////////////////////////
    EntrySize = callback_arg.EntrySize;
    NctEntry = callback_arg.NctEntry;

    Length = callback_arg.offset + EntrySize;
    LocalBuffer = NvOsAlloc(Length);
    NvOsMemset(LocalBuffer, 0, Length);
    NvOsMemcpy(LocalBuffer, &Header, sizeof(nct_part_head_type));
    NvOsMemcpy(LocalBuffer + callback_arg.offset, NctEntry, EntrySize);
    if (!partition_size)
    {
        query_part.Id = partition_id;
        cmd = Nv3pCommand_QueryPartition;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, cmd, &query_part, 0)
        );

        /* wait for status */
        b = nvflash_wait_status();
        VERIFY(b, err_str = "failed to query partition"; goto fail);
        partition_size = query_part.Size;
    }

    if(Length > partition_size)
    {
        err_str = "NCT partition is smaller than structure size";
        goto fail;
    }


    VERIFY(e == NvSuccess,
          err_str = "Converting ConfigTable to bin failed"; goto fail);

    cmd = Nv3pCommand_DownloadPartition;
    arg.Id = partition_id;
    arg.Length = Length;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );

    e = Nv3pDataSend(s_hSock, LocalBuffer, Length, 0);
    VERIFY(e == NvSuccess, err_str = "Nct partition send failed"; goto fail);

    NvAuPrintf( "NCT partition downloaded successfully\n");

fail:
    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
        b = NV_FALSE;
    }

    if(hFile)
        NvOsFclose(hFile);
    if(NctEntry)
        NvOsFree(NctEntry);
    if(LocalBuffer)
        NvOsFree(LocalBuffer);

    return b;
}

static char *
nvflash_get_nack( Nv3pNackCode code )
{
    switch( code ) {
    case Nv3pNackCode_Success:
        return "success";
    case Nv3pNackCode_BadCommand:
        return "bad command";
    case Nv3pNackCode_BadData:
        return "bad data";
    default:
        return "unknown";
    }
}

static NvBool
nvflash_flush_runbdktests_results(NvU8* buf, NvOsFileHandle hFile)
{
    NvError e;
    const char *err_str = 0;

    if (hFile)
    {
        e = NvOsFwrite(hFile, buf, NvOsStrlen((char*)buf));
        VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);
    }
    else
        NvAuPrintf("%s", (char*)buf);
    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed  NvError 0x%x \n", e);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    return NV_FALSE;
}

static NvFlashParserStatus NvBdkPerformTest(
    NvBdkCallbackArg *a,
    NvBDkTest_arg *suite_arg,
    char *filename)
{
    NvU32 temp = 1;
    Nv3pCmdGetSuitesInfo suites = {0};
    NvU8 *suite_buffer = NULL;
    NvU8 *suite_head = NULL;
    NvBool b;
    NvU32 k = 0;
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_RunBDKTest;
    const char *err_str = NULL;
    NvFlashParserStatus ret = P_ERROR;
    NvU32 size = BDKTEST_RESULT_BUFFER_SIZE;
    NvU32 i = 0;
    NvU32 count = 0;
    NvU8 *transaction_buf = NULL;
    NvU32 bytes = 0;
    NvBool all_suite= NV_FALSE;
    char boardName[20];
    NvU8 *log_buffer = NULL;
    NvU32 totalTestTime = 0;

    NvOsMemset(boardName, 0, sizeof(boardName));
    NvOsGetConfigString("TARGET_PRODUCT", boardName, sizeof(boardName));
    if (!boardName[0])
        NvOsStrcpy(boardName, "unknown");
    if (a->out_format)
    {
        NvOsSnprintf(
            (char*)a->out_buffer, size,
            "<?xml version=\"1.0\"?>\n"
            "<BDKTEST>\n"
                "\t<BOARD>%s</BOARD>\n",
            boardName
        );
        e = nvflash_flush_runbdktests_results(a->out_buffer, a->hFile);
        VERIFY(e, err_str = "flush result failed"; goto fail);
    }

    // If it is "all" then read back  name of each suites
    if (!NvOsStrcmp(a->bdktest_params.Suite, "all"))
    {
        cmd = Nv3pCommand_BDKGetSuiteInfo;

        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, cmd, &suites, 0)
        );
        suite_buffer = NvOsAlloc(suites.size);
        if (!suite_buffer)
        {
            err_str = "Insufficient system memory";
            goto fail;
        }
        suite_head = suite_buffer;

        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(s_hSock, suite_buffer,suites.size, 0, 0)
        );

        // Disabling usbf test from all tests due to  inconsistency
        all_suite = NV_TRUE;

        b = nvflash_wait_status();
        VERIFY(b, goto fail);

        // The first 4 bytes are the no. of suites & following has the name of suites
        temp = *((NvU32 *)suite_buffer);
        suite_buffer += sizeof(NvU32);
    }

    NvOsMemset(a->out_buffer, 0, size);
    if (a->out_format)
    {
        NvOsSnprintf(
            (char*)a->out_buffer, size,
            "\t<TESTPLAN suite=\"%s\" type=\"%s\" >\n",
             a->bdktest_params.Suite, a->bdktest_params.Argument
            );
    }
    else
    {
        NvOsSnprintf(
            (char*)a->out_buffer, size ,
            "\n==============================================\n"
            "Running test entry: %s: %s\n"
            "==============================================\n",
             a->bdktest_params.Suite, a->bdktest_params.Argument
        );
    }
    // Process each suites independently irrespective of all
    while(temp--)
    {
        if (suites.size)
        {
            NvOsStrcpy(a->bdktest_params.Suite, (char *)suite_buffer);
            k = NvOsStrlen((char *)suite_buffer) + 1;
            suites.size -= k;
            suite_buffer += k;
        }

        // Find the Running suite PrivData
        for (k = 0; k < MAX_SUITE_NO; k++)
        {
            if (!NvOsStrcmp(a->bdktest_params.Suite, suite_arg[k].suite))
            {
                a->bdktest_params.PrivData = suite_arg[k].arg_data;
                break;
            }
        }

        if (k == MAX_SUITE_NO)
        {
            NvOsMemset(&(a->bdktest_params.PrivData),
                        0, sizeof(a->bdktest_params.PrivData));
        }

        // Write/Print test entry
        if (a->hFile)
        {
            e = NvOsFwrite(a->hFile, a->out_buffer,
                            NvOsStrlen((char*)a->out_buffer));
            VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);
        }
        else
        {
            NvAuPrintf("%s\n", (char*)a->out_buffer);
        }

        NvOsMemset(a->out_buffer, 0, size);

        cmd = Nv3pCommand_RunBDKTest;

        // USBF test not supported in all test suite due to the inconsistency
        if (all_suite && !NvOsStrcmp(a->bdktest_params.Suite, "usbf"))
            goto clean;

        // Start test run process
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, cmd, &a->bdktest_params, 0)
        );

        // Send the File if valid size exists
        if (a->bdktest_params.PrivData.File_size)
        {
            if (nvflash_match_file_ext(filename, ".txt"))
            {
                // send the structure
            }
            else
            {
                nvflash_sendfile(filename, 0);
                NvAuPrintf("\n");
            }
        }

        // If no tests are available convey it on the host
        if (!a->bdktest_params.NumTests)
        {
           NvAuPrintf("No Tests available for the following entry: %s %s %s\n",
                   a->bdktest_params.Suite, a->bdktest_params.Argument,
                                                    a->bdktest_params.PrivData);
        }
        else
        {
            count++;
            NvAuPrintf("%d) Running %s with %d tests \n",
                  count, a->bdktest_params.Suite, a->bdktest_params.NumTests);

            // Allocating memory for receiving result
            log_buffer = NvOsAlloc(BDKTEST_RESULT_BUFFER_SIZE);
            VERIFY(log_buffer, goto fail);
            NvOsMemset(log_buffer, 0, BDKTEST_RESULT_BUFFER_SIZE);

            for (i = 0; i < a->bdktest_params.NumTests ; i++)
            {
                /*
                 *  ------------------------------
                 * | length of data |    data     |
                 *  ------------------------------
                 * The size of length is 4 bytes and the size of data is variable.
                 *
                 *  -------------------------
                 * | length | [suite name]   |
                 *  -------------------------
                 * | length | [test name]    |
                 *  -------------------------
                 * | length | [status]       |
                 *  -------------------------
                 * | length | [elapsed time] |
                 *  -------------------------
                 * | length | [message]      |
                 *  -------------------------
                 */
                NvU8 *p;
                NvU32 suiteLen;
                NvU32 typeLen;
                NvU32 statusLen;
                NvU32 elapsedTimeLen;
                NvU32 msgLen;
                NvU32 elapsedTime;
                char *type;
                Nv3pStatus status;
                void *msg;


                NvOsMemset(a->out_buffer, 0, size);
                // if testsuite running is usbf,
                // receive perf test data from device.
                if ((!NvOsStrcmp(a->bdktest_params.Suite, "usbf"))
                                           && (i < a->bdktest_params.NumTests))
                {
                    transaction_buf = NvOsAlloc(DATA_CHUNK_SIZE_1MB);
                    if (!transaction_buf)
                        return NvError_InsufficientMemory;
                    NV_CHECK_ERROR_CLEANUP(
                        Nv3pDataReceive(s_hSock, transaction_buf,
                                               DATA_CHUNK_SIZE_1MB ,&bytes , 0)
                    );
                    NV_CHECK_ERROR_CLEANUP(
                        Nv3pDataSend(s_hSock, transaction_buf,
                                               DATA_CHUNK_SIZE_1MB, 0)
                    );
                }
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pDataReceive(s_hSock, a->out_buffer, size, 0, 0)
                );
                {
                    p = a->out_buffer;

                    NvOsMemcpy(&suiteLen, p, sizeof(suiteLen));
                    p += sizeof(suiteLen);
                    p += suiteLen;

                    NvOsMemcpy(&typeLen, p, sizeof(typeLen));
                    p += sizeof(typeLen);
                    type = (char *)p;
                    p += typeLen;

                    NvOsMemcpy(&statusLen, p, sizeof(statusLen));
                    p += sizeof(statusLen);
                    NvOsMemcpy(&status, p, statusLen);
                    p += statusLen;

                    NvOsMemcpy(&elapsedTimeLen, p, sizeof(elapsedTimeLen));
                    p += sizeof(elapsedTimeLen);
                    NvOsMemcpy(&elapsedTime, p, sizeof(elapsedTimeLen));
                    p += elapsedTimeLen;

                    NvOsMemcpy(&msgLen, p, sizeof(msgLen));
                    p += sizeof(msgLen);
                    msg = (char *)p;
                }
                totalTestTime += elapsedTime;
                if (status != Nv3pStatus_Ok)
                {
                    const char * bdk_err_str = 0;
                    switch (status)
                    {
                        case Nv3pStatus_BDKTestTimeout:
                            bdk_err_str = "Timeout";
                            break;
                        case Nv3pStatus_BDKTestRunFailure:
                            bdk_err_str = "Fail";
                            break;
                        case Nv3pStatus_InvalidState:
                            bdk_err_str = "Not Tested";
                            break;
                        default:
                            bdk_err_str = "Unknown";
                            break;
                    }
                    NvAuPrintf("[BDKTEST_ERROR:%s %s]\n", type, bdk_err_str);
                    a->testFailed = NV_TRUE;
                }
                if (a->out_format)
                {
                    NvOsSnprintf(
                        (char*)log_buffer, size,
                        "\t\t<%s>\n"
                        "\t\t\t<status>\"%s\"</status>\n"
                        "\t\t\t<elapsedTime>%d</elapsedTime>\n"
                        "\t\t</%s>\n",
                        type,
                        (status == Nv3pStatus_BDKTestRunFailure) ? msg : nvflash_status_string(status),
                        elapsedTime,
                        type
                        );
                }
                else
                {
                    NvOsSnprintf(
                        (char*)log_buffer, size,
                        "%s\n", msg
                        );
                }
                e = nvflash_flush_runbdktests_results(log_buffer, a->hFile);
                VERIFY(e, err_str = "flush result failed"; goto fail);
            }

            NvOsMemset(a->out_buffer, 0, size);

            /* get runner information */
            NV_CHECK_ERROR_CLEANUP(
                Nv3pDataReceive(s_hSock, a->out_buffer, size, 0, 0)
            );
            NvAuPrintf("  >> runner log<< \n");
            NvAuPrintf("%s\n", (char *)a->out_buffer);

            if (a->out_format)
            {
                NvOsMemset(a->out_buffer, 0, size);
                NvOsSnprintf(
                    (char*)a->out_buffer, size,
                    "\t</TESTPLAN>\n"
                    );
                e = nvflash_flush_runbdktests_results(a->out_buffer, a->hFile);
                VERIFY(e, err_str = "flush result failed"; goto fail);
            }
        }
        b = nvflash_wait_status();
        VERIFY(b, goto fail);
    }
    if (a->out_format)
    {
        NvOsMemset(a->out_buffer, 0, size);
        NvOsSnprintf(
            (char*)a->out_buffer, size,
            "\t<TotalTestTime>%d</TotalTestTime>\n"
            "</BDKTEST>\n",
            totalTestTime
        );
        e = nvflash_flush_runbdktests_results(a->out_buffer, a->hFile);
        VERIFY(e, err_str = "flush result failed"; goto fail);
    }
    NvAuPrintf(
        "===================================\n"
        "Total Time Taken %d Ms\n"
        "===================================\n",
        totalTestTime
    );
    ret = P_CONTINUE;
    goto clean;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n", cmd, e);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
clean:
    if (suite_head)
        NvOsFree(suite_head);
    if (transaction_buf)
        NvOsFree(transaction_buf);

    return ret;
}

static NvFlashParserStatus
nvflash_bdk_parser_callback(NvFlashVector *rec, void *aux)
{
    NvU32 i = 0;
    NvU32 nPairs = rec->n;
    NvFlashSectionPair *Pairs = rec->data;
    NvBdkCallbackArg *a = (NvBdkCallbackArg *) aux;
    static NvBDkTest_arg suite_arg[MAX_SUITE_NO];
    static NvU32 j = 0;
    NvU32 k = 0;
    NvBool arg_section = NV_FALSE;
    NvOsStatType stat;
    static char filename[NV3P_STRING_MAX];
    NvFlashParserStatus ret = P_ERROR;
    NvU32 temp;

    for(i = 0; i < nPairs; i++)
    {
        switch(Pairs[i].key[0])
        {
            case 's':
                if (!NvFlashIStrcmp(Pairs[i].key, "suite"))
                    NvOsStrcpy(a->bdktest_params.Suite, Pairs[i].value);
                else
                {
                    NvAuPrintf("Invalid token for suite");
                    goto fail;
                }
                break;

            case 't':
                if (!NvFlashIStrcmp(Pairs[i].key, "test"))
                    NvOsStrcpy(a->bdktest_params.Argument, Pairs[i].value);
                else
                {
                    NvAuPrintf("Invalid token for test");
                    goto fail;
                }
                break;

           case 'a':
                if (!NvFlashIStrcmp(Pairs[i].key, "arg"))
                {
                    for (k = 0; k < MAX_SUITE_NO ;k++)
                    {
                        if (!NvFlashIStrcmp(suite_arg[k].suite, Pairs[i].value))
                            break;
                    }
                    if (k == MAX_SUITE_NO)
                    {
                        NvOsStrcpy(suite_arg[j].suite, Pairs[i].value);
                        NvOsMemset(&(suite_arg[j].arg_data),
                            0, sizeof(suite_arg[j].arg_data));
                    }
                    else
                    {
                        NvOsMemset(&(suite_arg[k].arg_data), 0,
                                     sizeof(suite_arg[k].arg_data));
                    }
                    arg_section = NV_TRUE;
                }
                else
                {
                    NvAuPrintf("Invalid token for arg");
                    goto fail;
                }
                break;

           case 'i':
                if (!NvFlashIStrcmp(Pairs[i].key, "instance"))
                {
                    temp = NvUStrtoul(Pairs[i].value, NULL, 0);
                    if (k != MAX_SUITE_NO)
                        suite_arg[k].arg_data.Instance = temp;
                    else
                        suite_arg[j].arg_data.Instance = temp;
                }
                else
                {
                    NvAuPrintf("Invalid token for instance ");
                    goto fail;
                }
                break;

           case 'm':
               if (!NvFlashIStrcmp(Pairs[i].key, "mem_add"))
               {
                   temp = NvUStrtoul(Pairs[i].value, NULL, 0);
                   if (k != MAX_SUITE_NO)
                       suite_arg[k].arg_data.Mem_address = temp;
                   else
                       suite_arg[j].arg_data.Mem_address = temp;
               }
               else
               {
                   NvAuPrintf("Invalid token for mem_add");
                   goto fail;
               }
               break;

           case 'r':
               if (!NvFlashIStrcmp(Pairs[i].key, "reserved"))
               {
                   temp = NvUStrtoul(Pairs[i].value, NULL, 0);
                   if (k != MAX_SUITE_NO)
                       suite_arg[k].arg_data.Reserved = temp;
                   else
                       suite_arg[j].arg_data.Reserved = temp;
               }
               else
               {
                   NvAuPrintf("Invalid token for reserved");
                   goto fail;
               }
               break;

           case 'f':
                if (!NvFlashIStrcmp(Pairs[i].key, "filename"))
                {
                    NvOsStrcpy(filename, Pairs[i].value);

                    if (nvflash_match_file_ext(filename,".txt"))
                    {
                        //Parse the file & fill the structure
                    }
                    else
                    {
                        NvOsStat(Pairs[i].value, &stat);
                        if (k != MAX_SUITE_NO)
                            suite_arg[k].arg_data.File_size = (NvU32)stat.size;
                        else
                            suite_arg[j].arg_data.File_size = (NvU32)stat.size;
                    }
                }
                else
                {
                    NvAuPrintf("Invalid token for filename");
                    goto fail;
                }
                break;

            default:
                NvAuPrintf("Invalid token");
        }
    }

    if (arg_section)
    {
        if ((k == MAX_SUITE_NO) && arg_section)
            j++;
        return P_CONTINUE;
    }
    ret = NvBdkPerformTest(a, suite_arg, filename);
fail:
    return ret;
}

static NvBool
nvflash_cmd_runbdktests(NvFlashCmdBDKTest *a)
{
    NvBool b=NV_TRUE;
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_SetBDKTest;
    Nv3pCmdSetBDKTest set_arg;
    Nv3pCmdReset reset_arg;
    NvOsStatType stat;
    const char *err_str = 0;
    NvU32 size = BDKTEST_RESULT_BUFFER_SIZE;
    NvU8 *resultbuf = 0;
    NvOsFileHandle hFile1 = 0, hFile = 0;
    NvBdkCallbackArg callback_arg;
    NvBool log_xml = NV_FALSE;
    NvBool bdktest_reset = NV_FALSE;

    // Open TestPlan config file
    e = NvOsFopen(a->cfgfilename, NVOS_OPEN_READ, &hFile1 );
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    e = NvOsFstat(hFile1, &stat);
    VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);

    if (a->outfilename == NULL)
    {
        if ((a->opts >> BDKTESTOPTIONS_LOGFORMAT_BIT) & 0x000F)
            log_xml = NV_TRUE;
        else
            log_xml = NV_FALSE;
    }
    else if (a->outfilename)
    {
        if ((a->opts >> BDKTESTOPTIONS_LOGFORMAT_BIT) & 0x000F)
            log_xml = NV_TRUE;
        else
            log_xml = NV_FALSE;
    }

    if (a->outfilename)
    {
        e = NvOsFopen(a->outfilename, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile);
        VERIFY(e == NvSuccess, err_str = "file create failed"; goto fail);
    }

    // Allocating memory for receiving result
    resultbuf = NvOsAlloc(size);
    VERIFY(resultbuf, goto fail);
    NvOsMemset(resultbuf, 0, size);

    callback_arg.hFile = hFile;
    callback_arg.out_buffer = resultbuf;
    callback_arg.out_format = log_xml;
    callback_arg.testFailed = NV_FALSE;

    // Set BDK test condition
    // StopOnFail is to stop BDKTest when test is failed.
    // Timeout is to prevent stuck of nvflash. It is for each BDKTest case.
    cmd = Nv3pCommand_SetBDKTest;
    if ((a->opts >> BDKTESTOPTIONS_STOPONFAIL_BIT) & 0x0001)
        set_arg.StopOnFail = NV_TRUE;
    else
        set_arg.StopOnFail = NV_FALSE;
    set_arg.Timeout = (a->opts >> BDKTESTOPTIONS_TIMEOUT_BIT) & 0x00FF;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &set_arg, 0)
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);

    // Parse and send test entries
    e = nvflash_parser(hFile1, &nvflash_bdk_parser_callback,
                       NULL, &callback_arg);

    if ((a->opts >> BDKTESTOPTIONS_RESET_BIT) & 0x0001)
        bdktest_reset = NV_TRUE;

    if ((!callback_arg.testFailed) && bdktest_reset)
    {
        cmd = Nv3pCommand_Reset;
        reset_arg.Reset = Nv3pResetType_RecoveryMode;
        reset_arg.DelayMs = 100;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, cmd, &reset_arg, 0)
        );
        b = nvflash_wait_status();
        VERIFY(b, goto fail);
    }
    goto clean;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n", cmd, e);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    b = NV_FALSE;
clean:
    if (a->outfilename)
        NvOsFclose(hFile);
    if (resultbuf)
        NvOsFree(resultbuf);
    if (callback_arg.testFailed)
        b = NV_FALSE;
    return b;
}

void nvflash_buildbct(void)
{
    NvError e = NvSuccess;
    const char *pBctOutFile = NULL;
    const char *pBctFile = NULL;
    NvU32 DeviceId;
    NvU32 DevInstance = 0;
    NvS32 StartExtension;
    NvFlashBctFiles *pBuildBctParam = NULL;
    NvUsbDeviceHandle hUsb = NULL;
    BuildBctArg ArgBuildBct;

    NvAuPrintf("building bct \n");
    e = NvFlashCommandGetOption(NvFlashOption_Buildbct,
                                (void **)&pBuildBctParam);
    VERIFY(e == NvSuccess, goto fail);

    pBctFile = pBuildBctParam->BctOrCfgFile;
    StartExtension = NvOsStrlen(pBctFile);
    StartExtension -= NvOsStrlen(BCT_CFG_FILE_EXTENSION);
    StartExtension = StartExtension > 0 ? StartExtension : 0;

    if (NvFlashIStrcmp(pBctFile + StartExtension, BCT_CFG_FILE_EXTENSION) == 0)
    {
        pBctOutFile = pBuildBctParam->OutputBctFile;
        if (pBctOutFile == NULL)
        {
            pBctOutFile = "flash.bct";
        }

        e = NvFlashCommandGetOption(NvFlashOption_EmulationDevId,
                                    (void *)&DeviceId);
        VERIFY(e == NvSuccess, goto fail);

        if (!DeviceId)
        {
            e = NvFlashCommandGetOption(NvFlashOption_DevInstance,
                                        (void *)&DevInstance);
            VERIFY(e == NvSuccess, goto fail);

            nvflash_rcm_open(&hUsb, DevInstance);
            VERIFY(hUsb, goto fail);
            // obtain the product id
            DeviceId = nvflash_read_devid(hUsb);
            nvflash_rcm_close(hUsb);
        }

        switch (DeviceId)
        {
            case 0x30:
            case 0x35:
            case 0x14:
            case 0x40:
                break;
            default:
            {
                NvAuPrintf("invalid chip id\n");
                e = NvError_BadParameter;
                goto fail;
            }
        }

        NvOsMemset(&ArgBuildBct, 0, sizeof(ArgBuildBct));
        ArgBuildBct.pCfgFile = pBctFile;
        ArgBuildBct.pBctFile = pBctOutFile;
        ArgBuildBct.ChipId = DeviceId;
        ArgBuildBct.Offset = 0;

        // Use nvbuildbct library to create the bct file
        NV_CHECK_ERROR_CLEANUP(NvBuildBct(&ArgBuildBct));
    }

fail:
    if (e)
        NvAuPrintf("buildbct failed \n");

}

static NvBool nvflash_cmd_symkeygen(NvFlashCmdSymkeygen *a)
{
    Nv3pCommand cmd;
    Nv3pCmdSymKeyGen arg;
    NvError e;
    NvOsStatType stat;
    const char *err_str = 0;
    NvBool b = NV_FALSE;

    e = NvOsStat(a->spub_filename, &stat );
    arg.Length = (NvU32)stat.size;
    VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);
    cmd = Nv3pCommand_symkeygen;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    if (!a->outfilename)
        a->outfilename = "d_session_encrypt.txt";
    b = nvflash_sendfile(a->spub_filename,0);
    VERIFY(b, err_str = "sendfile failed"; goto fail);
    b = nvflash_recvfile(a->outfilename, arg.Length );
    VERIFY(b, err_str = "recieve file failed"; goto fail);

    b = NV_TRUE;
fail:
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    return b;
}

static NvError nvflash_data_send(NvU64 bytesLeftToTransfer, NvU8 *buf)
{
    NvU32 transferSize = 0;
    char *spinner = "-\\|/";
    NvError e = NvSuccess;
    NvU64 offset;
    NvU32 spin_idx = 0;

    offset = 0;
    while (bytesLeftToTransfer)
    {
        transferSize = (NvU32) ((bytesLeftToTransfer > NVFLASH_DOWNLOAD_CHUNK) ?
                                NVFLASH_DOWNLOAD_CHUNK : bytesLeftToTransfer);
        e = Nv3pDataSend(s_hSock, buf + offset, transferSize, 0);
        if (e)
            goto fail;
        bytesLeftToTransfer -= transferSize;
        offset += transferSize;

        if(!s_Quiet)
        {
            NvAuPrintf("\r%c %llu/%llu bytes sent", spinner[spin_idx],
                        offset, offset + bytesLeftToTransfer);
            spin_idx = (spin_idx + 1) % 4;
        }
    }

fail:
    NvAuPrintf("\n");
    return e;
}
static NvBool nvflash_cmd_dfuseburn(NvFlashCmdDFuseBurn *a)
{
    Nv3pCommand cmd;
    Nv3pCmdDFuseBurn arg;
    NvU8 *fuse_buff =NULL;
    NvU8 *bct_buff = NULL;
    NvU8 *bl_buff = NULL;
    NvU8 *mb_buff = NULL;
    Nv3pCmdDownloadPartition d_arg;
    NvBool b;
    NvError e = NvSuccess;
    NvU32 size;

    NvAuPrintf("Starting fuse burning code \n");

    cmd = Nv3pCommand_DownloadPartition;
    b = nvflash_parse_blob(a->blob_dsession, &bl_buff, NvFlash_Bl, (NvU32 *) &size);
    VERIFY(b, goto fail);
    size -= sizeof(NvU32);
    d_arg.Id = *((NvU32 *)(bl_buff + size));
    d_arg.Length = size;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &d_arg, 0)
    );
    e = nvflash_data_send(size, (NvU8 *)bl_buff);
    VERIFY(e == NvSuccess, goto fail);
    b = nvflash_wait_status();
    NvAuPrintf("Bootloader send successfully \n");
    VERIFY( b, goto fail );
    d_arg.Length = 0;
    size = 0;
    nvflash_parse_blob(a->blob_dsession, &mb_buff, NvFlash_mb, (NvU32 *) &size);
    if (size)
    {
        size -= sizeof(NvU32);
        d_arg.Id = *((NvU32 *)(mb_buff + size));
        d_arg.Length = size;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, cmd, &d_arg, 0)
        );
        e = nvflash_data_send(size, mb_buff);
        VERIFY(e == NvSuccess, goto fail);
        b = nvflash_wait_status();
        VERIFY( b, goto fail );
        NvAuPrintf("Microboot send successfully \n");
    }
    b = nvflash_parse_blob(a->blob_dsession, &bct_buff, NvFlash_Bct, (NvU32 *) &arg.bct_length);
    VERIFY(b, goto fail);
    b = nvflash_parse_blob(a->blob_dsession, &fuse_buff, NvFlash_Fuse, (NvU32 *) &arg.fuse_length);
    VERIFY(b, goto fail);

    cmd = Nv3pCommand_DFuseBurn;
    NV_CHECK_ERROR(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 ));
    e = nvflash_data_send(arg.bct_length,bct_buff);
    e = nvflash_data_send(arg.fuse_length,fuse_buff);
    b = NV_TRUE;
    NvAuPrintf("Completed fuse burning code \n");
fail:
    if (fuse_buff)
        NvOsFree(fuse_buff);
    if (bct_buff)
        NvOsFree(bct_buff);
    if (bl_buff)
        NvOsFree(bl_buff);
    if (mb_buff)
        NvOsFree(mb_buff);
    return b ;
}

static NvBool
nvflash_cmd_reset(NvFlashCmdReset *a)
{
    NvBool b;
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_Reset;
    Nv3pCmdReset arg;

    arg.Reset = a->reset;
    arg.DelayMs = a->delay_ms;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    b = nvflash_wait_status();
    VERIFY( b, goto fail );
    return b;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n",cmd,e);
    return NV_FALSE;
}

static NvBool
nvflash_cmd_readnctitem(NvFlashCmdReadWriteNctItem *a)
{
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_ReadNctItem;
    Nv3pCmdReadWriteNctItem arg;
    void *buf = NULL;

    arg.EntryIdx = a->EntryIdx;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );
    buf = NvOsAlloc(sizeof(nct_item_type));
    NV_CHECK_ERROR_CLEANUP(
        Nv3pDataReceive( s_hSock, (NvU8*)buf, sizeof(nct_item_type), 0, 0 )
    );
    switch( arg.Type )
    {
        case 0x10:
            NvAuPrintf("\nNCT Item(%d): %d\n", arg.EntryIdx, *(NvU8 *)buf);
            break;
        case 0x20:
            NvAuPrintf("\nNCT Item(%d): 0x%x\n", arg.EntryIdx, *(NvU16 *)buf);
            break;
        case 0x40:
            NvAuPrintf("\nNCT Item(%d): 0x%x\n", arg.EntryIdx, *(NvU32 *)buf);
            break;
        case 0x80:
            NvAuPrintf("\nNCT Item(%d): %s\n", arg.EntryIdx, buf);
            break;
        case 0x1A:
            break;
        case 0x2A:
            break;
        case 0x4A:
            break;
        case 0x8A:
            break;
        default:
            NvAuPrintf("\nNCT Item(%d) undefined type\n");
            break;
    }
    NvOsFree(buf);
    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n",cmd,e);
    NvOsFree(buf);
    return NV_FALSE;
}

#if ENABLE_NVDUMPER
static NvBool
nvflash_cmd_nvtbootramdump(NvFlashCmdNvtbootRAMDump *a)
{
    NvBool b = NV_FALSE;
    NvError e = NvSuccess;
    Nv3pCommand cmd;
    Nv3pCmdNvtbootRAMDump arg;
    NvOsFileHandle hFile = 0;
    NvU8 *Buf = 0;
    NvU32 Size = 0;
    NvU8 *tempPtr = NULL;
    NvU32 Bytes = 0;
    char Spinner[] = "-\\|/";
    NvU32 SpinIndex = 0;
    NvU32 BytesRecieved = 0;
    #define NVFLASH_UPLOAD_CHUNK (1024 * 2)

    arg.Offset = a->Offset;
    arg.Length = a->Length;
    NvAuPrintf( "Dumping Sdram StartAddr 0x%x total 0x%x bytes into: %s\n", a->Offset, a->Length, a->filename );

    cmd = Nv3pCommand_NvtbootRAMDump;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, cmd, &arg, 0)
    );
    b = nvflash_wait_status();
    VERIFY(b, goto fail);

    b = nvflash_recvfile(a->filename, arg.Length);
    VERIFY(b, goto fail);

    NvAuPrintf( "\nRamDump Successfully, device reboot\n");

    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("Failed sending command %d NvError %d\n",cmd,e);

    return NV_FALSE;
}
#endif

static NvBool
nvflash_parse_nctdata( const char* buf, NvU32 idx, void* data )
{
    if (!data)
        return NV_FALSE;

    switch( idx )
    {
        case NCT_ID_SERIAL_NUMBER:
            NvOsStrcpy((char *)data, buf);
            break; // String
        case NCT_ID_WIFI_MAC_ADDR:
        case NCT_ID_BT_ADDR:
            // TBD
            break; // 1byte data array
        case NCT_ID_CM_ID:
        case NCT_ID_LBH_ID:
            *(NvU16 *)data = (NvU16)NvUStrtoul(buf, 0, 0);
            break; // 2byte data
        case NCT_ID_FACTORY_MODE:
        case NCT_ID_RAMDUMP:
            *(NvU32 *)data = (NvU32)NvUStrtoul(buf, 0, 0);
            break; // 4byte data
        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
nvflash_cmd_writenctitem(NvFlashCmdReadWriteNctItem *a)
{
    NvError e;
    Nv3pCommand cmd = Nv3pCommand_WriteNctItem;
    Nv3pCmdReadWriteNctItem arg;

    arg.EntryIdx = a->EntryIdx;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    arg.Data = NvOsAlloc(sizeof(nct_item_type));
    NvOsMemset(arg.Data, 0, (sizeof(nct_item_type)));
    nvflash_parse_nctdata(a->Data, arg.EntryIdx, arg.Data);
    e = Nv3pDataSend(s_hSock, (NvU8 *)arg.Data, sizeof(nct_item_type), 0);
    VERIFY(e == NvSuccess, goto fail);

    NvOsFree(arg.Data);
    return NV_TRUE;

fail:
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x \n",cmd,e);
    NvOsFree(arg.Data);
    return NV_FALSE;

}


/** executes the commands */
NvBool
nvflash_dispatch( void )
{
    NvBool b = NV_TRUE;
    NvBool bBL = NV_FALSE;
    NvBool bCheckStatus;
    NvBool bSync = NV_FALSE;
    NvError e;
    void *arg;
    char *cfg_filename = NULL;
    char *bct_filename = NULL;
    const NvFlashBctFiles *bct_files;
    char *bootloader_filename;
    NvU32 nCmds;
    NvFlashCommand cmd;
    const char *err_str = 0;
    NvU32 odm_data = 0;
    NvU8 *devType;
    NvU32 devConfig;
    NvBool FormatAllPartitions;
    NvBool verifyPartitionEnabled;
    NvBool IsBctSet;
    NvBool bSetEntry = NV_FALSE;
    Nv3pCmdDownloadBootloader loadPoint;
    Nv3pTransportMode TransportMode;
    NvBool SimulationMode = NV_FALSE;
    NvFlashFuseBypass fuseBypass_data;
    NvFlashVersionStruct *versioninfo;
    const char *pBctSection = NULL;
    NvBool IsDownloadCommand = NV_FALSE;
    NvBool Reset = NV_FALSE;
    NvFlashCmdReset ResetArg = {0};
    NvFlashAutoFlash *pAutoFlash = NULL;
    NvBool IsLimitedPowerMode = NV_FALSE;

    e = NvFlashCommandGetOption(NvFlashOption_AutoFlash, (void *)&pAutoFlash);
    VERIFY(e == NvSuccess, goto fail);

    if (pAutoFlash != NULL && pAutoFlash->DoAutoFlash)
        NvFlashSetDefaultFlashValues();

    /* get all of the options, just in case */
    e = NvFlashCommandGetOption( NvFlashOption_ConfigFile,
        (void *)&cfg_filename );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption(NvFlashOption_Bct,
        (void *)&bct_files);
    VERIFY( e == NvSuccess, goto fail );
    bct_filename = (char*)bct_files->BctOrCfgFile;

    e = NvFlashCommandGetOption( NvFlashOption_Bootloader,
        (void *)&bootloader_filename );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_Quiet, (void *)&s_Quiet );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_OdmData, (void *)&odm_data );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBootDevType,
      (void *)&devType );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBootDevConfig,
      (void *)&devConfig );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_FormatAll,
      (void *)&FormatAllPartitions );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_VerifyEnabled, (void *)&verifyPartitionEnabled);
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_SetBct,
      (void *)&IsBctSet );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption( NvFlashOption_TransportMode,
      (void *)&TransportMode );
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption(NvFlashOption_FuseBypass,
                                (void *)&fuseBypass_data);
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption(NvFLashOption_VersionInfo,
                                (void *)&versioninfo);
    VERIFY( e == NvSuccess, goto fail );

    e = NvFlashCommandGetOption(NvFlashOption_UpdateBct,
                                (void **)&pBctSection);
    VERIFY(e == NvSuccess, goto fail);

    e = NvFlashCommandGetOption( NvFlashOption_ReportFlashProgress,
      (void *)&s_ReportFlashProgress );
    VERIFY( e == NvSuccess, goto fail );

    if (versioninfo->IsVersionInfo)
    {
        NvFlashBinaryVersionInfo NvFlashVersion;

        nvflash_parse_binary_version(NVFLASH_VERSION, &NvFlashVersion);
        if ((versioninfo->MajorNum1 == 0) && (versioninfo->MajorNum2 == 0))
        {
            // if Input is not given,then just print the current
            // version information
            versioninfo->MajorNum1 = NvFlashVersion.MajorNum;
            versioninfo->MajorNum2 = NvFlashVersion.MajorNum;
        }
        b = nvflash_display_versioninfo(versioninfo->MajorNum1,
                                        versioninfo->MajorNum2);
        goto clean;
    }
#ifndef NVHOST_QUERY_DTB
    if(TransportMode == Nv3pTransportMode_Sema)
        SimulationMode = NV_TRUE;

    if( cfg_filename )
    {
        b = nvflash_configfile( cfg_filename );
        VERIFY( b, goto fail );
    }

#ifdef NV_EMBEDDED_BUILD
    e = nvflash_create_filesystem_image();
    VERIFY(e == NvSuccess , goto fail);

    e = nvflash_create_kernel_image();
    VERIFY(e == NvSuccess , goto fail);
#endif
    // Check all partitions added to the list are existent
    // and have associated data.
    // This should be always done after parsing of the config file
    // since the number of devices and partitions per device
    // are known only after that.
    if (verifyPartitionEnabled)
    {
        b = nvflash_check_verify_option_args();
        VERIFY( b, goto fail );
    }
#endif
    /* Initialize ResetArg */
    ResetArg.reset = NvFlashResetType_Normal;
    ResetArg.delay_ms = 0;

    /* connect to the nvap */
    b = nvflash_connect();
    VERIFY( b, goto fail );

#ifdef NVHOST_QUERY_DTB
    goto clean;
#endif
    nCmds = NvFlashGetNumCommands();
    VERIFY( nCmds >= 1,
        err_str = "no more commands to execute"; goto fail );

    /* commands are executed in order (from the command line). all commands
     * that can be handled by the miniloader will be handled by the miniloader
     * until the bootloader is required, at which point the bootloader will
     * be downloaded.
     *
     * if resume mode is specified, then don't try to download a bootloader
     * since it should already be running.
     */
    if( s_Resume )
    {
        bBL = NV_TRUE;
    }

    if( IsBctSet && SimulationMode)
    {
        e = NvFlashCommandGetOption(NvFlashOption_Bct,
                   (void *)&bct_files);
        VERIFY( e == NvSuccess, goto fail );
        VERIFY(bct_files->BctOrCfgFile, err_str = "bct file required for this command"; goto fail);
        b = nvflash_cmd_setbct(bct_files);
        VERIFY( b, err_str = "setbct failed"; goto fail );
        b = nvflash_wait_status();
        VERIFY( b, goto fail );
        if( odm_data )
        {
            b = nvflash_cmd_odmdata( odm_data );
            VERIFY( b, err_str = "odm data failure"; goto fail );
        }
    }

    /* force a sync if the odm data, etc., has been specified -- this is
     * paranoia since the 'go' command implicitly will sync.
     */
    if( odm_data || devType || devConfig )
    {
        bSync = NV_TRUE;
    }

    #define CHECK_BL() \
        do { \
            if( !bBL && !SimulationMode) \
            { \
                if( bootloader_filename ) \
                { \
                    b = nvflash_cmd_bootloader( bootloader_filename ); \
                    VERIFY( b, err_str = "bootloader download failed"; \
                        goto fail ); \
                } \
                else \
                { \
                    err_str = "no bootloader was specified"; \
                    goto fail; \
                } \
                bBL = NV_TRUE; \
                b = nvflash_validate_nv3pserver_version(s_hSock);     \
                VERIFY(b, err_str = "validate nv3pserver version failed"; goto fail);   \
            } \
        } while( 0 )

    /* check setentry option for downloading u-boot */
    if (NvFlashCommandGetOption( NvFlashOption_EntryAndAddress,
                                (void *)&loadPoint ) == NvSuccess)
    {
        bSetEntry = NV_TRUE;
    }

    /* commands that change the partition layout or BCT will need to
     * be followed up eventually with a Sync (or Go) command. this will be
     * issued before nvflash exits and is controlled by bSync.
     */

    while( nCmds )
    {
        bCheckStatus = NV_TRUE;

        e = NvFlashCommandGetCommand( &cmd, &arg );
        VERIFY( e == NvSuccess, err_str = "invalid command"; goto fail );

        switch( cmd ) {
        case NvFlashCommand_Help:
            nvflash_usage();
            bCheckStatus = NV_FALSE;
            break;
        case NvFlashCommand_CmdHelp:
            {
                NvFlashCmdHelp *a = (NvFlashCmdHelp *)arg;
                nvflash_cmd_usage(a);
                NvOsFree( a );
                bCheckStatus = NV_FALSE;
                break;
            }
        case NvFlashCommand_Create:
            VERIFY( IsBctSet, err_str = "--setbct required for --create "; goto fail );
            /* need a cfg file */
            VERIFY( s_Devices, err_str = "config file required for this"
                " command is missing"; goto fail );

            CHECK_BL();

            e = NvOsGetSystemTime(&EndTime);
            VERIFY(e == NvSuccess, err_str = "Getting system time failed";
                goto fail);

           NvAuPrintf("ML execution and Cpu Handover took %d Secs\n",
               (EndTime.Seconds - StartTime.Seconds));

            /* if set entry for u-boot, exit from here */
            if (bSetEntry == NV_TRUE)
                return NV_TRUE;

            e = NvOsGetSystemTime(&StartTime);
            VERIFY(e == NvSuccess, err_str = "Getting system time failed";
                goto fail);
            b = nvflash_backup_partitions();
            e = NvOsGetSystemTime(&EndTime);
            VERIFY(e == NvSuccess, err_str = "Getting system time failed";
                goto fail);

            NvAuPrintf("Partition backup took %d Secs\n",
                (EndTime.Seconds - StartTime.Seconds));
            VERIFY(b, err_str = "Backup partition failed"; goto fail);
            b = nvflash_cmd_create();
            VERIFY( b, err_str = "create failed"; goto fail );
            nvflash_remove_backuped_partitions();
            /* nvflash_cmd_create() will check for the status since there are
             * multiple nv3p commands.
             */
            bCheckStatus = NV_FALSE;
            bSync = NV_TRUE;
            break;
        case NvFlashCommand_SetTime:
            CHECK_BL();
            b = nvflash_cmd_settime();
            VERIFY( b, err_str = "create failed"; goto fail );
            bCheckStatus = NV_FALSE;
            break;
#if NV_FLASH_HAS_OBLITERATE
        case NvFlashCommand_Obliterate:
            /* need a cfg file */
            VERIFY( s_Devices, err_str = "config file required for this"
                " command is missing"; goto fail );
            CHECK_BL();
            b = nvflash_cmd_obliterate();
            VERIFY( b, err_str = "obliterate failed"; goto fail );
            bCheckStatus = NV_FALSE;
            bSync = NV_TRUE;
            break;
#endif
        case NvFlashCommand_GetBit:
        {
            NvFlashCmdGetBit *a =
                (NvFlashCmdGetBit *)arg;
            b = nvflash_cmd_getbit( a );
            NvOsFree( a );
            VERIFY( b, err_str = "getbit failed"; goto fail );
            bCheckStatus = NV_FALSE;
            break;
        }
        case NvFlashCommand_DumpBit:
        {
            NvFlashCmdDumpBit *a =
                (NvFlashCmdDumpBit *)arg;
            b = nvflash_cmd_dumpbit( a );
            NvOsFree( a );
            VERIFY( b, err_str = "dumpbit failed"; goto fail );
            bCheckStatus = NV_FALSE;
            break;
        }
        case NvFlashCommand_GetBct:
            VERIFY( bct_filename,
                err_str = "bct file required for this command";
                goto fail );
            b = nvflash_cmd_getbct(bct_filename);
            VERIFY( b, err_str = "getbct failed"; goto fail );
            bCheckStatus = NV_FALSE;
            break;
        case NvFlashCommand_Go:
            CHECK_BL();

            /* if set entry for u-boot, exit from here */
            if (bSetEntry == NV_TRUE)
                return NV_TRUE;

            b = nvflash_cmd_go();
            VERIFY( b, err_str = "go failed\n"; goto fail );
            break;
        case NvFlashCommand_SetBoot:
        {
            NvFlashCmdSetBoot *a = (NvFlashCmdSetBoot *)arg;
            CHECK_BL();
            // If input argument is a partition name string , then retrieve the partition id.
            if (!a->PartitionID)
            {
                a->PartitionID =
                    nvflash_get_partIdByName(a->PartitionName, NV_FALSE);
                VERIFY(a->PartitionID, err_str = "partition not found in partition table";
                       goto fail);
            }
            b = nvflash_cmd_setboot( a );
            NvOsFree( a );
            VERIFY( b, err_str = "setboot failed"; goto fail );
            bSync = NV_TRUE;
            break;
        }
        case NvFlashCommand_FormatPartition:
        {
            NvFlashCmdFormatPartition *a =
                (NvFlashCmdFormatPartition *)arg;
            VERIFY( !IsBctSet, err_str = "--setbct not requied --formatpatition"; goto fail );
            CHECK_BL();
            // If input argument is a partition name string , then retrieve the partition id.
            if (!a->PartitionID)
            {
                a->PartitionID =
                    nvflash_get_partIdByName(a->PartitionName, NV_FALSE);
                VERIFY(a->PartitionID, err_str = "partition not found in partition table";
                       goto fail);
            }
            b = nvflash_cmd_formatpartition(Nv3pCommand_FormatPartition, a->PartitionID);
            NvOsFree( a );
            VERIFY( b, err_str = "format partition failed"; goto fail );
            bCheckStatus = NV_FALSE;
            bSync = NV_TRUE;
            break;
        }
        case NvFlashCommand_Download:
        {
            NvFlashCmdDownload *a = (NvFlashCmdDownload *)arg;

            VERIFY(!IsBctSet,
                   err_str = "--setbct not required --download"; goto fail);

            CHECK_BL();

            IsDownloadCommand = NV_TRUE;
            bSync = NV_TRUE;

            if (a->PartitionID != 0 || a->PartitionName != NULL)
            {
                b = NvFlashDownloadPartition(a);
                NvOsFree(a);
            }
            else
            {
                bCheckStatus = NV_FALSE;

                /* If not partition is given then filename points to
                 * flash.cfg. Parse flash.cfg first.
                 */
                b = nvflash_configfile(a->filename);

                NvOsFree(a);
                a = NULL;

                VERIFY(b, goto fail);

                /* Download all partitions present in flash.cfg */
                b = NvFlashFlashAllPartitions();
            }

            VERIFY(b, goto fail);
            break;
        }
        case NvFlashCommand_Read:
        {
            NvFlashCmdRead *a = (NvFlashCmdRead *)arg;
            VERIFY( !IsBctSet, err_str = "--setbct not required --read"; goto fail );
            CHECK_BL();
            // If input argument is a partition name string , then retrieve the partition id.
            if (!a->PartitionID)
            {
                a->PartitionID =
                    nvflash_get_partIdByName(a->PartitionName, NV_FALSE);
                VERIFY(a->PartitionID, err_str = "partition not found in partition table";
                       goto fail);
            }
            b = nvflash_cmd_read( a );
            NvOsFree( a );
            VERIFY( b, err_str = "read failed"; goto fail );
            break;
        }
        case NvFlashCommand_RawDeviceRead:
        {
            NvFlashCmdRawDeviceReadWrite *a = (NvFlashCmdRawDeviceReadWrite*)arg;
            CHECK_BL();
            b = nvflash_cmd_read_raw( a );
            NvOsFree( a );
            VERIFY( b, err_str = "readdeviceraw failed"; goto fail );
            break;
        }
        case NvFlashCommand_RawDeviceWrite:
        {
            NvFlashCmdRawDeviceReadWrite *a = (NvFlashCmdRawDeviceReadWrite*)arg;
            CHECK_BL();
            b = nvflash_cmd_write_raw( a );
            NvOsFree( a );
            VERIFY( b, err_str = "writedeviceraw failed"; goto fail );
            break;
        }
        case NvFlashCommand_GetPartitionTable:
        {
            NvFlashCmdGetPartitionTable *a =
                (NvFlashCmdGetPartitionTable *)arg;
            // Sending dummy to handle the double pointer. This & zero initalizations
            // also serve the purpose to notify that this call is made, when no skip.
            Nv3pPartitionInfo *dummy = NULL;
            a->NumLogicalSectors = 0;
            a->StartLogicalSector = 0;
            a->ReadBctFromSD = NV_FALSE;
            CHECK_BL();
            b = nvflash_cmd_getpartitiontable(a, &dummy);
            NvOsFree( a );
            VERIFY( b, err_str = "get partition table failed"; goto fail );
            break;
        }
        case NvFlashCommand_SetOdmCmd:
        {
            NvFlashCmdSetOdmCmd* a = (NvFlashCmdSetOdmCmd *)arg;
            CHECK_BL();
            b = nvflash_cmd_setodmcmd( a );
            NvOsFree( a );
            VERIFY( b, err_str = "set odm cmd failed"; goto fail );
            e = NvFlashCommandGetOption(NvFlashOption_OdmCmdLimitedPowerMode,
                (void *)&IsLimitedPowerMode);
            VERIFY(e == NvSuccess, goto fail);
            if (IsLimitedPowerMode)
                bCheckStatus = NV_FALSE;

            bSync = NV_TRUE;
            break;
        }
        case NvFlashCommand_NvPrivData:
        {
            NvFlashCmdNvPrivData *a = (NvFlashCmdNvPrivData *)arg;
            CHECK_BL();
            b = nvflash_cmd_setnvinternal(a);
            VERIFY( b, err_str = "setting nvinternal blob failed";
                    goto fail );
            bSync = NV_TRUE;
            break;
        }
        case NvFlashCommand_RunBDKTest:
        {
            NvFlashCmdBDKTest*a = (NvFlashCmdBDKTest *)arg;
            CHECK_BL();
            b = nvflash_cmd_runbdktests(a);
            NvOsFree(a);
            VERIFY(b, err_str = "RunBDKTest cmd failed"; goto fail);
            bCheckStatus = NV_FALSE;
            break;
        }
        case NvFlashCommand_FuseWrite:
        {
            NvFlashCmdFuseWrite *a = (NvFlashCmdFuseWrite *)arg;
            CHECK_BL();
            b = nvflash_fusewrite(a->config_file);
            VERIFY(b, err_str = "FuseWrite cmd failed"; goto fail);
            bSync = NV_TRUE;
                break;
            }
        case NvFlashCommand_Symkeygen:
        {
            NvFlashCmdSymkeygen *a = (NvFlashCmdSymkeygen *)arg;
            CHECK_BL();
            b= nvflash_cmd_symkeygen(a);
            NvOsFree(a);
            VERIFY(b,err_str = "Symkeygen command failed"; goto fail);
            bSync = NV_FALSE;
            break;
        }
        case NvFlashCommand_DFuseBurn:
        {
            NvFlashCmdDFuseBurn *a = (NvFlashCmdDFuseBurn *)arg;
            CHECK_BL();
            b = nvflash_cmd_dfuseburn(a);
            NvOsFree(a);
            VERIFY(b,err_str = "DFuseBurn command failed"; goto fail);
            bSync = NV_FALSE;
            break;
        }
        case NvFlashCommand_Sync:
        {
            bSync = NV_TRUE;
            bCheckStatus = NV_FALSE;
            break;
        }
#if ENABLE_NVDUMPER
	 case NvFlashCommand_NvtbootRAMDump:
        {
            NvFlashCmdNvtbootRAMDump *a = (NvFlashCmdNvtbootRAMDump *)arg;
            b = nvflash_cmd_nvtbootramdump( a );
            NvOsFree( a );
            VERIFY( b, err_str = "nvtbootramdump failed"; goto fail );
            break;
        }
#endif
        case NvFlashCommand_SkipSync:
        {
            Nv3pCommand cmd;

            cmd = Nv3pCommand_SkipSync;

            CHECK_BL();

            NV_CHECK_ERROR_CLEANUP(
                Nv3pCommandSend(s_hSock, cmd, 0, 0)
            );

            bSync = NV_FALSE;
            break;
        }
        case NvFlashCommand_Reset:
        {
            NvFlashCmdReset *a = (NvFlashCmdReset *)arg;
            ResetArg.reset = a->reset;
            ResetArg.delay_ms = a->delay_ms;
            Reset = NV_TRUE;
            NvOsFree(a);
            bCheckStatus = NV_FALSE;
            break;
        }
        case NvFlashCommand_ReadNctItem:
        {
            NvFlashCmdReadWriteNctItem *a = (NvFlashCmdReadWriteNctItem *)arg;
            CHECK_BL();
            b = nvflash_cmd_readnctitem(a);
            NvOsFree(a);
            VERIFY(b,err_str = "ReadNctItem command failed"; goto fail);
            break;
        }
        case NvFlashCommand_WriteNctItem:
        {
            NvFlashCmdReadWriteNctItem *a = (NvFlashCmdReadWriteNctItem *)arg;
            CHECK_BL();
            b = nvflash_cmd_writenctitem(a);
            NvOsFree(a);
            VERIFY(b,err_str = "WriteNctItem command failed"; goto fail);
            break;
        }
        default:
            err_str = "unknown command";
            goto clean;
        }


        /* bootloader will send a status after every command. */
        if( bCheckStatus )
        {
            b = nvflash_wait_status();
            VERIFY( b, err_str = "bootloader error"; goto fail );
        }

        if( cmd == NvFlashCommand_Go )
        {
            b = NV_TRUE;
            break;
        }

        nCmds--;
    }
    // for execution of updatebct with bctsections other than BLINFO
    // BLINFO bctsection only supported with download command
    if (pBctSection)
    {
        if (NvFlashIStrcmp(pBctSection, "BLINFO"))
        {
            VERIFY(bct_filename,
                err_str = "bct file required for this command";
                    goto fail);
            VERIFY(!IsBctSet,
                err_str = "updatebct and setbct are not suported at same time";
                    goto fail);
            CHECK_BL();
            b = nvflash_cmd_updatebct(bct_filename, pBctSection);
            bSync = NV_TRUE;
            VERIFY(b, err_str = "updatebct failed"; goto fail);
            b = nvflash_wait_status();
            VERIFY(b, goto fail);
        }
        else if (!IsDownloadCommand)
        {
            b = NV_FALSE;
            err_str = "BLINFO Section only supported with download command";
            goto fail;
        }
    }

    /* about to exit, should send a sync command for last-chance error
     * handling/reporting.
     */
    if( bSync )
    {
        CHECK_BL();
        b = nvflash_cmd_sync();
        VERIFY( b, err_str = "sync failed"; goto fail );

        b = nvflash_wait_status();
        VERIFY( b, goto fail );
    }

    e = NvFlashCommandGetOption( NvFlashOption_VerifyEnabled, (void *)&verifyPartitionEnabled);
    VERIFY( e == NvSuccess, goto fail );
    if (verifyPartitionEnabled)
    {
        b = nvflash_verify_partitions();
        VERIFY( b, err_str = "verify partitions failed"; goto fail );
    }

    // Reset should be at the end
    if( Reset )
    {
        CHECK_BL();
        b = nvflash_cmd_reset(&ResetArg);
        VERIFY(b,err_str = "Reset command failed"; goto fail);
    }
    b = NV_TRUE;
    goto clean;

fail:

    b = NV_FALSE;
    if( err_str )
    {
        Nv3pNackCode code = Nv3pGetLastNackCode(s_hSock);

        NvAuPrintf( "command failure/warning: %s ", err_str );

        if (code != Nv3pNackCode_Success)
        {
            NvAuPrintf( "(%s)\n", nvflash_get_nack(code) );

            /* should be a status message from the bootloader */
           (void)nvflash_wait_status();
        }

        NvAuPrintf( "\n" );

    }

clean:

    return b;
}

int
main( int argc, const char *argv[] )
{
    NvBool b;
    NvError e;
    int ret = 0;
    const char *err_str = 0;
#ifndef NVHOST_QUERY_DTB
    Nv3pTransportMode TransportMode = Nv3pTransportMode_default;
    NvOsSystemTime Start;
    NvOsSystemTime End;
#endif

#ifdef NVHOST_QUERY_DTB
    e = NvFlashCommandParse( argc, argv );
    VERIFY( e == NvSuccess, err_str = NvFlashCommandGetLastError();
    goto fail );
#else
    NvAuPrintf("Nvflash %s started\n", NVFLASH_VERSION);

    e = NvOsGetSystemTime(&Start);
    VERIFY(e == NvSuccess, err_str = "Getting system time failed";
        goto fail);

    if( argc < 2 )
    {
        nvflash_usage();
        return 0;
    }
    if( argc == 2 )
    {
        if( NvOsStrcmp( argv[1], "--help" ) == 0 || NvOsStrcmp( argv[1], "-h" ) == 0 )
        {
            nvflash_usage();
            return 0;
        }
    }
    if( argc == 3 )
    {
        if( NvOsStrcmp( argv[1], "--cmdhelp" ) == 0 || NvOsStrcmp( argv[1], "-ch" ) == 0 )
        {
            NvFlashCmdHelp a;
            a.cmdname = argv[2];
            nvflash_cmd_usage(&a);
            return 0;
        }
    }
    if( NvOsStrcmp( argv[1], "--resume" ) == 0 ||
        NvOsStrcmp( argv[1], "-r" ) == 0 )
    {
        NvAuPrintf( "[resume mode]\n" );
        s_Resume = NV_TRUE;
    }

    NvFlashVerifyListInitLstPartitions();

    /* parse commandline */
    e = NvFlashCommandParse( argc, argv );

    VERIFY( e == NvSuccess, err_str = NvFlashCommandGetLastError();
        goto fail );

    if (!NvOsStrcmp(argv[1], "--buildbct") ||
        (argc > 3 && !NvOsStrcmp(argv[3], "--buildbct") &&
         !NvOsStrcmp( argv[1], "--deviceid")))
    {
        nvflash_buildbct();
        return 0;
    }

    /*checking nvflash and nvsbktool version compatibility*/
    b = nvflash_check_compatibility();
    if(!b)
        goto fail;

    NvFlashCommandGetOption(NvFlashOption_TransportMode,(void*)&TransportMode);

    if(TransportMode == Nv3pTransportMode_Sema)
    {
        NvU32 EmulDevId = 0;
        NvU32 SimulationOpMode = 0;
        NvFlashCommandGetOption(NvFlashOption_EmulationDevId,(void*)&EmulDevId);
        nvflash_set_devid(EmulDevId);

        //check the opmode for simulation
        NvFlashCommandGetOption(NvFlashOption_DeviceOpMode,
           (void*)&SimulationOpMode);
        if (SimulationOpMode)
            nvflash_set_opmode(SimulationOpMode);

        e = nvflash_start_pseudoserver();
        if (e !=  NvSuccess)
        {
            NvAuPrintf("\r\n simulation mode not supported \r\n");
            goto fail;
        }
    }
#endif
    /* execute commands */
    b = nvflash_dispatch();
    if( !b )
    {
        goto fail;
    }
#ifndef NVHOST_QUERY_DTB
    e = NvOsGetSystemTime(&End);
    VERIFY(e == NvSuccess, err_str = "Getting system time failed";
        goto fail);
    NvAuPrintf("Time taken for flashing %d Secs\n",
        (End.Seconds - Start.Seconds));
#endif
    ret = 0;
    goto clean;

fail:
    ret = -1;

clean:
#ifndef NVHOST_QUERY_DTB
    if(TransportMode == Nv3pTransportMode_Sema)
        nvflash_exit_pseudoserver();

    /* Free all devices, partitions, and the strings within. */
    {
        NvFlashDevice*      d;
        NvFlashDevice*      next_d;
        NvFlashPartition*   p;
        NvFlashPartition*   next_p;
        NvU32               i;
        NvU32               j;

        for (i = 0, d = s_Devices; i < s_nDevices; i++, d = next_d)
        {
            next_d = d->next;

            for (j = 0, p = d->Partitions; j < d->nPartitions; j++, p = next_p)
            {
                next_p = p->next;
#ifdef NV_EMBEDDED_BUILD
                if (p->Dirname)
                    NvOsFree(p->Dirname);
                if (p->ImagePath)
                    NvOsFree(p->ImagePath);
                if (p->OS_Args)
                    NvOsFree(p->OS_Args);
                if (p->RamDiskPath)
                    NvOsFree(p->RamDiskPath);
#endif
                if (p->Filename)
                    NvOsFree(p->Filename);
                if (p->Name)
                    NvOsFree(p->Name);

                NvOsFree(p);
            }

            NvOsFree(d);
        }
    }

    NvFlashVerifyListDeInitLstPartitions();
    // Free the partition layout buffer retrieved in nvflash_get_partlayout
    if (s_pPartitionInfo)
        NvOsFree(s_pPartitionInfo);
#endif
    if( err_str )
    {
        NvAuPrintf( "%s\n", err_str );
    }
    Nv3pClose( s_hSock );
    return ret;
}

#ifndef NVHOST_QUERY_DTB
NvBool
nvflash_check_verify_option_args(void)
{

    NvError e = NvSuccess;
    NvU32 PartitionId = 0xFFFFFFFF;
    char Partition[NV_FLASH_PARTITION_NAME_LENGTH];
    NvFlashDevice *d;
    NvFlashPartition *p;
    NvU32 i, j;
    NvBool PartitionFound = NV_FALSE;
    NvBool SeekLstStart = NV_TRUE;

    while(e == NvSuccess)
    {
        e = NvFlashVerifyListGetNextPartition(Partition, SeekLstStart);
        PartitionId = NvUStrtoul(Partition, 0, 0);
        SeekLstStart = NV_FALSE;
        // Thsi condition occurs if there are no elements in the verify
        // partitions list or when the end of list is reached.
        if (e !=  NvSuccess)
        {
            break;
        }
        // If all partitions are marked for verification,
        // send verify command for each partition.

        if(PartitionId != 0xFFFFFFFF)
        {
            if(s_Devices && s_nDevices)
            {
                d = s_Devices;
                //do this for every partition having data
                for( i = 0; i < s_nDevices; i++, d = d->next )
                {
                    p = d->Partitions;
                    for( j = 0; j < d->nPartitions; j++, p = p->next )
                    {
                        if (PartitionId == p->Id || !NvOsStrcmp(Partition, p->Name))
                        {
                            PartitionFound = NV_TRUE;
                            if (!p->Filename)
                            {
                                NvAuPrintf("ERROR: no data to verify for partition %s!!\n",
                                    p->Name);
                                goto fail;
                            }
                        }
                    }
                }
                if (!PartitionFound)
                {
                    NvAuPrintf("ERROR: Partition %s does not exist.\n",
                               Partition);
                    goto fail;
                }
            }
            else
            {
                NvAuPrintf("ERROR: Device not known.\n");
                goto fail;
            }
        }
    }

    return NV_TRUE;

fail:
    NvAuPrintf("ERROR: Verify partition arguments check failed: %s\n",
                Partition);
    return NV_FALSE;
}
#endif

NvBool
nvflash_enable_verify_partition( NvU32 PartitionID )
{
    Nv3pCommand cmd = 0;
    Nv3pCmdVerifyPartition arg;
    NvBool verifyPartition = NV_FALSE;
    NvBool b= NV_FALSE;
    NvError e = NvSuccess;
    const char *PartitionName = NULL;
    const char *err_str = 0;

    // Check if partition exists in the verify list.
    PartitionName = nvflash_get_partNameById(PartitionID, NV_FALSE);
    VERIFY(PartitionName, err_str = "partition not found in partition table";
           goto fail);
    // Passing both PartName and PartitionId to find in Verifiy list. This is because
    // it is not known whether the name or the Id was used to mark the partition for
    // verification.
    verifyPartition = (NvFlashVerifyListIsPartitionToVerify(PartitionID, PartitionName)
                       == NvSuccess);

    // Enable verification of partition as appropriate.
    if (verifyPartition)
    {
        NvAuPrintf("Enabling verification for partition %s...\n", PartitionName);
        // Send command and break
        cmd = Nv3pCommand_VerifyPartitionEnable;
        arg.Id = PartitionID;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
        );
        DEBUG_VERIFICATION_PRINT(("Enabled verification...\n"));
        b = nvflash_wait_status();
        VERIFY( b, goto fail );
    }
    else
    {
        DEBUG_VERIFICATION_PRINT(("WARNING: Partition %s not marked for "
                                  "verification...\n", PartitionName));
    }

    return NV_TRUE;

fail:
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;
}

NvBool
nvflash_send_verify_command(NvU32 PartitionId)
{

    Nv3pCommand cmd;
    Nv3pCmdVerifyPartition arg;
    NvBool b= NV_FALSE;
    NvError e = NvSuccess;
    const char *PartitionName = NULL;
    const char *err_str = 0;

    cmd = Nv3pCommand_VerifyPartition;
    arg.Id = PartitionId;

    PartitionName = nvflash_get_partNameById(PartitionId, NV_FALSE);
    VERIFY(PartitionName, err_str = "partition not found in partition table";
           goto fail);
    NvAuPrintf("Verifying partition %s...Please wait!!\n", PartitionName);

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, &arg, 0 )
    );

    b = nvflash_wait_status();
    VERIFY( b, goto fail );

    NvAuPrintf("Verification successful!!\n",PartitionId);
    return NV_TRUE;

fail:
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    if(e != NvSuccess)
        NvAuPrintf("failed executing command %d NvError 0x%x\n",cmd,e);
    return NV_FALSE;

}

NvBool
nvflash_verify_partitions(void)
{

    NvBool b= NV_FALSE;
    NvError e = NvSuccess;
    Nv3pCommand cmd;
    NvU32 PartitionId = 0xFFFFFFFF;
    char Partition[NV_FLASH_PARTITION_NAME_LENGTH];
    NvFlashDevice *d;
    NvFlashPartition *p;
    NvU32 i, j;
    NvBool PartitionFound = NV_FALSE;
    NvBool SeekLstStart = NV_TRUE;

    do
    {
        e = NvFlashVerifyListGetNextPartition(Partition, SeekLstStart);
        PartitionId = NvUStrtoul(Partition, 0, 0);
        SeekLstStart = NV_FALSE;
        if(e != NvSuccess)
        {
            break;
        }
        // If all partitions are marked for verification,
        // send verify command for each partition.

        // Verification should not assume that the partition
        // index specified is valid and that it has data to verify
        // In any of these conditions, throw an error.
        if(PartitionId == 0xFFFFFFFF)
        {
            d = s_Devices;
            //do this for every partition having data
            for( i = 0; i < s_nDevices; i++, d = d->next )
            {
                    p = d->Partitions;
                    for( j = 0; j < d->nPartitions; j++, p = p->next )
                    {
                        if (p->Filename)
                        {
                            PartitionId = p->Id;
                            b = nvflash_send_verify_command(PartitionId);
                            VERIFY( b, goto fail );
                        }
                    }
                }
            break;
        }
        else
        {
            d = s_Devices;
            //do this for every partition having data
            for( i = 0; i < s_nDevices; i++, d = d->next )
            {
                p = d->Partitions;
                for( j = 0; j < d->nPartitions; j++, p = p->next )
                {
                    if (PartitionId == p->Id || !NvOsStrcmp(Partition, p->Name))
                    {
                        PartitionFound = NV_TRUE;
                        if (p->Filename)
                        {
                            PartitionId = p->Id;
                            b = nvflash_send_verify_command(PartitionId);
                            VERIFY( b, goto fail );
                        }
                        else
                        {
                            NvAuPrintf("ERROR: no data to verify for partition %s!!\n",
                                p->Name);
                            goto fail;
                        }
                    }
                }
            }
            if (!PartitionFound)
            {
                NvAuPrintf("ERROR: Partition %s does not exist.\n", Partition);
                goto fail;
            }
        }
    }while(1);

    // Indicate to device that verification of partitions is over.
    cmd = Nv3pCommand_EndVerifyPartition;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend( s_hSock, cmd, 0, 0 )
    );

    NvAuPrintf("VERIFICATION COMPLETE....\n");

    return NV_TRUE;

fail:
    NvAuPrintf("ERROR: Verification failed for partition %s\n",
                Partition);
    return NV_FALSE;
}

NvBool
nvflash_get_partlayout(NvBool ReadBctAtDevice)
{
    const char *err_str = 0;
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvFlashCmdGetPartitionTable *a = NULL;

    // Get the partition table from the device
    a = NvOsAlloc(sizeof(NvFlashCmdGetPartitionTable));
    if (!a)
    {
        err_str = "Insufficient system memory";
        b = NV_FALSE;
        goto fail;
    }

    NvOsMemset(a, 0, sizeof(NvFlashCmdGetPartitionTable));
    a->ReadBctFromSD = ReadBctAtDevice;
    b = nvflash_cmd_getpartitiontable(a, &s_pPartitionInfo);
    VERIFY(b, err_str = "Unable to retrieve Partition table from device"; goto fail);
    b = nvflash_wait_status();
    VERIFY(b, goto fail);

fail:
    if (a)
        NvOsFree(a);
    if (err_str)
        NvAuPrintf("%s NvError 0x%x\n", err_str , e);
    return b;
}

NvU32
nvflash_get_partIdByName(const char *pPartitionName, NvBool ReadBctAtDevice)
{
    const char *err_str = 0;
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvU32 Index = 0;

    if (!s_pPartitionInfo)
    {
        b = nvflash_get_partlayout(ReadBctAtDevice);
        VERIFY(b, goto fail);
    }

    while(s_pPartitionInfo[Index].PartId)
    {
        if (!NvOsStrcmp((char*)s_pPartitionInfo[Index].PartName, pPartitionName))
            return s_pPartitionInfo[Index].PartId;
        Index++;
    };

fail:
    if (err_str)
        NvAuPrintf("%s NvError 0x%x\n", err_str , e);
    return 0;
}

char*
nvflash_get_partNameById(NvU32 PartitionId, NvBool ReadBctAtDevice)
{
    const char *err_str = 0;
    NvError e = NvSuccess;
    NvBool b = NV_TRUE;
    NvU32 Index = 0;

    if (!s_pPartitionInfo)
    {
        b = nvflash_get_partlayout(ReadBctAtDevice);
        VERIFY(b, goto fail);
    }

    while(s_pPartitionInfo[Index].PartName)
    {
        if (s_pPartitionInfo[Index].PartId == PartitionId)
            return (char*) s_pPartitionInfo[Index].PartName;
        Index++;
    };

fail:
    if (err_str)
        NvAuPrintf("%s NvError 0x%x\n", err_str , e);
    return NULL;
}

#ifdef NVHOST_QUERY_DTB
NvError nvgetdtb_getdata(NvU8 *BoardInfo, NvU32 Size, NvBool *ListBoardId)
{
    Nv3pCommand cmd = Nv3pCommand_ReadBoardInfo;
    NvBool b = NV_TRUE;
    NvError e = NvSuccess;
    char *pErrStr = NULL;

    e = Nv3pCommandSend(s_hSock, cmd, 0 , 0);
    VERIFY(e == NvSuccess, pErrStr = "unable to read BoardInfo"; goto fail;)

    e = Nv3pDataReceive(
            s_hSock, BoardInfo, Size, 0, 0);
    VERIFY(e == NvSuccess, pErrStr = "data receive failure"; goto fail);

    e = NvFlashCommandGetOption(
            NvFlashOption_ListBoardId,(void **)ListBoardId);
    VERIFY(e == NvSuccess, pErrStr = "ReadBoardId option failure"; goto fail);

    b = nvflash_wait_status();
    VERIFY(b, pErrStr = "unable to read board info"; goto fail);
fail:
    if (!b)
        e = NvError_UsbfTxfrFail;
    if (pErrStr)
        NvAuPrintf("%s NvError 0x%x\n", pErrStr, e);
    return e;
}

NvError nvgetdtb_forcerecovery(void)
{
    Nv3pCommand cmd = Nv3pCommand_Recovery;
    NvBool b = NV_TRUE;
    NvError e = NvSuccess;
    char *pErrStr = NULL;

    e = Nv3pCommandSend(s_hSock, cmd, 0, 0);
    VERIFY(e == NvSuccess, pErrStr = "unable to set force recovery"; goto fail);

    b = nvflash_wait_status();
    VERIFY(b, pErrStr = "unable to set recovery"; goto fail);

    // Required for setting force recovery
    NvOsSleepMS(750);
fail:
    if (!b)
        e = NvError_UsbfTxfrFail;
    if (pErrStr)
        NvAuPrintf("%s NvError 0x%x\n", pErrStr, e);
    return e;
}
#endif
