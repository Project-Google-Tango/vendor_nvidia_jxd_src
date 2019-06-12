/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/*
 * This driver is independent of rm and can be used across anywhere in
 * the bootloader
 */
#include "nvddk_mipibif.h"
#include "nvos.h"
#include "nvassert.h"
#include "arapb_misc.h"
#include "aos.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"

#define MISC_PA_BASE              0x70000000
#define NV_ADDRESS_MAP_MIPIBIF_BASE 0x70120000

#define NV_MIPIBIF_REG_SIZE   0x400
#define CLK 13
#define TAUBIF 2
#define ACK_BIT (1<<9)

static void NvDdkSetMipiBifClock(void)
{
    CLOCK(X);
}

static void NvDdkMipiBifMaskIrq(NvU32 Mask)
{
    NvU32 IntMask;
    NV_MIPIBIF_READ(MIPIBIF_INTERRUPT_EN, IntMask);
    IntMask &= ~Mask;
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
}

static void NvDdkMipiBifCommandComplete(void)
{
    NvU32 Reg;
    while(1)
    {
        NV_MIPIBIF_READ(MIPIBIF_INTERRUPT_STATUS, Reg);
        if ((Reg & 0x20)!=0)
            break;
    }
    Reg = 0x20;
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_STATUS, Reg);

}

void NvDdkMipiBifSendBRESCmd()
{
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_BRES);
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, 0x80000000);
    NvDdkMipiBifCommandComplete();
}

#if 0
static NvBool NvDdkMipiBifSendDIP0Cmd(void)
{
    NvU32 Reg;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_DIP0);
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, MIPIBIF_DEFAULT_INTMASK);
    Reg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_BUS_QUERY;
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, Reg);
    NvDdkMipiBifCommandComplete();
    NV_MIPIBIF_READ(MIPIBIF_STATUS, Reg);
    if(Reg & MIPIBIF_BQ_RECV_STATUS)
        return NV_TRUE;
    return NV_FALSE;

}

static NvBool NvDdkMipiBifSendDIP1Cmd(void)
{
    NvU32 Reg;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_DIP1);
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, MIPIBIF_DEFAULT_INTMASK);
    Reg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_BUS_QUERY;
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, Reg);
    NvDdkMipiBifCommandComplete();
    NV_MIPIBIF_READ(MIPIBIF_STATUS, Reg);
    if(Reg & MIPIBIF_BQ_RECV_STATUS)
        return NV_TRUE;
    return NV_FALSE;

}

static NvBool NvDdkMipiBifSendDIE0Cmd(void)
{
    NvU32 Reg;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_DIE0);
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, MIPIBIF_DEFAULT_INTMASK);
    Reg = MIPIBIF_CTRL_GO;
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, Reg);
    NvDdkMipiBifCommandComplete();
    return NV_FALSE;
}

static NvBool NvDdkMipiBifSendDIE1Cmd(void)
{
    NvU32 Reg;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_DIE1);
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, MIPIBIF_DEFAULT_INTMASK);
    Reg = MIPIBIF_CTRL_GO;
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, Reg);
    NvDdkMipiBifCommandComplete();
    return NV_FALSE;
}

static NvBool NvDdkMipiBifSendDISSCmd(void)
{
    NvU32 Reg;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, MIPI_BIF_BUS_COMMAND_DISS);
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, MIPIBIF_DEFAULT_INTMASK);
    Reg = MIPIBIF_CTRL_GO;
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, Reg);
    NvDdkMipiBifCommandComplete();

    return NV_FALSE;

}
#endif

void NvDdkMipiBifHardReset()
{
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_HARD_RESET);
    NvDdkMipiBifCommandComplete();
}

#if 0
static NvBool NvDdkMipiBifDeviceDetection(NvU32 N)
{
    if (!N)
        return;
    if (NvDdkMipiBifSendDIP0Cmd())
    {
        NvDdkMipiBifSendDIE0Cmd();
        NvDdkMipiBifDeviceDetection(N-1);
    }
    if (NvDdkMipiBifSendDIP1Cmd())
    {
        NvDdkMipiBifSendDIE1Cmd();
        NvDdkMipiBifDeviceDetection(N-1);
    }
}

static void NvDdkMipiBifBusQuery(void)
{
    NvDdkMipiBifInit();
    NvDdkMipiBifHardReset();
    NvDdkMipiBifSendDISSCmd();
    NvDdkMipiBifDeviceDetection(80);
}
#endif

static void NvDdkMipiBifProgramTimings(void)
{
    NvU32 TimingPvt;
    NvU32 Timing0;
    NvU32 Timing1;
    NvU32 Timing2;
    NvU32 Timing3;
    NvU32 Timing4;

    TimingPvt = 0 << MIPIBIF_TIMING_TBIF_0_SHIFT;
    TimingPvt |= 2 << MIPIBIF_TIMING_TBIF_1_SHIFT;
    TimingPvt |= 4 << MIPIBIF_TIMING_TBIF_STOP_SHIFT;

    NV_MIPIBIF_WRITE(MIPIBIF_TIMING_PVT, TimingPvt);


    Timing0 = 3 << MIPIBIF_TIMING0_TRESP_MIN_SHIFT;
    Timing0 |= 0xe << MIPIBIF_TIMING0_TRESP_MAX_SHIFT;
    Timing0 |= (CLK * TAUBIF - 1) << MIPIBIF_TIMING0_TBIF_SHIFT;
    NV_MIPIBIF_WRITE(MIPIBIF_TIMING0, Timing0);

    Timing2 = (2500 * CLK - 1) << MIPIBIF_TIMING2_TPDL_SHIFT;
    Timing2 |= (50 * CLK - 1) << MIPIBIF_TIMING2_TPUL_SHIFT;
    NV_MIPIBIF_WRITE(MIPIBIF_TIMING2, Timing2);

    Timing3 = (100 * CLK - 1) << MIPIBIF_TIMING3_TACT_SHIFT;
    Timing3 |= (11000 * CLK - 1) << MIPIBIF_TIMING3_TPUP_SHIFT;

    NV_MIPIBIF_WRITE(MIPIBIF_TIMING3, Timing3);

    NV_MIPIBIF_READ(MIPIBIF_TIMING1,Timing1);
    Timing1 = Timing1 |((5000/TAUBIF)-1);
    NV_MIPIBIF_WRITE(MIPIBIF_TIMING1, Timing1);

    Timing4 = 0xdab << MIPIBIF_TIMING4_TCLWS_SHIFT;
    NV_MIPIBIF_WRITE(MIPIBIF_TIMING4, Timing4);
}

static int NvDdkMipiBifEmptyRxFifo(MipiBifHandle *Handle)
{
    NvU32 Val;
    NvU32 RxFifoAvailable;
    NvU8 *Buf = Handle->Buf;;
    NvU32 RemainingBuf = Handle->Len;
    NvU32 ToBeTransferred = RemainingBuf;

    NV_MIPIBIF_READ(MIPIBIF_FIFO_STATUS, Val);

    RxFifoAvailable = (Val & MIPIBIF_RX_FIFO_FULL_COUNT_MASK) >> MIPIBIF_RX_FIFO_FULL_COUNT_SHIFT;
    if (ToBeTransferred > RxFifoAvailable)
        ToBeTransferred = RxFifoAvailable;

    while (ToBeTransferred > 0)
    {
        NV_MIPIBIF_READ(MIPIBIF_RX_FIFO,Val);
        Val = Val >> 7;
        if (!(Val & ACK_BIT))
        {
            //dev_warn(mipi_bif_dev->dev, "error: Error received in data %x \n", val);
            break; // What else has to be done here?
        }
        *Buf = Val & 0xFF;
        //dev_err(mipi_bif_dev->dev, "data %x \n", *buf);
        RxFifoAvailable--;
        ToBeTransferred--;
        RemainingBuf--;
        Buf++;
    }
    //Msg->Len= buf_remaining;
    Handle->Buf = Buf;
    return 0;
}


static void NvDdkMipiBifSend(NvU32 Val, MipiBifHandle *Handle)
{
//    NvOsDebugPrintf("\nsend--> %0x\n",Val);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,Val);
    Handle->CmdCnt++;
}

static int NvDdkMipiBifFillTxFifo(MipiBifHandle *Handle)
{
    NvU8 *Buf;
    NvU32 Val;
    NvU32 RemainingBuf;
    NvU32 TxFifoAvailable;
    NvU32 ToBeTransferred;

    Buf = Handle->Buf;
    RemainingBuf = Handle->Len;
    ToBeTransferred = RemainingBuf;

    NV_MIPIBIF_READ(MIPIBIF_FIFO_STATUS, Val);

    TxFifoAvailable = (Val & MIPIBIF_TX_FIFO_EMPTY_COUNT_MASK) >> MIPIBIF_TX_FIFO_EMPTY_COUNT_SHIFT;

    if (TxFifoAvailable > 0)
    {
        if (ToBeTransferred > TxFifoAvailable)
            ToBeTransferred = TxFifoAvailable;

        Handle->Buf = Buf + ToBeTransferred;
        RemainingBuf -= ToBeTransferred;
        //buf_remaining = Msg->Len;

        while (ToBeTransferred)
        {
            NvDdkMipiBifSend(*Buf,Handle);
            Buf++;
            ToBeTransferred--;
            TxFifoAvailable--;
        }
    }
    return 0;
}

static int NvDdkMipiBifFlushFifos(void)
{
    NvU32 Val;

    NV_MIPIBIF_READ(MIPIBIF_FIFO_CONTROL,Val);
    Val |= MIPIBIF_RXFIFO_FLUSH;
    Val |= MIPIBIF_TXFIFO_FLUSH;

    NV_MIPIBIF_WRITE(MIPIBIF_FIFO_CONTROL, Val);
    while (1)
    {
        NV_MIPIBIF_READ(MIPIBIF_FIFO_CONTROL, Val);
        if (Val & (MIPIBIF_RXFIFO_FLUSH | MIPIBIF_RXFIFO_FLUSH))
            ;
        else
            break;
    }
    return 0;
}
#if 0
static NvU32 NvDdkMipiBifDISScmd(void)
{
}
#endif
NvBool NvDdkMipiBifProcessCmds(MipiBifHandle *Handle)
{

    NvU32 SdaPart;
    NvU32 EraPart;
    NvU32 WraPart;
    NvU32 RraPart;
    NvU32 Rbe = MIPI_BIF_BUS_COMMAND_RBE0;
    NvU32 Rbl = MIPI_BIF_BUS_COMMAND_RBL0;
    NvU32 CtrlReg = 0;


    NvU32 DefIntMask = MIPIBIF_BCL_ERROR_INTERRUPTS_EN | MIPIBIF_FIFO_ERROR_INTERRUPTS_EN | MIPIBIF_INTERRUPT_XFER_DONE_INT_EN;
    NvU32 IntMask = DefIntMask;

    Handle->CmdCnt = 0;
    NvDdkMipiBifInit();

    NvDdkMipiBifFlushFifos();

    SdaPart = Handle->DevAddr & 0xFF;

    EraPart = (Handle->RegAddr & 0xFF00) >> 8;

/* Following restrictions right now to make driver simpler
 - No DASM
 - No AIO
 - No TQ
 - WRITE below implemented for single slave, no AIO. Slave selected everytime by SDA.
 - Not sure if receiving large number of data has to be split into multiple transactions.
*/
    if (Handle->Cmds== MIPI_BIF_WRITE)
    {
        if (Handle->Len== 0 || (!Handle->Buf))
            //return -EINVAL;
        // Enable Interrupts

        CtrlReg = MIPIBIF_CTRL_COMMAND_NO_READ;
        WraPart = Handle->RegAddr& 0xFF;

        NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
        NvDdkMipiBifSend( EraPart | MIPI_BIF_ERA, Handle);
        NvDdkMipiBifSend( WraPart | MIPI_BIF_WRA, Handle);

        CtrlReg |= ((Handle->Len + Handle->CmdCnt- 1) << MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
        NvDdkMipiBifFillTxFifo(Handle);

        if (Handle->Len)
            IntMask |= MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN;

        NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
        // NvOsDebugPrintf("\nwrite--> %0x\n",int_mask);
        CtrlReg |= MIPIBIF_CTRL_GO;
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        //NvOsDebugPrintf("\nwrite--> %0x\n",ctrl_reg);
        NvDdkMipiBifCommandComplete();
        NvDdkMipiBifMaskIrq(MIPIBIF_INTERRUPT_TXF_DATA_REQ_INT_EN);
    }
    else if (Handle->Cmds== MIPI_BIF_READDATA)
    {
        RraPart = Handle->RegAddr & 0xFF;
        Rbe |= ((Handle->Len & 0xFF00) >> 8);
        Rbl |= (Handle->Len & 0xFF);

        if (Handle->Len == 256)
            Rbe= Rbl = 0;

        IntMask |= MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN;
        CtrlReg |= MIPIBIF_CTRL_COMMAND_READ_DATA;

        NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
        //NvAvpMipiBifSend( rbe | MIPI_BIF_BUS_COMMAND_RBE0);
        NvDdkMipiBifSend( Rbl | MIPI_BIF_BUS_COMMAND_RBL0, Handle);
        NvDdkMipiBifSend( EraPart | MIPI_BIF_ERA, Handle);
        NvDdkMipiBifSend( RraPart | MIPI_BIF_RRA, Handle);
        CtrlReg |= ((Handle->CmdCnt- 1)<< MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
        // Anything else to be accounted ??
        NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
        CtrlReg |= MIPIBIF_CTRL_GO;
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        NvDdkMipiBifCommandComplete();
        if (Handle->Len)
            NvDdkMipiBifEmptyRxFifo(Handle);
        NvDdkMipiBifMaskIrq(MIPIBIF_INTERRUPT_RXF_DATA_REQ_INT_EN);
    }
    else if (Handle->Cmds == MIPI_BIF_INT_READ)
    {
        NvU32 Val;
        NvDdkMipiBifSend( SdaPart | MIPI_BIF_SDA, Handle);
        NvDdkMipiBifSend( MIPI_BIF_BUS_COMMAND_EINT,Handle);
        CtrlReg = MIPIBIF_CTRL_COMMAND_INTERRUPT_DATA;
        CtrlReg |= ((Handle->CmdCnt - 1) << MIPIBIF_CTRL_PACKETCOUNT_SHIFT);
        CtrlReg |= MIPIBIF_CTRL_GO;

        NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        NvDdkMipiBifCommandComplete();
        NvOsWaitUS(5000);
        NV_MIPIBIF_READ(MIPIBIF_STATUS, Val);
        if(!(Val& MIPIBIF_INTERRUPT_RECV_STATUS))
        {
                NvOsDebugPrintf("Interrupt not received\n");
               return NV_FALSE;
        }
        NvOsDebugPrintf("Interrupt  received\n");
    }
    else if (Handle->Cmds == MIPI_BIF_STDBY)
    {
        NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,MIPI_BIF_BUS_COMMAND_STBY);
        CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_STBY;
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        NvDdkMipiBifCommandComplete();
    }
    else if (Handle->Cmds == MIPI_BIF_ACTIVATE)
    {
        NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, IntMask);
        CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_ACTIVATE;
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        NvDdkMipiBifCommandComplete();
    }
    else if (Handle->Cmds == MIPI_BIF_PWRDOWN)
    {
        NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,MIPI_BIF_BUS_COMMAND_PWDN);
        CtrlReg = MIPIBIF_CTRL_GO | MIPIBIF_CTRL_COMMAND_PWDN;
        NV_MIPIBIF_WRITE(MIPIBIF_CTRL, CtrlReg);
        NvDdkMipiBifCommandComplete();
    }
    else if (Handle->Cmds == MIPI_BIF_HARD_RESET)
    {
        NvDdkMipiBifHardReset();
    }
    return NV_TRUE;
}
/**
 * @brief Provides a handle for mipibif
 */
void
NvDdkMipiBifInit()
{
    NvU32 Fifo_ctrl;
    NvDdkSetMipiBifClock();
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, 0);
    Fifo_ctrl = 0<<MIPIBIF_RXFIFO_ATN_LVL_SHIFT;
    Fifo_ctrl |= 5<<MIPIBIF_TXFIFO_ATN_LVL_SHIFT;
    Fifo_ctrl |= MIPIBIF_RXFIFO_FLUSH;
    Fifo_ctrl |= MIPIBIF_TXFIFO_FLUSH;
    NV_MIPIBIF_WRITE(MIPIBIF_INTERRUPT_EN, Fifo_ctrl);
    NvDdkMipiBifProgramTimings();
}

NvU32 WriteData[]={0xa,0xb,0xc,0xd,0xe,0xf,0xe,0xd};
NvU32 ReadData[10];

#if 0
static void NvDdkWriteDataCommand(void)
{
    NvU32 i;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x601);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x10F);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x209);
    for (i=0;i<8;i++)
    {
        NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO,WriteData[i]);
    }
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, 0x80000A00 );
    NvDdkMipiBifCommandComplete();
}

static void NvDdkReadDataCommand(void)
{
    NvU32 i;
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x601);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x428);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x10F);
    NV_MIPIBIF_WRITE(MIPIBIF_TX_FIFO, 0x309);
    NV_MIPIBIF_WRITE(MIPIBIF_CTRL, 0x80100300 );
    NvDdkMipiBifCommandComplete();
    for (i = 0; i < 8; i++)
    {
        NV_MIPIBIF_READ(MIPIBIF_RX_FIFO,ReadData[i]);
    }
    i = 0;
    for (i = 0; i < 8 ; i++)
        NvOsDebugPrintf("ReadData[ %d] - %x", i,(( ReadData[i] >> 7) & 0xff));

}
#endif

