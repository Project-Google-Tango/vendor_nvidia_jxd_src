/*
 * Copyright (c) 2011 - 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_SE_CORE_H
#define INCLUDED_NVDDK_SE_CORE_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_init.h"
#include "nvrm_memmgr.h"

#define IN_BITS(x)  ((x)<<3)
#define IN_BYTES(x) ((x)>>3)

/*  size of LL buffer is 4MB */
#define NVDDK_SE_HW_LL_BUFFER_SIZE_BYTES (0x400000)

/* DMA data buffer address alignment */
#define NVDDK_SE_HW_DMA_ADDR_ALIGNMENT (64)

/* Max timeout for SE = 10sec */
#define SE_WAITOUT_TIME (10000)


/* Number of LLs maintained */
#define NVDDK_SE_LL_NUM (2)

#define SE_AES_NUM_SLOTS 16

#define SRK_SLOT_NUM 0
#define SSK_SLOT_NUM 15

#define SE_AES_BLOCK_SIZE 16
#define SE_KEY256_SIZE 32
#define SE_KEY192_SIZE 24
#define SE_KEY128_SIZE 16
#define SE_AES_IV_SIZE 16
#define SE_MAX_LAST_BLOCK_SIZE 0xFFFFF
#define SE_RSA_CONTEXT_SAVE_KEYSLOT_COUNT 4

#define SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX(x) \
        (x << SE_CTX_SAVE_CONFIG_0_RSA_KEY_INDEX_SHIFT)

#define SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD(x) \
        (x << SE_CTX_SAVE_CONFIG_0_RSA_WORD_QUAD_SHIFT)

#define SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX(x) \
        (x << SE_CTX_SAVE_CONFIG_0_AES_KEY_INDEX_SHIFT)

#define SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD(x) \
        (x << SE_CTX_SAVE_CONFIG_0_AES_WORD_QUAD_SHIFT)

#define SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD(x) \
        (x << SE_CTX_SAVE_CONFIG_0_STICKY_WORD_QUAD_SHIFT)

#define SE_CONTEXT_SAVE_RANDOM_DATA_SIZE 16
#define SE_CONTEXT_SAVE_STICKY_BITS_SIZE 16
#define SE_CONTEXT_KNOWN_PATTERN_SIZE 16
#define SE_CONTEXT_BUFER_SIZE_T30 1072
#define SE_CONTEXT_BUFER_SIZE 2112

/**
 * Key slot numbers that are suported by SE.
 * All slots can be configured as secure or
 * non-secure.
 */
typedef enum
{
    /* Key slot 0 */
    SeAesHwKeySlot_0,
    /* Key slot 1 */
    SeAesHwKeySlot_1,
    /* Key slot 2 */
    SeAesHwKeySlot_2,
    /* Key slot 3 */
    SeAesHwKeySlot_3,
    /* Key slot 4 */
    SeAesHwKeySlot_4,
   /* Key slot 5 */
    SeAesHwKeySlot_5,
    /* Key slot 6 */
    SeAesHwKeySlot_6,
    /* Key slot 7 */
    SeAesHwKeySlot_7,
   /* Key slot 8 */
    SeAesHwKeySlot_8,
    /* Key slot 9 */
    SeAesHwKeySlot_9,
    /* Key slot 10 */
    SeAesHwKeySlot_10,
    /* Key slot 11 */
    SeAesHwKeySlot_11,
    /* Key slot 12 */
    SeAesHwKeySlot_12,
    /* Key slot 13 */
    SeAesHwKeySlot_13,
    /* Key slot 14 */
    SeAesHwKeySlot_14,
    /* Key slot 15 */
    SeAesHwKeySlot_15,
    /* Total Number of keyslots */
    SeAesHwKeySlot_NumExt,
    /* Key slot reserved for SBK */
    SeAesHwKeySlot_Sbk = SeAesHwKeySlot_14,
    /* Key slot Reserved for SSK */
    SeAesHwKeySlot_Ssk = SeAesHwKeySlot_15,
} SeAesHwKeySlot;


typedef struct
{
    /*  Last buffer number */
    NvU32 LLBufNum;
    /*  Buffer Physical start address */
    NvU8 *LLBufPhyStartAddr;
    /*  Buffer size in bytes */
    NvU32 LLBufSize;
} NvDdkSeLLInfo;

typedef struct
{
    /*  LinkedList Physical Address */
    NvDdkSeLLInfo *LLPhyAddr;
    /*  LinkedList Virtual Address */
    NvDdkSeLLInfo *LLVirAddr;
    /*  LinkedList Buffer Virtual Address */
    NvU8 *LLBufVirAddr;
} NvDdkSeLL;

/*  Block sizes of SHA algorithms */
typedef enum
{
    SHA1_BLOCK_SIZE = 512,
    SHA224_BLOCK_SIZE = 512,
    SHA256_BLOCK_SIZE = 512,
    SHA384_BLOCK_SIZE = 1024,
    SHA512_BLOCK_SIZE = 1024
} NvDdkSeShaBlockSize;

/*  SHA Engine Context */
typedef struct
{
    /*  Specifies whether or not the HW initialization is required */
    NvBool IsSHAInitHwHash;
    /*  SHA Packet mode */
    NvU32 ShaPktMode;
    /*  SHA Final Hash Length */
    NvU32 ShaFinalHashLen;
    /*  SHA Block Size */
    NvU32 ShaBlockSize;
    /*  SHA operating mode */
    NvDdkSeShaEncMode Mode;
    /*  Size of total message length that needs to be hashed in bits */
    NvU64 MsgLength;
    /*  Size of Message left in bits */
    NvU64 MsgLeft;
    /*  Hash result */
    NvU32 HashResult[16];
    /*  Input LL that needs to be submitted for processing */
    NvDdkSeLLInfo *LLAddr;
} NvDdkSeShaHWCtx;

/*  Algorithm independent SE Engine Context */
typedef struct
{
    /*  Device Handle */
    NvRmDeviceHandle hRmDevice;
    /*  Memory handle to the LL h/w Buffer  */
    NvRmMemHandle hDmaMemBuf;
    /*  Memory handle to the LL h/w */
    NvRmMemHandle hDmaLLMem;
    /*  Interrupt handle */
    NvOsInterruptHandle InterruptHandle;
    /*  SE registers virtual base address */
    NvU32 *pVirtualAddress;
    /*  SE registers map size */
    NvU32 BankSize;
    /*  Power client ID */
    NvU32 RmPowerClientId;
    /*  Input LL */
    NvDdkSeLL InputLL[NVDDK_SE_LL_NUM];
    /*  Output LL */
    NvDdkSeLL OutputLL[NVDDK_SE_LL_NUM];
    /*  Specifies whether has there been any error */
    NvBool ErrFlag;
    /*  Specifies which buffer is being used */
    NvBool BufferInUse;
} NvDdkSeHWCtx;

typedef struct
{
    /* Specifies operation to be performed */
    NvDdkSeAesOperationalMode OpMode;
    /* Specifies if Operation is Encryption or Decryption */
    NvBool IsEncryption;
    /* Specifies the Slot number (0 - 15) where SSK is stored */
    NvU32 SskKeySlotNum;
    /* Specifies the Slot number where  Sbk is Stored */
    NvU32 SbkKeySlotNum;
    /* Type of key been used */
    NvDdkSeAesKeyType KeyType;
    /* Key slot Used in the current operation */
    NvU32 KeySlotNum;
    /* Pointer to the key to be used */
    NvU8 Key[NvDdkSeAesConst_MaxKeyLengthBytes];
    /* Length of the key being used */
    NvU32 KeyLenInBytes;
    /* Pointer to IV to be used */
    NvU8 IV[NvDdkSeAesConst_IVLengthBytes];
    /* Size of current Input Data */
    NvU32 SrcBufSize;
    /* Counter value used for CTR Mode*/
    NvU32 Counter;
   /* Indicates if Engine is Disabled or not */
    NvBool IsEngineDisabled;
    /* Input LL Address to be programmed */
    NvU32 InLLAddr;
    /* Output LL Address to be programmed */
    NvU32 OutLLAddr;
} NvDdkSeAesHWCtx;

/* RSA Specific */
typedef struct
{
    /* RSA Modulus Length in bits */
    NvU32  RsaModulusLenInBits;
    /* RSA Exponent Length in bits */
    NvU32  RsaExponentLenInBits;
    /* Key Slot */
    NvU32  RsaKeySlot;
    /* Destination */
    NvBool RsaDestIsMemory;
    /* Pointer to the modulus*/
    void *RsaModulus;
    /* Pointer to the exponent */
    void *RsaExponent;
    /* Input LL Address that needs to be submitted for processing */
    NvU32  InLLAddr;
    /* Output LL Address that needs to be submitted for processing */
    NvU32  OutLLAddr;
} NvDdkSeRsaHWCtx;

/*
 * Software context for RNG Operation.
 */
typedef struct
{
    /* Rng Mode (FORCE INSTANTIATE or FORCE RESEED or NORMAL) */
    NvU32 RngInitMode;
    /* Rng Key to be used (128b or 192b or 256b) */
    NvU32 RngKeyUsed;
    /* length of the out put data to be generated */
    NvU32 RngOutSizeInBytes;
    /* Rng destination (Memory or SRK )
     * ::CAUTION: only MEMORY is been supported from the software side
     */
    NvU32 RngDest;
    /* Rng source (Memory or Entropy) */
    NvU32 RngSrc;
    /* Rng reseed counter (specifed by the h/w folkes) */
    NvU32 RngReseedCounter;
    /* Specifies if Rng context is set of not*/
    NvBool IsRngContextSet;
    /* contains the address of the input linkedlist incase of
     * Memory as Source.
     */
    NvU32 InLLAddr;
    /* addaress of the output linked list. */
    NvU32 OutLLAddr;
    /* address of the Input linked list incase of Reseed exhaust interrupt. */
    NvU32 REIInputLLAddr;
} NvDdkSeRngHWCtx;

/*  Union of Crypto operations contexts supported by SE   */
typedef union
{
    /*  SE - SHA Context */
    NvDdkSeShaHWCtx SeShaHwCtx;
    /* SE - AES Context */
    NvDdkSeAesHWCtx SeAesHwCtx;
    /* RSA -RSA Context */
    NvDdkSeRsaHWCtx SeRsaHwCtx;
    /* RNG context */
    NvDdkSeRngHWCtx SeRngHwCtx;
} NvDdkSeAlgo;

/*  Client context */
typedef struct
{
    /*  Algorithm being used in SE */
    NvDdkSeOperatingMode OpMode;
    /*  Algorithm Context */
    NvDdkSeAlgo Algo;
} NvDdkSe;

/* function pointers to Hw interface */
typedef struct
{
     NvError
        (*pfNvDdkSeShaProcessBuffer)(NvDdkSeShaHWCtx* const hSeShaHwCtx);
     NvError
        (*pfNvDdkSeShaBackUpRegs)(NvDdkSeShaHWCtx* const hSeShaHwCtx);
     NvError
        (*pfNvDdkSeAesSetKey)(NvDdkSeAesHWCtx *hSeAesHwCtx);
     NvError
        (*pfNvDdkSeAesProcessBuffer)(NvDdkSeAesHWCtx *hSeAesHwCtx);
     NvError
        (*pfNvDdkSeAesCMACProcessBuffer)(NvDdkSeAesHWCtx *hSeAesHwCtx);
     NvError
        (*pfNvDdkCollectCMAC)(NvU32 *pBuffer);
     void
        (*pfNvDdkSeAesWriteLockKeySlot)(SeAesHwKeySlot KeySlotIndex);
     void
        (*pfNvDdkSeAesSetIv)(NvDdkSeAesHWCtx *hSeAesHwCtx);
     void
        (*pfNvDdkSeAesGetIv)(NvDdkSeAesHWCtx *hSeAesHwCtx);
     NvError
        (*pfNvDdkSeRsaSetKey)(NvDdkSeRsaHWCtx* const hSeRsaHwCtx);
     NvError
        (*pfNvDdkSeRsaProcessBuffer)(NvDdkSeRsaHWCtx* const hSeRsaHwCtx);
     NvError
        (*pfNvDdkSeRsaCollectOutputData)(NvU32 *Buffer, NvU32 Size,
                NvBool DestIsMemory);
     NvError
         (*pfNvDdkSeRngGenerateRandomNumber)(NvDdkSeRngHWCtx *hSeRngHwCtx);
} NvDdkSeHwInterface;

typedef struct
{
    NvBool IsSPRngSupported;
    NvBool IsPkcSupported;
    NvDdkSeHwInterface *pHwIf;
} NvDdkSeCoreCapabilities;

/**
 * Perform Modular Exponentation operation.
 *
 * @params hSeRsaHwCtx Rsa h/w Context.
 * @params pKeyDataInfo contains input data required for operation
 * (source data, size of source data,... etc).
 * @params pOutDataInfo contains the pointer to output buffer.
 *
 * @retval NvSuccess if the operation is successful.
 * @retval NvError_InvalidSize if the modulus size != 2048 or 1024 or 512.
 * @retval NvError_BadParameter if any of hSeRsaHwCtx or pOutDataInfo or
 * pKeyDataInfo are not valid.
 */
NvError
NvDdkSeRSAPerformModExpo(NvDdkSeRsaHWCtx *hSeRsaHwCtx,
                         const NvSeRsaKeyDataInfo* pKeyDataInfo,
                         NvSeRsaOutDataInfo* pOutDataInfo);
/**
 * Setup Rng context.
 *
 * @Params hSeRngHwCtx Rng H/W context
 * @params pInitInfo contains  the information needed for initiation of Rng.
 *
 * @retval NvSuccess if the operation is successful.
 * @retval NvError_BadParameter if any of hSeRngHwCtx or pInitInfo are Invaild.
 */
NvError
NvDdkSeRngSetUpContext(NvDdkSeRngHWCtx *hSeRngHwCtx,
        const NvDdkSeRngInitInfo *pInitInfo);

/**
 * Generate the Random number.
 *
 * @Params hSeRngHwCtx Rng H/W context
 * @params pProcessInInfo is a pointer to input(bascically the size of the
 * random number to be generated).
 * @params pProcessOutInfo is a pointer to the output(pointer to hold random
 * number).
 *
 * @retval NvSuccess if the operation is successful.
 * @retval NvError_BadParameter if any of hSeRngHwCtx or pProcessInInfo
 * or pProcessOutInfo.
 */
NvError
NvDdkSeRngGetRandomNumber(NvDdkSeRngHWCtx *hSeRngHwCtx,
         const NvDdkSeRngProcessInInfo *pProcessInInfo,
         const NvDdkSeRngProcessOutInfo *pProcessOutInfo);

/* SHA Specific */
NvError
NvDdkSeShaInit(NvDdkSeShaHWCtx *hSeShaHwCtx, const NvDdkSeShaInitInfo *pInitInfo);

NvError
NvDdkSeShaUpdate(NvDdkSeShaHWCtx *hSeShaHwCtx, const NvDdkSeShaUpdateArgs *pProcessBufInfo);

NvError
NvDdkSeShaFinal(NvDdkSeShaHWCtx *hSeShaHwCtx, NvDdkSeShaFinalInfo *pFinalInfo);

/**
 * Finds a free key slot and writes Key and Iv(if mentioned) into the key slot
 *
 * @param hSeAesHwCtx Aes H/w context
 * @param pKeyInfo contains key and Iv information
 *
 * @retval NvSuccess if setting Key and Iv is successful
 * @retval NvError_BadParameter if Key size != 192 or 128 or 256 .
 * @retval NvError_AlreadyAllocated if all key slots are allocated
 */
NvError
NvDdkSeAesSelectKey(NvDdkSeAesHWCtx *hSeAesHwCtx, const NvDdkSeAesKeyInfo *pKeyInfo);
/**
 * Copies the Initial vector from AesContext to given Iv buffer
 *
 * @param hSeAesHwCtx Aes H/w context
 * @param pIvInfo contains IV buffer
 *
 * @retval NvSuccess if copying Iv is successful
 * @retval  NvError_BadParameter if expected Iv size is less than 16 Bytes.
 */
NvError
NvDdkSeAesGetInitialVector(NvDdkSeAesHWCtx *hSeAesHwCtx, NvDdkSeAesGetIvArgs *pIvInfo);

/**
 * Selects the mode of operation to be performed
 *
 * @param hSeAesHwCtx Aes H/w context
 * @param pOperation pointer to operation info
 *
 * @retval NvSuccess if Selecting operation is successful
 * @retval NvError_NotSupported if Mode of operation is neither of
 * Ofb, Ctr, Cbc, Ecb.
 */
NvError
NvDdkSeAesSelectOperation(NvDdkSeAesHWCtx *hSeAesHwCtx, const NvDdkSeAesOperation *pOperation);

/**
 * Performes Aes Operation of the given buffer
 *
 * @params hSeAesHwCtx Aes H/w context
 * @param pOperation pointer to ProcessBuffer info
 *
 * @retval NvSuccess if Operation is successful
 * @retval NvError_InvalidState if Operation mode is not any of Ofb, Ctr,Cbc, Ecb.
 * @retval NvError_NotSupported if Operation mode is either cbc or ecb
 * @retval NvError_BadParameter if buffer size is not a proper multiple of 16 .
 */
NvError
NvDdkSeAesProcessBuffer(
    NvDdkSeAesHWCtx *hSeAesHwCtx,
    const NvDdkSeAesProcessBufferInfo *pProcessBufInfo);

/**
 * Write 0's in SBK key-slot
 *
 * @params hSeAesHwCtx Aes H/w context
 *
 * @retval NvSuccess if clearing Sbk is successful
 */
NvError
NvDdkSeAesClearSecureBootKey(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Performs Cmac operation on the given buffer
 *
 * @params hSeAesHwCtx Aes H/w context
 * @params pInInfo pointer to information need by cmac
 * @params pOutinfo pointer to o/p information from the operation.
 *
 * @retval NvSuccess if CMAC operation is successful
 * @retval NvError_InvalidSize if expected output cmac length is < 16 .
 */
NvError
NvDdkSeAesComputeCMAC(
        NvDdkSeAesHWCtx *hSeAesHwCtx,
        const NvDdkSeAesComputeCmacInInfo *pInInfo,
        NvDdkSeAesComputeCmacOutInfo *pOutInfo);

/**
 * Releases the key slot aquired for performing opeartion
 *
 * @params hSeAesHwCtx Aes H/w context
 */
void
NvDdkSeAesReleaseKeySlot(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Write Lock secure storage key.
 *
 * @params hSeAesHwCtx Aes H/w context
 *
 * @retval NvSuccess if locking is successful
 * @retval NvError_NotInitialized if context is not initialized
 */
NvError
NvDdkSeAesLockSecureStorageKey(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Set and lock SecureStoragekey
 *
 * @params hSeAesHwCtx Aes H/w context
 * @parmas SskInfo contains ssk
 *
 * @retval NvSuccess if setting and locking of ssk is successful
 * @retval NvError_NotInitialized if context is not initialized
 */
NvError
NvDdkSeAesSetAndLockSecureStorageKey(
    NvDdkSeAesHWCtx *hSeAesHwCtx,
    const NvDdkSeAesSsk *SskInfo);

/**
 * Set's the Iv of the specified keyslot with given IV
 *
 * @params hSeAesHwCtx Aes H/w context
 * @params pSetIvInfo contains the new Iv
 *
 * @retval NvSuccess if setting Iv is successful
 * @retval NvError_BadParameter if New Iv is NULL
 * @retval NvError_NotSupported if keyslotindex is > 15 .
 */
NvError
NvDdkSeAesSetIv(NvDdkSeAesHWCtx *hSeAesHwCtx, const NvDdkSeAesSetIvInfo *pSetIvInfo);

NvError
NvDdkSeAesGetIv(NvDdkSeAesHWCtx *hSeAesHwCtx, NvDdkSeAesSetIvInfo *pSetIvInfo);

NvError
NvDdkSeCoreInit(NvRmDeviceHandle hRmDevice);

void
NvDdkSeCoreDeInit(void);

NvBool
NvDdkIsSeCoreInitDone(void);

/**
 * Encrypts SE context with SRK and saves to a buffer.
 * The buffer address will be written into a scratch register
 * and SRK into 4 secure scratch registers.
 *
 * This context save operation will be called by bootloader
 * while entering suspend. The context will be restored by
 * bootrom on resume.
 *
 * The format for context save operation:
 *
 * For T30:
 * 16 bytes of encrypted random data
 * 16 bytes of encrypted sticky bits context
 * 512 bytes of encrypted key slot context
 * 256 bytes of encrypted original IV context
 * 256 bytes of encrypted updated IV context
 * 16 bytes of encrypted known pattern
 *
 * For T114 and above:
 * 16 bytes of encrypted random data
 * 32 bytes of encrypted sticky bits context
 * 512 bytes of encrypted key slot context
 * 256 bytes of encrypted original IV context
 * 256 bytes of encrypted updated IV context
 * 1024 bytes of encrypted RSA key slot context
 * 16 bytes of encrypted known pattern
 *
 * @retval NvSuccess if SE context save operation is successful
 * @retval NvError_BadValue if the # of blocks for context save
 * exceeds max block size
 * @retval NvError_Timeout if context save operartion timeouts
 * @retval NvError_InvalidState if the SE engine give error status
 * during context save operation.
 */
NvError
NvDdkSeSuspend(void);

#if defined(__cplusplus)
}
#endif

#endif
