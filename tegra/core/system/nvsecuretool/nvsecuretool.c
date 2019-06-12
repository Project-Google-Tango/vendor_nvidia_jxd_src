/*
 * Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvsecuretool.h"
#include "nvsecure_bctupdate.h"
#include "nvddk_fuse.h"
#include "nv_rsa.h"
#include "nv_cryptolib.h"
#include "nvflash_parser.h"
#include "nvflash_configfile.h"
#include "nvflash_util.h"
#include "nvsecure_bctupdate.h"
#include "nvboothost_t12x.h"
#include "nvboothost_t1xx.h"
#include "nvbuildbct.h"
#include "nvtest.h"

#define RSA_KEY_SIZE  256
#define NVSECURETOOL_VERSION NVSBKTOOL_VERSION
#define DSESSION_PUBLIC_KEY  "spub.txt"
#define DSESSION_PRIVATE_KEY "spriv.txt"
#define PKC_FILE_APPEND      "_signed"
#define SBK_FILE_APPEND      "_encrypt"
#define ZEROSBK_FILE_APPEND  "_hash"
#define BCT_EXTENSION        ".bct"
#define CFG_EXTENSION        ".cfg"
#define OEM_FIELD_SIZE       32

NvU32 g_OptionSbk[4] = {0};
static const char *s_BlobOut             = "blob.bin";
static const char *s_OptionBootLoaderIn  = NULL;
static const char *s_OptionBootLoaderOut = NULL;
static const char *s_OptionBctIn         = NULL;
static const char *s_OptionBctOut        = NULL;
static const char *s_OptionCfgFile       = NULL;
static const char *s_KeyFileIn           = NULL;
static const char *s_FuseConfigIn        = NULL;
static const char *s_DsessionKeyFile     = NULL;
static const char *s_RsaKeyFileIn        = NULL;
static const char *s_PubKeyFile          = DSESSION_PUBLIC_KEY;
static const char *s_PrivKeyFile         = DSESSION_PRIVATE_KEY;
static const char *s_PubHashFile         = "pub.sha";
static const char *s_Oemfile             = NULL;
static const char *s_tidfile             = NULL;
static NvBctHandle s_BctHandle    = NULL;
static NvOsFileHandle s_BlobFile  = NULL;
NvU32 g_OptionChipId              = 0;
NvU32 g_BlockSize                 = 128;
NvSecureMode g_SecureMode         = 0;
NvSecureBctInterface g_BctInterface;
static const char *s_NvTestFilename = NULL;
static const char *s_AppletFile = NULL;
NvBool g_Encrypt = NV_TRUE;

typedef NvError (*pNvSecureHostRcmCreateMsgFromBuffer) (
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    NvU8 *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength,
    NvU8 *PayloadBuffer,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk,
    NvU8 **pMsgBuffer);

typedef NvU32 (*pNvBootHostRcmGetMsgLength) (NvU8 *MsgBuffer);

/* Updates the blob with bootloader and microbootloader
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux is ignored
 *
 * Returns NvSuccess if parsing is successful otherwise NvError
 */
static NvError
NvSecureUpdateBlobWithBootloaders(
        NvFlashPartition *Partition,
        void *Aux);

/* Encrypts/Signs the bootloader
 *
 * @param BlFileIn Bootloader file name
 * @param PageSize Bootloader will be aligned to this value
 * @param BlBuffer Buffer containing encrypted bootloader
 * @param BlSize Size of bootloader buffer
 * @param HashBuffer Buffer containing Signature/Hash of bootoader
 * @param HashSize Size of HashBuffer
 *
 * Returns NvSuccess if no errors.
 */
NvError
NvSecureSignEncryptBootloader(
        const char *BlFileIn,
        NvU32 PageSize,
        NvU8 **BlBuffer,
        NvU32 *BlSize,
        NvU8 **HashBuffer,
        NvU32 *HashSize);

/* Performs the operations needed for secure flow for fuse burning
 *
 * Returns NvSuccess if no errors.
 */
static NvError
NvSecureDsession(void);

/* Parses the fuse config file and updates the blob with encrypted
 * fuse data.
 *
 * Returns NvSuccess if no errors.
 */
static NvError
NvSecureFillBlobWithFuseData(void);

/* Definition in nvflash lib.
 * String case insensitive comparison.
 */
extern NvS32 NvFlashIStrcmp(const char *pString1, const char *pString2);

/* miniloader applets */
static NvU8 s_MiniLoader_t30[] =
{
    #include "nvflash_miniloader_t30.h"
};

static NvU8 s_MiniLoader_t11x[] =
{
    #include "nvflash_miniloader_t11x.h"
};

static NvU8 s_MiniLoader_t12x[] =
{
#ifdef NVTBOOT_EXISTS
    #include "nvtboot_t124.h"
#else
    #include "nvflash_miniloader_t12x.h"
#endif
};

void
NvSecureTool_Usage(void)
{
    NvAuPrintf(
        "nvsecuretool usage:\n\n"
        "nvsecuretool --sbk 0xXXXXXXXX 0xXXXXXXXX 0xXXXXXXXX 0xXXXXXXXX <options>\n"
        "\tUsed to create encrypted and signed bootloader, microboot,\n"
        "\tminiloader(along with RCM messages) and bct\n"

        "OR\n"

        "nvsecuretool --pkc keyfile <options>\n"
        "\tUsed to create signed BCT with bootloader, microboot signature\n"
        "\tminiloader(along with RCM messages) and bct\n"
        "\n\nUse --help or -h for command details\n\n"
    );
}

void
NvSecureTool_Help(void)
{
    NvAuPrintf(
        "\nFor SBK mode:\n"
        "  --sbk <0xXXXXXXXX 0xXXXXXXXX 0xXXXXXXXX 0xXXXXXXXX>\n"
        "\tMandatory, keys(0xXX..) are used to encrypt and sign images. \n"
        "\tNote that the sbk key should be in hex format i.e. 0xXXXXXXXX\n"
        "  --chip <N>\n"
        "\tMandatory, used to encrypt appropriate miniloader based on chip id\n"
        "\twhere N {0x35, 0x40}\n"
        "  --blob <file>\n"
        "\tOptional, used to store blob containing RCM messages, bl Hash etc.\n"
        "\tin this file\n"
        "  --bct <infile> [outfile]\n"
        "\tMandatory, bct file(infile) is encrypted, signed and saved in specified\n"
        "\toutfile otherwise in flash_encrypt.bct\n"
        "  --bl <infile> [outfile]\n"
        "\tMandatory, command line bootloader file(infile) is encrypted, signed\n"
        "\tand saved in specified outfile otherwise in bootloader_encrypt.bin\n"
        "  --cfg <infile>\n"
        "\tMandatory, cfg file containing partition layout used for flashing\n"
        "\tTool wil extract bootloaders (including microboot) from input cfg,\n"
        "\tencrypt using provided key and save them as\n"
        "\t<filename without extension>_encrypt.<filename extension>\n"
        "  --oemfile <file>\n"
        "\tOptional, used to pass oem data\n"
        "  --blocksize <N>\n"
        "\tOptional, blocksize in terms of number of sectors\n"
        "  --applet <file>\n"
        "\tOptional, used to pass the applet to be downloaded by bootrom\n"
        "\n\n"
        "For PKC mode:\n"
        "  --pkc keyfile \n"
        "\tMandatory, key file in polarssl/openssl format for generating RSA key pairs\n"
        "  --chip <N>\n"
        "\tMandatory, used to encrypt appropriate miniloader based on chip id\n"
        "\twhere N {0x35, 0x40}\n"
        "  --blob <file>\n"
        "\tOptional, used to store blob containing RCM messages, bl Hash etc.\n"
        "\tin this file\n"
        "  --bct <infile> [outfile]\n"
        "\tMandatory, bct file(infile) is signed and saved into outfile specified\n"
        "\totherwise in flash_signed.bct\n"
        "  --bl <infile> [outfile]\n"
        "\tMandatory, command line bootloader file(infile), warmboot will be signed,\n"
        "\thash of this warmboot signed bootloader will be saved in blob\n"
        "  --cfg <infile>\n"
        "\tMandatory, input cfg file containing partition layout used while flashing.\n"
        "\tTool wil extract bootloaders from input cfg, sign warmboot and save them in\n"
        "\t<filename without extension>_signed.<filename extension>\n"
        "  --oemfile <file>\n"
        "\tOptional, used to pass oem data\n"
        "  --blocksize <N>\n"
        "\tOptional, blocksize in terms of number of sectors\n"
        "  --nvtest <file>\n"
        "\tOptional, used to sign nvtest file\n"
        "  --applet <file>\n"
        "\tOptional, used to pass the applet to be downloaded by bootrom\n"
        "\n"
    );
}

NvU32
nvflash_get_devid(void)
{
    return g_OptionChipId;
}

NvBool
NvSecureCheckValidKeyInput(const char *arg)
{
    NvU32 i;
    NvU32 Len;

    Len = NvOsStrlen(arg);
    if (Len < 3 || Len > 10)
        return NV_FALSE;

    if (arg[0] != '0' || (arg[1] != 'x' && arg[1] != 'X'))
        return NV_FALSE;

    for (i = 2; i < Len; i++)
    {
       if ((arg[i] >= '0' && arg[i] <= '9') ||
           (arg[i] >= 'A' && arg[i] <= 'F') ||
           (arg[i] >= 'a' && arg[i] <= 'f'))
       {
           continue;
       }
       else
       {
           return NV_FALSE;
       }
    }

    return NV_TRUE;
}

NvBool
NvSecureParseCommandLine(int argc, const char *argv[])
{
    int i    = 0;
    NvBool b = NV_TRUE;
    const char *err_str = NULL;
    const char *arg     = NULL;
    static NvBool ZeroSBK = NV_TRUE;

    for (i = 1; i < argc; i++)
    {
        arg = argv[i];
        if (NvOsStrcmp(arg, "--sbk") == 0)
        {
            NvU32 j;
            NvU32 tmp;
            VERIFY((i + 4) < argc,
                    err_str = "--sbk argument missing"; goto fail);

            for (j = 0; j < 4; j++)
            {
                i++;
                /* Check input key format */
                b = NvSecureCheckValidKeyInput(argv[i]);
                if (b == NV_FALSE)
                {
                    NvAuPrintf("\n--sbk wrong input argument: %s\n", argv[i]);
                    NvSecureTool_Usage();
                    goto fail;
                }

                tmp = NvUStrtoul(argv[i], 0, 0);
                /* need to reverse the byte order */
                g_OptionSbk[j] = (tmp & 0xff) << 24 |
                                 ((tmp >> 8) & 0xff) << 16 |
                                 ((tmp >> 16) & 0xff) << 8 |
                                 ((tmp >> 24) & 0xff);
               if (g_OptionSbk[j] != 0)
                   ZeroSBK = NV_FALSE;
            }
            if (ZeroSBK)
            {
                g_Encrypt = NV_FALSE;
                NvAuPrintf("Using Zero SBK key\n");
            }
            g_SecureMode = NvSecure_Sbk;
        }
        else if (NvOsStrcmp(arg, "--pkc") == 0)
        {
            i++;
            /* Check input key format */
            s_KeyFileIn = argv[i];
            g_SecureMode = NvSecure_Pkc;
        }
        else if (NvOsStrcmp(arg, "--chip") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--chip argument missing"; goto fail);
            i++;
            g_OptionChipId = NvUStrtoul(argv[i], 0, 0);
        }
        else if (NvOsStrcmp(arg, "--nvtest") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--nvtest argument missing"; goto fail);
            i++;
            s_NvTestFilename = argv[i];
        }
        else if (NvOsStrcmp(arg, "--blob") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--blob argument missing"; goto fail);
            i++;
            s_BlobOut = argv[i];
        }
        else if (NvOsStrcmp(arg, "--bct") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--bct argument missing"; goto fail);
            i++;
            s_OptionBctIn = argv[i];

            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                i++;
                s_OptionBctOut = argv[i];
            }
        }
        else if (NvOsStrcmp(arg, "--bl") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--bl argument missing"; goto fail);
            i++;
            s_OptionBootLoaderIn = argv[i];
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                i++;
                s_OptionBootLoaderOut = argv[i];
            }
        }
        else if (NvOsStrcmp(arg, "--cfg") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--cfg argument missing"; goto fail);
            i++;
            s_OptionCfgFile= argv[i];
        }
        else if (NvOsStrcmp(arg, "--keygen") == 0)
        {
            VERIFY((i + 1) < argc,
                   err_str = "--keygen argument missing"; goto fail);
            i++;
            s_RsaKeyFileIn = argv[i];
        }
        else if (NvOsStrcmp(arg, "--dsession") == 0)
        {
             VERIFY((i + 1) < argc,
                    err_str = "--dsession argument missing"; goto fail);
            i++;
            s_DsessionKeyFile = argv[i];
        }
        else if (NvOsStrcmp(arg, "--fuse_cfg") == 0)
        {
             VERIFY((i + 1) < argc,
                    err_str = "--fuse_cfg argument missing"; goto fail);
            i++;
            s_FuseConfigIn = argv[i];
        }
        else if (NvOsStrcmp(arg, "--keyfile") == 0)
        {
             VERIFY((i + 1) < argc,
                    err_str = "--keyfile argument missing"; goto fail);
            i++;
            s_PubKeyFile = argv[i];
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                i++;
                s_PrivKeyFile = argv[i];
            }
        }
        else if (NvOsStrcmp(arg, "--oemfile") == 0)
        {
            VERIFY((i + 1) < argc,
                err_str = "--oemfile argument missing"; goto fail);
            i++;
            s_Oemfile = argv[i];
        }
        else if (NvOsStrcmp(arg, "--tidfile") == 0)
        {
            VERIFY((i + 1) < argc,
                err_str = "--tidfile argument missing"; goto fail);
            i++;
            s_tidfile = argv[i];
        }
        else if (NvOsStrcmp(arg, "--blocksize") == 0)
        {
            VERIFY((i + 1) < argc,
                err_str = "--blocksize argument missing"; goto fail);
            i++;
            g_BlockSize = NvUStrtoul(argv[i], 0, 0);
        }
        else if (NvOsStrcmp(arg, "--applet") == 0)
        {
            VERIFY((i + 1) < argc,
                err_str = "--applet argument missing"; goto fail);
            i++;
            s_AppletFile = argv[i];
        }
        else
        {
            NvAuPrintf("\nunknown command %s\n", arg);
            NvSecureTool_Usage();
            goto fail;
        }
    }

    /* If --keygen is used then no need to validate other options. */
    if (s_RsaKeyFileIn != NULL)
        goto clean;

    /* Throw error if compulsory options are not used */
    VERIFY(g_SecureMode != 0,
           err_str = "--sbk  or --pkc option needed"; goto fail);
    VERIFY(g_OptionChipId != 0,
           err_str = "--Chip id option needed"; goto fail);
    // NvTest signing only requires chip and secure mode setting
    if (s_NvTestFilename != NULL)
    {
        goto clean;
    }
    VERIFY(s_OptionCfgFile != NULL,
           err_str = "--cfg file is required"; goto fail);
    VERIFY(s_OptionBctIn != NULL,
           err_str = "--bct option needed"; goto fail);
    VERIFY(s_OptionBootLoaderIn != NULL,
           err_str = "--bl option is needed"; goto fail);
    goto clean;

fail:
    b = NV_FALSE;
    if (err_str)
    {
        NvAuPrintf("\n%s\n", err_str);
        NvSecureTool_Usage();
    }
clean:
    return b;
}

NvError
NvSecureUpdateBlob(NvU32 HeaderType, NvU32 HeaderLength, const NvU8 *BlobBuf)
{
    NvError e     = NvSuccess;
    char *err_str = NULL;
    NvU32 flags   = 0;
    NvFlashBlobHeader header;

    if (s_BlobFile == NULL)
    {
        flags = NVOS_OPEN_CREATE | NVOS_OPEN_WRITE;
        e = NvOsFopen(s_BlobOut, flags, &s_BlobFile);
        VERIFY(e == NvSuccess, err_str = "Blob file open failed"; goto fail);
    }

    header.BlobHeaderType = HeaderType;
    header.length    = HeaderLength;
    header.reserved1 = 0;
    header.reserved2 = 0;
    NvOsMemset(header.resbuff, 0x0, NV3P_AES_HASH_BLOCK_LEN);

    e = NvOsFwrite(s_BlobFile, &header, sizeof(header));
    VERIFY(e == NvSuccess, err_str = "Blob header write failed"; goto fail);

    e = NvOsFwrite(s_BlobFile, BlobBuf, HeaderLength);
    VERIFY(e == NvSuccess, err_str = "Blob buffer write failed"; goto fail);

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    return e;
}

NvError
NvSecurePrepareRcmData(void)
{
    NvError e            = NvSuccess;
    char *err_str        = NULL;
    NvU8* pMiniLoader    = NULL;
    NvU32 MiniLoaderSize = 0;
    NvU8 *RcmMsgBuff     = NULL;
    NvFlashRcmDownloadData Download_arg;
    NvU32 MsgBufferLength;
    NvDdkFuseOperatingMode OperatingMode;
    pNvSecureHostRcmCreateMsgFromBuffer RcmCreateMsg;
    pNvBootHostRcmGetMsgLength RcmGetMsgLength;
    NvOsFileHandle hFile = 0;
    NvOsStatType Stat;
    NvU32 ReadBytes = 0;

    if (g_OptionChipId == 0x30)
    {
        pMiniLoader     = s_MiniLoader_t30;
        MiniLoaderSize  = sizeof(s_MiniLoader_t30);
        RcmCreateMsg    = &NvBootHostRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostRcmGetMsgLength;
    }
    else if (g_OptionChipId == 0x35)
    {
        pMiniLoader     = s_MiniLoader_t11x;
        MiniLoaderSize  = sizeof(s_MiniLoader_t11x);
        RcmCreateMsg    = &NvBootHostT1xxRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostT1xxRcmGetMsgLength;
    }
    else if (g_OptionChipId == 0x40)
    {
        pMiniLoader = s_MiniLoader_t12x;
        MiniLoaderSize = sizeof(s_MiniLoader_t12x);
        RcmCreateMsg    = &NvBootHostT12xRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostT12xRcmGetMsgLength;
    }
    else
    {
        e = NvError_BadParameter;
        VERIFY(0, err_str = "Invalid ChipId\n"; goto fail);
    }

    if (s_AppletFile)
    {
        pMiniLoader = NULL;

        e = NvOsFopen(s_AppletFile, NVOS_OPEN_READ, &hFile);
        VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

        e = NvOsFstat(hFile, &Stat);
        VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);

        MiniLoaderSize = Stat.size;

        pMiniLoader = NvOsAlloc(MiniLoaderSize);

        if (!pMiniLoader)
        {
            e = NvError_InsufficientMemory;
            err_str = "file stat failed";
            goto fail;
        }
        e = NvOsFread(hFile, pMiniLoader,
                        MiniLoaderSize, &ReadBytes);
        VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);
    }
    OperatingMode = NvDdkFuseOperatingMode_OdmProductionSecure;
    if (g_SecureMode == NvSecure_Pkc)
    {
        OperatingMode = NvDdkFuseOperatingMode_OdmProductionSecurePKC;
        RcmCreateMsg  = &NvSecureHostRcmCreateMsgFromBuffer;
    }

    /* download the miniloader to the bootrom */
    Download_arg.EntryPoint = g_BctInterface.NvSecureGetMlAddress();

    e = (*RcmCreateMsg) (NvFlashRcmOpcode_QueryRcmVersion, NULL,
                         (NvU8 *)NULL, 0, 0, NULL, OperatingMode,
                         (NvU8 *)g_OptionSbk, &RcmMsgBuff);

    VERIFY(e == NvSuccess,
        err_str = "Query RCM version message creation failed"; goto fail);

    MsgBufferLength = (*RcmGetMsgLength) (RcmMsgBuff);

    e = NvSecureUpdateBlob(NvFlash_QueryRcmVersion, MsgBufferLength, RcmMsgBuff);
    VERIFY(e == NvSuccess,
        err_str = "Writing Query RCM version to blob failed"; goto fail);

    NvOsFree(RcmMsgBuff);
    RcmMsgBuff = NULL;

    e = (*RcmCreateMsg) (NvFlashRcmOpcode_DownloadExecute, NULL,
                         (NvU8 *)&Download_arg, sizeof(Download_arg),
                         MiniLoaderSize, pMiniLoader, OperatingMode,
                         (NvU8 *)g_OptionSbk, &RcmMsgBuff);

    VERIFY(e == NvSuccess,
        err_str = "Send miniloader message creation failed"; goto fail);

    MsgBufferLength = (*RcmGetMsgLength) (RcmMsgBuff);

    e = NvSecureUpdateBlob(NvFlash_SendMiniloader, MsgBufferLength, RcmMsgBuff);
    VERIFY(e == NvSuccess,
        err_str = "Writing Send Miniloader message to blob failed"; goto fail);
    NvOsFree(RcmMsgBuff);
    RcmMsgBuff = NULL;

    if (g_OptionChipId != 0x30)
    {
        NvU32 Size     = sizeof(NvSecureUniqueId);
        NvU32 Instance = 0;
        NvSecureUniqueId *DevUniqueId = NULL;

        DevUniqueId = NvOsAlloc(Size);
        if (!DevUniqueId)
        {
            e = NvError_InsufficientMemory;
            err_str = "Allocating buffer for unique id failed";
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(s_BctHandle, NvBctDataType_UniqueChipId,
                &Size, &Instance, DevUniqueId)
        );

        e = (*RcmCreateMsg) (NvFlashRcmOpcode_EnableJtag, NULL,
                             (NvU8 *)DevUniqueId, sizeof(DevUniqueId), 0, NULL,
                             OperatingMode, (NvU8 *)g_OptionSbk, &RcmMsgBuff);

        VERIFY(e == NvSuccess,
               err_str = "Enable Jtag message creation failed"; goto fail);

        MsgBufferLength = (*RcmGetMsgLength) (RcmMsgBuff);

        e = NvSecureUpdateBlob(NvFlash_EnableJtag, MsgBufferLength, RcmMsgBuff);
        VERIFY(e == NvSuccess,
            err_str = "Writing jtag enable message to blob failed"; goto fail);

        if (DevUniqueId)
            NvOsFree(DevUniqueId);
    }

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);

    if (RcmMsgBuff)
        NvOsFree(RcmMsgBuff);

    if (hFile)
        NvOsFclose(hFile);

    if (pMiniLoader && s_AppletFile)
        NvOsFree(pMiniLoader);
    return e;
}

NvError
NvSecureCreateBctBuffer(void)
{
    NvError e            = NvSuccess;
    NvU32 BctSize        = 0;
    size_t BytesRead     = 0;
    NvOsFileHandle hFile = NULL;
    char *err_str        = NULL;
    NvU8 *BctBuffer      = NULL;
    NvS32 ExtStartIndex  = 0;
    BuildBctArg BctArgs;

    ExtStartIndex = NvOsStrlen(s_OptionBctIn);
    ExtStartIndex -= NvOsStrlen(CFG_EXTENSION);
    ExtStartIndex = ExtStartIndex > 0 ? ExtStartIndex : 0;

    if(NvFlashIStrcmp(s_OptionBctIn + ExtStartIndex, CFG_EXTENSION) == 0)
    {
        NvOsMemset(&BctArgs, 0, sizeof(BctArgs));
        BctArgs.ChipId  = g_OptionChipId;
        BctArgs.pCfgFile = s_OptionBctIn;
        e = NvBuildBct(&BctArgs);
        VERIFY(e == NvSuccess, err_str = "Building BCT failed"; goto fail);
        BctBuffer = BctArgs.pBctBuffer;
        BctSize = BctArgs.BctSize;
    }
    else
    {
        BctSize = g_BctInterface.NvSecureGetBctSize();

        /* allocate buffer for bct and copy raw bct into it */
        BctBuffer = NvOsAlloc(BctSize);
        VERIFY(BctBuffer, err_str = "Bct buffer allocation failed"; goto fail);

        NvOsMemset(BctBuffer, 0x0, BctSize);
        e = NvOsFopen(s_OptionBctIn, NVOS_OPEN_READ, &hFile);
        VERIFY(e == NvSuccess, err_str = "Bct file open failed"; goto fail);

        e = NvOsFread(hFile, BctBuffer, BctSize, &BytesRead);
        VERIFY(e == NvSuccess, err_str = "Bct file read failed"; goto fail);
    }

    e = NvBctInit(&BctSize, BctBuffer, &s_BctHandle);
    VERIFY(e == NvSuccess, err_str = "Bct init failed"; goto fail);

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (hFile)
        NvOsFclose(hFile);
    return e;
}

NvError
NvSecureComputeBctHash(NvU8 *BctBuf)
{
    NvError e           = NvSuccess;
    const char *err_str = NULL;
    NvU8 *BctHashBuf    = NULL;
    NvU32 HashSize      = 0;
    NvU8 *PubKey        = NULL;
    NvU32 BctSize       = 0;
    NvU32 Size;
    NvU32 Instance;
    NvU32 DataOffset;
    NvBctDataType BctDataType;

    DataOffset = NvSecureGetDataOffset();
    NvAuPrintf("Data offset is %d\n", DataOffset);

    BctSize = g_BctInterface.NvSecureGetBctSize();

    NvSecureSignEncryptBuffer(g_SecureMode, NULL, BctBuf + DataOffset,
            BctSize - DataOffset, &BctHashBuf, &HashSize, NV_FALSE, NV_FALSE);

    Instance = 0;
    if (g_SecureMode == NvSecure_Pkc)
    {
        Size = sizeof(NvBootRsaKeyModulus);
        PubKey = NvOsAlloc(Size);
        VERIFY(PubKey, e = NvError_InsufficientMemory;
               err_str = "Allocating memory for publick Key failed"; goto fail);
        NvOsMemset(PubKey, 0x0, Size);

        NvSecurePkcGetPubKey(PubKey);

        BctDataType = NvBctDataType_RsaPssSig;

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(s_BctHandle, NvBctDataType_RsaKeyModulus,
                         &Size, &Instance, PubKey)
        );
    }
    else
    {
        BctDataType = NvBctDataType_CryptoHash;
    }

    Instance = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_BctHandle, BctDataType,
                     &HashSize, &Instance, BctHashBuf)
    );

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (BctHashBuf)
        NvOsFree(BctHashBuf);
    if (PubKey)
        NvOsFree(PubKey);
    return e;
}

NvError
NvSecureSaveBct(void)
{
    NvError e            = NvSuccess;
    NvOsFileHandle hFile = NULL;
    NvU32 BctSize        = 0;
    const char *err_str  = 0;
    char *BctOutFile     = NULL;

    /* calculate bct crypto Hash and save in it. */
    e = NvSecureComputeBctHash((NvU8 *) s_BctHandle);
    VERIFY(e == NvSuccess, err_str = "Updating bct Hash failed"; goto fail);

    /* write encrypted & signed bct buffer into output bct file
     * after all BLs info and bct crypt Hash are updated in bct
     */
    BctSize = g_BctInterface.NvSecureGetBctSize();

    if (s_OptionBctOut == NULL)
    {
        BctOutFile = NvSecureGetOutFileName(s_OptionBctIn, BCT_EXTENSION);
        if (BctOutFile == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        s_OptionBctOut = BctOutFile;
    }
    e = NvSecureSaveFile(s_OptionBctOut, (NvU8 *) s_BctHandle, BctSize);
    VERIFY(e == NvSuccess, err_str = "Bct out file open failed"; goto fail);
    NvAuPrintf("bct file %s saved as %s\n", s_OptionBctIn, s_OptionBctOut);

fail:
    if (BctOutFile)
        NvOsFree(BctOutFile);
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (hFile)
        NvOsFclose(hFile);
    return e;
}

NvU32
NvSecureGetBctPageSize(void)
{
    NvError e      = NvSuccess;
    NvU32 PageSize = 0;
    NvU32 Size     = sizeof(NvU32);
    NvU32 Instance = 0;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(s_BctHandle, NvBctDataType_BootDevicePageSizeLog2,
                &Size, &Instance, &PageSize)
    );
    PageSize = 1 << PageSize;

fail:
    return PageSize;
}

NvU32
NvSecureGetBctBlockSize(void)
{
    NvError e       = NvSuccess;
    NvU32 BlockSize = 0;
    NvU32 Size     = sizeof(NvU32);
    NvU32 Instance = 0;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(s_BctHandle, NvBctDataType_BootDeviceBlockSizeLog2,
                &Size, &Instance, &BlockSize)
    );
    BlockSize = 1 << BlockSize;

fail:
    return BlockSize;
}

NvU32
NvSecureGetDataOffset(void)
{
    NvError e      = NvSuccess;
    NvU32 Size     = 0;
    NvU32 Instance = 0;
    NvU32 DataOffset;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(s_BctHandle, NvBctDataType_HashDataOffset,
                     &Size, &Instance, &DataOffset)
    );
    return DataOffset;

fail:
    return 0;
}

char*
NvSecureGetOutFileName(const char *FileName, const char *Extension)
{
    char *OutFileName = NULL;
    NvU32 FileLen;
    NvU32 ExtStartIndex;
    NvU32 AppendLen;
    NvU32 ExtLen;
    const char *AppendString = NULL;

    AppendString = (g_SecureMode == NvSecure_Pkc) ?
                   PKC_FILE_APPEND : SBK_FILE_APPEND;

    if (g_SecureMode == NvSecure_Sbk && !g_Encrypt)
        AppendString = ZEROSBK_FILE_APPEND;

    AppendLen     = NvOsStrlen(AppendString);
    FileLen       = NvOsStrlen(FileName);
    ExtStartIndex = FileLen;

    while(FileName[ExtStartIndex] != '.' && ExtStartIndex--);

    ExtLen = FileLen - ExtStartIndex;
    FileLen = ExtStartIndex;

    if (Extension != NULL)
        ExtLen = NvOsStrlen(Extension);
    else
        Extension = FileName + FileLen;

    OutFileName = NvOsAlloc(FileLen + AppendLen + ExtLen + 1);
    if (OutFileName == NULL)
    {
        NvAuPrintf("Error while allocating buffer for filename\n");
        goto fail;
    }

    NvOsMemset(OutFileName, 0x0, FileLen + AppendLen + ExtLen + 1);
    NvOsMemcpy(OutFileName, FileName, FileLen);
    NvOsMemcpy(OutFileName + FileLen, AppendString, AppendLen);
    NvOsMemcpy(OutFileName + ExtStartIndex + AppendLen, Extension, ExtLen);

fail:
    return OutFileName;
}

NvError
NvSecureSignEncryptBuffer(
        NvSecureMode SecureMode,
        NvU8 *Key,
        NvU8 *SrcBuf,
        NvU32 SrcSize,
        NvU8 **HashBuf,
        NvU32 *HashSize,
        NvBool EncryptHash,
        NvBool SignWarmBoot)
{
    NvError e           = NvSuccess;
    const char *err_str = NULL;
    NvU8 *HashBuffer    = NULL;
    NvU32 Size;

    Size = SecureMode == NvSecure_Pkc ? RSA_KEY_SIZE : NV3P_AES_HASH_BLOCK_LEN;
    HashBuffer  = NvOsAlloc(Size);
    VERIFY(HashBuffer != NULL, e = NvError_InsufficientMemory;
           err_str = "Allocating hash buffer failed"; goto fail);

    NvOsMemset(HashBuffer, 0x0, Size);

    if (SecureMode == NvSecure_Pkc)
    {
        if (*((NvU32 *)SrcBuf + 1) != MICROBOOT_HEADER_SIGN &&
            SignWarmBoot == NV_TRUE)
        {
            if (g_BctInterface.NvSecureSignWarmBootCode != NULL)
            {
                e = g_BctInterface.NvSecureSignWarmBootCode(SrcBuf, SrcSize);
                if (e != NvSuccess)
                    NvAuPrintf("\nWarning: Warmboot Signing failed\n");
            }
        }

        NV_CHECK_ERROR_CLEANUP(
            NvSecurePkcSign(SrcBuf, HashBuffer, SrcSize)
        );
    }
    else
    {
        NvU32 NumAesBlocks;
        NumAesBlocks = SrcSize / NV3P_AES_HASH_BLOCK_LEN;
        if (Key == NULL)
            Key = (NvU8 *)g_OptionSbk;

        NvBootHostCryptoEncryptSignBuffer(g_Encrypt, NV_TRUE, Key,
                                     SrcBuf, NumAesBlocks, HashBuffer, NV_FALSE);
        if (EncryptHash)
        {
            NvBootHostCryptoEncryptSignBuffer(NV_TRUE, NV_FALSE, Key,
                                    HashBuffer, 1, SrcBuf, NV_FALSE);
        }
    }

    *HashSize = Size;
    *HashBuf  = HashBuffer;

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (e != NvSuccess && HashBuffer != NULL)
        NvOsFree(HashBuffer);
    return e;
}

NvError
NvSecureSaveFile(
        const char *FileName,
        const NvU8 *Buffer,
        NvU32 BufSize)
{
    NvError e            = NvSuccess;
    char *err_str        = NULL;
    NvOsFileHandle hFile = NULL;

    e = NvOsFopen(FileName, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile);
    VERIFY(e == NvSuccess, err_str = "File open for writing failed"; goto fail);

    e = NvOsFwrite(hFile, Buffer, BufSize);
    VERIFY(e == NvSuccess, err_str = "File write failed"; goto fail);

fail:
    if (hFile)
        NvOsFclose(hFile);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    return e;
}

NvError
NvSecureReadFile(
        const char *FileName,
        NvU8 **Buffer,
        NvBool AesPad,
        NvU32 *BufSize,
        NvU32 PageSize,
        NvU32 AppendSize)
{
    NvError e            = NvSuccess;
    char *err_str        = NULL;
    NvOsFileHandle hFile = NULL;
    NvU32 FileSize       = 0;
    size_t ReadBytes     = 0;
    NvU32 PadSize        = 0;
    NvU32 PaddedSize     = 0;
    NvU8 *FileBuf        = NULL;
    NvU32 ActualSize     = 0;
    NvOsStatType FileStat;

    e = NvOsFopen(FileName, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, err_str = "File open for reading failed"; goto fail);

    NvOsFstat(hFile, &FileStat);
    FileSize   = (NvU32)FileStat.size;
    PaddedSize = FileSize;
    ActualSize = FileSize;

    if (AesPad)
    {
        PadSize  = FileSize % NV3P_AES_HASH_BLOCK_LEN;
        /* AES block length padding. */
        if (PadSize)
        {
            PaddedSize = FileSize - PadSize + NV3P_AES_HASH_BLOCK_LEN;
            PadSize = 0;
        }
        if (g_OptionChipId == 0x20 || g_OptionChipId == 0x30)
        {
            /* Workaround for Boot ROM bug 378464 */
            if (PaddedSize % PageSize == 0)
            {
                PadSize += NV3P_AES_HASH_BLOCK_LEN;
                PaddedSize += NV3P_AES_HASH_BLOCK_LEN;
            }
        }
        FileSize = PaddedSize + NV3P_AES_HASH_BLOCK_LEN + sizeof(NvU32);
    }

    FileSize += AppendSize;

    FileBuf  = NvOsAlloc(FileSize);
    VERIFY(FileBuf != NULL, e = NvError_InsufficientMemory;
           err_str = "Allocating buffer for reading failed"; goto fail);
    NvOsMemset(FileBuf, 0x0, FileSize);

    e = NvOsFread(hFile, FileBuf, ActualSize, &ReadBytes);
    VERIFY(e == NvSuccess, err_str = "File read failed"; goto fail);

    if ((PaddedSize != ActualSize) &&
        ((PaddedSize - ActualSize) != NV3P_AES_HASH_BLOCK_LEN))
    {
        FileBuf[ActualSize + PadSize] = 0x80;
    }

    if (AesPad)
       NvAuPrintf("padded size is %d\n", PaddedSize - ActualSize);
    if (BufSize)
        *BufSize = FileSize;

    *Buffer = FileBuf;

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (hFile)
        NvOsFclose(hFile);
    return e;
}

NvError
NvSecureFillBlobWithFuseData(void)
{
    NvError e            = NvSuccess;
    NvU8 *PrivateKey     = NULL;
    NvU8 *PublicKey      = NULL;
    NvU8 *DsessionKey    = NULL;
    NvU8 *FuseData       = NULL;
    NvU8 *Hash           = NULL;
    NvU32 HashSize       = 0;
    NvU32 Size           = 0;
    NvU8 *Oem_data       = NULL;
    char *err_str        = NULL;
    NvOsFileHandle hFile = NULL;
    NvU32 DecyptedKey[64];
    NvFuseWriteCallbackArg CallbackArg;

    /* Read public, private key & encrypted symmetric key in buffers */
    e = NvSecureReadFile(s_PrivKeyFile, &PrivateKey, NV_FALSE, NULL, 0, 0);
    VERIFY(e == NvSuccess,
           err_str = "Private key file read failed"; goto fail);

    e = NvSecureReadFile(s_DsessionKeyFile, &DsessionKey, NV_FALSE, NULL, 0, 0);
    VERIFY(e == NvSuccess,
           err_str = "Dsession key file read failed"; goto fail);

    e = NvSecureReadFile(s_PubKeyFile, &PublicKey, NV_FALSE, NULL, 0, 0);
    VERIFY(e == NvSuccess,
           err_str = "Public key file read failed"; goto fail);

    /* Decrypt symmetric key got from device */
    NvRSAEncDecMessage((NvU32 *)DecyptedKey,(NvU32 *)DsessionKey,
                       (NvU32 *)PrivateKey, (NvBigIntModulus *)PublicKey);

    /* Parse the data from fuse config file */
    NvOsMemset(&CallbackArg, 0, sizeof(CallbackArg));

    e = NvOsFopen(s_FuseConfigIn, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess,
           err_str = "Fuse file open failed"; goto fail);

    e = nvflash_parser(hFile, &nvflash_fusewrite_parser_callback,
                       NULL, &CallbackArg);
    VERIFY(e == NvSuccess,
           err_str = "Fuse file parsing failed"; goto fail);

    Size = sizeof(CallbackArg.FuseInfo);
    FuseData = NvOsAlloc(Size);
    VERIFY(FuseData, e = NvError_InsufficientMemory;
           err_str = "Allocating fuse data buffer failed"; goto fail);

    NvOsMemcpy(FuseData, &(CallbackArg.FuseInfo), Size);

    /* Encrypt fuse data with the symmetric key got from device */
    e = NvSecureSignEncryptBuffer(NvSecure_Sbk, (NvU8 *) DecyptedKey,
                 (NvU8 *) FuseData, Size, &Hash, &HashSize, NV_FALSE, NV_FALSE);
    VERIFY(e == NvSuccess,
           err_str = "Encrypting fuse data failed"; goto fail);

    /* Save encrypted fuse data in blob */
    e = NvSecureUpdateBlob(NvFlash_Fuse, Size, (NvU8*)FuseData);
    VERIFY(e == NvSuccess,
           err_str = "Writing encrypted fuse data to blob failed"; goto fail);

    NvAuPrintf("Fuse data extracted and updated in blob\n");

    if (s_Oemfile)
    {
        /* Encrypt OEM data with the symmetric key got from device */
        if (Hash)
            NvOsFree(Hash);
        Hash = NULL;
        HashSize = 0;
        Size = OEM_FIELD_SIZE;
        e = NvSecureReadFile(s_Oemfile, &Oem_data, NV_FALSE, NULL, 0, 0);
        VERIFY(e == NvSuccess,
            err_str = "OEM file read failed"; goto fail);
        e = NvSecureSignEncryptBuffer(NvSecure_Sbk, (NvU8 *) DecyptedKey,
                 (NvU8 *) Oem_data, Size, &Hash, &HashSize, NV_FALSE, NV_FALSE);
        VERIFY(e == NvSuccess,
            err_str = "Encrypting OEM data failed"; goto fail);
        /* Save encrypted OEM data in blob */
        e = NvSecureUpdateBlob(NvFlash_oem, Size, (NvU8*)Oem_data);
        VERIFY(e == NvSuccess,
            err_str = "Writing encrypted OEM data to blob failed"; goto fail);

        NvAuPrintf("OEM data updated in blob\n");
    }

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (Hash)
        NvOsFree(Hash);
    if (FuseData)
        NvOsFree(FuseData);
    if (PrivateKey)
        NvOsFree(PrivateKey);
    if (PublicKey)
        NvOsFree(PublicKey);
    if (DsessionKey)
        NvOsFree(DsessionKey);
    if (Oem_data)
        NvOsFree(Oem_data);
    if (hFile)
        NvOsFclose(hFile);
    return e;
}

NvError
NvSecureUpdateBlobWithBootloaders(
        NvFlashPartition *Partition,
        void *Aux)
{
    NvError e           = NvSuccess;
    const char *err_str = NULL;
    NvU8 *BlBuf         = NULL;
    NvU32 BlSize        = 0;
    NvU8 *HashBuf       = NULL;
    NvU32 HashSize      = 0;
    NvU32 PaddedSize    = 0;
    NvU32 PageSize      = 0;
    NvU32 BlobHeaderType;

    if (Partition->Type != Nv3pPartitionType_Bootloader)
        goto fail;

    PageSize = NvSecureGetBctPageSize();
    NV_CHECK_ERROR_CLEANUP(
        NvSecureReadFile(Partition->Filename, &BlBuf,
                         NV_TRUE, &PaddedSize, PageSize, 0)
    );
    BlSize = PaddedSize - NV3P_AES_HASH_BLOCK_LEN - sizeof(NvU32);

    if (*((NvU32 *)BlBuf + 1) == MICROBOOT_HEADER_SIGN)
        BlobHeaderType = NvFlash_mb;
    else
        BlobHeaderType = NvFlash_Bl;

    NV_CHECK_ERROR_CLEANUP(
        NvSecureSignEncryptBuffer(g_SecureMode, NULL, BlBuf,
                           BlSize, &HashBuf, &HashSize, NV_FALSE, NV_TRUE)
    );

    *((NvU32 *)(BlBuf + BlSize)) = Partition->Id;

    e = NvSecureUpdateBlob(BlobHeaderType, BlSize + sizeof(NvU32),
                           BlBuf);
    VERIFY(e == NvSuccess,
         err_str = "Updating blob with Bootloaders is failed\n"; goto fail);

fail:
    if (HashBuf)
        NvOsFree(HashBuf);
    if (BlBuf)
        NvOsFree(BlBuf);
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    return e;
}

NvError
NvSecureDsession(void)
{
    NvError e           = NvSuccess;
    const char *err_str = NULL;
    NvU32 BctSize       = 0;

    e = NvSecureFillBlobWithFuseData();
    VERIFY(e == NvSuccess,
           err_str = "Secure fuse processing failed\n"; goto fail);

    e = NvSecurePartitionOperation(&NvSecureUpdateBlobWithBootloaders, NULL);
    VERIFY(e == NvSuccess,
           err_str = "Updating Bootloaders in Blob failed"; goto fail);

    BctSize = g_BctInterface.NvSecureGetBctSize();

    e = NvSecureUpdateBlob(NvFlash_Bct, BctSize, (NvU8 *)s_BctHandle);
    VERIFY(e == NvSuccess,
           err_str = "Writing encrypted bct to blob failed"; goto fail);

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    return e;
}

static
NvError NvSecureStorePubKeyHash(void)
{
    NvU8 *PubKey = NULL;
    NvU32 Size = 0;
    NvU8 Hash[HASH_SIZE + 1];
    static NvU8 AsciiHash[HASH_SIZE * 2 + 1];
    NvU32 Count = 0;
    NvError e = NvSuccess;

    Size = sizeof(NvBootRsaKeyModulus);
    PubKey = NvOsAlloc(Size);
    VERIFY(PubKey, e = NvError_InsufficientMemory; goto fail);
    NvOsMemset(PubKey, 0x0, Size);

    NvSecurePkcGetPubKey(PubKey);
    e = NvSecureGenerateSHA256Hash(PubKey, Hash, Size);
    VERIFY(e == NvSuccess, goto fail);

    for (Count = 0; Count < HASH_SIZE; Count++)
        NvOsSnprintf((char *)&AsciiHash[Count * 2], 3, "%02x", Hash[Count]);

    // store the ascii form of hash in file
    e = NvSecureSaveFile(s_PubHashFile, AsciiHash, HASH_SIZE * 2);
    VERIFY(e == NvSuccess, goto fail);

    NvAuPrintf("Public key Hash stored in %s\n", s_PubHashFile);

fail:
    if(PubKey)
        NvOsFree(PubKey);
    return e;
}

static NvError NvSecureEncryptTid(void)
{
    NvOsStatType stat;
    size_t bytesread = 0;
    NvOsFileHandle hFile = NULL;
    NvU32 size;
    NvU8 *buffer = NULL;
    NvU32 NumAesBlocks;
    char * err_str = 0;
    NvError e = NvSuccess;

    if (g_SecureMode == NvSecure_Pkc)
    {
        e = NvError_NotSupported;
        err_str = "Not supported for pkc mode";
        goto fail;
    }

    e = NvOsFopen(s_tidfile, NVOS_OPEN_READ|NVOS_OPEN_WRITE, &hFile);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    e = NvOsFstat(hFile, &stat);
    VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);

    size = (NvU32)stat.size;
    buffer = NvOsAlloc(NV3P_AES_HASH_BLOCK_LEN);
    NvOsMemset(buffer, 0, NV3P_AES_HASH_BLOCK_LEN);

    e = NvOsFread(hFile, buffer, size, &bytesread);
    VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);

    if (bytesread != 3)
    {
        e = NvError_BadParameter;
        err_str = "Invalid T_ID file";
        goto fail;
    }

    NumAesBlocks = 1;
    NvBootHostCryptoEncryptSignBuffer(NV_TRUE,
                                      NV_FALSE,
                                      (NvU8*)g_OptionSbk,
                                      buffer,
                                      NumAesBlocks,
                                      NULL,
                                      NV_FALSE);

    e = NvOsFseek(hFile, 0, NvOsSeek_Set);
    VERIFY(e == NvSuccess, err_str = "file seek failed"; goto fail);

    e = NvOsFwrite(hFile, (NvU8 *)buffer, NV3P_AES_HASH_BLOCK_LEN);
    VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);

    NvAuPrintf("%s ->encrypted tidfile \n",s_tidfile);

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (hFile)
        NvOsFclose(hFile);
    if (buffer)
        NvOsFree(buffer);
    return e;
}


int
main(int argc, const char *argv[])
{
    NvError e           = NvSuccess;
    NvBool RetBool      = NV_TRUE;
    NvU32 RetVal        = 0;
    const char *err_str = NULL;
    NvU8 *BlBuf         = NULL;
    NvU8 *BlSig         = NULL;
    NvU32 BlSize        = 0;
    NvU32 SigSize       = 0;
    NvU32 PageSize      = 0;
    char *BlFileOut     = NULL;
    const char *BlobVersion = NULL;
    NvU32 NumBootloaders;
    BctBlUpdateData UpdateBlData;
    NvSecureEncryptSignBlData EncryptSignBlData;

    NvAuPrintf("\nnvsecuretool version %s\n", NvSecureGetToolVersion());

    if (argc < 2)
    {
        NvSecureTool_Help();
        return 0;
    }
    if (argc == 2)
    {
        if (NvOsStrcmp(argv[1], "--help") == 0
            || NvOsStrcmp(argv[1], "-h") == 0)
        {
            NvSecureTool_Help();
            return 0;
        }
    }

    /* parse commandlines arguments */
    RetBool = NvSecureParseCommandLine(argc, argv);
    if (RetBool == NV_FALSE)
    {
        NvAuPrintf("parsing command line failed\n\n");
        RetVal = 1;
        goto fail;
    }

    /* Generate key pairs for initiating secure flow. */
    if (s_RsaKeyFileIn != NULL)
    {
        e = NvSecureGenerateKeyPair(s_RsaKeyFileIn);
        VERIFY(e == NvSuccess,
               err_str = "Generating key pairs failed"; goto fail);

        e = NvSecureGetKeys(s_PubKeyFile, s_PrivKeyFile);
        VERIFY(e == NvSuccess,
               err_str = "Generating Key files failed"; goto fail);

        NvAuPrintf("Private Key is stored in %s\n", s_PrivKeyFile);
        NvAuPrintf("Public Key is stored in %s\n", s_PubKeyFile);

        goto clean;
    }

    if ((g_SecureMode == NvSecure_Pkc) && g_OptionChipId == 0x30)
    {
        NvAuPrintf("PKC is not supported for 0x%d chips\n\n", g_OptionChipId);
        goto fail;
    }

    if (s_NvTestFilename != NULL)
    {
        RetVal = NvSecureSignNvTest();
        VERIFY(RetVal == NV_FALSE,
               err_str = "Sign NvTest failed"; return RetVal);
        return RetVal;
    }

    /* Use right bct structure */
    switch (g_OptionChipId)
    {
        case 0x30:
            NvSecureGetBctT30Interface(&g_BctInterface);
            break;
        case 0x35:
            NvSecureGetBctT1xxInterface(&g_BctInterface);
            break;
        case 0x40:
            NvSecureGetBctT12xInterface(&g_BctInterface);
            break;
        default:
            NvAuPrintf("Invalid chip id used\n\n");
            RetVal = 1;
            goto fail;
    }

    if (s_tidfile)
    {
        RetVal = NvSecureEncryptTid();
        if (RetVal)
        {
            NvAuPrintf("T_ID Encryption failed\n");
            goto fail;
        }
    }

    BlobVersion = NvSecureGetBlobVersion();
    /* Save version info in blob */
    e = NvSecureUpdateBlob(NvFlash_Version,
                           NvOsStrlen(BlobVersion) + 1,
                           (const NvU8 *) BlobVersion);
    VERIFY(e == NvSuccess,
           err_str = "Preparing blob version data failed\n\n"; goto fail);

    /* Create bct buffer to store all bl info. */
    RetVal = NvSecureCreateBctBuffer();
    VERIFY(RetVal == NV_FALSE,
           err_str = "creating bct buffer failed\n\n"; goto fail);

    /* parse the cfg file */
    RetVal = NvSecureParseCfg(s_OptionCfgFile);
    VERIFY(RetVal == NV_FALSE,
           err_str = "Cfg parsing failed\n\n"; goto fail);

    /* Get the number of enabled bootlaoders in cfg */
    NumBootloaders = 0;
    e = NvSecurePartitionOperation(&NvSecureGetNumEnabledBootloader,
                                   &NumBootloaders);
    VERIFY(e == NvSuccess,
           err_str = "Error while computing number of bootloaders"; goto fail);

    NvAuPrintf("Number of bootloaders %d\n", NumBootloaders);

    PageSize = NvSecureGetBctPageSize();
    /* Fill the bootloader information in BCT */
    NvOsMemset(&UpdateBlData, 0x0, sizeof(UpdateBlData));
    UpdateBlData.PageSize  = PageSize;
    UpdateBlData.BlockSize = NvSecureGetBctBlockSize();
    UpdateBlData.SecureBctHandle = s_BctHandle;
    UpdateBlData.NumBootloaders = NumBootloaders;
    UpdateBlData.BlInstance = NumBootloaders - 1;
    e = NvSecurePartitionOperation(&NvSecureUpdateBctWithBlInfo, &UpdateBlData);
    VERIFY(e == NvSuccess,
          err_str = "Updating Bootloader information in BCT failed"; goto fail);

    /* Generate key pairs for PKC mode. */
    if (g_SecureMode == NvSecure_Pkc)
    {
        e = NvSecureGenerateKeyPair(s_KeyFileIn);
        VERIFY(e == NvSuccess,
               err_str = "Generating key pairs failed"; goto fail);

        /* store public key Hash in file */
        e = NvSecureStorePubKeyHash();
        VERIFY(e == NvSuccess,
            err_str = "Storing public hash in file failed"; goto fail);

    }

    /* Save encrypted Rcm messages in blob. */
    RetVal = NvSecurePrepareRcmData();
    VERIFY(RetVal == NV_FALSE,
           err_str = "preparing blob rcm data failed"; goto fail);

    /* Fill the bootloader hash into BCT */
    EncryptSignBlData.PageSize =  PageSize;
    EncryptSignBlData.SecureBctHandle = s_BctHandle;
    EncryptSignBlData.BlInstance = NumBootloaders - 1;
    e = NvSecurePartitionOperation(&NvSecureEncryptSignBootloaders,
                                   &EncryptSignBlData);
    VERIFY(e == NvSuccess,
           err_str = "Updating Bootloader hash in BCT failed"; goto fail);

    e = NvSecurePartitionOperation(&NvSecureSignTrustedOSImage, NULL);
    VERIFY(e == NvSuccess,
           err_str = "Signing trusted os image failed"; goto fail);

    e = NvSecurePartitionOperation(&NvSecureSignWB0, NULL);
    VERIFY(e == NvSuccess, err_str = "Signing WB0 image failed";);

    /* Comput bootloader signature and store it in blob */
    NV_CHECK_ERROR_CLEANUP(
        NvSecureReadFile(s_OptionBootLoaderIn, &BlBuf,
                         NV_TRUE, &BlSize, PageSize, 0)
    );
    if (BlBuf == NULL)
        goto fail;

    BlSize = BlSize - NV3P_AES_HASH_BLOCK_LEN - sizeof(NvU32);

    NV_CHECK_ERROR_CLEANUP(
        NvSecureSignEncryptBuffer(g_SecureMode, NULL, BlBuf,
                                  BlSize, &BlSig, &SigSize, NV_TRUE, NV_TRUE)
    );
    VERIFY(e == NvSuccess,
           err_str = "error while encrypting/signing command line bootloader";
           goto fail);

    /* Save Booloader signature in blob */
    e = NvSecureUpdateBlob(NvFlash_BlHash, SigSize, BlSig);

    VERIFY(e == NvSuccess,
           err_str = "Writing bootloaer hash in blob failed"; goto fail);

    /* Save encrypted/signed command line bootloader */
    if (s_OptionBootLoaderOut == NULL)
    {
        BlFileOut = NvSecureGetOutFileName(s_OptionBootLoaderIn, NULL);
        if (BlFileOut == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        s_OptionBootLoaderOut = BlFileOut;
    }

    e = NvSecureSaveFile(s_OptionBootLoaderOut, BlBuf, BlSize);
    VERIFY(e == NvSuccess,
           err_str = "Error while saving encrypted bootloader"; goto fail);

    NvAuPrintf("Command line bootloader file %s is saved as %s\n",
               s_OptionBootLoaderIn, s_OptionBootLoaderOut);

    /* Save Bct */
    RetVal = NvSecureSaveBct();
    VERIFY(RetVal == NV_FALSE, err_str =  "BCT saving failed\n\n"; goto fail);

    if (s_DsessionKeyFile != NULL)
        NvSecureDsession();

    NvAuPrintf("Blob is saved as %s\n", s_BlobOut);

    goto clean;

fail:
    RetVal = 1;

clean:
    if (BlFileOut != NULL)
        NvOsFree(BlFileOut);
    if (BlBuf)
        NvOsFree(BlBuf);
    if (BlSig)
        NvOsFree(BlSig);
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (s_BlobFile)
        NvOsFclose(s_BlobFile);

    return RetVal;
}

NvError
NvSecureSignNvTest(void)
{
    NvError e            = NvSuccess;
    char *err_str        = NULL;
    NvU8* pNvTest        = NULL;
    NvU32 NvTestSize     = 0;
    NvU8 *RcmMsgBuff     = NULL;
    NvFlashRcmDownloadData Download_arg;
    NvU32 MsgBufferLength;
    NvDdkFuseOperatingMode OperatingMode;
    pNvSecureHostRcmCreateMsgFromBuffer RcmCreateMsg;
    pNvBootHostRcmGetMsgLength RcmGetMsgLength;

    /* Generate key pairs for PKC mode. */
    if (g_SecureMode == NvSecure_Pkc)
    {
        e = NvSecureGenerateKeyPair(s_KeyFileIn);
        VERIFY(e == NvSuccess,
               err_str = "Generating key pairs failed"; goto fail);

        /* store public key Hash in file */
        e = NvSecureStorePubKeyHash();
        VERIFY(e == NvSuccess,
            err_str = "Storing public hash in file failed"; goto fail);

    }

    if (g_OptionChipId == 0x30)
    {
        RcmCreateMsg    = &NvBootHostRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostRcmGetMsgLength;
    }
    else if (g_OptionChipId == 0x35)
    {
        RcmCreateMsg    = &NvBootHostT1xxRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostT1xxRcmGetMsgLength;
    }
    else if (g_OptionChipId == 0x40)
    {
        RcmCreateMsg    = &NvBootHostT12xRcmCreateMsgFromBuffer;
        RcmGetMsgLength = &NvBootHostT12xRcmGetMsgLength;
    }
    else
    {
        e = NvError_BadParameter;
        VERIFY(0, err_str = "Invalid ChipId\n"; goto fail);
    }

    OperatingMode = NvDdkFuseOperatingMode_OdmProductionSecure;
    if (g_SecureMode == NvSecure_Pkc)
    {
        OperatingMode = NvDdkFuseOperatingMode_OdmProductionSecurePKC;
        RcmCreateMsg  = &NvSecureHostRcmCreateMsgFromBuffer;
    }

    LoadNvTest(s_NvTestFilename, &Download_arg.EntryPoint,
               &pNvTest, &NvTestSize);

    e = (*RcmCreateMsg) (NvFlashRcmOpcode_DownloadExecute, NULL,
                         (NvU8 *)&Download_arg, sizeof(Download_arg),
                          NvTestSize, pNvTest, OperatingMode,
                         (NvU8 *)g_OptionSbk, &RcmMsgBuff);

    VERIFY(e == NvSuccess,
        err_str = "Signed NvTest message creation failed"; goto fail);

    MsgBufferLength = (*RcmGetMsgLength) (RcmMsgBuff);

    SaveSignedNvTest(MsgBufferLength, RcmMsgBuff,
                     s_NvTestFilename, ".signed");
    VERIFY(e == NvSuccess,
        err_str = "Save singed NvTest failed"; goto fail);
    NvOsFree(RcmMsgBuff);
    RcmMsgBuff = NULL;

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (RcmMsgBuff)
        NvOsFree(RcmMsgBuff);
    return e;
}
