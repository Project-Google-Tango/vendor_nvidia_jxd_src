/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
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
 *           NvDDK USB Host private functions</b>
 *
 * @b Description: Implements USB Host  controller private functions
 *
 */

#define NV_ENABLE_DEBUG_PRINTS 0

#include "nvddk_usbh_priv.h"
#include "t12x/arusb.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvassert.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvrm_pmu.h"


// .---------------------------MACRO DEFINITIONS--------------------------------

/* Defines for USB register read and writes */
#define USB_REG_RD(reg)\
    NV_READ32(pUsbHcdCtxt->UsbOtgVirAdr + (pUsbHcdCtxt->RegOffsetArray[(UsbhRegId_##reg)]/4))

#define USB_REG_WR(reg, data)\
    NV_WRITE32(pUsbHcdCtxt->UsbOtgVirAdr + (pUsbHcdCtxt->RegOffsetArray[(UsbhRegId_##reg)]/4), (data))

// Read perticular field value from reg mentioned
#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, reg, field, value)

// Define  macro with specified register, field & defines
#define USB_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, reg, field, define)

#define USB_DRF_DEF_VAL(reg, field, define) \
    (USB2_CONTROLLER_USB2D##_##reg##_0_##field##_##define)

#define USB_REG_READ_VAL(reg, field) \
    USB_DRF_VAL(reg, field, USB_REG_RD(reg))

#define USB_DEF_RESET_VAL(reg) \
    NV_RESETVAL(USB2_CONTROLLER_USB2D, reg)

#define USB_REG_SET_DRF_NUM(reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB2_CONTROLLER_USB2D, reg, field, num, USB_REG_RD(reg))

#define USB_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_USB2D, reg, field, define, USB_REG_RD(reg))

#define USB_REG_UPDATE_NUM(reg, field, num) \
    USB_REG_WR(reg, USB_REG_SET_DRF_NUM(reg, field, num))

// UPdate "field" of "reg" with the "define" value
#define USB_REG_UPDATE_DEF(reg, field, define) \
    USB_REG_WR(reg, USB_REG_SET_DRF_DEF(reg, field, define))

/* Defines for USB Device Queue Head read and writes */
#define USB_HQH_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, HOST_QUEUE_HEAD, field, define)

/* Shift the number to the field _RANGE */
#define USB_HQH_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, HOST_QUEUE_HEAD, field, number)

// Read specified "field" from "Value" passed
#define USB_HQH_DRF_VAL(field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, HOST_QUEUE_HEAD, field, value)

/* Defines for USB Device Transfer Desctiptor read and writes */
#define USB_HTD_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, HOST_QUEUE_TRANSFER_DESCRIPTOR, field, \
define)

#define USB_HTD_DRF_DEF_VAL(field, define) \
    USB_DRF_DEF_VAL(HOST_QUEUE_TRANSFER_DESCRIPTOR, field, define)

// Shifts the number  to the field _RANGE
#define USB_HTD_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, HOST_QUEUE_TRANSFER_DESCRIPTOR, field, \
number)

// Read a field  from "Value" passed
#define USB_HTD_DRF_VAL(field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, HOST_QUEUE_TRANSFER_DESCRIPTOR, field, value)

#define CHIP_ID_NUM_T124 0x40

void UsbhPrivGetCapabilities(NvDdkUsbhHandle pUsbHcdCtxt)
{
    static NvDdkUsbhCapabilities s_UsbhCap[] =
    {
        {CHIP_ID_NUM_T124,NV_FALSE},
    };
    NvDdkUsbhCapabilities *pUsbhCap = NULL;
    NvRmModuleCapability s_UsbhModCap[] =
    {
        {2, 0, 0, NvRmModulePlatform_Silicon, &s_UsbhCap[1]}, // T124, USB1
        {2, 1, 0, NvRmModulePlatform_Silicon, &s_UsbhCap[1]}, // T124 USB2
        {2, 2, 0, NvRmModulePlatform_Silicon, &s_UsbhCap[1]}, // T123 USB3
    };

    NV_ASSERT_SUCCESS(
        NvRmModuleGetCapabilities(pUsbHcdCtxt->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, pUsbHcdCtxt->Instance),
            s_UsbhModCap, NV_ARRAY_SIZE(s_UsbhModCap),
            (void**)&pUsbhCap));

    // Fill the client capabilities structure.
    NvOsMemcpy(&pUsbHcdCtxt->Caps, pUsbhCap, sizeof(NvDdkUsbhCapabilities));

    pUsbHcdCtxt->Caps.ChipId = 0x40;
    pUsbHcdCtxt->Caps.IsPortSpeedFromPortSC = NV_FALSE;

    if (pUsbHcdCtxt->Caps.ChipId == CHIP_ID_NUM_T124)
        UsbhPrivT124StoreRegOffsets(pUsbHcdCtxt);

    pUsbHcdCtxt->UsbhGetPortSpeed = &UsbhPrivT124GetPortSpeed;
}


static void ReleaseTransferTds(NvDdkUsbhHandle pUsbHcdCtxt, NvDdkUsbhTransfer *pTransfer)
{
    UsbHcdQh *ptempHcdQh = (UsbHcdQh*)pTransfer->EpPipeHandler;
    UsbHcdTd *ptempHcdTD = ptempHcdQh->Td_Head;
    NvU8 bufferStartIndx = 0, BuffersCount = 0;

    while (ptempHcdTD)
    {
        /* If This perticular TD is belongs to specified  tranfer then only release this TD. */
        if (ptempHcdTD->pTransfer == pTransfer)
        {
            // Release buffer associated with this TD
            bufferStartIndx = (ptempHcdTD->pBuffer -pUsbHcdCtxt->pUsbXferBuf) /USB_MAX_MEM_BUF_SIZE;
            BuffersCount = ((ptempHcdTD->bytesToTransfer -1) / USB_MAX_MEM_BUF_SIZE) + 1;
            // Change the buffer indexes to 1(used)
            NvOsMemset((void*)&pUsbHcdCtxt->pUsbXferBufIndex[bufferStartIndx], 0, BuffersCount);
            ptempHcdTD->Used = 0;
        }
        ptempHcdTD = ptempHcdTD->pNxtHcdTd;
    }
}

static void UsbhPrivAddTransferToList(NvDdkUsbhHandle pUsbHcdCtxt, NvDdkUsbhTransfer *pTransfer)
{
    // TODO: need to handle  multiple transfers
#if 0
    // Cheak wheather transfer list is NULL
    if (TransferListHead != NULL)
    {
        // Loop till your  End of the list
        while (TransferListHead->pNxtTranfer != NULL)
        {
            TransferListHead = TransferListHead->pNxtTranfer;
        }
    }
    // incase  transfer list doesnt have any pending trasnfers
    if (TransferListHead == NULL)
    {
        TransferListHead = pTransfer;
    }
    else
    {
        TransferListHead->pNxtTranfer = pTransfer;
    }
#endif

    // Incase  transfer list doesnt have any pending trasnfers
    if (pUsbHcdCtxt->pTransferListHead == NULL)
    {
        pUsbHcdCtxt->pTransferListHead = pTransfer;
    }
}

static void RemoveTransferFromList(NvDdkUsbhHandle pUsbHcdCtxt, NvDdkUsbhTransfer *pTransfer)
{
    // NvDdkUsbhTransfer * TransferListHead = pUsbHcdCtxt->pTransferListHead;

    // TODO: Asume only one tranfer is submitted At a time.
    // TODO: Need to update this func, to mutiple transfers

    if (pUsbHcdCtxt->pTransferListHead != NULL)
    {
        pUsbHcdCtxt->pCompeltedTransferListHead = pUsbHcdCtxt->pTransferListHead;
        pUsbHcdCtxt->pTransferListHead = NULL;
    }
}

static void UsbhPrivAsyncEventHandler(NvDdkUsbhHandle pUsbHcdCtxt, NvU32 TranErr)
{
    NvDdkUsbhTransfer *pTransfer = pUsbHcdCtxt->pTransferListHead;
    UsbHcdQh *ptempHcdQh;
    UsbHcdTd *ptempHcdTD;
    NvU32 transferredBytes = 0, DtdToken = 0;
    NvU32 Status = 0, PidOfTd = 0;

    // incase if pTransfer is NULL, then procede
    while (pTransfer)
    {
     /*
      * Take TD head from HCDQH of perticular tranfer and verify status of the TD.
      * If still active,go for next transfer. If transfer status is 0,
      * then transfer is done and success. In this case,
      * calculate total tranfferred bytes, update the pTransfer and go to the next TD.
      * If transfer status is failure, fill the transfer status with failure TD status
      *.and go to the next transfer.
      */
        ptempHcdQh = (UsbHcdQh*)pTransfer->EpPipeHandler;
        ptempHcdTD = ptempHcdQh->Td_Head;
        pTransfer->ActualLength = 0;

        while (ptempHcdTD != NULL)
        {
            DtdToken = ptempHcdTD->pHcTd->DtdToken;
            Status = (DtdToken & (USB_HTD_DRF_DEF(ACTIVE, SET) |
                                   USB_HTD_DRF_DEF(HALTED, SET) |
                                   USB_HTD_DRF_DEF(DATA_BUFFER_ERROR, SET) |
                                   USB_HTD_DRF_DEF(BABBLE_DETECTED, SET) |
                                   USB_HTD_DRF_DEF(TRANSACTION_ERROR, SET) |
                                   USB_HTD_DRF_DEF(MISSED_MICRO_FRAME, SET)));

            // Read the PID of this TD
            PidOfTd = USB_HTD_DRF_VAL(PID_CODE, DtdToken);

            NV_DEBUG_PRINTF(("USBHDDK::UsbhPrivAsyncEventHandler status = %d::PidofTD = %d\n", Status, PidOfTd));
            /* If Status of TD still active, this TD might not have been processed yet. then check next ptransfer */
            if (Status & USB_HTD_DRF_DEF(ACTIVE, SET))
            {
                break;
            }
            else if (Status && TranErr) // If overall transfer is success, go to next TD
            {
                /* It represents some error  so just fill the Status of the transfer with this status and return back. */
                pTransfer->Status = Status;

                // This TD is failed so abort the transfer
                ReleaseTransferTds(pUsbHcdCtxt, pTransfer);
                RemoveTransferFromList(pUsbHcdCtxt, pTransfer);
                NV_DEBUG_PRINTF(("USBHDDK111::UsbhPrivAsyncEventHandler status = %d::PidofTD = %d\n", Status, PidOfTd));
                return;
            }
            // Store the status
            pTransfer->Status = Status;

            /**
             * TD IS SUCCESS.. Then calculate the bytes tranferred in this
             * TD and update the ptranfer.
             */
            if (PidOfTd != USB_HTD_DRF_DEF_VAL(PID_CODE, SETUP))
            {
                transferredBytes = ptempHcdTD->bytesToTransfer -
                        USB_HTD_DRF_VAL(TOTAL_BYTES, ptempHcdTD->pHcTd->DtdToken);

                // If it is IN PIPE, memcopy tranferred bytes to actual buffer provided by the client
                if (PidOfTd == USB_HTD_DRF_DEF_VAL(PID_CODE, IN))
                {
                    NvOsMemcpy((void *)&pTransfer->pTransferBuffer[pTransfer->ActualLength],
                            (void *)ptempHcdTD->pBuffer, transferredBytes);
                }
                // Update the actual Lengh transfered to  tranfer
                pTransfer->ActualLength += transferredBytes;
            }
            // Check for short packet transfer
            if (transferredBytes != ptempHcdTD->bytesToTransfer)
            {
                /*
                 * Incase of bulk, it represents the Short packet.
                 * So abort all TD's of this tranfer and return
                 * pTranfer  to the upper layer. For Control transfer type,
                 * just procede to next tranfer
                 */
                if (IS_PIPE_BULK(ptempHcdQh->Pipe))
                {
                    pTransfer->Status = Status;
                    ReleaseTransferTds(pUsbHcdCtxt, pTransfer);
                    RemoveTransferFromList(pUsbHcdCtxt, pTransfer);
                    return;
                }
            }
            // Go to the next HcdTD
            ptempHcdTD = ptempHcdTD->pNxtHcdTd;
        }

        /* Incase all TD of this transfer are completed so  Just retire the
           all TD's asociated with this tranfer */
        if (ptempHcdTD == NULL)
        {
            ReleaseTransferTds(pUsbHcdCtxt, pTransfer);
            RemoveTransferFromList(pUsbHcdCtxt, pTransfer);
            pTransfer->Status = Status;
            NV_DEBUG_PRINTF(("USBHDDK::UsbhPrivAsyncEventHandler Completed SUCCESS\n"));
            return;
        }
        /* If status of present transfer is active (0x80), then go to the next transfer in the list and verify */
        // TODO: Need to handle incase of multiple transfers in queue
        // pTransfer = pTransfer->pNxtTranfer;
    }
}

static NvError UsbhPrivAckInterrupts(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvError retStatus = NvSuccess;
    NvU32 NewUsbInts = 0;
    NvU32 regVal = 0;

    NV_ASSERT(pUsbHcdCtxt);

    // Get bitmask of status for new USB ints that we have enabled.
    regVal = USB_REG_RD(USBSTS);
    // prepare the Interupt status mask and get the interrupted bits value
    NewUsbInts = (regVal & (USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
                                USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
                                USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
                                USB_DRF_DEF(USBSTS, UEI, ERROR) |
                                USB_DRF_DEF(USBSTS, UI, INT)));
    // Clear the interrupted bits by writing back the staus value to register
    regVal |= NewUsbInts;
    USB_REG_WR(USBSTS, regVal);

    if (NewUsbInts & USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE))
    {
        if (USB_REG_READ_VAL(PORTSC1, CSC))
        {
            if (USB_REG_READ_VAL(PORTSC1, CCS))
            {
                // If CCS is 1, Device is present on port
                pUsbHcdCtxt->UsbhCableConnected = NV_TRUE;
                pUsbHcdCtxt->UsbhCableDisConnected = NV_FALSE;
                NV_DEBUG_PRINTF(
                    ("USBHDDK::UsbhPrivAckInterrupts::Cable Connected\n"));
            }
            else
            {
                pUsbHcdCtxt->UsbhCableDisConnected = NV_TRUE;
                pUsbHcdCtxt->UsbhCableConnected = NV_FALSE;
                 NV_DEBUG_PRINTF(
                     ("USBHDDK::UsbhPrivAckInterrupts::Cable Disconnected\n"));
            }
        }
        // Verify port has been enabled or not (It will be enabled by HC)
        if (USB_REG_READ_VAL(PORTSC1, PE))
        {
            pUsbHcdCtxt->PortEnabled = NV_TRUE;
            pUsbHcdCtxt->UsbhGetPortSpeed(pUsbHcdCtxt);
            NV_DEBUG_PRINTF(("USBHDDK::UsbhPrivAckInterrupts::Port Enabled\n"));
        }
    }

    // Check for USB Int or USB Error Interrupt
    if (NewUsbInts & (USB_DRF_DEF(USBSTS, UEI, ERROR) || USB_DRF_DEF(USBSTS, UI, INT)))
    {
        NV_DEBUG_PRINTF(
            ("USBHDDK::UsbhPrivAckInterruptsUSB INT or ERRORINT::%d\n", NewUsbInts));
        UsbhPrivAsyncEventHandler(pUsbHcdCtxt, (NewUsbInts & USB_DRF_DEF(USBSTS, UEI, ERROR)));
        pUsbHcdCtxt->TranferDone = NV_TRUE;

        if (pUsbHcdCtxt->EventSema)
        {
            NvOsSemaphoreSignal(pUsbHcdCtxt->EventSema);
        }
    }
    return retStatus;
}

void UsbHcdPrivISR(void *arg)
{
    UsbhPrivAckInterrupts((NvDdkUsbhHandle)arg);
    NvRmInterruptDone(((NvDdkUsbhHandle)arg)->InterruptHandle);
}

NvError UsbhPrivRegisterInterrupts(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvRmDeviceHandle hDevice = pUsbHcdCtxt->hRmDevice;
    NvOsInterruptHandler IntHandlers = UsbHcdPrivISR;
    NvU32 IrqList = NvRmGetIrqForLogicalInterrupt(hDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, pUsbHcdCtxt->Instance), 0);
    return NvRmInterruptRegister(
                hDevice,
                1,
                &IrqList,
                &IntHandlers,
                pUsbHcdCtxt,
                &(pUsbHcdCtxt->InterruptHandle),
                NV_TRUE);
}

static void UsbhPrivInitQheads(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvU32 i;
    // CLear data structures
    NvOsMemset((void*)pUsbHcdCtxt->pUsbDevHcQhAddr, 0, sizeof(UsbhQh) * USB_MAX_EP_COUNT);

    NvOsMemset((void*)pUsbHcdCtxt->pUsbDevHcTdAddr, 0, sizeof(UsbhTd) * USB_MAX_DTDS);

    NvOsMemset((void*)pUsbHcdCtxt->pUsbHcdQhAddr, 0, sizeof(UsbHcdQh) * USB_MAX_EP_COUNT);

    NvOsMemset((void*)pUsbHcdCtxt->pUsbHcdTdAddr, 0, sizeof(UsbHcdTd) * USB_MAX_DTDS);

    NvOsMemset((void*)pUsbHcdCtxt->pUsbXferBuf, 0, USB_MAX_TXFR_SIZE_BYTES);

    NvOsFlushWriteCombineBuffer();

    // Initialize HcdQh Qh address with  UsbhQh addess
    for (i = 0; i < USB_MAX_EP_COUNT; i++)
    {
        pUsbHcdCtxt->pUsbHcdQhAddr[i].pHcQh = &pUsbHcdCtxt->pUsbDevHcQhAddr[i];
        pUsbHcdCtxt->pUsbHcdQhAddr[i].Used = 0;
    }
    for (i = 0; i < USB_MAX_DTDS; i++)
    {
        pUsbHcdCtxt->pUsbHcdTdAddr[i].pHcTd = &pUsbHcdCtxt->pUsbDevHcTdAddr[i];
        pUsbHcdCtxt->pUsbHcdTdAddr[i].Used = 0;
    }
}

NvError UsbhPrivAllocateBuffers(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // Allocate memory for all QHs and QTD's
    NvError err = NvSuccess;
    static const NvRmHeap s_Heaps[] = {NvRmHeap_IRam};

    NvU32 Align;
    NvU32 Offset = 0;

    // One queue head per endpoint direction.each endpoint has 2 directions.
    pUsbHcdCtxt->VirtualBufferSize = sizeof(UsbhQh) * USB_MAX_EP_COUNT;
    // One DTD per buffer
    pUsbHcdCtxt->VirtualBufferSize += sizeof(UsbhTd) * USB_MAX_DTDS;
    // Transfer buffer size for IN and Out transactions
    pUsbHcdCtxt->VirtualBufferSize += USB_MAX_USB_BUF_SIZE;
    // UsbHcdQh buffer
    pUsbHcdCtxt->VirtualBufferSize += sizeof(UsbHcdQh) * USB_MAX_EP_COUNT;
    // USB Hcd TD buffer
    pUsbHcdCtxt->VirtualBufferSize += sizeof(UsbHcdTd) * USB_MAX_DTDS;

    // Alignment must be 4K for USB buffers.
    Align = USB_HW_BUFFER_ALIGNMENT;

    pUsbHcdCtxt->hUsbBuffMemHandle = NULL;
    err = NvRmMemHandleAlloc(pUsbHcdCtxt->hRmDevice,
                        s_Heaps, NV_ARRAY_SIZE(s_Heaps),
                        Align, NvOsMemAttribute_WriteCombined,
                        pUsbHcdCtxt->VirtualBufferSize, 0, 1,
                        &pUsbHcdCtxt->hUsbBuffMemHandle);
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("UsbhError>AllocateBuffers::Create memory handle Fail!\n"));
        goto fail;
    }

    pUsbHcdCtxt->UsbXferPhyBuf = NvRmMemPin(pUsbHcdCtxt->hUsbBuffMemHandle);

    // Mapping one time for the entire size;
    err = NvRmMemMap(pUsbHcdCtxt->hUsbBuffMemHandle,
                     Offset, pUsbHcdCtxt->VirtualBufferSize,
                     NVOS_MEM_READ_WRITE,
                     (void **)&(pUsbHcdCtxt->UsbVirtualBuffer));
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("UsbhError>AllocateBuffers::RmMemMap 1 Fail!\n"));
        goto fail;
    }

    Offset = 0;
    pUsbHcdCtxt->pUsbXferBuf = (pUsbHcdCtxt->UsbVirtualBuffer + Offset);

    Offset += USB_MAX_USB_BUF_SIZE;
    pUsbHcdCtxt->UsbDevHcQhPhyAddr = pUsbHcdCtxt->UsbXferPhyBuf + Offset;
    pUsbHcdCtxt->pUsbDevHcQhAddr = (UsbhQh *)(pUsbHcdCtxt->UsbVirtualBuffer + Offset);

    Offset += sizeof(UsbhQh) * USB_MAX_EP_COUNT;
    pUsbHcdCtxt->UsbDevHcTdPhyAddr = pUsbHcdCtxt->UsbXferPhyBuf + Offset;
    pUsbHcdCtxt->pUsbDevHcTdAddr = (UsbhTd*)(pUsbHcdCtxt->UsbVirtualBuffer + Offset);

    Offset += sizeof(UsbhTd) *USB_MAX_DTDS;
    pUsbHcdCtxt->UsbHcdQhPhyAddr = pUsbHcdCtxt->UsbXferPhyBuf + Offset;
    pUsbHcdCtxt->pUsbHcdQhAddr = (UsbHcdQh*)(pUsbHcdCtxt->UsbVirtualBuffer + Offset);

    Offset += sizeof(UsbHcdQh) * USB_MAX_EP_COUNT;
    pUsbHcdCtxt->UsbHcdTdPhyAddr = pUsbHcdCtxt->UsbXferPhyBuf + Offset;
    pUsbHcdCtxt->pUsbHcdTdAddr = (UsbHcdTd *)(pUsbHcdCtxt->UsbVirtualBuffer + Offset);

    Offset += sizeof(UsbHcdTd) * USB_MAX_EP_COUNT;
    // allocate memory for tranfer Buffer intex
    pUsbHcdCtxt->pUsbXferBufIndex = NvOsAlloc(sizeof(NvU8) * USB_MAX_MEM_BUF_COUNT);

    // Inititialize all buffer indexes to Zero
    NvOsMemset((void*)pUsbHcdCtxt->pUsbXferBufIndex, 0, sizeof(NvU8) * USB_MAX_MEM_BUF_COUNT);
    // Inititialize Qheads
    UsbhPrivInitQheads(pUsbHcdCtxt);
    return err;

fail:
    if (pUsbHcdCtxt->hUsbBuffMemHandle)
    {
        // Unmapping the Virtual Addresses
        NvRmMemUnmap(pUsbHcdCtxt->hUsbBuffMemHandle,
                     pUsbHcdCtxt->UsbVirtualBuffer,
                     pUsbHcdCtxt->VirtualBufferSize);
        // Unpinning the Memory
        NvRmMemUnpin(pUsbHcdCtxt->hUsbBuffMemHandle);
        NvRmMemHandleFree(pUsbHcdCtxt->hUsbBuffMemHandle);
    }
    return err;
}

void UsbhPrivDeAllocateBuffers(NvDdkUsbhHandle pUsbHcdCtxt)
{
    if (pUsbHcdCtxt->hUsbBuffMemHandle)
    {
        // Unmapping the Virtual Addresses
        NvRmMemUnmap(pUsbHcdCtxt->hUsbBuffMemHandle,
                pUsbHcdCtxt->UsbVirtualBuffer,
                pUsbHcdCtxt->VirtualBufferSize);
        // Unpinning the Memory
        NvRmMemUnpin(pUsbHcdCtxt->hUsbBuffMemHandle);
        NvRmMemHandleFree(pUsbHcdCtxt->hUsbBuffMemHandle);
    }
}

NvError UsbhPrivInitializeController(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US*2;
    NvU32 regValue;

    if (pUsbHcdCtxt->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiNullPhy)
    {
        // Reset the controller
        USB_REG_UPDATE_DEF(USBCMD, RST, SET);

        do
        {
            // Wait till bus reset clears.
            regValue = USB_REG_READ_VAL(USBCMD, RST);
            if (!TimeOut)
            {
                NV_DEBUG_PRINTF(("UsbhPrivInitController:: Wait for reset failed val = %d\n", regValue));
                return NvError_Timeout;
            }
            NvOsWaitUS(1);
            TimeOut--;
        } while (regValue);
    }
    regValue = USB_REG_READ_VAL(USBMODE, CM);
    return NvSuccess;
}

NvBool UsbPrivIsACableConnected(NvDdkUsbhHandle pUsbHcdCtxt)
{
    const NvOdmUsbProperty *pUsbProperty;

    NV_ASSERT(pUsbHcdCtxt);

    if (pUsbHcdCtxt->Instance == UTMI_INSTANCE)
    {
        pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb,
                pUsbHcdCtxt->Instance);
        if (pUsbProperty == NULL)
        {
            // no ODM USB Property set, so return NV_TRUE
            return NV_TRUE;
        }
        else if (pUsbProperty->ConMuxType == NvOdmUsbConnectorsMuxType_MicroAB_TypeA)
        {
            // always power the controller, do not wait for cable connect
            return NV_TRUE;
        }
        else if (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_CableId)
        {
            if (UsbhPrivIsIdPinSetToLow(pUsbHcdCtxt))
                return NV_TRUE;
            else
                return NV_FALSE;
        }
        else if (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_Gpio)
        {
            NvRmGpioPinState value = NvRmGpioPinState_High;
            NvRmGpioReadPins(pUsbHcdCtxt->hUsbHcdGpioHandle,
                    &pUsbHcdCtxt->CableIdGpioPin, &value, 1);
            if (value == NvRmGpioPinState_Low)
            {
                // A cable is present
                return NV_TRUE;
            }
        }
        else
        {
           // always return NV_TRUE;
           return NV_TRUE;
        }
    }
    else if (pUsbHcdCtxt->Instance == ULPI_INSTANCE)
    {
        return NV_TRUE;
    }
    else  // any other instance
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}


void UsbhPrivClearInterrupts(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // Update Controller USBINT register to enable interrupts
    NvU32 regValue;

    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));

    // clear all pending interrupts, if any
    regValue = (USB_DRF_DEF(USBSTS, SLI, SUSPENDED) |
            USB_DRF_DEF(USBSTS, SRI, SOF_RCVD) |
            USB_DRF_DEF(USBSTS, URI, USB_RESET) |
            USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
            USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
            USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
            USB_DRF_DEF(USBSTS, UEI, ERROR) |
            USB_DRF_DEF(USBSTS, UI, INT));
    USB_REG_WR(USBSTS, regValue);

    // clear OTGSC interupts
    regValue = (USB_DRF_DEF(OTGSC, DPIS, INT_SET) |
            USB_DRF_DEF(OTGSC, ONEMSS, INT_SET) |
            USB_DRF_DEF(OTGSC, BSEIS, INT_SET) |
            USB_DRF_DEF(OTGSC, BSVIS, INT_SET) |
            USB_DRF_DEF(OTGSC, ASVIS, INT_SET) |
            USB_DRF_DEF(OTGSC, AVVIS, INT_SET) |
            USB_DRF_DEF(OTGSC, IDIS, INT_SET));
    USB_REG_WR(OTGSC, regValue);
}

void UsbhPrivEnableInterrupts(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvU32 regValue;

    // Set interrupts to occur immediately
    USB_REG_UPDATE_DEF(USBCMD, ITC, IMMEDIATE);

    // Enable USB interrupts
    regValue = (USB_DRF_DEF(USBINTR, PCE, ENABLE) |
            USB_DRF_DEF(USBINTR, UEE, ENABLE) |
            USB_DRF_DEF(USBINTR, SEE, ENABLE) |
            USB_DRF_DEF(USBINTR, UE, ENABLE));
    USB_REG_WR(USBINTR, regValue);
}

void usbhPrivConfigPortPower(NvDdkUsbhHandle pUsbHcdCtxt, NvBool set)
{
    // check if the controller implements the port power control
    if (USB_REG_READ_VAL(HCSPARAMS, PPC))
    {
        // change the port power
        if (set)
        {
            USB_REG_UPDATE_DEF(PORTSC1, PP, POWERED);
        }
        else
        {
            USB_REG_UPDATE_DEF(PORTSC1, PP, NOT_POWERED);
        }
    }
}

void UsbhPrivEnableAsyncScheduleParkMode(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // check the Async schedule park capability  Default value of Async schedule park capability: 0x1.
    if (USB_REG_READ_VAL(HCCPARAMS, ASP))
    {
        // Set the Async schedule park mode enable bit
        USB_REG_UPDATE_DEF(USBCMD, ASPE, ENABLE);
        // Set the Async schedule park mode count
        // Default value of Async schedule park mode count:0x3
        // This holds good in the case of high speed endpoints only
        USB_REG_UPDATE_DEF(USBCMD, ASP1_ASP0, DEFAULT);
    }
}

NvBool UsbhPrivIsAsyncSheduleEnabled(NvDdkUsbhHandle pUsbHcdCtxt)
{
    if (USB_REG_READ_VAL(USBSTS, AS))
    {
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}
void UsbhPrivEnableAsyncShedule(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // Enable the schedule only if host controller is running.
    // otherwise we will hang up waiting for asyncScheduleSts
    // bit to become 1 which will never occur.
    if (USB_REG_READ_VAL(USBCMD, RS))
    {
        // Enable Async Schedule
        USB_REG_UPDATE_DEF(USBCMD, ASE, ENABLE);
        // Wait until   status  says Async Shedule enabled
        while (!(USB_REG_READ_VAL(USBSTS, AS) == USB_DRF_DEF_VAL(USBSTS, AS, ENABLE)));
    }
}

void UsbhPrivDisableAsyncShedule(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // Disable the shedule
    USB_REG_UPDATE_DEF(USBCMD, ASE, DISABLE);
    // Wait until  disable status updates into status register
    while (!(USB_REG_READ_VAL(USBSTS, AS) == USB_DRF_DEF_VAL(USBSTS, AS, DISABLE)));
}

///////////////  Queue Head and TD related functions   ///////////

NvError UsbhPrivAllocateQueueHead(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdQh **pNewHcdQh)
{
    NvU32 i = 0;
    for (; i < USB_MAX_EP_COUNT; i++)
    {
        if (pUsbHcdCtxt->pUsbHcdQhAddr[i].Used == 0)
        {
            *pNewHcdQh = &pUsbHcdCtxt->pUsbHcdQhAddr[i];
            // Set HcQh bytes as 0
            NvOsMemset((void *)(*pNewHcdQh)->pHcQh, 0, sizeof(UsbhQh));
            pUsbHcdCtxt->pUsbHcdQhAddr[i].Used = 1;
            break;
        }
    }
    if (i >= USB_MAX_EP_COUNT)
        return NvSuccess;  // TODO: Error Value need to be finalized

    return NvSuccess;
}

NvError
UsbhPrivConfigureQH(
    NvDdkUsbhHandle pUsbHcdCtxt,
    UsbHcdQh *pNQh,
    NvU32 pipe,
    NvU16 SpeedOfDevice,
    NvU32 MaxPktLength)
{
    // set type  as QH
    pNQh->pHcQh->QhHLinkPtr = USB_HQH_DRF_DEF(QH_ITD_TYPE, QH);
    pNQh->pHcQh->EpCapChar0 |= (USB_HQH_DRF_NUM(DEV_ADDRESS, USB_PIPEDEVICE(pipe)) |
                                    USB_HQH_DRF_NUM(ENPT_NUMBER, USB_PIPEENDPOINT(pipe)) |
                                    USB_HQH_DRF_NUM(ENDPT_SPEED, (SpeedOfDevice -1)) |
                                    USB_HQH_DRF_NUM(MAX_PKT_LENGTH, MaxPktLength));

    // Setting Data toglling
    if (IS_PIPE_CONTROL(pipe))
    {
        pNQh->pHcQh->EpCapChar0 |= USB_HQH_DRF_DEF(DATA_TOGGLE_CTRL, QTD_DT);
    }
    else if (IS_PIPE_BULK(pipe))
    {
        pNQh->pHcQh->EpCapChar0 |= USB_HQH_DRF_DEF(DATA_TOGGLE_CTRL, QH_DT);
    }

    // If the QH.EPS field indicates the endpoint is not a high-speed endpoint,
    // and the endpoint is a control endpoint, then software must set :Control
    // Ep Flag" bit to a one.
    if ((SpeedOfDevice != NvDdkUsbPortSpeedType_High) && (IS_PIPE_CONTROL(pipe)))
    {
        pNQh->pHcQh->EpCapChar0 |= USB_HQH_DRF_DEF(CTRL_EP_FLAG, CTRP_EP);
    }
    // port number for root hub will be 0
    // pNQh->pHcQh->EpCapChar1 |= (0 <<
    pNQh->Pipe = pipe;
    // Set terminate bit for nextDtd ptr as invalid
    pNQh->pHcQh->NextDTDPtr = USB_HQH_DRF_DEF(QTD_TERMINATE, INVALID_TD_PTR);
    pNQh->pHcQh->AltNextDTDPtr = USB_HQH_DRF_DEF(ALT_QTD_TERMINATE, INVALID_TD_PTR);
    return NvSuccess;
}

void UsbhPrivAddToAsyncList(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdQh *pNQh)
{
    UsbhQh *tempHcQh;

    // If the Async Schedule is empty, then make this QH as the first node
    if (USB_REG_RD(ASYNCLISTADDR) == 0)
    {
        // set the async base address
        USB_REG_WR(ASYNCLISTADDR, (NvU32)pNQh->pHcQh);
        // If the list is empty, Next Link pointer points to itself and set type as QH
        pNQh->pHcQh->QhHLinkPtr = (NvU32)((NvU32)(pNQh->pHcQh) | USB_HQH_DRF_DEF(QH_ITD_TYPE, QH));
        // make this node as the head of the reclamation list
        pNQh->pHcQh->EpCapChar0 |= USB_HQH_DRF_DEF(HEAD_RECLAMATION_LIST, SET);
        // Store  first Qh in pAsyncListHead
        pUsbHcdCtxt->pAsyncListHead = pNQh;
    }
    else
    {
        // Add the new QH at the end of the first QH, no matter how many QHs exist
        // in the ASYNC list, coz' the list is CIRCULAR
        tempHcQh = (UsbhQh*)(USB_REG_RD(ASYNCLISTADDR));
        pNQh->pHcQh->QhHLinkPtr = tempHcQh->QhHLinkPtr;
        tempHcQh->QhHLinkPtr = ((NvU32)(pNQh->pHcQh)) | USB_HQH_DRF_DEF(QH_ITD_TYPE, QH);
    }
}

void UsbhPrivDeleteFromAsyncList(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdQh *pDHcdQh)
{
    UsbhQh *pTempHQh, *pDelHQh = pDHcdQh->pHcQh;

    // loop until required qh found or until end of the list
    for (pTempHQh = pUsbHcdCtxt->pAsyncListHead->pHcQh;
            (pTempHQh->QhHLinkPtr != (NvU32)pDelHQh) && (pTempHQh->QhHLinkPtr !=
            (NvU32)pUsbHcdCtxt->pAsyncListHead->pHcQh); pTempHQh = (UsbhQh *)pTempHQh->QhHLinkPtr);

    if (pTempHQh->QhHLinkPtr == (NvU32)pDelHQh)
    {
        // it means there is only one qh in the list
        if (pDelHQh == pUsbHcdCtxt->pAsyncListHead->pHcQh)
        {
            // Set NULL to  Async list head
            pUsbHcdCtxt->pAsyncListHead = NULL;
            // Set the async base address as NULL
            USB_REG_WR(ASYNCLISTADDR, 0);
        }
        else
        {
            // update  tempqh's link pointer with del qh link pointer
            pTempHQh->QhHLinkPtr = pDelHQh->QhHLinkPtr;
            // If del Qh is having reclamtion bit set, then set H bit of next
            // QH pDelHQh
            // make this node as the head of the reclamation list
            if (USB_HQH_DRF_VAL(HEAD_RECLAMATION_LIST, pDelHQh->EpCapChar0))
            {
                ((UsbhQh *)pDelHQh->QhHLinkPtr)->EpCapChar0 |= USB_HQH_DRF_DEF(HEAD_RECLAMATION_LIST, SET);
            }
        }
        // Then  clear all fields of delHcdQh
        pDHcdQh->Pipe = 0;
        pDHcdQh->Used = 0;
        pDHcdQh->Td_Head = 0;
    }
}

static NvError UsbhPrivAllocateHcdTD(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdTd **pNewTD)
{
    NvU32 i = 0;
    for (; i < USB_MAX_DTDS; i++)
    {
        if (pUsbHcdCtxt->pUsbHcdTdAddr[i].Used == 0)
        {
            *pNewTD = &pUsbHcdCtxt->pUsbHcdTdAddr[i];

            NV_DEBUG_PRINTF(("USBHDDK::ALLOC_HCDTD::HcdTxt 0x%x:: NewTD = 0x%x\n", pUsbHcdCtxt, *pNewTD));
            // Memset  with 0
            NvOsMemset((void*)(*pNewTD)->pHcTd, 0, sizeof(UsbhTd));
            // Set Terminate bit 1
            (*pNewTD)->pHcTd->NextDTDPtr = USB_HTD_DRF_DEF(QTD_TERMINATE, INVALID_TD_PTR);

            (*pNewTD)->pHcTd->AltNextDTDPtr = USB_HTD_DRF_DEF(ALT_QTD_TERMINATE, INVALID_TD_PTR);

            pUsbHcdCtxt->pUsbHcdTdAddr[i].Used = 1;
            break;
        }
    }
    if (i >= USB_MAX_DTDS)
        return NvSuccess;  // TODO: This return value need to be defined.
    return NvSuccess;
}

static void UsbhFillHcdTD(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdTd *pHcdTD, NvDdkUsbhTransfer*  pTransfer, NvU32 Status)
{
    pHcdTD->pHcTd->DtdToken |= Status;
    pHcdTD->pTransfer = pTransfer;
    pHcdTD->pNxtHcdTd = NULL;

    // checks for 3 transaction errors and when the counter value
    // counts from one to zero, queue head is halted and   USBERR INT is asserted
    if (pUsbHcdCtxt->UsbPortSpeed == NvDdkUsbPortSpeedType_High)
    {
        pHcdTD->pHcTd->DtdToken |= USB_HTD_DRF_NUM(ERR_COUNTER, 0);
    }
    else
    {
        pHcdTD->pHcTd->DtdToken |= USB_HTD_DRF_NUM(ERR_COUNTER, 3);
    }
}

static void UsbhPrivLinkTds(UsbHcdTd *pCurrentTD, UsbHcdTd *pNewHcdTD)
{
    // Set current Td's next Td as new one and set terminate bit 0
    pCurrentTD->pHcTd->NextDTDPtr = ((NvU32)pNewHcdTD->pHcTd);
    // TODO: To we need to set ALTdtdPtr also ????
    pCurrentTD->pNxtHcdTd = pNewHcdTD;
}

static void UsbhPrivLinkTdsToQh(NvDdkUsbhHandle pUsbHcdCtxt, UsbHcdQh *pHcdQh, UsbHcdTd *pHcdTdListHead)
{
    UsbhTd  *pCurrentTd;
    // Stop executing the Async List before modifying the list
    UsbhPrivDisableAsyncShedule(pUsbHcdCtxt);
    // Current QTD pointer points to the next QTD Pointer  and should mask
    // Terminate bit
    pCurrentTd = (UsbhTd*)((pHcdQh->pHcQh->NextDTDPtr) & (~0x7));

    // Check if this is the first QTD being entered in the List.If not traverse till the end
    if (pCurrentTd != NULL)
    {
        // Loop till your nextQTD points to the Reserved QTD in the list.
        while ((UsbhTd *)((pCurrentTd->NextDTDPtr) & (~0x7)) != NULL)
        {
            // Move to the next QTD in the list
            pCurrentTd = (UsbhTd *)((pCurrentTd->NextDTDPtr) & (~0x7));
        }
    }

    /*
     * Incase current Td is NULL, It represents no out standing TD's present in
     * QUEUE so link directly to QH, if not add new TD's to the  end of the Td_list
     * caclulated.
     */

    if (pCurrentTd == NULL)
    {
        // Make the Next QTD pointer field of the QH now point to this QTD
        // And  set Terminate bit 0
        pHcdQh->pHcQh->NextDTDPtr = (NvU32)(pHcdTdListHead->pHcTd);

        // TODO: Do we require to set Alt Next DTD of qh also to new td??
        // If it is the first TD for this Qh, Store this as TD head
        pHcdQh->Td_Head = pHcdTdListHead;
    }
    else
    {
        // Insert this new QTD just after the one which was previously pointing
        // And Set TERMINATE bit of currentTD to  0
        pCurrentTd->NextDTDPtr = (NvU32)(pHcdTdListHead->pHcTd);
        // TODO: Do we require to set Alt Next DTD of qh also to new td??
    }

    // Add pTransfer pointer to the Transfer list maintained by the Hcd structure
    UsbhPrivAddTransferToList(pUsbHcdCtxt, pHcdTdListHead->pTransfer);
    UsbhPrivEnableAsyncShedule(pUsbHcdCtxt);
}

static int
UsbhPrivAllocateFillBufferForTD(
        NvDdkUsbhHandle pUsbHcdCtxt,
        UsbHcdTd *pHcdTD,
        NvU8 *givenBuffer,
        NvU32 bufferLengh)
{
    NvU32  BuffersRequired;
    NvU32 i = 0, j = 0, continousFreeBuffers = 0;
    NvU8 *tempBuf;
    NvU32 count = 0;

    // if buffer length is >  16K, then alocate 16K only
    if (bufferLengh > USB_MAX_DTD_BUF_SIZE)
        bufferLengh = USB_MAX_DTD_BUF_SIZE;

    BuffersRequired = ((bufferLengh -1) / USB_MAX_MEM_BUF_SIZE) + 1;
    // Need all consecutive buffers required
    for (i = 0; ((i < USB_MAX_MEM_BUF_COUNT) && (continousFreeBuffers < BuffersRequired)); i++)
    {
        if (pUsbHcdCtxt->pUsbXferBufIndex[i] == 0)
        {
            if (continousFreeBuffers == 0)
                j = i;
            // Check this is contigious or not
            if (i == (j+continousFreeBuffers))
            {
                continousFreeBuffers++;
            }
            else
            {
                j = i;
                continousFreeBuffers = 1;
            }
        }
    }

    // If we get required number of continous buffers, store buffer address and return success.

    if (continousFreeBuffers == BuffersRequired)
    {
        pHcdTD->pBuffer = &(pUsbHcdCtxt->pUsbXferBuf[USB_MAX_MEM_BUF_SIZE * j]);
        // Change the buffer indexes to 1(used)
        NvOsMemset((void*)&pUsbHcdCtxt->pUsbXferBufIndex[j], 1, continousFreeBuffers);
    }
 /* else
    {
    // return NvError;
    // TODO: Need to define proper return value here and change the Func
    }*/

    tempBuf = pHcdTD->pBuffer;
    // total 4 buffer pointers for TD
    for (i = 0; ((i < 4) && (count < bufferLengh)); i++)
    {
        pHcdTD->pHcTd->BufferPtrs[i] = (NvU32)tempBuf;
        tempBuf += USB_MAX_MEM_BUF_SIZE;
        if ((count + USB_MAX_MEM_BUF_SIZE) < bufferLengh)
            count += USB_MAX_MEM_BUF_SIZE;
        else
            count = bufferLengh;
    }

    // IF this pipe PID is OUT / SETUP pkt, then copy the buffer given by the client to actual  tranfer buffer
    if (USB_HTD_DRF_VAL(PID_CODE, pHcdTD->pHcTd->DtdToken) !=
            USB_DRF_DEF_VAL(HOST_QUEUE_TRANSFER_DESCRIPTOR, PID_CODE, IN))
    {
        NvOsMemcpy((void *)pHcdTD->pBuffer, (void *)givenBuffer, bufferLengh);
        NvOsFlushWriteCombineBuffer();
    }


    pHcdTD->bytesToTransfer = count;
    return count;
}

void UsbhPrivAsyncSubmitTransfer(NvDdkUsbhHandle pUsbHcdCtxt, NvDdkUsbhTransfer* pTransfer, NvU32 pipe)
{
    UsbHcdTd *Td_Head = 0, *pNewHcdTD = 0, *pPrevHcdTD = 0;
    NvU32 tranferLength = pTransfer->TransferBufferLength;
    NvU8 *buf = pTransfer->pTransferBuffer;
    UsbHcdQh *pHcdQh = (UsbHcdQh*)pTransfer->EpPipeHandler;
    NvU32 bufferLenghPerTD = 0;
    NvU32 direction = 0;

    NV_DEBUG_PRINTF(("USBHDDK::AsyncMake:::PTRANSFER = 0x%x::pipe = 0x%x::pipetype = %d\n",
                     pTransfer,
                     pipe, USB_PIPETYPE(pipe)));

    if (USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Control)
    {
        // Allocate TD  for setup pkt
        UsbhPrivAllocateHcdTD(pUsbHcdCtxt, &pNewHcdTD);

        // Fill Td_Head of HcdQh with setup td
        Td_Head = pNewHcdTD;
        // Fill TD with required perameters
        UsbhFillHcdTD(pUsbHcdCtxt, pNewHcdTD, pTransfer, 0x80);
        bufferLenghPerTD = NVDDK_USBH_SETUP_PKT_SIZE; /* sizeof setup pkt */

        // Fill the setup pid code and Data toggle bit as 0
        pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, SETUP) |
                USB_HTD_DRF_DEF(DATA_TOGGLE, DATA0));

        // Setup pkt size always  8 bytes, so Transfer bytes to Transfer is 8
        pNewHcdTD->pHcTd->DtdToken |=
            USB_HTD_DRF_NUM(TOTAL_BYTES, bufferLenghPerTD);

        // Allocate  and fill the buffer as per requirement of this TD
        UsbhPrivAllocateFillBufferForTD(pUsbHcdCtxt, pNewHcdTD, pTransfer->SetupPacket, bufferLenghPerTD);
    }
    // Making of Read TD's  for  Data Stage
    while (tranferLength > 0)
    {
        pPrevHcdTD = pNewHcdTD;
        UsbhPrivAllocateHcdTD(pUsbHcdCtxt, &pNewHcdTD);

        if ((USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Bulk) && (!Td_Head))
        {
            // Fill Td_Head of HcdQh with setup td
            Td_Head = pNewHcdTD;
        }

        if (USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Bulk)
        {
            // Fill the pid code and Data toggle bit as 1
            if (IS_PIPEIN(pHcdQh->Pipe))
            {
                pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, IN) | USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));
                direction = USB_HTD_DRF_DEF_VAL(PID_CODE, IN);
            }
            else
            {
                // Incase if it is OUT
                pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, OUT)|USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));

                direction = USB_HTD_DRF_DEF_VAL(PID_CODE, OUT);
            }
        }
        else if (USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Control)
        {
            if (pTransfer->SetupPacket[0] & 0x80)
            {
                pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, IN) | USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));
                direction = USB_HTD_DRF_DEF_VAL(PID_CODE, IN);
            }
            else
            {
                // Incase if it is OUT
                pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, OUT)|USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));

                direction = USB_HTD_DRF_DEF_VAL(PID_CODE, OUT);
            }
        }

        UsbhFillHcdTD(pUsbHcdCtxt, pNewHcdTD, pTransfer, 0x80);
        // Allocate buffer as per requirement of this TD
        bufferLenghPerTD = UsbhPrivAllocateFillBufferForTD(pUsbHcdCtxt, pNewHcdTD, buf, tranferLength);

        // Total tranfer size for this TD
        pNewHcdTD->pHcTd->DtdToken |= USB_HTD_DRF_NUM(TOTAL_BYTES, bufferLenghPerTD);

        if (pPrevHcdTD)
            UsbhPrivLinkTds(pPrevHcdTD, pNewHcdTD);

        buf += bufferLenghPerTD;
        tranferLength -= bufferLenghPerTD;
    }

    // Add status TD only for Control PIPE
    if (USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Control)
    {
        // Make a  TD for setup stage
        pPrevHcdTD = pNewHcdTD;

        // Allocate TD for status tarnfer
        UsbhPrivAllocateHcdTD(pUsbHcdCtxt, &pNewHcdTD);
        UsbhFillHcdTD(pUsbHcdCtxt, pNewHcdTD, pTransfer, 0x80);
        /* Total tanfer length = 0: PID taken as PID OUT || | data toggle =
           for zero length DATA stages, STATUS is always IN */
        if (pTransfer->TransferBufferLength == 0)
        {
            pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, IN) |USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));
        }
        else if (direction == USB_HTD_DRF_DEF_VAL(PID_CODE, IN))
        {
            pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, OUT) |USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));
        }
        else
        {
            pNewHcdTD->pHcTd->DtdToken |= (USB_HTD_DRF_DEF(PID_CODE, IN) |USB_HTD_DRF_DEF(DATA_TOGGLE, DATA1));
        }

        // Buffer size will be Zero, PID value depends on wheather
        // data stage is there or not and in case if it is present
        // PID of status is depends on direction of last transation
        // Link Td
        UsbhPrivLinkTds(pPrevHcdTD, pNewHcdTD);
    }
    // set IOC  for last TD.
    pNewHcdTD->pHcTd->DtdToken |= USB_HTD_DRF_DEF(IOC, ENABLE);

    // Link  TD Head to the Async list header
    if ((USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Control) || (USB_PIPETYPE(pipe) == NvDdkUsbhPipeType_Bulk))
    {
        // NvDdkUsbhPipeType_Control and NvDdkUsbhPipeType_Bulk cases only supported now.
        UsbhPrivLinkTdsToQh(pUsbHcdCtxt, pHcdQh, Td_Head);
    }
}

NvError UsbhPrivRunController(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US*2;
    NvU32 regValue;

    // Start up controller.
    USB_REG_UPDATE_DEF(USBCMD, RS, RUN);
    do {
        // wait till Controller starts running.
        regValue = USB_REG_READ_VAL(USBCMD, RS);
        if (!TimeOut)
        {
            NV_DEBUG_PRINTF(("UsbhPrivRunController:: Controller START failed val = %d\n", regValue));
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue != USB_DRF_DEF_VAL(USBCMD, RS, RUN));

    while (!USB_REG_READ_VAL(PORTSC1, CCS))
    {
        NvOsSleepMS(1);
    };
    pUsbHcdCtxt->UsbhCableConnected = NV_TRUE;
    /* This 100 msec debunce interval is provided to ensure ensure that the electrical and mechanical
       connection is stable before software attempts to reset the attached device.
       This is as per USB Spec. page number 150 */
    NvOsSleepMS(100);

    if (pUsbHcdCtxt->UsbhCableConnected)
    {
        // reset the port
        USB_REG_UPDATE_DEF(PORTSC1, PR, USB_RESET);
        NvOsSleepMS(56);
        // Wait until port reset completes
        while (USB_REG_READ_VAL(PORTSC1, PR));
    }
    // Get port speed
    pUsbHcdCtxt->UsbhGetPortSpeed(pUsbHcdCtxt);

    return NvSuccess;
}

NvError UsbhPrivStopController(NvDdkUsbhHandle pUsbHcdCtxt)
{
    // STOP the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, STOP);

    /* Wait untill HC halted */
    while (!USB_REG_READ_VAL(USBSTS, HCH))
    {
        NvOsWaitUS(1);
    }
    return NvSuccess;
}

void UsbhPrivSuspendPort(NvDdkUsbhHandle pUsbHcdCtxt, NvBool enable)
{
    if (enable)
    {
        USB_REG_UPDATE_DEF(PORTSC1, SUSP, NOT_SUSPEND);
    }
    else
    {
        USB_REG_UPDATE_DEF(PORTSC1, SUSP, SUSPEND);
        // Wait until port suspend completes
        while (!USB_REG_READ_VAL(PORTSC1, SUSP))
        {
            NvOsWaitUS(1);
        };
    }
}

void
UsbhPrivConfigVbusInterrupt(
        NvDdkUsbhHandle pUsbHcdCtxt,
        NvBool Enable)
{
    NvDdkUsbPhyIoctl_VBusInterruptInputArgs Vbus;

    Vbus.EnableVBusInterrupt = Enable;

    NV_ASSERT_SUCCESS(
            NvDdkUsbPhyIoctl(pUsbHcdCtxt->hUsbPhy,
                NvDdkUsbPhyIoctlType_VBusInterrupt,
                &Vbus, NULL));
}


NvBool UsbhPrivIsIdPinSetToLow(NvDdkUsbhHandle pUsbHcdCtxt)
{
    NvDdkUsbPhyIoctl_IdPinStatusOutputArgs IdStatus;

    NV_ASSERT_SUCCESS(
            NvDdkUsbPhyIoctl(pUsbHcdCtxt->hUsbPhy,
            NvDdkUsbPhyIoctlType_IdPinStatus,
            NULL,
            &IdStatus));

    return IdStatus.IdPinSetToLow;
}

NvError
UsbhPrivEnableVbus(NvDdkUsbhHandle pUsbhHandle, NvU32 Instance)
{
    const NvOdmGpioPinInfo *pUsbhGpioPinInfo  = NULL;
    NvU32 GpioPinCount = 0;
    NvError e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(NvRmGpioOpen(pUsbhHandle->hRmDevice,
                   &(pUsbhHandle->hUsbHcdGpioHandle)));
        pUsbhGpioPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Usb,
                                        Instance,
                                        &GpioPinCount);
    NV_CHECK_ERROR_CLEANUP(NvRmGpioAcquirePinHandle(
            pUsbhHandle->hUsbHcdGpioHandle,
            pUsbhGpioPinInfo[Instance].Port,
            pUsbhGpioPinInfo[Instance].Pin,
            &pUsbhHandle->CableIdGpioPin));
    NV_CHECK_ERROR_CLEANUP(NvRmGpioConfigPins
           (pUsbhHandle->hUsbHcdGpioHandle,
             &pUsbhHandle->CableIdGpioPin,
                1, NvRmGpioPinMode_InputData));

fail:
return e;
}

