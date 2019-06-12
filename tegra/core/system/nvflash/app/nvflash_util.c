/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvflash_util.h"
#include "nvflash_util_private.h"
#include "nvapputil.h"
#include "nvutil.h"
#include "nvbuildbct.h"
#include "nvflash_parser.h"
#include "nvflash_commands.h"

static NvU32 g_sDevId = 0;
static Nv3pCmdGetPlatformInfo *s_PlatformInfo;
static NvDdkFuseOperatingMode s_OpMode = 3;

#define CEIL_A_DIV_B(a, b) ((a)/(b) + (1 && ((a)%(b))))

#define VERIFY( exp, code ) \
    if( !(exp) ) { \
        code; \
    }

/* this value should be updated as per change in Bootrom*/
#define BOOTROM_PAGESIZE 512

typedef struct NvBctConfParserArgRec
{
    NvFlashBctParseInfo BctParseInfo;
    NvBool Found;
} NvBctConfParserArg;

static NvFlashParserStatus
nvflash_bctConfig_parser_callback(NvFlashVector *rec, void *aux);

NvU32
nvflash_get_bct_size( void )
{
    NvFlashUtilHal Hal;

    Hal.devId = g_sDevId;
    if (NvFlashUtilGetT30Hal(&Hal)
        || NvFlashUtilGetT11xHal(&Hal)
        || NvFlashUtilGetT12xHal(&Hal))
    {
        return Hal.GetBctSize();
    }
    return 0;
}

void
nvflash_set_devid( NvU32 devid )
{
    g_sDevId = devid;
}

NvU32
nvflash_get_devid( void )
{
    return g_sDevId;
}

void
nvflash_set_platforminfo( Nv3pCmdGetPlatformInfo *Info )
{
    s_PlatformInfo = Info;
}

void
nvflash_get_bl_loadaddr( NvU32* entry, NvU32* addr)
{
    NvFlashUtilHal Hal;

    Hal.devId = g_sDevId;
    if (NvFlashUtilGetT30Hal(&Hal)
        || NvFlashUtilGetT11xHal(&Hal)
        || NvFlashUtilGetT12xHal(&Hal)
        )
    {
        *entry = Hal.GetBlEntry();
        *addr = Hal.GetBlAddress();
    }
}

void
nvflash_set_nctinfoinbct(NctCustInfo NCTCustInfo, NvU8 * BctBuffer)
{
    NvFlashUtilHal Hal;

    Hal.devId = g_sDevId;
    if (NvFlashUtilGetT30Hal(&Hal)
        || NvFlashUtilGetT11xHal(&Hal)
        || NvFlashUtilGetT12xHal(&Hal))
    {
        return Hal.SetNCTCustInfoFieldsInBct(NCTCustInfo, BctBuffer);
    }

}

/* don't include the header -- source orgianization with binary release
 * issue.
 */
// FIXME: should use const char * for inputs
NvError
PostProcDioOSImage(NvU8 *InputFile, NvU8 *OutputFile);
NvError
NvConvertStoreBin(NvU8 *InputFile, NvU32 BlockSize, NvU8 *OutputFile);

NvBool
nvflash_util_preprocess( const char *filename, NvFlashPreprocType type,
    NvU32 blocksize, char **new_filename )
{
    NvError e;
    NvBool b;
    char *f = 0;

    if( type == NvFlashPreprocType_DIO )
    {
        f = "post_process.dio2";

        NV_CHECK_ERROR_CLEANUP(
            PostProcDioOSImage( (NvU8 *)filename, (NvU8 *)f )
        );
    }
    else if( type == NvFlashPreprocType_StoreBin )
    {
        // FIXME: should this be a .dio2?
        f = "post_munge.dio2";

        NV_CHECK_ERROR_CLEANUP(
            NvConvertStoreBin( (NvU8 *)filename, blocksize, (NvU8 *)f )
        );
    }
    else
    {
        goto fail;
    }

    *new_filename = f;

    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;

clean:
    return b;
}

NvError
nvflash_util_dumpbit(NvU8 * BitBuffer, const char *dumpOption)
{
    NvFlashUtilHal Hal;

    Hal.devId = g_sDevId;
    if (NvFlashUtilGetT30Hal(&Hal)
        || NvFlashUtilGetT11xHal(&Hal)
        || NvFlashUtilGetT12xHal(&Hal)
       )
    {
        Hal.GetDumpBit(BitBuffer, dumpOption);
        return NvSuccess;
    }
    return NvError_BadParameter;
}

int
nvflash_util_atoi(char *str, int len)
{
    int n = 0, i = 0;

    if (!str)
        return -1;
    while (i < len)
    {
        n = n * 10 + (str[i] - '0');
        i++;
    }
    return n;
}

NvU64 nvflash_util_roundsize(NvU64 Num, NvU32 SizeMultiple)
{
    NvU64 Rem;
    Rem = Num % SizeMultiple;
    if (Rem != 0)
        Num = (CEIL_A_DIV_B(Num, SizeMultiple)) * SizeMultiple;

    return Num;
}

NvBool nvflash_sign_wb0(const char *pFileName, NvU32 *Wb0Codelength,
                               NvU8 **Wb0Buf)
{
    NvError e = NvSuccess;
    char *pErrStr = NULL;
    NvBool Ret = NV_TRUE;
    size_t ReadBytes = 0;
    NvOsStatType FileStat;
    NvOsFileHandle hFile = NULL;
    NvU8 *Buffer = NULL;
    NvFlashUtilHal Hal;

    if (Wb0Codelength == NULL || Wb0Buf == NULL || pFileName == NULL)
    {
        e = NvError_BadParameter;
        pErrStr = "Argument is null";
        goto fail;
    }

    e = NvOsStat(pFileName, &FileStat);
    if (e != NvSuccess)
    {
        pErrStr = "File not found";
        goto fail;
    }

    *Wb0Codelength = (NvU32)FileStat.size;

    if ((*Wb0Codelength % NV3P_AES_HASH_BLOCK_LEN) != 0)
    {
        *Wb0Codelength += NV3P_AES_HASH_BLOCK_LEN -
                     (*Wb0Codelength % NV3P_AES_HASH_BLOCK_LEN);
    }

    e = NvOsFopen(pFileName, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, pErrStr = "File open for reading failed"; goto fail);

    Buffer  = NvOsAlloc(*Wb0Codelength);
    VERIFY(Buffer, e = NvError_InsufficientMemory;
           pErrStr = "Allocating buffer for reading failed"; goto fail);
    NvOsMemset(Buffer, 0x0, *Wb0Codelength);

    e = NvOsFread(hFile, Buffer, (NvU32)FileStat.size, &ReadBytes);
    VERIFY(e == NvSuccess, pErrStr = "File read failed"; goto fail);

    Hal.devId = g_sDevId;
    if (NvFlashUtilGetT12xHal(&Hal))
    {
        e = Hal.NvSecureSignWB0Image(Buffer, *Wb0Codelength);
        VERIFY(e == NvSuccess, pErrStr = "WB0 signing failed"; goto fail);
        *Wb0Buf = Buffer;
        goto clean;
    }
    else
    goto fail;

fail:
    Ret = NV_FALSE;
    if (pErrStr != NULL)
        NvAuPrintf("%s NvError 0x%x\n", pErrStr, e);
    if (Buffer)
        NvOsFree(Buffer);
clean:
   if (hFile != NULL)
        NvOsFclose(hFile);

   return Ret;
}

NvBool
nvflash_align_bootloader_length(
    const char *pFileName,
    NvU32 *pPadSize,
    NvU8 **pPadBuf)
{
    NvError e = NvSuccess;
    NvBool Ret = NV_TRUE;
    NvU64 Total = 0;
    NvU64 PadSize = 0;
    NvU32 BytesToAdd = 0;
    NvU32 ChipId = 0;
    NvU8 *pBuf = NULL;
    char *pErrStr = NULL;
    NvOsStatType FileStat;
    NvOsFileHandle hFile = NULL;

    if (pPadSize == NULL || pPadBuf == NULL || pFileName == NULL)
    {
        e = NvError_BadParameter;
        pErrStr = "Argument is null";
        goto fail;
    }

    e = NvOsStat(pFileName, &FileStat);
    if (e != NvSuccess)
    {
        pErrStr = "File not found";
        goto fail;
    }

    Total = FileStat.size;

    if ((Total % NV3P_AES_HASH_BLOCK_LEN) != 0)
    {
        BytesToAdd = (NvU32)(NV3P_AES_HASH_BLOCK_LEN -
                     (Total % NV3P_AES_HASH_BLOCK_LEN));
    }

    // WORKAROUND FOR HW BUG 378464
    ChipId = nvflash_get_devid();
    if (ChipId == 0x30)
    {
        if ((Total + BytesToAdd) % BOOTROM_PAGESIZE == 0)
        {
            BytesToAdd += NV3P_AES_HASH_BLOCK_LEN;
            PadSize += NV3P_AES_HASH_BLOCK_LEN;
        }
    }

    if (BytesToAdd)
    {
        pBuf = NvOsAlloc(BytesToAdd);
        VERIFY(pBuf, pErrStr = "buffer allocation failed"; goto fail);
        NvOsMemset(pBuf, 0x0, BytesToAdd);

        if (BytesToAdd != NV3P_AES_HASH_BLOCK_LEN)
            pBuf[PadSize] = 0x80;

        NvAuPrintf("padded %d bytes to bootloader\n", BytesToAdd);

        /* If able to open file in append mode then write the pad buffer
         * at the end of file and save it.
         */
        e = NvOsFopen(pFileName, NVOS_OPEN_APPEND, &hFile);
        if (e == NvSuccess)
        {
            e = NvOsFseek(hFile, 0, NvOsSeek_End);
            VERIFY(e == NvSuccess, pErrStr = "file seek failed"; goto fail);
            e = NvOsFwrite(hFile, pBuf, (size_t) BytesToAdd);
            VERIFY(e == NvSuccess, pErrStr = "file write failed"; goto fail);
            NvOsFree(pBuf);
            pBuf = NULL;
        }
    }

    *pPadBuf  = pBuf;
    *pPadSize = BytesToAdd;

    goto clean;

fail:
    Ret = NV_FALSE;
    if (pErrStr != NULL)
        NvAuPrintf("%s NvError 0x%x\n", pErrStr, e);
    if (pBuf != NULL)
        NvOsFree(pBuf);

clean:
   if (hFile != NULL)
        NvOsFclose(hFile);

    return Ret;
}

NvError
nvflash_util_buildbct(
    const char *pCfgFile,
    const char *pBctFile,
    const NvFlashBctParseInfo *pBctParseInfo,
    NvU8 **pBctBuffer,
    NvU32 *pBctSize)
{
    NvError e = NvSuccess;
    const char *pErrStr = NULL;
    NvU64 Offset = 0;
    NvBctConfParserArg CallbackArg;
    NvOsFileHandle hFile = NULL;
    BuildBctArg ArgBuildBct;

    NvOsMemset(&ArgBuildBct, 0, sizeof(ArgBuildBct));

    if ((pBctParseInfo != NULL) && (g_sDevId != 0x30))
    {
        CallbackArg.Found    = NV_FALSE;
        CallbackArg.BctParseInfo = *pBctParseInfo;

        e = NvOsFopen(pCfgFile, NVOS_OPEN_READ, &hFile);
        VERIFY(e == NvSuccess, pErrStr = "Cfg file open failed"; goto fail);

        e = nvflash_parser(hFile, &nvflash_bctConfig_parser_callback,
                            &Offset, &CallbackArg);

        VERIFY(e == NvSuccess,
               pErrStr = "Error while parsing config file."; goto fail);

        if (CallbackArg.Found == NV_FALSE && Offset)
        {
            NvAuPrintf("\nNo matching BCT for:\n"
                       "Board         %d\n"
                       "Fab No        %d\n"
                       "Board SKU     %d\n"
                       "Mem Type      %d\n"
                       "Mem Frequency %d\n"
                       "Chip SKU      %d\n\n",
                        pBctParseInfo->BoardId,
                        pBctParseInfo->BoardFab,
                        pBctParseInfo->BoardSku,
                        pBctParseInfo->MemType,
                        pBctParseInfo->MemFreq,
                        pBctParseInfo->SkuType);

            e = NvError_BadParameter;
            goto fail;
        }
    }

    if (Offset)
    {
        NvAuPrintf("\nFound matching BCT for:\n"
                   "Board         %d\n"
                   "Fab No        %d\n"
                   "Board SKU     %d\n"
                   "Mem Type      %d\n"
                   "Mem Frequency %d\n"
                   "Chip SKU      %d\n\n",
                    CallbackArg.BctParseInfo.BoardId,
                    CallbackArg.BctParseInfo.BoardFab,
                    CallbackArg.BctParseInfo.BoardSku,
                    CallbackArg.BctParseInfo.MemType,
                    CallbackArg.BctParseInfo.MemFreq,
                    CallbackArg.BctParseInfo.SkuType);
    }

    ArgBuildBct.pCfgFile = pCfgFile;
    ArgBuildBct.pBctFile = pBctFile;
    ArgBuildBct.ChipId  = g_sDevId;
    ArgBuildBct.Offset  = Offset;
    ArgBuildBct.pPlatformInfo = s_PlatformInfo;

    e = NvBuildBct(&ArgBuildBct);

    if ((pBctFile == NULL) && (pBctBuffer != NULL))
        *pBctBuffer = ArgBuildBct.pBctBuffer;

    if (pBctSize != NULL)
        *pBctSize = ArgBuildBct.BctSize;

fail:
    if (pErrStr != NULL)
        NvAuPrintf("%s\n", pErrStr);
    if (hFile != NULL)
        NvOsFclose(hFile);

    return e;
}

static NvFlashParserStatus
nvflash_bctConfig_parser_callback(NvFlashVector *rec, void *aux)
{
    NvU32 i;
    NvFlashParserStatus ret   = P_CONTINUE;
    NvU32 nPairs              = rec->n;
    NvBool found              = NV_TRUE;
    NvFlashSectionPair *Pairs = rec->data;
    NvBool unknown_token     = NV_FALSE;
    NvBctConfParserArg *arg  = (NvBctConfParserArg *) aux;
    NvFlashBctParseInfo *pParseInfo  = &(arg->BctParseInfo);
    NvU32 Freq;
    NvFlashBctParseInfo CfgInfo;

    /* Initialize the local copy of board info structure with the requested
     * data, while parsing the CFG file overwrite the local copy with info
     * from CFG. Finally compare info from CFG file with requested data to
     * find correct BCT params
     */

    NvOsMemcpy(&CfgInfo, pParseInfo, sizeof(Nv3pBoardId));
    CfgInfo.SkuType = 0;
    Freq = CfgInfo.MemFreq;

    for(i = 0; i < nPairs; i++)
    {
        if(NvFlashIStrcmp(Pairs[i].key, "board_no") == 0)
        {
            CfgInfo.BoardId = NvUStrtoul(Pairs[i].value, NULL, 0);
        }
        else if(NvFlashIStrcmp(Pairs[i].key, "fab_no") == 0)
        {
            CfgInfo.BoardFab = NvUStrtoul(Pairs[i].value, NULL, 10);
        }
        else if(NvFlashIStrcmp(Pairs[i].key, "board_sku") == 0)
        {
            CfgInfo.BoardSku = NvUStrtoul(Pairs[i].value, NULL, 10);
        }
        else if(NvFlashIStrcmp(Pairs[i].key, "mem_type") == 0)
        {
            CfgInfo.MemType = NvUStrtoul(Pairs[i].value, NULL, 10);
        }
        else if(NvFlashIStrcmp(Pairs[i].key, "mem_freq") == 0)
        {
            CfgInfo.MemFreq = NvUStrtoul(Pairs[i].value, NULL, 0);
            Freq = CfgInfo.MemFreq;
            if(pParseInfo->MemFreq == 0)
                CfgInfo.MemFreq = 0;
        }
        else if(NvFlashIStrcmp(Pairs[i].key, "sku") == 0)
        {
            CfgInfo.SkuType = (NvU32) NvUStrtoul(Pairs[i].value, NULL, 0);
        }
        else
        {
            unknown_token = NV_TRUE;
            goto fail;
        }
    }

    if(NvOsMemcmp(pParseInfo, &CfgInfo, sizeof(CfgInfo)) == 0)
    {
        pParseInfo->MemFreq = Freq;
        ret = P_STOP;
        goto end;
    }

fail:
    found = NV_FALSE;
    if(unknown_token)
    {
        NvAuPrintf("Invalid token in config file: %s\n", Pairs[i].key);
        ret = P_ERROR;
    }
end:
    arg->Found = found;
    return ret;
}

/**
 * Pre calculated modulo 2 division remainder for 256 bytes combination
 */
static NvU32 crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/**
 *Performs CRC32 over given buffer
 */
NvU32 nvflash_crc32(NvU32 CrcInitVal, const NvU8 *buf, NvU32 size)
{
    NvU32 crc = CrcInitVal ^ ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    return crc ^ ~0U;
}
void nvflash_set_opmode(NvU32 OpMode)
{
    s_OpMode = OpMode;
}

NvDdkFuseOperatingMode nvflash_get_opmode(void)
{
    return s_OpMode;
}
