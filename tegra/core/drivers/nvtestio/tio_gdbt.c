/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_gdbt.h"
#include "nvassert.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioNibbleToAscii() - convert low 4 bits of nibble to ascii hex
//===========================================================================
char NvTioNibbleToAscii(NvU32 nibble)
{
    nibble &= 0xf;
    return (char)(nibble >= 10 ? nibble + (NvU32)'a' - 10 : nibble + (NvU32)'0');
}

//===========================================================================
// NvTioAsciiToNibble() - convert hex nibble to ascii
//===========================================================================
int NvTioAsciiToNibble(char c)
{
    if (c <  '0') return -1;
    if (c <= '9') return c-'0';
    if (c <  'A') return -1;
    if (c <= 'F') return c-'A'+10;
    if (c <  'a') return -1;
    if (c <= 'f') return c-'a'+10;
    return -1;
}

//===========================================================================
// NvTioHexToU32() - convert hex string to ascii
//===========================================================================
NvU32 NvTioHexToU32(const char *p, char **end, size_t maxlen)
{
    const char *e = maxlen > 0 ? p+maxlen : p;
    NvU32 val = 0;
    while(p != e) {
        int v = NvTioAsciiToNibble(*p);
        if (v<0)
            break;
        val <<= 4;
        val += v;
        p++;
    }
    if (end)
        *end = (char*)p;
    return val;
}

//===========================================================================
// NvTioHexToMem() - convert ascii hex to bytes in mem
//===========================================================================
NvError NvTioHexToMem(void *dst, const char *src, NvU32 dst_byte_cnt)
{
    NvU32 bad = 0;
    NvU32 val;
    volatile NvU8 *d = dst;

    while(dst_byte_cnt--) {
        val = (((NvU32)NvTioAsciiToNibble(src[0]))<<4) |
              (((NvU32)NvTioAsciiToNibble(src[1]))   );
        src += 2;
        bad |= val;
        *(d++) = (NvU8)(val & 0xff);
    }

    return (bad & ~0xffUL) ? NvError_BadValue : NvSuccess;
}

//===========================================================================
// NvTioHexToMemAlignedDestructive() - NvTioHexToMem() with aligned writes
//===========================================================================
//
// IMPORTANT NOTE: This function will destroy the data at src and the 3 bytes
// preceding src!
//
NvError NvTioHexToMemAlignedDestructive(
                    void *dst, 
                    char *src, 
                    NvU32 dst_byte_cnt)
{
    NvError err;
    NvUPtr align1 = (NvUPtr)dst_byte_cnt | (NvUPtr)dst;

    //
    // 32 bit writes
    //
    if ((align1 & 3) == 0) {
        NvU32 cnt4 = dst_byte_cnt>>2;
        volatile NvU32 *d = dst;
        NvU32 *s = (NvU32*)(((NvUPtr)src) & ~3UL);

        err = NvTioHexToMem(s, src, dst_byte_cnt);
        while(cnt4--) {
            *(d++) = *(s++);
        }

    //
    // 16 bit writes
    //
    } else if ((align1 & 1) == 0) {
        NvU32 cnt2 = dst_byte_cnt>>1;
        volatile NvU16 *d = dst;
        NvU16 *s = (NvU16*)(((NvUPtr)src) & ~1UL);

        err = NvTioHexToMem(s, src, dst_byte_cnt);
        while(cnt2--) {
            *(d++) = *(s++);
        }

    //
    // 8 bit reads
    //
    } else {
        err = NvTioHexToMem(dst, src, dst_byte_cnt);
    }
    return err;
}

//===========================================================================
// NvTioGdbtCmdBegin() - Begin a gdbtarget command
//===========================================================================
void NvTioGdbtCmdBegin(
                    NvTioGdbtCmd *cmd, 
                    void *buf, 
                    size_t len)
{
    NV_ASSERT(len>=4);
    cmd->buf    = buf;
    cmd->ptr    = cmd->buf;
    cmd->bufend = cmd->buf + len;
    *(cmd->ptr++) = '$';
    cmd->sum = 0;
}
                    
//===========================================================================
// NvTioGdbtCmdChar() - add character to gdbtarget command
//===========================================================================
void NvTioGdbtCmdChar(NvTioGdbtCmd *cmd, char c)
{
    if (cmd->bufend - cmd->ptr < 2)
        return;
    if (c==0 || c=='}' || c=='#' || c=='$' || c=='*') {
        cmd->sum     += '}';
        *(cmd->ptr++) = '}';
        c ^= 0x20;
    }

    cmd->sum     += c;
    *(cmd->ptr++) = c;
}
                    
//===========================================================================
// NvTioGdbtCmdString() - add a string to a command
//===========================================================================
void NvTioGdbtCmdString(NvTioGdbtCmd *cmd, const char *str)
{
    while(*str)
        NvTioGdbtCmdChar(cmd, *(str++));
}

//===========================================================================
// NvTioGdbtCmdData() - add a set of arbitrary data bytes to a command
//===========================================================================
void NvTioGdbtCmdData(NvTioGdbtCmd *cmd, const void *data, size_t cnt)
{
    const char *p = data;
    while (cnt--)
        NvTioGdbtCmdChar(cmd, *(p++));
}

//===========================================================================
// NvTioGdbtCmdHexData() - add set of arbitrary data bytes to a command in hex
//===========================================================================
void NvTioGdbtCmdHexData(NvTioGdbtCmd *cmd, const void *data, size_t cnt)
{
    const volatile NvU8 *src = data;
    NvU32 val;
    while (cnt--) {
        val = *(src++);
        NvTioGdbtCmdChar(cmd, NvTioNibbleToAscii(val>>4));
        NvTioGdbtCmdChar(cmd, NvTioNibbleToAscii(val   ));
    }
}

//===========================================================================
// NvTioGdbtCmdHexDataAligned() - NvTioGdbtCmdHexData() with aligned reads
//===========================================================================
void NvTioGdbtCmdHexDataAligned(NvTioGdbtCmd *cmd, const void *data, size_t cnt)
{
    NvUPtr align1 = (NvUPtr)cnt | (NvUPtr)data;

    //
    // out of room?  (error is detected later in NvTioGdbtCmdEnd())
    //
    if ((size_t)(cmd->bufend - cmd->ptr) < cnt*2 + 4) {
        cmd->ptr = cmd->bufend;
        return;
    }

    //
    // 32 bit reads
    //
    if ((align1 & 3) == 0) {
        size_t cnt4 = cnt>>2;
        const volatile NvU32 *src = data;
        NvU32 *dst = (NvU32*)(((NvUPtr)cmd->ptr + cnt + 3) & ~3UL);
        void  *data_copy = dst;
        while(cnt4--) {
            *(dst++) = *(src++);
        }
        NvTioGdbtCmdHexData(cmd, data_copy, cnt);

    //
    // 16 bit reads
    //
    } else if ((align1 & 1) == 0) {
        size_t cnt2 = cnt>>1;
        const volatile NvU16 *src = data;
        NvU16 *dst = (NvU16*)(((NvUPtr)cmd->ptr + cnt + 1) & ~1UL);
        void  *data_copy = dst;
        while(cnt2--) {
            *(dst++) = *(src++);
        }
        NvTioGdbtCmdHexData(cmd, data_copy, cnt);

    //
    // 8 bit reads
    //
    } else {
        NvTioGdbtCmdHexData(cmd, data, cnt);
    }
}

//===========================================================================
// NvTioGdbtCmdHexU32() - add a hex integer to a command
//===========================================================================
void NvTioGdbtCmdHexU32(NvTioGdbtCmd *cmd, NvU32 v, int mindigits)
{
    int s;
    mindigits = (mindigits > 0) ? mindigits * 4 : 4;
    for (s=28; s>=0; s-=4) {
        NvU32 n = (v >> s) & 0xf;
        if (n || s<mindigits) {
            mindigits = s;
            NvTioGdbtCmdChar(cmd, NvTioNibbleToAscii((NvU8)n));
        }
    }
}

//===========================================================================
// NvTioGdbtCmdHexU32Comma() - add a hex integer to a command followed by comma
//===========================================================================
void NvTioGdbtCmdHexU32Comma(NvTioGdbtCmd *cmd, NvU32 v, int mindigits)
{
    NvTioGdbtCmdHexU32(cmd, v, mindigits);
    NvTioGdbtCmdChar(cmd,',');
}

//===========================================================================
// NvTioGdbtCmdName64() - add a 64-bit packed ASCII name to a command
//===========================================================================
void NvTioGdbtCmdName64(NvTioGdbtCmd *cmd, NvU64 Name)
{
    /* The name is in the opposite endianness as character order, so reverse
     * the bytes on the way out.
     */
    int i;

    for (i = 0; i < 64/8; i++)
    {
        NvTioGdbtCmdChar(cmd, (char)((Name >> (64-8)) & 0xff));
        Name <<= 8;
    }
}


//===========================================================================
// NvTioGdbtGetChars() - get more characters in input buffer
//===========================================================================
NvError NvTioGdbtGetChars(NvTioRemote *remote, NvU32 timeout_msec)
{
    NvError err;
    size_t len = remote->bufLen - remote->cnt;
    NV_ASSERT(remote->bufLen >= remote->cnt);
    NV_ASSERT(remote->cmdLen == -1);    // should be no pending command
    if (len == 0)
        return DBERR(NvError_InsufficientMemory);
    err = NvTioFreadTimeout(
                    &remote->stream, 
                    remote->buf + remote->cnt, 
                    len,
                    &len, 
                    timeout_msec);
    if (!err)
        remote->cnt += len;
    return DBERR(err);
}

//===========================================================================
// NvTioGdbtEraseCommand() - erase command from buffer
//===========================================================================
void NvTioGdbtEraseCommand(NvTioRemote *remote)
{
    NV_ASSERT(remote->dst == -1);
    remote->cmdLen = -1;
}

//===========================================================================
// NvTioGdbtFlush() - flush input buffer
//===========================================================================
NvError NvTioGdbtFlush(NvTioRemote *remote)
{
    NvError err = NvSuccess;

    NV_ASSERT(remote->cmdLen == -1);    // should be no pending command

#if 1
    // TODO: use this once timeout_msec==0 is supported
    do {
        remote->src = remote->cnt = 0;
        err = NvTioGdbtGetChars(remote, 0);
    } while(!err);
    if (err == NvError_Timeout)
        err =  NvSuccess;
#endif

    remote->cmdLen = -1;
    remote->dst = -1;
    remote->src = remote->cnt = 0;
    return DBERR(err);
}

//===========================================================================
// NvTioGdbtAckWait() - wait for ack - returns FileOperationFailed for -
//===========================================================================
//
// Returns NvSuccess when an ack ('+' character) is received.
// Returns NvError_BadValue if - is received.
// Returns NvError_Timeout if timeout occurs.
// Returns other error if other error occurs.
//
NvError NvTioGdbtAckWait(NvTioRemote *remote, NvU32 timeout_msec)
{
    NvError err;
    int c;
    NV_ASSERT(remote->cmdLen == -1);    // should be no pending command
    for (;;) {
        if (remote->src == remote->cnt) {
            remote->src =  remote->cnt = 0;
            err = NvTioGdbtGetChars(remote, timeout_msec);
            if (err)
                return DBERR(err);
            NV_ASSERT(remote->src != remote->cnt);
        }
        c = remote->buf[remote->src++];
        switch (c) {
            case '+':
                return NvSuccess;
            case '-':
                return NvError_BadValue;
            case '#':
            case '$':
                return NvError_InvalidState;
            default:
                break;
        }
    }
}

//===========================================================================
// NvTioGdbtCmdEnd() - end a gdbtarget command
//===========================================================================
static NV_INLINE NvError NvTioGdbtCmdEnd(NvTioGdbtCmd *cmd)
{
    if (cmd->bufend - cmd->ptr < 3) {
        cmd->ptr = cmd->buf;
        return NvError_InsufficientMemory;
    }
    cmd->ptr[0] = '#';
    cmd->ptr[1] = NvTioNibbleToAscii(cmd->sum >> 4);
    cmd->ptr[2] = NvTioNibbleToAscii(cmd->sum     );
    cmd->ptr += 3;
    return NvSuccess;
}
                    
//===========================================================================
// NvTioGdbtCmdSend() - send a gdbtarget command and wait for ack
//===========================================================================
NvError NvTioGdbtCmdSend(NvTioRemote *remote, NvTioGdbtCmd *cmd)
{
    int retryCnt = 5;           // Number of times to try before failing
    int isRunCmd = 0;
    NvU32 timeout_msec = NV_TIO_INFINITE_TIMEOUTS ?
                                        NV_WAIT_INFINITE :
                                        NV_TIO_GDBT_TIMEOUT;
    NvError err;

    remote->isRunCmd = 0;
    err = NvTioGdbtCmdEnd(cmd);
    if (err)
        return DBERR(err);

    if (cmd->ptr - cmd->buf == 5 && cmd->buf[1]=='c')
        isRunCmd = 1;

    NV_ASSERT(remote->cmdLen == -1);
    if (remote->cmdLen != -1) {
        return DBERR(NvError_InvalidState);
    }
    do {
        err = NvTioGdbtFlush(remote);
        if (err)
            break;

        // send ack in case other end is stuck waiting on ack.
        NvTioFwrite(&remote->stream, "+", 1);

        err = NvTioFwrite(&remote->stream, cmd->buf, cmd->ptr - cmd->buf);
        if (err)
            break;
        err = NvTioGdbtAckWait(remote, timeout_msec);
    } while(err && retryCnt--);

    if (!err)
        remote->isRunCmd = isRunCmd;

    return DBERR(err);
}

//===========================================================================
// Nack() - debugging - count # nacks received
//===========================================================================
static void Nack(void)
{
    static volatile int foo = 1;
    foo++;
}

//===========================================================================
// NvTioGdbtCmdCheck() - Check for a complete command/reply in the buffer
//===========================================================================
//
// If a complete command/reply is in the buffer
//      - a + ack is sent
//      - the command will always begin at remote->buf[0]
//      - the command will always end   at remote->buf[remote->cmdLen-1]
//      - the command is 0 terminated  (remote->buf[remote->cmdLen] = 0)
//      - NvSuccess is returned
//
// If a bad or incomplete command is received:
//      - a - nack is sent
//      - NvError_BadValue is returned
//
// Returns NvError_Timeout if command is not yet complete.
//
NvError NvTioGdbtCmdCheck(NvTioRemote *remote)
{
    char *d;    // next byte in decoded command
    char *p;    // next byte from transport
    char *e;    // end of bytes from transport
    int   sum0;     // 1st char in received sumcheck
    int   sum1;     // 2nd char in received sumcheck
    int   c;        // the latest character
    
    if (remote->cmdLen >= 0)
        return NvSuccess;

    d = remote->buf + remote->dst;
    p = remote->buf + remote->src;
    e = remote->buf + remote->cnt;

    //
    // wait for start of a new command
    //
    if (remote->dst < 0) {
        for (;;) {
            if (p == e) {
                remote->src = remote->cnt = 0;
                return DBERR(NvError_Timeout);
            }
            c = *(p++);
            if (c == '$')
                break;
            if (c == '#') {
                remote->src = p - remote->buf;
                return DBERR(NvError_InvalidState);
            }
        }
        d = remote->buf;
        remote->sum = 0;
        remote->esc = 0;
    }


    //
    // read in command bytes and calculate checksum
    //
    for (;;) {
        if (p == e) {
            remote->cnt = remote->src = remote->dst = d - remote->buf;
            return DBERR(NvError_Timeout);
        }

        c = *(p++);
        if (c=='#')
            break;
        if (remote->esc) {
            //
            // if esc > 0 then this is an escape char (XOR with 0x20)
            //
            remote->sum += c;
            *(d++) = c ^ 0x20;
            remote->esc = 0;
            continue;
        }
        remote->sum += c;
        if (c=='}') {
            remote->esc = 1;
        } else {
            *(d++) = c;
        }
    }

    //
    // do we have both checksum chars in buffer yet?
    //
    if (e - p < 2) {
        remote->dst = (d - remote->buf);
        remote->src = (p - remote->buf) - 1;
        return DBERR(NvError_Timeout);
    }

    //
    // found end of command 
    //      - p points to first checksum byte
    //      - d points to end of message contents
    //
    *d = 0;
    remote->src = (p - remote->buf) + 2;
    
    sum0 = NvTioAsciiToNibble((NvU8)p[0]);
    sum1 = NvTioAsciiToNibble((NvU8)p[1]);
    if (((NvU32)sum0<<4) + ((NvU32)sum1) != (NvU32)remote->sum) {
        Nack();
        NvTioFwrite(&remote->stream, "-\r\n", 3);
        remote->dst = -1;
        return DBERR(NvError_BadValue);
    }

    NvTioFwrite(&remote->stream, "+\r\n", 3);
    remote->cmdLen = d - remote->buf;
    remote->dst = -1;
    return NvSuccess;
}

//===========================================================================
// NvTioGdbtCmdWait() - wait until a valid command or reply is in the buffer
//===========================================================================
//
// If <cmd> is non null it is sent first
//
// If a complete command is in the buffer
//      - a + ack is sent
//      - the command will always begin at remote->buf[0]
//      - the command will always end   at remote->buf[remote->cmdLen-1]
//      - the command is 0 terminated  (remote->buf[remote->cmdLen] = 0)
//      - NvSuccess is returned
//
// If a bad or incomplete command is received:
//      - a - nack is sent
//      - NvError_BadValue is returned
//
// Returns NvError_Timeout if command is not yet complete.
//
NvError NvTioGdbtCmdWait(
                NvTioRemote *remote, 
                NvTioGdbtCmd *cmd,
                NvU32 timeout_msec)
{
    NvError err;

    if (cmd && cmd->ptr != cmd->buf) {
        err = NvTioGdbtCmdSend(remote, cmd);
        if (err)
            return err;
        cmd->ptr = cmd->buf;
        if (remote->cmdLen >= 0)
            return NvSuccess;
    }

    for (;;) {
        err = NvTioGdbtCmdCheck(remote);
        if (err != NvError_Timeout)
            break;
        err = NvTioGdbtGetChars(remote, timeout_msec);
        if (DBERR(err))
            break;
    }
    return DBERR(err);
}

//===========================================================================
// NvTioGdbtCmdParse() - parse an incoming GdbTarget command or reply
//===========================================================================
//
// Parse a command or reply into parameters
//
// Initial letter goes into parms->cmd  (0 if 0-length command)
//
// parms->parmCnt = number of parameters
//
// Each parm has
//    parm->str  - null terminated parameter string (MAY CONTAIN 0 characters!)
//    parm->len  - number of chars in parm string (not counting NULL term)
//    parm->isHex - 1 if all chars are valid hex digits, 0 otherwise
//    parm->val   - hex value (only valid if isHex==1 and len<=8)
//
NvError NvTioGdbtCmdParse(NvTioGdbtParms *parms, char *cmd, size_t len)
{
    char *ce = cmd + len;
    NvTioGdbtParm *prev  = NULL;
    NvTioGdbtParm *p  = parms->parm;
    NvTioGdbtParm *pe = p + NV_TIO_MAX_PARMS;
    parms->parmCnt = 0;
    if (len < 1) {
        parms->cmd = 0;
        return NvSuccess;
    }
    parms->cmd = *cmd++;
    for (;;p++) {
        if (p == pe) {
            return NvError_InsufficientMemory;
        }
        p->str = cmd;
        if (p > parms->parm) {
            prev = p-1;
            if (prev->separator == ':') {
                p->isHex = 1;
                p->len = ce-cmd;
                break;
            }
        }
        p->val = NvTioHexToU32(cmd, &cmd, ce - cmd);
        p->isHex = (cmd != p->str);
        while (cmd != ce && *cmd!=',' && *cmd!=';' && *cmd!=':') {
            p->isHex = 0;
            cmd++;
        }
        p->len = cmd - p->str;
        if (cmd == ce)
            break;
        p->separator = *cmd;
        *cmd++ = 0;
    }
    NV_ASSERT(*ce == 0);
    p->separator = 0;
    parms->parmCnt = p - parms->parm + 1;
    return NvSuccess;
}


