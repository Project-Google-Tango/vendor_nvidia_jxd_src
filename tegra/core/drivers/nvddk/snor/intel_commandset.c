/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Intel functions for the SNOR device interface</b>
 *
 * @b Description:  Implements the Intel interfacing functions for the SNOR
 * hw interface.
 *
 */

#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"
#include "snor_priv_driver.h"
#include "intel_commandset.h"

/*
 * Intel SNOR functions
 */

static NvBool IntelIsSnorProgramComplete(SnorDeviceIntRegister *pDevIntRegs,NvU32 Address)
{
    NvU16 Status;
    NvBool sr7;
    NvU32 time = 0;

    /* check status for ready bit */
    Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,Address);
    sr7 = READY_BIT_CHECK(Status);
    while (!sr7)
    {
        /* if time has exceeded max timeout time, return failure */
        if(time > INTEL_MAX_TIMEOUT)
            return NV_FALSE;
        /* else wait appropriate minimum time */
        NvOsWaitUS(INTEL_MIN_TIME);
        time += INTEL_MIN_TIME;
        /* then probe ready status bit again */
        Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,Address);
        sr7 = READY_BIT_CHECK(Status);
    }
    /* if we have received ready status bit before timeout then return true */
    return NV_TRUE;
}



SnorCommandStatus IntelFormatDevice (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion)
{
    NvU16 Status;
    NvU32 BlockAdd = 0;
    NvU32 NumOfBlocks = pDevInfo->TotalBlocks;
    /* Erase all Blocks */
    while(NumOfBlocks)
    {
        /* Erase Block */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Block_Erase_Setup);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Block_Erase);

        /* Same Status monitor suffices for Erase, Program operations */
        if(IntelIsSnorProgramComplete(pDevIntRegs, BlockAdd) == NV_FALSE){
            NvOsDebugPrintf("\r\n%s : Format Device Error, Time out while formating", __func__);
            goto fail;
        }

        /* Check Error */
        Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd, BlockAdd);
        if(VPP_RANGE_CHECK(Status) || CMND_SEQ_CHECK(Status) ||
            BLCK_ERASE_CHECK(Status) || BLCK_LOCK_CHECK(Status)){
            NvOsDebugPrintf("\r\n%s : Format Device Error, status (0x%x)", __func__, Status);
            goto fail;
        }

        /* Clear Status Register Error Bits */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

        /* Reset into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

        /* Increment Block */
        BlockAdd += (pDevInfo->EraseSize >> 1);
        NumOfBlocks--;
    }
    return SnorCommandStatus_Success;

fail:
    /* Clear Status Register Error Bits */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

    /* Reset into read mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

    return SnorCommandStatus_EraseError;
}

SnorCommandStatus IntelEraseSector (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 StartOffset,
        NvU32 Length,
        NvBool IsWaitForCompletion)
{
    // Assumed both parameters are aligned to 256K
    // Now taken care in the nvflash config file
    NvU16 Status;
    NvU32 BlockAdd = (StartOffset >> 1) & INTEL_BLOCK_ADDRESS_MASK;
    NvU32 NumOfBlocks = (Length > 0 && Length < pDevInfo->EraseSize) ? 1 : Length / pDevInfo->EraseSize;
    while(NumOfBlocks)
    {
        /* Erase Block */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Block_Erase_Setup);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Block_Erase);

        /* Same Status monitor suffices for Erase, Program operations */
        if(IntelIsSnorProgramComplete(pDevIntRegs, BlockAdd) == NV_FALSE){
            NvOsDebugPrintf("\r\n%s : Erase Error, Time out while erasing", __func__);
            goto fail;
        }

        /* Check Error */
        Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd, BlockAdd);
        if(VPP_RANGE_CHECK(Status) || CMND_SEQ_CHECK(Status) ||
            BLCK_ERASE_CHECK(Status) || BLCK_LOCK_CHECK(Status)){
            NvOsDebugPrintf("\r\n%s : Erase Error, status (0x%x)", __func__, Status);
            goto fail;
        }

        /* Clear Status Register Error Bits */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

        /* Reset into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

        /* Increment Block */
        BlockAdd += (pDevInfo->EraseSize >> 1);
        NumOfBlocks--;
    }
    return SnorCommandStatus_Success;

fail:
    /* Clear Status Register Error Bits */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

    /* Reset into read mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

    return SnorCommandStatus_EraseError;
}

SnorCommandStatus IntelProtectSectors(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 Offset,
        NvU32 Size,
        NvBool IsLastPartition)
{
    NvU8 Attempts = INTEL_LOCK_ATTEMPTS;
    NvU16 LockStatus;
    NvU32 BlockAdd = (Offset >> 1) & INTEL_BLOCK_ADDRESS_MASK;
    NvU32 NumOfBlocks = (Size / pDevInfo->EraseSize);

    /* Lock Blocks */
    while(NumOfBlocks)
    {
        /* Lock Block */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Security_Setup);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Lock_Block);

        /* NVNOTE : There doesn't seem to be a method to check for error (if we even wanted to)
         * even though the documentation claims it to be "optional". Never the less we should
         * clear the status register if there are error bits just to be consistent. */

        /* Clear Status Register Error Bits */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

        /* Check lock status */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Id);
        LockStatus = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd, (BlockAdd + 0x2));

        /* Reset into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

        /* If still unlocked, repeat lock of sector until success */
        if(UNLOCK_CHECK(LockStatus)){
            if(!Attempts){
                NvOsDebugPrintf("\r\n%s : Lock Error, couldn't lock Block 0x%x", __func__, BlockAdd);
                return SnorCommandStatus_LockError;
            }
            Attempts--;
            continue;
        }

        /* Increment sector */
        BlockAdd += (pDevInfo->EraseSize >> 1);
        NumOfBlocks--;

        /* Reset lock attempts */
        Attempts = INTEL_LOCK_ATTEMPTS;
    }

    return SnorCommandStatus_Success;
}

SnorCommandStatus IntelUnprotectAllSectors(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo)
{
    NvU8 Attempts = INTEL_LOCK_ATTEMPTS;
    NvU16 LockStatus;
    NvU32 BlockAdd = 0;
    NvU32 NumOfBlocks = pDevInfo->TotalBlocks;

    /* Unlock all Blocks */
    while(NumOfBlocks)
    {
        /* Unlock Block */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Security_Setup);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Unlock_Block);

        /* NVNOTE : There doesn't seem to be a method to check for error (if we even wanted to)
         * even though the documentation claims it to be "optional". Never the less we should
         * clear the status register if there are error bits just to be consistent. */

        /* Clear Status Register Error Bits */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Clear_Status_Register);

        /* Check lock status */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Id);
        LockStatus = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd, (BlockAdd + 0x2));

        /* Reset into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

        /* If still locked, repeat lock of sector until success */
        if(LOCK_CHECK(LockStatus)){
            if(!Attempts){
                NvOsDebugPrintf("\r\n%s : Unlock Error, couldn't unlock Block 0x%x", __func__, BlockAdd);
                return SnorCommandStatus_LockError;
            }
            Attempts--;
            continue;
        }

        /* Increment sector */
        BlockAdd += (pDevInfo->EraseSize >> 1);
        NumOfBlocks--;

        /* Reset lock attempts */
        Attempts = INTEL_LOCK_ATTEMPTS;
    }

    return SnorCommandStatus_Success;
}

SnorCommandStatus IntelProgram (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 ByteOffset,
        NvU32 SizeInBytes,
        const NvU8* Buffer,
        NvBool IsWaitForCompletion)
{
    // Programming can happen only in 16 bit words
    NvU32 WordOffset = ByteOffset >> 1, TempWordOffset, NumWords;
    NvS32 SizeInWords = SizeInBytes >> 1;
    NvU32 SectorAddress;
    NvU16* pWriteData = (NvU16*)Buffer;
    NvU16 Status;

    /* Intel device supports 16 words buffer. Following conditions
     * are to be met for buffered writes
     * Max of 512 words can be programmed at once.
     * The sector address of all addresses should be the same
     * i.e; Only words in the same sector can be in the buffer
     */
    while(SizeInWords > 0)
    {
        TempWordOffset = WordOffset;
        SectorAddress = WordOffset & INTEL_SECTOR_ADDRESS_MASK;

        /* Write Setup */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress, SnorCmd_Intel_Buffered_Program_Setup);

        /* Calculate the number of words in current block */
        for( NumWords=0 ;
             (NumWords < INTEL_BUFFER_SIZE) && ((TempWordOffset & INTEL_SECTOR_ADDRESS_MASK) == SectorAddress);
             NumWords++, TempWordOffset++) ;
        if(!NumWords)
            break;
        NumWords = ((NvS32)NumWords > SizeInWords) ? (NvU32)SizeInWords : NumWords;
        SizeInWords -= NumWords;

        /* Write Number of words */
        /* Actually NumWords-1 */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,SectorAddress,NumWords-1);

        /* Fill up the buffer */
        for( ; NumWords; WordOffset++, pWriteData++, NumWords--)
            SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,WordOffset,*pWriteData);

        /* Program to flash */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress,SnorCmd_Intel_Buffered_Program);

        /* Wait for Completion */
        if(IntelIsSnorProgramComplete(pDevIntRegs,SectorAddress) == NV_FALSE){
            NvOsDebugPrintf("\r\n%s : Buffer Write Error, Time out while writing", __func__);
            goto fail;
        }

        /* Check Error */
        Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,SectorAddress);
        if(VPP_RANGE_CHECK(Status)){
            NvOsDebugPrintf("\r\n%s : Buffer write error, VPP Range Error status (0x%x)", __func__, Status);
            goto fail;
        }
        if(PRGRM_ERR_CHECK(Status)){
            NvOsDebugPrintf("\r\n%s : Buffer write error, Programming Error status (0x%x)", __func__, Status);
            if(OBJ_OVRWRT_CHECK(Status)){
                NvOsDebugPrintf("\r\n%s : Attempting to write to Object Mode Sector, who has previously been written to, without erasing (0x%x)", __func__, Status);
            }
            goto fail;
        }
        if(BLCK_LOCK_CHECK(Status)){
            NvOsDebugPrintf("\r\n%s : Buffer write error, Writing to Locked Block Error status (0x%x)", __func__, Status);
            goto fail;
        }

        /* Clear Status Register Error Bits */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress, SnorCmd_Intel_Clear_Status_Register);

        /* Reset into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress, SnorCmd_Intel_Read_Array);
    }

    return SnorCommandStatus_Success;

fail:
    /* Clear Status Register Error Bits */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress, SnorCmd_Intel_Clear_Status_Register);

    /* Reset into read mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, SectorAddress, SnorCmd_Intel_Read_Array);

    return SnorCommandStatus_Error;
}

SnorCommandStatus IntelReadMode(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion)
{
    SnorCommandStatus e = SnorCommandStatus_Success;
    NvU32 BlockAdd = 0;
    NvU32 NumOfBlocks = pDevInfo->TotalBlocks;

    /* Reset all Blocks */
    while(NumOfBlocks)
    {
        /* Reset the block into read mode */
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, BlockAdd, SnorCmd_Intel_Read_Array);

        /* Increment sector */
        BlockAdd += (pDevInfo->EraseSize >> 1);
        NumOfBlocks--;
    }
    return e;
}
