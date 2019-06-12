/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "arusb_otg.h"
#include "nvboot_error.h"
#include "nvboot_usbf_int.h"
#include "nvboot_clocks_int.h"
#ifdef TARGET_SOC_AP20
#include "nvboot_usbf_local.h"
#else
#include "nvboot_usbf_hw.h"
#endif
#include "nvboot_reset_int.h"
#include "nvboot_clocks.h"
#include "nvboot_hardware_access_int.h"
#include "nvboot_util_int.h"
#include "nvrm_drf.h"
#include "arclk_rst.h"
#include "tio_local.h"
#include "project.h"
#include "nvboot_bit.h"

#undef USB_REG_RD

#define USB_REG_RD(reg)\
    NV_READ32(NV_ADDRESS_MAP_USB_BASE + USB2_CONTROLLER_USB2D_##reg##_0)

#undef USB_REG_WR
#define USB_REG_WR(reg, data)\
    NV_WRITE32((NV_ADDRESS_MAP_USB_BASE + USB2_CONTROLLER_USB2D_##reg##_0), data)

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################


#define NV_TIO_USB_RC_BUFFER_IDX  0
#define NV_TIO_USB_TX_BUFFER_IDX  1
#define NV_TIO_USB_EP0_BUFFER_IDX  2

#define NV_TIO_WAIT_AFTER_CLOSE   1000000 // 1 second

#define NV_TIO_USB_TARGET_DEBUG 0

#if NV_TIO_USB_TARGET_DEBUG
#define DBUI(x) NvOsDebugPrintf x
#else
#define DBUI(x)
#endif

// defined in TEGRA_TOP/microboot/nvboot/t124/usbf/nvboot_usbf_common.c
extern NvBootUsbfContext *s_pUsbfCtxt;

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

typedef enum NvTrAp15UsbReadStateEnum
{
    NvTrAp15UsbReadState_idle,
    NvTrAp15UsbReadState_pending,
} NvTrAp15UsbReadState;

typedef struct NvTrAp15UsbStateRec
{
    NvU8  isInit;
    NvU8  isCableConnected;
    NvU8  isOpen;
    NvU8  isSmallSegment;   // first seg. is always small, last one may be small

    NvU32 bufferIndex;      // index of USB receive buffer
    NvU32 inReadBuffer;     // data already in receive buffer
    NvU8* inReadBufferStart;// pointer to start of those data

    NvTrAp15UsbReadState readState;
} NvTrAp15UsbState;

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

/* Read and write must use separate buffers */
__align(4096) NvU8 gs_readBufferMemory[NV_TIO_USB_MAX_SEGMENT_SIZE];
__align(4096) NvU8 gs_writeBufferMemory[NV_TIO_USB_MAX_SEGMENT_SIZE];
//
// ep0 buffer's size must equal NVBOOT_USB_MAX_TXFR_SIZE_BYTES
// defined in microboot/nvboot/t124/include/t124/nvboot_usbf_int.h
//
__align(4096) NvU8 gs_ep0BufferMemory[NVBOOT_USB_MAX_TXFR_SIZE_BYTES];

/* Pointers to a pair of 4k aligned buffers */
static NvU8* gs_buffer[3] = {
    &gs_readBufferMemory[0],
    &gs_writeBufferMemory[0],
    &gs_ep0BufferMemory[0]
};

// Notice that this sets isInit, isCableConnected, and all other state to zero.
static NvTrAp15UsbState gs_UsbState = { 0 };

// See if the header is "small enough". If not, the variable holding the header
// should not be allocated on stack (e.g. declare it static).
//NV_CT_ASSERT(sizeof(NvTioUsbMsgHeader)<=8);

//###########################################################################
//############################### CODE ######################################
//###########################################################################


/**
* @result NV_TRUE The USB is set up correctly and is ready to
* transmit and receive messages.
* @param pUsbBuffer pointer to a memory buffer into which data to the host
*        will be stored. This buffer must be 4K bytes in IRAM and must be
*        aligned to 4K bytes. Otherwise it will assert.
* @result NV_FALSE The USB failed to set up correctly.
*/
static NvBool NvTrAp15UsbSetup(NvU8 *pUsbBuffer)
{
    int timeout;

    if (!gs_UsbState.isInit)
    {
        // Initialize the USB controller
        NvBootUsbfInit();
        gs_UsbState.isInit = 1;
    }

    for (timeout=50; timeout; timeout--)        // 20 is too short time
    {
        // In T148, VBUS sensor was removed from usb controller, so we
        // cannot use NvBootUsbfIsCableConnected for cable detection.
        // Instead, assume cable is connected and do enumeration.
#ifndef TARGET_SOC_T148
        // Look for cable connection
        if (NvBootUsbfIsCableConnected())
#endif
        {
            // Start enumeration
            if (NvBootUsbfStartEnumeration(pUsbBuffer) == NvBootError_Success)
            {
                // Success fully enumerated
                return NV_TRUE;
            }
            else
            {
                // Failed to enumerate.
                return NV_FALSE;
            }
        }
        NvOsWaitUS(1000);
    }

    // Cable not connected
    return NV_FALSE;
}

static NvError NvTrAp15UsbWriteSegment(
                                       const void *ptr,
                                       NvU32 payload
#if NV_TIO_USB_USE_HEADERS
                                       ,NvTioUsbMsgHeader* msgHeader
#endif
                                       )
{
    NvU8* dst = gs_buffer[NV_TIO_USB_TX_BUFFER_IDX];
    NvU32 bytes2Send;
    NvBootUsbfEpStatus EpStatus;

    NvBootUsbfHandlePendingControlTransfers(
        gs_buffer[NV_TIO_USB_EP0_BUFFER_IDX]);
#if NV_TIO_USB_USE_HEADERS
    bytes2Send = payload <= NV_TIO_USB_SMALL_SEGMENT_PAYLOAD
                                        ? NV_TIO_USB_SMALL_SEGMENT_SIZE
                                        : NV_TIO_USB_LARGE_SEGMENT_SIZE;
    msgHeader->payload = payload;
    NvOsMemcpy(dst, msgHeader, sizeof(NvTioUsbMsgHeader));
    dst += sizeof(NvTioUsbMsgHeader);
#else
    bytes2Send = payload;
    if (bytes2Send>NV_TIO_USB_MAX_SEGMENT_SIZE)
        return DBERR(NvError_InsufficientMemory);
#endif

    NvOsMemcpy(dst, ptr, payload);
    NvBootUsbfTransmitStart(gs_buffer[NV_TIO_USB_TX_BUFFER_IDX], bytes2Send);
    do {
        EpStatus = NvBootUsbfQueryEpStatus(NvBootUsbfEndPoint_BulkIn);
        DBUI(("EpStatus = 0x%08x after NvBootUsbfTransmitStart\n", (NvU32)EpStatus));
        if(EpStatus != NvBootUsbfEpStatus_TxfrComplete)
        {
            NvBootUsbfHandlePendingControlTransfers(
                gs_buffer[NV_TIO_USB_EP0_BUFFER_IDX]);
        }
    } while( (EpStatus == NvBootUsbfEpStatus_TxfrActive) ||
             (EpStatus == NvBootUsbfEpStatus_Stalled) );

    if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
    {
        if (NvBootUsbfGetBytesTransmitted() != bytes2Send)
            return DBERR(NvError_FileWriteFailed);
    }
    else
    {
        // In T148, VBUS sensor was removed from usb controller, so we
        // cannot use NvBootUsbfIsCableConnected for cable detection.
        // Instead, assume cable is connected.
#ifndef TARGET_SOC_T148
        // Look for cable connection
        if (NvBootUsbfIsCableConnected())
#endif
        {
            // if cable is present then Stall out endpoint saying host
            // not send any more data to us.
            NvBootUsbfEpSetStalledState(NvBootUsbfEndPoint_BulkOut, NV_TRUE);
            return DBERR(NvError_UsbfEpStalled);
        }
        gs_UsbState.isCableConnected = 0;
        return DBERR(NvError_UsbfCableDisConnected);
    }

    return NvSuccess;
}

#if NV_TIO_USB_USE_HEADERS
static void NvTrAp15SendDebugMessage(char* msg)
{
    NvTioUsbMsgHeader msgHeader;

    NvOsMemset(&msgHeader, 0, sizeof(NvTioUsbMsgHeader));
    msgHeader.isDebug = NV_TRUE;
    NvTrAp15UsbWriteSegment(msg, NvOsStrlen(msg)+1, &msgHeader);
}
#endif

static NvError NvTrAp15UsbReadSegment(
                                      NvU32* buf_count,
                                      NvU32  timeout_msec)
{
    NvBootUsbfEpStatus EpStatus;
    NvU32 endTime = timeout_msec + NvOsGetTimeMS();
    NvU32 curTime;
    NvBootError err;

    err = NvBootUsbfHandlePendingControlTransfers(
            gs_buffer[NV_TIO_USB_EP0_BUFFER_IDX]);

    if(err) {
        *buf_count = 0;
        DBUI(("Error (0x%08x) handling control request\n", err));
        return DBERR(NvError_UsbfTxfrFail);
    }

    if (gs_UsbState.readState == NvTrAp15UsbReadState_idle)
    {
        DBUI(("NvTrAp15UsbReadSegment: new read, %d bytes\n", *buf_count));
        NvBootUsbfReceiveStart(gs_buffer[NV_TIO_USB_RC_BUFFER_IDX], *buf_count);
    }
    else
    {
        DBUI(("NvTrAp15UsbReadSegment: read pending\n"));
    }

    *buf_count = 0;
    gs_UsbState.readState = NvTrAp15UsbReadState_pending;

    do
    {
        DBUI(("NvTrAp15UsbReadSegment: looping\n"));

        EpStatus = NvBootUsbfQueryEpStatus(NvBootUsbfEndPoint_BulkOut);

        DBUI(("EpStatus = 0x%08x after NvBootUsbfRecieveStart\n", (NvU32)EpStatus));
        if (EpStatus != NvBootUsbfEpStatus_TxfrComplete)
        {

            err = NvBootUsbfHandlePendingControlTransfers(
                    gs_buffer[NV_TIO_USB_EP0_BUFFER_IDX]);
            if(err) {
                DBUI(("Error (0x%08x) handling control request\n", err));
                return DBERR(NvError_UsbfTxfrFail);
            }

            NvBootUtilWaitUS(1);

            // Timeout immediately
            if (!timeout_msec)
            {
                DBUI(("NvTrAp15UsbReadSegment: timeout immediately\n"));
                return DBERR(NvError_Timeout);
            }

            // Timeout never
            if (timeout_msec == NV_WAIT_INFINITE)
                continue;

            curTime = NvOsGetTimeMS();
            if (curTime > endTime)
                return DBERR(NvError_Timeout);
        }
    } while( (EpStatus == NvBootUsbfEpStatus_TxfrActive) ||
             (EpStatus == NvBootUsbfEpStatus_Stalled) );

    gs_UsbState.readState = NvTrAp15UsbReadState_idle;

    if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
    {
        *buf_count = NvBootUsbfGetBytesReceived();
        DBUI(("NvTrAp15UsbReadSegment: %d bytes read\n", *buf_count));
    }
    else
    {
        if (EpStatus == NvBootUsbfEpStatus_TxfrIdle)
            DBUI(("NvTrAp15UsbReadSegment: off loop idle\n"));
        else if (EpStatus == NvBootUsbfEpStatus_TxfrFail)
            DBUI(("NvTrAp15UsbReadSegment: off loop fail\n"));
        else if (EpStatus == NvBootUsbfEpStatus_Stalled)
            DBUI(("NvTrAp15UsbReadSegment: off loop stalled\n"));
        else if (EpStatus == NvBootUsbfEpStatus_NotConfigured)
            DBUI(("NvTrAp15UsbReadSegment: off loop not configured\n"));

        return DBERR(NvError_UsbfTxfrFail);
    }

    return NvSuccess;
}

//===========================================================================
// NvTrAp15UsbCheckPath() - check filename to see if it is a usb port
//===========================================================================
static NvError NvTrAp15UsbCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a usb port
    //
    if (!NvTioOsStrncmp(path, "usb:", 4))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "debug:", 6))
        return NvSuccess;
    return DBERR(NvError_BadValue);
}

//===========================================================================
// NvTrAp15UsbClose()
//===========================================================================
void NvBootUsbfClose(void)
{
    // Clear device address
    USB_REG_WR(PERIODICLISTBASE, USB_DEF_RESET_VAL(PERIODICLISTBASE));
    // Clear setup token, by reading and wrting back the same value
    USB_REG_WR(ENDPTSETUPSTAT, USB_REG_RD(ENDPTSETUPSTAT));
    // Clear endpoint complete status bits.by reading and writing back
    USB_REG_WR(ENDPTCOMPLETE, USB_REG_RD(ENDPTCOMPLETE));

    // STOP the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
    // Set the USB mode to the IDLE before reset
    USB_REG_UPDATE_DEF(USBMODE, CM, IDLE);

    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));

    // Reset the USB controller
    NvBootResetSetEnable(NvBootResetDeviceId_UsbId, NV_TRUE);
    NvBootResetSetEnable(NvBootResetDeviceId_UsbId, NV_FALSE);

    // Disable clock to the USB controller
    NvBootClocksSetEnable(NvBootClocksClockId_UsbId, NV_FALSE);

    // Stop PLLU clock
    NvBootClocksStopPll(NvBootClocksPllId_PllU);

    // Wait to provide time for the host to drop its connection.
    NvOsWaitUS(NV_TIO_WAIT_AFTER_CLOSE);

    // Enable NvBootUsbfInit to be called again
    s_pUsbfCtxt = 0;
}

static void NvTrAp15UsbClose(NvTioStream *stream)
{
    // TODO: cancel all pending transfers
    NvBootUsbfClose();

    gs_UsbState.isInit = 0;
    gs_UsbState.isCableConnected = 0;
    gs_UsbState.isOpen = 0;
    gs_UsbState.isSmallSegment = 0;
    gs_UsbState.bufferIndex = 0;
    gs_UsbState.inReadBuffer = 0;
    gs_UsbState.inReadBufferStart = 0;
    gs_UsbState.readState = 0;

    NvOsMemset (gs_buffer[0], 0, NV_TIO_USB_MAX_SEGMENT_SIZE);
    NvOsMemset (gs_buffer[1], 0, NV_TIO_USB_MAX_SEGMENT_SIZE);
    NvOsMemset (gs_ep0BufferMemory, 0, NVBOOT_USB_MAX_TXFR_SIZE_BYTES);

    // Wait for 1s to give the host a chance to remove the device before
    // subsequent USB operations.
    // do not use NvOsSleepMS(1000), that will cause AOS to suspend !!!
    NvOsWaitUS(NV_TIO_WAIT_AFTER_CLOSE);
}

//===========================================================================
// NvTrAp15UsbFopen()
//===========================================================================
static NvError NvTrAp15UsbFopen(
                    const char *path,
                    NvU32 flags,
                    NvTioStream *stream)
{
    gs_UsbState.bufferIndex = NV_TIO_USB_RC_BUFFER_IDX;

    // Check the cable connection for very first time
    if (!gs_UsbState.isCableConnected)
        gs_UsbState.isCableConnected =
            NvTrAp15UsbSetup(gs_buffer[gs_UsbState.bufferIndex]);

    if (!gs_UsbState.isCableConnected)
        return DBERR(NvError_UsbfCableDisConnected);

    gs_UsbState.isOpen = 1;
    gs_UsbState.inReadBuffer = 0;

    gs_UsbState.isSmallSegment = NV_TRUE;
    gs_UsbState.readState = NvTrAp15UsbReadState_idle;

    DBUI(("NvTrAp15UsbFopen\n"));

    return NvSuccess;
}

//===========================================================================
// NvTrAp15UsbFwrite()
//===========================================================================
static NvError NvTrAp15UsbFwrite(
                    NvTioStream *stream,
                    const void *buf,
                    size_t size)
{
    NvError result;
    NvU8* src = (NvU8*)buf;
#if NV_TIO_USB_USE_HEADERS
    NvU8 isSmallSegment = NV_TRUE;
    NvTioUsbMsgHeader msgHeader;

    NvOsMemset(&msgHeader, 0, sizeof(NvTioUsbMsgHeader));
#endif

    while (size)
    {
        NvU32 payload;

        DBUI(("NvTrAp15UsbFwrite: call NvTrAp15UsbWriteSegment\n\n"));
#if NV_TIO_USB_USE_HEADERS
        payload = isSmallSegment ? NV_TIO_USB_SMALL_SEGMENT_PAYLOAD
                                 : NV_TIO_USB_LARGE_SEGMENT_PAYLOAD;
        payload = NV_MIN(size, payload);

        msgHeader.isDebug = NV_FALSE;
        msgHeader.isNextSegmentSmall =
                (size - msgHeader.payload <= NV_TIO_USB_SMALL_SEGMENT_PAYLOAD);
        isSmallSegment = msgHeader.isNextSegmentSmall;
        result = NvTrAp15UsbWriteSegment(src, payload, &msgHeader);
#else
        payload = NV_MIN(size, NV_TIO_USB_MAX_SEGMENT_SIZE);
        result = NvTrAp15UsbWriteSegment(src, payload);
#endif

        if (result)
            return DBERR(result);
        src += payload;
        size -= payload;
    }

    DBUI(("NvTrAp15UsbFwrite: return success\n\n"));
    return NvSuccess;
}

//===========================================================================
// NvTrAp15UsbFread()
//===========================================================================
static NvError NvTrAp15UsbFread(
                    NvTioStream *stream,
                    void        *buf,
                    size_t       buf_count,
                    size_t      *recv_count,
                    NvU32        timeout_msec)
{
    NvError err;
    NvU8* dst = (NvU8*)buf;

    *recv_count = 0;

    while (buf_count)
    {
        NvU32 bytes2Receive;

        if (gs_UsbState.inReadBuffer)
        {
            NvU32 bytesToCopy = NV_MIN(buf_count, gs_UsbState.inReadBuffer);
            DBUI(("NvTrAp15UsbFread: copy from segment\n"));
            NvOsMemcpy(dst, gs_UsbState.inReadBufferStart, bytesToCopy);
            *recv_count += bytesToCopy;
            dst += bytesToCopy;
            buf_count -= bytesToCopy;
            gs_UsbState.inReadBufferStart += bytesToCopy;
            gs_UsbState.inReadBuffer -= bytesToCopy;
        }

        // FIXME - check logic; may need to indicate last segment in a message
        //  to avoid need to timeout, because NvTio tries to fill the buffer.
        //  Setting timeout to 0 if something was already received should do
        //  the trick
        if (!buf_count)
        {
            DBUI(("NvTrAp15UsbFread: copied, returning\n"));
            break;
        }
        if (*recv_count)
            timeout_msec = 0;

#if NV_TIO_USB_USE_HEADERS
        bytes2Receive = gs_UsbState.isSmallSegment
                            ? NV_TIO_USB_SMALL_SEGMENT_SIZE
                            : NV_TIO_USB_LARGE_SEGMENT_SIZE;
#else
        bytes2Receive = NV_TIO_USB_MAX_SEGMENT_SIZE;
#endif

        DBUI(("NvTrAp15UsbFread: segment read start\n"));
        err = NvTrAp15UsbReadSegment(&bytes2Receive,
                                     timeout_msec);
        switch (err)
        {
            case NvError_Timeout: DBUI(("NvError_Timeout\n")); break;
            case NvError_UsbfTxfrFail: DBUI(("NvError_UsbfTxfrFail\n")); break;
            case NvSuccess: DBUI(("NvSuccess\n")); break;
            default: DBUI(("UNKNOWN!\n")); break;
        }
        if (err)
        {
            if (err == NvError_Timeout && *recv_count)
                return NvSuccess;
            else
                return DBERR(err);
        }

        DBUI(("NvTrAp15UsbFread: setup new segment\n"));

#if NV_TIO_USB_USE_HEADERS
        {
            NvTioUsbMsgHeader* msgHeader;

            msgHeader = (NvTioUsbMsgHeader*)gs_buffer[NV_TIO_USB_RC_BUFFER_IDX];
            gs_UsbState.inReadBufferStart = gs_buffer[NV_TIO_USB_RC_BUFFER_IDX]
                                                    + sizeof(NvTioUsbMsgHeader);
            gs_UsbState.inReadBuffer = msgHeader->payload;
            gs_UsbState.isSmallSegment = msgHeader->isNextSegmentSmall;
        }
#else
        gs_UsbState.inReadBufferStart = gs_buffer[NV_TIO_USB_RC_BUFFER_IDX];
        gs_UsbState.inReadBuffer = bytes2Receive;
#endif
    }

    return NvSuccess;
}

//===========================================================================
// NvTioRegisterUsb()
//===========================================================================
void NvTioRegisterUsb(void)
{
    static NvTioStreamOps ops = {0};

    //defer the hardware init until the first fopen

    ops.sopName        = "Ap15_usb";
    ops.sopCheckPath   = NvTrAp15UsbCheckPath;
    ops.sopFopen       = NvTrAp15UsbFopen;
    ops.sopFread       = NvTrAp15UsbFread;
    ops.sopFwrite      = NvTrAp15UsbFwrite;
    ops.sopClose       = NvTrAp15UsbClose;

    NvTioRegisterOps(&ops);
}
