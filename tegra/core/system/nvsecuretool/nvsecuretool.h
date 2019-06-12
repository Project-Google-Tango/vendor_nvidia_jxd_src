/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVSECURETOOL_H
#define INCLUDED_NVSECURETOOL_H

#include "nvos.h"
#include "nvbct.h"
#include "nvutil.h"
#include "nvapputil.h"
#include "nvboothost.h"
#include "nvflash_version.h"

#define LP0_EXIT_RUN_ADDRESS    0x40020000

#define HASH_SIZE               32
#define RSA_KEY_SIZE            256
#define NV3P_AES_HASH_BLOCK_LEN 16

#define MICROBOOT_HEADER_SIGN   0xDEADBEEF

#define VERIFY(exp, code) \
    if(!(exp)) { \
        code; \
    }

/* Defines the supported key types */
typedef enum
{
    NvSecure_Pkc = 1,
    NvSecure_Sbk,
}NvSecureMode;

typedef struct NvSecureUniqueIdRec
{
    NvU32 ECID_0;
    NvU32 ECID_1;
    NvU32 ECID_2;
    NvU32 ECID_3;
}NvSecureUniqueId;

typedef struct NvSecureBctInterfaceRec
{
    NvU32 (*NvSecureGetMlAddress)(void);
    NvU32 (*NvSecureGetBctSize)(void);
    NvU32 (*NvSecureGetBctCryptoHashSize)(void);
    NvU32 (*NvSecureGetPageSize)(NvU8 *BctBuf);
    NvU32 (*NvSecureGetBlLoadAddr)(void);
    NvU32 (*NvSecureGetBlEntryPoint)(void);
    NvU32 (*NvSecureGetMbLoadAddr)(void);
    NvU32 (*NvSecureGetMbEntryPoint)(void);
    NvU32 (*NvSecureGetMaxMbSize)(void);
    void (*NvSecureGetDevUniqueId)(NvU8 *BctBuf,NvSecureUniqueId *UniqueId);
    NvError (*NvSecureSignWarmBootCode)(NvU8* BlSrcBuf, NvU32 BlLength);
    NvError (*NvSecureSignWB0Image)(NvU8* WB0Buf, NvU32 WB0Length);
}NvSecureBctInterface;


// outputs information about how to use nvsecuretool
void NvSecureTool_Usage(void);

// outputs information about various commands used with nvsecure tool
void NvSecureTool_Help(void);

/**
 * validates the format of the input SBK key string
 *
 * @param arg sbk string input as a command line argument
 *
 * Returns NV_TRUE if key format is valid(hex input) otherwise NV_FALSE
 */
NvBool NvSecureCheckValidKeyInput(const char *arg);

/**
 * parse commandline provided to nvsecuretool
 *
 * @param argc number of arguments in command line
 * @param argv list of arguments in command line
 *
 * Returns NV_TRUE if parsing is successful otherwise NV_FALSE
 */
NvBool NvSecureParseCommandLine(int argc, const char *argv[]);

/**
 * update blob to include info like sbktool version RCM messages and Bl hash
 *
 * @param HeaderType type of info this header is associated with.
 * @param HeaderLength data size of info in bytes.
 * @param BlobBuf whole info to be stored in blob.
 *
 * Returns NvSuccess if parsing is successful otherwise NV_FALSE
 */
NvError NvSecureUpdateBlob(
        NvU32 HeaderType,
        NvU32 HeaderLength,
        const NvU8 *BlobBuf);

/**
 * Prepares RCM messages encrypted and signed and save them
 * in two data files. these files used to communicate with bootrom.
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecurePrepareRcmData(void);

/**
 * allocate bct handler to global bct buffer
 *
 * @param pBctHandle handle to bct buffer
 * @param BctBuffer bct buffer whose handler has to be allocated
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecureAllocateBct(NvBctHandle *pBctHandle, NvU8 *BctBuffer);

/**
 * Create global bct buffer and store input bct file data into it.
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecureCreateBctBuffer(void);

/**
 * compute hash of bct buffer and store it in bct
 *
 * @param BctBuf bct buffer whose hash is to calculated
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecureComputeBctHash(NvU8 *BctBuf);

/**
 * Save bct in ouput bct file after storing bl info and crypto hash of bct
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecureSaveBct(void);

/**
 * Get page size from bct
 * Return page size if successful otherwise 0
 */
NvU32 NvSecureGetBctPageSize(void);

/**
 * Get block size from bct
 * Return block size if successful otherwise 0
 */
NvU32
NvSecureGetBctBlockSize(void);

/**
 * Fills bootloader info like load addr, entry point, version and hash into global bct
 * @param BlHashBuf hash of bootloader to be updated
 * @param nBootCopy number of bl info to be updated
 * @param IsMicroboot if microboot it is true
 *
 * Return NvSuccess if successful otherwise appropriate error code
 */
NvError NvSecureFillBlInfo(NvU8 *BlHashBuf, NvU32 nBootCopy, NvBool IsMicroboot);

/* Return the offset of data on which signature is generated. */
NvU32
NvSecureGetDataOffset(void);

/**
 * Generates cryptohash on input buffer
 * @param SrcBuf pointer to the input buffer for signing
 * @param HashBuf pointer to the o/p buffer  in which cryptohash is stored
 * @param ActualSize size of input buffer
 *
 */

void
NvSecureGenerateCryptoHash(
    NvU8 * SrcBuf,NvU8 *HashBuf,NvU32 ActualSize);

//pkc changes
//fixme :comments to be added based on buildimage implementation
NvBool NvSecureCheckIsValidPKC(const char *arg);
NvError NvSecureGenerateAndFillPublicKey(const char *arg);
NvError NvSecurePreparePKCRcmData(void);
NvError
NvSecureComputeAndFillBctSignature(
    const char *BlFileIn, NvBctHandle BctHandle, NvBool Bl);


/* Initialize bct interface to use appropriate bct structure whether
 * t30 or t114 or t124
 */
void NvSecureGetBctT30Interface(NvSecureBctInterface *pBctInterface);
void NvSecureGetBctT1xxInterface(NvSecureBctInterface *pBctInterface);
void NvSecureGetBctT12xInterface(NvSecureBctInterface *pBctInterface);

NvError NvBootSecurePKCRcmSignMsgT1xx(NvU8 *MsgBuffer);
NvError NvBootSecurePKCRcmSignMsgT12x(NvU8 *MsgBuffer);

NvError
NvSecurePkcSign(NvU8 *Src, NvU8 *Dst, NvU32 Length);
NvError
NvSecureGenerateSHA256Hash(NvU8 *Src, NvU8 *Dst, NvU32 Length);
void
NvSecurePkcGetPubKey( NvU8 *Dst);
void
NvSecurePkcGetPubKeyT12x(NvU8 *Dst);
void
NvSecurePkcGetPubKeyT1xx(NvU8 *Dst);

NvError
NvSecureHostRcmCreateMsgFromBuffer(
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    NvU8 *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength,
    NvU8 *PayloadBuffer,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk,
    NvU8 **pMsgBuffer);

NvError
NvSecureGenerateKeyPair(const char *KeyFile);

/*
 *Performs encryption of fuse data with decrypted symmetric key
 */
NvError NvSecureFuseOperation (void);

/*
 *Stores the public & private key pair in text files
 */
NvError NvSecureGetKeys(const char*spub,const char *spriv);

/* Reads the file into bufer and also pads if necessary
 *
 * @param FileName Name of the file to be read into buffer
 * @param Buffer Points the the buffer containing file contents
 * @param AesPad True then buffer is aligned to AES size
 * @param BufSize Memory allocated to Buffer
 * @param PageSize PageSize to used to align the buffer
 *
 * Returns NvSuccess if file read successfuly
 */
NvError
NvSecureReadFile(
        const char *FileName,
        NvU8 **Buffer,
        NvBool AesPad,
        NvU32 *BufSize,
        NvU32 PageSize,
        NvU32 AppendSize);

/* Saves the buffer into file
 *
 * @param FileName Name of the file into which buffer is to be written
 * @param Buffer Contains the content to be written to file
 * @param BufSize Size of Buffer which is to be written
 *
 * Returns NvSuccess if file is write is successful
 */
NvError
NvSecureSaveFile(
        const char *FileName,
        const NvU8 *Buffer,
        NvU32 BufSize);

/* Returns the out file name corresponding to Mode(SBK/PKC)
 *
 * @Param FileName name for which output file name is computed
 * @param Extension extension to be replaced or appended
 *
 * Return the string of out file name if successful else null
 */
char*
NvSecureGetOutFileName(const char *FileName, const char *Extension);

/* Computes the hash of buffer according to Mode
 *
 * @param KeyType Type of key (SBK/PKC)
 * @param Key Pointer to a buffer containing key
 * @param SrcBuf Buffer for which hash is to be computed
 * @param SrcSize Size of input buffer
 * @param HashBuf Points to the buffer containig hash
 * @param HashSize Holds the size of hash buffer
 * @param EncryptHash Ecrypts the hash for SBK mode only
 * @param SignWarmBoot Sign warmboot if present in PKC mode only
 * Returns NvSucces if hash computations is successfule
 */
NvError
NvSecureSignEncryptBuffer(
        NvSecureMode SecureMode,
        NvU8 *Key,
        NvU8 *SrcBuf,
        NvU32 SrcSize,
        NvU8 **HashBuf,
        NvU32 *HashSize,
        NvBool EncryptHash,
        NvBool SignWarmBoot);

/* Returns the nvsecuretool version */
const char* NvSecureGetToolVersion(void);

/* Returns the blob version */
const char* NvSecureGetBlobVersion(void);

/* Sign nvtest */
NvError NvSecureSignNvTest(void);

#endif//INCLUDED_NVSECURETOOL_H

