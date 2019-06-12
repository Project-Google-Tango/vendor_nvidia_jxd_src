/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#include "avp_uart_headers.h"
#include "avp_uart.h"
#include "arapbpm.h"
#include "nvodm_query.h"
#include "nvbct.h"

#define AVP_UART_DEBUG_BAUD   115200
#define NV_BIT_ADDRESS        0x40000000

static NvU32 s_UartBaseAddress[] =
{
    0x70006000,
    0x70006040,
    0x70006200,
    0x70006300,
    0x70006400
};

NvU32 s_UartPort = 0;

static NV_INLINE NvU32 NvAvpUartTxReady(void)
{
    NvU32 Reg;

    NV_UART_READ(s_UartBaseAddress[s_UartPort], LSR, Reg);
    return Reg & UART_LSR_0_THRE_FIELD;
}

static NV_INLINE NvU32 NvAvpUartRxReady(void)
{
    NvU32 Reg;

    NV_UART_READ(s_UartBaseAddress[s_UartPort], LSR, Reg);
    return Reg & UART_LSR_0_RDR_FIELD;
}

static NV_INLINE void NvAvpUartTx(NvU8 c)
{
    NV_UART_WRITE(s_UartBaseAddress[s_UartPort], THR_DLAB_0, c);
}

static NV_INLINE NvU32 NvAvpUartRx(void)
{
    NvU32 Reg;

    NV_UART_READ(s_UartBaseAddress[s_UartPort], THR_DLAB_0, Reg);
    return Reg;
}

NvS32 NvAvpUartPoll(void)
{
    if (NvAvpUartRxReady())
        return NvAvpUartRx();

    return -1;
}

static NV_INLINE NvU32 NvAvpUartTrasmitComplete(void)
{
    NvU32 Reg;

    NV_UART_READ(s_UartBaseAddress[s_UartPort], LSR, Reg);
    return Reg & UART_LSR_0_TMTY_FIELD;
}


static void NvAvpSetUartClock(NvU32 port)
{
    switch(port)
    {
        case 0://uartA
            CLOCK(A,L);
            break;
        case 1://uartB
            CLOCK(B,L);
            break;
        case 2://uartC
            CLOCK(C,H);
            break;
        case 3://uartD
            CLOCK(D,U);
            break;
#ifndef DISABLE_UARTE
        case 4://uartE
            CLOCK(E,U);
            break;
#endif
        default:
            NV_ASSERT(0);
    }
}

NvOdmDebugConsole NvAvpGetDebugPort(void)
{

    const NvBootInfoTable *pBit;
    NvBctAuxInternalData *pBctAux;
    NvOdmDebugConsole debugconsole = NvOdmDebugConsole_Dcc;

    pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);

    if (pBit->BctPtr)
    {
        pBctAux = (NvBctAuxInternalData *)((NvU32)(pBit->BctPtr->CustomerData) +
                                          NVBOOT_BCT_CUSTOMER_DATA_SIZE -
                                          sizeof(NvBctAuxInternalData));

        debugconsole = NvOdmQueryGetOdmdataField(pBctAux->CustomerOption,
                       NvOdmOdmdataField_DebugConsoleOptions);
    }
    return debugconsole;
}

static NvU32 NvAvpGetChipIdMajorRev(void)
{
    NvU32 regVal = 0;
    NvU32 majorId = 0;
    NV_MISC_READ(GP_HIDREV, regVal);
    majorId = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, regVal);
    return majorId;
}

static void NvAvpUartSetBaudRate(NvU32 PllpFreq)
{
    NvU32 RegValue;
    void* pUart;

    pUart = (void*)s_UartBaseAddress[s_UartPort];
    // Enable DLAB access
    RegValue = NV_DRF_NUM(UART, LCR, DLAB, 1);
    NV_UART_WRITE(pUart, LCR, RegValue);

    if(NvAvpGetChipIdMajorRev() == APB_MISC_GP_HIDREV_0_MAJORREV_EMULATION)
    {
        // Prepare the divisor value, if it is FPGA, then the PLLP is 13MHz
        RegValue = (PLLP_FIXED_FREQ_KHZ_13000 * 1000) / (AVP_UART_DEBUG_BAUD * 16);
    }
    else
    {
        // Prepare the divisor value
        switch(PllpFreq)
        {
        case PLLP_FIXED_FREQ_KHZ_216000:
            RegValue = (PLLP_FIXED_FREQ_KHZ_216000 * 1000) / (AVP_UART_DEBUG_BAUD * 16);
            break;
        case PLLP_FIXED_FREQ_KHZ_432000:
            RegValue = (PLLP_FIXED_FREQ_KHZ_432000 * 1000) / (AVP_UART_DEBUG_BAUD * 16);
            break;
        case PLLP_FIXED_FREQ_KHZ_408000:
        default:
            RegValue = (PLLP_FIXED_FREQ_KHZ_408000 * 1000) / (AVP_UART_DEBUG_BAUD * 16);
            break;
        }
    }
    // Program DLAB
    NV_UART_WRITE(pUart, THR_DLAB_0, RegValue & 0xFF);
    NV_UART_WRITE(pUart, IER_DLAB_0, (RegValue >> 8) & 0xFF);

    // Disable DLAB access
    RegValue = NV_DRF_NUM(UART, LCR, DLAB, 0);
    NV_UART_WRITE(pUart, LCR, RegValue);
}

void NvAvpUartInit(NvU32 pll)
{
    NvU32 RegValue;
    NvOdmDebugConsole debugport;
    void* pUart;

#ifndef BOOT_MINIMAL_BL
    debugport = NvAvpGetDebugPort();
#else
    if(NvAvpGetChipIdMajorRev() == APB_MISC_GP_HIDREV_0_MAJORREV_EMULATION)
        debugport = NvOdmDebugConsole_UartA;
    else
        debugport = NvOdmDebugConsole_UartD;
#endif

    // If Debug Console is Automation ttys* the debugport
    // should be reduced to the range of UARTA to UARTD port.

    if (debugport >= NvOdmDebugConsole_Automation)
        debugport -= NvOdmDebugConsole_Automation;

    if (debugport >= NvOdmDebugConsole_UartA)
        s_UartPort = debugport - NvOdmDebugConsole_UartA;

    NvAvpSetUartClock(s_UartPort);

    pUart = (void*)s_UartBaseAddress[s_UartPort];

    NvAvpUartSetBaudRate(pll);

    // Program FIFO congrol Reg to clear Tx, Rx FIRO and to enable them
    RegValue = NV_DRF_NUM(UART, IIR_FCR, TX_CLR, 1) |
                         NV_DRF_NUM(UART, IIR_FCR, RX_CLR, 1) |
                         NV_DRF_NUM(UART, IIR_FCR, FCR_EN_FIFO, 1);
    NV_UART_WRITE(pUart, IIR_FCR, RegValue);

    // Write Line Control Reg to set no parity, 1 stop bit, word Lengh 8
    RegValue = NV_DRF_NUM(UART, LCR, PAR, 0) |
                         NV_DRF_NUM(UART, LCR, STOP, 0) |
                         NV_DRF_NUM(UART, LCR, WD_SIZE, 3);
    NV_UART_WRITE(pUart, LCR, RegValue);

    NV_UART_WRITE(pUart, MCR, UART_MCR_0_SW_DEFAULT_VAL);
    NV_UART_WRITE(pUart, MSR, UART_MSR_0_SW_DEFAULT_VAL);
    NV_UART_WRITE(pUart, SPR, UART_SPR_0_SW_DEFAULT_VAL);
    NV_UART_WRITE(pUart, IRDA_CSR, UART_IRDA_CSR_0_SW_DEFAULT_VAL);
    NV_UART_WRITE(pUart, ASR, UART_ASR_0_SW_DEFAULT_VAL);

    // Flush any old characters out of the RX FIFO.
    while(NvAvpUartRxReady())
        (void)NvAvpUartRx();
}

void NvAvpUartWrite(const void *ptr)
{
    const NvU8 *p = ptr;
    while(*p)
    {
        while(!NvAvpUartTxReady())
            ;
        if (*p == '\n')
        {
            NvAvpUartTx('\r');
            while(!NvAvpUartTxReady())
                ;
        }
        NvAvpUartTx(*p);
        p++;
    }
    while(!NvAvpUartTrasmitComplete())
        ;
}

void NvOsAvpDebugPrintf(const char *format, ...)
{
    va_list ap;
    char msg[256];

    va_start(ap, format);
    NvSimpleVsnprintf(msg, sizeof(msg), format, ap);
    NvAvpUartWrite(msg);
    va_end( ap );
}
