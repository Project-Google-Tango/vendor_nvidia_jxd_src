/*
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include "tio_local.h"
#include "aruart.h"
#include "artimer.h"
#include "nvrm_drf.h"
#include "nvboot_bit.h"
#include "project.h"


//###########################################################################
//############################### DEFINES ###################################
//###########################################################################


//===========================================================================
// UART register access and defines
//===========================================================================
#define NV_TRN_UARTA_BASE   (NV_ADDRESS_MAP_APB_UARTA_BASE)

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################


//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioBootedFromUart() - true if booted from uart
//===========================================================================
static NvBool NvTioBootedFromUart(void)
{
    // NOTE: FIXME!
    // This does not work because the contents of the BIT get corrupted
    // somewhere within AOS or during scatter loading.
    // This is not a critical issue

    //volatile NvBootInfoTable *bit = (volatile NvBootInfoTable *)0x40000000;
    //return (bit->BootType == NvBootType_Uart);

    // Always return true for now until this is debugged.
    return NV_TRUE;
}

//===========================================================================
// NvTrWrite8() - write 8 bit register
//===========================================================================
static NV_INLINE void NvTrWrite8(NvTioStream *stream, NvU32 offset, NvU8 value)
{
    *(stream->f.regs + offset) = value;
}

//===========================================================================
// NvTrRead8() - read 8 bit register
//===========================================================================
static NV_INLINE NvU8 NvTrRead8(NvTioStream *stream, NvU32 offset)
{
    return *(stream->f.regs + offset);
}

static NV_INLINE NvU32 NvTrT1xxUartRecvStatus(NvTioStream *stream)
{
    NvU32 status = (NvU32) NvTrRead8(stream, UART_LSR_0);
    status &= ( UART_LSR_0_BRK_FIELD  | UART_LSR_0_FERR_FIELD |
                UART_LSR_0_PERR_FIELD | UART_LSR_0_OVRF_FIELD );
    return status;
}

//===========================================================================
// NvTrUartRxReady() - true if character has been received
//===========================================================================
static NV_INLINE NvU32 NvTrUartRxReady(NvTioStream *stream)
{
    NvU32 Status = (NvU32) NvTrRead8(stream, UART_LSR_0);
    return (NV_DRF_VAL(UART,LSR,RDR,Status) == UART_LSR_0_RDR_DATA_IN_FIFO);
}

//===========================================================================
// NvTrUartRx() - receive a character
//===========================================================================
static NV_INLINE NvU8 NvTrUartRx(NvTioStream *stream)
{
    return NvTrRead8(stream, UART_THR_DLAB_0_0);
}

//===========================================================================
// NvTrT1xxUartCheckPath() - check filename to see if it is a uart port
//===========================================================================
static NvError NvTrT1xxUartCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a uart port
    //
    if (!NvTioOsStrncmp(path, "uart:", 5))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "debug:", 6))
        return NvSuccess;
    return DBERR(NvError_BadValue);
}

//===========================================================================
// NvTrT1xxUartClose()
//===========================================================================
static void NvTrT1xxUartClose(NvTioStream *stream)
{
    // flush uart port
    while( NvTrUartRxReady(stream) &&
           !NvTrT1xxUartRecvStatus(stream) )
    {
        NvTrUartRx(stream);
    }

    NvTrWrite8( stream, UART_IIR_FCR_0,
                NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
                NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) |
                NV_DRF_DEF(UART, IIR_FCR, TX_CLR, CLEAR) );
}

//===========================================================================
// NvTrT1xxUartFopen()
//===========================================================================
static NvError NvTrT1xxUartFopen(const char *path,
                                 NvU32 flags,
                                 NvTioStream *stream)
{
    //
    // refer to NvBootUartInit in
    // //hw/ap_tlit1/drv/bootrom/nvboot/uart/nvboot_uart.c
    //

    stream->f.regs = (unsigned char volatile *)NV_TRN_UARTA_BASE;

#define LCR_8BIT_NOPAR_2STOP \
  ( NV_DRF_DEF(UART,LCR,WD_SIZE,WORD_LENGTH_8) | \
    NV_DRF_DEF(UART,LCR,PAR,NO_PARITY) | \
    NV_DRF_DEF(UART,LCR,STOP,ENABLE) )

    // Configure for 8 bit, no parity, 1 stop bit, this also remove the DLAB enable bit
    NvTrWrite8(stream, UART_LCR_0, LCR_8BIT_NOPAR_2STOP);

    //Clear all Rx and trasnsmit
    NvTrWrite8(stream, UART_IIR_FCR_0, 0x00);
    //clear all interrupt
    NvTrWrite8(stream, UART_IER_DLAB_0_0, 0x00);
    //  Setup UART FIFO - Enable FIFO Mode
    NvTrWrite8( stream, UART_IIR_FCR_0,
                NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE));

    NvTrWrite8(stream, UART_IIR_FCR_0,
              NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
              NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) |
              NV_DRF_DEF(UART, IIR_FCR, TX_CLR, CLEAR) );
    //
    // Flush any old characters out of the FIFO
    //
    while( NvTrUartRxReady(stream)
          & !NvTrT1xxUartRecvStatus(stream) )
        (void)NvTrUartRx(stream);

    //  initialize Fifo Control Register.
    NvTrWrite8(stream, UART_IIR_FCR_0,
              NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
              NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) |
              NV_DRF_DEF(UART, IIR_FCR, TX_CLR, CLEAR) );

    return NvSuccess;
}

//===========================================================================
// NvTrT1xxUartFwrite()
//===========================================================================
static NvError NvTrT1xxUartFwrite(NvTioStream *stream,
                                  const void *ptr,
                                  size_t size)
{
    //
    // refer to NvBootUartPollingWrite in
    // //hw/ap_tlit1/drv/bootrom/nvboot/uart/nvboot_uart.c
    //
    size_t BytesLeft = size;
    NvU32 FifoSpace;
    NvU8  Status;
    NvU8 *pSrc = (NvU8*) ptr;

    if (size == 0)
    {
        return NvSuccess;
    }

    while (BytesLeft)
    {
        // Wait till transmitter empty.
        while (1)
        {
            Status = NvTrRead8(stream, UART_LSR_0);
            if (NV_DRF_VAL(UART,LSR,FIFOE,Status) == UART_LSR_0_FIFOE_ERR )
            {
                NvTrWrite8( stream, UART_IIR_FCR_0,
                           NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
                           NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) |
                           NV_DRF_DEF(UART, IIR_FCR, TX_CLR, CLEAR) );
                // error code not really important, placeholder
                return NvError_FileWriteFailed;
            }
            if ((NV_DRF_VAL(UART,LSR,TMTY,Status) == UART_LSR_0_TMTY_EMPTY) &&
                (NV_DRF_VAL(UART,LSR,THRE,Status) == UART_LSR_0_THRE_EMPTY) )
            {
                break;
            }
        }

        // Transmit up to 16 bytes
        FifoSpace = 16;
        while (BytesLeft && FifoSpace)
        {
            NvTrWrite8(stream, UART_THR_DLAB_0_0, *pSrc);
            pSrc++;
            BytesLeft--;
            FifoSpace--;
        } // while
    } // while

    while (1)
    {
        Status = NvTrRead8(stream, UART_LSR_0);
        if ( NV_DRF_VAL(UART,LSR,FIFOE,Status) == UART_LSR_0_FIFOE_ERR )
        {
            NvTrWrite8(stream, UART_IIR_FCR_0,
                      NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
                      NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) |
                      NV_DRF_DEF(UART, IIR_FCR, TX_CLR, CLEAR) );
            // error code not really important, placeholder
            return NvError_FileWriteFailed;
        }
        if ((NV_DRF_VAL(UART,LSR,TMTY,Status) == UART_LSR_0_TMTY_EMPTY) &&
            (NV_DRF_VAL(UART,LSR,THRE,Status) == UART_LSR_0_THRE_EMPTY) )
        {
            break;
        }
    }

    return NvSuccess;
}

//===========================================================================
// NvTrT1xxUartFread()
//===========================================================================
static NvError NvTrT1xxUartFread(NvTioStream *stream,
                                 void        *buf,
                                 size_t       buf_count,
                                 size_t      *recv_count,
                                 NvU32        timeout_msec)
{
    //
    // refer to NvBootUartPollingRead in
    // //hw/ap_tlit1/drv/bootrom/nvboot/uart/nvboot_uart.c
    //
    NvU8 *p   = buf;
    NvError err = NvSuccess;
    *recv_count = 0;

    //
    // Timeout immediately
    //
    if (!timeout_msec) {
        while(buf_count--) {
            if(NvTrT1xxUartRecvStatus(stream)) {
                err = NvError_FileReadFailed;
                goto done;
            }
            if ( !NvTrUartRxReady(stream) )
            {
                if (buf != p)
                    goto done;
                return DBERR(NvError_Timeout);
            }
            *(p++) = NvTrUartRx(stream);
        }

    //
    // Timeout never
    //
    } else if (timeout_msec == NV_WAIT_INFINITE) {
        while(buf_count--) {
            while(!NvTrUartRxReady(stream)) {
                if(NvTrT1xxUartRecvStatus(stream)) {
                    err = NvError_FileReadFailed;
                    goto done;
                }
                if (buf != p)
                    goto done;
            }
            *(p++) = NvTrUartRx(stream);
        }

    //
    // timeout_msec
    //
    } else {
        NvU32 t0 = NvOsGetTimeMS();
        NvU32 cnt=0;
        while(buf_count--) {
            while( !NvTrUartRxReady(stream) ) {
                if(NvTrT1xxUartRecvStatus(stream)) {
                    err = NvError_FileReadFailed;
                    goto done;
                }

                if (buf != p)
                    goto done;
                if (cnt++>100) {
                    NvU32 d = NvOsGetTimeMS() - t0;
                    if (d >= timeout_msec)
                        return DBERR(NvError_Timeout);
                    timeout_msec -= d;
                    t0 += d;
                    cnt = 0;
                }
            }
            *(p++) = NvTrUartRx(stream);
        }
    }

done:
    *recv_count = p - (NvU8*)buf;
    if (err == NvError_FileReadFailed)
    {
        // careful if modifying the error code
        NvTrWrite8( stream,  UART_IIR_FCR_0,
                    NV_DRF_DEF(UART, IIR_FCR, FCR_EN_FIFO, ENABLE) |
                    NV_DRF_DEF(UART, IIR_FCR, RX_CLR, CLEAR) );
    }
    return DBERR(err);
}

//===========================================================================
// NvTioRegisterUart()
//===========================================================================
void NvTioRegisterUart(void)
{
    static NvTioStreamOps ops = {0};

    ops.sopName        = "T1xx_uart";
    ops.sopCheckPath   = NvTrT1xxUartCheckPath;
    ops.sopFopen       = NvTrT1xxUartFopen;
    ops.sopFread       = NvTrT1xxUartFread;
    ops.sopFwrite      = NvTrT1xxUartFwrite;
    ops.sopClose       = NvTrT1xxUartClose;
    ops.sopNext        = NULL;

    if (NvTioBootedFromUart()) {
        NvTioRegisterOps(&ops);
    }
}
