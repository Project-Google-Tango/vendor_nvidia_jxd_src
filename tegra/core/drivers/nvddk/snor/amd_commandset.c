/*
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
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
 *           AMD functions for the SNOR device interface</b>
 *
 * @b Description:  Implements the AMD interfacing functions for the SNOR
 * hw interface.
 *
 */

#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"
#include "snor_priv_driver.h"
#include "amd_commandset.h"

/*
 * AMD SNOR functions
 */


static NvBool AMDIsSnorProgramComplete(SnorDeviceIntRegister *pDevIntRegs)
{
    NvU16 Status;
    NvBool LastDQ6ToggleState;
    NvBool CurrentDQ6ToggleState;

    Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x00) & 0xFFFF;
    LastDQ6ToggleState = (Status >> 6)& 0x1;
    /*
     * NOTE: No recommended time out given by AMD. Letting our code spin
     */
    while (1)
    {
        Status = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x00) & 0xFF;
        CurrentDQ6ToggleState = (Status >> 6)& 0x1;
        if (LastDQ6ToggleState == CurrentDQ6ToggleState)
            break;
        LastDQ6ToggleState = CurrentDQ6ToggleState;
    }
    return NV_TRUE;
}

SnorCommandStatus AMDFormatDevice (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion)
{
    SnorCommandStatus e = SnorCommandStatus_Success;
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x555, SnorCmd_AMD_Erase_Setup);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x555, SnorCmd_AMD_Chip_Erase);
    // Same Status monitor suffices for Erase, Program operations
    if(AMDIsSnorProgramComplete(pDevIntRegs) == NV_FALSE)
        e = SnorCommandStatus_EraseError;
    // Reset into read mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x00, SnorCmd_AMD_Read_Array);
    return e;
}

SnorCommandStatus AMDEraseSector (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 StartOffset,
        NvU32 Length,
        NvBool IsWaitForCompletion)
{
    // Assumed both parameters are aligned to 128K
    // Now taken care in the nvflash config file
    NvU32 SectorAdd = StartOffset;
    NvU32 NumOfSector = Length / AMD_SECTOR_SIZE ;
    while(NumOfSector)
    {
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x555, SnorCmd_AMD_Erase_Setup);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,(SectorAdd >>1), SnorCmd_AMD_Sector_Erase);
        // Same Status monitor suffices for Erase, Program operations
        if(AMDIsSnorProgramComplete(pDevIntRegs) == NV_FALSE)
            goto fail;
        // Reset into read mode
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x00, SnorCmd_AMD_Read_Array);
        SectorAdd += AMD_SECTOR_SIZE;
        NumOfSector --;
    }
    return SnorCommandStatus_Success;

fail:
    // Reset into read mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd, 0x00, SnorCmd_AMD_Read_Array);

    return SnorCommandStatus_EraseError;
}

SnorCommandStatus AMDProtectSectors(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 Offset,
        NvU32 Size,
        NvBool IsLastPartition)
{
    NvError e = NvSuccess;
    volatile NvU16 Status;
    void *pNorBaseAdd = (void *)pDevIntRegs->pNorBaseAdd;
    NvU32 StartSectorAdd = Offset & ~(AMD_SECTOR_SIZE - 1);
    NvU32 SectorAdd, EndSectorAdd = (Offset + Size - 1) & ~(AMD_SECTOR_SIZE - 1);
    // Reset the device into read mode
    SNOR_FLASH_WRITE16(pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    /* NVTODO : Factory reset is 'presistent' protection
     * but we should set this non-volatile bit to ensure
     * no other SW can change this to password protection
     */
    for (SectorAdd = StartSectorAdd; SectorAdd <= EndSectorAdd; SectorAdd += AMD_SECTOR_SIZE)
    {
        NvBool LastDQ6ToggleState;
        NvBool CurrentDQ6ToggleState, DQ5State, DQ0State;
        // Enter PPB Global Non-Volatile mode
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_PPB_Entry);
        // Program the PPB bit for this sector
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x0, SnorCmd_AMD_PPB_Program_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, (SectorAdd >> 1), SnorCmd_AMD_PPB_Program_1);
        // Wait until operation is complete
        Status = SNOR_FLASH_READ16(pNorBaseAdd, 0x0) & 0xFFFF;
        LastDQ6ToggleState = (Status >> 6)& 0x1;
        while (1)
        {
            // FIXME : Add a timeout.
            Status = SNOR_FLASH_READ16(pNorBaseAdd, 0x0) & 0xFF;
            CurrentDQ6ToggleState = (Status >> 6)& 0x1;
            DQ5State = (Status >> 5)& 0x1;
            // If DQ6 does not toggle
            if (LastDQ6ToggleState == CurrentDQ6ToggleState)
            {
                NvOsWaitUS(AMD_LOCK_WAIT_TIME);
                Status = SNOR_FLASH_READ16(pNorBaseAdd, (SectorAdd >> 1)) & 0xFFFF;
                DQ0State = (Status >> 0)& 0x1;
                if (DQ0State)
                {
                    e = NvError_InvalidState;
                }
                break;
            }
            // If DQ6 toggles && DQ5 is equal to 1
            else if (DQ5State)
            {
                Status = SNOR_FLASH_READ16(pNorBaseAdd, 0x0) & 0xFFFF;
                LastDQ6ToggleState = (Status >> 6)& 0x1;
                Status = SNOR_FLASH_READ16(pNorBaseAdd, 0x0) & 0xFF;
                CurrentDQ6ToggleState = (Status >> 6)& 0x1;
                // If DQ6 toggles, error out
                if (LastDQ6ToggleState != CurrentDQ6ToggleState)
                {
                    e = NvError_InvalidState;
                }
                else
                {
                    Status = SNOR_FLASH_READ16(pNorBaseAdd, (SectorAdd >> 1)) & 0xFFFF;
                    DQ0State = (Status >> 0)& 0x1;
                    if (DQ0State)
                    {
                        e = NvError_InvalidState;
                    }
                }
                break;
            }
            LastDQ6ToggleState = CurrentDQ6ToggleState;
        }
        if (e != NvSuccess)
        {
            goto fail;
        }
        // Exit PPB Global Non-Volatile mode
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_1);
    }
    /* Is this is the last partition on the device
     * freeze the PPB Lock bit
     */
    if (IsLastPartition == NV_TRUE) {
        // Program Global volatile freeze bit to 0
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_PPBL_Entry);
        // Set the the bit
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_PPBL_Set_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_PPBL_Set_1);
        // Wait until the Lock is set (0)
        while (SNOR_FLASH_READ16(pNorBaseAdd, 0x00));
        // Exit PPB Global volatile-freeze mode
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_1);
        // Enter PPB Global Non-Volatile mode
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_PPB_Entry);
        // Exit PPB Global Non-Volatile mode
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_0);
        SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_1);
    }
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Read_Array);
    return (SnorCommandStatus)e;
fail :
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Read_Array);
    // Exit PPB Global Non-Volatile mode
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_0);
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_1);
    return (SnorCommandStatus)e;
}

SnorCommandStatus AMDUnprotectAllSectors(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo)
{
    NvU16 Status = 0;
    void *pNorBaseAdd = (void*)pDevIntRegs->pNorBaseAdd;
    // Enter PPB Global Non-Volatile mode
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_Command_Sequence_0);
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x2AA, SnorCmd_AMD_Command_Sequence_1);
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x555, SnorCmd_AMD_PPB_Entry);
    // Erasure of PPB bits
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x0, SnorCmd_AMD_PPB_All_Erase_0);
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x0, SnorCmd_AMD_PPB_All_Erase_1);
    // FIXME : Wait for status.
    NvOsWaitUS(AMD_UNLOCK_WAIT_TIME);
    // Exit PPB Global Non-Volatile mode
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_0);
    SNOR_FLASH_WRITE16(pNorBaseAdd, 0x00, SnorCmd_AMD_Exit_1);
    return Status;
}

SnorCommandStatus AMDProgram (
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 ByteOffset,
        NvU32 SizeInBytes,
        const NvU8* Buffer,
        NvBool IsWaitForCompletion)
{
    // 16-bit A/D split SNOR.
    // Programming can happen only in 16 bit words
    NvU32 WordOffset = ByteOffset >> 1, TempWordOffset, NumWords;
    NvS32 SizeInWords = SizeInBytes >> 1;
    NvU32 SectorAddress; //A_max - A_4
    NvU16* pWriteData = (NvU16*)Buffer;
    NvBool IsSuccess;
    /* AMD device supports 16 words buffer. Following conditions
     * are to be met for buffered writes
     * Max of 16 words can be programmed at once.
     * The sector address A_max - A_4 of all addresses should be the same
     * i.e; Only words in the same sector can be in the buffer
     */
    while(SizeInWords > 0)
    {
        // a. Unlock cycles
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x555, SnorCmd_AMD_Command_Sequence_0);
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x2AA, SnorCmd_AMD_Command_Sequence_1);
        // b. Program Address, 0x25
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,WordOffset,SnorCmd_AMD_Write_to_Buffer);
        TempWordOffset = WordOffset;
        SectorAddress = WordOffset & AMD_SECTOR_ADDRESS_MASK;
        // Calculate the number of words in current sector
        for( NumWords=0 ;
             (NumWords <= AMD_BUFFER_SIZE) && ((TempWordOffset & AMD_SECTOR_ADDRESS_MASK) == SectorAddress);
             NumWords++, TempWordOffset++) ;
        if(!NumWords)
            break;
        NumWords = ((NvS32)NumWords > SizeInWords) ? (NvU32)SizeInWords : NumWords;
        SizeInWords -= NumWords;
        // c. Sector Adress, (Word Count - 1)
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,SectorAddress,NumWords-1);
        // d. Fill up the buffer
        for( ; NumWords; WordOffset++, pWriteData++, NumWords--)
            SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,WordOffset,*pWriteData);
        // e. Program to Flash
        SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,SectorAddress,SnorCmd_AMD_Program_Buffer_to_Flash);
        // f. Wait for Completion
        IsSuccess=AMDIsSnorProgramComplete(pDevIntRegs);
        if(!IsSuccess)
            return SnorCommandStatus_LockError;
    }
    return SnorCommandStatus_Success;
}

SnorCommandStatus AMDReadMode(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion)
{
    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    return SnorCommandStatus_Success;
}
