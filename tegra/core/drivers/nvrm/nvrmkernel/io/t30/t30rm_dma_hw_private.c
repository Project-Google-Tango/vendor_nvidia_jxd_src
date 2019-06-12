/*
 * Copyright (c) 2007-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           DMA Resource manager private API for Hw access </b>
 *
 * @b Description: Implements the private interface of the nnvrm dma to access
 * the hw apb/ahb dma register.
 *
 * This files implements the API for accessing the register of the Dma
 * controller and configure the dma transfers for Ap15.
 */

#include "nvrm_dma.h"
#include "ap15/rm_dma_hw_private.h"
#include "t30/arapbdma.h"
#include "t30/arapbdmachan.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"

#define APBDMACHAN_READ32(pVirtBaseAdd, reg) \
        NV_READ32((pVirtBaseAdd) + ((APBDMACHAN_CHANNEL_0_##reg##_0)/4))
#define APBDMACHAN_WRITE32(pVirtBaseAdd, reg, val) \
    do { \
        NV_WRITE32(((pVirtBaseAdd) + ((APBDMACHAN_CHANNEL_0_##reg##_0)/4)), (val)); \
    } while(0)

static const NvU32 s_AudioApbIf_Trigger[] = {
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_APBIF_CH0,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_APBIF_CH1,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_APBIF_CH2,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_APBIF_CH3
};

static const NvU32 s_Uart_Trigger[] = {
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_UART_A,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_UART_B,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_UART_C,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_UART_D,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_UART_E
};

static const NvU32 s_Slink_Trigger[] = {
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B1,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B2,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B3,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B4,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B5,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_SL2B6
};

static const NvU32 s_I2c_Trigger[] = {
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_I2C,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_I2C2,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_I2C3,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_I2C4,
        APBDMACHAN_CHANNEL_0_CSR_0_REQ_SEL_DVC_I2C
};

static void
ConfigureDmaRequestor(
    DmaChanRegisters *pDmaChRegs,
    NvRmDmaModuleID DmaReqModuleId,
    NvU32 DmaReqInstId)
{
    // Check for the dma module Id and based on the dma module Id, decide
    // the trigger requestor source.
    switch (DmaReqModuleId)
    {
        /// Specifies the dma module Id for memory
        case NvRmDmaModuleID_Memory:
            // Dma transfer will be from memory to memory.
            // Use the reset value only for the ahb data transfer.
            break;


        case NvRmDmaModuleID_AudioApbIf:
            // Dma requestor is the audio-apb interface
            NV_ASSERT(DmaReqInstId < 4);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_NUM(APBDMACHAN_CHANNEL_0,
                                        CSR, REQ_SEL, s_AudioApbIf_Trigger[DmaReqInstId],
                                        pDmaChRegs->ControlReg);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
            break;

        case NvRmDmaModuleID_Uart:
            // Dma requestor is the uart.
            NV_ASSERT(DmaReqInstId < 5);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_NUM(APBDMACHAN_CHANNEL_0,
                                        CSR, REQ_SEL, s_Uart_Trigger[DmaReqInstId],
                                        pDmaChRegs->ControlReg);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                        CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                        APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_8,
                                        pDmaChRegs->ApbSequenceReg);
            break;

        case NvRmDmaModuleID_Vfir:
            // Dma requestor is the vfir.
            NV_ASSERT(DmaReqInstId < 1);
            if (DmaReqInstId == 1)
                pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, REQ_SEL, UART_B, pDmaChRegs->ControlReg);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
            break;

        case NvRmDmaModuleID_Hsi:
            // Dma requestor is the HSI controller.
            NV_ASSERT(DmaReqInstId < 1);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, REQ_SEL, HSI, pDmaChRegs->ControlReg);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
            break;

        case NvRmDmaModuleID_Slink:
            // Dma requestor is the Slink controller.
            NV_ASSERT(DmaReqInstId < 6);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_NUM(APBDMACHAN_CHANNEL_0,
                                        CSR, REQ_SEL, s_Slink_Trigger[DmaReqInstId],
                                        pDmaChRegs->ControlReg);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
            break;


        case NvRmDmaModuleID_I2c:
            // Dma requestor is the I2c controller.
            NV_ASSERT(DmaReqInstId < 5);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_NUM(APBDMACHAN_CHANNEL_0,
                                        CSR, REQ_SEL, s_I2c_Trigger[DmaReqInstId],
                                        pDmaChRegs->ControlReg);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
            break;

        case NvRmDmaModuleID_Dtv:
            // Dma requestor is the Dtv controller.
            NV_ASSERT(DmaReqInstId < 1);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, REQ_SEL, DTV, pDmaChRegs->ControlReg);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);


        case NvRmDmaModuleID_Owr:
            // Dma requestor is the Owr controller.
            NV_ASSERT(DmaReqInstId < 1);

            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, REQ_SEL, OWR, pDmaChRegs->ControlReg);
            pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            CSR, FLOW, ENABLE, pDmaChRegs->ControlReg);
            pDmaChRegs->ApbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                            APB_SEQ, APB_BUS_WIDTH, BUS_WIDTH_32,
                                            pDmaChRegs->ApbSequenceReg);
        default:
            NV_ASSERT(!"Invalid module");
    }
}

/**
 * Configure the Apb dma register as per clients information.
 * This function do the register setting based on device Id and will  be stored
 * in the dma handle. This information will be used when there is dma transfer
 * request and want to configure the dma controller registers.
 */
static void
InitApbDmaRegisters(
    DmaChanRegisters *pDmaChRegs,
    NvRmDmaModuleID DmaReqModuleId,
    NvU32 DmaReqInstId)
{
    pDmaChRegs->pHwDmaChanReg = NULL;

    //  Set the dma register of dma handle to their power on reset values.
    pDmaChRegs->ControlReg = NV_RESETVAL(APBDMACHAN_CHANNEL_0, CSR);
    pDmaChRegs->AhbSequenceReg = NV_RESETVAL(APBDMACHAN_CHANNEL_0, AHB_SEQ);
    pDmaChRegs->ApbSequenceReg = NV_RESETVAL(APBDMACHAN_CHANNEL_0,APB_SEQ);

    // Configure the dma register for the OnceMode
    pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0, CSR,
                                ONCE, SINGLE_BLOCK, pDmaChRegs->ControlReg);

    // Configure the dma register for  enabling the interrupt so that it will generate the interrupt
    // after transfer completes.
    pDmaChRegs->ControlReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0, CSR,
                                IE_EOC, ENABLE, pDmaChRegs->ControlReg);

    // Configure the dma register for  interrupting the cpu only.
    pDmaChRegs->AhbSequenceReg = NV_FLD_SET_DRF_DEF(APBDMACHAN_CHANNEL_0,
                                AHB_SEQ, INTR_ENB, CPU, pDmaChRegs->AhbSequenceReg);

    // Configure the dma registers as per requestor information.
    ConfigureDmaRequestor(pDmaChRegs, DmaReqModuleId, DmaReqInstId);
}

void NvRmPrivDmaInitT30DmaHwInterfaces(DmaHwInterface *pApbDmaInterface)
{
    pApbDmaInterface->DmaHwInitRegistersFxn = InitApbDmaRegisters;
}
