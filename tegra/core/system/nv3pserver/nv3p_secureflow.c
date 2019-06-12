/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nverror.h"
#include "nv3p.h"
#include "nvrm_module.h"
#include "nvrm_pmu.h"
#include "nvpartmgr.h"
#include "nvfs_defs.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvstormgr.h"
#include "nvsystem_utils.h"
#include "nvbct.h"
#include "nvbu.h"
#include "nvbl_query.h"
#include "nvbl_memmap_nvap.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_hash_defs.h"
#include "nvodm_pmu.h"
#include "nvcrypto_cipher.h"
#include "nv3p_server_utils.h"
#include "nvodm_fuelgaugefwupgrade.h"
#include "nvsku.h"
#include "nvddk_fuse.h"
#include "nvflash_version.h"
#include "nvddk_se_blockdev.h"
#include "nv_rsa.h"
#include "nvcrypto_common_defs.h"
#include "nv3p_server_private.h"

#define IN_BYTES(x) ((x)>>3)
#define SE_TEST_RNG_MAX_SEED_LEN IN_BYTES(384)
#define SE_TEST_RNG_OUT_MIN_SIZE IN_BYTES(128)
#define MAX_ROUNDS 200
#define RSA_KEY_SIZE 256

NvU8 g_RandomNumber[SE_TEST_RNG_OUT_MIN_SIZE * MAX_ROUNDS];
NvU32 g_DSession[4];
extern Nv3pServerState *s_pState;

void NvRSAEncDecMessage(NvU32* output, NvU32* message, NvU32* exponent,
    NvBigIntModulus* n);

#ifdef NV_LDK_BUILD
// no support other than android build
void NvRSAEncDecMessage(NvU32* output, NvU32* message, NvU32* exponent,
    NvBigIntModulus* n)
{
   return;
}
#endif
static NvError PerformRng(void);

typedef struct SeRngTestVecData_Rec
{
    NvU8 RngSeed[SE_TEST_RNG_MAX_SEED_LEN];
    NvU32 RngSeedLen;
    NvDdkSeRngKey KeySize;
    NvU8 RandomNumber[SE_TEST_RNG_OUT_MIN_SIZE * MAX_ROUNDS];
} SeRngTestVecData;

#if NV_RNG_SUPPORT
static SeRngTestVecData Rng1Vector =
{
    {
        0xa6, 0xf4, 0xd9, 0x72, 0xd2, 0xfc, 0x94, 0xe9, 0xb9, 0x82, 0xb5, 0xdc,
        0xec, 0xc8, 0x24, 0xdb, 0x83, 0xad, 0xd7, 0xd5, 0x36, 0x2e, 0xb7, 0x57,
        0x05, 0x33, 0x3f, 0x51, 0x22, 0xf5, 0x42, 0x2e
    },
    32,
    NvDdkSeRngKeySize_128,
    {
        0x00
    }
};
#endif

/*
 *To generate the symmetric key on the device
 */
NvError PerformRng(void)
{
    NvError e = NvSuccess;
#if NVODM_BOARD_IS_SIMULATION == 0
#if NV_RNG_SUPPORT

    NvDdkBlockDevHandle SeHandle = NULL;
    NvDdkSeRngInitInfo  RngInitInfo;
    NvDdkSeRngProcessInInfo RngInInfo;
    NvDdkSeRngProcessOutInfo RngOutInfo;
    /*  open the SE device */
    NV_CHECK_ERROR_CLEANUP(NvDdkSeBlockDevOpen(0, 0, &SeHandle));

    RngInitInfo.RngMode = NvDdkSeRngMode_ForceReseed;
    RngInitInfo.RngKeyUsed       = Rng1Vector.KeySize;
    RngInitInfo.RngSeed          = Rng1Vector.RngSeed;
    RngInitInfo.RngSeedSize      = Rng1Vector.RngSeedLen;
    RngInitInfo.IsSrcMem         = 0;
    RngInitInfo.IsDestMem        = 1;

    NV_CHECK_ERROR_CLEANUP(SeHandle->NvDdkBlockDevIoctl(SeHandle,
                                     NvDdkSeBlockDevIoctlType_RNGSetUpContext,
                                     sizeof(RngInitInfo),
                                     0,
                                     (const void *)&RngInitInfo,
                                     NULL
                                    ));

    RngInInfo.OutPutSizeReq  = (0) ? SE_TEST_RNG_OUT_MIN_SIZE * 195:
        SE_TEST_RNG_OUT_MIN_SIZE * 95;
    RngOutInfo.pRandomNumber = g_RandomNumber;

    NV_CHECK_ERROR_CLEANUP(SeHandle->NvDdkBlockDevIoctl(SeHandle,
                                     NvDdkSeBlockDevIoctlType_RNGGetRandomNum,
                                     sizeof(RngInInfo),
                                     sizeof(RngOutInfo),
                                     (const void *)&RngInInfo,
                                     (void *)&RngOutInfo
                                    ));
fail:
    /*  Close the device */
    if (SeHandle)
        SeHandle->NvDdkBlockDevClose(SeHandle);
#endif
#endif
    return e;
}

/*
 *To encryted the symmetric key generated on device
 */
COMMAND_HANDLER(Symkeygen)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdSymKeyGen *a = (Nv3pCmdSymKeyGen *)arg;
    NvU32 BytesLeftToTransfer;
    NvU32 TransferSize;
    NvU8 *pTemp = NULL;
    NvU8 *pData = NULL;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU8 *pDsessionEnc = NULL;
    NvU32 PubKey[64 + 1] = {0};
    NvU8 *pDsessionPoint = NULL;

    BytesLeftToTransfer = a->Length;
    a->Length = RSA_KEY_SIZE;
    PubKey[0] = 0x010001;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    pData = NvOsAlloc(BytesLeftToTransfer);
    if (!pData)
        NV_CHECK_ERROR_CLEAN_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    NvOsMemset(pData, 0, BytesLeftToTransfer);
    pTemp = pData;

    do
    {
        TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        BytesLeftToTransfer;
        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(hSock, pData, TransferSize, 0, 0),
            Nv3pStatus_DataTransferFailure);

        pData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);

    BytesLeftToTransfer = a->Length;

    pDsessionEnc = NvOsAlloc(BytesLeftToTransfer);
    if (!pDsessionEnc)
        NV_CHECK_ERROR_CLEAN_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    NvOsMemset(pDsessionEnc, 0, BytesLeftToTransfer);
    NvOsDebugPrintf("Starting device_session_key encryption \n");

#if NVODM_BOARD_IS_SIMULATION == 0
    NV_CHECK_ERROR_CLEANUP(PerformRng());

    NvOsMemcpy((NvU8 *)g_DSession,
        (NvU8 *)g_RandomNumber, NV3P_AES_HASH_BLOCK_LEN);

    NvRSAEncDecMessage(
        (NvU32 *)pDsessionEnc,
        (NvU32 *)g_DSession,
        PubKey,
        (NvBigIntModulus *)pTemp);
#endif
    pDsessionPoint = pDsessionEnc;
    NvOsDebugPrintf("Completed device_session_key encryption \n");

    while (BytesLeftToTransfer)
    {
        TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        BytesLeftToTransfer;;

        NV_CHECK_ERROR_FAIL_3P(
            Nv3pDataSend(hSock, pDsessionEnc, TransferSize, 0),
            Nv3pStatus_DataTransferFailure);

        BytesLeftToTransfer -= TransferSize;
        pDsessionEnc += TransferSize;
    }
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nSymkeygen failed. NvError %u NvStatus %u\n", e, s);
    if (pTemp)
        NvOsFree(pTemp);
    if (pDsessionPoint)
        NvOsFree(pDsessionPoint);

    return ReportStatus(hSock, Message, s, 0);
}

/*
 *To decrypt the encryted fuse data & do fuse burning
 */
COMMAND_HANDLER(DSession_fuseBurn)
{
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU32 BytesLeftToTransfer;
    NvU32 TransferSize;
    Nv3pCmdDFuseBurn *a = (Nv3pCmdDFuseBurn *)arg;
    NvError e = NvSuccess;
    NvU8 *pFuseData = NULL;
    NvU8 *pBctData = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU8 bctPartId;
    NvU8 *pBctPoint = NULL;
    NvU8 *pFusePoint = NULL;
    NvU32 i = 0;
    NvU32 pData[32];
    NvFuseWrite *pFuseInfo = NULL;

    BytesLeftToTransfer = a->bct_length;

    pBctData = NvOsAlloc(BytesLeftToTransfer);
    if (!pBctData)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    pBctPoint = pBctData;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    NvOsDebugPrintf("Starting DSession_fuseProcessing \n");

    do
    {
        TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        BytesLeftToTransfer;

        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(hSock, pBctData, TransferSize, 0, 0),
            Nv3pStatus_DataTransferFailure);

        pBctData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);

    NvOsDebugPrintf("bct file recieved successfully \n");

    BytesLeftToTransfer = a->fuse_length;
    pFuseData = NvOsAlloc(BytesLeftToTransfer);

    if (!pFuseData)
        NV_CHECK_ERROR_CLEAN_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    pFusePoint = pFuseData;

    do
    {
        TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        BytesLeftToTransfer;

        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(hSock, pFuseData, TransferSize, 0, 0),
            Nv3pStatus_DataTransferFailure);

        pFuseData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);

    NvOsDebugPrintf("fuse file recieved successfully \n");

    Size = sizeof(NvU8);

    NV_CHECK_ERROR_CLEAN_3P(
        NvBctGetData(
            s_pState->BctHandle,
            NvBctDataType_BctPartitionId,
            &Size, &Instance, &bctPartId),
        Nv3pStatus_InvalidBCTPartitionId);

    NV_CHECK_ERROR_CLEAN_3P(LoadPartitionTable(NULL),
        Nv3pStatus_InvalidPartitionTable);

    NvBlUpdateCustomerDataBct(pBctPoint);

    NV_CHECK_ERROR_CLEAN_3P(
        NvBuBctUpdate(
            s_pState->BctHandle,
            s_pState->BitHandle, bctPartId, a->bct_length, pBctPoint),
        Nv3pStatus_BctWriteFailure);

    NvOsDebugPrintf("sync done successfully \n");

    NV_CHECK_ERROR_CLEAN_3P(
        BctEncryptDecrypt(
            (NvU8 *)pFusePoint,
            a->fuse_length,
            0,
            NvCryptoCipherAlgoAesKeyType_UserSpecified,
            (NvU8 *)g_DSession),
        Nv3pStatus_CryptoFailure);

#if NVODM_BOARD_IS_SIMULATION == 0
    pFuseInfo = (NvFuseWrite *)pFusePoint;
    if (pFuseInfo->fuses[0].fuse_type != NvDdkFuseDataType_SkipFuseBurn)
    {
        for (i = 0; pFuseInfo->fuses[i].fuse_type != 0; i++)
        {
            Size = 0;
            NV_CHECK_ERROR_CLEAN_3P(
                NvDdkFuseGet(
                    pFuseInfo->fuses[i].fuse_type,
                    (NvU8 *)pData, &Size),
                Nv3pStatus_BadParameter);

            NV_CHECK_ERROR_CLEAN_3P(
                NvDdkFuseSet(
                    pFuseInfo->fuses[i].fuse_type,
                    (NvU8 *)pFuseInfo->fuses[i].fuse_value, &Size),
                Nv3pStatus_BadParameter);

            if (pFuseInfo->fuses[i].fuse_type == NvDdkFuseDataType_DeviceKey ||
                    pFuseInfo->fuses[i].fuse_type ==
                                                NvDdkFuseDataType_SecureBootKey)
            {
                 NV_CHECK_ERROR_CLEAN_3P(
                    NvDdkFuseProgram(), Nv3pStatus_InvalidState);

                 NvDdkFuseClearArrays();
            }
            if (pFuseInfo->fuses[i].fuse_type == NvDdkFuseDataType_OdmProduction
                    && pFuseInfo->fuses[i + 1].fuse_type != 0)
            {
                NvOsDebugPrintf("security mode should be burned last \n");
                s = Nv3pStatus_BadParameter;
                e = NvError_BadParameter;
                goto clean;
            }
        }
        NvOsDebugPrintf("Programming fuse ...\n");

        NV_CHECK_ERROR_CLEAN_3P(
            NvDdkFuseProgram(), Nv3pStatus_InvalidState);

        NvOsDebugPrintf("Programmed fuse successsfully\n");
    }
    else
    {
        NvOsDebugPrintf("fuse programming bypassed \n");
    }
#endif
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nDSession_FuseBurn failed. NvError %u NvStatus %u\n", e, s);

    if (pFusePoint)
        NvOsFree(pFusePoint);
    if (pBctPoint)
        NvOsFree(pBctPoint);

return ReportStatus(hSock, Message, s, 0);
}

