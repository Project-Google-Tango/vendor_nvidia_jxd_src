#ifndef INCLUDED_TIO_GDBT_H
#define INCLUDED_TIO_GDBT_H
/*
 * Copyright (c) 2005-2007 NVIDIA Corporation.  All rights reserved.
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

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

//
// NV_TIO_GDBT_USE_O_REPLY - enable use of O gdbtarget command (faster stdout)
// NV_TIO_GDBT_O_REPLY_HEX - hex encode O command (required by gdbtarget
//                            standard, but faster when off)
//
#define NV_TIO_GDBT_USE_O_REPLY     1       // set to 1 for faster
#define NV_TIO_GDBT_O_REPLY_HEX     0       // set to 0 for faster

// the errno for file I/O: should we define it here or directly use system calls?
#define NV_TI_GDBT_ERRNO_EOF        "8"
#define NV_TI_GDBT_ERRNO_EBADF      "9"
#define NV_TI_GDBT_ERRNO_EUNKNOWN   "9999"

#define NV_TI_GDBT_SIGNAL_TRAP      0x5

// number of bytes of overhead sent with each data packet
#define NV_TIO_GDBT_MSG_OVERHEAD    6

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioGdbtCmd - outgoing gdb target command
//===========================================================================
typedef struct NvTioGdbtCmdRec {
    char    *buf;
    char    *bufend;
    char    *ptr;
    NvU8     sum;
} NvTioGdbtCmd;

//===========================================================================
// NvTioGdbtParm - incoming gdb target command/reply parameter
//===========================================================================
typedef struct NvTioGdbtParmRec {
    NvU32    val;
    char    *str;
    NvU32    len;
    int      isHex;
    int      separator;
} NvTioGdbtParm;

//===========================================================================
// NvTioGdbtParms - incoming gdb target command/reply
//===========================================================================
typedef struct NvTioGdbtParmsRec {
    int             cmd;
    NvU32           parmCnt;
    NvTioGdbtParm   parm[NV_TIO_MAX_PARMS];
} NvTioGdbtParms;

//###########################################################################
//############################### PROROTYPES ################################
//###########################################################################

/*-------------------------------------------------------------------------*/
/** NvTioNibbleToAscii() - convert low 4 bits of nibble to ascii hex
 *
 *  @param nibble - low 4 bits are used.  Upper 28 bits ignored.
 */
char    NvTioNibbleToAscii(NvU32 nibble);

/*-------------------------------------------------------------------------*/
/** NvTioAsciiToNibble() - convert hex char c to its value.
 *
 *  @param c - hex digit in ascii [0-9a-fA-F]
 *
 *  Returns -1 if c is not [0-9a-fA-F]
 */
int     NvTioAsciiToNibble(char c);

/*-------------------------------------------------------------------------*/
/** NvTioHexToU32() - convert ascii hex bytes to a U32 value
 *
 *  @param p       - pointer to start of bytes
 *  @param end    - if not NULL this will point to the first non-hex char
 *  @param maxlen - if not NULL this will point to the first non-hex char
 *
 *  value is returned
 *  If *p is not [0-9a-fA-F] then end is set to p and 0 is returned.
 */
NvU32   NvTioHexToU32(
                    const char *p, 
                    char **end, 
                    size_t maxlen);

/*-------------------------------------------------------------------------*/
/** NvTioHexToMem() - convert ascii hex to byte values
 *
 *  @param dst - the bytes are placed here
 *  @param src - the hex characters come from here
 *  @param dst_byte_cnt - number of bytes to write to dst
 *
 *  Returns NvError_BadValue if src is not ascii hex characters
 */
NvError NvTioHexToMem(
                    void *dst, 
                    const char *src, 
                    NvU32 dst_byte_cnt);

/*-------------------------------------------------------------------------*/
/** NvTioHexToMemAlignedDestructive() - convert ascii hex to byte values
 *
 *  IMPORTANT NOTE: bytes at src are scrambled!
 *  IMPORTANT NOTE: 3 bytes preceding src are scrambled!
 * 
 * Uses aligned writes to dst:
 *   - if dst and dst_byte_cnt are multiples of 4, 32 bit writes will be used
 *   - if dst and dst_byte_cnt are multiples of 2, 16 bit writes will be used
 *
 *  @param dst - the bytes are placed here
 *  @param src - the hex characters come from here
 *  @param dst_byte_cnt - number of bytes to write to dst
 *
 *  Returns NvError_BadValue if src is not ascii hex characters
 */
NvError NvTioHexToMemAlignedDestructive(
                    void *dst, 
                    char *src,          // NOTE: *src is clobbered!!!
                    NvU32 dst_byte_cnt);

void    NvTioGdbtCmdBegin(
                    NvTioGdbtCmd *cmd, 
                    void *buf, 
                    size_t len);
void    NvTioGdbtCmdChar(
                    NvTioGdbtCmd *cmd, 
                    char c);
void    NvTioGdbtCmdString(
                    NvTioGdbtCmd *cmd, 
                    const char *str);
void    NvTioGdbtCmdData(
                    NvTioGdbtCmd *cmd, 
                    const void *data, 
                    size_t cnt);
void    NvTioGdbtCmdHexData(
                    NvTioGdbtCmd *cmd, 
                    const void *data, 
                    size_t cnt);
void    NvTioGdbtCmdHexDataAligned(
                    NvTioGdbtCmd *cmd, 
                    const void *data, 
                    size_t cnt);
void    NvTioGdbtCmdHexU32(
                    NvTioGdbtCmd *cmd, 
                    NvU32 v, 
                    int mindigits);
void    NvTioGdbtCmdHexU32Comma(
                    NvTioGdbtCmd *cmd, 
                    NvU32 v, 
                    int mindigits);
void    NvTioGdbtCmdName64(
                    NvTioGdbtCmd *cmd,
                    NvU64 Name);
NvError NvTioGdbtCmdSend(
                    NvTioRemote *remote, 
                    NvTioGdbtCmd *cmd);

void    NvTioGdbtEraseCommand(
                    NvTioRemote *remote);
NvError NvTioGdbtFlush(
                    NvTioRemote *remote);
NvError NvTioGdbtAckWait(
                    NvTioRemote *remote, 
                    NvU32 timeout_msec);
NvError NvTioGdbtCmdCheck(
                    NvTioRemote *remote);
NvError NvTioGdbtGetChars(
                    NvTioRemote *remote, 
                    NvU32 timeout_msec);
NvError NvTioGdbtCmdWait(
                    NvTioRemote *remote, 
                    NvTioGdbtCmd *cmd,
                    NvU32 timeout_msec);
NvError NvTioGdbtCmdParse(
                    NvTioGdbtParms *parms, 
                    char *cmd, 
                    size_t len);

#endif // INCLUDED_TIO_GDBT_H
