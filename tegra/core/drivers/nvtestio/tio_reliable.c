/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "nvos.h"
#include "nvassert.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

//
// NV_TIO_RELI_BUFFER_SIZE must be power of 2
// NV_TIO_RELI_WINDOW_SIZE must be power of 2
//
#define NV_TIO_RELI_BUFFER_SIZE         0x80
#define NV_TIO_RELI_BUFFER_MASK         (NV_TIO_RELI_BUFFER_SIZE-1)
#define NV_TIO_RELI_WINDOW_SIZE         (NV_TIO_RELI_BUFFER_SIZE-2)
#define NV_TIO_RELI_PKT_SIZE            20
#define NV_TIO_RELI_TIMEOUT_MS          4000
#define NV_TIO_RELI_RETRY_DELAY_MS      100
#define NV_TIO_RELI_CLOSE_TIMEOUT_MS    1000
#define NV_TIO_RELI_RESET_CNT           5
#define NV_TIO_RELI_READ_DELAY_MS       1

//
// use 1 byte indices fot txStart, txEnd, rxStart, etc
//
#define NV_TIO_RELI_WRAP_MASK           0xff


#define NV_TIO_RELI_ERR_TIMEOUT     -1
#define NV_TIO_RELI_ERR_PROTOCOL    -2
#define NV_TIO_RELI_ERR_COMM        -3
#define NV_TIO_RELI_ERR_EOF         -4

#define DUMP_RELI_STATE(re) \
    do { \
        DBD(("RELI:   txStart=%02x   rstCnt=%02x\n", \
            re->txStart, \
            re->txResetCnt)); \
        DBD(("RELI:   txPend =%02x   pendSz=%02x\n", \
            re->txPend, \
            (re->txEnd - re->txPend)&NV_TIO_RELI_WRAP_MASK)); \
        DBD(("RELI:   txEnd  =%02x   txSize=%02x\n", \
            re->txEnd, \
            (re->txEnd - re->txStart)&NV_TIO_RELI_WRAP_MASK)); \
        DBD(("RELI:   rxStart=%02x\n",re->rxStart)); \
        DBD(("RELI:   rxEnd  =%02x   rxSize=%02x\n", \
            re->rxEnd, \
            (re->rxEnd - re->rxStart)&NV_TIO_RELI_WRAP_MASK)); \
    } while(0)

//
// Special TOKEN bytes in stream
//
#define NV_TIO_RELI_TOK_HDR     0xe7
#define NV_TIO_RELI_TOK_EOF     0xe8
#define NV_TIO_RELI_TOK_UNR     0xe9
#define NV_TIO_RELI_TOK_ESC     0xea

#define NV_TIO_RELI_TOK_MIN     0xe7
#define NV_TIO_RELI_TOK_MAX     0xea

#define NV_TIO_RELI_TOK_XOR     0x20

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

typedef struct NvTioReliRec {
    NvTioStream     subStream;
    NvU32           txTotal;    // total data bytes sent
    NvU32           rxTotal;    // total data bytes received
    NvU8            rxEofCnt;   // consecutive EOF chars received
    NvU8            rxUnrCnt;   // consecutive End chars received
    NvU8            rxEof;      // other end is closing file
    NvU8            rxUnr;      // other end is changing to unreliable mode
    NvU8            txCnt;      // # of tx since last packet receive
    NvU8            txStart;    // first txBuf byte that is not yet acked
    NvU8            txEnd;      // byte following last valid byte in txBuf
    NvU8            txPend;     // next byte to send (assuming ack)
    NvU8            txResetCnt; // count for retry already sent packets
    NvU8            rxStart;    // first rxBuf byte that is not yet consumed
    NvU8            rxEnd;      // byte following last byte in rxBuf
    NvU8            txBuf[NV_TIO_RELI_BUFFER_SIZE];
    NvU8            rxBuf[NV_TIO_RELI_BUFFER_SIZE];
} NvTioReli;

//
// format of a data packet
//
// byte
// offset  
//   0       hdr = NV_TIO_RELI_TOK_HDR
//   1       len = number of data bytes in this packet (may be 0, never 0xff)
//   2       dst = offset of 1st byte in packed mod NV_TIO_RELI_BUFFER_SIZE
//   3       ack = offset of last byte received in other direction
//   4...    <len> data bytes
//   4+<len> sum = sum of all bytes except <hdr>
//
// NOTE: all data, len, dst, ack, and sum bytes with values between
//        NV_TIO_RELI_TOK_MIN and NV_TIO_RELI_TOK_MAX are escaped as
// the following sequence:
//          NV_TIO_RELI_TOK_ESC
//          (original-byte)^NV_TIO_RELI_TOK_XOR
//
// SPECIAL SEQUENCES:
//
// EndOfFile:
//      Sent as: 16 NV_TIO_RELI_TOK_EOF bytes
//      Received as: 3 consecutive NV_TIO_RELI_TOK_EOF bytes
//
// BecomeUnreliable:
//      Sent as: 16 NV_TIO_RELI_TOK_UNR bytes, then 1 CR
//      Received as: 3 consecutive NV_TIO_RELI_TOK_UNR bytes followed by all
//      following NV_TIO_RELI_TOK_UNR bytes up until (and including) the first
//      non-NV_TIO_RELI_TOK_UNR byte.
//
//

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioReliPutcRaw()
//===========================================================================
static NvError NvTioReliPutcRaw(int c, NvTioReli *re)
{
    NvU8 buf[1];
    buf[0] = (NvU8)c;
    return re->subStream.ops->sopFwrite(&re->subStream,buf,1);
}

//===========================================================================
// NvTioReliSendEof() - send EOF packet
//===========================================================================
static void NvTioReliSendEof(NvTioReli *re)
{
    int i;
    //
    // other end needs to receive 3.  Send 16 to be sure.
    //
    for (i=0; i<16; i++) {
        NvTioReliPutcRaw(NV_TIO_RELI_TOK_EOF, re);
    }
}

//===========================================================================
// NvTioReliGetcRaw()
//===========================================================================
// Return vals
//    >=0   valid char
//    -1 = NV_TIO_RELI_ERR_TIMEOUT
//    -2 = NV_TIO_RELI_ERR_PROTOCOL
//    -3 = NV_TIO_RELI_ERR_COMM
//    -4 = NV_TIO_RELI_ERR_EOF
static int NvTioReliGetcRaw(NvTioReli *re, NvU32 timeout_ms)
{ 
    NvU8 buf[1];
    size_t len;
    NvError err;
    if (re->rxEof)
        return NV_TIO_RELI_ERR_EOF;
    err = re->subStream.ops->sopFread(
                    &re->subStream,
                    buf,
                    1,
                    &len,
                    timeout_ms);
    if (err || len!=1) {
        if (err!=NvError_Timeout || timeout_ms!=0) {
            DBD(("RELI: Read Error %s %08x %s:%d\n",
                err==NvError_Timeout?"TIMEOUT":"",
                err,
                __FILE__,__LINE__));
        }
        return (err == NvError_Timeout)   ? NV_TIO_RELI_ERR_TIMEOUT :
               (err == NvError_EndOfFile) ? NV_TIO_RELI_ERR_EOF :
                                            NV_TIO_RELI_ERR_COMM;
    }
    if (buf[0] == NV_TIO_RELI_TOK_EOF) {
        if (++re->rxEofCnt >= 3) {
            re->rxEof = 1;
            return NV_TIO_RELI_ERR_EOF;
        }
        return NV_TIO_RELI_ERR_PROTOCOL;
    } else {
        re->rxEofCnt = 0;
    }
    if (buf[0] == NV_TIO_RELI_TOK_UNR) {
        if (++re->rxUnrCnt >= 3) {
            re->rxUnr = 1;
        }
        return NV_TIO_RELI_ERR_PROTOCOL;
    } else {
        re->rxUnrCnt = 0;
    }
    return (int)buf[0];
}

//===========================================================================
// NvTioReliGetc()
//===========================================================================
// Return vals
//    >=0   valid char
//    -1 = NV_TIO_RELI_ERR_TIMEOUT
//    -2 = NV_TIO_RELI_ERR_PROTOCOL
//    -3 = NV_TIO_RELI_ERR_COMM
//    -4 = NV_TIO_RELI_ERR_EOF
static int NvTioReliGetc(NvTioReli *re, NvU8 *sum)
{ 
    int c = NvTioReliGetcRaw(re, NV_TIO_RELI_TIMEOUT_MS);
    if (c<0)
        return c;
    if (c==NV_TIO_RELI_TOK_HDR) {
        DBD(("RELI: Protocol Error %s:%d\n",__FILE__,__LINE__));
        return NV_TIO_RELI_ERR_PROTOCOL;
    }
    if (c==NV_TIO_RELI_TOK_ESC) {
        c = NvTioReliGetcRaw(re, NV_TIO_RELI_TIMEOUT_MS);
        if (c<0)
            return c;
        c ^= NV_TIO_RELI_TOK_XOR;
        if (c<NV_TIO_RELI_TOK_MIN || c>NV_TIO_RELI_TOK_MAX) {
            DBD(("RELI: Protocol Error %s:%d\n",__FILE__,__LINE__));
            return NV_TIO_RELI_ERR_PROTOCOL;
        }
    }
    *sum += c;
    return c;
}

//===========================================================================
// NvTioReliPutc()
//===========================================================================
static NvError NvTioReliPutc(NvU8 c, NvTioReli *re, NvU8 *sum)
{
    NvError err;
    *sum += c;
    if (c>=NV_TIO_RELI_TOK_MIN && c<=NV_TIO_RELI_TOK_MAX) {
        err = NvTioReliPutcRaw(NV_TIO_RELI_TOK_ESC, re);
        if (err)
            return DBERR(err);
        c ^= NV_TIO_RELI_TOK_XOR;
    }
    err = NvTioReliPutcRaw(c, re);
    return DBERR(err);
}

//===========================================================================
// NvTioReliRecv() - receive 1 packet (not including 1st byte)
//===========================================================================
// Return vals
//    0  = success
//    -1 = NV_TIO_RELI_ERR_TIMEOUT  (ignored)
//    -3 = NV_TIO_RELI_ERR_PROTOCOL (ignored)
//    -3 = NV_TIO_RELI_ERR_COMM
//    -4 = NV_TIO_RELI_ERR_EOF
static int NvTioReliRecv(NvTioReli *re)
{
    NvU8 window, deltaWindow;
    NvU8 calcSum, dummy = 0;
    int rxLen;
    int rxOff;
    int rxSum;
    int txEnd;
    int c;
    calcSum = 0;
    rxLen = NvTioReliGetc(re, &calcSum);
    if (rxLen < 0) {
        DBD(("RELI: Error %d %s:%d\n",rxLen,__FILE__,__LINE__));
        return rxLen;
    }
    rxOff = NvTioReliGetc(re, &calcSum);
    if (rxOff < 0) {
        DBD(("RELI: Error %d %s:%d\n",rxOff,__FILE__,__LINE__));
        return rxOff;
    }
    txEnd = NvTioReliGetc(re, &calcSum);
    if (txEnd < 0) {
        DBD(("RELI: Error %d %s:%d\n",txEnd,__FILE__,__LINE__));
        return txEnd;
    }

    if (rxOff != re->rxEnd) {
        DBD(("RELI: Protocol Error %s:%d\n",__FILE__,__LINE__));
        return NV_TIO_RELI_ERR_PROTOCOL;
    }

    DBD(("RELI: recv 1 l=%02x o=%02x rx=%02x ...\n",
        (int)rxLen,
        (int)rxOff,
        (int)txEnd));

    //
    // The new rx window size must be < max window size
    //
    window = (re->rxEnd - re->rxStart) & NV_TIO_RELI_WRAP_MASK;
    if ((NvU32)window + (NvU32)rxLen > NV_TIO_RELI_WINDOW_SIZE) {
        DBD(("RELI: Protocol Error %s:%d\n",__FILE__,__LINE__));
        return NV_TIO_RELI_ERR_PROTOCOL;
    }

    re->rxTotal += rxLen;
    DBD(("RELIS:     recv   %08x    rxTotal=%08x txTotal=%08x\n",
        rxLen,
        re->rxTotal,
        re->txTotal));
        
    while(rxLen--) {
        c = NvTioReliGetc(re, &calcSum);
        if (c < 0) {
            if (DUMPON()) {
                DBD(("RELI: Error %d %s:%d\n",c,__FILE__,__LINE__));
                DUMP_RELI_STATE(re);
            }
            return c;
        }
        re->rxBuf[(rxOff++)&NV_TIO_RELI_BUFFER_MASK] = (NvU8)c;
    }
    rxOff &= NV_TIO_RELI_WRAP_MASK;

    rxSum = NvTioReliGetc(re, &dummy);
    if (rxSum != (int)calcSum) {
        if (DUMPON()) {
            DBD(("RELI: ... recv bad sum (rxSum=%02x calc=%02x)\n",
                (int)rxSum,
                (int)calcSum));
            DUMP_RELI_STATE(re);
        }
        return rxSum < 0 ? rxSum : NV_TIO_RELI_ERR_PROTOCOL;
    }

    if (DUMPON()) {
        DBD(("RELI: ... recv success rxOff=%02x\n",rxOff));
        DUMP_RELI_STATE(re);
    }

    //
    // Show what we got
    //
    if (DUMPON()) {
        if (re->rxEnd != (NvU8)rxOff) {
            NvU8 idx = re->rxEnd;
            DBD(("RELID: recv: "));
            for (idx = re->rxEnd; idx != rxOff;) {
                c = re->rxBuf[idx&NV_TIO_RELI_BUFFER_MASK];
                DBD((" %c ", (c>=' ' && c<0x7f)?c:'.'));
                idx = (idx + 1) & NV_TIO_RELI_WRAP_MASK;
            }
            DBD(("\nRELID:       "));
            for (idx = re->rxEnd; idx != rxOff;) {
                c = re->rxBuf[idx&NV_TIO_RELI_BUFFER_MASK];
                DBD(("%02x ", c));
                idx = (idx + 1) & NV_TIO_RELI_WRAP_MASK;
            }
            DBD(("\n"));
        }
    }

    //
    // Packet is confirmed good here - update rxEnd
    //
    re->rxEnd = (NvU8)rxOff;

    //
    // The change in window size must be <= orig window size
    //
    window      = (re->txEnd   - re->txStart) & NV_TIO_RELI_WRAP_MASK;
    deltaWindow = ((NvU8)txEnd - re->txStart) & NV_TIO_RELI_WRAP_MASK;
    if (deltaWindow <= window) {
        re->txStart = (NvU8)txEnd;
    }


    re->txCnt = 0;

    if (DUMPON()) {
        DUMP_RELI_STATE(re);
    }

    return 0;
}


//===========================================================================
// NvTioReliSend() - send 1 packet
//===========================================================================
static NvError NvTioReliSend(NvTioReli *re)
{
    NvError err;
    int txLen;
    NvU8 sum = 0;
    NvU8 dummy = 0;
    NvU8 txOff;

    txLen = (re->txEnd - re->txPend) & NV_TIO_RELI_WRAP_MASK;
    NV_ASSERT(txLen <= NV_TIO_RELI_WINDOW_SIZE);
    if (txLen > NV_TIO_RELI_PKT_SIZE)
        txLen = NV_TIO_RELI_PKT_SIZE;

    err = NvTioReliPutcRaw(NV_TIO_RELI_TOK_HDR, re);
    if (err)
        return DBERR(err);
    err = NvTioReliPutc(txLen, re, &sum);
    if (err)
        return DBERR(err);
    err = NvTioReliPutc(re->txPend, re, &sum);
    if (err)
        return DBERR(err);
    err = NvTioReliPutc(re->rxEnd, re, &sum);
    if (err)
        return DBERR(err);

    DBD(("RELI: send 1 l=%02x o=%02x rx=%02x (Data & sum follow)\n",
        (int)txLen,
        (int)re->txPend,
        (int)re->rxEnd));

    re->txTotal += txLen;
    DBD(("RELIS:     send   %08x    rxTotal=%08x txTotal=%08x\n",
        txLen,
        re->rxTotal,
        re->txTotal));
        

    txOff = re->txPend;
    while(txLen--) {
        int c = re->txBuf[(txOff++)&NV_TIO_RELI_BUFFER_MASK];
        err = NvTioReliPutc(c, re, &sum);
        if (err)
            return DBERR(err);
    }
    re->txPend = txOff;

    err = NvTioReliPutc(sum, re, &dummy);

    if (DUMPON()) {
        DBD(("RELI: ... sent (sum=%02x)\n",sum));
        DUMP_RELI_STATE(re);
    }

    return DBERR(err);
}

//===========================================================================
// NvTioReliRun() - receive packets, then send 1 packet
//===========================================================================
static NvError NvTioReliRun(NvTioReli *re, NvU32 timeout_ms)
{
    while(1) {
        int hdr;
        NvU8 oldTxStart = re->txStart;

        //
        // check for incoming packet
        //
        do {
            hdr = NvTioReliGetcRaw(re, timeout_ms);
            timeout_ms = 0;
        } while(hdr >= 0 && hdr != NV_TIO_RELI_TOK_HDR);
        if (hdr != NV_TIO_RELI_ERR_TIMEOUT) {
            if (hdr != NV_TIO_RELI_TOK_HDR) {
                if (hdr == NV_TIO_RELI_ERR_EOF)
                    return DBERR(NvError_EndOfFile);
                if (hdr == NV_TIO_RELI_ERR_PROTOCOL)
                    continue;
                return DBERR(NvError_FileOperationFailed);
            }
            hdr = NvTioReliRecv(re);
            if (hdr == NV_TIO_RELI_ERR_EOF)
                return DBERR(NvError_EndOfFile);
            if (hdr == NV_TIO_RELI_ERR_COMM)
                return DBERR(NvError_FileOperationFailed);
            continue;
        }

        if (re->txPend == re->txStart) {
            re->txResetCnt = 0;
        } else if (oldTxStart != re->txStart) {
            re->txResetCnt = 0;
        } else {
            re->txResetCnt++;
            if (re->txResetCnt >= NV_TIO_RELI_RESET_CNT) {
                NvU32 delta = (re->txPend - re->txStart) & NV_TIO_RELI_WRAP_MASK;
                re->txPend = re->txStart;
                re->txTotal -= delta;
                DBD(("RELIS:     RESEND %08x    rxTotal=%08x txTotal=%08x\n",
                    delta,
                    re->rxTotal,
                    re->txTotal));
            }
        }

        re->txCnt++;

        //
        // Send transmit packet
        //
        return NvTioReliSend(re);
    }
}

//===========================================================================
// NvTioReliFread()
//===========================================================================
static NvError NvTioReliFread(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_ms)
{
    NvError err;
    NvU8 *p = ptr;
    int idx;
    size_t len;
    NvTioReli *re = stream->f.reli;

    if (DUMPON()) {
        DBD(("RELI: NvTioReliFread(%02x bytes)\n",(int)size));
        DUMP_RELI_STATE(re);
    }

    //
    // get chars or timeout
    //
    if (re->rxEnd == re->rxStart) {
        err = NvTioReliRun(re, NV_TIO_RELI_READ_DELAY_MS);
        if (err)
            return DBERR(err);
        while (re->rxEnd == re->rxStart) {
            NvU32 delay = NV_TIO_RELI_READ_DELAY_MS;
            if (delay > timeout_ms) {
                delay = timeout_ms;
                if (!delay)
                    return NvError_Timeout;
            }
            DBD(("RELIS: fread sleep %dms %s:%d\n",
                delay,
                __FILE__,__LINE__));
            NvOsSleepMS(delay);
            if (timeout_ms != NV_WAIT_INFINITE)
                timeout_ms -= delay;
            err = NvTioReliRun(re, NV_TIO_RELI_READ_DELAY_MS);
            if (err)
                return DBERR(err);
        }
    }

    //
    // Copy to buffer
    //
    len = (re->rxEnd - re->rxStart) & NV_TIO_RELI_WRAP_MASK;
    NV_ASSERT(len <= NV_TIO_RELI_WINDOW_SIZE);
    size = size < len ? size : len;

    *bytes = size;

    idx = re->rxStart;
    while(size--)
        *(p++) = re->rxBuf[(idx++)&NV_TIO_RELI_BUFFER_MASK];
    re->rxStart = idx;

    if (DUMPON()) {
        DBD(("RELI: NvTioReliFread RETURNING: %02x bytes\n",*bytes));
        NV_TIO_DBR(99,ptr,*bytes);
        DUMP_RELI_STATE(re);
    }

    return NvSuccess;
}

//===========================================================================
// NvTioReliFwrite()
//===========================================================================
static NvError NvTioReliFwrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size)
{
    NvTioReli *re = stream->f.reli;
    NvError err;
    const NvU8 *p = ptr;

    if (DUMPON()) {
        DBD(("RELI: NvTioReliFwrite(%02x bytes)\n",size));
        NV_TIO_DBW(99,ptr,size);
        DUMP_RELI_STATE(re);
    }

    if (re->rxEof) {
        return DBERR(NvError_EndOfFile);
    }

    while (size) {
        NvU8 idx;
        NvU8 oldTxStart;
        size_t len = (size_t)((re->txEnd - re->txStart) &
                                    NV_TIO_RELI_WRAP_MASK);
        NV_ASSERT(len <= NV_TIO_RELI_WINDOW_SIZE);
        len = NV_TIO_RELI_WINDOW_SIZE - len;
        len = len < size ? len : size;
        size -= len;
        idx = re->txEnd;
        while (len--)
            re->txBuf[(idx++)&NV_TIO_RELI_BUFFER_MASK] = *(p++);
        re->txEnd = idx;

        oldTxStart = re->txStart;
        err = NvTioReliRun(re, NV_TIO_RELI_RETRY_DELAY_MS);
        if (err)
            return DBERR(err);

        if (size && oldTxStart == re->txStart) {
            DBD(("RELIS: fwrite sleep %dms %s:%d\n",
                NV_TIO_RELI_RETRY_DELAY_MS,
                __FILE__,__LINE__));
            NvOsSleepMS(NV_TIO_RELI_RETRY_DELAY_MS);
        }
    }
    if (DUMPON()) {
        DBD(("RELI: NvTioReliFwrite() RETURNING\n"));
        DUMP_RELI_STATE(re);
    }

    

    return NvSuccess;
}

//===========================================================================
// NvTioReliFflushTimeout()
//===========================================================================
static NvError NvTioReliFflushTimeout(
                        NvTioStreamHandle stream,
                        NvU32 timeout_ms)
{
    NvError err = NvSuccess;
    NvTioReli *re = stream->f.reli;

    if (DUMPON()) {
        DBD(("RELI: Flushing stream\n"));
        DUMP_RELI_STATE(re);
    }

    if (re->rxEof) {
        return DBERR(NvError_EndOfFile);
    }

    err = NvTioReliRun(re, NV_TIO_RELI_RETRY_DELAY_MS);
    while(re->txStart != re->txEnd) {
        NvU32 delay = NV_TIO_RELI_RETRY_DELAY_MS;
        if (delay > timeout_ms) {
            delay = timeout_ms;
            if (!delay)
                return NvError_Timeout;
        }
        DBD(("RELIS: fflush sleep %dms %s:%d\n",
            delay,
            __FILE__,__LINE__));
        NvOsSleepMS(delay);
        if (timeout_ms != NV_WAIT_INFINITE)
            timeout_ms -= delay;

        err = NvTioReliRun(re, NV_TIO_RELI_RETRY_DELAY_MS);
        if (err)
            return DBERR(err);
    }

    if (re->subStream.ops->sopFflush)
        err = re->subStream.ops->sopFflush(&re->subStream);

    if (DUMPON()) {
        DBD(("RELI: Flushed stream\n"));
        DUMP_RELI_STATE(re);
    }

    return DBERR(err);
}

//===========================================================================
// NvTioReliFflush()
//===========================================================================
static NvError NvTioReliFflush(NvTioStreamHandle stream)
{
    return NvTioReliFflushTimeout(stream, NV_WAIT_INFINITE);
}

#define NV_TIO_RELIABLE_POLL    0
#if NV_TIO_RELIABLE_POLL
//===========================================================================
// NvTioReliPollPrep()
//===========================================================================
static NvError NvTioReliPollPrep(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
}

//===========================================================================
// NvTioReliPollCheck()
//===========================================================================
static NvError NvTioReliPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
}
#endif

//===========================================================================
// NvTioReliClose()
//===========================================================================
static void NvTioReliClose(NvTioStreamHandle stream)
{
    NvTioReli *re = stream->f.reli;

    if (DUMPON()) {
        DBD(("RELI: Closing stream\n"));
        DUMP_RELI_STATE(re);
    }

    if (!re->rxEof)
        NvTioReliFflushTimeout(stream, NV_TIO_RELI_CLOSE_TIMEOUT_MS);
    if (!re->rxEof)
        NvTioReliSendEof(re);

    *stream = re->subStream;

    NvOsFree(re);
    if (stream->ops->sopClose)
        stream->ops->sopClose(stream);

    DBD(("RELI: Stream is now closed!\n"));
}

//===========================================================================
// NvTioReliConnect() - Begin a new connection
//===========================================================================
static NvError NvTioReliConnect(NvTioStreamHandle stream, NvU32 timeout_ms)
{
    NvTioReli *re = stream->f.reli;
    NvError err;

    re->rxEofCnt= 0;
    re->rxEof   = 0;
    re->txTotal = 0;
    re->rxTotal = 0;
    re->txCnt   = 0;
    re->txStart = 0;
    re->txPend  = 0;
    re->txEnd   = 1;
    re->txResetCnt = 0;
    re->rxStart = 0;
    re->rxEnd   = 0;
    re->txBuf[0] = '@';
    re->rxBuf[0] = 0;

    err = NvTioReliRun(re, NV_TIO_RELI_RETRY_DELAY_MS);
    if (err)
        return DBERR(err);

    //
    // Wait until we have received a packed and and ack for our first tx
    //
    while(re->rxBuf[0] != '@' || re->txStart == 0) {
        NvU32 delay = NV_TIO_RELI_RETRY_DELAY_MS;
        if (delay > timeout_ms) {
            delay = timeout_ms;
            if (!delay)
                return NvError_Timeout;
        }
        DBD(("RELIS: connect sleep %dms %s:%d\n",
            delay,
            __FILE__,__LINE__));
        NvOsSleepMS(delay);
        if (timeout_ms != NV_WAIT_INFINITE)
            timeout_ms -= delay;
        err = NvTioReliRun(re, NV_TIO_RELI_RETRY_DELAY_MS);
        if (err)
            return DBERR(err);

        // resend right away if no response (for faster connect)
        re->txTotal -= (re->txPend - re->txStart) & NV_TIO_RELI_WRAP_MASK;
        re->txPend = re->txStart;
    }

    //
    // ignore the initial '@' character
    //
    re->rxStart = 1;

    if (DUMPON()) {
        DBD(("RELIS: Reliable connection established\n"));
        DBD(("RELID: Reliable connection established\n"));
        DUMP_RELI_STATE(re);
    }

    return NvSuccess;
}

//===========================================================================
// NvTioMakeUnreliable() - make a reliable stream into a regular stream
//===========================================================================
NvError NvTioMakeUnreliable(NvTioStreamHandle stream)
{
    int i;
    NvTioReli *re;

    if (stream->magic != NV_TIO_STREAM_MAGIC)
        return DBERR(NvError_BadParameter);

    re = stream->f.reli;

    if (re->rxEof)
        return DBERR(NvError_EndOfFile);

    if (DUMPON()) {
        DBD(("RELI: Make stream unreliable\n"));
        DUMP_RELI_STATE(re);
    }

    NvTioReliFflushTimeout(stream, NV_WAIT_INFINITE);

    if (re->rxEof)
        return DBERR(NvError_EndOfFile);

    //
    // Indicate we want to become unreliable
    //
    for (i=0; i<8; i++)
        NvTioReliPutcRaw(NV_TIO_RELI_TOK_UNR, re);

    //
    // Wait for at least 1 NV_TIO_RELI_TOK_UNR
    //
    while(!re->rxUnrCnt) {
        int rv = NvTioReliGetcRaw(re, 1000);
        if (rv == NV_TIO_RELI_ERR_TIMEOUT)
            NvTioReliPutcRaw(NV_TIO_RELI_TOK_UNR, re);
        if (rv == NV_TIO_RELI_ERR_EOF)
            return DBERR(NvError_EndOfFile);
        if (rv == NV_TIO_RELI_ERR_COMM)
            return DBERR(NvError_FileOperationFailed);
    }

    //
    // send CR to indicate we are going to unreliable mode NOW
    //
    NvTioReliPutcRaw('\n', re);

    //
    // Wait for a non-NV_TIO_RELI_TOK_UNR char
    //
    while(1) {
        NvU8 buf[1];
        size_t len;
        NvError err = re->subStream.ops->sopFread(
                    &re->subStream,
                    buf,
                    1,
                    &len,
                    NV_WAIT_INFINITE);
        if (err)
            return DBERR(err);
        if (buf[0] != NV_TIO_RELI_TOK_UNR)
            break;
    }

    *stream = re->subStream;

    NvOsFree(re);
    DBD(("RELI: Stream is now unreliable!\n"));
    return NvSuccess;
}

//===========================================================================
// NvTioMakeReliable() - make a stream into a reliable stream
//===========================================================================
NvError NvTioMakeReliable(NvTioStreamHandle stream, NvU32 timeout_ms)
{
    static NvTioStreamOps reliableOps = {0};
    NvTioReli *re;
    NvError err;

    if (stream->magic != NV_TIO_STREAM_MAGIC)
        return DBERR(NvError_BadParameter);

    if (!stream->ops->sopFwrite || !stream->ops->sopFread)
        return DBERR(NvError_BadParameter);

    if (!reliableOps.sopFwrite) {
        reliableOps.sopName = "ReliableOps";
        reliableOps.sopClose = NvTioReliClose;
        reliableOps.sopFread = NvTioReliFread;
        reliableOps.sopFwrite = NvTioReliFwrite;
        reliableOps.sopFflush = NvTioReliFflush;
#if NV_TIO_RELIABLE_POLL
        reliableOps.sopPollPrep = NvTioReliPollPrep;
        reliableOps.sopPollCheck = NvTioReliPollCheck;
#endif
    }

    re = NvOsAlloc(sizeof(NvTioReli));
    if (!re)
        return DBERR(NvError_InsufficientMemory);

    re->subStream = *stream;
    stream->ops = &reliableOps;
    stream->f.reli = re;

    err = NvTioReliConnect(stream, timeout_ms);
    if (err) {
        *stream = re->subStream;
        NvOsFree(re);
        return DBERR(err);
    }

    return NvSuccess;
}

