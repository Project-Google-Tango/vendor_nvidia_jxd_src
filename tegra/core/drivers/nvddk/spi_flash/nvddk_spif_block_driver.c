/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** @file
 * @brief <b>Spi flash Block Driver</b>
 *
 * @b Description: Contains SPI flash block driver implementation.
 */

#include "nvddk_blockdev.h"
#include "nvrm_spi.h"
#include "nvddk_spif.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvodm_query.h"
#include "nvddk_spi.h"

/* Enable the following flag to enable debug prints */
#define ENABLE_DEBUG_PRINTS 0

#if ENABLE_DEBUG_PRINTS
#define SPIF_DEBUG_PRINT(x) NvOsDebugPrintf x
#else
#define SPIF_DEBUG_PRINT(x)
#endif

/* Error prints are enabled by default */
#define SPIF_ERROR_PRINT(x) NvOsDebugPrintf x

/* Macro to convert constant to a string */
#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))

/* Bits per packet in the RM SPI transaction */
#define SPI_BITS_PER_PACKET 8

/* SPI flash read command size in bytes */
#define SPIF_READ_COMMAND_SIZE   0x4

/* SPI flash write command size in bytes */
#define SPIF_WRITE_COMMAND_SIZE   0x4

/* Bytes to read when DeviceID command */
#define SPIF_DEVICEID_READ_BYTES 8

/* Structure to store the info related to the SPI flash device */
typedef struct SpifDevRec
{
    /* Handle to the block device manager.
     * Make sure that NvDdkBlockDev is the first
     * element of this structure.
     */
    NvDdkBlockDev BlockDev;

    /* SPI controller instance number */
    NvU32 Instance;

    /* Partition ID */
    NvU32 MinorInstance;

    /* Device ID of the SPI flash device */
    NvU32 DevId;

    /* Manufacture ID of the SPI flash device */
    NvU32 ManId;

    /* Flag to know the read verify write status */
    NvBool IsReadVerifyWriteEnabled;

    /* Handle to the NvRm SPI driver */
    NvRmSpiHandle hRmSpi;

    /* Handle to the NvDdk SPI driver */
    NvDdkSpiHandle hDdkSpi;

    /* Lock for multi process protection */
    NvOsMutexHandle SpifLock;

    /* Partition address in the SPI flash for creating
     * a new partition. The partition address starts from zero
     * and is updated/incremented while allocating a new partition
     * by the size of the newly created partition.
     */
    NvU32 PartitionAddr;

    /* Total number of sectors on the SPI flash */
    NvU32 TotalSectors;

    /* Number of bytes for each block in the SPI flash */
    NvU32 BytesPerBlock;

    /* Write protection type of the SPI flash */
    NvDdkBlockDevWriteProtectionType WriteProtectionType;

    /* Pointer to the local buffer allocated by the SPI
     * block driver for doing the RM SPI transactions.
     */
    NvU8* Buf;

    /* Pointer to the SPI block driver ODM configuration */
    const NvOdmQuerySpifConfig *pSpifOdmConf;

    /* Indicates if the driver is opened or not */
    NvBool IsOpen;
} SpifDev;

/* Structure to store the info related SPI block driver */
typedef struct SpifInfoRec
{
    /* RM Handle passed from block device manager */
    NvRmDeviceHandle hRm;

    /* Maximum SPI controller instances on the SOC */
    NvU32 MaxInstances;

    /* Indicates if the block driver is initialized */
    NvBool IsInitialized;

    /* Pointer to the list of spi devices */
    SpifDev *SpifDevList;

    /* Io module */
    NvU32 IoModule;
} SpifInfo;

/* Contains the spi block driver info */
static SpifInfo s_SpifInfo;

static NvError
SpifRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors);
static NvError
SpifWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors);
static void
SpifGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo);

/***************** Utility functions *****************/
static NvError
SpifUtilReadStatus(
    SpifDev *pSpifDev)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU32 timeout = 0;
    NvError e = NvError_Timeout;
    NvU8 cmd;
    NvU8 ReadBuf[2];

    SPIF_DEBUG_PRINT(("SpifUtilReadStatus..\n"));

    cmd = pSpifConf->ReadStatusCmd;
#define SPI_BUSY_MAX_TIMEOUT_USEC   15000000
#define SPI_BUSY_POLL_STEP_USEC 100
    while (timeout < SPI_BUSY_MAX_TIMEOUT_USEC)
    {
#ifndef NV_USE_DDK_SPI
        NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz, ReadBuf,
            &cmd, 2, SPI_BITS_PER_PACKET);
#else
        NvDdkSpiTransaction(pSpifDev->hDdkSpi,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz, ReadBuf,
            &cmd, 2, SPI_BITS_PER_PACKET);
#endif
        if (!(ReadBuf[1] & pSpifConf->DeviceBusyStatus))
        {
            e = NvSuccess;
            break;
        }
        NvOsWaitUS(SPI_BUSY_POLL_STEP_USEC);
        timeout +=SPI_BUSY_POLL_STEP_USEC ;
    }
#undef  SPI_BUSY_POLL_STEP_USEC
#undef  SPI_BUSY_MAX_TIMEOUT_USEC
    return e;
}

static void SpifUtilWriteEnable(SpifDev *pSpifDev)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU8 cmd;

    SPIF_DEBUG_PRINT(("SpifUtilWriteEnable..\n"));

    cmd = pSpifConf->WriteEnableCmd;

#ifndef NV_USE_DDK_SPI
    NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#else
    NvDdkSpiTransaction(pSpifDev->hDdkSpi,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#endif
}

static void SpifUtilWriteDisable(SpifDev *pSpifDev)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU8 cmd;

    SPIF_DEBUG_PRINT(("SpifUtilWriteDisable..\n"));

    cmd = pSpifConf->WriteDisableCmd;

#ifndef NV_USE_DDK_SPI
     NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#else
     NvDdkSpiTransaction(pSpifDev->hDdkSpi,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#endif
}

static NvError
SpifUtilWriteStatus(
    SpifDev *pSpifDev,
    NvU8 status)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU8 cmd[2];
    NvError e = NvSuccess;

    SPIF_DEBUG_PRINT(("SpifUtilWriteStatus..\n"));

    SpifUtilWriteEnable(pSpifDev);

    cmd[0] = pSpifConf->WriteStatusCmd;
    cmd[1] = status;

#ifndef NV_USE_DDK_SPI
    NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        NULL, cmd, 2, SPI_BITS_PER_PACKET);
#else
    NvDdkSpiTransaction(pSpifDev->hDdkSpi,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        NULL, cmd, 2, SPI_BITS_PER_PACKET);
#endif

    e = SpifUtilReadStatus(pSpifDev);
    SpifUtilWriteDisable(pSpifDev);
    return e;
}

static NvU32
SpifUtilReadDeviceId(
    SpifDev *pSpifDev)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU32 DevId;
    NvU8 cmd[SPIF_DEVICEID_READ_BYTES];

    NvOsMemset(cmd, 0, sizeof(cmd));
    /* Read the device Id */
    cmd[0] = pSpifConf->ReadDeviceIdCmd;
    cmd[1] = 0;
    cmd[2] = 0;
    cmd[3] = 0;

#ifndef NV_USE_DDK_SPI
    NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        cmd, cmd, SPIF_DEVICEID_READ_BYTES, SPI_BITS_PER_PACKET);
#else
    NvDdkSpiTransaction(pSpifDev->hDdkSpi,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        cmd, cmd, SPIF_DEVICEID_READ_BYTES, SPI_BITS_PER_PACKET);
#endif

    DevId = (NvU32)(((cmd[4]) << 24) |
                    (cmd[5] << 16) |
                    (cmd[6] << 8) |
                    (cmd[7]));
    SPIF_DEBUG_PRINT(("SpifUtilReadDeviceId::DevId = 0x%x\n",DevId));

    return DevId;
}

static NvError
SpifUtilRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU32 BytesRequested = NumberOfSectors * pSpifConf->BytesPerSector;
    NvU8* pReadBuf = (NvU8*)pBuffer;
    NvU32 BytesToBeTransferred = 0;
    NvU8  cmd[4];
    NvU32 SectorAddr = SectorNum * pSpifConf->BytesPerSector;

    SPIF_DEBUG_PRINT(("SpifUtilRead..\n"));

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pReadBuf);
    NV_ASSERT(NumberOfSectors);
    /* Check if requested read sectors exceed the total sectors on the device */
    NV_ASSERT(NumberOfSectors <= pSpifDev->TotalSectors);

    if ((SectorNum + NumberOfSectors) > pSpifDev->TotalSectors)
    {
        /* Requested read sectors crossing the device boundary */
        NV_ASSERT(!"Trying to read more than SPI flash device size\n");
        SPIF_ERROR_PRINT(("SPIF ERROR: Trying to read more than SPI flash \
            device size..\n"));

        return NvError_BadParameter;
    }

    while (BytesRequested)
    {
        /* Issue read of one sector at a time */
        BytesToBeTransferred =
            (BytesRequested > pSpifConf->BytesPerSector) ?
            pSpifConf->BytesPerSector : BytesRequested;

        /* Frame the read command */
        cmd[0] = pSpifConf->ReadDataCmd;
        cmd[1] = (SectorAddr >> 16) & 0xFF;
        cmd[2] = (SectorAddr >> 8) & 0xFF;
        cmd[3] = SectorAddr & 0xFF;

#ifndef NV_USE_DDK_SPI
        NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            pSpifDev->Buf, cmd,
            (BytesToBeTransferred + SPIF_READ_COMMAND_SIZE),
            SPI_BITS_PER_PACKET);
#else
        NvDdkSpiTransaction(pSpifDev->hDdkSpi,
            pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
            pSpifDev->Buf, cmd,
            (BytesToBeTransferred + SPIF_READ_COMMAND_SIZE),
            SPI_BITS_PER_PACKET);
#endif

        NvOsMemcpy(pReadBuf, (pSpifDev->Buf + SPIF_READ_COMMAND_SIZE),
                    BytesToBeTransferred);

        pReadBuf += BytesToBeTransferred;
        BytesRequested -= BytesToBeTransferred;

        /* Increment the sector address */
        SectorAddr += BytesToBeTransferred;
    }
    return e;
}

static NvError
SpifUtilWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvU32 BytesRequested = NumberOfSectors * pSpifConf->BytesPerSector;
    NvU8* pWriteBuf = (NvU8*)pBuffer;
    NvU32 BytesToBeTransferred = 0;
    NvU32 PageAddr = SectorNum * pSpifConf->BytesPerSector;

    SPIF_DEBUG_PRINT(("SpifUtilWrite::SectorNum=%d::NumberOfSectors = %d\n",SectorNum, NumberOfSectors));
    NV_ASSERT(hBlockDev);
    NV_ASSERT(pWriteBuf);
    NV_ASSERT(NumberOfSectors);

    /* Check if requested write sectors exceed the total sectors on device */
    NV_ASSERT(NumberOfSectors <= pSpifDev->TotalSectors);

    if ((SectorNum + NumberOfSectors) > pSpifDev->TotalSectors)
    {
        /* Requested write sectors crossing the device boundary */
        NV_ASSERT(!"Trying to program more than SPI flash device size\n");
        SPIF_ERROR_PRINT(("SPIF ERROR: Trying to program more than SPI flash \
            device size..\n"));

        NvOsMutexUnlock(pSpifDev->SpifLock);
        return NvError_BadParameter;
    }

    while (BytesRequested)
    {
        /* Issue write enable before every write/program command. This is
         * required to support flashes which does automatic write disable
         * after write/program command.
         */
        SpifUtilWriteEnable(pSpifDev);

        /* Program one page size */
        BytesToBeTransferred =
            (BytesRequested > pSpifConf->BytesPerPage) ?
            pSpifConf->BytesPerPage : BytesRequested;

        /* Frame the page program command */
        pSpifDev->Buf[0] = pSpifConf->WriteDataCmd;
        pSpifDev->Buf[1] = (PageAddr >> 16) & 0xFF;
        pSpifDev->Buf[2] = (PageAddr >> 8) & 0xFF;
        pSpifDev->Buf[3] = PageAddr & 0xFF;

        NvOsMemcpy((pSpifDev->Buf + SPIF_WRITE_COMMAND_SIZE),
                    pWriteBuf,BytesToBeTransferred);

#ifndef NV_USE_DDK_SPI
        NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf,
                (BytesToBeTransferred + SPIF_WRITE_COMMAND_SIZE),
                SPI_BITS_PER_PACKET);
#else
        NvDdkSpiTransaction(pSpifDev->hDdkSpi,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf,
                (BytesToBeTransferred + SPIF_WRITE_COMMAND_SIZE),
                SPI_BITS_PER_PACKET);
#endif

        NV_CHECK_ERROR_CLEANUP(SpifUtilReadStatus(pSpifDev));
        pWriteBuf += BytesToBeTransferred;
        BytesRequested -= BytesToBeTransferred;

        /* Increment the page address */
        PageAddr += BytesToBeTransferred;
    }

fail:
    SpifUtilWriteDisable(pSpifDev);
    return e;
}

static NvError
SpifUtilRdWrSector(
    SpifDev *pSpifDev,
    NvU32 Opcode,
    NvU32 InputSize,
    const void * InputArgs)
{
    NvError e = NvSuccess;
    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pRdPhysicalIn =
        (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;
    NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pWrPhysicalIn =
        (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;

    SPIF_DEBUG_PRINT(("SpifUtilRdWrSector..\n"));

    /* Data is read from/written on chip block size at max each time */
    if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
    {
        NV_ASSERT(InputSize ==
            sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs));
        e = SpifUtilRead(&(pSpifDev->BlockDev),
                        pRdPhysicalIn->SectorNum,
                        pRdPhysicalIn->pBuffer,
                        pRdPhysicalIn->NumberOfSectors);
    }
    else if (Opcode == NvDdkBlockDevIoctlType_WritePhysicalSector)
    {
        NV_ASSERT(InputSize ==
            sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs));
        e = SpifUtilWrite(&(pSpifDev->BlockDev),
                        pWrPhysicalIn->SectorNum,
                        pWrPhysicalIn->pBuffer,
                        pWrPhysicalIn->NumberOfSectors);
    }
    else
    {
        SPIF_ERROR_PRINT(("SPIF ERROR: Invalid Opcode[0x%x] \n", Opcode));
        NV_ASSERT(!"SPI flash inavlid Opcode\n");
    }
    return e;
}

static NvError
SpifUtilFormatDevice(SpifDev* pSpifDev)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvError e = NvSuccess;
    NvU8 cmd;

    SPIF_DEBUG_PRINT(("SpifUtilFormatDevice..\n"));
    NV_ASSERT(pSpifDev);

    SpifUtilWriteEnable(pSpifDev);

    /* Chip Erase */
    cmd = pSpifConf->ChipEraseCmd;

#ifndef NV_USE_DDK_SPI
    NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#else
    NvDdkSpiTransaction(pSpifDev->hDdkSpi,
        pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
        NULL, &cmd, 1, SPI_BITS_PER_PACKET);
#endif

    e = SpifUtilReadStatus(pSpifDev);
    SpifUtilWriteDisable(pSpifDev);
    return e;
}

static NvError
SpifUtilEraseSectors(
    SpifDev *pSpifDev,
    NvU32 StartSector,
    NvU32 NumOfSectors)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvError e = NvSuccess;
    NvU32 count = NumOfSectors;
    NvU32 Addr = StartSector * pSpifConf->BytesPerSector;

    SPIF_DEBUG_PRINT(("SpifUtilEraseSectors..\n"));

    NV_ASSERT(pSpifDev);
    NV_ASSERT(count);
    /* Check if requested erase sectors exceed the
     * total sectors on the device.
     */
    NV_ASSERT(count <= pSpifDev->TotalSectors);

    if (count == pSpifDev->TotalSectors)
    {
        if (StartSector == 0)
        {
            /* Erase the entire device */
            SPIF_DEBUG_PRINT(("SPIF ERROR: Erasing the entire device..\n"));
            e = SpifUtilFormatDevice(pSpifDev);
        }
        else
        {
            /* Requested erase sectors crossing the device boundary */
            e = NvError_BadParameter;

            SPIF_ERROR_PRINT(("SPIF ERROR: Trying to erase more than chipsize \
                NumberOfSectors[0x%x] TotalBlocks[0x%x]\n", \
                count, pSpifConf->TotalBlocks));
            NV_ASSERT(!"SPIF Trying to erase more than chip size");
        }
    }
    else
    {
        while (count)
        {
            if (count >=  pSpifConf->SectorsPerBlock)
            {
                pSpifDev->Buf[0] = pSpifConf->BlockEraseCmd;
                pSpifDev->Buf[1] = (Addr >> 16) & 0xFF;
                pSpifDev->Buf[2] = (Addr >> 8) & 0xFF;
                pSpifDev->Buf[3] = Addr & 0xFF;
                Addr += pSpifDev->BytesPerBlock;
                count -= pSpifConf->SectorsPerBlock;
            }
            else
            {
                pSpifDev->Buf[0] = pSpifConf->SectorEraseCmd;
                pSpifDev->Buf[1] = (Addr >> 16) & 0xFF;
                pSpifDev->Buf[2] = (Addr >> 8) & 0xFF;
                pSpifDev->Buf[3] = Addr & 0xFF;
                Addr += pSpifConf->BytesPerSector;
                count--;
            }
            /* Issue write enable before every erase command. This is
             * required to support flashes which does automatic write disable
             * after erase command.
             */
            SpifUtilWriteEnable(pSpifDev);
#ifndef NV_USE_DDK_SPI
            NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf, 4, SPI_BITS_PER_PACKET);
#else
                NvDdkSpiTransaction(pSpifDev->hDdkSpi,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf, 4, SPI_BITS_PER_PACKET);
#endif
            NV_CHECK_ERROR_CLEANUP(SpifUtilReadStatus(pSpifDev));
        }
    }

fail:
    SpifUtilWriteDisable(pSpifDev);
    return e;
}

static NvError
SpifUtilEraseBlocks(
    SpifDev *pSpifDev,
    NvU32 StartBlock,
    NvU32 NumOfBlocks)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvError e = NvSuccess;
    NvU32 count = NumOfBlocks;
    NvU32 Addr = StartBlock * pSpifDev->BytesPerBlock;

    SPIF_DEBUG_PRINT(("SpifUtilEraseBlocks..\n"));

    NV_ASSERT(pSpifDev);
    NV_ASSERT(count);
    /* Check if requested erase blocks exceed the
     * total blocks on the device.
     */
    NV_ASSERT(count <= pSpifConf->TotalBlocks);

    if (count == pSpifConf->TotalBlocks)
    {
        if (StartBlock == 0)
        {
            /* Erase the entire device */
            SPIF_DEBUG_PRINT(("SPIF ERROR: Erasing the entire device..\n"));
            e = SpifUtilFormatDevice(pSpifDev);
        }
        else
        {
            /* Requested erase blocks crossing the device boundary */
            e = NvError_BadParameter;

            SPIF_ERROR_PRINT(("SPIF ERROR: Trying to erase more than chipsize \
                NumberOfBlocks[0x%x] TotalBlocks[0x%x]\n",
                count, pSpifConf->TotalBlocks));
            NV_ASSERT(!"SPIF Trying to erase more than chip size");
        }
    }
    else
    {
        while (count)
        {
            pSpifDev->Buf[0] = pSpifConf->BlockEraseCmd;
            pSpifDev->Buf[1] = (Addr >> 16) & 0xFF;
            pSpifDev->Buf[2] = (Addr >> 8) & 0xFF;
            pSpifDev->Buf[3] = Addr & 0xFF;
            Addr += pSpifDev->BytesPerBlock;
            count--;

            /* Issue write enable before every erase command. This is
             * required to support flashes which does automatic write disable
             * after erase command.
             */
            SpifUtilWriteEnable(pSpifDev);
#ifndef NV_USE_DDK_SPI
            NvRmSpiTransaction(pSpifDev->hRmSpi, pSpifConf->SpiPinMap,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf, 4, SPI_BITS_PER_PACKET);
#else
            NvDdkSpiTransaction(pSpifDev->hDdkSpi,
                pSpifConf->SpiChipSelectId, pSpifConf->SpiSpeedKHz,
                NULL, pSpifDev->Buf, 4, SPI_BITS_PER_PACKET);
#endif
            NV_CHECK_ERROR_CLEANUP(SpifUtilReadStatus(pSpifDev));
        }
    }

fail:
    SpifUtilWriteDisable(pSpifDev);
    return e;
}

static NvError
SpifUtilLockSectors(
    SpifDev *pSpifDev,
    NvU32 StartSector,
    NvU32 NumberOfSectors,
    NvBool EnableLocks)
{
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
    NvError e = NvSuccess;
    NvU32 DevId = 0;
    NvU32 WpStatus = 0;
    NvU32 StartBlock = StartSector/pSpifConf->SectorsPerBlock;
    NvU32 NumberOfBlocks = NumberOfSectors/pSpifConf->SectorsPerBlock;
    SPIF_DEBUG_PRINT(("SpifUtilLockSectors..\n"));

    DevId = SpifUtilReadDeviceId(pSpifDev);
    WpStatus = NvOdmQuerySpifWPStatus(DevId,
                    StartBlock, NumberOfBlocks);

    /* TODO: Take Write protection type into account */
    SpifUtilWriteStatus(pSpifDev, WpStatus);

    return e;
}

static NvError
SpifConfigureWriteProtection(
    SpifDev* pSpifDev,
    NvU32 WriteProtectionType)
{
    SPIF_DEBUG_PRINT(("SpifConfigureWriteProtection..\n"));

    pSpifDev->WriteProtectionType = WriteProtectionType;
    return NvSuccess;
}

static void
UtilGetIoctlName(
    NvU32 Opcode,
    NvU8 *IoctlStr)
{
    switch(Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ReadPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WritePhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            MACRO_GET_STR(IoctlStr,
                NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
            MACRO_GET_STR(IoctlStr,
                NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors);
            break;
        case NvDdkBlockDevIoctlType_FormatDevice:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_FormatDevice);
            break;
        case NvDdkBlockDevIoctlType_PartitionOperation:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_PartitionOperation);
            break;
        case NvDdkBlockDevIoctlType_AllocatePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_AllocatePartition);
            break;
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_EraseLogicalSectors);
            break;
        case NvDdkBlockDevIoctlType_ErasePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePartition);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            MACRO_GET_STR(IoctlStr,
                NvDdkBlockDevIoctlType_WriteVerifyModeSelect);
            break;
        case NvDdkBlockDevIoctlType_LockRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_LockRegion);
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePhysicalBlock);
            break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            MACRO_GET_STR(IoctlStr,
                NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus);
            break;
        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
            MACRO_GET_STR(IoctlStr,
                NvDdkBlockDevIoctlType_VerifyCriticalPartitions);
            break;
        default:
            /* Illegal Ioctl string */
            MACRO_GET_STR(IoctlStr, UnknownIoctl);
            break;
    }
}

/****************** Functions to populate the block driver*****************/
static NvError
SpifOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    SpifDev** hSpifBlkDev)
{
    NvError e = NvSuccess;
    SpifDev *pSpifDev;
    const NvOdmQuerySpifConfig *pSpifConf;
    NvU32 DevId = 0;


    SPIF_DEBUG_PRINT(("SpifOpen::Instance = %d::MinorInstance = %d\n",Instance,MinorInstance));
    NV_ASSERT(Instance < s_SpifInfo.MaxInstances);

    pSpifDev = &s_SpifInfo.SpifDevList[Instance];

    NvOsMutexLock(pSpifDev->SpifLock);

    if (pSpifDev->IsOpen)
    {
        /* Simultaneous opening of the driver by
         * multiple clients is not allowed.
         */
        *hSpifBlkDev = pSpifDev;
        goto fail;
    }

#ifndef NV_USE_DDK_SPI
    NV_CHECK_ERROR_CLEANUP(NvRmSpiOpen(s_SpifInfo.hRm,
                                        s_SpifInfo.IoModule,
                                        Instance,
                                        NV_TRUE,
                                        &pSpifDev->hRmSpi));
#else
    pSpifDev->hDdkSpi = NvDdkSpiOpen(Instance);
#endif

    pSpifDev->Instance = Instance;
    pSpifDev->MinorInstance = MinorInstance;
    pSpifDev->PartitionAddr = 0;

    /* Get the default ODM configuration by passing the device Id as zero */
    pSpifConf = NvOdmQueryGetSpifConfig(Instance, 0);
    if (!pSpifConf)
    {
        SPIF_ERROR_PRINT(("SPIF ERROR: SpifOpen failed..\n"));
        goto fail;
    }

    pSpifDev->pSpifOdmConf = pSpifConf;

    /* Read the SPI flash device ID */
    DevId = SpifUtilReadDeviceId(pSpifDev);
    /* Get the SPI flash device specific ODM configuration by passing the
     * flash device id read.
     */
    pSpifConf = NvOdmQueryGetSpifConfig(Instance, DevId);
    if (!pSpifConf)
    {
        SPIF_ERROR_PRINT(("SPIF ERROR: SpifOpen failed..\n"));
        goto fail;
    }
    pSpifDev->TotalSectors = pSpifConf->TotalBlocks * pSpifConf->SectorsPerBlock;
    pSpifDev->BytesPerBlock = pSpifConf->SectorsPerBlock * pSpifConf->BytesPerSector;

    pSpifDev->Buf = NvOsAlloc(pSpifConf->BytesPerSector +
            NV_MAX((SPIF_READ_COMMAND_SIZE), (SPIF_WRITE_COMMAND_SIZE)));
    if (!pSpifDev->Buf)
    {
        SPIF_ERROR_PRINT(("SPIF ERROR: SpifOpen failed..\n"));
        goto fail;
    }

    pSpifDev->IsOpen = NV_TRUE;
    *hSpifBlkDev = pSpifDev;

fail:
    NvOsMutexUnlock(pSpifDev->SpifLock);
    return e;
}

static void SpifClose(NvDdkBlockDevHandle hBlockDev)
{
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;

    SPIF_DEBUG_PRINT(("SpifClose..\n"));
    NV_ASSERT(hBlockDev);

    NvOsMutexLock(pSpifDev->SpifLock);

    if (!pSpifDev->IsOpen)
    {
        SPIF_ERROR_PRINT(("Trying to close driver without open\n"));
        goto end;
    }

    if (pSpifDev->Buf)
    {
        NvOsFree(pSpifDev->Buf);
        pSpifDev->Buf = NULL;
    }

    NvRmSpiClose(pSpifDev->hRmSpi);
    pSpifDev->hRmSpi = NULL;

    pSpifDev->PartitionAddr = 0;
    pSpifDev->TotalSectors = 0;
    pSpifDev->BytesPerBlock = 0;
    pSpifDev->pSpifOdmConf = NULL;
    pSpifDev->IsOpen = NV_FALSE;
end:
    NvOsMutexUnlock(pSpifDev->SpifLock);
}

static void
SpifGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBlockDevInfo);

    NvOsMutexLock(pSpifDev->SpifLock);

    pBlockDevInfo->BytesPerSector = pSpifConf->BytesPerSector;
    pBlockDevInfo->SectorsPerBlock = pSpifConf->SectorsPerBlock;
    pBlockDevInfo->TotalBlocks = pSpifConf->TotalBlocks;
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Fixed;
    SPIF_DEBUG_PRINT(("SpifGetDeviceInfo..BytesPerSector = %d::SectorsPerBlock=%d::TotalBlocks=%d\n",
            pBlockDevInfo->BytesPerSector, pBlockDevInfo->SectorsPerBlock, pBlockDevInfo->TotalBlocks));

    NvOsMutexUnlock(pSpifDev->SpifLock);
}

static NvError
SpifRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;

    SPIF_DEBUG_PRINT(("SpifRead..SectorNum = %d::NumberOfSectors=%d\n",
            SectorNum, NumberOfSectors));

    NvOsMutexLock(pSpifDev->SpifLock);
    e = SpifUtilRead(hBlockDev, SectorNum, pBuffer, NumberOfSectors);
    NvOsMutexUnlock(pSpifDev->SpifLock);
    return e;
}

static NvError
SpifWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;

    SPIF_DEBUG_PRINT(("SpifWrite..SectorNum = %d::NumberOfSectors=%d\n",
            SectorNum, NumberOfSectors));

    NvOsMutexLock(pSpifDev->SpifLock);
    e = SpifUtilWrite(hBlockDev, SectorNum, pBuffer, NumberOfSectors);
    NvOsMutexUnlock(pSpifDev->SpifLock);
    return e;
}

static void
SpifPowerUp(NvDdkBlockDevHandle hBlockDev)
{
    SPIF_DEBUG_PRINT(("SpifPowerUp..\n"));
    /* Not supported */
}

static void
SpifPowerDown(NvDdkBlockDevHandle hBlockDev)
{
    SPIF_DEBUG_PRINT(("SpifPowerDown..\n"));
    /* Not supported */
}

static  NvError
SpifBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError e = NvSuccess;
    SpifDev* pSpifDev = (SpifDev* ) hBlockDev;
    const NvOdmQuerySpifConfig *pSpifConf = pSpifDev->pSpifOdmConf;
#define MAX_IOCTL_STRING_LENGTH_BYTES   80
    NvU8 IoctlName[MAX_IOCTL_STRING_LENGTH_BYTES];
    NvU32 BlockCount = 0;

    NvOsMutexLock(pSpifDev->SpifLock);
    /* Decode the IOCTL opcode */
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            SPIF_DEBUG_PRINT(("SpifBlockDevIoctlType RdWrSector \
                        Opcode[%d]..\n", Opcode));
            NV_ASSERT(InputArgs);

            /* Check if partition create is done with part id equal to 0 */
            if (pSpifDev->MinorInstance)
            {
                SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType RdWrSector \
                    MinorInstance[%d]..\n", pSpifDev->MinorInstance));
                e = NvError_NotSupported;
                break;
            }
            e = SpifUtilRdWrSector(pSpifDev,
                                Opcode, InputSize, InputArgs);
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
                *pMapPhysicalSectorIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)
                InputArgs;

            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
                *pMapPhysicalSectorOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)
                OutputArgs;
            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs)
                );

            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs)
                );

            /* Check if partition create is done with part id equal to 0 */
            if (pSpifDev->MinorInstance)
            {
                SPIF_DEBUG_PRINT(
                    ("NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector \
                    MinorInstance[%d]..\n", pSpifDev->MinorInstance));
                e = NvError_NotSupported;
                break;
            }
            SPIF_DEBUG_PRINT(
                    ("NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector \
                    PhysicalSectorNum[%d]..\n",
                    pMapPhysicalSectorIn->LogicalSectorNum));

            /* Physical and logical address is the same for SPI flash */
            pMapPhysicalSectorOut->PhysicalSectorNum =
                        pMapPhysicalSectorIn->LogicalSectorNum;
        }
        break;

        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
        {
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs
                *pLogicalBlockIn =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs *)
                InputArgs;

            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs
                *pPhysicalBlockOut =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs *)
                OutputArgs;

            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs)
                );

            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs)
                );

            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors \
                PartitionPhysicalSectorStart[%d] \
                PartitionPhysicalSectorStop[%d]\n",
                pLogicalBlockIn->PartitionLogicalSectorStart,
                pLogicalBlockIn->PartitionLogicalSectorStop));

            pPhysicalBlockOut->PartitionPhysicalSectorStart =
                    pLogicalBlockIn->PartitionLogicalSectorStart;
            pPhysicalBlockOut->PartitionPhysicalSectorStop =
                    pLogicalBlockIn->PartitionLogicalSectorStop;
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
        {
            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_FormatDevice..\n"));
            e = SpifUtilFormatDevice(pSpifDev);
        }
        break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            SPIF_DEBUG_PRINT(("SpifBlockDevIoctl::NvDdkBlockDevIoctlType_PartitionOperation\n"));

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);

            /* Check if partition create is done with part id equal to 0 */
            if (pSpifDev->MinorInstance)
            {
                SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_PartitionOperation \
                        MinorInstance[%d]..\n", pSpifDev->MinorInstance));
                e = NvError_NotSupported;
                break;
            }
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_AllocatePartition.I/P::StartPhysicalSectorAddress = \
                %d::NumLogicalSectors=%d\n",pAllocPartIn->StartPhysicalSectorAddress,
                pAllocPartIn->NumLogicalSectors));

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));

            /* Check if partition create is done with part id equal to 0 */
            if (pSpifDev->MinorInstance)
            {
                SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_AllocatePartition \
                        MinorInstance[%d]..\n", pSpifDev->MinorInstance));
                e = NvError_NotSupported;
                break;
            }

            if (pAllocPartIn->AllocationType ==
                    NvDdkBlockDevPartitionAllocationType_Absolute)
            {
                SPIF_DEBUG_PRINT(
                    ("NvDdkBlockDevPartitionAllocationType_Absolute \
                    StartPhysicalSectorAddress[%d], NumLogicalSectors[%d]..\n",
                    pAllocPartIn->StartPhysicalSectorAddress,
                    pAllocPartIn->NumLogicalSectors));

                pAllocPartOut->StartLogicalSectorAddress =
                    pAllocPartIn->StartPhysicalSectorAddress;
                pAllocPartOut->NumLogicalSectors =
                    pAllocPartIn->NumLogicalSectors;
            }
            else if (pAllocPartIn->AllocationType ==
                        NvDdkBlockDevPartitionAllocationType_Relative)
            {
                SPIF_DEBUG_PRINT(
                    ("NvDdkBlockDevPartitionAllocationType_Relative \
                    StartPhysicalSectorAddress[%d], NumLogicalSectors[%d]..\n",
                    pAllocPartIn->StartPhysicalSectorAddress,
                    pAllocPartIn->NumLogicalSectors));

                pAllocPartOut->StartLogicalSectorAddress =
                        pSpifDev->PartitionAddr;

                /* Address needs to be block aligned */
                BlockCount =
                    pAllocPartIn->NumLogicalSectors/pSpifConf->SectorsPerBlock;
                if (BlockCount * pSpifConf->SectorsPerBlock !=
                        pAllocPartIn->NumLogicalSectors)
                    BlockCount++;

                pAllocPartOut->NumLogicalSectors =
                        BlockCount * pSpifConf->SectorsPerBlock;
                pSpifDev->PartitionAddr += pAllocPartOut->NumLogicalSectors;
                SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_AllocatePartition.O/P.::StartLogicalSectorAddress = %d \
                    ::NumLogicalSectors=%d\n",pAllocPartOut->StartLogicalSectorAddress,
                    pAllocPartOut->NumLogicalSectors));
            }
            else
            {
                SPIF_DEBUG_PRINT(
                    ("SPIF Block driver, Invalid allocation type\n"));
                NV_ASSERT(!"SPIF Block driver, Invalid allocation type");
            }
        }
        break;

        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
        {
            NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;

            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_EraseLogicalSectors..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs));
            NV_ASSERT(ip->NumberOfLogicalSectors);

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_EraseLogicalSectors \
                StartLogicalSector[%d] NumberOfLogicalSectors[%d]\n",
                ip->StartLogicalSector,
                ip->NumberOfLogicalSectors));

            e = SpifUtilEraseSectors(pSpifDev, ip->StartLogicalSector,
                                    ip->NumberOfLogicalSectors);
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePartition:
        {
            NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_ErasePartition..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs));
            NV_ASSERT(ip->NumberOfLogicalSectors);

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_ErasePartition \
                StartLogicalSector[%d] NumberOfLogicalSectors[%d]\n",
                ip->StartLogicalSector,
                ip->NumberOfLogicalSectors));

            e = SpifUtilEraseSectors(pSpifDev, ip->StartLogicalSector,
                        ip->NumberOfLogicalSectors);
        }
        break;

        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
        {
            NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs
                *pWriteVerifyModeIn =
                (NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;
            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_WriteVerifyModeSelect..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs));

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_WriteVerifyModeSelect \
                    IsReadVerifyWriteEnabled[%d] \n",
                    pWriteVerifyModeIn->IsReadVerifyWriteEnabled));

            pSpifDev->IsReadVerifyWriteEnabled =
                pWriteVerifyModeIn->IsReadVerifyWriteEnabled;
        }
        break;

        case NvDdkBlockDevIoctlType_LockRegion:
        {
            NvDdkBlockDevIoctl_LockRegionInputArgs *pLockRegionIn =
                (NvDdkBlockDevIoctl_LockRegionInputArgs *)InputArgs;

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_LockRegion..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_LockRegionInputArgs));
            NV_ASSERT(pLockRegionIn->NumberOfSectors);

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_LockRegion \
                NumberOfSectors[%d] EnableLocks[%d]\n", \
                pLockRegionIn->NumberOfSectors, pLockRegionIn->EnableLocks));

            e = SpifUtilLockSectors(pSpifDev, pLockRegionIn->LogicalSectorStart,
                    pLockRegionIn->NumberOfSectors, pLockRegionIn->EnableLocks);
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
        {
            /* Erases sector */
            NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs
                *pErasePhysicalBlock =
                (NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs *)InputArgs;

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_ErasePhysicalBlock..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs));
            NV_ASSERT(pErasePhysicalBlock->NumberOfBlocks);

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_ErasePhysicalBlock \
                BlockNum[%d] NumberOfBlocks[%d] \n",
                pErasePhysicalBlock->BlockNum,
                pErasePhysicalBlock->NumberOfBlocks));

            e = SpifUtilEraseBlocks(pSpifDev, pErasePhysicalBlock->BlockNum,
                    pErasePhysicalBlock->NumberOfBlocks);
        }
        break;

        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs
                *pPhysicalQueryOut =
                    (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)
                    OutputArgs;

            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus..\n"));

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);

            /* There is no bad block management in SPI flash */
            pPhysicalQueryOut->IsGoodBlock= NV_TRUE;
            pPhysicalQueryOut->IsLockedBlock = NV_FALSE;
        }
        break;

        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
        {
            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_VerifyCriticalPartitions..\n"));
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_ConfigureWriteProtection:
        {
            NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs *pInput =
                (NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs *)
                InputArgs;

            SPIF_DEBUG_PRINT(
                ("NvDdkBlockDevIoctlType_ConfigureWriteProtection..\n"));

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs));

            SPIF_DEBUG_PRINT(("NvDdkBlockDevIoctlType_ConfigureWriteProtection \
                WriteProtectionType[%d] Region[%d] \n", \
                pInput->WriteProtectionType, pInput->Region));

            e = SpifConfigureWriteProtection(pSpifDev,
                    pInput->WriteProtectionType);
        }
        break;

        default:
            SPIF_ERROR_PRINT(("SPIF ERROR: Illegal block driver Ioctl..\n"));
            e = NvError_BlockDriverIllegalIoctl;
        break;
    }
    NvOsMutexUnlock(pSpifDev->SpifLock);

    if (e != NvSuccess)
    {
        SPIF_ERROR_PRINT(("SPIF ERROR: SpifBlockDevIoctl failed \
                            error[0x%x]..\n", e));
        // function to return name corresponding to ioctl opcode
        UtilGetIoctlName(Opcode, IoctlName);
        NvOsDebugPrintf("\r\nInst=%d, SPI Flash ioctl %s failed: \
                error code=0x%x ", pSpifDev->Instance, IoctlName, e);
    }

#undef  MAX_IOCTL_STRING_LENGTH_BYTES
    return e;
}

static void
SpifRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    SPIF_DEBUG_PRINT(("SpifRegisterHotplugSemaphore..\n"));
    /* Not supported */
}

static void SpifFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    SPIF_DEBUG_PRINT(("SpifFlushCache..\n"));
    /* Not supported */
}

/***************** Public Functions ************************/
NvError NvDdkSpifBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvU32 i;

    SPIF_DEBUG_PRINT(("NvDdkSpifBlockDevInit..\n"));

    NV_ASSERT(hDevice);

    if (!s_SpifInfo.IsInitialized)
    {
        s_SpifInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Spi);

        if (s_SpifInfo.MaxInstances)
        {
            s_SpifInfo.IoModule = NvOdmIoModule_Sflash;
        }
        else // For T30 there is no dedicated spi interface for SFlash. Need to use Slink.
        {
            s_SpifInfo.MaxInstances =
                NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Slink);
            s_SpifInfo.IoModule = NvOdmIoModule_Slink;
        }

        s_SpifInfo.SpifDevList = NvOsAlloc(
            sizeof(SpifDev) * s_SpifInfo.MaxInstances);
        if (!s_SpifInfo.SpifDevList)
        {
            e = NvError_InsufficientMemory;

            SPIF_ERROR_PRINT(("SPIF ERROR: NvDdkSpifBlockDevInit \
                    failed error[0x%x]..\n", e));
            goto fail;
        }

        NvOsMemset((void *)s_SpifInfo.SpifDevList, 0,
            (sizeof(SpifDev) * s_SpifInfo.MaxInstances));

        /* Create locks for all the device instances */
        for (i = 0; i < s_SpifInfo.MaxInstances; i++)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvOsMutexCreate(&s_SpifInfo.SpifDevList[i].SpifLock));
        }

        /* Save the RM Handle */
        s_SpifInfo.hRm = hDevice;
        s_SpifInfo.IsInitialized = NV_TRUE;
    }

fail:
    return e;
}

void NvDdkSpifBlockDevDeinit(void)
{
    NvU32 i;

    SPIF_DEBUG_PRINT(("NvDdkSpifBlockDevDeinit..\n"));

    if (s_SpifInfo.IsInitialized)
    {
        /* Free the locks */
        for (i = 0; i < s_SpifInfo.MaxInstances; i++)
        {
            NvOsMutexDestroy(s_SpifInfo.SpifDevList[i].SpifLock);
        }
        NvOsFree(s_SpifInfo.SpifDevList);
        s_SpifInfo.SpifDevList = NULL;

        s_SpifInfo.hRm = NULL;
        s_SpifInfo.MaxInstances = 0;
        s_SpifInfo.IsInitialized = NV_FALSE;
    }
}

NvError
NvDdkSpifBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e;
    SpifDev *hSpifBlkDev = NULL;

    SPIF_DEBUG_PRINT(("NvDdkSpifBlockDevOpen..\n"));

    NV_ASSERT(Instance <= s_SpifInfo.MaxInstances);
    NV_ASSERT(phBlockDev);
    NV_ASSERT(s_SpifInfo.IsInitialized);


    NV_CHECK_ERROR_CLEANUP(SpifOpen(Instance, MinorInstance, &hSpifBlkDev));

    hSpifBlkDev->BlockDev.NvDdkBlockDevClose = &SpifClose;
    hSpifBlkDev->BlockDev.NvDdkBlockDevGetDeviceInfo = &SpifGetDeviceInfo;
    hSpifBlkDev->BlockDev.NvDdkBlockDevReadSector = &SpifRead;
    hSpifBlkDev->BlockDev.NvDdkBlockDevWriteSector = &SpifWrite;
    hSpifBlkDev->BlockDev.NvDdkBlockDevPowerUp = &SpifPowerUp;
    hSpifBlkDev->BlockDev.NvDdkBlockDevPowerDown = &SpifPowerDown;
    hSpifBlkDev->BlockDev.NvDdkBlockDevIoctl = &SpifBlockDevIoctl;
    hSpifBlkDev->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
                                &SpifRegisterHotplugSemaphore;
    hSpifBlkDev->BlockDev.NvDdkBlockDevFlushCache = &SpifFlushCache;

    *phBlockDev = &(hSpifBlkDev->BlockDev);

fail:
    return e;
}

