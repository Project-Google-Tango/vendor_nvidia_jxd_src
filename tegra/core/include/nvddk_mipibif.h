/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Driver Development Kit: MIPI BIF Interface</b>
 */

/**
 * @defgroup nvddk_mipibif_group Battery Interface (BIF) Controller APIs
 *
 * The BIF standard is a low frequency serial interface standard applicable to
 * a mobile terminal (phones, tablets, other portable battery operated devices).
 * The BIF standard defines a serial protocol for communication with a Smart
 * Battery or Low Cost Battery as well as other batter- related devices in the
 * platform. The BIF protocol defines a basic method for battery identification
 * using a resistive ID as well as a method for batteries that contain a BIF
 * compliant controller that can support more advanced methods of authentication
 * and battery charge control. The BIF protocol requires the combination of
 * purely digital control to implement the communication protocol as well as
 * analog support for resistive battery identification and battery removal
 * detection.
 *
 * MIPI BIF is supported by the NVIDIA Driver Development Kit Mipibif Interface
 * (described next), which provides direct NvDDK function calls to initialize
 * BIF controller and process commands. The NvDDK interface is available in the
 * boot loader and as part of the main system image.
 *
 * <hr>
 * <a name="nvddk_if"></a><h2>NvDDK Mipibif Interface</h2>
 *
 * This topic includes the following sections:
 * - Limitations
 * - NvDdk Mipibif Library Usage
 * - Mipibif Test Code Example
 *
 * <h3>Limitations</h3>
 *
 * This software has been tested on NVIDIA&reg;Tegra&reg; ceres platforms
 * with origa2 Slave device.
 *
 * @note This software is currently under development, and this early release
 * is intended to enable customers early access to verify some basic
 * functionality of controller using origa2 Slave with the understanding that
 * the interface is subject to change and is not completed. Because of this,
 * it is important to verify the desired functionality<b>with a smart
 * battery pack</b>.
 *
 * <h3>NvDdk Library Usage</h3>
 *
 * - NvDdkMipiBifSendBRESCmd -> Bus command to reset all Slave devices.
 * - NvDdkMipiBifHardReset -> Command to do hardware reset of all Slave devices,
 *      including their volatile parameter registers.
 * - NvDdkMipiBifBusQuery -> Special bus command that allows the Host to
 *       simultaneously poll binary information from all Slave devices
 *       connected to the bus.
 * - NvDdkMipiBifProcessCmds -> Processes various commands like read, write,
 *      power down, activate, etc.
 * - NvDdkMipiBifInit -> Sets BIf clocks, FIFO control, and programs the timing
 *      registers.
 * - NvDdkWriteDataCommand -> Writes data to FIFO.
 * - NvDdkReadDataCommand -> Reads data from FIFO.
 *
 * <h3>Example BIF Controller Programming Code</h3>
 *
 * The following example shows one way to use BIF Programming APIs.
 *
 * @code
 * //
 * // BIF Controller Programming
 * //
 * #include "nvddk_mipibif.h"
 * #include "testsources.h"
 *
 * NvBool NvDdkMipiBifProcessCmds(MipiBifHandle *Handle)
 * {
 *     NvU32 SdaPart;
 *     NvU32 EraPart;
 *     NvU32 WraPart;
 *     NvU32 RraPart;
 *     NvU32 Rbe = MIPI_BIF_BUS_COMMAND_RBE0;
 *     NvU32 Rbl = MIPI_BIF_BUS_COMMAND_RBL0;
 *     NvU32 CtrlReg = 0;
 *
 *     NvU32 DefIntMask = MIPIBIF_BCL_ERROR_INTERRUPTS_EN |
 *          MIPIBIF_FIFO_ERROR_INTERRUPTS_EN | MIPIBIF_INTERRUPT_XFER_DONE_INT_EN;
 *     NvU32 IntMask = DefIntMask;
 *
 *     Handle->CmdCnt = 0;
 *     NvDdkMipiBifInit();
 *
 *     NvDdkMipiBifFlushFifos();
 *
 *     SdaPart = Handle->DevAddr & 0xFF;
 *
 *     EraPart = (Handle->RegAddr & 0xFF00) >> 8;
 *
 * Following restrictions right now to make driver simpler
 *  - No DASM
 *  - No AIO
 *  - No TQ
 *  - WRITE below implemented for single Slave, no AIO. Slave selected everytime
 *        by SDA.
 *  - Not sure if receiving large number of data has to be split into multiple
 *        transactions.
 *
 *      if (Handle->Cmds== MIPI_BIF_WRITE)
 *     {
 *         if (Handle->Len== 0 || (!Handle->Buf))
 *             //return -EINVAL;
 *         // Enable Interrupts
 *
 *         CtrlReg = MIPIBIF_CTRL_COMMAND_NO_READ;
 *         WraPart = Handle->RegAddr& 0xFF;
 *
 *         NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
 *         NvDdkMipiBifSend( EraPart | MIPI_BIF_ERA, Handle);
 *         NvDdkMipiBifSend( WraPart | MIPI_BIF_WRA, Handle);
 *
 *         CtrlReg |= ((Handle->Len + Handle->CmdCnt- 1) << MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
 *         NvDdkMipiBifFillTxFifo(Handle);
 *
 *         if (Handle->Len)
 *             IntMask |= MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN;
 *
 *         NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
 *         // NvOsDebugPrintf("\nwrite--> %0x\n",int_mask);
 *         CtrlReg |= MIPIBIF_CTRL_GO;
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         //NvOsDebugPrintf("\nwrite--> %0x\n",ctrl_reg);
 *         NvDdkMipiBifCommandComplete();
 *         NvDdkMipiBifMaskIrq(MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN);
 *     }
 *     else if (Handle->Cmds== MIPI_BIF_READDATA)
 *     {
 *         RraPart = Handle->RegAddr & 0xFF;
 *         Rbe |= ((Handle->Len & 0xFF00) >> 8);
 *         Rbl |= (Handle->Len & 0xFF);
 *
 *         if (Handle->Len == 256)
 *             Rbe= Rbl = 0;
 *
 *         IntMask |= MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN;
 *         CtrlReg |= MIPIBIF_CTRL_COMMAND_READ_DATA;
 *
 *         NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
 *         //NvAvpMipiBifSend( rbe | MIPI_BIF_BUS_COMMAND_RBE0);
 *         NvDdkMipiBifSend( Rbl | MIPI_BIF_BUS_COMMAND_RBL0, Handle);
 *         NvDdkMipiBifSend( EraPart | MIPI_BIF_ERA, Handle);
 *         NvDdkMipiBifSend( RraPart | MIPI_BIF_RRA, Handle);
 *         CtrlReg |= ((Handle->CmdCnt- 1)<< MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
 *         // Anything else to be accounted ??
 *         NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
 *         CtrlReg |= MIPIBIF_CTRL_GO;
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         NvDdkMipiBifCommandComplete();
 *         if (Handle->Len)
 *             NvDdkMipiBifEmptyRxFifo(Handle);
 *         NvDdkMipiBifMaskIrq(MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN);
 *     }
 *     else if (Handle->Cmds == MIPI_BIF_INT_READ)
 *     {
 *         NvU32 Val;
 *         NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
 *         NvDdkMipiBifSend( MIPI_BIF_BUS_COMMAND_EINT,Handle);
 *         CtrlReg = MIPIBIF_CTRL_COMMAND_INTERRUPT_DATA;
 *         CtrlReg |= ((Handle->CmdCnt - 1) << MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
 *         CtrlReg |= MIPIBIF_CTRL_GO;
 *
 *         NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         NvDdkMipiBifCommandComplete();
 *         NvOsWaitUS(5000);
 *         NV_MIPIBIF_READ(MIPIBIF_STATUS, Val);
 *         if(!(Val& MIPIBIF_INTERRUPT_RECV_STATUS))
 *         {
 *                 NvOsDebugPrintf("Interrupt not received\n");
 *                return NV_FALSE;
 *         }
 *         NvOsDebugPrintf("Interrupt  received\n");
 *     }
 *     else if (Handle->Cmds == MIPI_BIF_STDBY)
 *     {
 *         NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,MIPI_BIF_BUS_COMMAND_STBY);
 *         CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_STBY;
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         NvDdkMipiBifCommandComplete();
 *     }
 *     else if (Handle->Cmds == MIPI_BIF_ACTIVATE)
 *     {
 *         NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
 *         CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_ACTIVATE;
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         NvDdkMipiBifCommandComplete();
 *     }
 *     else if (Handle->Cmds == MIPI_BIF_PWRDOWN)
 *     {
 *         NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,MIPI_BIF_BUS_COMMAND_PWDN);
 *         CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_PWDN;
 *         NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
 *         NvDdkMipiBifCommandComplete();
 *     }
 *     else if (Handle->Cmds == MIPI_BIF_HARD_RESET)
 *     {
 *         NvDdkMipiBifHardReset();
 *     }
 *     return NV_TRUE;
 * }
 *
 * static NvBool NvDdkMipiBifWriteTest()
 * {
 *     NvBool Ret = NV_FALSE;
 *     MipiBifHandle Handle;
 *     static NvU8 rbuf[20];
 *     NvU8 wbuf[] = "abcdefgh";
 *
 *     Handle.DevAddr = 0x01;
 *     Handle.RegAddr = 0x0F09;
 *     Handle.Cmds = MIPI_BIF_WRITE;
 *     Handle.Buf = wbuf;
 *     Handle.Len = 8;
 *     Ret = NvDdkMipiBifProcessCmds(&Handle);
 *     NvOsDebugPrintf("Data written:%s\n", wbuf);
 *     Handle.Len = 8;
 *     Handle.Cmds = MIPI_BIF_READDATA;
 *     Handle.Buf = rbuf;
 *     Ret = NvDdkMipiBifProcessCmds(&Handle);
 *     NvOsDebugPrintf("Data read:%s\n", rbuf);
 *     return Ret;
 * }
 * @endcode
 *
 * @ingroup nvddk_modules
 * @{
 */

#ifndef _NVDDK_MIPIBIF_H
#define _NVDDK_MIPIBIF_H

#include "nverror.h"
#include "nvcommon.h"
#include "arclk_rst.h"

#define NV_ADDRESS_MAP_PPSB_CLK_RST_BASE     0x60006000
#define NV_ADDRESS_MAP_APB_MISC_BASE         0x70000000
#define NV_ADDRESS_MAP_MIPIBIF_BASE 0x70120000

#define TEGRA_MIPIBIF_TIMEOUT 1000

#define MIPIBIF_CTRL 0x0
#define MIPIBIF_CTRL_GO (1 << 31)
#define MIPIBIF_CTRL_COMMAND_TYPE_SHIFT 20
#define MIPIBIF_CTRL_COMMAND_NO_READ (0x0 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_READ_DATA (0x1 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_INTERRUPT_DATA (0x2 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_BUS_QUERY (0x3 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_STBY (0x4 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_PWDN (0x5 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_HARD_RESET (0x6 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_ACTIVATE (0x7 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_COMMAND_EXIT_INT_MODE (0x8 << MIPIBIF_CTRL_COMMAND_TYPE_SHIFT)
#define MIPIBIF_CTRL_PACKETCOUNT_SHIFT 8
#define MIPIBIF_CTRL_PACKETCOUNT_MASK (1FF << MIPIBIF_CTRL_PACKETCOUNT_SHIFT)

#define MIPIBIF_TIMING_PVT 0x4
#define MIPIBIF_TIMING_TBIF_0_SHIFT 16
#define MIPIBIF_TIMING_TBIF_1_SHIFT 12
#define MIPIBIF_TIMING_TBIF_STOP_SHIFT 8

#define MIPIBIF_TIMING0 0x8
#define MIPIBIF_TIMING0_TRESP_MIN_SHIFT 28
#define MIPIBIF_TIMING0_TRESP_MAX_SHIFT 20
#define MIPIBIF_TIMING0_TBIF_SHIFT 0

#define MIPIBIF_TIMING1 0xc
#define MIPIBIF_TIMING1_INT_TO_SHIFT 16
#define MIPIBIF_TIMING1_TINTARM_SHIFT 8
#define MIPIBIF_TIMING1_TINTTRA_SHIFT 4
#define MIPIBIF_TIMING1_TINTACT_SHIFT 0

#define MIPIBIF_TIMING2 0x10
#define MIPIBIF_TIMING2_TPDL_SHIFT 15
#define MIPIBIF_TIMING2_TPUL_SHIFT 0

#define MIPIBIF_TIMING3 0x14
#define MIPIBIF_TIMING3_TACT_SHIFT 20
#define MIPIBIF_TIMING3_TPUP_SHIFT 0

#define MIPIBIF_TIMING4 0x18
#define MIPIBIF_TIMING4_TCLWS_SHIFT 0

#define MIPIBIF_STATUS 0x2c
#define MIPIBIF_NUM_PACKETS_TRANSMITTED_SHIFT 16
#define MIPIBIF_NUM_PACKETS_TRANSMITTED_MASK (0x1FF << MIPIBIF_NUM_PACKETS_TRANSMITTED_SHIFT)
#define MIPIBIF_NUM_PACKETS_RCVD_SHIFT 4
#define MIPIBIF_NUM_PACKETS_RCVD_MASK (0x1FF << MIPIBIF_NUM_PACKETS_RCVD_SHIFT)
#define MIPIBIF_CTRL_BUSY (1<<2)
#define MIPIBIF_INTERRUPT_RECV_STATUS (1<<1)
#define MIPIBIF_BQ_RECV_STATUS (1<<0)

#define MIPIBIF_INTERRUPT_EN 0x30
#define MIPIBIF_INTERRUPT_RXF_UNR_OVR_INT_EN (1<<10)
#define MIPIBIF_INTERRUPT_TXF_UNR_OVR_INT_EN (1<<9)
#define MIPIBIF_INTERRUPT_NO_RESPONSE_ERR_INT_EN (1<<8)
#define MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN (1<<7)
#define MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN (1<<6)
#define MIPIBIF_INTERRUPT_XFER_DONE_INT_EN (1<<5)
#define MIPIBIF_INTERRUPT_INV_ERR_INT_EN (1<<4)
#define MIPIBIF_INTERRUPT_PKT_RECV_ERR_INT_EN (1<<3)
#define MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR_INT_EN (1<<2)
#define MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR_INT_EN (1<<1)
#define MIPIBIF_INTERRUPT_PARITY_ERR_INT_EN (1<<0)

#define MIPIBIF_BCL_ERROR_INTERRUPTS_EN (                                   \
    MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR_INT_EN                          \
   | MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR_INT_EN                       \
   | MIPIBIF_INTERRUPT_NO_RESPONSE_ERR_INT_EN)

#define MIPIBIF_FIFO_ERROR_INTERRUPTS_EN (                                  \
    MIPIBIF_INTERRUPT_RXF_UNR_OVR_INT_EN                                    \
    | MIPIBIF_INTERRUPT_TXF_UNR_OVR_INT_EN)

#define MIPIBIF_DEFAULT_INTMASK (                                             \
    MIPIBIF_BCL_ERROR_INTERRUPTS_EN | MIPIBIF_FIFO_ERROR_INTERRUPTS_EN         \
    | MIPIBIF_INTERRUPT_XFER_DONE_INT_EN)

#define MIPIBIF_INTERRUPT_STATUS 0x34
#define MIPIBIF_INTERRUPT_NO_RESPONSE_ERR (1<<8)
#define MIPIBIF_INTERRUPT_RXF_DATA_REQ (1<<7)
#define MIPIBIF_INTERRUPT_TXF_DATA_REQ (1<<6)
#define MIPIBIF_INTERRUPT_XFER_DONE (1<<5)
#define MIPIBIF_INTERRUPT_INV_ERR (1<<4)
#define MIPIBIF_INTERRUPT_PKT_RECV_ERR (1<<3)
#define MIPIBIF_INTERRUPT_LOW_PHASE_IN_WORD_ERR (1<<2)
#define MIPIBIF_INTERRUPT_INCOMPLETE_PKT_RECV_ERR (1<<1)
#define MIPIBIF_INTERRUPT_PARITY_ERR (1<<0)

#define MIPIBIF_TX_FIFO 0x38
#define MIPIBIF_TX_FIFO_MASK 0x3FF

#define MIPIBIF_RX_FIFO 0x3c
#define MIPIBIF_RX_FIFO_MASK 0x3FFFF

#define MIPIBIF_FIFO_CONTROL 0x40
#define MIPIBIF_RXFIFO_FLUSH (1<<9)
#define MIPIBIF_TXFIFO_FLUSH (1<<8)
#define MIPIBIF_RXFIFO_ATN_LVL_SHIFT 4
#define MIPIBIF_TXFIFO_ATN_LVL_SHIFT 0
#define MIPIBIF_FIFO_STATUS 0x44
#define MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT 24
#define MIPIBIF_RX_FIFO_FULL_COUNT_MASK (0xFF<<MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT)
#define MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT 16
#define MIPIBIF_TX_FIFO_EMPTY_COUNT_MASK (0xFF << MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT)
#define MIPIBIF_TX_FIFO_OVF (1<<7)
#define MIPIBIF_TX_FIFO_UNR (1<<6)
#define MIPIBIF_RX_FIFO_OVF (1<<5)
#define MIPIBIF_RX_FIFO_UNR (1<<4)
#define MIPIBIF_TX_FIFO_FULL (1<<3)
#define MIPIBIF_TX_FIFO_EMPTY (1<<2)
#define MIPIBIF_RX_FIFO_FULL (1<<1)
#define MIPIBIF_RX_FIFO_EMPTY (1<<0)

/** Below represent 2-bit (9:8) prefixes for commands. */
#define MIPI_BIF_WD (0x0<<8)        /**< Bits 7:0 are meant for data. */
#define MIPI_BIF_ERA (0x1<<8)        /**< Bits 7:0 are meant for reg add (high). */
#define MIPI_BIF_WRA (0x2<<8)        /**< Bits 7:0 are meant for reg add (low). */
#define MIPI_BIF_RRA (0x3<<8)        /**< Bits 7:0 are meant for reg add (low). */

#define MIPI_BIF_BUS_COMMAND (0x4<<8)        /**< Bits 7:0 are meant to indicate the type of bus command. */
#define MIPI_BIF_EDA (0x5<<8)        /**< Bits 7:0 are meant for device add (high). */
#define MIPI_BIF_SDA (0x6<<8)        /**< Bits 7:0 are meant for device add (low). */

/** Below are 10-bit (9:0) bus commands. */
#define MIPI_BIF_BUS_COMMAND_RESET (0x0 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_PWDN (0x2 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_STBY (0x3 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_EINT (0x10 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_ISTS (0x11 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_RBL0 ((0x2 << 4) | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_RBE0 ((0x3 << 4) | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DASM (0x40 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_BRES (0x00 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DISS (0x80 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DILC (0x81 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIE0 (0x84 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIE1 (0x85 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIP0 (0x86 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DIP1 (0x87 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_DRES (0xc0 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_TQ (0xc2 | MIPI_BIF_BUS_COMMAND)
#define MIPI_BIF_BUS_COMMAND_AIO (0xc4 | MIPI_BIF_BUS_COMMAND)

/** Below are command codes to be used from client side. */
#define MIPI_BIF_WRITE 0x0000
#define MIPI_BIF_READDATA 0x0001
#define MIPI_BIF_INT_READ 0x0002
#define MIPI_BIF_STDBY 0x0003
#define MIPI_BIF_PWRDOWN 0x0004
#define MIPI_BIF_ACTIVATE 0x0005
#define MIPI_BIF_INT_EXIT 0x0006
#define MIPI_BIF_HARD_RESET 0x0007



#define NV_MIPIBIF_READ(Offset, value)                                        \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_MIPIBIF_BASE + Offset); \
    } while (0)

#define NV_MIPIBIF_WRITE(Offset, value)                                             \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_MIPIBIF_BASE + Offset),       \
                   value);                                                    \
    } while (0)

#define NV_CLK_RST_READ(reg, value)                                           \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                    \
                          + CLK_RST_CONTROLLER_##reg##_0);                    \
    } while (0)

#define NV_CLK_RST_WRITE(reg, value)                                          \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                          \
                    + CLK_RST_CONTROLLER_##reg##_0), value);                  \
    } while (0)

#define CLOCK(clock)                                                    \
    do                                                                        \
    {                                                                         \
        NvU32 Reg1;                                                            \
        NvU32 Reg2;                                                            \
        NvU32 Reg3;                                                            \
        NV_CLK_RST_READ(RST_DEVICES_##clock, Reg1);                            \
        NV_CLK_RST_READ(CLK_OUT_ENB_##clock, Reg2);                            \
        Reg1 = Reg1 | (1 << 13);                                               \
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg1);                           \
        Reg2 = Reg2 & ~(1 << 13);                                               \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg2);                            \
        Reg3 = 0xc0000000;                                                        \
        NV_CLK_RST_WRITE(CLK_SOURCE_MIPIBIF, Reg3);                            \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg1);                            \
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg2);                           \
} while (0)

/**
 * Holds the information for processing the bus commands.
 */
typedef struct MipiBifHandleRec {
    /// Holds the Slave device address.
    NvU16 DevAddr;
    /// Holds the register address.
    NvU16 RegAddr;
    /// Defines the BIF commands:
    /// - MIPI_BIF_WRITE 0x0000
    /// - MIPI_BIF_READDATA 0x0001
    /// - MIPI_BIF_BUSQUERY 0x0002
    /// - MIPI_BIF_INT_READ 0x0003
    /// - MIPI_BIF_STDBY 0x0004
    /// - MIPI_BIF_PWRDOWN 0x0005
    NvU16 Cmds;
    ///  Holds the msg length.
    NvU16 Len;
    /// Holds a pointer to the msg.
    NvU8 *Buf;
    /// Holds the number of cmds to be processed.
    NvU8 CmdCnt;
    /// Holds the BIF Time Base.
    NvU8 TauBif;
    /// Holds the BIF Clk in megahertz (MHz).
    NvU8 Clk;
}MipiBifHandle;

/**
 * Initializes clocks and programms FIFO control
 * and timing registers.
 */
void NvDdkMipiBifInit(void);

/**
 * Processes various commands, like bus reset, power down mode,
 * standby, read, and write operations.
 *
 * @param Handle Handle to the MipiBif controller.
 */
NvBool NvDdkMipiBifProcessCmds(MipiBifHandle *Handle);

/**
 * Does the hardware reset of all Slave devices,
 *      including their volatile parameter registers.
 */
void NvDdkMipiBifHardReset(void);

/**
 * Bus command to reset all Slave devices.
 */
void NvDdkMipiBifSendBRESCmd(void);

/** @} */
#endif
