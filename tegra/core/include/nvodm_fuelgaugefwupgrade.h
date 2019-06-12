/**
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVODM_FUELGAUGEFWUPGRADE_H
#define INCLUDED_NVODM_FUELGAUGEFWUPGRADE_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvos.h"

#define NvOdmFFU_BUFF_LEN                   65536
#define NvOdmFFU_MAX_DATA__LEN              96
#define NvOdmFFU_FLASH_RETRY_CNT            3
#define NvOdmFFU_LINE_DATALEN               128
#define NvOdmFFU_BUFF_LINELEN               512
#define NvOdmFFU_I2C_TRANSACTION_TIMEOUT    9000
#define NvOdmFFU_I2C_CLOCK_FREQ_KHZ         100

#define NvOdmFFUDEBUG 0

#define NvOdmFFU_NORMAL_I2C_ADDR       0xAA
#define NvOdmFFU_ROM_I2C_ADDR          0x16
#define NvOdmFFU_DELAY_4000MSEC        4000

/**
 * NvOdmFFUCommand: Structure to hold a I2C command.
 */
struct NvOdmFFUCommand
{
    NvU32 cmd;
    NvU32 device_address;
    NvU32 register_address;
    NvU8 data[NvOdmFFU_LINE_DATALEN];
    NvU32 data_len;
    NvU32 i2c_delay;
    struct NvOdmFFUCommand *pNext;
};

/**
 * NvOdmFFUBuff: Structure to hold the data received from 3p Server.
 */
struct NvOdmFFUBuff
{
    NvU8 data[NvOdmFFU_BUFF_LEN];
    NvU32 data_len;
    struct NvOdmFFUBuff *pNext;
};

/**
 * Enum to classify the I2C commands.
 */
enum {
    NvOdmFFU_READ = 0x1,
    NvOdmFFU_WRITE,
    NvOdmFFU_COMPARE,
    NvOdmFFU_DELAY,
    NvOdmFFU_UNUSED,
};

/**
 * NvOdmFFULine : Structure to hold a Line of data.
 */
struct NvOdmFFULine
{
    NvU8 data[NvOdmFFU_BUFF_LINELEN];
    NvU32 len;
};

/**
 * Receives the buffer from the 3pserver, parses it and executes the resulted
 * I2C commands
 *
 * @param hSock [In] A handle to the socket stream.
 * @param max_filelen1 [In] Length of file1.
 * @param max_filelen2 [In] Length of file2.
 * @retval NvSuccess
 */
NvError
NvOdmFFUMain(struct NvOdmFFUBuff *pHeadBuff1,
    struct NvOdmFFUBuff *pHeadBuff2);

#if defined(__cplusplus)
}
#endif

#endif    /* End of INCLUDED_NVODM_FUELGAUGEFWUPGRADE_H*/

