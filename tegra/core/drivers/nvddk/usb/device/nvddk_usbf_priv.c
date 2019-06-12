/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK USB F private functions</b>
 *
 * @b Description: Implements USB functuion controller private functions
 *
 */

#include "nvddk_usbf_priv.h"
#include "nvrm_drf.h"
#include "t30/arusb.h"
#include "t30/arapb_misc.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvassert.h"
#include "nvrm_analog.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"


// ---------------------------MACRO DEFINITIONS--------------------------------

/* Defines for USB register read and writes */
#define USB_REG_OFFSET_RD(ofs) \
    NV_READ32(pUsbFuncCtxt->UsbOtgVirAdr + ((ofs)/4))

#define USB_REG_OFFSET_WR(ofs,data) \
    NV_WRITE32(pUsbFuncCtxt->UsbOtgVirAdr +((ofs)/4),(data))

#define USB_REG_OFFSET_SET_DRF_NUM(ofs, reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB2_CONTROLLER_2_USB2D, reg, field, num, USB_REG_OFFSET_RD(ofs))

#define USB_REG_OFFSET_SET_DRF_DEF(ofs, reg, field, def) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, def, USB_REG_OFFSET_RD(ofs))

#define USB_REG_OFFSET_UPDATE_NUM(ofs, reg, field, num) \
    USB_REG_OFFSET_WR(ofs, USB_REG_OFFSET_SET_DRF_NUM(ofs, reg, field, num))

#define USB_REG_OFFSET_UPDATE_DEF(ofs, reg, field, def) \
    USB_REG_OFFSET_WR(ofs, USB_REG_OFFSET_SET_DRF_DEF(ofs, reg, field, def))

#define USB_REG_RD(reg)\
    NV_READ32(pUsbFuncCtxt->UsbOtgVirAdr + ((USB2_CONTROLLER_2_USB2D_##reg##_0)/4))

#define USB_REG_WR(reg, data)\
    NV_WRITE32(pUsbFuncCtxt->UsbOtgVirAdr + ((USB2_CONTROLLER_2_USB2D_##reg##_0)/4), (data))

#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_2_USB2D, reg, field, value)

#define USB_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define)

#define USB_DRF_DEF_VAL(reg, field, define) \
    (USB2_CONTROLLER_2_USB2D##_##reg##_0_##field##_##define)

#define USB_REG_READ_VAL(reg, field) \
    USB_DRF_VAL(reg, field, USB_REG_RD(reg))

#define USB_DEF_RESET_VAL(reg) \
    NV_RESETVAL(USB2_CONTROLLER_2_USB2D, reg)

#define USB_FLD_SET_DRF_DEF(reg, field, define, value) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define, value)

#define USB_REG_SET_DRF_NUM(reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB2_CONTROLLER_2_USB2D, reg, field, num, USB_REG_RD(reg))

#define USB_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define, USB_REG_RD(reg))

#define USB_REG_UPDATE_NUM(reg, field, num) \
    USB_REG_WR(reg, USB_REG_SET_DRF_NUM(reg, field, num))

#define USB_REG_UPDATE_DEF(reg, field, define) \
    USB_REG_WR(reg, USB_REG_SET_DRF_DEF(reg, field, define))


/* Defines for USB Device Queue Head read and writes */
#define USB_DQH_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, define)

#define USB_DQH_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, number)

#define USB_DQH_DRF_VAL(field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, value)

#define USB_DQH_FLD_SHIFT_VAL(field) \
    NV_FIELD_SHIFT(USB2_CONTROLLER_USB2D_DEVICE_QUEUE_HEAD_0_##field##_RANGE)

/* Defines for USB Device Transfer Desctiptor read and writes */
#define USB_DTD_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, DEVICE_TRANSFER_DESCRIPTOR, field, define)

#define USB_DTD_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, DEVICE_TRANSFER_DESCRIPTOR, field, number)

#define USB_DTD_DRF_VAL(field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, DEVICE_TRANSFER_DESCRIPTOR, field, value)



// Define for converting endpoint number to the word mask
#define USB_EP_NUM_TO_WORD_MASK(EndPoint) \
    (((USB_IS_EP_IN(EndPoint)) ? USB_EP_IN_MASK : USB_EP_OUT_MASK) << \
    (EndPoint >> 1))

#define PTR_TO_ADDR(x) ((NvU32)(x))
#define ADDR_TO_PTR(x) ((NvU8*)(x))

/*******************************************************************************
 * Declarations of the USB private functions.that access the controller hardware.
 *******************************************************************************/

/**
 * Initializes the queue heads with the queue head buffer pointer
 */
static void
UsbPrivInitQheads (
    NvDdkUsbFunction *pUsbFuncCtxt);

/**
 * Gets the physical address of the DTD
 * based on the virtual address of the DTD
 */
static NvU32
UsbPrivGetPhyDTD(
    NvDdkUsbFunction *pUsbFuncCtxt,
    UsbDevTransDesc* dtdChainVir);

/**
 * Gets the Eptype from the endpoint structure
 */
static NvU32
UsbPrivGetEpTypeFromEndPoint(
    const NvDdkUsbfEndpoint* EndPoint);

/**
 * Gets the Eptype from the endpoint
 */
static NvU32
UsbPrivGetEpType(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint);

/**
 * Acknowledges and clears interrupt(s) as specified by client.
 */
static void UsbPrivAckInterrupts (NvDdkUsbFunction *pUsbFuncCtxt);

/**
 * Primes the endpoint for the data transfers
 */
static void
UsbPrivPrimeEndpoint(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint);

/**
 * Waits on the synchronus data transfers untill the data transfer or fail
 */
static NvError
UsbPrivWaitForSyncTxferComplete(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 WaitTimeoutMs);


/*****************************************************************************
 ************ Implimentation of the USB private functions ********************
 *****************************************************************************/

NvU8*
UsbPrivGetVirtualBufferPtr(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvRmPhysAddr  phyAddress)
{
    NvU32 offset = 0;

    offset = phyAddress - pUsbFuncCtxt->pUsbXferPhyBuf;

    return (&pUsbFuncCtxt->pUsbXferBuf[offset]);
}

static NvU32
UsbPrivGetEpTypeFromEndPoint (
    const NvDdkUsbfEndpoint* EndPoint)
{
    if (EndPoint->isControl)
    {
        return USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, CTRL);
    }
    else if (EndPoint->isBulk)
    {
        return USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, BULK);
    }
    else if (EndPoint->isInterrupt)
    {
        return USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, INTR);
    }
    else if (EndPoint->isIsoc)
    {
        return USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, ISO);
    }
    else
    {
        return USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, CTRL);
    }
}

NvU32
UsbPrivGetFreeMemoryBlock(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 BytesRequired,
    NvU32 EndPoint)
{
    NvU32 DesiredConsecutiveBlocks = 0;
    NvU32 FreeBlocksStartIndex = USB_MAX_MEM_BUF_COUNT;
    NvU32 ConsecutiveFreeBlocks = 0;
    NvU32 i = 0;

    DesiredConsecutiveBlocks = USB_GET_MAX(1,
                                (BytesRequired + USB_MAX_MEM_BUF_SIZE - 1) /
                                 USB_MAX_MEM_BUF_SIZE);

    for (i = 0; i < USB_MAX_MEM_BUF_COUNT; i++)
    {
        // Is the current block free?
        if (!pUsbFuncCtxt->pUsbXferBufForDtd[i])
        {
            ConsecutiveFreeBlocks++;
            if (ConsecutiveFreeBlocks == DesiredConsecutiveBlocks)
            {
                FreeBlocksStartIndex = (i + 1) - ConsecutiveFreeBlocks;
                break;
            }
        }
        else
        {
            ConsecutiveFreeBlocks = 0;
        }
    }

    if (FreeBlocksStartIndex != USB_MAX_MEM_BUF_COUNT)
    {
        return ((pUsbFuncCtxt->pUsbXferPhyBuf +
                (FreeBlocksStartIndex * USB_MAX_MEM_BUF_SIZE)));
    }

    return 0;
}

void
UsbPrivTxfrLockBuffer(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 PhysBuffer,
    NvU32 SizeBytes)
{
    NvU32 NumberOfBlocksAllocated = 0;
    NvU32 FreeBlocksStartIndex = USB_MAX_MEM_BUF_COUNT;
    NvU32 i;

    NumberOfBlocksAllocated = USB_GET_MAX(1,
                                (SizeBytes + USB_MAX_MEM_BUF_SIZE - 1) /
                                 USB_MAX_MEM_BUF_SIZE);

    // Search for the Address to find the index
    for (i = 0; i < USB_MAX_MEM_BUF_COUNT; i++)
    {
        if ((pUsbFuncCtxt->pUsbXferPhyBuf +
            (i * USB_MAX_MEM_BUF_SIZE)) ==
            (PhysBuffer))
        {
            FreeBlocksStartIndex = i;
            break;
        }
    }

    if (FreeBlocksStartIndex != USB_MAX_MEM_BUF_COUNT)
    {
        for (i = FreeBlocksStartIndex;
             i < (FreeBlocksStartIndex +
             NumberOfBlocksAllocated);
             i++)
        {
            // We got the buffer index so mark them as allocated
            pUsbFuncCtxt->pUsbXferBufForDtd[i] =
                &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
                [(EndPoint*USB_MAX_DTDS_PER_EP)];
        }
    }
}

void UsbPrivTxfrUnLockBuffer(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvU32 DtdIndex = 0;

    // Unlock the allocated Memory buffer for the Endpoint
    // by deleting the DTD so that this memory can be used
    // for next transactions.
    for (DtdIndex = 0; DtdIndex < USB_MAX_MEM_BUF_COUNT; DtdIndex++)
    {
        // Is the current block locked for this endoint?
        if (pUsbFuncCtxt->pUsbXferBufForDtd[DtdIndex] ==
            &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
            [(EndPoint*USB_MAX_DTDS_PER_EP)])
        {
            // Mark the buffer as Unused
            pUsbFuncCtxt->pUsbXferBufForDtd[DtdIndex] = 0;
        }
    }
}


static NvU32
UsbPrivGetPhyDTD (
    NvDdkUsbFunction *pUsbFuncCtxt,
    UsbDevTransDesc* dtdChainVir)
{
    NvU32 i;
    NvU32 DtdPhysAddr = 0;

    for (i = 0; i < USB_MAX_DTDS(pUsbFuncCtxt); i++)
    {
        if (dtdChainVir == &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc[i])
        {
            DtdPhysAddr = pUsbFuncCtxt->UsbTxferDescriptorPhyBufAddr +
                          (i * sizeof(UsbDevTransDesc));
            return DtdPhysAddr;
        }
    }

    return DtdPhysAddr;
}


static void
UsbPrivInitQheads(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvU32 EndPoint;

    // Clear data structures
    NvOsMemset((void*) pUsbFuncCtxt->UsbDescriptorBuf.pQHeads, 0,
        sizeof(UsbDevQueueHead) * pUsbFuncCtxt->UsbfEndpointCount);
    NvOsMemset((void*) pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc, 0,
        sizeof(UsbDevTransDesc) * USB_MAX_DTDS(pUsbFuncCtxt));
    NvOsMemset((void*) pUsbFuncCtxt->pUsbXferBuf, 0, USB_MAX_TXFR_SIZE_BYTES);
    NvOsMemset((void*) pUsbFuncCtxt->pUsbXferBufForDtd, 0,
        sizeof(UsbDevTransDesc*) * USB_MAX_MEM_BUF_COUNT);
    NvOsFlushWriteCombineBuffer();

    for (EndPoint = 0; EndPoint < pUsbFuncCtxt->UsbfEndpointCount; EndPoint++)
    {
        // Indicate the endpoint is cleared.
        pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] = NV_FALSE;
        // Clear the ep transfer error info.
        pUsbFuncCtxt->EpTxfrError[EndPoint] = NV_FALSE;

        // Clear the endpoint bytes requested
        pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint] = 0;

        pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;
    }

    // Initialize the EndPoint Base Addr with the Q Head base address
    USB_REG_WR(ASYNCLISTADDR, pUsbFuncCtxt->QHeadsPhyAddr);
}


static NvU32
UsbPrivGetEpType(
    NvDdkUsbFunction * pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvU32 EpOffs;

    EpOffs = (USB2_CONTROLLER_2_USB2D_ENDPTCTRL1_0 -
              USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0);
    EpOffs *= (EndPoint>>1);
    EpOffs += USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0;
    EpOffs >>= 2;

    if (USB_IS_EP_IN(EndPoint))
    {
        return NV_DRF_VAL(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, TXT,
                NV_READ32(pUsbFuncCtxt->UsbOtgVirAdr + EpOffs));
    }
    else
    {
        return NV_DRF_VAL(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, RXT,
                NV_READ32(pUsbFuncCtxt->UsbOtgVirAdr + EpOffs));
    }
}


NvU32
UsbPrivGetPacketSize(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvU32 packetSize = USB_FULL_SPEED_PKT_SIZE_BYTES;
    NvBool hs = NV_FALSE;
    NvU32 eptype = UsbPrivGetEpType(pUsbFuncCtxt, EndPoint);

    // Return packet size based on speed and endpoint type.
    switch (eptype)
    {
        case USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, CTRL):
            packetSize = USB_FULL_SPEED_PKT_SIZE_BYTES;
            break;
        case USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, ISO):
            packetSize = USB_HIGH_SPEED_PKT_SIZE_BYTES;
            break;
        case USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, BULK):
        case USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, INTR):
        default:
            hs = (pUsbFuncCtxt->UsbPortSpeed == UsbPortSpeedType_High);
            packetSize = hs ?
                         USB_HIGH_SPEED_PKT_SIZE_BYTES :
                         USB_FULL_SPEED_PKT_SIZE_BYTES;
            break;
    }

    return packetSize;
}

NvError
UsbPrivAllocateBuffers(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvRmPhysAddr OtgPhysAddr)
{
    NvError err = NvSuccess;
    NvU32 align = 0;
    NvU32 offset = 0;

    // one DTD per buffer
    pUsbFuncCtxt->VirtualBufferSize = sizeof(UsbDevTransDesc) *
                                      USB_MAX_DTDS(pUsbFuncCtxt);
    // transfer buffer size for IN and Out transactions
    pUsbFuncCtxt->VirtualBufferSize += USB_MAX_USB_BUF_SIZE;

    // alignment must be 4K for USB buffers.
    align = USB_HW_BUFFER_ALIGNMENT;

    pUsbFuncCtxt->hUsbBuffMem = NULL;

    err = NvRmMemHandleAlloc(pUsbFuncCtxt->hRmDevice, NULL, 0, align,
            NvOsMemAttribute_WriteCombined, pUsbFuncCtxt->VirtualBufferSize,
            0, 1, &pUsbFuncCtxt->hUsbBuffMem);

    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("UsbfError>AllocateBuffers::Create memory handle Fail!\n"));
        goto fail;
    }

    pUsbFuncCtxt->pUsbXferPhyBuf = NvRmMemPin(pUsbFuncCtxt->hUsbBuffMem);

    // Mapping one time for the entire size;
    err = NvRmMemMap(pUsbFuncCtxt->hUsbBuffMem, offset,
            pUsbFuncCtxt->VirtualBufferSize, NVOS_MEM_READ_WRITE,
            (void **)&(pUsbFuncCtxt->UsbVirtualBuffer));

    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("UsbfError>AllocateBuffers::RmMemMap 1 Fail!\n"));
        goto fail;
    }

    offset = 0;
    pUsbFuncCtxt->pUsbXferBuf = (pUsbFuncCtxt->UsbVirtualBuffer + offset);
    offset += USB_MAX_USB_BUF_SIZE;

    pUsbFuncCtxt->UsbTxferDescriptorPhyBufAddr =
        pUsbFuncCtxt->pUsbXferPhyBuf + offset;
    pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc =
        (UsbDevTransDesc *) (pUsbFuncCtxt->UsbVirtualBuffer + offset);

    // Using the Internal Queue heads of the USB controller
    pUsbFuncCtxt->QHeadsPhyAddr = OtgPhysAddr + USB2_QH_USB2D_QH_EP_0_OUT_0;
    pUsbFuncCtxt->UsbDescriptorBuf.pQHeads = (volatile UsbDevQueueHead *)
        (pUsbFuncCtxt->UsbOtgVirAdr +(USB2_QH_USB2D_QH_EP_0_OUT_0/4));

    pUsbFuncCtxt->UsbMaxTransferSize = USB_MAX_TXFR_SIZE_BYTES;

    return err;

fail:
    if (pUsbFuncCtxt->hUsbBuffMem)
    {
        // Unmapping the Virtual Addresses
        NvRmMemUnmap(pUsbFuncCtxt->hUsbBuffMem,
                    pUsbFuncCtxt->UsbVirtualBuffer,
                    pUsbFuncCtxt->VirtualBufferSize);

        // Unpinning the Memory
        NvRmMemUnpin(pUsbFuncCtxt->hUsbBuffMem);

        NvRmMemHandleFree(pUsbFuncCtxt->hUsbBuffMem);
        // Set to NULL so that RM calls will not be called again
        pUsbFuncCtxt->hUsbBuffMem = NULL;
    }

    return err;
}

void
UsbPrivDeAllocateBuffers(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    if (pUsbFuncCtxt->hUsbBuffMem)
    {
        // Unmapping the Virtual Addresses
        NvRmMemUnmap(pUsbFuncCtxt->hUsbBuffMem,
                    pUsbFuncCtxt->UsbVirtualBuffer,
                    pUsbFuncCtxt->VirtualBufferSize);

        // Unpinning the Memory
        NvRmMemUnpin(pUsbFuncCtxt->hUsbBuffMem);
        NvRmMemHandleFree(pUsbFuncCtxt->hUsbBuffMem);
        // Set to NULL so that RM calls will not be called again
        pUsbFuncCtxt->hUsbBuffMem = NULL;
    }
}

void
UsbPrivConfigureEndpoints(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvDdkUsbfEndpoint *pEp = &pUsbFuncCtxt->UsbfEndpoint[0];
    NvU32 EndPoint;
    for (EndPoint = 0;
         EndPoint<pUsbFuncCtxt->UsbfEndpointCount;
         EndPoint++, pEp++)
    {
        pEp->handle = EndPoint;
        pEp->isIn = (USB_IS_EP_IN(EndPoint)) ? NV_TRUE : NV_FALSE;
        if (EndPoint<USB_EP_MISC)
        {
            pEp->isControl = NV_TRUE;
            pEp->isBulk = pEp->isInterrupt = pEp->isIsoc = NV_FALSE;
        }
        else
        {
            pEp->isControl = NV_FALSE;
            pEp->isBulk = pEp->isInterrupt = NV_TRUE;
            if (EndPoint==3)
                pEp->isIsoc = NV_FALSE;
            else
                pEp->isIsoc = NV_TRUE;
        }
    }
}

void
UsbPrivInitalizeEndpoints(
    NvDdkUsbFunction *pUsbFuncCtxt,
    const NvDdkUsbfEndpoint *EndPointInfo,
    NvBool config)
{
    volatile UsbDevQueueHead *pQueueHead;
    NvU32 epType = 0;
    NvU32 EndPoint = EndPointInfo->handle;
    NvU32 EpOffs;

    if (EndPoint >= pUsbFuncCtxt->UsbfEndpointCount)
        return;

    EpOffs = (USB2_CONTROLLER_2_USB2D_ENDPTCTRL1_0 -
              USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0);
    EpOffs *= (EndPoint>>1);
    EpOffs += USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0;

    /*
     * If non-null config pointer, set up endpoints for indicated configuration,
     * otherwise clear and disable non-control endpoints.
     */
    if (config)
    {
        // Set up Queue Head for endpoint.
        pQueueHead = &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[EndPoint];
        NvOsMemset((void*) pQueueHead, 0, USB_HW_QUEUE_HEAD_SIZE_BYTES);

        // Interrupt on setup if it is endpoint 0_OUT.
        if (EndPoint == USB_EP_CTRL_OUT)
        {
            pQueueHead->EpCapabilities = USB_DQH_DRF_DEF(IOC, ENABLE);
        }

        // terminate the next DTD pointer by writing 1 = SET
        pQueueHead->NextDTDPtr = USB_DQH_DRF_DEF(TERMINATE, SET);

        pQueueHead->EpCapabilities |= USB_DQH_DRF_NUM(MAX_PACKET_LENGTH,
                                      UsbPrivGetPacketSize(pUsbFuncCtxt,
                                      EndPoint));

        epType = UsbPrivGetEpTypeFromEndPoint(EndPointInfo);

        if (USB_IS_EP_IN(EndPoint))
        {
            USB_REG_OFFSET_UPDATE_NUM(EpOffs, ENDPTCTRL0, TXT, epType);
            USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, TXS, EP_OK);
            if (EndPoint>=USB_EP_MISC)
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL1, TXR, RESET_PID_SEQ);
            USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, TXE, ENABLE);
        }
        else
        {
            USB_REG_OFFSET_UPDATE_NUM(EpOffs, ENDPTCTRL0, RXT, epType);
            USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, RXS, EP_OK);
            if (EndPoint>=USB_EP_MISC)
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL1, RXR, RESET_PID_SEQ);
            USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, RXE, ENABLE);
        }
    }
    else
    {
        if (EndPoint>=USB_EP_MISC)
        {
            if (USB_IS_EP_IN(EndPoint))
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, TXE, DISABLE);
            else
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL0, RXE, DISABLE);
        }
    }
}


NvError
UsbPrivIntializeController(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvError ErrValue = NvSuccess;
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 regValue;

    pUsbFuncCtxt->UsbfCableConnected = UsbfPrivIsCableConnected(pUsbFuncCtxt);

    /// If specific charger need to be detected
    if ((!pUsbFuncCtxt->DummyChargerSupported) && pUsbFuncCtxt->UsbfCableConnected)
    {
        pUsbFuncCtxt->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        pUsbFuncCtxt->ChargerDetected = NV_FALSE;
        pUsbFuncCtxt->CheckForCharger = NV_TRUE;
        UsbfPrivConfigureChargerDetection(pUsbFuncCtxt, NV_TRUE, NV_TRUE);
    }

    if(pUsbFuncCtxt->UsbInterfaceType != NvOdmUsbInterfaceType_UlpiNullPhy)
    {
        UsbPrivStatusClearOnReset(pUsbFuncCtxt);

        // STOP the USB controller
        USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
        // Set the USB mode to the IDLE before reset
        USB_REG_UPDATE_DEF(USBMODE, CM, IDLE);
        // Reset the controller
        USB_REG_UPDATE_DEF(USBCMD, RST, SET);

        TimeOut = CONTROLLER_HW_TIMEOUT_US;
        do {
            // Wait till bus reset clears.
            regValue = USB_REG_READ_VAL(USBCMD, RST);
            if (!TimeOut)
            {
                return NvError_Timeout;
            }
            NvOsWaitUS(1);
            TimeOut--;
        } while (regValue);

        // Wait for  phy clock to become valid
        ErrValue = NvDdkUsbPhyWaitForStableClock(pUsbFuncCtxt->hUsbPhy);
        if (ErrValue != NvSuccess)
        {
            return ErrValue;
        }
    }

    // set the controller to device controller mode
    USB_REG_UPDATE_DEF(USBMODE, CM, DEVICE_MODE);

    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        // wait till device mode change is finished.
        regValue = USB_REG_READ_VAL(USBMODE, CM);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue != USB_DRF_DEF_VAL(USBMODE, CM, DEVICE_MODE));

    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));

    // clear all pending interrupts, if any
    regValue = USB_DRF_DEF(USBSTS, SLI, SUSPENDED) |
               USB_DRF_DEF(USBSTS, SRI, SOF_RCVD) |
               USB_DRF_DEF(USBSTS, URI, USB_RESET) |
               USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
               USB_DRF_DEF(USBSTS, SEI, ERROR) |
               USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
               USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
               USB_DRF_DEF(USBSTS, UEI, ERROR) |
               USB_DRF_DEF(USBSTS, UI, INT);
    USB_REG_WR(USBSTS, regValue);

    // clear OTGSC interupts
    regValue = USB_DRF_DEF(OTGSC, DPIS, INT_SET) |
               USB_DRF_DEF(OTGSC, ONEMSS, INT_SET) |
               USB_DRF_DEF(OTGSC, BSEIS, INT_SET) |
               USB_DRF_DEF(OTGSC, BSVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, ASVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, AVVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, IDIS, INT_SET);
    USB_REG_WR(OTGSC, regValue);

    // Clear setup packet status
    regValue = USB_REG_RD(ENDPTSETUPSTAT);
    USB_REG_WR(ENDPTSETUPSTAT, regValue);

    // Set interrupts to occur immediately
    USB_REG_UPDATE_DEF(USBCMD, ITC, IMMEDIATE);

    // Enable USB interrupts
    regValue = USB_DRF_DEF(USBINTR, SLE, ENABLE) |
               USB_DRF_DEF(USBINTR, URE, ENABLE) |
               USB_DRF_DEF(USBINTR, SEE, ENABLE) |
               USB_DRF_DEF(USBINTR, PCE, ENABLE) |
               USB_DRF_DEF(USBINTR, UEE, ENABLE) |
               USB_DRF_DEF(USBINTR, UE, ENABLE);
    USB_REG_WR(USBINTR, regValue);

    // Enable OTGSC Bsession valid Interrupt
    regValue = USB_REG_SET_DRF_DEF(OTGSC, BSVIE, ENABLE);
    USB_REG_WR(OTGSC, regValue);

    pUsbFuncCtxt->PortChangeDetected = NV_FALSE;
    pUsbFuncCtxt->UsbResetDetected = NV_FALSE;
    pUsbFuncCtxt->SetupPacketDetected = NV_FALSE;
    pUsbFuncCtxt->PortSuspend = NV_FALSE;

    // Initialize the Queue heads
    UsbPrivInitQheads(pUsbFuncCtxt);

    // Initialie the control endpoints
    UsbPrivInitalizeEndpoints(pUsbFuncCtxt,
                              &pUsbFuncCtxt->UsbfEndpoint[USB_EP_CTRL_OUT],
                              NV_TRUE);
    UsbPrivInitalizeEndpoints(pUsbFuncCtxt,
                              &pUsbFuncCtxt->UsbfEndpoint[USB_EP_CTRL_IN],
                              NV_TRUE);

    // Disable the Charger detection logic before we start the controller
    UsbfPrivConfigureChargerDetection(pUsbFuncCtxt, NV_FALSE, NV_FALSE);
    pUsbFuncCtxt->LineState = USB_REG_READ_VAL(PORTSC1, LS);

    // Start up controller.
    USB_REG_UPDATE_DEF(USBCMD, RS, RUN);
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        // wait till device starts running.
        regValue = USB_REG_READ_VAL(USBCMD, RS);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue != USB_DRF_DEF_VAL(USBCMD, RS, RUN));

    return ErrValue;
}

void
UsbfPrivDisableControllerInterrupts(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));
}


void
UsbPrivStatusClearOnReset(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    // Clear the HW status registers on reset
    // Clear device address by writing the reset value
    USB_REG_WR(PERIODICLISTBASE, USB_DEF_RESET_VAL(PERIODICLISTBASE));
    // Clear setup token, by reading and wrting back the same value
    USB_REG_WR(ENDPTSETUPSTAT, USB_REG_RD(ENDPTSETUPSTAT));
    // Clear endpoint complete status bits.by reading and writing back
    USB_REG_WR(ENDPTCOMPLETE, USB_REG_RD(ENDPTCOMPLETE));
}

NvError
UsbPrivEndPointFlush(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 EndpointMask;
    NvU32 RegValue;

    // Get the Endpoint mask depending on the endpoin number
    EndpointMask = (EndPoint == s_USB_FLUSH_ALL_EPS) ?
                   (EndPoint) : (NvU32)(USB_EP_NUM_TO_WORD_MASK(EndPoint));

    // Flush endpoints
    USB_REG_WR(ENDPTFLUSH, EndpointMask);
    do {
        RegValue = USB_REG_RD(ENDPTFLUSH);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    // Wait for status bits to clear as well.
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        RegValue = USB_REG_RD(ENDPTSTATUS);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    // Wait till all primed endpoints clear.
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        RegValue = USB_REG_RD(ENDPTPRIME);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    return NvSuccess;
}

NvBool
UsbPrivBSessionValid(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    if (USB_REG_READ_VAL(OTGSC, BSV))
    {
        // cable is present
        pUsbFuncCtxt->UsbfCableConnected = NV_TRUE;
        return NV_TRUE;
    }
    else
    {
        pUsbFuncCtxt->UsbfCableConnected = NV_FALSE;
        return NV_FALSE;
    }
}

NvError
UsbPrivTxfrClearAll(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvError ErrorStatus = NvSuccess;
    NvU32 regVal = 0;

    // Flush all endpoints
    UsbPrivEndPointFlush(pUsbFuncCtxt, s_USB_FLUSH_ALL_EPS);

    for (regVal = 0; regVal<pUsbFuncCtxt->UsbfEndpointCount; regVal++)
    {
        pUsbFuncCtxt->EpTxferCompleted[regVal] = NV_FALSE;
        pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[regVal] = NV_FALSE;
        pUsbFuncCtxt->EpTxfrError[regVal] = NV_FALSE;
        pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[regVal] = 0;
    }

    // Clear endpoint complete status bits.
    regVal = USB_REG_RD(ENDPTCOMPLETE);
    USB_REG_WR(ENDPTCOMPLETE, regVal);

    return ErrorStatus;
}

NvError
UsbPrivTxfrClear(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvError ErrorStatus = NvSuccess;
    NvU32 regVal = 0;
    NvU32 DtdIndex = 0;

    // Flush the endpoint before clearing it.
    ErrorStatus = UsbPrivEndPointFlush(pUsbFuncCtxt, EndPoint);
    if (ErrorStatus != NvSuccess)
    {
        return ErrorStatus;
    }

    // Clear the Queue Head before configuration
    NvOsMemset((void*) &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[EndPoint],
               0,
               USB_HW_QUEUE_HEAD_SIZE_BYTES);

    // Each endpoint has Four DTDS clear all of them
    for (DtdIndex = 0; DtdIndex < USB_MAX_DTDS_PER_EP; DtdIndex++)
    {
        // Clear the DTDs of this endpoint
        NvOsMemset((void*) &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
                   [(EndPoint*USB_MAX_DTDS_PER_EP) + DtdIndex],
                   0,
                   sizeof(UsbDevTransDesc));
    }

    // Clear the allocated Memory buffer for the Endpoint by deleting the DTD
    UsbPrivTxfrUnLockBuffer(pUsbFuncCtxt, EndPoint);

    // Clear the EP Transfer complete status
    pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_FALSE;

    // Indicate the endpoint is cleared.
    pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] = NV_FALSE;

    // Clear the ep transfer error info.
    pUsbFuncCtxt->EpTxfrError[EndPoint] = NV_FALSE;

    // Clear the endpoint bytes requested
    pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint] = 0;

    // Clear the Signal indication status
    pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;

    // Clear endpoint complete status bits.
    //regVal = USB_REG_RD(ENDPTCOMPLETE);
    regVal = USB_EP_NUM_TO_WORD_MASK(EndPoint);
    USB_REG_WR(ENDPTCOMPLETE, regVal);

    return ErrorStatus;
}

NvError
UsbPrivReceive(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU8 *pClientDataBuf,
    NvU32 *pRcvdBytes,
    NvBool EnableZlt,
    NvU32 WiatTimeoutMs)
{
    NvError retStatus = NvSuccess;
    volatile UsbDevQueueHead *pQueueHead = NULL;
    UsbDevTransDesc *pUsbDevTxfrDesc = NULL;
    NvU8* pVirtualBuffer = NULL;
    NvU32 TotalBytesReceived = 0;
    NvU32 BytesRequested = *pRcvdBytes;
    NvU32 PhysBufferAddr;
    // Physical address for the DTD
    NvU32 pPhyDTD;
    NvU32 BytesRecievedInDTd = 0;
    NvBool Done = NV_FALSE;
    NvU32 NumOfDtdsCompleted = 0;

    // Make sure this is an OUT endpoint.
    if (USB_IS_EP_IN(EndPoint))
    {
        return NvError_UsbInvalidEndpoint;
    }

    // Check if endpoint is already configured
    if (pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint])
    {
        // Wait for transfer to finish or error.
        retStatus = UsbPrivTxfrWait(pUsbFuncCtxt,
                                    EndPoint,
                                    USB_MAX_TXFR_WAIT_TIME_MS);
        if (retStatus == NvSuccess)
        {
            // Get atleaset once the EP status
            retStatus = UsbPrivEpGetStatus(pUsbFuncCtxt, EndPoint);
        }

        if (retStatus == NvError_UsbfTxfrComplete)
        {
            // Clear the endpoint config flag
            pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] = NV_FALSE;
            // clear the ep transfer complete
            pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_FALSE;
            // Set return Status to Success so that data
            // will be copied to the client buffer
            retStatus = NvSuccess;
        }
    }
    else
    {
        retStatus = UsbPrivTxfrClear(pUsbFuncCtxt, EndPoint);
        if (retStatus != NvSuccess)
        {
            return retStatus;
        }
        PhysBufferAddr = UsbPrivGetFreeMemoryBlock(
                            pUsbFuncCtxt, BytesRequested, EndPoint);
        if (PhysBufferAddr)
        {
            // Mark the buffer as used before starting the transaction
            UsbPrivTxfrLockBuffer(
                pUsbFuncCtxt, EndPoint, PhysBufferAddr, BytesRequested);
            // Start up transaction.
            UsbPrivTxfrStart(pUsbFuncCtxt,
                EndPoint, BytesRequested, PhysBufferAddr, NV_TRUE, EnableZlt, NV_TRUE);
            // Wait for transfer to finish or error.
            retStatus = UsbPrivWaitForSyncTxferComplete(
                            pUsbFuncCtxt, EndPoint, WiatTimeoutMs);
        }
        else
        {
            retStatus = NvError_InsufficientMemory;
        }
    }

    if (retStatus == NvSuccess)
    {
        //  We are here means the transfer is complete without errors.
        // Temporary pointer to queue head.
        pQueueHead = &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[EndPoint];
        // get the first DTD for this endpoint
        pUsbDevTxfrDesc = &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
                        [(EndPoint*USB_MAX_DTDS_PER_EP)];
        while (!Done)
        {
            // Get the physical address of the DTD
            pPhyDTD=UsbPrivGetPhyDTD(pUsbFuncCtxt, pUsbDevTxfrDesc);
            // Is this DTD same as in Q head
            if (pPhyDTD == pQueueHead->CurrentDTDPtr)
            {
                // this is the last DTD for this ep
                // calaculate number of bytes programmed in this DTD
                BytesRecievedInDTd =
                    (pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint] -
                    (USB_MAX_DTD_BUF_SIZE * NumOfDtdsCompleted));
                // Find the actual number of bytes received on this DTD
                BytesRecievedInDTd = (BytesRecievedInDTd -
                    USB_DTD_DRF_VAL(TOTAL_BYTES, pUsbDevTxfrDesc->DtdToken));
                // We are done with this loop set to TRUE for quitting the loop
                Done = NV_TRUE;
            }
            else
            {
                // we are here means more than one DTD is programmed
                // find number of bytes received in this DTD
                BytesRecievedInDTd = (USB_MAX_DTD_BUF_SIZE -
                                      USB_DTD_DRF_VAL(TOTAL_BYTES,
                                      pUsbDevTxfrDesc->DtdToken));
            }
            if (BytesRecievedInDTd)
            {
                // get the virtual buffer pointer for the first buffer in this DTD
                pVirtualBuffer = UsbPrivGetVirtualBufferPtr(pUsbFuncCtxt,
                                    pUsbDevTxfrDesc->BufPtrs[0] & 0xFFFFF000);
                // copy the bytes received in this DTD to the client
                NvOsMemcpy(pClientDataBuf, pVirtualBuffer, BytesRecievedInDTd);
                // incriment the client buffer pointer with the number of bytes copied
                pClientDataBuf += BytesRecievedInDTd;
                // track the total number of bytes received on this EP
                TotalBytesReceived += BytesRecievedInDTd;
            }
            // get the next virtual DTD pointer
            pUsbDevTxfrDesc++;
            // Advance the number of TDs completed
            NumOfDtdsCompleted++;
        } ;
        // Set the total number of bytes actually recieved
        *pRcvdBytes = TotalBytesReceived;
        // we are done with the transfer clear the buffers
        retStatus = UsbPrivTxfrClear(pUsbFuncCtxt, EndPoint);
        // If this is the control endpoint then setup the setup packet
        if (EndPoint == USB_EP_CTRL_OUT)
        {
            // Configure for next setup packet,
            // this will make sure device is ready for any setup
            // packet from host Initialie the control endpoints
            UsbPrivInitalizeEndpoints(pUsbFuncCtxt,
                &pUsbFuncCtxt->UsbfEndpoint[USB_EP_CTRL_OUT], NV_TRUE);
        }
    }

    return retStatus;
}

NvError
UsbPrivEpGetStatus(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvU32 EpCtrlRegValue = 0;
    NvError retStatus = NvSuccess;
    volatile UsbDevQueueHead *pQueueHead;
    NvU32 EpOffs;
    UsbDevTransDesc *pUsbDevTxfrDesc = NULL;
    NvU32 pPhyDTD;

    EpOffs = (USB2_CONTROLLER_2_USB2D_ENDPTCTRL1_0 -
              USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0);
    EpOffs *= (EndPoint>>1);
    EpOffs += USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0;
    EpOffs >>= 2;

    if (EndPoint<pUsbFuncCtxt->UsbfEndpointCount)
        EpCtrlRegValue = NV_READ32(pUsbFuncCtxt->UsbOtgVirAdr + EpOffs);

    // If endpoint is stalled, report that
    // TXS, TXE, RXS and RXE are at the same location in ENDPTCTRL0
    // and ENDPTCTRL1 so reading from the same DRF register is OK
    if ((USB_IS_EP_IN(EndPoint)) ?
         USB_DRF_VAL(ENDPTCTRL0, TXS, EpCtrlRegValue) :
         USB_DRF_VAL(ENDPTCTRL0, RXS, EpCtrlRegValue))
    {
        retStatus = NvError_UsbfEpStalled;
        goto Exit;
    }

        // If port is not enabled, return end point is not configured.
    if ((USB_IS_EP_IN(EndPoint)) ?
         USB_DRF_VAL(ENDPTCTRL0, TXE, EpCtrlRegValue) :
         USB_DRF_VAL(ENDPTCTRL0, RXE, EpCtrlRegValue))
    {
        // Check If there is any error in the transaction.
        if (pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] &&
            pUsbFuncCtxt->EpTxfrError[EndPoint])
        {
            retStatus = NvError_UsbfTxfrFail;
            goto Exit;
        }
        // Temporary pointer to queue head.
        pQueueHead = &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[EndPoint];
        // Look for an error by inspecting the DTD fields stored in the DQH.
        if (pQueueHead->DtdToken & (USB_DQH_DRF_DEF(HALTED, SET)|
                              USB_DQH_DRF_DEF(DATA_BUFFER_ERROR, SET)|
                              USB_DQH_DRF_DEF(TRANSACTION_ERROR, SET)))
        {
            retStatus = NvError_UsbfTxfrFail;
            goto Exit;
        }
        // If endpoint active, check to see if it has completed an operation.
        if ((USB_REG_RD(ENDPTPRIME) & USB_EP_NUM_TO_WORD_MASK(EndPoint)) ||
            (USB_REG_RD(ENDPTSTATUS) & USB_EP_NUM_TO_WORD_MASK(EndPoint)) ||
            (pQueueHead->DtdToken & USB_DQH_DRF_DEF(ACTIVE, SET)))
        {
            // Set return status to the Active
            retStatus = NvError_UsbfTxfrActive;
            // Transfer is active in the Queue head, check all DTDs status
            // get the first DTD for this endpoint
            pUsbDevTxfrDesc = &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
                            [(EndPoint*USB_MAX_DTDS_PER_EP)];
            // Get the physical address of the DTD
            pPhyDTD=UsbPrivGetPhyDTD(pUsbFuncCtxt, pUsbDevTxfrDesc);
            // Check DTD is same as in the Q head
            while (pPhyDTD != pQueueHead->CurrentDTDPtr)
            {
                // We are here means Q head has more than One DTD.
                // check current DTD has any erros in the transaction.
                if (pUsbDevTxfrDesc->DtdToken & (USB_DTD_DRF_DEF(HALTED, SET)|
                                      USB_DTD_DRF_DEF(DATA_BUFFER_ERROR, SET)|
                                      USB_DTD_DRF_DEF(TRANSACTION_ERROR, SET)))
                {
                    // this transaction encountered error
                    retStatus = NvError_UsbfTxfrFail;
                    goto Exit;
                }
                else if (pQueueHead->DtdToken & USB_DQH_DRF_DEF(ACTIVE, SET))
                {
                    // DTD is still active, H/W hasn't completed for this DTD
                    retStatus = NvError_UsbfTxfrActive;
                    break;
                }
                else
                {
                    // We are here means there is no error in the DTD
                    // Check EP is configured and we recived the complete interrupt
                    if ((pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint]) &&
                        (pUsbFuncCtxt->EpTxferCompleted[EndPoint]))
                    {
                        // set status as Transfer is complete
                        retStatus = NvError_UsbfTxfrComplete;
                    }
                }
                // get the next virtual DTD pointer
                pUsbDevTxfrDesc++;
                // Get the physical address of the DTD
                pPhyDTD=UsbPrivGetPhyDTD(pUsbFuncCtxt, pUsbDevTxfrDesc);
            }
        }
        else
        {
            // Check for a complete transaction.
            if (pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint])
            {
                retStatus = NvError_UsbfTxfrComplete;
            }
        }
    }
    else
    {
        // Return as endpoint is not configured
        // TODO: define another error enum
        retStatus = NvSuccess;
    }

Exit:
    // override error status if cable is disconnected
    if (!UsbPrivBSessionValid(pUsbFuncCtxt))
    {
        retStatus = NvError_UsbfCableDisConnected;
    }

    return retStatus;
}

NvError
UsbPrivEpTransmit(
    NvDdkUsbFunction * pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU8* pClientBufferPtr,
    NvU32 *pBytesToTransfer,
    NvBool EndTransfer,
    NvU32 WiatTimeoutMs)
{
    NvError retStatus = NvSuccess;
    NvU32 TotalBytes = *pBytesToTransfer;
    NvU8* pVirtualBuffer;
    NvU32 PhysBufferAddr;
    NvBool EnableZlt = NV_FALSE;

    // Make sure this is an IN endpoint.
    if (!USB_IS_EP_IN(EndPoint))
    {
        return NvError_UsbInvalidEndpoint;
    }

    retStatus = UsbPrivTxfrClear(pUsbFuncCtxt, EndPoint);
    if (retStatus != NvSuccess)
    {
        return retStatus;
    }

    // If this is the end oftransfer for this transaction,
    // check for Zero packet enabling, if total bytes is multiple of
    // MaxPacket Size then enable zero packet at the end of transfer
    if (EndTransfer && !(TotalBytes %
        UsbPrivGetPacketSize(pUsbFuncCtxt, EndPoint)))
    {
        EnableZlt = NV_TRUE;
    }

    // Get Free physical buffer for transmit
    PhysBufferAddr = UsbPrivGetFreeMemoryBlock(
                        pUsbFuncCtxt, TotalBytes, EndPoint);

    if (PhysBufferAddr)
    {
        // Mark the buffer as used before starting the transaction
        UsbPrivTxfrLockBuffer(pUsbFuncCtxt,
            EndPoint, PhysBufferAddr, TotalBytes);
        pVirtualBuffer = UsbPrivGetVirtualBufferPtr(
                            pUsbFuncCtxt, PhysBufferAddr);
        NvOsMemcpy(pVirtualBuffer, pClientBufferPtr, TotalBytes);
        NvOsFlushWriteCombineBuffer();
        // Start up transaction.
        UsbPrivTxfrStart(pUsbFuncCtxt,
            EndPoint, TotalBytes, PhysBufferAddr, NV_TRUE, EnableZlt, NV_TRUE);
        // Wait for transfer to finish or error.
        retStatus = UsbPrivWaitForSyncTxferComplete(
                        pUsbFuncCtxt, EndPoint, WiatTimeoutMs);
        if (retStatus == NvSuccess)
        {
            retStatus = UsbPrivTxfrClear(pUsbFuncCtxt, EndPoint);
        }
    }
    else
    {
        retStatus = NvError_InsufficientMemory;
    }

    return retStatus;
}

NvError
UsbPrivTxfrWait(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 timeoutMs)
{
    NvError retStatus = NvError_UsbfTxfrFail;

    // Get atleaset once the EP status
    retStatus = UsbPrivEpGetStatus(pUsbFuncCtxt, EndPoint);

    // wait for transfer complete if transfer is active
    if (retStatus == NvError_UsbfTxfrActive)
    {
        NvU32 t0 = NvOsGetTimeMS();
        NvU32 d = 0;

        // either success or error break here and return the status
        // or continue till time out or transfer is active
        while ((retStatus == NvError_UsbfTxfrActive) && (d < timeoutMs))
        {
            // Wait time reduce to meet Set Address Req requirement
            NvOsWaitUS(100);
            retStatus = UsbPrivEpGetStatus(pUsbFuncCtxt, EndPoint);
            d = NvOsGetTimeMS() - t0;
        }
    }

    // Completed is the same thing as OK.
    if (retStatus == NvError_UsbfTxfrComplete)
    {
        retStatus = NvSuccess;
    }

    return retStatus;
}

void
UsbPrivEpSetStalledState(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvBool StallState)
{
    NvU32 EpOffs;

    EpOffs = (USB2_CONTROLLER_2_USB2D_ENDPTCTRL1_0 -
              USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0);
    EpOffs *= (EndPoint>>1);
    EpOffs += USB2_CONTROLLER_2_USB2D_ENDPTCTRL0_0;

    //  Control endpoints can only be stalled as a pair.  The unstall automatically
    //  when another setup packet arrives.
    if ((EndPoint==USB_EP_CTRL_IN) || (EndPoint==USB_EP_CTRL_OUT))
    {
        if (StallState)
        {
            NvU32 RegVal =
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, TXT, CTRL) |
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, RXT, CTRL) |
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, TXE, ENABLE) |
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, RXE, ENABLE) |
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, TXS, EP_STALL) |
                NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, ENDPTCTRL0, RXS, EP_STALL);
            USB_REG_WR(ENDPTCTRL0, RegVal);
        }
    }
    else if (EndPoint < pUsbFuncCtxt->UsbfEndpointCount)
    {
        if (USB_IS_EP_IN(EndPoint))
        {
            USB_REG_OFFSET_UPDATE_NUM(EpOffs, ENDPTCTRL1, TXS, StallState);
            if (!StallState)
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL1, TXR, RESET_PID_SEQ);
        }
        else
        {
            USB_REG_OFFSET_UPDATE_NUM(EpOffs, ENDPTCTRL1, RXS, StallState);
            if (!StallState)
                USB_REG_OFFSET_UPDATE_DEF(EpOffs, ENDPTCTRL1, RXR, RESET_PID_SEQ);
        }
    }
}

NvError
UsbPrivAckEndpoint(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint)
{
    NvError retStatus = NvSuccess;

    // NvOsDebugPrintf("Enter USBF ACK EP\n");

    retStatus = UsbPrivTxfrClear(pUsbFuncCtxt, EndPoint);
    if (retStatus == NvSuccess)
    {
        // Configure the endpoint to send zero byte ack
        UsbPrivTxfrStart(pUsbFuncCtxt, EndPoint, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE);

        // Wait for transfer to finish or error.
        retStatus = UsbPrivTxfrWait(pUsbFuncCtxt, EndPoint, USB_ACK_WAIT_TIME_MS);

        if (retStatus == NvSuccess)
        {
            // Indicate the endpoint is cleared.
            pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] = NV_FALSE;
            // Clear the ep transfer error info.
            pUsbFuncCtxt->EpTxfrError[EndPoint] = NV_FALSE;
            // Clear the endpoint bytes requested
            pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint] = 0;
            // clear the ep transfer complete
            pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_FALSE;
            // clear the signal indication status
            pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;
        }
    }

    // NvOsDebugPrintf("Exit USBF ACK EP\n");
    return retStatus;
}

void
UsbPrivSetAddress(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 Address)
{
    // Set device address in the USB controller
    USB_REG_UPDATE_NUM(PERIODICLISTBASE, USBADR, Address);
}

void
UsbPrivISR(
    void *arg)
{
    NvDdkUsbFunction *pUsbFuncCtxt = (NvDdkUsbFunction *)arg;

    NV_ASSERT(pUsbFuncCtxt);

    NvOsIntrMutexLock(pUsbFuncCtxt->hIntrMutex);

    //NvOsDebugPrintf("Enter USBF ISR\n");
    // Allow ISR to handle events only if there is Open RefCount
    if (pUsbFuncCtxt->RefCount)
    {
        UsbPrivAckInterrupts(pUsbFuncCtxt);
        NvRmInterruptDone(pUsbFuncCtxt->InterruptHandle);
    }
    //NvOsDebugPrintf("Exit USBF ISR\n");

    NvOsIntrMutexUnlock(pUsbFuncCtxt->hIntrMutex);
}

NvError
UsbPrivRegisterInterrupts(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvRmDeviceHandle hDevice;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;
    if (pUsbFuncCtxt->InterruptHandle)
    {
        return NvSuccess;
    }
    hDevice = pUsbFuncCtxt->hRmDevice;
    IntHandlers = UsbPrivISR;
    IrqList = NvRmGetIrqForLogicalInterrupt(hDevice,
                NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, pUsbFuncCtxt->Instance),
                0);

    return NvRmInterruptRegister(hDevice, 1, &IrqList, &IntHandlers,
                pUsbFuncCtxt, &(pUsbFuncCtxt->InterruptHandle), NV_TRUE);
}

static void
UsbPrivAckInterrupts (
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvU32 NewUsbInts = 0;
    NvU32 EpCompleted = 0;
    NvU32 EpSetupStatus = 0;
    NvU32 regVal = 0;
    NvU32 EndPoint = 0;
    NvBool SingalClientSema = NV_FALSE;
    NvBool UsbTxfrError = NV_FALSE;
    UsbLineStateType  LineState = UsbLineStateType_SE0;

    // Assert if handle is not created
    NV_ASSERT(pUsbFuncCtxt && pUsbFuncCtxt->RefCount);

    if (pUsbFuncCtxt->IsUsbSuspend == NV_TRUE)
    {
        pUsbFuncCtxt->UsbfCableConnected = UsbfPrivIsCableConnected(pUsbFuncCtxt);
        pUsbFuncCtxt->NotifyReset = NV_FALSE;
        pUsbFuncCtxt->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        if (pUsbFuncCtxt->UsbfCableConnected)
        {
            SingalClientSema = NV_TRUE;
            // Disable the VBUS interrupt
            UsbfPrivConfigVbusInterrupt(pUsbFuncCtxt, NV_FALSE);
        }
    }
    else
    {
        // Get bitmask of status for new USB ints that we have enabled.
        regVal = USB_REG_RD(USBSTS);
        // prepare the Interupt status mask and get the interrupted bits value
        NewUsbInts = (regVal & (USB_DRF_DEF(USBSTS, SLI, SUSPENDED) |
                                USB_DRF_DEF(USBSTS, SRI, SOF_RCVD) |
                                USB_DRF_DEF(USBSTS, URI, USB_RESET) |
                                USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
                                USB_DRF_DEF(USBSTS, SEI, ERROR) |
                                USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
                                USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
                                USB_DRF_DEF(USBSTS, UEI, ERROR) |
                                USB_DRF_DEF(USBSTS, UI, INT)));
        // Clear the interrupted bits by writing back the staus value to register
        regVal |= NewUsbInts;
        USB_REG_WR(USBSTS, regVal);
        // Read the endpoint complete status.
        EpCompleted = USB_REG_RD(ENDPTCOMPLETE);
        // Write back the same value to clear the register
        USB_REG_WR(ENDPTCOMPLETE, EpCompleted);
        // Get the USB Setup packet status
        EpSetupStatus = USB_REG_RD(ENDPTSETUPSTAT);
        // Write back the same value to clear the register
        USB_REG_WR(ENDPTSETUPSTAT, EpSetupStatus);

        regVal = USB_REG_RD(OTGSC);
        USB_REG_WR(OTGSC, regVal);
        if (USB_DRF_VAL(OTGSC, BSVIS, regVal))
        {
            if (USB_DRF_VAL(OTGSC, BSV, regVal))
            {
                // cable is present
                pUsbFuncCtxt->UsbfCableConnected = NV_TRUE;
                pUsbFuncCtxt->NotifyReset = NV_FALSE;
            }
            else
            {
                // cable is disconnected
                pUsbFuncCtxt->UsbfCableConnected = NV_FALSE;
                pUsbFuncCtxt->NotifyReset = NV_FALSE;
            }
        }

        if (NewUsbInts & USB_DRF_DEF(USBSTS, SLI, SUSPENDED))
        {
            // NvOsDebugPrintf("USB Port Suspend Interrupt\n");
            pUsbFuncCtxt->PortSuspend = NV_TRUE;
            pUsbFuncCtxt->NotifyReset = NV_TRUE;
        }

        if (NewUsbInts & USB_DRF_DEF(USBSTS, URI, USB_RESET))
        {
            // NvOsDebugPrintf("UsbEventType_Reset\n");
            if (pUsbFuncCtxt->NotifyReset)
            {
                pUsbFuncCtxt->UsbResetDetected = NV_TRUE;
            }
            pUsbFuncCtxt->CheckForCharger = NV_FALSE;
            pUsbFuncCtxt->ChargerDetected = NV_FALSE;
        }

        if (NewUsbInts & USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE))
        {
            // NvOsDebugPrintf("UsbEventType_PortChange\n");
            pUsbFuncCtxt->PortChangeDetected = NV_TRUE;
            pUsbFuncCtxt->UsbPortSpeed = (UsbPortSpeedType)
            USB_REG_READ_VAL(HOSTPC1_DEVLC, PSPD);
        }

        // Check for setup packet arrival. Store it and clear the setup lockout.
        if (EpSetupStatus & USB_DRF_DEF(ENDPTSETUPSTAT, ENDPTSETUPSTAT0, SETUP_RCVD))
        {
            // NvOsDebugPrintf("Setup packet Int\n");
            pUsbFuncCtxt->SetupPacketDetected = NV_TRUE;
            NvOsMemcpy((void*) &pUsbFuncCtxt->setupPkt,
                (void*) &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[USB_EP_CTRL_OUT].setupBuffer0,
                USB_SETUP_PKT_SIZE);
            pUsbFuncCtxt->CheckForCharger = NV_FALSE;
            pUsbFuncCtxt->ChargerDetected = NV_FALSE;
        }
        // Check if there is any transaction error occured
        if (NewUsbInts & USB_DRF_DEF(USBSTS, UEI, ERROR))
        {
            // NvOsDebugPrintf("UsbEventType_Error\n");
            UsbTxfrError = NV_TRUE;
        }

        if ((EpCompleted & USB_DRF_DEF(ENDPTCOMPLETE, ERCE0, COMPLETE)) &&
            !pUsbFuncCtxt->SetupPacketDetected)
        {
            pUsbFuncCtxt->EpTxferCompleted[USB_EP_CTRL_OUT] = NV_TRUE;
            pUsbFuncCtxt->EpTxfrError[USB_EP_CTRL_OUT] = UsbTxfrError;
        }

        if (EpCompleted & USB_DRF_DEF(ENDPTCOMPLETE, ETCE0, COMPLETE))
        {
            pUsbFuncCtxt->EpTxferCompleted[USB_EP_CTRL_IN] = NV_TRUE;
            pUsbFuncCtxt->EpTxfrError[USB_EP_CTRL_IN] = UsbTxfrError;
        }

        EpCompleted &= ~(USB_DRF_DEF(ENDPTCOMPLETE, ERCE0, COMPLETE) |
                         USB_DRF_DEF(ENDPTCOMPLETE, ETCE0, COMPLETE));
        EpCompleted >>= 1;

        for (EndPoint=USB_EP_MISC;
             EndPoint<pUsbFuncCtxt->UsbfEndpointCount && EpCompleted; EndPoint+=2)
        {
            if (EpCompleted & USB_DRF_DEF(ENDPTCOMPLETE, ERCE0, COMPLETE))
            {
                pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_TRUE;
                pUsbFuncCtxt->EpTxfrError[EndPoint] = UsbTxfrError;
            }

            if (EpCompleted & USB_DRF_DEF(ENDPTCOMPLETE, ETCE0, COMPLETE))
            {
                pUsbFuncCtxt->EpTxferCompleted[EndPoint+1] = NV_TRUE;
                pUsbFuncCtxt->EpTxfrError[EndPoint+1] = UsbTxfrError;
            }

            EpCompleted &= ~(USB_DRF_DEF(ENDPTCOMPLETE, ERCE0, COMPLETE) |
                             USB_DRF_DEF(ENDPTCOMPLETE, ETCE0, COMPLETE));
            EpCompleted >>= 1;
        }

        for (EndPoint = 0; EndPoint < pUsbFuncCtxt->UsbfEndpointCount; EndPoint++)
        {
            if (pUsbFuncCtxt->SignalTxferSema[EndPoint] &&
                pUsbFuncCtxt->EpTxferCompleted[EndPoint])
            {
                pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;
                pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_FALSE;
                pUsbFuncCtxt->EpTxfrError[EndPoint] = UsbTxfrError;
                NvOsSemaphoreSignal(pUsbFuncCtxt->hTxferSema);
            }
        }

        SingalClientSema = NV_TRUE;
    }

    if (pUsbFuncCtxt->CheckForCharger)
    {
        if (UsbfPrivIsChargerDetected(pUsbFuncCtxt))
        {
            pUsbFuncCtxt->ChargerDetected = NV_TRUE;
            LineState = USB_REG_READ_VAL(PORTSC1, LS);
            if (LineState == UsbLineStateType_SE0)
            {
                pUsbFuncCtxt->UsbfChargerType = NvOdmUsbChargerType_SE0;
            }
            else
            {
                pUsbFuncCtxt->UsbfChargerType = NvOdmUsbChargerType_SE1;
            }
            UsbfPrivConfigureChargerDetection(pUsbFuncCtxt, NV_FALSE, NV_FALSE);
        }
        else
        {
            pUsbFuncCtxt->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        }
    }

    if (pUsbFuncCtxt->hClientSema && SingalClientSema)
    {
        NvOsSemaphoreSignal(pUsbFuncCtxt->hClientSema);
    }

}

void
UsbPrivTxfrStart(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 MaxTxfrBytes,
    NvU32 PhyBufferAddr,
    NvBool SignalSema,
    NvBool EnableZlt,
    NvBool EnableIntr)
{
    volatile UsbDevQueueHead *pUsbDevQueueHead = NULL;
    UsbDevTransDesc *pUsbDevTxfrDesc = NULL;
    // NvU32 regVal = 0;
    NvU32 BytesLeft = 0;
    NvU32 DtdIndex = 0;
    NvU32 BufferIndex = 0;
    NvU32 BytesPerDTD = 0;
    NvU32 Eptype = UsbPrivGetEpType(pUsbFuncCtxt, EndPoint);

    // Reference to queue head for configureation.
    pUsbDevQueueHead = &pUsbFuncCtxt->UsbDescriptorBuf.pQHeads[EndPoint];

    // Interrupt on setup if it is endpoint 0_OUT.
    if (EndPoint == USB_EP_CTRL_OUT)
    {
        pUsbDevQueueHead->EpCapabilities = USB_DQH_DRF_DEF(IOC, ENABLE);
    }

    // setup the Q head with Max packet length and zero length terminate
    pUsbDevQueueHead->EpCapabilities |= (USB_DQH_DRF_NUM(MAX_PACKET_LENGTH,
                    UsbPrivGetPacketSize(pUsbFuncCtxt, EndPoint))|
                    (EnableZlt ? USB_DQH_DRF_DEF(ZLT, ZERO_LENGTH_TERM_ENABLED) :
                    USB_DQH_DRF_DEF(ZLT, ZERO_LENGTH_TERM_DISABLED)));

    // Set mult field for ISO endpoints.
    if (Eptype == USB_DRF_DEF_VAL(ENDPTCTRL0, TXT, ISO))
    {
        pUsbDevQueueHead->EpCapabilities |= USB_DQH_DRF_DEF(MULT, MULTI_1);
    }

    // Don't to terminate the next DTD pointer by writing 0 = Clear
    // we will assign the DTD pointer after configuring DTD
    pUsbDevQueueHead->NextDTDPtr = USB_DQH_DRF_DEF(TERMINATE, CLEAR);

    // Indicate EP is configured
    pUsbFuncCtxt->UsbDescriptorBuf.EpConfigured[EndPoint] = NV_TRUE;
    // Set to FALSE before starting the transaction
    pUsbFuncCtxt->EpTxfrError[EndPoint] = NV_FALSE;
    // Store the Bytes requested by client
    pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint] = MaxTxfrBytes;

    // Interrupt on transfer complete
    pUsbDevQueueHead->DtdToken = EnableIntr ?
                                 USB_DQH_DRF_DEF(IOC, ENABLE):
                                 USB_DQH_DRF_DEF(IOC, DISABLE);

    // indicate semaphore signaling for synchronus transfers.
    if (SignalSema)
    {
        pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_TRUE;
    }
    else
    {
        pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;
    }

    // Assign bytes left to the MaxTxfrBytes requested for loop
    BytesLeft = MaxTxfrBytes;

    // Each endpoint has four DTDS and can transfer upto 64 Kbytes.
    // Each DTD can transfer 16 KBytes each of 4Kbytes
    // Start with DtdIndex for this endpoint = 0
    DtdIndex = 0;
    do
    {
        // Reference to DTD for configuration.
        pUsbDevTxfrDesc = &pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
                           [(EndPoint*USB_MAX_DTDS_PER_EP) + DtdIndex];

        BytesPerDTD = USB_GET_MIN(USB_MAX_DTD_BUF_SIZE, BytesLeft);

        // Set number of bytes to transfer in DTD.and set status to active
        pUsbDevTxfrDesc->DtdToken = USB_DTD_DRF_DEF(ACTIVE, SET) |
                                    (EnableIntr ? USB_DTD_DRF_DEF(IOC, ENABLE) :
                                     USB_DTD_DRF_DEF(IOC, DISABLE)) |
                                    USB_DTD_DRF_NUM(TOTAL_BYTES, BytesPerDTD);

        // Decriment Bytes left, if zero bytes packet no need to dicriment
        if (BytesLeft)
        {
            BytesLeft -= BytesPerDTD;
        }
        BufferIndex = 0;
        // Allocate Buffers for One DTD = Max Four Buffers of 4KBytes
        while (BytesPerDTD)
        {
            // Assign buffer pointer to DTD.
            pUsbDevTxfrDesc->BufPtrs[BufferIndex++] = PhyBufferAddr;
            BytesPerDTD -= USB_GET_MIN(USB_MAX_MEM_BUF_SIZE, BytesPerDTD);
            // Set to Next Physical address
            PhyBufferAddr += USB_MAX_MEM_BUF_SIZE;
        }

        // Assign the first DTD pointer to the Queue head
        if (DtdIndex == 0)
        {
            // DTD address need to program only upper 27 bits
            pUsbDevQueueHead->NextDTDPtr |= USB_DQH_DRF_NUM(NEXT_DTD_PTR,
                            (UsbPrivGetPhyDTD(pUsbFuncCtxt, pUsbDevTxfrDesc) >>
                            USB_DQH_FLD_SHIFT_VAL(NEXT_DTD_PTR)));
        }
        else
        {
            // If we are chaining the DTDs then assign the curent DTD ptr to the
            // Next of the previous DTD pointer.
            pUsbFuncCtxt->UsbDescriptorBuf.pDataTransDesc
            [(EndPoint*USB_MAX_DTDS_PER_EP) + (DtdIndex - 1)].NextDtd =
            UsbPrivGetPhyDTD(pUsbFuncCtxt, pUsbDevTxfrDesc);
        }

        if (BytesLeft)
        {
            // If more bytes left then chain the DTD, incriment for next DTD Index
            DtdIndex++;
        }
        else
        {
            // If we don't have any more bytes left then this is the last DTD
            // terminate it.
            pUsbDevTxfrDesc->NextDtd = USB_DTD_DRF_DEF(TERMINATE, SET);
        }
    } while (BytesLeft);

    // Clear the EP txfer complete before starting the transaction
    pUsbFuncCtxt->EpTxferCompleted[EndPoint] = NV_FALSE;
    // Start the transaction on the USB Bus by priming the endpoint
    UsbPrivPrimeEndpoint(pUsbFuncCtxt, EndPoint);
    // regVal = USB_REG_RD(ENDPTPRIME);
    // regVal |= USB_EP_NUM_TO_WORD_MASK(EndPoint);;
    // USB_REG_WR(ENDPTPRIME, regVal);
}

static void UsbPrivPrimeEndpoint(NvDdkUsbFunction *pUsbFuncCtxt, NvU32 EndPoint)
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 PrimeTry = 0;
    NvU32 regVal = 0;
    NvU32 EndpointMask;

    // Get the Endpoint mask depending on the endpoint number
    EndpointMask = USB_EP_NUM_TO_WORD_MASK(EndPoint);

    // Try priming the endpoint several times.for OUT transfers
    for (PrimeTry = 0; PrimeTry < 5; PrimeTry++)
    {
        // Prime the endpoint
        regVal = USB_REG_RD(ENDPTPRIME);
        regVal |= USB_EP_NUM_TO_WORD_MASK(EndPoint);
        USB_REG_WR(ENDPTPRIME, regVal);

        // No retries, bail out.for IN endpoint
        if (USB_IS_EP_IN(EndPoint))
            return;

        // Wait till prime bit goes away for OUT endpoint
        TimeOut = CONTROLLER_HW_TIMEOUT_US;
        do {
            // Watch for endpoint to be primed
            if ((USB_REG_RD(ENDPTPRIME) & EndpointMask) ||
                (USB_REG_RD(ENDPTSTATUS) & EndpointMask))
            {
               // Return if endpoint is enabled
                return;
            }
            NvOsWaitUS(1);
            TimeOut--;
        } while (TimeOut);

        // go back to the loop and retry priming the endpoint
    }

    NV_DEBUG_PRINTF(("Could Not prime EP[%d] for Bytes[%d] \n", EndPoint,
                     pUsbFuncCtxt->UsbDescriptorBuf.BytesRequestedForEp[EndPoint]));
}


static NvError
UsbPrivWaitForSyncTxferComplete(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 WaitTimeoutMs)
{
    NvError retStatus = NvSuccess;
    NvU32 TimeOut = USB_MAX_TXFR_WAIT_TIME_MS;

    //NvOsDebugPrintf("hTxferSema Wait for EP[%d]\n", EndPoint);
    retStatus = NvOsSemaphoreWaitTimeout(pUsbFuncCtxt->hTxferSema, WaitTimeoutMs);

    // We are here means either by now we got the semaphore signaled or
    // semaphore wait is timedout, we don't need any more signal for this EP.
    pUsbFuncCtxt->SignalTxferSema[EndPoint] = NV_FALSE;

    if ((retStatus != NvError_Timeout) && WaitTimeoutMs)
    {
        // We are not timed out means ISR signled the semaphore indicating transfer complete
        // if transfer is active, wait for transfer complete status in the Queue heads,
        // There may be delay for updating the Queue head status. Wait till hardware
        // time out or EP status chnage before getting out of the loop
        do {
            // Get atleaset once the EP status
            retStatus = UsbPrivEpGetStatus(pUsbFuncCtxt, EndPoint);
            if (retStatus != NvError_UsbfTxfrActive)
            {
                // Ep Status is changed bailout
                break;
            }
            NvOsSleepMS(1);
            TimeOut--;
        } while (TimeOut);
    }
    else
    {
        // Get the transfer status to see if current transaction is completed,
        retStatus = UsbPrivEpGetStatus(pUsbFuncCtxt, EndPoint);
    }

    // Completed is the same thing as OK.
    if (retStatus == NvError_UsbfTxfrComplete)
    {
        retStatus = NvSuccess;
    }

    return retStatus;
}

void
UsbPrivSetTestMode(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 TestModeSelector)
{
    USB_REG_UPDATE_NUM(PORTSC1, PTC, TestModeSelector);
}

NvU32 UsbfPrivGetEndpointCount(NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvU32 EndpointCount = 0;

    /* Number of endpoints IN and OUT togather is */
    EndpointCount = (USB_REG_READ_VAL(DCCPARAMS, DEN)) * 2;

    return EndpointCount;
}

NvBool
UsbfPrivIsCableConnected(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvDdkUsbPhyIoctl_VBusStatusOutputArgs Status;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(pUsbFuncCtxt->hUsbPhy,
            NvDdkUsbPhyIoctlType_VBusStatus,
            NULL, &Status));

    return Status.VBusDetected;
}

NvBool
UsbfPrivIsChargerDetected(
    NvDdkUsbFunction *pUsbFuncCtxt)
{
    NvDdkUsbPhyIoctl_DedicatedChargerStatusOutputArgs Status;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(pUsbFuncCtxt->hUsbPhy,
            NvDdkUsbPhyIoctlType_DedicatedChargerStatus,
            NULL, &Status));

    return Status.ChargerDetected;
}

void
UsbfPrivConfigureChargerDetection(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvBool EnableDetection,
    NvBool EnableInterrupt)
{
    NvDdkUsbPhyIoctl_DedicatedChargerDetectionInputArgs Charger;

    Charger.EnableChargerDetection = EnableDetection;
    Charger.EnableChargerInterrupt = EnableInterrupt;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(pUsbFuncCtxt->hUsbPhy,
            NvDdkUsbPhyIoctlType_DedicatedChargerDetection,
            &Charger, NULL));
}

void
UsbfPrivConfigVbusInterrupt(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvBool Enable)
{
    NvDdkUsbPhyIoctl_VBusInterruptInputArgs Vbus;

    Vbus.EnableVBusInterrupt = Enable;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(pUsbFuncCtxt->hUsbPhy,
            NvDdkUsbPhyIoctlType_VBusInterrupt,
            &Vbus, NULL));
}



