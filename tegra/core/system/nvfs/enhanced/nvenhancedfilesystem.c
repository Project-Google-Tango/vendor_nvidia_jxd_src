/**
 * Copyright (c) 2008-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define NV_ENABLE_DEBUG_PRINTS 0

#include "nvcrypto_common_defs.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_cipher.h"
#include "nvpartmgr_defs.h"
#include "nvenhancedfilesystem.h"


//Macros for error checkings
#define CHECK_PARAM(x) \
        if (!(x)) \
        { \
           NV_ASSERT(x); \
           return NvError_BadParameter; \
        }

#define CHECK_MEM(x) \
        if (!(x)) \
        { \
           e = NvError_InsufficientMemory; \
           goto fail; \
        }

/*
 * Local structures
 */

typedef struct NvEnhancedFileSystemRec
{
    NvFileSystem hFileSystem;
    NvPartitionHandle hPartition;
    NvDdkBlockDevHandle hBlkDevHandle;
    NvU32 RefFileCount;
}NvEnhancedFileSystem;

// File Header structure
typedef struct NvEnhanceFSHeaderRec
{
    // Holds crypto Hash value
    NvU8 Hash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    // Holds Crypto Hash padding data
    NvU8 HashPaddingData[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    // Holds Crypto Cipher padding data
    NvU8 CipherPaddingData[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    // Holds file size
    NvU64 FileSize;
    // Holds crypto Hash size
    NvU32 HashSize;
    // Holds crypto Hash padding size
    NvU32 HashPaddingSize;
    // Holds crypto Cipher padding size
    NvU32 CipherPaddingSize;
    // Holds file data version. Currently it is not used
    NvU32 DataVersion;
    // Flag to indicate whether data in the file is valid or not
    // 0 is valid. 1 in invalid
    NvU8 IsFileDataValid;
    // Indicates whether the file is encrypted or not when data was written
    NvBool IsFileEncrypted;
    // Indicates whether the Hash was performed or not when data was written
    NvBool IsHashPerformed;
}NvEnhancedFSHeader;

// FileSystemFile structure. Holds file specific data
typedef struct NvEnhancedFileSystemFileRec
{
    // NvFileSystemFile Handle
    NvFileSystemFile hFileSystemFile;
    // File header
    NvEnhancedFSHeader FileHeader;
    // Holds computed hash value when file is opened in read mode
    NvU8 CurrentHash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    // Maximum file size. It is size of partition in bytes
    NvU64 MaxFileSize;
    // Current file position
    NvU64 FileOffset;
    // Enhanced file system pointer
    NvEnhancedFileSystem* pEnhancedFileSystem;
    // Crypto Hash Handle
    NvCryptoHashAlgoHandle hCryptoHashAlgo;
    // Crypto Cipher Handle
    NvCryptoCipherAlgoHandle hCryptoCipherAlgo;
    // Internal file cache buffer pointer
    NvU8* pFileCachedBuffer;
    // File name
    char* pFileName;
    // Current Read/Write Sector number
    NvU32 SectorNum;
    // Maintains file open mode
    NvU32 OpenMode;
    // Number of bytes in file cache
    NvU32 CachedBufferSize;
    // Number of bytes in sector
    NvU32 BytesPerSector;
    // Hash block size
    NvU32 HashBlockSize;
    // Crypto Cipher block size
    NvU32 CipherBlockSize;
    // Stores Crypto cipher algoritham type.
    NvCryptoCipherAlgoType CipherAlgoType;
    /// parameters for cipher algorithm
    NvCryptoCipherAlgoParams CipherAlgoParams;
    // Flag to indicate whether the buffer in Hash processing
    // conatins first crypto block or not
    NvBool HashFirstBlock;
    // Flag to indicate whether the buffer in Cipher processing
    // conatins first crypto block or not
    NvBool CipherFirstBlock;
    // Indicates whether Cipher Algoritham is set or not
    NvBool CipherAlgoSet;
    // Indicates whether Hash Algoritham is set or not
    NvBool HashAlgoSet;
    // Indicates whether current Hash value is computed or not
    // when a file is in read mode
    NvBool IsCurrentHashAvailable;
}NvEnhancedFileSystemFile;


typedef struct NvEnhancedFSFileNameListRec
{
    char *FilePath;
    struct NvEnhancedFSFileNameListRec* pNext;
}NvEnhancedFSFileNameList;

/*
 * Global Variables
 */

//This is the head of the single linked list of all the open files
static NvEnhancedFSFileNameList* s_pNvEnhancedFSFileNameList = NULL;

//Mutex to provide exclusion while accessing file system functions
static NvOsMutexHandle s_NvEnhancedFSFileMutex;
static NvOsMutexHandle s_NvEnhancedFSMutex;

/*
 * File system mount reference count.
 * It is incremented in every Enhanced file system mount and decremented in
 * unmount. 
 * File System will be deinitialized only when gs_FSMountCount is zero
 */

static NvU32 gs_FSMountCount = 0;

/**
 * AddToNvEnhancedFSList()
 *
 * Adds FileName to the FileName list
 *
 * @ param pFileName pointer to file name to be addded
 *
 * @ retval NvSuccess if success
 * @ retval NvError_InvalidParameter if invalid name
 * @ retval NvError_InsufficientMemory if memory alocation fails
 */

static NvError AddToNvEnhancedFSList(const char *pFileName)
{
    NvError e = NvSuccess;
    NvEnhancedFSFileNameList* pNode = NULL;
    NvU32 SizeOfString = 0;

    CHECK_PARAM(pFileName)
    pNode = NvOsAlloc(sizeof(NvEnhancedFSFileNameList));
    CHECK_MEM(pNode);

    SizeOfString = NvOsStrlen(pFileName);
    pNode->FilePath = NULL;
    pNode->pNext = NULL;
    pNode->FilePath = NvOsAlloc(SizeOfString + 1);
    CHECK_MEM(pNode->FilePath);
    NvOsStrncpy(pNode->FilePath, pFileName, SizeOfString);
    pNode->FilePath[SizeOfString] = '\0';

    if (s_pNvEnhancedFSFileNameList == NULL)
    {
        s_pNvEnhancedFSFileNameList = pNode;
    }
    else
    {
        pNode->pNext = s_pNvEnhancedFSFileNameList;
        s_pNvEnhancedFSFileNameList = pNode;
    }
    return e;

fail:
    if (pNode)
    {
        if (pNode->FilePath)
        {
            NvOsFree(pNode->FilePath);
        }
        NvOsFree(pNode);
    }
    return e;
}

/**
 * RemoveFromNvEnhancedFSList()
 *
 * removes the node from the cache FS list which contains the sent file name
 *
 * @param pFileName filename to be removed
 *
 * @retval NvSuccess if success
 * @retval NvError_InvalidParameter if invalid filename or File list is NULL
 */

static NvError RemoveFromNvEnhancedFSList(const char *pFileName)
{
    NvError Err = NvSuccess;
    NvEnhancedFSFileNameList* pCurNode = NULL;
    NvEnhancedFSFileNameList* pPrevNode = NULL;
    CHECK_PARAM(s_pNvEnhancedFSFileNameList);
    CHECK_PARAM(pFileName);

    pCurNode = s_pNvEnhancedFSFileNameList;
    // Check whether the current node has same file name or not.
    // If yes, then break the loop
    while ((pCurNode != NULL) &&
        (NvOsStrcmp(pCurNode->FilePath, pFileName)))
    {
        // Requested file is not found yet
        pPrevNode = pCurNode;
        pCurNode = pCurNode->pNext;
    }

    // If pCurNode is null, it means file name not found
    // return error from here
    CHECK_PARAM(pCurNode);
    if (pPrevNode)
    {
        pPrevNode->pNext = pCurNode->pNext;
    }
    else
    {
        // if pCurNote is the only element, then its pCurNode->pNext
        // is always NULL. Or else it contains next node address
        s_pNvEnhancedFSFileNameList = pCurNode->pNext;
    }
    NvOsFree(pCurNode->FilePath);
    NvOsFree(pCurNode);

    return Err;
}

/**
 * InNvEnhancedFSList()
 *
 * Checks if a given path exist in File name list or not
 *
 * @param pSearchPathString string to be checked in FS list
 *
 * @retval NV_TRUE if name exist in FS list
 * @retval NV_FALSE if name does not exist in FS list
 */

static NvBool InNvEnhancedFSList(const char *pSearchPathString)
{
    NvEnhancedFSFileNameList* pFileNameList = NULL;
    NV_ASSERT(pSearchPathString);
    if ((pSearchPathString == NULL) || (s_pNvEnhancedFSFileNameList == NULL))
    {
       return NV_FALSE;
    }
    else
    {
        pFileNameList = s_pNvEnhancedFSFileNameList;
        // Check whether the current node has same file name or not.
        // If yes, then break the loop
        while ((pFileNameList != NULL) &&
            (NvOsStrcmp(pFileNameList->FilePath, pSearchPathString)))
        {
                pFileNameList = pFileNameList->pNext;
        }
    }
    return (pFileNameList == NULL)? NV_FALSE:NV_TRUE;
}


/**
 * GetCurrentHash()
 *
 * Computes Hash value for the given File
 * It reads data from device and does hash processing.
 * The hash processing must be done in multiple of crypto hash block size.
 * For last crypto hash block, the padding data will be copied from header
 * (which was computed in file close).
 *
 * @param pFile pointer to NvEnhancedFileSystemFile
 * @Param pHashValue buffer pointer to hold hash value.
 *
 * @retval NvError_Success No Error
 * @retval NvError_BadParameter Illegal data buffer address (NULL)
 * @retval NvError_InvalidSize Hash computation error
 *
 */

static NvError
GetCurrentHash(
    NvEnhancedFileSystemFile* pFile,
    void* pHashValue)
{
    NvError e = NvSuccess;
    NvCryptoHashAlgoHandle hCryptoHashAlgo = NULL;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU8* pPageBuffer = NULL;
    NvU8* pTempBuff = NULL;
    NvPartitionHandle hPartition;
    NvDdkBlockDevHandle hBlockDeviceDriver;
    NvS64 FileSize = 0;
    NvU32 PageSize = 0;
    NvU32 CurrentPageNum = 0;
    NvU32 i = 0;
    NvU32 NoOfPages = 0;
    NvU32 NoOfCryptoHashBlks = 0;
    NvU32 CryptoBlkSize = 0;
    NvU32 HashSize = NVCRYPTO_CIPHER_AES_BLOCK_BYTES;
    NvBool FirstBlock = NV_TRUE;

    // Validate file parameters
    CHECK_PARAM(pFile);
    CHECK_PARAM(pFile->FileHeader.FileSize);
    CHECK_PARAM(pFile->hCryptoHashAlgo);
    CHECK_PARAM(pFile->pEnhancedFileSystem->hBlkDevHandle);
    CHECK_PARAM(pFile->pEnhancedFileSystem->hPartition);
    CHECK_PARAM(pFile->HashBlockSize);
    CHECK_PARAM(pHashValue);

    hBlockDeviceDriver =
        pFile->pEnhancedFileSystem->hBlkDevHandle;
    hPartition =
        pFile->pEnhancedFileSystem->hPartition;
    hCryptoHashAlgo = pFile->hCryptoHashAlgo;
    CryptoBlkSize = pFile->HashBlockSize;
    FileSize = pFile->FileHeader.FileSize;
    hBlockDeviceDriver->NvDdkBlockDevGetDeviceInfo(
                    hBlockDeviceDriver,
                    &BlockDevInfo);
    if (!BlockDevInfo.TotalBlocks)
    {
        e = NvError_DeviceNotFound;
        NV_DEBUG_PRINTF(("EFS GetCurrentHash: Either device is not "
                    " present or its not responding. File open failed \n"));
        goto fail;
    }
    PageSize = BlockDevInfo.BytesPerSector;
    pPageBuffer = NvOsAlloc(PageSize + (2 * NVCRYPTO_CIPHER_AES_BLOCK_BYTES));
    CHECK_MEM(pPageBuffer);
    pTempBuff = pPageBuffer;
    NoOfPages = (((NvU32)FileSize + (PageSize -1)) / PageSize);
    CurrentPageNum = (NvU32)hPartition->StartLogicalSectorAddress;

    // Read untill last but one page
    for (i = 0; i < (NoOfPages - 1); i++)
    {
        e = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                hBlockDeviceDriver,
                CurrentPageNum++,
                pPageBuffer,
                1);
        if (e != NvSuccess)
        {
            e = NvError_FileReadFailed;
            NV_DEBUG_PRINTF(("EFS GetCurrentHash: Device driver Read "
                                    "Failed. File Read Failed \n"));
            goto fail;
        }
        // process data
        e = hCryptoHashAlgo->ProcessBlocks(
                                        hCryptoHashAlgo,
                                        PageSize,
                                        pPageBuffer,
                                        FirstBlock,
                                        NV_FALSE);
        if (e != NvSuccess)
        {
            NV_DEBUG_PRINTF(("EFS GetCurrentHash: Hash processing failed"
                                            "for beginning blocks.\n"));
            goto fail;
        }
        FileSize -= PageSize;
        FirstBlock = NV_FALSE;
    }

    // Read last page and process it
    e = hBlockDeviceDriver->NvDdkBlockDevReadSector(
            hBlockDeviceDriver,
            CurrentPageNum,
            pPageBuffer,
            1);
    if (e != NvSuccess)
    {
        e = NvError_FileReadFailed;
        NV_DEBUG_PRINTF(("EFS GetCurrentHash: Device driver Read "
                                "Failed for last page \n"));
        goto fail;
    }

    // Process all the late page crypto blocks except last block
    NoOfCryptoHashBlks =
        (((NvU32)FileSize + (CryptoBlkSize -1)) / CryptoBlkSize);

    if (--NoOfCryptoHashBlks)
    {
        e = hCryptoHashAlgo->ProcessBlocks(
                                        hCryptoHashAlgo,
                                        NoOfCryptoHashBlks * CryptoBlkSize,
                                        pPageBuffer,
                                        FirstBlock,
                                        NV_FALSE);
        if (e != NvSuccess)
        {
            NV_DEBUG_PRINTF(("EFS GetCurrentHash: Hash Process Block "
                                    "Failed for last page data. \n"));
            goto fail;
        }
        FirstBlock = NV_FALSE;
        FileSize -= (CryptoBlkSize * NoOfCryptoHashBlks);
        pTempBuff = pPageBuffer + (CryptoBlkSize * NoOfCryptoHashBlks);
    }

    // Process the last block after appending the padding
    if (pFile->FileHeader.HashPaddingSize)
    {
        // Copy the the padding data from header 
        NvOsMemcpy(
            pTempBuff + FileSize,
            pFile->FileHeader.HashPaddingData,
            pFile->FileHeader.HashPaddingSize);

        FileSize += pFile->FileHeader.HashPaddingSize;
    }

    // process the last crypto block. 
    e = hCryptoHashAlgo->ProcessBlocks(
                                    hCryptoHashAlgo,
                                    (NvU32)FileSize,
                                    pTempBuff,
                                    FirstBlock,
                                    NV_TRUE);

    if (e != NvSuccess)
    {
        NV_DEBUG_PRINTF(("EFS GetCurrentHash: Hash Process Block "
                            "Failed for last block of the file data. \n"));
        goto fail;
    }

    // Now get the value
    e = hCryptoHashAlgo->QueryHash(
                hCryptoHashAlgo,
                &HashSize,
                pHashValue);
    if (e != NvSuccess)
    {
        NV_DEBUG_PRINTF(("EFS GetCurrentHash: QueryHash "
                                "Failed. \n"));
        goto fail;
    }

fail:
    if (pPageBuffer)
    {
        NvOsFree(pPageBuffer);
    }
    return e;
}

/**
 * EncryptAndSignBlocks()
 *
 * Helper function to encrypt and sign data
 *
 * @param pNvEnhancedFileSystemFile pointer to NvEnhancedFileSystemFile
 * @Param NumBytes Number of bytes in buffer.
 * @Param pBuffer pointer to buffer
 * @Param IsLastHashBlock Indicates whether the buffer contains last Hash 
 *              block or not
 * @Param IsLastCiperBlock Indicates whether the buffer contains last Cipher 
 *              block or not
 *
 * @retval NvError_Success No Error
 * @retval NvError_BadParameter Illegal data buffer address (NULL) or Number of 
 *      bytes is zero or pNvEnhancedFileSystemFile is Illegal
 *
 */

static NvError
EncryptAndSignBlocks(
NvEnhancedFileSystemFile * pNvEnhancedFileSystemFile,
    NvU32 NumBytes,
    void * pBuffer,
    NvBool IsLastHashBlock,
    NvBool IsLastCiperBlock)
{
    NvError Err = NvSuccess;
    NvCryptoHashAlgoHandle hCryptoHashAlgo = NULL;
    NvCryptoCipherAlgoHandle hCryptoCipherAlgo = NULL;
    NvEnhancedFSHeader *pFileHdr = NULL;
    NvU8 *pTempBuff = NULL;
    NvU32 PayloadSize = 0;
    NvU32 PaddingSize = 0;
    NvU32 BytesPerSector =0;

    CHECK_PARAM(pNvEnhancedFileSystemFile);
    CHECK_PARAM(NumBytes);
    CHECK_PARAM(pBuffer);

    hCryptoHashAlgo =
        pNvEnhancedFileSystemFile->hCryptoHashAlgo;
    hCryptoCipherAlgo =
        pNvEnhancedFileSystemFile->hCryptoCipherAlgo;
    BytesPerSector = pNvEnhancedFileSystemFile->BytesPerSector;

    // If CipherAlgoritham is specified Encrypt buffer
    if (hCryptoCipherAlgo)
    {
        if (!IsLastCiperBlock)
        {
            // It is not last block. So process directly with out computing
            // padding
            if (pNvEnhancedFileSystemFile->CipherAlgoSet && hCryptoCipherAlgo)
            {
                Err = hCryptoCipherAlgo->ProcessBlocks(
                            hCryptoCipherAlgo,
                            NumBytes,
                            pBuffer,
                            pBuffer,
                            pNvEnhancedFileSystemFile->CipherFirstBlock,
                            IsLastCiperBlock);
                if (Err != NvSuccess)
                {
                    Err = NvError_InvalidState;
                    goto ReturnStatus;
                }
                pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
            }
        }
        else
        {
            // Last block exists. Compute padding bytes
            pTempBuff = pBuffer;
            pFileHdr = &pNvEnhancedFileSystemFile->FileHeader;

            // Calculate padding size and padding bytes for last block
            // by calling QueryPaddingByPayloadSize API.

            PayloadSize =
                ((NumBytes / pNvEnhancedFileSystemFile->CipherBlockSize) *
                    pNvEnhancedFileSystemFile->CipherBlockSize);
            PayloadSize = NumBytes - PayloadSize;

            // Padding size is the numer of bytes available in buffer
            // If buffer size is less than the required padding size,
            // QueryPaddingByPayloadSize returns error.
            // QueryPaddingByPayloadSize updates padding size
            // in PaddingSize variable
            PaddingSize = (BytesPerSector -
                pNvEnhancedFileSystemFile->CachedBufferSize +
                (2 * NVCRYPTO_CIPHER_AES_BLOCK_BYTES));

            // Get the padding data
            Err = hCryptoCipherAlgo->QueryPaddingByPayloadSize(
                        hCryptoCipherAlgo,
                        PayloadSize,
                        &PaddingSize,
                        pTempBuff + NumBytes);
            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF((" EFS Crypto Cipher:"
                    " QueryPaddingByPayloadSize failed. \n"));
                Err = NvError_InvalidState;
            }

            // Process last crypto cipher block
            Err = hCryptoCipherAlgo->ProcessBlocks(
                        hCryptoCipherAlgo,
                        (NumBytes + PaddingSize),
                        pTempBuff,
                        pTempBuff,
                        pNvEnhancedFileSystemFile->CipherFirstBlock,
                        NV_TRUE);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF((" EFS Crypto Cipher: QProcessBlocks"
                    " failed. \n"));
                Err = NvError_InvalidState;
            }
            pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;

            // Store padding data in header. This is again used in
            // decrypting when the file is opened in read mode
            if (PaddingSize)
            {
                // Copy padded data in header
                NvOsMemcpy(
                    pFileHdr->CipherPaddingData,
                    pTempBuff + NumBytes,
                    PaddingSize);
            }

            pFileHdr->CipherPaddingSize = PaddingSize;
            pFileHdr->IsFileEncrypted = NV_TRUE;
        }
    }

    // If HashAlgoritham is specified then compute hash
    if (hCryptoHashAlgo)
    {
        if (!IsLastHashBlock)
        {
            // It is not last block. So process directly with out calculating
            // padding
            if (pNvEnhancedFileSystemFile->HashAlgoSet
            && hCryptoHashAlgo)
            {
                Err = hCryptoHashAlgo->ProcessBlocks(
                            hCryptoHashAlgo,
                            NumBytes,
                            pBuffer,
                            pNvEnhancedFileSystemFile->HashFirstBlock,
                            IsLastHashBlock);
                if (Err != NvSuccess)
                {
                    Err = NvError_InvalidState;
                    goto ReturnStatus;
                }
                pNvEnhancedFileSystemFile->HashFirstBlock = NV_FALSE;
            }
        }
        else
        {
            // Last block exists. Compute padding bytes
            pTempBuff = pBuffer;
            pFileHdr = &pNvEnhancedFileSystemFile->FileHeader;

            // To Process last hash block, first calculate Padding size and
            // padding bytes.by calling QueryPaddingByPayloadSize API.

            // Here payload size = Number of bytes to fill the last crypto block
            PayloadSize =
                ((NumBytes / pNvEnhancedFileSystemFile->HashBlockSize) *
                    pNvEnhancedFileSystemFile->HashBlockSize);
            PayloadSize = NumBytes - PayloadSize;

            PaddingSize = (BytesPerSector - NumBytes +
                (2 * NVCRYPTO_CIPHER_AES_BLOCK_BYTES));

            // Get the padding data
            Err = hCryptoHashAlgo->QueryPaddingByPayloadSize(
                        hCryptoHashAlgo,
                        PayloadSize,
                        &PaddingSize,
                        pTempBuff + NumBytes);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF((" EFS Hashing:"
                    " QueryPaddingByPayloadSize failed. \n"));
                Err = NvError_InvalidState;
            }

            // Process last crypto hash block
            Err = hCryptoHashAlgo->ProcessBlocks(
                        hCryptoHashAlgo,
                        (NumBytes + PaddingSize),
                        pTempBuff,
                        pNvEnhancedFileSystemFile->HashFirstBlock,
                        NV_TRUE);
            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF((" EFS Hashing:"
                    " QProcessBlocks failed. \n"));
                Err = NvError_InvalidState;
            }
            pNvEnhancedFileSystemFile->HashFirstBlock = NV_FALSE;
            // Store padding data in header. This is again used in
            // computing hash when the file is opened in read mode
            if (PaddingSize)
            {
                // Copy padded data in header
                NvOsMemcpy(
                    pFileHdr->HashPaddingData,
                    pTempBuff + NumBytes,
                    PaddingSize);
            }

            pFileHdr->HashPaddingSize = PaddingSize;
            pFileHdr->HashSize = NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

            Err = hCryptoHashAlgo->QueryHash(
                        hCryptoHashAlgo,
                        &pFileHdr->HashSize,
                        pFileHdr->Hash);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EFS Hashing: Crypto "
                    " QueryHash failed \n"));
                Err = NvError_InvalidState;
            }
            pFileHdr->IsHashPerformed = NV_TRUE;
        }
    }

ReturnStatus:
    return Err;

}

/**
 * DecryptData()
 *
 * Helper function to Decrypt given input buffer
 *
 * @param pNvEnhancedFileSystemFile pointer to NvEnhancedFileSystemFile
 * @Param NumBytes Number of bytes in buffer.
 * @Param pBuffer pointer to buffer
 * @Param IsFirstCiperBlock Indicates whether the buffer contains First Cipher
 *              block or not
 * @Param IsLastCiperBlock Indicates whether the buffer contains last Cipher 
 *              block or not
 *
 * @retval NvError_Success No Error
 * @retval NvError_BadParameter Illegal data buffer address (NULL) or Number of 
 *      bytes is zero or pNvEnhancedFileSystemFile is Illegal
 *
 */

static NvError
DecryptData(
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile,
    NvU32 NumBytes,
    void * pBuffer,
    NvBool IsFirstCiperBlock,
    NvBool IsLastCiperBlock)
{
    NvError Err = NvSuccess;
    NvU8* pTempBuff = NULL;
    NvCryptoCipherAlgoHandle hCryptoCipherAlgo = NULL;
    NvU32 PaddingSize = 0;
    NvU32 CipherBlkSize = 0;
    NvU32 NoOfBlocks = 0;
    NvU32 ResidualBytes = 0;

    CHECK_PARAM(pNvEnhancedFileSystemFile);
    CHECK_PARAM(NumBytes);
    CHECK_PARAM(pBuffer);

    hCryptoCipherAlgo = pNvEnhancedFileSystemFile->hCryptoCipherAlgo;
    CHECK_PARAM(hCryptoCipherAlgo);

    CipherBlkSize = pNvEnhancedFileSystemFile->CipherBlockSize;
    PaddingSize = pNvEnhancedFileSystemFile->FileHeader.CipherPaddingSize;
    pTempBuff = (NvU8 *)pBuffer;

    // If it is not last block, process directly
    if (!IsLastCiperBlock)
    {
        Err = hCryptoCipherAlgo->ProcessBlocks(
                    hCryptoCipherAlgo,
                    NumBytes,
                    pTempBuff,
                    pTempBuff,
                    pNvEnhancedFileSystemFile->CipherFirstBlock,
                    NV_FALSE);
        if (Err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("EFS NvFileSystemFileClose: Crypto Hash "
                " Process Blocks failed for beginning blocks. \n"));
            goto ReturnStatus;
        }
        pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
    }
    else
    {
        // If it is last block
        // Here payload size = Number of bytes in last crypto block
        NoOfBlocks = (NumBytes / CipherBlkSize);
        ResidualBytes = (NumBytes % CipherBlkSize);
        
        if ((NoOfBlocks) && (!ResidualBytes))
        {
            NoOfBlocks--;
        }
        if (NoOfBlocks)
        {
            Err = hCryptoCipherAlgo->ProcessBlocks(
                        hCryptoCipherAlgo,
                        NoOfBlocks * CipherBlkSize,
                        pTempBuff,
                        pTempBuff,
                        pNvEnhancedFileSystemFile->CipherFirstBlock,
                        NV_FALSE);
            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EFS NvFileSystemFileClose: Crypto Hash "
                    " Process Blocks failed for beginning blocks. \n"));
                goto ReturnStatus;
            }
            pTempBuff += (NoOfBlocks * CipherBlkSize);
            pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
        }

        if (!ResidualBytes)
        {
            ResidualBytes = CipherBlkSize;
        }

        if (PaddingSize)
        {
            // Copy padded data from header
            NvOsMemcpy(
                pTempBuff + ResidualBytes,
                pNvEnhancedFileSystemFile->FileHeader.CipherPaddingData,
                PaddingSize);
        }

        Err = hCryptoCipherAlgo->ProcessBlocks(
                     hCryptoCipherAlgo,
                     ResidualBytes + PaddingSize,
                     pTempBuff,
                     pTempBuff,
                     pNvEnhancedFileSystemFile->CipherFirstBlock,
                     NV_TRUE);

        if (Err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("EFS NvFileSystemFileClose: Crypto Hash "
            " Process Blocks failed for beginning blocks. \n"));
            goto ReturnStatus;
        }
        pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
    }
ReturnStatus:
    return Err;
}

/**
 * DecryptData()
 *
 * Helper function to read data from media and to decrypt it
 *
 * @param pNvEnhancedFileSystemFile pointer to NvEnhancedFileSystemFile
 * @Param SectorNumber Indicates the sector number upto where data has to be
 *      read and decrypted from the current sector number.
 *      If this is less then the current sector number(in case of backward
 *      seek), then cipher algoritham is deinitialized and initialized.
 *      And the data will be read from beginning of the file
 *
 * @Param FilePosition New file position. If it is zero then data will not be
 *      from media
 *
 * @retval NvError_Success No Error
 * @retval NvError_BadParameter Illegal data buffer address (NULL) or Number of 
 *      bytes is zero or pNvEnhancedFileSystemFile is Illegal
 *
 */

static NvError
ReadAndDecrypt(
    NvEnhancedFileSystemFile * pNvEnhancedFileSystemFile,
    NvU32 SectorNumber,
    NvS64 FilePosition)
{
    NvError e = NvSuccess;
    NvU8* pFileBuffer = NULL;
    NvEnhancedFSHeader *pFileHdr = NULL;
    NvPartitionHandle hPartition;
    NvU32 StartSectorNumber = 0;
    NvCryptoCipherAlgoHandle hCryptoCipherAlgo = NULL;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvCryptoCipherAlgoType CipherAlgoType;

    NvU32 BytesPerSector = 0;
    NvU64 FileSize = 0;
    NvU32 LastSector = 0;
    NvU32 LastSectorBytes = 0;

    CHECK_PARAM(pNvEnhancedFileSystemFile);
    CHECK_PARAM(pNvEnhancedFileSystemFile->hCryptoCipherAlgo);
    CHECK_PARAM(FilePosition >= 0);

    pFileHdr = &pNvEnhancedFileSystemFile->FileHeader;
    hBlockDeviceDriver =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hBlkDevHandle;

    hPartition =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hPartition;

    BytesPerSector = pNvEnhancedFileSystemFile->BytesPerSector;
    pFileBuffer = pNvEnhancedFileSystemFile->pFileCachedBuffer;
    hCryptoCipherAlgo = pNvEnhancedFileSystemFile->hCryptoCipherAlgo;
    CipherAlgoType = pNvEnhancedFileSystemFile->CipherAlgoType;
    FileSize = pNvEnhancedFileSystemFile->FileHeader.FileSize;

    if ((SectorNumber +1) == pNvEnhancedFileSystemFile->SectorNum)
    {
        // It means sector is already in cache. Return from here
        return NvSuccess;
    }

    // Detemine whether it is a forward seek or a backward seek
    if ((SectorNumber +1) < pNvEnhancedFileSystemFile->SectorNum)
    {
        // It is backward seek
        // Deinitialize crypto cipher algo
        hCryptoCipherAlgo->ReleaseAlgorithm(hCryptoCipherAlgo);
        pNvEnhancedFileSystemFile->hCryptoCipherAlgo = NULL;

        // Initialize Cipher algo again
        NV_CHECK_ERROR_CLEANUP(
            NvCryptoCipherSelectAlgorithm(
                CipherAlgoType,
                &pNvEnhancedFileSystemFile->hCryptoCipherAlgo)
        );

        hCryptoCipherAlgo = pNvEnhancedFileSystemFile->hCryptoCipherAlgo;

        NV_CHECK_ERROR_CLEANUP(
            hCryptoCipherAlgo->SetAlgoParams(
                hCryptoCipherAlgo,
                &pNvEnhancedFileSystemFile->CipherAlgoParams)
        );

        pNvEnhancedFileSystemFile->CipherFirstBlock = NV_TRUE;
        StartSectorNumber = (NvU32)hPartition->StartLogicalSectorAddress;
    }
    else
    {
        // It is forward seek
        StartSectorNumber = pNvEnhancedFileSystemFile->SectorNum;
    }

    if (!FilePosition)
    {
        // It is beginning of the file. No need to read data from media
        pNvEnhancedFileSystemFile->SectorNum =
            (NvU32)hPartition->StartLogicalSectorAddress;

        return e;
    }

    // compute last sector number
    LastSector = ((((NvU32)FileSize + BytesPerSector -1) / BytesPerSector) +
                        (NvU32)hPartition->StartLogicalSectorAddress) -1;

    // Compute number of bytes in last sector
    LastSectorBytes = ((NvU32)FileSize % BytesPerSector) ?
            ((NvU32)FileSize % BytesPerSector): BytesPerSector;

    do
    {
        // Read data from media
        e = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                    hBlockDeviceDriver,
                    StartSectorNumber,
                    pFileBuffer,
                    1);

        if (e != NvSuccess)
        {
            e = NvError_FileReadFailed;
            NV_DEBUG_PRINTF(("Enhanced FS Read: Device driver Read "
            "Failed. File Read Failed \n"));
            goto fail;
        }

        if (StartSectorNumber != LastSector)
        {
            // It is not last sector so decrpt data with out adding padding
            NV_CHECK_ERROR_CLEANUP(hCryptoCipherAlgo->ProcessBlocks(
                        hCryptoCipherAlgo,
                        BytesPerSector,
                        pFileBuffer,
                        pFileBuffer,
                        pNvEnhancedFileSystemFile->CipherFirstBlock,
                        NV_FALSE)
            );

            pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
        }
        else
        {
            // It is last sector. Add padding
            NvOsMemcpy(
                pFileBuffer + LastSectorBytes,
                pFileHdr->CipherPaddingData,
                pFileHdr->CipherPaddingSize);

            NV_CHECK_ERROR_CLEANUP(hCryptoCipherAlgo->ProcessBlocks(
                        hCryptoCipherAlgo,
                        LastSectorBytes + pFileHdr->CipherPaddingSize,
                        pFileBuffer,
                        pFileBuffer,
                        pNvEnhancedFileSystemFile->CipherFirstBlock,
                        NV_TRUE)
            );

            pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
        }
        StartSectorNumber++;
    }while (StartSectorNumber <= SectorNumber);

    pNvEnhancedFileSystemFile->SectorNum = StartSectorNumber;

fail:
    return e;
}

NvError NvEnhancedFileSystemInit(void)
{
    NvError e = NvSuccess;
    if (!s_NvEnhancedFSFileMutex)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_NvEnhancedFSFileMutex));
    }
    if (!s_NvEnhancedFSMutex)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_NvEnhancedFSMutex));
    }
    return e;

fail:
    if (s_NvEnhancedFSFileMutex)
    {
        NvOsMutexDestroy(s_NvEnhancedFSFileMutex);
    }
    s_NvEnhancedFSFileMutex = NULL;
    s_NvEnhancedFSMutex = NULL;
    return e;
}

/*
    This will DeInit Enhanced file system. 
    It will deinit only when mounted file system count is zero
*/
void NvEnhancedFileSystemDeinit(void)
{
    // DeInit File system only when mounted file system count is zero
    if (gs_FSMountCount == 0)
    {
        if (s_NvEnhancedFSFileMutex)
        {
            NvOsMutexDestroy(s_NvEnhancedFSFileMutex);
            s_NvEnhancedFSFileMutex = NULL;
        }

        if (s_NvEnhancedFSMutex)
        {
            NvOsMutexDestroy(s_NvEnhancedFSMutex);
            s_NvEnhancedFSMutex = NULL;
        }
    }

}

/*
    This will unmount Enhanced file system. It frees memory for Enhanced file
    system only when the open file count is zero. It returns AccessDeinied if
    open reference count is not zero. Client has to call FileClose before
    Unmount
*/
static NvError NvFileSystemUnmount(NvFileSystemHandle hFileSystem)
{
    NvError Err = NvSuccess;
    NvEnhancedFileSystem *pNvEnhancedFileSystem = NULL;
    CHECK_PARAM(hFileSystem);
    NvOsMutexLock(s_NvEnhancedFSMutex);
    pNvEnhancedFileSystem = (NvEnhancedFileSystem *)hFileSystem;
    if (pNvEnhancedFileSystem->RefFileCount !=0)
    {
        Err = NvError_AccessDenied;
        NV_DEBUG_PRINTF(("NvFileSystemUnmount: Error One or more. "
                    "Files still open but Unmount issued. \n"));
    }
    else
    {
        NvOsFree(hFileSystem);
        hFileSystem = NULL;
    }
    gs_FSMountCount--;
    NvOsMutexUnlock(s_NvEnhancedFSMutex);
    return Err;
}

static NvError
NvFileSystemFileQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileStat *pStat)
{
    NvError Err = NvSuccess;
    NvDdkBlockDevInfo BlockDevInfo;
    NvEnhancedFileSystem *pNvEnhancedFileSystem = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvPartitionHandle hPartHandle = NULL;
    NvU32 PageSize = 0;

    CHECK_PARAM(hFileSystem);
    CHECK_PARAM(pStat);
    pStat->Size = 0;
    pNvEnhancedFileSystem = (NvEnhancedFileSystem *)hFileSystem;
    hBlkDev = pNvEnhancedFileSystem->hBlkDevHandle;
    hPartHandle = pNvEnhancedFileSystem->hPartition;
    hBlkDev->NvDdkBlockDevGetDeviceInfo(hBlkDev, &BlockDevInfo);
    if (!BlockDevInfo.TotalBlocks)
    {
        Err = NvError_DeviceNotFound;
        pStat->Size = 0;
        NV_DEBUG_PRINTF(("EFS NvFileSystemFileQueryStat: Either device"
                " is not present or its not responding. \n"));
        goto ReturnStatus;
    }

    PageSize = BlockDevInfo.BytesPerSector;
    pStat->Size = ((NvU32)(hPartHandle->NumLogicalSectors) * PageSize);

ReturnStatus:

    return Err;
}

static NvError
NvFileSystemFileSystemQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileSystemStat *pStat)
{
    return NvError_NotSupported;
}

static NvError NvFileSystemFormat(NvFileSystemHandle hFileSystem,
        NvU64 StartLogicalSector, NvU64 NumLogicalSectors,
        NvU32 PTendSector, NvBool IsPTpartition)
{
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvEnhancedFileSystem *pNvEnhFileSystem = NULL;
    NvError Err = NvSuccess;
    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs ErasePartArgs;

    CHECK_PARAM(hFileSystem);
    NvOsMutexLock(s_NvEnhancedFSFileMutex);

    pNvEnhFileSystem = (NvEnhancedFileSystem *)hFileSystem;
    // Call block driver partition Erase API
    hBlkDev = pNvEnhFileSystem->hBlkDevHandle;
    ErasePartArgs.StartLogicalSector = (NvU32)StartLogicalSector;
    ErasePartArgs.NumberOfLogicalSectors = (NvU32)NumLogicalSectors;
    ErasePartArgs.PTendSector = PTendSector;
    ErasePartArgs.IsPTpartition = IsPTpartition;
    ErasePartArgs.IsTrimErase = NV_FALSE;
    ErasePartArgs.IsSecureErase = NV_FALSE;
    Err = hBlkDev->NvDdkBlockDevIoctl(hBlkDev, 
            NvDdkBlockDevIoctlType_ErasePartition, 
            sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs), 0,
            &ErasePartArgs, NULL);

    NvOsMutexUnlock(s_NvEnhancedFSFileMutex);
    return Err;
}

/*
 * It does following things:
 * 1. Frees the resources allocated in Open call.
 * 2. Decrements RefFileCount.
 * 3. Removes file name from open file names list
 * 4. If the file is open in Write mode, Does the following things
 *     a). If there is Data in file cache, it will be encrypted computed
 *     (If Cipher algo is set)
 *     b) Hash is computed (If hash algo is set) and it is stored in
 *     File Header
 *     c) Marks the file data as valid
 *     d) Writes data in cache to device
 *     e) Increments file size with cached buffer size
 *     f) Writes header in last sector
 */
static NvError
NvFileSystemFileClose(
    NvFileSystemFileHandle hFile)
{
    NvError e = NvSuccess;
    NvError Err = NvSuccess;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    NvEnhancedFileSystem *pNvEnhancedFileSystem = NULL;
    NvU8* pTempBuff = NULL;
    NvEnhancedFSHeader * pFSHeader = NULL;
    NvCryptoHashAlgoHandle hCryptoHashAlgo = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvPartitionHandle hPartHandle = NULL;
    NvU32 HeaderSectorNumber = 0;

    CHECK_PARAM(hFile);
    NvOsMutexLock(s_NvEnhancedFSFileMutex);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile *)hFile;
    pNvEnhancedFileSystem = pNvEnhancedFileSystemFile->pEnhancedFileSystem;
    hCryptoHashAlgo = pNvEnhancedFileSystemFile->hCryptoHashAlgo;
    hBlkDev = pNvEnhancedFileSystem->hBlkDevHandle;
    pTempBuff = pNvEnhancedFileSystemFile->pFileCachedBuffer;

    // Check whether file is opened in write mode or not
    if (pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_WRITE)
    {
        pFSHeader = &pNvEnhancedFileSystemFile->FileHeader;

        if (pNvEnhancedFileSystemFile->CachedBufferSize)
        {
             Err = EncryptAndSignBlocks(
                        pNvEnhancedFileSystemFile,
                        pNvEnhancedFileSystemFile->CachedBufferSize,
                        pTempBuff,
                        NV_TRUE,
                        NV_TRUE);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EnhancedFS: Crypto failed in Write \n"));
                Err = NvError_InvalidState;
                goto ReturnStatus;
            }

            // Data valid flag. 0 is valid. 1 is invalid.
            pFSHeader->IsFileDataValid = 0x0;

            // Flush the last sector data to device
            Err = hBlkDev->NvDdkBlockDevWriteSector(
                hBlkDev,
                pNvEnhancedFileSystemFile->SectorNum,
                pTempBuff,
                1);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EFS NvFileSystemFileClose: Device "
                            " driver write failed \n"));
                goto ReturnStatus;
            }

            // Increment file size with cachedBufferSize
            pFSHeader->FileSize +=
                pNvEnhancedFileSystemFile->CachedBufferSize;
        }

        // Write header
        hPartHandle = pNvEnhancedFileSystem->hPartition;
        HeaderSectorNumber =
            (NvU32)(hPartHandle->StartLogicalSectorAddress +
            hPartHandle->NumLogicalSectors -1);

        NvOsMemset(pTempBuff, 0x0, pNvEnhancedFileSystemFile->BytesPerSector);
        NvOsMemcpy(
            pTempBuff,
            &pNvEnhancedFileSystemFile->FileHeader,
            sizeof(NvEnhancedFSHeader));

        // Write header in the last sector of the partition
        Err = hBlkDev->NvDdkBlockDevWriteSector(
            hBlkDev,
            HeaderSectorNumber,
            pTempBuff,
            1);

        if (Err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("EFS NvFileSystemFileClose: Device "
                " Driver error while writing File header \n"));
        }
    }

ReturnStatus:
    e = RemoveFromNvEnhancedFSList(pNvEnhancedFileSystemFile->pFileName);

    if (pNvEnhancedFileSystem->RefFileCount)
    {
        pNvEnhancedFileSystem->RefFileCount--;
    }

    if (pNvEnhancedFileSystemFile->pFileCachedBuffer)
    {
        NvOsFree(pNvEnhancedFileSystemFile->pFileCachedBuffer);
        pNvEnhancedFileSystemFile->pFileCachedBuffer = NULL;
    }

    if (pNvEnhancedFileSystemFile->pFileName)
    {
        NvOsFree(pNvEnhancedFileSystemFile->pFileName);
        pNvEnhancedFileSystemFile->pFileName = NULL;
    }

    if (hCryptoHashAlgo)
    {
        hCryptoHashAlgo->ReleaseAlgorithm(hCryptoHashAlgo);
        pNvEnhancedFileSystemFile->hCryptoHashAlgo = NULL;
    }

    NvOsFree(pNvEnhancedFileSystemFile);
    hFile = NULL;

    if (Err != NvSuccess)
    {
        e = Err;
    }

    NvOsMutexUnlock(s_NvEnhancedFSFileMutex);
    return e;
}

static NvError
NvFileSystemFileRead(
    NvFileSystemFileHandle hFile,
    void *pBuffer,
    NvU32 BytesToRead,
    NvU32 *BytesRead)
{
    NvError Err = NvSuccess;
    NvU64 FileSize = 0;
    NvU32 NumOfPagesToRead = 0;
    NvU32 BytesToCopy = 0;
    NvU32 BytesPerSector = 0;
    NvU32 ResidualLength = 0;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pClientBuffer = (NvU8 *)pBuffer;
    NvU8* pFileBuffer = NULL;
    NvEnhancedFSHeader *pFileHdr = NULL;
    NvPartitionHandle hPartHandle = NULL;

    CHECK_PARAM(hFile);
    CHECK_PARAM(pBuffer);
    CHECK_PARAM(BytesRead);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile*)hFile;
    CHECK_PARAM(pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_READ);

    hBlockDeviceDriver =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hBlkDevHandle;
    BytesPerSector = pNvEnhancedFileSystemFile->BytesPerSector;
    pFileBuffer = pNvEnhancedFileSystemFile->pFileCachedBuffer;
    pFileHdr = &pNvEnhancedFileSystemFile->FileHeader;
    hPartHandle = pNvEnhancedFileSystemFile->pEnhancedFileSystem->hPartition;
    *BytesRead = 0;
    FileSize = pFileHdr->FileSize;

    // Verify File Limit
    if ((pNvEnhancedFileSystemFile->FileOffset + BytesToRead) > FileSize)
    {
        Err = NvError_InvalidSize;
        NV_DEBUG_PRINTF(("Enhanced FS Read: Number of bytes to read exceeds "
                                " file size limit. File Read failed. \n"));
        goto ReturnStatus;
    }
    while (BytesToRead)
    {
        if (pNvEnhancedFileSystemFile->CachedBufferSize)
        {
            BytesToCopy = (BytesToRead <=
                    pNvEnhancedFileSystemFile->CachedBufferSize) ?
                    BytesToRead : pNvEnhancedFileSystemFile->CachedBufferSize;

            // Copy cached buffer into client buffer
            NvOsMemcpy(pClientBuffer, pFileBuffer + (BytesPerSector -
                    pNvEnhancedFileSystemFile->CachedBufferSize), BytesToCopy);

            BytesToRead -= BytesToCopy;
            pClientBuffer += BytesToCopy;
            *BytesRead += BytesToCopy;
            pNvEnhancedFileSystemFile->FileOffset += BytesToCopy;
            pNvEnhancedFileSystemFile->CachedBufferSize -= BytesToCopy;
        }
        else
        {
            // There is no data avaialable in cache. Need to read from Device
            // Calculate number of pages to read from device
            NumOfPagesToRead = (BytesToRead / BytesPerSector);
            ResidualLength = (BytesToRead % BytesPerSector);

            // Decrement NumOfPagesToRead by 1 if ResidualBytes is zero
            // so that last one sector is read seperately in file system
            // cache buffer
            if ((NumOfPagesToRead) && (!ResidualLength))
            {
                NumOfPagesToRead--;
            }

            if (NumOfPagesToRead)
            {
                Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                            hBlockDeviceDriver,
                            pNvEnhancedFileSystemFile->SectorNum,
                            pClientBuffer,
                            NumOfPagesToRead);

                if (Err != NvSuccess)
                {
                    Err = NvError_FileReadFailed;
                    NV_DEBUG_PRINTF(("Enhanced FS Read: Device driver Read "
                                            "Failed. File Read Failed \n"));
                    goto ReturnStatus;
                }

                // If crypto cipher algo specified, decrypt data
                if (pNvEnhancedFileSystemFile->hCryptoCipherAlgo)
                {
                    // Decrypt Data
                    Err = DecryptData(
                                pNvEnhancedFileSystemFile,
                                (NumOfPagesToRead * BytesPerSector),
                                pClientBuffer,
                                pNvEnhancedFileSystemFile->CipherFirstBlock,
                                NV_FALSE);

                    if (Err != NvSuccess)
                    {
                        Err = NvError_FileReadFailed;
                        NV_DEBUG_PRINTF(("Enhanced FS Read: Data decryption "
                            "Failed. File Read Failed \n"));
                        goto ReturnStatus;
                    }
                    pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
                }

                BytesToRead -= NumOfPagesToRead * BytesPerSector;
                pClientBuffer += NumOfPagesToRead * BytesPerSector;
                *BytesRead += NumOfPagesToRead * BytesPerSector;
                pNvEnhancedFileSystemFile->FileOffset += NumOfPagesToRead *
                                                            BytesPerSector;

                pNvEnhancedFileSystemFile->SectorNum += NumOfPagesToRead;
            }

            if (!ResidualLength)
            {
                // ResidualLength is zero, it means NumOfPagesToRead
                // is already decrmented by one in the above if condition.
                // Make ResidualLength size to one sector size.
                ResidualLength = BytesPerSector;
            }

            // Read one sector to fill remaining bytes
            // Determine whether it is this is last sector or not


            Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                        hBlockDeviceDriver,
                        pNvEnhancedFileSystemFile->SectorNum,
                        pFileBuffer,
                        1);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Enhanced FS Read: Device driver Read "
                                    "Failed \n"));
                Err = NvError_FileReadFailed;
                goto ReturnStatus;
            }

            // Decrypt Data
            if (pNvEnhancedFileSystemFile->hCryptoCipherAlgo)
            {
                NvU32 LastSector;
                NvU32 NoOfBytesToProcess = 0;

                // compute last sector number
                LastSector = (((NvU32)FileSize + BytesPerSector - 1) /
                    BytesPerSector) +
                    (NvU32)hPartHandle->StartLogicalSectorAddress - 1;

                // Compute number of bytes to process
                if (pNvEnhancedFileSystemFile->SectorNum == LastSector)
                {
                    // If is is last sector, number of bytes to process is
                    // equal to number bytes in last secotr
                    NoOfBytesToProcess = ((NvU32)FileSize % BytesPerSector);
                }

                if (!NoOfBytesToProcess)
                {
                    NoOfBytesToProcess = BytesPerSector;
                }

                // Decrypt Data
                Err = DecryptData(
                            pNvEnhancedFileSystemFile,
                            NoOfBytesToProcess,
                            pFileBuffer,
                            pNvEnhancedFileSystemFile->CipherFirstBlock,
                            (pNvEnhancedFileSystemFile->SectorNum ==
                            LastSector));

                if (Err != NvSuccess)
                {
                    Err = NvError_FileReadFailed;
                    NV_DEBUG_PRINTF(("Enhanced FS Read: Data decryption "
                                            "Failed. File Read Failed \n"));
                    goto ReturnStatus;
                }
                pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;
            }

            NvOsMemcpy(pClientBuffer, pFileBuffer, ResidualLength);
            BytesToRead -= ResidualLength;
            *BytesRead += ResidualLength;
            pNvEnhancedFileSystemFile->SectorNum++;
            pNvEnhancedFileSystemFile->FileOffset += ResidualLength;
            pNvEnhancedFileSystemFile->CachedBufferSize =
                        BytesPerSector - ResidualLength;
        }
    }

ReturnStatus:
    return Err;
}

/*
 * NvFileSystemFileWrite API.
 *
 * It writes data into media only if data is more than one sector. If the
 * data is less than or equal to one sector then it will be copied into
 * file cache and written to media only in the next write call(in the next 
 * write call also total number to bytes to write should be more 
 * than one sector)  Or it will be written in Close API.
 *
 */
static NvError
NvFileSystemFileWrite(
    NvFileSystemFileHandle hFile,
    const void *pBuffer,
    NvU32 BytesToWrite,
    NvU32 *BytesWritten)
{
    NvError Err = NvSuccess;
    NvU32 NumOfPagesToWrite = 0;
    NvU32 BytesToCopy = 0;
    NvU32 ResidualBytes = 0;
    NvU32 BytesPerSector = 0;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pClientBuffer = (NvU8 *)pBuffer;
    NvU8* pFileBuffer = NULL;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    NvEnhancedFSHeader * pFSHeader = NULL;

    CHECK_PARAM(hFile);
    CHECK_PARAM(pBuffer);
    CHECK_PARAM(BytesWritten);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile*)hFile;

    CHECK_PARAM(pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_WRITE);

    hBlockDeviceDriver =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hBlkDevHandle;

    BytesPerSector = pNvEnhancedFileSystemFile->BytesPerSector;
    pFileBuffer = pNvEnhancedFileSystemFile->pFileCachedBuffer;
    pFSHeader = &pNvEnhancedFileSystemFile->FileHeader;
    *BytesWritten = 0;

    // Verify File limit
    if ((pNvEnhancedFileSystemFile->FileOffset + BytesToWrite) >
            pNvEnhancedFileSystemFile->MaxFileSize)
    {
        Err = NvError_InvalidSize;
        NV_DEBUG_PRINTF(("EnhancedFS Write: Number of bytes to write exceeds "
                "max file size limit. File Write Failed \n"));
        goto StatuReturn;
    }

    while (BytesToWrite)
    {
        BytesToCopy = (BytesToWrite < (BytesPerSector -
                pNvEnhancedFileSystemFile->CachedBufferSize)) ? BytesToWrite :
                (BytesPerSector - pNvEnhancedFileSystemFile->CachedBufferSize);
        // Write data to media only when data is more than one sector
        // If it is less than one sector or equal to one sector, write it in
        // next write call or write it in close API.
        if (BytesToCopy)
        {
            NvOsMemcpy(
                pFileBuffer + pNvEnhancedFileSystemFile->CachedBufferSize,
                pClientBuffer,
                BytesToCopy);

            BytesToWrite -= BytesToCopy;
            pNvEnhancedFileSystemFile->CachedBufferSize += BytesToCopy;
            *BytesWritten += BytesToCopy;
            pNvEnhancedFileSystemFile->FileOffset += BytesToCopy;
            pClientBuffer += BytesToCopy;

            // Data in File cache buffer is written into media only when it is
            // more than one sector size.
            // And it will be written in next while loop
            // or it will be written when File Close is called

            // Here do not increment file size. It will be incremented in 
            // the else part and/or in file close
            continue;
        }
        else
        {
            // If reached here means data to write is more than one sector size

             Err = EncryptAndSignBlocks(
                        pNvEnhancedFileSystemFile,
                        BytesPerSector,
                        pFileBuffer,
                        NV_FALSE,
                        NV_FALSE);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EnhancedFS: Crypto failed in Write \n"));
                Err = NvError_InvalidState;
                goto StatuReturn;
            }

            pNvEnhancedFileSystemFile->HashFirstBlock = NV_FALSE;
            pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;

            Err = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                    hBlockDeviceDriver,
                    pNvEnhancedFileSystemFile->SectorNum,
                    pFileBuffer,
                    1);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EnhancedFS Write: Device driver Write "
                        "Failed. File Write failed. \n"));
                Err = NvError_FileWriteFailed;
                goto StatuReturn;
            }
            pNvEnhancedFileSystemFile->SectorNum++;
            pNvEnhancedFileSystemFile->CachedBufferSize = 0;
            // Increment file size by sector size
            pFSHeader->FileSize += BytesPerSector;

        }
        NumOfPagesToWrite = BytesToWrite / BytesPerSector;
        ResidualBytes = BytesToWrite % BytesPerSector;

        // Always write last page data in file close
        // So decrement NumOfPagesToWrite by 1 if ResidualBytes is zero
        if ((NumOfPagesToWrite) && (!ResidualBytes))
        {
            NumOfPagesToWrite--;
        }

        if (NumOfPagesToWrite)
        {
             Err = EncryptAndSignBlocks(
                        pNvEnhancedFileSystemFile,
                        (BytesPerSector * NumOfPagesToWrite),
                        pClientBuffer,
                        NV_FALSE,
                        NV_FALSE);
            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EnhancedFS: Crypto failed in Write \n"));
                Err = NvError_InvalidState;
                goto StatuReturn;
            }
            Err = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                    hBlockDeviceDriver,
                    pNvEnhancedFileSystemFile->SectorNum,
                    pClientBuffer,
                    NumOfPagesToWrite);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("EnhancedFS Write: Device driver Write "
                                            "Failed. File Write failed. \n"));
                Err = NvError_FileWriteFailed;
                goto StatuReturn;
            }

            BytesToWrite -= NumOfPagesToWrite * BytesPerSector;
            pNvEnhancedFileSystemFile->SectorNum += NumOfPagesToWrite;
            *BytesWritten += NumOfPagesToWrite * BytesPerSector;
            pNvEnhancedFileSystemFile->FileOffset += NumOfPagesToWrite *
                        BytesPerSector;

            pClientBuffer += NumOfPagesToWrite * BytesPerSector;
            pFSHeader->FileSize += NumOfPagesToWrite * BytesPerSector;
        }

        if (!ResidualBytes)
        {
            // ResidualBytes is zero, it means NumOfPagesToWrite
            // is already decrmented by one in the above if condition.
            // Make residual bytes size to one sector size.
            ResidualBytes = BytesPerSector;
        }

        NvOsMemcpy(pFileBuffer, pClientBuffer, ResidualBytes);
        BytesToWrite -= ResidualBytes;
        *BytesWritten += ResidualBytes;
        pNvEnhancedFileSystemFile->FileOffset += ResidualBytes;
        pClientBuffer +=ResidualBytes;
        pNvEnhancedFileSystemFile->CachedBufferSize = ResidualBytes;
        // Do not incrment file size
    }

StatuReturn:
    return Err;
}

/*
 * NvFileSystemFileSeek API
 * It changes the file Current postion depending on
 * number of bytes to seek and position from where to seek
 */
static NvError
NvFileSystemFileSeek(
    NvFileSystemFileHandle hFile,
    NvS64 ByteOffset,
    NvU32 Whence)
{
    NvError e = NvSuccess;
    NvU32 BytesPerSector = 0;
    NvU32 SectorNumber = 0;
    NvU32 FileStartSectorNum = 0;
    NvU64 FileSize = 0;
    NvS64 NewFilePosition = 0;
    NvU32 OffsetInCache = 0;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    NvPartitionHandle hPartition = NULL;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pFileBuffer = NULL;

    CHECK_PARAM(hFile);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile*)hFile;

    hBlockDeviceDriver =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hBlkDevHandle;
    hPartition =
        pNvEnhancedFileSystemFile->pEnhancedFileSystem->hPartition;
    BytesPerSector = pNvEnhancedFileSystemFile->BytesPerSector;
    pFileBuffer = pNvEnhancedFileSystemFile->pFileCachedBuffer;
    FileStartSectorNum = (NvU32)hPartition->StartLogicalSectorAddress;
    FileSize = pNvEnhancedFileSystemFile->FileHeader.FileSize;

    // Check if the file is in read mode
    if (pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_READ)
    {
        // compute new file position depending on the seek start position
        switch (Whence)
        {
            case NvOsSeek_Set:
                // If it is from starting, New file postion should be BytesOffset
                NewFilePosition = ByteOffset;
                break;

            case NvOsSeek_Cur:
                // If it is from current postion, New file postion should be
                // (BytesOffset to + File current position)
                NewFilePosition =
                    (pNvEnhancedFileSystemFile->FileOffset + ByteOffset);
                break;

            case NvOsSeek_End:
                // If it is from End, New file postion should be
                // (BytesOffset + FileSize).
                // Client has to give BytesOffset as zero or less than zero
                NewFilePosition = ((NvS64)FileSize + ByteOffset);
                break;

            default:
                NV_DEBUG_PRINTF(("Enhanced FS Seek: Invalid File Seek "
                                                " start position. \n"));
                NV_ASSERT(0);
                return NvError_BadParameter;
        }

        // Verify File Limit
        if ((NewFilePosition < 0) || (NewFilePosition > (NvS64)FileSize))
        {
            e= NvError_InvalidSize;
            NV_DEBUG_PRINTF(("Enhanced FS Seek: Invalid seek "
                "position \n"));
            goto ReturnStatus;
        }

        // Calculate page number and offset
        SectorNumber = (((NvU32)NewFilePosition / BytesPerSector)
                                + FileStartSectorNum);
        OffsetInCache = ((NvU32)NewFilePosition % BytesPerSector);

        if (pNvEnhancedFileSystemFile->CipherAlgoSet)
        {
            // Read and Decrypt
            if (!((SectorNumber == pNvEnhancedFileSystemFile->SectorNum) &&
                (!OffsetInCache)))
            {
                if ((!OffsetInCache) && (SectorNumber != FileStartSectorNum))
                {
                    // New file position is in sector boundary
                    SectorNumber--;
                }
                NV_CHECK_ERROR(ReadAndDecrypt(
                    pNvEnhancedFileSystemFile,
                    SectorNumber,
                    NewFilePosition)
                );
            }
        }
        else
        {
            if (((SectorNumber + 1) !=
                pNvEnhancedFileSystemFile->SectorNum) && OffsetInCache)
            {
                pNvEnhancedFileSystemFile->SectorNum = SectorNumber;
                NV_CHECK_ERROR(hBlockDeviceDriver->NvDdkBlockDevReadSector(
                        hBlockDeviceDriver,
                        pNvEnhancedFileSystemFile->SectorNum++,
                        pFileBuffer,
                        1)
                );
                pNvEnhancedFileSystemFile->CachedBufferSize =
                    BytesPerSector - OffsetInCache;
            }
            else if (OffsetInCache)
            {
                pNvEnhancedFileSystemFile->SectorNum = SectorNumber + 1;
            }
            else
            {
                pNvEnhancedFileSystemFile->SectorNum = SectorNumber;
            }
        }

        // Update Cached Buffered Size value
        if (OffsetInCache)
        {
                pNvEnhancedFileSystemFile->CachedBufferSize =
                    BytesPerSector - OffsetInCache;
        }
        else
        {
            pNvEnhancedFileSystemFile->CachedBufferSize = 0;
        }
    }
    else if (pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_WRITE)
    {
        switch (Whence)
        {
            case NvOsSeek_Set:
                 if (ByteOffset < (NvS64)pNvEnhancedFileSystemFile->FileOffset)
                {
                    // In write mode, backward seek is not allowed
                    e = NvError_InvalidSize;
                    NV_DEBUG_PRINTF(("ENHANCED FS Seek: Invalid seek "
                        "position \n"));
                    goto ReturnStatus;
                }
                NewFilePosition = ByteOffset;
                break;
            case NvOsSeek_Cur:
                if (ByteOffset < 0)
                {
                    // In write mode, backward seek is not allowed
                    e = NvError_InvalidSize;
                    NV_DEBUG_PRINTF(("ENHANCED FS Seek: Invalid seek"
                        " position \n"));
                    goto ReturnStatus;
                }
                NewFilePosition =
                    (pNvEnhancedFileSystemFile->FileOffset + ByteOffset);
                break;
            case NvOsSeek_End:
            default:
                NV_DEBUG_PRINTF(("ENHANCED FS Seek: Invalid File Seek start"
                                                " position. \n"));
                return NvError_BadParameter;
        }

        // Verify File Limit aganist maximum partition size
        if ((NewFilePosition < 0) ||
            (NewFilePosition > (NvS64)pNvEnhancedFileSystemFile->MaxFileSize))
        {
            e= NvError_InvalidSize;
            NV_DEBUG_PRINTF(("Enhanced FS Seek: Invalid seek "
                "position \n"));
            goto ReturnStatus;
        }

        // Calculate page number and offset
        SectorNumber = (((NvU32)NewFilePosition / BytesPerSector) +
                FileStartSectorNum);

        OffsetInCache = ((NvU32)NewFilePosition % BytesPerSector);

        // Check if the required sector number is already in the cached buffer
        if (SectorNumber == pNvEnhancedFileSystemFile->SectorNum)
        {
            // Buffer is already in cache.
            // Now check whether file offset is same as the new file position.
            // If it is not then memset with 0xFF upto the new position.
            if (NewFilePosition !=
                (NvS64)pNvEnhancedFileSystemFile->FileOffset)
            {
                // If New file position is not same as File Offset then new
                // position is always beyond current File Offset
                NvOsMemset(
                    (pFileBuffer +
                        pNvEnhancedFileSystemFile->CachedBufferSize),
                    0xFF,
                    (NvU32)(NewFilePosition -
                        pNvEnhancedFileSystemFile->FileOffset));

                pNvEnhancedFileSystemFile->CachedBufferSize +=
                    (NvU32)(NewFilePosition -
                        pNvEnhancedFileSystemFile->FileOffset);

            }
        }
        else
        {
            NvU32 RemainingNumberOfPages = 0;

            // New file position is in the next page. So, flush
            // cached buffer first
            if (pNvEnhancedFileSystemFile->CachedBufferSize)
            {
                NvOsMemset(
                    (pFileBuffer +
                        pNvEnhancedFileSystemFile->CachedBufferSize),
                    0xFF,
                    (BytesPerSector -
                        pNvEnhancedFileSystemFile->CachedBufferSize));

                ByteOffset -= (BytesPerSector -
                    pNvEnhancedFileSystemFile->CachedBufferSize);

                pNvEnhancedFileSystemFile->CachedBufferSize =
                    BytesPerSector;

                // Encrypt and write cached buffer to device only
                // when there are still some bytes to write
                // This is required if the file is encrypted
                if (ByteOffset)
                {
                    // Write currernt cached sector
                     e = EncryptAndSignBlocks(
                                pNvEnhancedFileSystemFile,
                                BytesPerSector,
                                pFileBuffer,
                                NV_FALSE,
                                NV_FALSE);

                    if (e != NvSuccess)
                    {
                        NV_DEBUG_PRINTF(("EnhancedFS: Crypto failed "
                                        "in Write \n"));

                        e = NvError_InvalidState;
                        goto ReturnStatus;
                    }

                    pNvEnhancedFileSystemFile->HashFirstBlock = NV_FALSE;
                    pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;

                    e = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                            hBlockDeviceDriver,
                            pNvEnhancedFileSystemFile->SectorNum,
                            pFileBuffer,
                            1);

                    if (e != NvSuccess)
                    {
                        NV_DEBUG_PRINTF(("EnhancedFS Write: Device driver "
                                "Write Failed. \n"));

                        e = NvError_FileWriteFailed;
                        goto ReturnStatus;
                    }
                    pNvEnhancedFileSystemFile->SectorNum++;
                    pNvEnhancedFileSystemFile->CachedBufferSize = 0;
                    pNvEnhancedFileSystemFile->FileHeader.FileSize +=
                        BytesPerSector;
                }
            }

            // Compute remaining number of pages to seek
            RemainingNumberOfPages = ((NvU32)ByteOffset / BytesPerSector);
            if (RemainingNumberOfPages && (!OffsetInCache))
            {
                // Last page needs to be written in file close.
                // So decrement number of pages by one
                RemainingNumberOfPages--;
            }

            // Write remaining pages with 0XFF data
            while (RemainingNumberOfPages--)
            {
                NvOsMemset(pFileBuffer,
                    0xFF,
                    BytesPerSector);

                 e = EncryptAndSignBlocks(
                            pNvEnhancedFileSystemFile,
                            BytesPerSector,
                            pFileBuffer,
                            NV_FALSE,
                            NV_FALSE);

                if (e != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("EnhancedFS: Crypto failed in "
                                "Write \n"));

                    e = NvError_InvalidState;
                    goto ReturnStatus;
                }

                pNvEnhancedFileSystemFile->HashFirstBlock = NV_FALSE;
                pNvEnhancedFileSystemFile->CipherFirstBlock = NV_FALSE;

                e = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                        hBlockDeviceDriver,
                        pNvEnhancedFileSystemFile->SectorNum,
                        pFileBuffer,
                        1);

                if (e != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("EnhancedFS Write: Device driver "
                        "Write Failed. File Write failed. \n"));

                    e = NvError_FileWriteFailed;
                    goto ReturnStatus;
                }
                pNvEnhancedFileSystemFile->SectorNum++;
                pNvEnhancedFileSystemFile->FileHeader.FileSize +=
                    BytesPerSector;

                ByteOffset -= BytesPerSector;
            }

            if (!ByteOffset)
            {
                // Bytes remaining is zero. Hence number of bytes to copy to
                // cached buffer is BytesPerSector.
                ByteOffset = BytesPerSector;
            }

            // Fill number of bytes in cache with 0xFF
            NvOsMemset(pFileBuffer, 0xFF, (NvU32)ByteOffset);
            pNvEnhancedFileSystemFile->CachedBufferSize = (NvU32)ByteOffset;
        }
    }
    else
    {
        // Invalid file mode
        e = NvError_NotSupported;
        goto ReturnStatus;
    }

    // Update file offset
    pNvEnhancedFileSystemFile->FileOffset = NewFilePosition;

ReturnStatus:
    return e;
}


/*
 * Gives the current file offset from the beginning of the file
 */
static NvError
NvFileSystemFtell(
    NvFileSystemFileHandle hFile,
    NvU64 *ByteOffset)
{
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;

    CHECK_PARAM(hFile);
    CHECK_PARAM(ByteOffset);

    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile*)hFile;
    *ByteOffset = pNvEnhancedFileSystemFile->FileOffset;
    return NvSuccess;
}

static NvError
NvFileSystemFileIoctl(
     NvFileSystemFileHandle hFile,
     NvFileSystemIoctlType IoctlType,
     NvU32 InputSize,
     NvU32 OutputSize,
     const void * InputArgs,
     void *OutputArgs)
{
    NvError e = NvSuccess;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    CHECK_PARAM(hFile);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile*)hFile;
    switch (IoctlType)
    {
        case NvFileSystemIoctlType_SetCryptoCipherAlgo:
        {
            NvDdkBlockDevIoctl_SetCryptoCipherAlgoInputArgs CipherAlgo;
            NvCryptoCipherAlgoHandle hCryptoCipherAlgo = NULL;
            CHECK_PARAM(InputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvDdkBlockDevIoctl_SetCryptoCipherAlgoInputArgs));
            if (!pNvEnhancedFileSystemFile->CipherAlgoSet)
            {
                NvOsMemcpy(&CipherAlgo, InputArgs,
                    sizeof(NvDdkBlockDevIoctl_SetCryptoCipherAlgoInputArgs));

                NV_CHECK_ERROR_CLEANUP(
                    NvCryptoCipherSelectAlgorithm(
                        CipherAlgo.CipherAlgoType,
                        &pNvEnhancedFileSystemFile->hCryptoCipherAlgo)
                );

                hCryptoCipherAlgo =
                    pNvEnhancedFileSystemFile->hCryptoCipherAlgo;

                NV_CHECK_ERROR_CLEANUP(
                    hCryptoCipherAlgo->SetAlgoParams(
                        hCryptoCipherAlgo, &CipherAlgo.CipherAlgoParams)
                );

                NV_CHECK_ERROR_CLEANUP(
                    hCryptoCipherAlgo->QueryBlockSize(
                        hCryptoCipherAlgo,
                        &pNvEnhancedFileSystemFile->CipherBlockSize)
                );

                // Store Cipher algo info in file handle
                pNvEnhancedFileSystemFile->CipherAlgoType =
                    CipherAlgo.CipherAlgoType;

                NvOsMemcpy(
                    &pNvEnhancedFileSystemFile->CipherAlgoParams,
                    &CipherAlgo.CipherAlgoParams,
                    sizeof(CipherAlgo.CipherAlgoParams));

                pNvEnhancedFileSystemFile->CipherAlgoSet = NV_TRUE;
            }
            break;
        }
        case NvFileSystemIoctlType_SetCryptoHashAlgo:
        {
            NvDdkBlockDevIoctl_SetCryptoHashAlgoInputArgs HashAlgo;
            NvCryptoHashAlgoHandle hCryptoHashAlgo = NULL;
            CHECK_PARAM(InputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvDdkBlockDevIoctl_SetCryptoHashAlgoInputArgs));
            if (!pNvEnhancedFileSystemFile->HashAlgoSet)
            {
                NvOsMemcpy(&HashAlgo, InputArgs,
                    sizeof(NvDdkBlockDevIoctl_SetCryptoHashAlgoInputArgs));

                NV_CHECK_ERROR_CLEANUP(
                    NvCryptoHashSelectAlgorithm(
                        HashAlgo.HashAlgoType,
                        &pNvEnhancedFileSystemFile->hCryptoHashAlgo)
                );

                hCryptoHashAlgo = pNvEnhancedFileSystemFile->hCryptoHashAlgo;

                NV_CHECK_ERROR_CLEANUP(
                    hCryptoHashAlgo->SetAlgoParams(
                        hCryptoHashAlgo, &HashAlgo.HashAlgoParams)
                );

                NV_CHECK_ERROR_CLEANUP(
                    hCryptoHashAlgo->QueryBlockSize(
                        hCryptoHashAlgo,
                        &pNvEnhancedFileSystemFile->HashBlockSize)
                );

                pNvEnhancedFileSystemFile->HashAlgoSet = NV_TRUE;
            }
            break;
        }

        case NvFileSystemIoctlType_QueryIsEncrypted:
        {
            NvFileSystemIoctl_QueryIsEncryptedOutputArgs*
                IsEncryptedOutputArgs =
                ((NvFileSystemIoctl_QueryIsEncryptedOutputArgs*)OutputArgs);
            CHECK_PARAM(OutputArgs);
            CHECK_PARAM(OutputSize ==
                sizeof(NvFileSystemIoctl_QueryIsEncryptedOutputArgs));

            // If the file is opened in write mode return error
            CHECK_PARAM(pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_READ);

            // Return whether file is encrypted or not when file was written
            IsEncryptedOutputArgs->IsEncrypted =
                pNvEnhancedFileSystemFile->FileHeader.IsFileEncrypted;

            break;
        }

        case NvFileSystemIoctlType_QueryIsHashed:
        {
            NvFileSystemIoctl_QueryIsHashedOutputArgs* HashOutPutArgs =
                (NvFileSystemIoctl_QueryIsHashedOutputArgs *)OutputArgs;

            CHECK_PARAM(OutputArgs);
            CHECK_PARAM(OutputSize ==
                sizeof(NvFileSystemIoctl_QueryIsHashedOutputArgs));

            if (pNvEnhancedFileSystemFile->OpenMode != NVOS_OPEN_READ)
            {
                return NvError_InvalidState;
            }
            HashOutPutArgs->IsHashed =
                pNvEnhancedFileSystemFile->FileHeader.IsHashPerformed;

            break;
        }

        case NvFileSystemIoctlType_GetStoredHash:
        {
            NvFileSystemIoctl_GetStoredHashInputArgs* StoredHashInputArgs =
                (NvFileSystemIoctl_GetStoredHashInputArgs* )InputArgs;

            CHECK_PARAM(InputArgs);
            CHECK_PARAM(OutputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvFileSystemIoctl_GetStoredHashInputArgs));

            CHECK_PARAM(OutputSize ==
                sizeof(NvFileSystemIoctl_GetStoredHashOutputArgs));

            if ((pNvEnhancedFileSystemFile->OpenMode != NVOS_OPEN_READ) ||
                (!pNvEnhancedFileSystemFile->FileHeader.IsHashPerformed))
            {
                return NvError_InvalidState;
            }

            ((NvFileSystemIoctl_GetStoredHashOutputArgs*)OutputArgs)->
                    HashSize = NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

            // Check buffer size first
            if (StoredHashInputArgs->BufferSize <
                NVCRYPTO_CIPHER_AES_BLOCK_BYTES)
            {
                return NvError_InsufficientMemory;
            }
            if (!StoredHashInputArgs->pBuffer)
            {
                return NvError_InvalidAddress;
            }
            NvOsMemcpy(
                StoredHashInputArgs->pBuffer,
                pNvEnhancedFileSystemFile->FileHeader.Hash,
                NVCRYPTO_CIPHER_AES_BLOCK_BYTES);

            break;
        }

        case NvFileSystemIoctlType_GetCurrentHash:
        {
            NvFileSystemIoctl_GetCurrentHashInputArgs* CurrentHashInputArgs =
                (NvFileSystemIoctl_GetCurrentHashInputArgs* )InputArgs;

            CHECK_PARAM(InputArgs);
            CHECK_PARAM(OutputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvFileSystemIoctl_GetCurrentHashInputArgs));

            CHECK_PARAM(OutputSize ==
                sizeof(NvFileSystemIoctl_GetCurrentHashOutputArgs));

            // Return error if current Hash algoritham is not set
            // Or if file is not opened in Read Mode
            if ((pNvEnhancedFileSystemFile->OpenMode != NVOS_OPEN_READ) ||
                !pNvEnhancedFileSystemFile->HashAlgoSet)
            {
                return NvError_InvalidState;
            }

            ((NvFileSystemIoctl_GetCurrentHashOutputArgs*)OutputArgs)->
                    HashSize = NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

            // Check buffer size first
            if (CurrentHashInputArgs->BufferSize <
                NVCRYPTO_CIPHER_AES_BLOCK_BYTES)
            {
                return NvError_InsufficientMemory;
            }
            if (!CurrentHashInputArgs->pBuffer)
            {
                return NvError_InvalidAddress;
            }
            if (!pNvEnhancedFileSystemFile->IsCurrentHashAvailable)
            {
                e = GetCurrentHash(
                        pNvEnhancedFileSystemFile,
                        pNvEnhancedFileSystemFile->CurrentHash);

                if (e != NvSuccess)
                {
                    return NvError_InvalidState;
                }
                pNvEnhancedFileSystemFile->IsCurrentHashAvailable = NV_TRUE;
            }

            NvOsMemcpy(
                CurrentHashInputArgs->pBuffer,
                pNvEnhancedFileSystemFile->CurrentHash,
                NVCRYPTO_CIPHER_AES_BLOCK_BYTES);

            break;
        }

        case NvFileSystemIoctlType_IsValidHash:
        {
            NvFileSystemIoctl_QueryIsValidHashOutputArgs *IsValidOutputArgs =
                (NvFileSystemIoctl_QueryIsValidHashOutputArgs *)OutputArgs;

            NvBool Diff = NV_TRUE;

            CHECK_PARAM(OutputArgs);
            CHECK_PARAM(OutputSize ==
                sizeof(NvFileSystemIoctl_QueryIsValidHashOutputArgs));

            // Verify hash only if file is opened in Read mode.
            // Else return error
            if (pNvEnhancedFileSystemFile->OpenMode != NVOS_OPEN_READ)
            {
                return NvError_InvalidState;
            }

            // Return error if Hash algoritham is not set. Also return
            // error if Hash was not computed when file is written
            if ((!pNvEnhancedFileSystemFile->HashAlgoSet)
                || (!pNvEnhancedFileSystemFile->FileHeader.IsHashPerformed))
            {
                return NvError_InvalidState;
            }

            // Check whether current hash is already available or not
            // If not , calculate current hash
            if (!pNvEnhancedFileSystemFile->IsCurrentHashAvailable)
            {
                // Get current hash
                e = GetCurrentHash(
                        pNvEnhancedFileSystemFile,
                        pNvEnhancedFileSystemFile->CurrentHash);
                if (e != NvSuccess)
                {
                    return NvError_InvalidState;
                }
                pNvEnhancedFileSystemFile->IsCurrentHashAvailable = NV_TRUE;
            }
            Diff = NvOsMemcmp(
                    pNvEnhancedFileSystemFile->FileHeader.Hash,
                    pNvEnhancedFileSystemFile->CurrentHash,
                     pNvEnhancedFileSystemFile->FileHeader.HashSize);

            if (Diff)
            {
                IsValidOutputArgs->IsValidHash = NV_FALSE;
            }
            else
            {
                IsValidOutputArgs->IsValidHash = NV_TRUE;
            }
            break;
        }

        case NvFileSystemIoctlType_WriteVerifyModeSelect:
        {
            NvFileSystemIoctl_WriteVerifyModeSelectInputArgs *WriteVerifyMode =
                (NvFileSystemIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;

            NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs
                BlkDrvrWriteVerifyIoctlArgs;

            NvDdkBlockDevHandle hBlockDeviceDriver =
                pNvEnhancedFileSystemFile->pEnhancedFileSystem->hBlkDevHandle;

            CHECK_PARAM(InputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvFileSystemIoctl_WriteVerifyModeSelectInputArgs));

            BlkDrvrWriteVerifyIoctlArgs.IsReadVerifyWriteEnabled =
                WriteVerifyMode->ReadVerifyWriteEnabled;

            NV_CHECK_ERROR_CLEANUP(
                hBlockDeviceDriver->NvDdkBlockDevIoctl(
                    hBlockDeviceDriver,
                    NvDdkBlockDevIoctlType_WriteVerifyModeSelect,
                    sizeof(NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs),
                    0,
                    &BlkDrvrWriteVerifyIoctlArgs,
                    NULL)
            );
            break;
        }

        default:
            e = NvError_BadParameter;
    }

fail:
    return e;
}

static NvError
NvFileSystemFileQueryFstat(
    NvFileSystemFileHandle hFile,
    NvFileStat *pStat)
{
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    CHECK_PARAM(hFile);
    CHECK_PARAM(pStat);
    pNvEnhancedFileSystemFile = (NvEnhancedFileSystemFile *)hFile;

    if (pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_READ)
    {
        // Check whether file data is valid or not
        if (pNvEnhancedFileSystemFile->FileHeader.IsFileDataValid == 0x0)
        {
            pStat->Size = pNvEnhancedFileSystemFile->FileHeader.FileSize;
        }
        else
        {
            pStat->Size = 0;
            return  NvError_InvalidState;
        }
    }
    else
    {
        // File is in write mode
        // In write mode return Max file size which is
        // complete partition size - sector size
        pStat->Size = pNvEnhancedFileSystemFile->MaxFileSize;
    }
    return NvSuccess;
}

static NvError
NvFileSystemFileOpen(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvU32 Flags,
    NvFileSystemFileHandle *phFile)
{
    NvError e = NvSuccess;
    NvEnhancedFileSystem *pNvEnhancedFileSystem = NULL;
    NvEnhancedFileSystemFile *pNvEnhancedFileSystemFile = NULL;
    NvDdkBlockDevInfo* pBlockDevInfo = NULL;
    NvPartitionHandle hPartHandle = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvU32 FileNameLength = 0;
    NvBool FileOpened = NV_FALSE;

    CHECK_PARAM(hFileSystem);
    CHECK_PARAM(FilePath);
    CHECK_PARAM(phFile);

    NvOsMutexLock(s_NvEnhancedFSFileMutex);
    // Check if file is already opened
    FileOpened = InNvEnhancedFSList(FilePath);
    if (FileOpened == NV_TRUE)
    {
        e = NvError_AccessDenied;
        goto fail;
    }
    pNvEnhancedFileSystem = (NvEnhancedFileSystem*)hFileSystem;
    // Check whether there are already files opened on this handle
    if (pNvEnhancedFileSystem->RefFileCount)
    {
        // Do not goto fail in this case.
        // Return error from here
        NvOsMutexUnlock(s_NvEnhancedFSFileMutex);
        return NvError_AccessDenied;
    }
    pBlockDevInfo = NvOsAlloc(sizeof(NvDdkBlockDevInfo));
    CHECK_MEM(pBlockDevInfo);
    pNvEnhancedFileSystemFile = NvOsAlloc(sizeof(NvEnhancedFileSystemFile));
    CHECK_MEM(pNvEnhancedFileSystemFile);

    // Add file name in file handle
    FileNameLength = NvOsStrlen(FilePath);
    pNvEnhancedFileSystemFile->pFileName = NvOsAlloc(FileNameLength + 1);
    CHECK_MEM(pNvEnhancedFileSystemFile->pFileName);
    NvOsStrncpy(pNvEnhancedFileSystemFile->pFileName,
        FilePath,
        FileNameLength);

    pNvEnhancedFileSystemFile->pFileName[FileNameLength] = '\0';

    pNvEnhancedFileSystemFile->CachedBufferSize = 0;
    hBlkDev = pNvEnhancedFileSystem->hBlkDevHandle;
    hBlkDev->NvDdkBlockDevGetDeviceInfo(hBlkDev,pBlockDevInfo);
    if (!pBlockDevInfo->TotalBlocks)
    {
        e = NvError_DeviceNotFound;
        NV_DEBUG_PRINTF(("Enhanced FS FileOpen: Either device is not present "
                            "or its not responding. File open failed \n"));
        goto fail;
    }
    hPartHandle = pNvEnhancedFileSystem->hPartition;
    pNvEnhancedFileSystemFile->BytesPerSector = pBlockDevInfo->BytesPerSector;
    pNvEnhancedFileSystemFile->SectorNum =
                (NvU32)hPartHandle->StartLogicalSectorAddress;

    // Alloate memory of page size + Max cipher block size
    pNvEnhancedFileSystemFile->pFileCachedBuffer =
        NvOsAlloc(pNvEnhancedFileSystemFile->BytesPerSector +
        (2 * NVCRYPTO_CIPHER_AES_BLOCK_BYTES));
    CHECK_MEM(pNvEnhancedFileSystemFile->pFileCachedBuffer);
    pNvEnhancedFileSystemFile->pEnhancedFileSystem =
        (NvEnhancedFileSystem*)hFileSystem;

    pNvEnhancedFileSystemFile->OpenMode = Flags;
    pNvEnhancedFileSystemFile->FileOffset = 0;

    // Need to write the header in last sector. So, max file size is
    // complete partition size - one sector.
    pNvEnhancedFileSystemFile->MaxFileSize =
        ((pBlockDevInfo->BytesPerSector * hPartHandle->NumLogicalSectors) -
                pNvEnhancedFileSystemFile->BytesPerSector);

    pNvEnhancedFileSystemFile->hCryptoHashAlgo = NULL;
    pNvEnhancedFileSystemFile->hCryptoCipherAlgo = NULL;
    pNvEnhancedFileSystemFile->CipherAlgoSet = NV_FALSE;
    pNvEnhancedFileSystemFile->HashAlgoSet = NV_FALSE;
    pNvEnhancedFileSystemFile->IsCurrentHashAvailable = NV_FALSE;
    pNvEnhancedFileSystemFile->HashFirstBlock = NV_TRUE;
    pNvEnhancedFileSystemFile->CipherFirstBlock = NV_TRUE;
    pNvEnhancedFileSystemFile->HashBlockSize = 0;
    pNvEnhancedFileSystemFile->CipherBlockSize = 0;
    pNvEnhancedFileSystemFile->CipherAlgoType = NvCryptoCipherAlgoType_Invalid;

    // If file is open in read mode then read header from last page
   if (pNvEnhancedFileSystemFile->OpenMode == NVOS_OPEN_READ)
   {
        e = hBlkDev->NvDdkBlockDevReadSector(
                hBlkDev,
                (NvU32)(hPartHandle->StartLogicalSectorAddress +
                hPartHandle->NumLogicalSectors) - 1,
                pNvEnhancedFileSystemFile->pFileCachedBuffer,
                1);

        if (e != NvSuccess)
        {
            e = NvError_DeviceNotFound;
            NV_DEBUG_PRINTF(("Enhanced FS Open: Device driver Read "
                                    "Failed. File Read Failed \n"));
            goto fail;
        }
        NvOsMemcpy(
            &pNvEnhancedFileSystemFile->FileHeader,
            pNvEnhancedFileSystemFile->pFileCachedBuffer,
            sizeof(NvEnhancedFSHeader));

        // Check whether data in the file is valid or not
        if (pNvEnhancedFileSystemFile->FileHeader.IsFileDataValid != 0x0)
        {
            e = NvError_InvalidState;
            goto fail;
        }
    }
   else
    {
        // Memset header with 0
        NvOsMemset(
            &pNvEnhancedFileSystemFile->FileHeader,
            0x0,
            sizeof(NvEnhancedFSHeader));
        // Data valid flag. 0 is valid. 1 is invalid.
        pNvEnhancedFileSystemFile->FileHeader.IsFileDataValid = 0xFF;
    }

    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileClose =
                                                    NvFileSystemFileClose;
    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileRead =
                                                    NvFileSystemFileRead;
    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileWrite =
                                                    NvFileSystemFileWrite;
    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileSeek =
                                                NvFileSystemFileSeek;
     pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFtell =
                                                NvFileSystemFtell;
    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileIoctl =
                                                    NvFileSystemFileIoctl;
    pNvEnhancedFileSystemFile->hFileSystemFile.NvFileSystemFileQueryFstat =
                                                NvFileSystemFileQueryFstat;
    // Add File name to FileList
    NV_CHECK_ERROR_CLEANUP(AddToNvEnhancedFSList(FilePath));
    pNvEnhancedFileSystem->RefFileCount++;
    *phFile = &pNvEnhancedFileSystemFile->hFileSystemFile;

    if (pBlockDevInfo)
    {
        NvOsFree(pBlockDevInfo);
        pBlockDevInfo = NULL;
    }
    NvOsMutexUnlock(s_NvEnhancedFSFileMutex);
    return e;

fail:
    if (pNvEnhancedFileSystemFile)
    {
        if (pNvEnhancedFileSystemFile->pFileCachedBuffer)
        {
            NvOsFree(pNvEnhancedFileSystemFile->pFileCachedBuffer);
            pNvEnhancedFileSystemFile->pFileCachedBuffer = NULL;
        }
        if (pNvEnhancedFileSystemFile->pFileName)
        {
            NvOsFree(pNvEnhancedFileSystemFile->pFileName);
            pNvEnhancedFileSystemFile->pFileName = NULL;
        }
        NvOsFree(pNvEnhancedFileSystemFile);
        pNvEnhancedFileSystemFile = NULL;
    }
    if (pBlockDevInfo)
    {
        NvOsFree(pBlockDevInfo);
        pBlockDevInfo = NULL;
    }
    *phFile = NULL;
    NvOsMutexUnlock(s_NvEnhancedFSFileMutex);
    return e;
}

NvError
NvEnhancedFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem)
{
    NvError e = NvSuccess;
    NvEnhancedFileSystem *pNvEnhancedFileSystem = NULL;
    CHECK_PARAM(hPart);
    CHECK_PARAM(hDevice);
    CHECK_PARAM(phFileSystem);
    NvOsMutexLock(s_NvEnhancedFSMutex);
    pNvEnhancedFileSystem = NvOsAlloc(sizeof(NvEnhancedFileSystem));
    CHECK_MEM(pNvEnhancedFileSystem);
    pNvEnhancedFileSystem->hFileSystem.NvFileSystemUnmount =
                            NvFileSystemUnmount;

    pNvEnhancedFileSystem->hFileSystem.NvFileSystemFileOpen =
                            NvFileSystemFileOpen;

    pNvEnhancedFileSystem->hFileSystem.NvFileSystemFileQueryStat =
                            NvFileSystemFileQueryStat;

    pNvEnhancedFileSystem->hFileSystem.NvFileSystemFileSystemQueryStat =
                            NvFileSystemFileSystemQueryStat;

    pNvEnhancedFileSystem->hFileSystem.NvFileSystemFormat = NvFileSystemFormat;

    pNvEnhancedFileSystem->hPartition = hPart;
    pNvEnhancedFileSystem->hBlkDevHandle = hDevice;
    pNvEnhancedFileSystem->RefFileCount = 0; //Init the RefFile Count to 0
    *phFileSystem = &pNvEnhancedFileSystem->hFileSystem;
    gs_FSMountCount++; // Increment FSMount count
    NvOsMutexUnlock(s_NvEnhancedFSMutex);
    return e;

fail:
    *phFileSystem = NULL;
    NvOsMutexLock(s_NvEnhancedFSMutex);
    return e;
}

