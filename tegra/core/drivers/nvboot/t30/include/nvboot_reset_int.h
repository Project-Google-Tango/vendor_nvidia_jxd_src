/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_reset_int.h - Declarations for reset support.
 *
 */

#ifndef INCLUDED_NVBOOT_RESET_INT_H
#define INCLUDED_NVBOOT_RESET_INT_H

#include "t30/arclk_rst.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Set of reset signals supported in the API */
// The corresponding enum embeds some information on the register structure
#define NVBOOT_RESET_DEVICE_BIT_OFFSET_MASK (0x1F)
#define NVBOOT_RESET_H_REG (0x20)
#define NVBOOT_RESET_U_REG (0x40)
#define NVBOOT_RESET_V_REG (0x100)
#define NVBOOT_RESET_W_REG (NVBOOT_RESET_V_REG + NVBOOT_RESET_H_REG)
#define NVBOOT_RESET_BIT_OFFSET(DeviceId)         (((NvU32) DeviceId) & NVBOOT_RESET_DEVICE_BIT_OFFSET_MASK)
#define NVBOOT_RESET_HAS_UPPERREG_ENB(DeviceId) (((NvU32) DeviceId) & NVBOOT_RESET_U_REG)
#define NVBOOT_RESET_HAS_VWREG_ENB(DeviceId)	(((NvU32) DeviceId) & NVBOOT_RESET_V_REG)
#define NVBOOT_RESET_REG_OFFSET(DeviceId)      (((NvU32) NVBOOT_RESET_HAS_UPPERREG_ENB(DeviceId) ) ? \
    ( (((NvU32) DeviceId) & NVBOOT_RESET_U_REG) >> 3 ) : ( (((NvU32) DeviceId) & NVBOOT_RESET_H_REG) >> 3 ))

typedef enum
{
    NvBootResetDeviceId_CpuId    = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_CPU_RST_SHIFT,
    NvBootResetDeviceId_CopId    = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_COP_RST_SHIFT,
    NvBootResetDeviceId_I2c1Id   = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_I2C1_RST_SHIFT,
    NvBootResetDeviceId_NandId   = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_NDFLASH_RST_SHIFT,
    NvBootResetDeviceId_SdmmcId  = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_SDMMC4_RST_SHIFT,
    NvBootResetDeviceId_UartaId  = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_UARTA_RST_SHIFT,
    NvBootResetDeviceId_UsbId    = CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_USBD_RST_SHIFT,
    NvBootResetDeviceId_McId     = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_MEM_RST_SHIFT    + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_EmcId    = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_EMC_RST_SHIFT    + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_BseaId   = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_BSEA_RST_SHIFT   + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_BsevId   = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_BSEV_RST_SHIFT   + NVBOOT_RESET_H_REG,

    //       from the changed controller.
    NvBootResetDeviceId_SpiId    = CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_SBC4_RST_SHIFT   + NVBOOT_RESET_U_REG,

    NvBootResetDeviceId_VdeId    = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_VDE_RST_SHIFT    + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_ApbDmaId = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_APBDMA_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_AhbDmaId = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_AHBDMA_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_SnorId   = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_NOR_RST_SHIFT    + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Usb3Id   = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_USB3_RST_SHIFT    + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Sdmmc3Id   = CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_SDMMC3_RST_SHIFT    + NVBOOT_RESET_U_REG,
    NvBootResetDeviceId_SeId	 = CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_SE_RST_SHIFT + NVBOOT_RESET_V_REG,
    NvBootResetDeviceId_I2c2Id   = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_I2C2_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_I2c3Id   = CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_I2C3_RST_SHIFT + NVBOOT_RESET_U_REG,
    NvBootResetDeviceId_I2c4Id   = CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_I2C4_RST_SHIFT + NVBOOT_RESET_V_REG,
    NvBootResetDeviceId_DvcI2cId = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_DVC_I2C_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Sbc1Id	 = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_SBC1_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Sbc2Id	 = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_SBC2_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Sbc3Id	 = CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_SBC3_RST_SHIFT + NVBOOT_RESET_H_REG,
    NvBootResetDeviceId_Sbc5Id	 = CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_SBC5_RST_SHIFT + NVBOOT_RESET_V_REG,
    NvBootResetDeviceId_Sbc6Id	 = CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_SBC6_RST_SHIFT + NVBOOT_RESET_V_REG,

   NvBootResetSignalId_Force32 = 0x7fffffff
} NvBootResetDeviceId;

#define NVBOOT_RESET_CHECK_ID(DeviceId) \
    NV_ASSERT( (DeviceId == NvBootResetDeviceId_CpuId)    || \
               (DeviceId == NvBootResetDeviceId_CopId)    || \
               (DeviceId == NvBootResetDeviceId_I2c1Id)   || \
               (DeviceId == NvBootResetDeviceId_NandId)   || \
               (DeviceId == NvBootResetDeviceId_SdmmcId)  || \
               (DeviceId == NvBootResetDeviceId_UartaId)  || \
               (DeviceId == NvBootResetDeviceId_UsbId)    || \
               (DeviceId == NvBootResetDeviceId_Usb3Id)   || \
               (DeviceId == NvBootResetDeviceId_McId)     || \
               (DeviceId == NvBootResetDeviceId_EmcId)    || \
               (DeviceId == NvBootResetDeviceId_BseaId)   || \
               (DeviceId == NvBootResetDeviceId_BsevId)   || \
               (DeviceId == NvBootResetDeviceId_VdeId)    || \
               (DeviceId == NvBootResetDeviceId_ApbDmaId) || \
               (DeviceId == NvBootResetDeviceId_AhbDmaId) || \
               (DeviceId == NvBootResetDeviceId_SnorId)   || \
               (DeviceId == NvBootResetDeviceId_Sdmmc3Id)   || \
               (DeviceId == NvBootResetDeviceId_SeId)   || \
               (DeviceId == NvBootResetDeviceId_SpiId)  || \
               (DeviceId == NvBootResetDeviceId_I2c2Id)   || \
               (DeviceId == NvBootResetDeviceId_I2c3Id)   || \
               (DeviceId == NvBootResetDeviceId_I2c4Id)   || \
               (DeviceId == NvBootResetDeviceId_DvcI2cId)   || \
               (DeviceId == NvBootResetDeviceId_Sbc1Id)  || \
               (DeviceId == NvBootResetDeviceId_Sbc2Id)  || \
               (DeviceId == NvBootResetDeviceId_Sbc3Id)  || \
               (DeviceId == NvBootResetDeviceId_Sbc5Id)  || \
               (DeviceId == NvBootResetDeviceId_Sbc6Id)  || \
               0 );


#define NVBOOT_RESET_STABILIZATION_DELAY (0x2)

/*
 * NvBootResetSetEnable(): Control the reset level for the identified reset signal
 */
void
NvBootResetSetEnable(const NvBootResetDeviceId DeviceId, const NvBool Enable);

/*
 * NvBootResetFullChip(): This is the highest possible reset, everything but AO is reset
 */
void
NvBootResetFullChip(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_RESET_INT_H */
