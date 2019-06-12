/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvassert.h"
#include "nvos.h"
#include "nvrm_drf.h"
#include "nvboot_reset_int.h"
#include "nvboot_fuse.h"
#include "nvboot_bit.h"
#include "nvboot_clocks_int.h"
#include "nvboot_pads_int.h"
#include "nvrm_memmgr.h"
#include "nvddk_fuse.h"
#include "nvddk_xusb_context.h"
#include "nvddk_xusbh.h"
#include "nvddk_xusb_msc.h"
#include "nvddk_xusb_arc.h"
#include "iram_address_allocation.h"
#include "xusb/ardev_t_xusb_csb.h"
#include "xusb/ardev_t_xusb_xhci.h"
#include "xusb/ardev_t_fpci_xusb.h"
#include "xusb/ardev_t_fpci_xusb_0.h"
#include "xusb/arxusb_host.h"
#include "xusb/arxusb_padctl.h"
#include "arfuse.h"
#include "arapb_misc.h"

#define NV_WRITE08(a,d)     *((volatile NvU8 *)(a)) = (d)
#define NV_WRITE16(a,d)     *((volatile NvU16 *)(a)) = (d)
#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)
#define NV_WRITE64(a,d)     *((volatile NvU64 *)(a)) = (d)
#define NV_READ8(a)         *((const volatile NvU8 *)(a))
#define NV_READ16(a)        *((const volatile NvU16 *)(a))
#define NV_READ32(a)        *((const volatile NvU32 *)(a))
#define NV_READ64(a)        *((const volatile NvU64 *)(a))

#define NV_XUSB_HOST_APB_BAR0_START         0x70090000
#define NV_XUSB_HOST_APB_DFPCI_CFG          0x70098000
#define NV_XUSB_HOST_IPFS_REGS              0x70099000
#define NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE 0x7009F000
#define NV_ADDRESS_MAP_FUSE_BASE            0x7000F800
#define NV_ADDRESS_MAP_CAR_BASE             0x60006000

#define FUSE_BOOT_DEVICE_INFO_0_BOOT_DEVICE_CONFIG_RANGE 13:0

#define NV_PF_VAL(r,f,v) \
    (((v)>> NV_FIELD_SHIFT(r##_##f##_RANGE)) & \
        NV_FIELD_MASK(r##_##f##_RANGE))

/// arxusb_padctl.h ////////
#define NV_XUSB_PADCTL_EXPAND(reg, value) \
    do \
    { \
        value = ((XUSB_PADCTL_##reg##_0)); \
    } while (0)

#define NV_XUSB_PADCTL_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE + XUSB_PADCTL_##reg##_0)); \
    } while (0)

#define NV_XUSB_PADCTL_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE + XUSB_PADCTL_##reg##_0), value); \
    } while (0)


///ardev_t_xusb_csb.h ////
#define NV_XUSB_CSB_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE + XUSB_CSB_##reg##_0)); \
    } while (0)

#define NV_XUSB_CSB_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE + XUSB_CSB_##reg##_0), value); \
    } while (0)

#define NV_XUSB_CSB_EXPAND(reg, value) \
    do \
    { \
         value = ((XUSB_CSB_##reg##_0)); \
    } while (0)

//// ardev_t_fpci_xusb.h ///
#define NV_XUSB_CFG_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_XUSB_HOST_APB_DFPCI_CFG + XUSB_CFG_##reg##_0)); \
    } while (0)

#define NV_XUSB_CFG_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_XUSB_HOST_APB_DFPCI_CFG + XUSB_CFG_##reg##_0), value); \
    } while (0)

#define NV_XUSB_CFG_IND_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_XUSB_HOST_APB_DFPCI_CFG + reg)); \
    } while (0)

#define NV_XUSB_CFG_IND_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_XUSB_HOST_APB_DFPCI_CFG + reg), value); \
    } while (0)

///ardev_t_xusb_xhci.h ////
#define NV_XUSB_XHCI_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_XUSB_HOST_APB_BAR0_START + XUSB_XHCI_##reg##_0)); \
    } while (0)

#define NV_XUSB_XHCI_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_XUSB_HOST_APB_BAR0_START+ XUSB_XHCI_##reg##_0), value); \
    } while (0)

///arxusb_host.h ////
#define NV_XUSB_HOST_READ(reg, value) \
    do \
    { \
        value = NV_READ32((NV_XUSB_HOST_IPFS_REGS + XUSB_HOST_##reg##_0)); \
    } while (0)

#define NV_XUSB_HOST_WRITE(reg, value) \
    do \
    { \
        NV_WRITE32((NV_XUSB_HOST_IPFS_REGS + XUSB_HOST_##reg##_0), value); \
    } while (0)

#define CAR_READ(reg_offset, num_offset) \
    NV_READ32( NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_##reg_offset##_0 + num_offset);

#define CAR_WRITE(reg_offset, num_offset, data) \
    do \
    { \
        NV_WRITE32( NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_##reg_offset##_0 + \
                                                            num_offset, data); \
    }while (0)


NvU8 *BufferXusbCmd = NULL;
NvU8 *BufferXusbStatus = NULL;
NvU8 *BufferXusbData = NULL;


static void PciRegUpdate(NvddkXusbContext *Context,
                         NvU32 Register,
                         NvBool Update,
                         NvU32 *value)
{
    NvU32 RegOffet = Register % XUSB_CFG_PAGE_SIZE;
    NvU32 PageNum = Register / XUSB_CFG_PAGE_SIZE;

    /*
    a. Find the CSB space address by looking into the ref manual. Here it is 0x110084
            >>> EXPAND
    b. Program the value of 0x110084/0x200 into NV_PROJ__XUSB_CFG_ARU_C11_CSBRANGE
            >>> NV_PROJ__XUSB_CFG_ARU_C11_CSBRANGE
    c. The corresponding register address in the configuration space (MMIO) is
       0x110084%0x200 + 0x800(base address in the configuration space) = 0x884
            >>> NV_XUSB_CFG_WRITE(CSB_ADDR, (RegOffet + 0x800));
    d. Read/write the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_DWRD0 register by issuing
       a PCI config read/write request at register address 0x884h.
            >>>> ?
    */
    // update PageNumber in usb3context
    if (Context->CurrentCSBPage != PageNum)
    {
        Context->CurrentCSBPage = PageNum;
        // update the NV_PROJ__XUSB_CFG_ARU_C11_CSBRANGE  with PageNum
        NV_XUSB_CFG_WRITE(ARU_C11_CSBRANGE, PageNum);
    }

    if (Update)
    {
        NV_XUSB_CFG_IND_WRITE((RegOffet + XUSB_CFG_CSB_ADDR_0), *value);
    }
    else
    {
        NV_XUSB_CFG_IND_READ((RegOffet + XUSB_CFG_CSB_ADDR_0), *value);
    }
}


/* Updated Boot Media port to Host controller port mapping
 *
    USB Boot Port 0x0 = XUSB Host Controller Port No. 3 = HSPI_PVTPORTSCx0
    USB Boot Port 0x1 = XUSB Host Controller Port No. 4 = HSPI_PVTPORTSCx1
    USB Boot Port 0x2 = XUSB Host Controller Port No. 5 (T124 only) = HSPI_PVTPORTSCx2

    USB Boot Port 0x7 = XUSB Host Controller Port No. 5 (T114) / No. 6 (T124)=
                             HSPI_PVTPORTSCx2 (T114) / = HSPI_PVTPORTSCx3 (T124)

    USB Boot Port 0x9 = XUSB Host Controller Port No. 6 (T114) / No. 7 (T124)=
                             HSPI_PVTPORTSCx3 (T114) / = HSPI_PVTPORTSCx4 (T124)
    USB Boot Port 0xA = XUSB Host Controller Port No. 7 (T114) / No. 8 (T124) =
                             HSPI_PVTPORTSCx4 (T114) / = HSPI_PVTPORTSCx5 (T124)
 *
 */
static NvBool BootPortControllerMapping(NvU8 BootPortNumber,
                                        NvU8* ControllerPortNumber)
{
    NvU8 BootHostControllerMapping[11] = {
                                          T114_USB_HOST_PORT_OTG0,
                                          T114_USB_HOST_PORT_OTG1,
                                          T114_RESERVED,
                                          0xF,
                                          0xF,
                                          0xF,
                                          0xF,
                                          T114_USB_HOST_PORT_ULPI,
                                          0xF,
                                          T114_USB_HOST_PORT_HSIC0,
                                          T114_USB_HOST_PORT_HSIC1,
                                         };
    // set to reserved.
    *ControllerPortNumber = T114_RESERVED;
    *ControllerPortNumber = BootHostControllerMapping[BootPortNumber];
    if (*ControllerPortNumber != T114_RESERVED)
        return NV_TRUE;
    else
        return NV_FALSE;
}

static NvError HwUsbIsDeviceAttached(NvddkXusbContext *Context )
{
    NvError ErrorCode = NvSuccess;
    NvU32 value = 0;
    NvU32 RegOffset = 0;
    NvU8 PortNumOffset = 0;
    NvU8 ControllerPortNumber = 0;

    if (!BootPortControllerMapping((Context->RootPortNum),&ControllerPortNumber))
    {
        return NvError_BadParameter;// not supported port.
    }
    // using the offset for portnum
    PortNumOffset = (ControllerPortNumber * PORT_INSTANCE_OFFSET);

    // check the register bits/root port no.,/csc/ccs of the port status for device attachment
    NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC3, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_PF_VAL(XUSB_CSB_HSPI_PVTPORTSC3_0, CCS , value);

    if (!value)
    {
        ErrorCode = NvError_XusbDeviceNotAttached;
    }
    return ErrorCode;
}

static NvError DevicePortInit(NvddkXusbContext *Context)
{
    NvError ErrorCode = NvSuccess;
    NvU32 value = 0;
    NvU32 RegOffset = 0;
    NvU8 PortNumOffset = 0;
    NvU8 ControllerPortNumber = 0;
    //reset should be asserted for 50ms anywhere between 1ms to 50ms, max timeout 100ms
    NvU32 TimeoutRetries = CONTROLLER_HW_RETRIES_2000;
    NvU32 TimeoutAttachDetachRetries = CONTROLLER_HW_RETRIES_2000;

#if NVBOOT_TARGET_RTL
    // XUSB_CSB_HSPI_PVTPORTSC3_0
    // Need to wait a max timeout value of 120000 clocks of 16.667ns before
    // device attachment is confirmed as described by bebick during debug.
    // To reduce the wait time wrie a value in XUSB_CFG_HSPX_CORE_CNT9_0,
    // say 1000 instead of 12000

    // hack only for sims to speed up.
    // select page before settign the value to the counter..
    NV_XUSB_CFG_READ(ARU_C11PAGESEL1, RegOffset);
    RegOffset = NV_FLD_SET_DRF_DEF(XUSB_CFG, ARU_C11PAGESEL1, HSP0, SEL, RegOffset);
    NV_XUSB_CFG_WRITE(ARU_C11PAGESEL1, RegOffset);
    // update the counter as mailed by bebick ..
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT5, 0x1C350);//0xC350
    // changing avlue from 11000 to 6000 as requested in bug
    // changing value from 6000 to 5000 after adjusting the osc-clk to be
    // exactly 13Mhz instead of 13.1,, count got misalligned
    // to 5998,, missing by 2.. hence setting it to 5000 as of now..
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT2, 0x1388);//0x1770) //0x2AF8
    // writing (7996) earlier programmed for 1000 to XUSB_CFG_HSPX_CORE_CNT9_0
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT9, 0x1F3C);


    // moving up due to page select..
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT1, 0xBB8);//0x4B0//0x5DC //0x1F3C
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT7, 0x1EE0);//0x4B0//0x1F3C
    // update for FS hacks
    // set bit20 and clear bit 12 (HS) for page select..
    NV_XUSB_CFG_READ(ARU_C11PAGESEL1, RegOffset);
    RegOffset = NV_FLD_SET_DRF_DEF(XUSB_CFG, ARU_C11PAGESEL1, HSP0, DESEL, RegOffset);
    RegOffset = NV_FLD_SET_DRF_DEF(XUSB_CFG, ARU_C11PAGESEL1, FSP0, SEL, RegOffset);
    NV_XUSB_CFG_WRITE(ARU_C11PAGESEL1, RegOffset);
    NV_XUSB_CFG_WRITE(HSPX_CORE_CNT3, 0x9C30);

    /* One write to setup the CSB_RANGE register
    For all NV_PROJ__XUSB_CSB_HSPI _PVTPORTSC* accesses, the port number is
    identified by USB port number register in FUSE_BOOT_DEVICE_INFO_0 register.
    Check for the device attachment by reading
    NV_PROJ__XUSB_CSB_HSPI _PVTPORTSC3_CCS field (should be '1')
    One write to clear 'NV_PROJ__XUSB_CSB_HSPI _PVTPORTSC2_CSC' field upon device detection.
    Wait 100ms.
    */
#endif

#if NVBOOT_TARGET_FPGA
    // set MFINT for debugging..
    NV_XUSB_CSB_EXPAND(ARU_MFINT, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = 0x0FEC0CC0;
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    NV_XUSB_CSB_EXPAND(CSB_RST_HSPI, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, CSB_RST_HSPI, 1, SET, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    TimeoutRetries = CONTROLLER_HW_RETRIES_2000;
    while (TimeoutRetries)
    {
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
        value = NV_PF_VAL(XUSB_CSB_CSB_RST_HSPI_0, 1, value);

        if (value == XUSB_CSB_CSB_RST_HSPI_0_1_NOT_PENDING)
            break;
        TimeoutRetries--;
        NvOsWaitUS(TIMEOUT_10US);
    }
        NvOsWaitUS(TIMEOUT_1MS);
#endif

#if NVBOOT_TARGET_FPGA
    if (Context->RootPortNum == 0)
    {
        /* Write 0 to RCV_TERM_GND
        nvpci pd e02005e4 12E
        nvpci pd e02005e0 220
        nvpci pd e02005e4 1000
        nvpci pd e02005e0 220
        nvpci pd e02005e0 124
        $output = nvpci pd e02005e8
        nvpci pd e02005e4 (($output>>16) & 0xFF7F) [right shift output by 16 bits and do
                                                    AND operation, basically changing
                                                    bit 7 to 0]
        nvpci pd e02005e0 224
        nvpci pd e02005e4 1012E
        nvpci pd e02005e0 220
        nvpci pd e02005e4 1000
        nvpci pd e02005e0 220
        */
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x12E);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x220);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1000);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x220);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x124);
        NV_XUSB_CFG_IND_READ(0x5e8, value);
        value = ((value >> 0x16) & 0xFF7F);
        NV_XUSB_CFG_IND_WRITE(0x5e4, value);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x224);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1012E);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x220);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1000);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x220);
    }
    else if (Context->RootPortNum == 1)
    {
        /*
        Write 0 to RCV_TERM_GND
        nvpci pd e02005e4 12E
        nvpci pd e02005e0 230
        nvpci pd e02005e4 1000
        nvpci pd e02005e0 230
        nvpci pd e02005e0 134
        $output = nvpci pd e02005e8
        nvpci pd e02005e4 (($output>>16) & 0xFF7F) [right shift output by 16 bits and do
                                                    AND operation, basically changing
                                                    bit 7 to 0]
        nvpci pd e02005e0 234
        nvpci pd e02005e4 1012E
        nvpci pd e02005e0 230
        nvpci pd e02005e4 1000
        nvpci pd e02005e0 230
        */
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x12E);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x230);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1000);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x230);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x134);
        NV_XUSB_CFG_IND_READ(0x5e8, value);
        value = ((value >> 0x16) & 0xFF7F);
        NV_XUSB_CFG_IND_WRITE(0x5e4, value);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x234);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1012E);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x230);
        NV_XUSB_CFG_IND_WRITE(0x5e4, 0x1000);
        NV_XUSB_CFG_IND_WRITE(0x5e0, 0x230);
    }

#endif
    // Enable sof count..
    NV_XUSB_CSB_EXPAND(ARU_CTRL, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, ARU_CTRL, MFCOUNT, RUN, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    if (!BootPortControllerMapping((Context->RootPortNum),&ControllerPortNumber))
    {
        return NvError_BadParameter;// not supported port.
    }
    // using the offset for portnum
    PortNumOffset = (ControllerPortNumber * PORT_INSTANCE_OFFSET);

    // as part of NoFw_boot_#7.doc udpates
    //    "    Write 'Disconnected' to NV_PROJ__XUSB_CSB_HSPI_PVTPORTSC1_PLS_CNTRL
    //Write '1' to NV_PROJ__XUSB_CSB_HSPI_PVTPORTSC1_PLS_VALID
    NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC1, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_CNTRL, DISCONNECTED, value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_VALID, SET, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    //"    Write 'Disconnected' to NV_PROJ__XUSB_CSB_FSPI_PVTPORTSC1_PLS_CNTRL
    // Write '1' to NV_PROJ__XUSB_CSB_FSPI_PVTPORTSC1_PLS_VALID
    NV_XUSB_CSB_EXPAND(FSPI_PVTPORTSC1, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_CNTRL, DISCONNECTED, value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_VALID, SET, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    while (TimeoutAttachDetachRetries)
    {
        NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC3, RegOffset);
        value = 0;
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
        value = NV_PF_VAL(XUSB_CSB_HSPI_PVTPORTSC3_0, CCS , value);

        if(value == XUSB_CSB_HSPI_PVTPORTSC3_0_CCS_DEV)
            break;
        TimeoutAttachDetachRetries--;

        NvOsWaitUS(TIMEOUT_10US);
    }

    if (!value && !TimeoutAttachDetachRetries)
    {
        return NvError_XusbDeviceNotAttached;
    }

    // One write to clear 'NV_PROJ__XUSB_CSB_HSPI _PVTPORTSC2_CSC' field upon device detection.
    // read & clear
    NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC2, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC2, CSC, CLEAR, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    //Wait 100ms.
#if NVBOOT_TARGET_RTL
    NvOsWaitUS(TIMEOUT_1MS);
#else
    NvOsWaitUS(TIMEOUT_100MS);
#endif

    // as part of NoFw_boot_#7.doc udpates
    //Write 'Reset' to USB3_CSB_FSPI_PVTPORTSC1_PLS_CNTRL register to reset the port.
    //Write '1' to NV_PROJ__XUSB_CSB_FSPI_PVTPORTSC1_PLS_VALID
    NV_XUSB_CSB_EXPAND(FSPI_PVTPORTSC1, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_VALID, SET, value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_CNTRL, RESET, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    /*
    Write 'Reset' to USB3_CSB_HSPI_PVTPORTSC1_PLS_CNTRL register to reset the port.
    Write '1' to NV_PROJ__XUSB_CSB_HSPI_PVTPORTSC1_PLS_VALID
    The reset operation is required for USB2.0 devices. The plan is to support
    USB2.0 mode only during boot.
    This corresponds to one register write request.
    Wait 50ms.
    */

    NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC1, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_VALID, SET, value);
    value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_CNTRL, RESET, value);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

    //Wait 50ms.
#if NVBOOT_TARGET_RTL
    NvOsWaitUS(TIMEOUT_1MS);
#else
    NvOsWaitUS(TIMEOUT_50MS);
#endif
    /* Confirm the port reset completion by looking at
    USB3_CSB_HSPI_PVTPORTSC3_PLS_STATUS == Enabled state (in Port instance)
    Boot rom needs to poll this field
    */
    NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC3, RegOffset);
    PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
    value = NV_DRF_VAL(XUSB_CSB, HSPI_PVTPORTSC3, PLS_STATUS, value);

    while (TimeoutRetries)
    {
        NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC3, RegOffset);
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
        value = NV_DRF_VAL(XUSB_CSB, HSPI_PVTPORTSC3, PLS_STATUS, value);

        if((value == XUSB_CSB_HSPI_PVTPORTSC3_0_PLS_STATUS_ENABLED) || \
           (value == XUSB_CSB_HSPI_PVTPORTSC3_0_PLS_STATUS_FS_MODE))
            break;
        TimeoutRetries--;
        // reset should be asserted for 50ms anywhere between 1ms to 50ms, max timeout 100ms
        NvOsWaitUS(TIMEOUT_1MS);// polling duration 1 ms
    }

    if (!TimeoutRetries)
        return NvError_InvalidState;//NvBootError_XusbPortResetFailed;

    // Wait 10ms for reset recovery.
    NvOsWaitUS(TIMEOUT_10MS);
    return ErrorCode;
}

static NvError NvBootXusbPrepareControlSetupTRB(NvddkXusbContext *Context,
                                                DeviceRequest *DeviceRequestPtr,
                                                NvBootTRTType *TrttTyp)
{
    SetupTRB *SetupTRBPtr = NULL;
    NvError ErrorCode = NvSuccess;

    Context->TRBRingCtrlEnquePtr = (TRB*)(Context->pCtxMemAddr + TRBRING_OFFSET);
    SetupTRBPtr = (SetupTRB *)(Context->TRBRingCtrlEnquePtr);

    //To make sure that all reserved bits are set to 0
    NvOsMemset((void *)SetupTRBPtr,0,sizeof(SetupTRB));
    switch (DeviceRequestPtr->bRequest)
    {
        case GET_DESCRIPTOR      :
        case GET_CONFIGURATION   :
        case SET_ADDRESS         :
        case SET_CONFIGURATION   :
        case GET_MAX_LUN         :
        case CLEAR_FEATURE       : break;

        default                  : ErrorCode = NvError_XusbInvalidBmRequest;
        break;
    }
    if (ErrorCode == NvSuccess)
    {
        SetupTRBPtr->bmRequestType = DeviceRequestPtr->bmRequestTypeUnion.bmRequestType;
        SetupTRBPtr->bmRequest     = DeviceRequestPtr->bRequest;
        SetupTRBPtr->wValue        = DeviceRequestPtr->wValue;
        SetupTRBPtr->wIndex        = DeviceRequestPtr->wIndex;
        SetupTRBPtr->wLength       = DeviceRequestPtr->wLength;
        SetupTRBPtr->TRBTfrLen     = USB_SETUP_PKT_SIZE;   //Tansfer 8 bytes

        /* Since Memory area for this TRB is already initialized to 0,
         * there is no need to program these fields
         * SetupTRBPtr->InterrupterTarget      =  0;
         * SetupTRBPtr->IOC                    =  0;
         */
        SetupTRBPtr->CycleBit      = Context->CycleBit;
        SetupTRBPtr->IDT           = 1;                   //Immediate
        SetupTRBPtr->TRBType       = NvBootTRB_SetupStage;
        if (DeviceRequestPtr->wLength != 0)
        {
            if (DeviceRequestPtr->bmRequestTypeUnion.stbitfield.dataTransferDirection ==
                DEV2HOST)
            {
                *TrttTyp = NvBootTRT_InDataStage;
            }
            else
            {
                //HOST2DEV
                *TrttTyp = NvBootTRT_OutDataStage;
            }
        }
        else
        {
            *TrttTyp = NvBootTRT_NoDataStage;
        }
        SetupTRBPtr->TRT = *TrttTyp;
    }
    //update TRB pointer
    (Context->TRBRingCtrlEnquePtr)++;
    return ErrorCode;
}


static void NvBootXusbPrepareControlDataTRB(NvddkXusbContext *Context,
                                            DeviceRequest *DeviceRequestPtr)
{
    DataStageTRB *DataStageTRBPtr = (DataStageTRB *)(Context->TRBRingCtrlEnquePtr);

    //To make sure that all reserved bits are set to 0
    NvOsMemset((void *)DataStageTRBPtr,0,sizeof(DataStageTRB));

    DataStageTRBPtr->DataBufferLo       = (NvU32)(BufferXusbData);
    DataStageTRBPtr->TRBTfrLen          = DeviceRequestPtr->wLength;
    /* No need to set to 0 since this memory area is already set to 0
    DataStageTRBPtr->DataBufferHi       = 0;
    DataStageTRBPtr->TDSize             = 0;
    DataStageTRBPtr->InterrupterTarget  = 0;
    DataStageTRBPtr->CycleBit           = 0;
    DataStageTRBPtr->ENT                = 0;
    DataStageTRBPtr->ISP                = 0;
    DataStageTRBPtr->NS                 = 0;
    DataStageTRBPtr->IOC                = 0;
    DataStageTRBPtr->IDT                = 0;
    */
    DataStageTRBPtr->CycleBit           = Context->CycleBit;
    DataStageTRBPtr->TRBType            = NvBootTRB_DataStage;

    /*Logic below is optimized to just one line of code since
    USB_DIR_OUT matches HOST2DEV enum
    USB_DIR_IN  matches DEV2HOST enum
    */
    DataStageTRBPtr->DIR                = DeviceRequestPtr->bmRequestTypeUnion.\
                                                stbitfield.dataTransferDirection;
    //update TRB pointer
    (Context->TRBRingCtrlEnquePtr)++;
    return;
}


static void NvBootXusbPrepareControlStatusTRB(NvddkXusbContext *Context,
                                              DeviceRequest *DeviceRequestPtr)
{
    StatusStageTRB *StatusStageTRBPtr = (StatusStageTRB *)(Context->TRBRingCtrlEnquePtr);

    //To make sure that all reserved bits are set to 0
    NvOsMemset((void *)StatusStageTRBPtr,0,sizeof(StatusStageTRB));

    StatusStageTRBPtr->TRBType      = NvBootTRB_StatusStage;
    StatusStageTRBPtr->DIR          = !(DeviceRequestPtr->bmRequestTypeUnion.\
                                              stbitfield.dataTransferDirection);
    StatusStageTRBPtr->CycleBit     = Context->CycleBit;

    /*No need to set to 0 as this memory area is already set to 0
    StatusStageTRBPtr->InterrupterTarget    = 0;
    StatusStageTRBPtr->CycleBit             = 0;
    StatusStageTRBPtr->ENT                  = 0;
    StatusStageTRBPtr->CH                   = 0;
    StatusStageTRBPtr->IOC                  = 0;
    */
    //update TRB pointer
    (Context->TRBRingCtrlEnquePtr)++;
    return;
}

void NvBootXusbPrepareEndTRB(NvddkXusbContext *Context)
{
    StatusStageTRB *EndStageTRBPtr = (StatusStageTRB *)(Context->TRBRingCtrlEnquePtr);

    //To make sure that all reserved bits are set to 0
    NvOsMemset((void *)EndStageTRBPtr,0,sizeof(StatusStageTRB));

    EndStageTRBPtr->TRBType      = NvBootTRB_Reserved;
    EndStageTRBPtr->CycleBit     = !(Context->CycleBit);
    //update TRB pointer
    Context->TRBRingCtrlEnquePtr = (TRB*)(Context->pCtxMemAddr + TRBRING_OFFSET);
}


NvError NvBootXusbProcessEpStallClearRequest(NvddkXusbContext *Context,
                                             USB_Endpoint_Type EpType )
{
    NvError ErrorCode = NvSuccess;
    NvBootTRTType TrttTyp = NvBootTRT_NoDataStage;
    DeviceRequest DeviceRequestVar;

    // Clear endpoint stall
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType = HOST2DEV_ENDPOINT;// 0x2.
    DeviceRequestVar.bRequest = CLEAR_FEATURE;
    DeviceRequestVar.wValue = 0; // feature selector.. ENDPOINT_HALT (0x00
    if (EpType == USB_CONTROL_BI)
    {
        DeviceRequestVar.wIndex = 0;    //wIndex field endpoint no., and diretion
    }
    else if (EpType == USB_BULK_OUT)
    {
        //wIndex field endpoint no., and diretion
        DeviceRequestVar.wIndex = (Context->EPNumAndPacketSize[USB_DIR_OUT].EndpointNumber);
    }
    else
    {
        NV_ASSERT(EpType == USB_BULK_IN);
        DeviceRequestVar.wIndex =
            ((Context->EPNumAndPacketSize[USB_DIR_IN].EndpointNumber) |= \
                                                    ENDPOINT_DESC_ADDRESS_DIR_IN);
    }
    DeviceRequestVar.wLength = 0; //Number of bytes to return

    ErrorCode = NvBootXusbPrepareControlSetupTRB(Context,&DeviceRequestVar,&TrttTyp);
    if (ErrorCode == NvSuccess)
    {
        if (TrttTyp != NvBootTRT_NoDataStage)
        {
            //data stage
            NvBootXusbPrepareControlDataTRB(Context,&DeviceRequestVar);
        }
        NvBootXusbPrepareControlStatusTRB(Context,&DeviceRequestVar);
        // Last TRB with Cycle bit modified for End TRB.
        NvBootXusbPrepareEndTRB(Context);
        NvBootXusbUpdateEpContext(Context,USB_CONTROL_BI);
        ErrorCode = NvBootXusbUpdWorkQAndChkCompQ(Context);
        if (ErrorCode == NvError_XusbEpStalled)
            ErrorCode = NvError_XusbDeviceResponseError;
    }
    return ErrorCode;
}

static NvError NvBootXusbProcessControlRequest(NvddkXusbContext *Context,
                                               DeviceRequest *DeviceRequestPtr)
{
    NvError ErrorCode = NvSuccess;
    NvBootTRTType TrttTyp = NvBootTRT_NoDataStage;
    NvU32 RetryCount = USB_MAX_TXFR_RETRIES;

    while (RetryCount)
    {
        ErrorCode = NvBootXusbPrepareControlSetupTRB(Context,
                                                     DeviceRequestPtr,
                                                     &TrttTyp);
        if (ErrorCode == NvSuccess)
        {
            if (TrttTyp != NvBootTRT_NoDataStage)
            {
                //data stage
                NvBootXusbPrepareControlDataTRB(Context,DeviceRequestPtr);
            }
            NvBootXusbPrepareControlStatusTRB(Context,DeviceRequestPtr);
            // Last TRB with Cycle bit modified for End TRB.
            NvBootXusbPrepareEndTRB(Context);
            NvBootXusbUpdateEpContext(Context,USB_CONTROL_BI);
            ErrorCode = NvBootXusbUpdWorkQAndChkCompQ(Context);
            if (ErrorCode == NvSuccess)
                break;
            if (ErrorCode == NvError_XusbEpStalled)
            {
                // get ep dir value
                ErrorCode = NvBootXusbProcessEpStallClearRequest(Context, USB_CONTROL_BI);
                break;
            }
            else
            {
                RetryCount--;
            }
        }
    }
    return ErrorCode;
}

static void InitializeEpContext(NvddkXusbContext *Context)
{
    EP *EpContext = Context->EndPtContext;

    NvOsMemset((void*)EpContext,0x0, sizeof(EP));
    //As Input, this field is initialized to ‘0’ by software.
    //    EpContext->EpDw0.EPState = 0;
    //    EpContext->EpDw8.ScratchPad = 0;// what shoudl be value ?
    //    EpContext->EpDw9.ScratchPad = 0;// what shoudl be value ?
    //    EpContext->EpDw10.RO = 0;// what shoudl be value ?
    //    EpContext->EpDw14.END = 0;
    //    EpContext->EpDw15.EndpointLinkHo = 0; // only the upper 4 bits
    //    EpContext->EpDw12.LPU = 0;// endpoint linked list poitner?

    EpContext->EpDw0.Interval       = ONE_MILISEC;          // 1 ms
    EpContext->EpDw1.CErr           = USB_MAX_TXFR_RETRIES; // max value
    //For High-Speed control and bulk endpoints this field shall be cleared to ‘0’.
    EpContext->EpDw1.MaxBurstSize   = MAX_BURST_SIZE;
    EpContext->EpDw6.CErrCnt = EpContext->EpDw1.CErr; // AS SET IN EpContext->EpDw1.CErr

    //Initialized to Max Burst Size of the Endpoint by firmware
    EpContext->EpDw7.NumP = MAX_BURST_SIZE;
    //This is the scratchpad used by FW to store EP-related information
    EpContext->EpDw10.TLM = SYSTEM_MEM;           // assuming system mem
    EpContext->EpDw10.DLM = SYSTEM_MEM;           // assuming system mem

    EpContext->EpDw11.HubAddr     = Context->HubAddress;  //from context..
    EpContext->EpDw11.RootPortNum = Context->RootPortNum; //from context..

    EpContext->EpDw12.Speed = HIGH_SPEED;

    EpContext->EpDw14.MRK = 1;
    EpContext->EpDw14.ELM = SYSTEM_MEM;// IRAM
    // For No FW boot, since we will have only one endpoint context active at a time,
    // link pointer should point to itself
    // (The address at which the endpoint context is created).
    // 28 bit lsb of the TR
    EpContext->EpDw14.EndpointLinkLo = ((NvU32)(Context->EndPtContext) >> 4);
}

void NvBootXusbUpdateEpContext( NvddkXusbContext *Context, USB_Endpoint_Type EpType)
{
    EP *EpContext = Context->EndPtContext;

    // Assert on Not supported EpType
    NV_ASSERT((EpType == USB_CONTROL_BI) ||
              (EpType == USB_BULK_IN)    ||
              (EpType == USB_BULK_OUT));

    EpContext->EpDw0.EPState            = USB_EP_STATE_RUNNING;
    EpContext->EpDw1.EPType             = EpType;
    EpContext->EpDw2.DCS                = Context->CycleBit;
    // 28 bit lsb of the TR
    EpContext->EpDw2.TRDequeuePtrLo     = ((NvU32)(Context->TRBRingCtrlEnquePtr) >> 4);
    EpContext->EpDw3.TRDequeuePtrHi     = 0; // only the upper 4 bits
    //FW(DW10) = Set to 1 for SET_ADDRESS request. 0 for all other cases.
    //0 for SET_ADDRESS. Assigned address later on.
    EpContext->EpDw11.DevAddr           = Context->stEnumerationInfo.DevAddr;
    EpContext->EpDw10.FW                = Context->SetAddress;
    // clear status
    EpContext->EpDw6.Status             = 0;
    EpContext->EpDw7.DataOffset         = 0;

    // This field represents the endpoint number on the device for which
    // transfer needs to be done. For control endpoint, DCI should be 1.
    // For Bulk IN/OUT endpoints it should have the corresponding endpoint numbers
    // as read from the device descriptor. BY DEFAULT DCI is set for control endpoint
    if (EpType != USB_CONTROL_BI)
    {
        // not control endpoint
        if (EpType == USB_BULK_IN)
        {
            EpContext->EpDw12.DCI =
                ((Context->EPNumAndPacketSize[USB_DIR_IN].EndpointNumber) * 2 + 1);
            EpContext->EpDw5.SEQNUM = Context->BulkSeqNumIn;
        }
        else if (EpType == USB_BULK_OUT)
        {
            EpContext->EpDw12.DCI =
                ((Context->EPNumAndPacketSize[USB_DIR_OUT].EndpointNumber) * 2);
            EpContext->EpDw5.SEQNUM = Context->BulkSeqNumOut;
        }

        // max packet size is byte swapped.. need to check..
        // EpContext->EpDw1.MaxPacketSize = Context->EPNumAndPacketSize[tmp].PacketSize;
        EpContext->EpDw1.MaxPacketSize = USB_TRB_AVERAGE_BULK_LENGTH;
        EpContext->EpDw4.AverageTRBLength = USB_TRB_AVERAGE_BULK_LENGTH;
    }
    else
    {
        //control endpoint
        EpContext->EpDw12.DCI             = DCI_CTRL; //set it to default value
        EpContext->EpDw1.MaxPacketSize    = Context->stEnumerationInfo.bMaxPacketSize0;
        EpContext->EpDw4.AverageTRBLength = USB_TRB_AVERAGE_CONTROL_LENGTH;
        EpContext->EpDw5.SEQNUM           = 0;
    }
    return;
}

static void InitializeContexts(NvddkXusbContext *Context, NvU8 RootPortNum)
{
    BufferXusbCmd = (NvU8*)(Context->pCtxMemAddr + CMD_BUF_OFFSET);
    BufferXusbStatus = (NvU8*)(Context->pCtxMemAddr + STS_BUF_OFFSET);
    BufferXusbData = (NvU8*)(Context->pCtxMemAddr + DATA_BUF_OFFSET);

    //Initialize the Context data structure.
    //Refer to xHCI spec for Endpoint context data structure
    Context->EndPtContext        = (EP*)(Context->pCtxMemAddr + EP_CTX_OFFSET);
    //Always point to start of TRB ring. Since the driver is operating in polling mode
    //there is always one request enqued
    Context->TRBRingEnquePtr     = (TRB*)(Context->pCtxMemAddr + TRBRING_OFFSET);
    //used purely by driver. Required to prepare control, data and status TRB
    Context->TRBRingCtrlEnquePtr = (TRB*)(Context->pCtxMemAddr + TRBRING_OFFSET);
    //Needed by xHCI controller. Look at Endpoint context data structure
    Context->TRBRingDequePtr     = (TRB*)(Context->pCtxMemAddr + TRBDEQ_OFFSET);

    //Control packet size before get descriptor device is issued.
    Context->stEnumerationInfo.bMaxPacketSize0 = USB_HS_CONTROL_MAX_PACKETSIZE;

    Context->RootPortNum         = RootPortNum;
    //Flag to indicate that set address request is not yet sent or successfully executed
    Context->SetAddress          = NV_FALSE;

    InitializeEpContext(Context);
    return;
}


static void NvBootXusbUpdateWorkQ(NvddkXusbContext *Context)
{
    NvU32 RegOffset =0;
    NvU32 value = 0;

    // program the PTRHI with 4 bit of MSB EndpointContext
    // 1. #define NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD2_PTRHI
    NV_XUSB_CSB_EXPAND(HS_BI_WORKQ_DWRD2, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);

    // set the KIND for DWORD 0
    //#define NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD0_KIND_EPTLIST_BULK_INOUT
    NV_XUSB_CSB_EXPAND(HS_BI_WORKQ_DWRD0, RegOffset);
    value = XUSB_CSB_HS_BI_WORKQ_DWRD0_0_KIND_EPTLIST_BULK_INOUT;
    PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);

    // lastly program the #define NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD1
    // #define NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD1_ELM
    //#define NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD1_PTRLO
    NV_XUSB_CSB_EXPAND(HS_BI_WORKQ_DWRD1, RegOffset);
    value = (NvU32)(Context->EndPtContext);
    PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);

    Context->Usb3Status.XusbDriverStatus  = NvddkXusbStatus_WorkQSubmitted;
}


static NvError NvBootXusbCheckCompQ(NvddkXusbContext *Context)
{
    NvError e = NvSuccess;
    NvU32 RegOffset =0;
    NvU32 value = 0;
    NvU32 timeout = 0;
    NvU32 EPstatus = 0;
    USB_Endpoint_Type EpType = USB_NOT_VALID;
    EP *EndPtContext = Context->EndPtContext;
    NvU32 StallErr = 0;

    Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollStart;

    // check EP context for status and then check complq register for updates.
    // timeout for 50 microseconds
    // while debugging on fpga this code timeout.. hence increasing this to 1 sec.
    timeout = TIMEOUT_1S;
    while (timeout)
    {
        // poll for completion status until
        // NV_PROJ__XUSB_CSB_SS_BI_COMPLQ_DWRD3_STATUS_COMPL_RETIRE or
        EPstatus = (NvU32)(EndPtContext->EpDw6.Status);
        // check for completion status enum are not defined for HS intentionally
        // since the same is defined in SS.
        if ((EPstatus != XUSB_CSB_SS_BI_COMPLQ_DWRD3_0_STATUS_NONE) &&
            (EPstatus <  XUSB_CSB_SS_BI_COMPLQ_DWRD3_0_STATUS_RESCH_CSW))
            break;
        NvOsWaitUS(TIMEOUT_1US);
        timeout--;
    }

    // check for completion status with SS enums
    if ((EPstatus != XUSB_CSB_SS_BI_COMPLQ_DWRD3_0_STATUS_COMPL_RETIRE) && (!timeout))
    {
        // update another status entry for NvddkXusbStatus_CompQPollError
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollError;

        // popping the entry to clear for next transfer..
        NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);

        return NvError_Timeout;
    }
    if (EPstatus == XUSB_CSB_SS_BI_COMPLQ_DWRD3_0_STATUS_ERR_STALL)
    {
        StallErr = 1;
    }
    // The FW should check the NV_PROJ__XUSB_CSB_HSBI_COMPLQ_CNTRL_VALID bit

    // while debugging on fpga this code timeout.. hence increasing this to 1 sec.
    timeout = CONTROLLER_HW_RETRIES_1SEC;
    while (timeout)
    {
        NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_PF_VAL(XUSB_CSB_HS_BI_COMPLQ_CNTRL_0, VALID , value);
        if(value)
            break;
        timeout--;
        NvOsWaitUS(TIMEOUT_1US);
    }

    if ((!value) && (!timeout))
    {
        // update another status entry for NvddkXusbStatus_CompQPollError
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollError;
        return NvError_Timeout;
    }

    // check COMPLQ_DWRD0_SUBKIND until
    // SUBKIND_OP_DONE || SUBKIND_EPT_ERROR || SUBKIND_EPT_DONE
    // NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_DWRD0_SUBKIND
    NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_DWRD0, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
    value = NV_PF_VAL(XUSB_CSB_HS_BI_COMPLQ_DWRD0_0, SUBKIND , value);

    //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.

    if (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_EPT_NRDY)
    {
        e = NvError_XusbEpNotReady;
        NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollEnd;

        // while debugging on fpga this code timeout..hence increasing this to 1 sec.
        timeout = CONTROLLER_HW_RETRIES_1SEC;
        while (timeout)
        {
            NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_DWRD0, RegOffset);
            PciRegUpdate(Context, RegOffset, XUSB_READ, &value);

            // check value for no activity!!
            value = NV_PF_VAL(XUSB_CSB_HS_BI_COMPLQ_DWRD0_0, SUBKIND , value);

            if (value != XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_EPT_NRDY)
            {
                //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.
                NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
                PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
                value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
                PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
                break;
            }
            timeout--;
            NvOsWaitUS(TIMEOUT_10US);
        }
        if (!timeout)
        {
            e = NvError_XusbEpNotReady;
        }
        else
        {
            e = NvSuccess;
        }
    }
    else if ((value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_OP_ERROR) ||
             (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_STOPREQ))
    {
        e =  NvError_XusbDeviceResponseError;// should be controller specific CHECK
    }
    else if (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_EPT_ERROR)
    {
        // only on XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_EPT_ERROR perform error handling!!
        e = NvError_XusbEpError;// perform error handling!!
    }

    // readh NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_DWRD1
    // NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_DWRD1_EPT_PTRLO
    // NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_DWRD1_EPT_PTRHI
    if (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_EPT_DONE)
    {
        //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.
        NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollEnd;

        e = NvSuccess;

        // while debugging on fpga this code timeout.. hence increasing this to 1 sec.
        timeout = CONTROLLER_HW_RETRIES_1SEC;
        while (timeout)
        {
            NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_DWRD0, RegOffset);
            PciRegUpdate(Context, RegOffset, XUSB_READ, &value);

            // check value for no activity!!
            value = NV_PF_VAL(XUSB_CSB_HS_BI_COMPLQ_DWRD0_0, SUBKIND , value);

            if (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_NO_ACTIVITY)
            {
                //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.
                NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
                PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
                value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
                PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
                break;
            }
            timeout--;
            NvOsWaitUS(TIMEOUT_1US);
        }

        if (!timeout)
        {
            e = NvError_Timeout;
        }
        else
        {
            //update sequence no.,
            EpType = (USB_Endpoint_Type)(EndPtContext->EpDw1.EPType);

            if (EpType == USB_BULK_OUT)
            {
                if (StallErr)
                    Context->BulkSeqNumOut = 0;
                else
                    Context->BulkSeqNumOut = (NvU32)(EndPtContext->EpDw5.SEQNUM);
            }
            else if (EpType == USB_BULK_IN)
            {
                if (StallErr)
                    Context->BulkSeqNumIn = 0;
                else
                    Context->BulkSeqNumIn= (NvU32)(EndPtContext->EpDw5.SEQNUM);
            }
        }
    }
    else
    {
        // should not be here..
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollError;
        //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.
        NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollEnd;

        // while debugging on fpga this code timeout.. hence increasing this to 1 sec.
        timeout = CONTROLLER_HW_RETRIES_1SEC;
        while (timeout)
        {
            NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_DWRD0, RegOffset);
            PciRegUpdate(Context, RegOffset, XUSB_READ, &value);

            // check value for no activity!!
            value = NV_PF_VAL(XUSB_CSB_HS_BI_COMPLQ_DWRD0_0, SUBKIND , value);

            if (value == XUSB_CSB_HS_BI_COMPLQ_DWRD0_0_SUBKIND_NO_ACTIVITY)
            {
                //Program the NV_PROJ__XUSB_CSB_HS_BI_COMPLQ_CNTRL_POP bit to '1'.
                NV_XUSB_CSB_EXPAND(HS_BI_COMPLQ_CNTRL, RegOffset);
                PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
                value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HS_BI_COMPLQ_CNTRL,POP, SET, value);
                PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
                break;
            }
            timeout--;
            NvOsWaitUS(TIMEOUT_1US);
        }

        if (!timeout)
        {
            e = NvError_Timeout;
        }
    }
    // delay between ep status done to iram content access
    NvOsWaitUS(TIMEOUT_10US);
    if(StallErr)
        e = NvError_XusbEpStalled;
    return e;
}


static NvError UsbTransferErrorHandling(NvddkXusbContext *Context)
{
    NvError e = NvSuccess;
    NvU32 RegOffset =0;
    NvU32 value = 0;
    NvU32 timeout = 0;
    TRB EventTrb;
    EventTRB EventDataTrb;
    // The BI write a transfer event TRB to the common event queue upon any
    //error condition in a transaction. It is the responsibility of the bootrom
    // code to read this queue upon error detection in the completion status.
    //Possible error completion codes include "Babble Detected Error",
    // "USB Transaction Error", and "Stall Error

    Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_EventQPollStart;

    // poll NV_PROJ__XUSB_CSB_ EVENTQ_CNTRL_VALID to check for a valid entry
    // while debugging on fpga this code timeout.. hence increasing this to 1 sec.
    timeout = CONTROLLER_HW_RETRIES_1SEC;
    while (timeout)
    {
        NV_XUSB_CSB_EXPAND(EVENTQ_CNTRL1, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_PF_VAL(XUSB_CSB_EVENTQ_CNTRL1_0, VALID , value);

        if (value)
            break;
        timeout--;
        NvOsWaitUS(TIMEOUT_1US);
    }

    if ((!value) && (!timeout))
    {
        // update another status entry for NvddkXusbStatus_CompQPollError
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_CompQPollError;
        return NvError_Timeout;
    }

    // Read NV_PROJ__XUSB_CSB_ EVENTQ_TRBDWRD0-3 to get the transfer event TRB.
    NV_XUSB_CSB_EXPAND(EVENTQ_TRBDWRD0, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_READ, &EventTrb.DataBuffer[0]);

    NV_XUSB_CSB_EXPAND(EVENTQ_TRBDWRD1, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_READ, &EventTrb.DataBuffer[1]);

    NV_XUSB_CSB_EXPAND(EVENTQ_TRBDWRD2, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_READ, &EventTrb.DataBuffer[2]);

    NV_XUSB_CSB_EXPAND(EVENTQ_TRBDWRD3, RegOffset);
    PciRegUpdate(Context, RegOffset, XUSB_READ, &EventTrb.DataBuffer[3]);

    // mapping the data event trb
    NvOsMemcpy((void *)(&EventDataTrb), (void*)(&EventTrb), sizeof(TRB));

    if (EventDataTrb.TRBType != NvBootTRB_EventData)
    {
        // not expected type TRB.
        NV_XUSB_CSB_EXPAND(EVENTQ_CNTRL1, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, EVENTQ_CNTRL1,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);
        Context->Usb3Status.EpStatus = NvBootComplqCode_USB_TRANS_ERR;
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_EventQPollError;
        //error retry..
        e = NvError_XusbEpRetry;
    }
    else
    {
        // Set NV_PROJ__XUSB_CSB_ EVENTQ_CNTRL_POP to '1' to remove event TRB from the queue.
        NV_XUSB_CSB_EXPAND(EVENTQ_CNTRL1, RegOffset);
        PciRegUpdate(Context, RegOffset, XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, EVENTQ_CNTRL1,POP, SET, value);
        PciRegUpdate(Context, RegOffset, XUSB_WRITE , &value);

        // check for the event type to NvBootTRB_TransferEvent
        // update the exact error type as described in 6.4.5 xhci spec.
        Context->Usb3Status.XusbDriverStatus = NvddkXusbStatus_EventQPollEnd;
        // update complettion code
        // report complettion code error status
        Context->Usb3Status.EpStatus = EventDataTrb.ComplCode;
    }
    return e;
}


NvError NvBootXusbUpdWorkQAndChkCompQ(NvddkXusbContext *Context)
{
    NvError e = NvSuccess;

    // update workQ
    NvBootXusbUpdateWorkQ(Context);

    NvOsWaitUS(TIMEOUT_10US);

    // poll for compq
    e = NvBootXusbCheckCompQ(Context);

    if (e == NvError_XusbEpError)
    {
        // Error handling code
        e = UsbTransferErrorHandling(Context);
        if ((e == NvSuccess) &&
            (Context->Usb3Status.XusbDriverStatus == NvddkXusbStatus_EventQPollEnd))
        {
            if (Context->Usb3Status.EpStatus  == NvBootComplqCode_STALL_ERR)
            {
                // clear endpoint stall
                e = NvError_XusbEpStalled;
            }
            else if(Context->Usb3Status.EpStatus  == NvBootComplqCode_USB_TRANS_ERR)
            {
                e = NvError_XusbEpRetry;
            }
        }
    }
    else if ((e == NvError_Timeout )                ||
             (e == NvError_XusbDeviceResponseError) ||
             (e == NvError_XusbEpNotReady))
    {
        // hw timeout/EP not ready/ controller error
        // retry the transfer
        e = NvError_XusbEpRetry;
    }
    return e;
}


static NvError ParseConfigurationDescriptor(NvddkXusbContext *Context)
{
    NvError ErrorCode = NvError_XusbParseConfigDescFail;
    UsbInterfaceDescriptor *UsbInterfaceDescriptorPtr;
    UsbEndpointDescriptor  *UsbEndpointDescriptorPtr;
    NvU32 NumInterfaces, NumEndPoints, i,j,EndpointDir;
    NvBool EndpointInitStatus[2];

    //Initialize to 0
    NumEndPoints = 0;
    // Get ptr to interface descriptor
    UsbInterfaceDescriptorPtr = (UsbInterfaceDescriptor *)( ((NvU8 *)(BufferXusbData)) +
                                                    UsbConfigDescriptorStructSz );
    //get number of supported interfaces
    NumInterfaces =((UsbConfigDescriptor *)(BufferXusbData))->bNumInterfaces;
    //Get endpoint descriptor for current interface descriptor
    UsbEndpointDescriptorPtr  =
    (UsbEndpointDescriptor *)(((NvU8 *)UsbInterfaceDescriptorPtr) +
                                                UsbInterfaceDescriptorStructSz);

    //Check end point descriptors for a given interface till bulk only IN and OUT
    //type endpoints are found
    //LOOP1
    for (i = 0; ((i < NumInterfaces) && (ErrorCode != NvSuccess)); i++)
    {
        //get next interface descriptor pointer. Interface Descriptor is followed by
        //array of Endpoint descriptor of size NumEndPoints
        //Value of NumEndPoints used is the value from previous interface Descriptor.
        //In the beginining of loop it is 0
        UsbInterfaceDescriptorPtr =
            (UsbInterfaceDescriptor *)(((NvU8 *)UsbInterfaceDescriptorPtr) +
                                   NumEndPoints * UsbEndpointDescriptorStructSz);
        //Get supported number of endpoints for current interface descriptor
        NumEndPoints              = UsbInterfaceDescriptorPtr->bNumEndpoints;
        //Get endpoint descriptor for current interface descriptor
        UsbEndpointDescriptorPtr  =
            (UsbEndpointDescriptor *)(((NvU8 *)UsbInterfaceDescriptorPtr) +
                                                 UsbInterfaceDescriptorStructSz);

        //hacking interface descriptor since it is reporting
        //class/subclass & protocol as 0xFF
        if (UsbInterfaceDescriptorPtr->bInterfaceClass != INTERFACE_CLASS_MASS_STORAGE )
            return NvError_XusbParseConfigDescFail;

        if ((UsbInterfaceDescriptorPtr->bInterfaceProtocol != \
                                    INTERFACE_PROTOCOL_BULK_ONLY_TRANSPORT) && \
            (UsbInterfaceDescriptorPtr->bInterfaceProtocol != \
                                    INTERFACE_PROTOCOL_BULK_ZIP_TRANSPORT))
            return NvError_XusbParseConfigDescFail;

        //if current interface class is mass storage and interface protocol is bulk
        //only transport then check endpoint descriptor for valid data else check next
        //interface descriptor if there is any
        if ((UsbInterfaceDescriptorPtr->bInterfaceClass ==     \
                                          INTERFACE_CLASS_MASS_STORAGE) && \
            ((UsbInterfaceDescriptorPtr->bInterfaceProtocol == \
                                INTERFACE_PROTOCOL_BULK_ONLY_TRANSPORT) || \
             (UsbInterfaceDescriptorPtr->bInterfaceProtocol == \
                                INTERFACE_PROTOCOL_BULK_ZIP_TRANSPORT)))
        {
            //Analyze Endpoint Descriptor
            //See if two end points one of type bulk in and other of type bulk out is found
            EndpointInitStatus[USB_DIR_OUT] = NV_FALSE;
            EndpointInitStatus[USB_DIR_IN]  = NV_FALSE;

            //LOOP2
            for (j = 0; j < NumEndPoints; j++)
            {
                if (UsbEndpointDescriptorPtr->bmAttributes == \
                                            ENDPOINT_DESC_ATTRIBUTES_BULK_TYPE)
                {
                    //Bulk type end point
                    EndpointDir = (UsbEndpointDescriptorPtr->bEndpointAddress) & \
                                                    ENDPOINT_DESC_ADDRESS_DIR_MASK;
                    if (EndpointDir == ENDPOINT_DESC_ADDRESS_DIR_IN)
                    {
                        EndpointDir = USB_DIR_IN;
                    }
                    else
                    {
                        EndpointDir = USB_DIR_OUT;
                    }
                    Context->EPNumAndPacketSize[EndpointDir].PacketSize =
                                    (UsbEndpointDescriptorPtr->wMaxPacketSize);
                    Context->EPNumAndPacketSize[EndpointDir].EndpointNumber =
                        (UsbEndpointDescriptorPtr->bEndpointAddress) & \
                        ENDPOINT_DESC_ADDRESS_ENDPOINT_MASK;
                    EndpointInitStatus[EndpointDir] = NV_TRUE;

                    if ((EndpointInitStatus[USB_DIR_OUT]== NV_TRUE) && \
                        (EndpointInitStatus[USB_DIR_IN] == NV_TRUE))
                    {
                        //Found IN as well as OUT endpoint
                        //Log interface number
                        Context->stEnumerationInfo.InterfaceIndx =
                                    UsbInterfaceDescriptorPtr->bInterfaceNumber;
                        // This error code is required for calling function as well as
                        // for coming out of LOOP1
                        ErrorCode = NvSuccess;
                        break; // Come out of LOOP2
                    }
                }
                //Check next endpoint descriptor
                UsbEndpointDescriptorPtr =
                    (UsbEndpointDescriptor *)((NvU8 *)UsbEndpointDescriptorPtr + \
                                              UsbEndpointDescriptorStructSz);
            }
        }
        else
        {
            //Check next interface if there is any
            continue;
        }
    }
    return ErrorCode;
}

// Initialize scsi commands (inquiry, read capacity/capacities, request sense,
// mode sense & test unit ready)
static NvError InitializeScsiMedia(NvddkXusbContext *Context)
{
    NvError e = NvSuccess;

    // Issue Inquiry Command
    NV_CHECK_ERROR(NvBootXusbMscBotProcessRequest(Context, INQUIRY_CMD_OPCODE));

    // test unit ready
    e = NvBootXusbMscBotProcessRequest(Context, TESTUNITREADY_CMD_OPCODE);

    if (e == NvSuccess)
    {
        //Read Capacity
        e  = NvBootXusbMscBotProcessRequest(Context, READ_CAPACITY_CMD_OPCODE);

        if (e == NvSuccess)
        {
            // Test unit command is issued for check the interface / device state
            e  = NvBootXusbMscBotProcessRequest(Context, TESTUNITREADY_CMD_OPCODE);
        }
        else
        {
            //Issue request sense command to diagnose read capacity failure
            e = NvBootXusbMscBotProcessRequest(Context, REQUESTSENSE_CMD_OPCODE);
        }
    }
    return e;
}

static NvError XusbEnumerateDevice(NvddkXusbContext *Context)
{
    NvError e = NvSuccess;
    DeviceRequest DeviceRequestVar;

    /*
     * Descriptor type in the high byte and the descriptor index in the low byte.
     * The descriptor index is used to select a specific descriptor
     * (only for configuration and string descriptors)
     * wIndex field specifies the Language ID for string descriptors
     * or is reset to zero for other descriptors
     */
    //Get device descriptor @ control endpoint 0
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType = DEV2HOST_DEVICE;
    DeviceRequestVar.bRequest                         = GET_DESCRIPTOR;
    //Descriptor type in the high byte and the descriptor index
    //in the low byte. The descriptor index is used to select a specific descriptor
    //(only for configuration and string descriptors)
    DeviceRequestVar.wValue                           = USB_DT_DEVICE << 8;
    //wIndex field specifies the Language ID for string descriptors or is reset to
    //zero for other descriptors
    DeviceRequestVar.wIndex                           = 0;
    //Number of bytes to return
    DeviceRequestVar.wLength                          = USB_DEV_DESCRIPTOR_SIZE;
    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));

    //update mps.
    Context->stEnumerationInfo.bMaxPacketSize0 =
                     ((UsbDeviceDescriptor *)(BufferXusbData))->bMaxPacketSize0;

    //------------------------------------------------------------
    // set address
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType   = HOST2DEV_DEVICE;
    DeviceRequestVar.bRequest                           = SET_ADDRESS;
    //specifies the device address to use for all subsequent accesses.
    DeviceRequestVar.wValue                             = XUSB_DEV_ADDR;
    DeviceRequestVar.wIndex                             = 0;
    DeviceRequestVar.wLength                            = 0;     //No data stage

    Context->SetAddress = NV_TRUE;
    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));

    // udpate context with dev Address and clear SetAddress
    Context->stEnumerationInfo.DevAddr                  = XUSB_DEV_ADDR;
    Context->SetAddress                                 = NV_FALSE;
    //---------------------------------------------------------------
    // get device descriptor with addressed control endpoint 0
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType   = DEV2HOST_DEVICE;
    DeviceRequestVar.bRequest                           = GET_DESCRIPTOR;
    //Descriptor type in the high byte and the descriptor index
    //in the low byte. The descriptor index is used to select a specific descriptor
    //(only for configuration and string descriptors)
    DeviceRequestVar.wValue                             = USB_DT_DEVICE << 8;
    //wIndex field specifies the Language ID for string descriptors or is
    // reset to zero for other descriptors
    DeviceRequestVar.wIndex                             = 0;
    //Number of bytes to return
    DeviceRequestVar.wLength                            = USB_DEV_DESCRIPTOR_SIZE;
    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));

    //update mps and configuration
    Context->stEnumerationInfo.bMaxPacketSize0          =
                     ((UsbDeviceDescriptor *)(BufferXusbData))->bMaxPacketSize0;
    Context->stEnumerationInfo.bNumConfigurations       =
                  ((UsbDeviceDescriptor *)(BufferXusbData))->bNumConfigurations;
    //-------------------------------------------------------------------------
    // get config descriptor only!!
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType   = DEV2HOST_DEVICE;
    DeviceRequestVar.bRequest                           = GET_DESCRIPTOR;
    //Descriptor type in the high byte and the descriptor index
    //in the low byte. The descriptor index is used to select a specific descriptor
    //(only for configuration and string descriptors)
    DeviceRequestVar.wValue                             = USB_DT_CONFIG << 8;
    //wIndex field specifies the Language ID for string descriptors or is reset
    //to zero for other descriptors
    DeviceRequestVar.wIndex                             = 0;
    //Number of bytes to return
    DeviceRequestVar.wLength                            = USB_CONFIG_DESCRIPTOR_SIZE;

    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));
    //update configuration value.
    Context->stEnumerationInfo.bConfigurationValue      =
                 ((UsbConfigDescriptor *)(BufferXusbData))->bConfigurationValue;

    //------------------------------------------------------------------------
    // get complete config descriptor
    //All other fields remain same as above
    //Number of bytes to return
    DeviceRequestVar.wLength                            =
                       ((UsbConfigDescriptor *)(BufferXusbData))->wTotalLength;;

    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));

    // parse configuration descriptor for configuration descriptor full length value
    // update bulk endpoints no., in context etc
    // check for MSD support
    // if the MSD device enable configuration else return
     NV_CHECK_ERROR(ParseConfigurationDescriptor(Context));

    //---------------------------------------------------------------------------------
    // set configuration !
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType   = HOST2DEV_DEVICE;
    DeviceRequestVar.bRequest  = SET_CONFIGURATION;
    DeviceRequestVar.wValue    = Context->stEnumerationInfo.bConfigurationValue;
    DeviceRequestVar.wIndex    = 0;
    DeviceRequestVar.wLength   = 0;

    NV_CHECK_ERROR(NvBootXusbProcessControlRequest(Context, &DeviceRequestVar));

    //----------------------------------------------------------------------------------
    // perform class specifc request
    // get max lun
    // Get max lun with addressed endpoint 0
    // The device shall return one byte of data that contains the maximum LUN
    // supported by the device. For example, If the device supports four LUNs then the
    // LUNs would be numbered from 0 to 3 and the return value would be 3.
    // If no LUN is associated with the device, the value returned shall be 0.
    // The host shall not send a command block wrapper (CBW) to a non-existing LUN.
    // Devices that do not support multiple LUNs may STALL this command.

    //TODO : What if device does not support max lun? Is LUN=0 good enough for BOT commands?
    DeviceRequestVar.bmRequestTypeUnion.bmRequestType   = CLASS_SPECIFIC_REQUEST;
    DeviceRequestVar.bRequest = GET_MAX_LUN;
    DeviceRequestVar.wValue   = 0;
    DeviceRequestVar.wIndex   = Context->stEnumerationInfo.InterfaceIndx;
    DeviceRequestVar.wLength  = USB_GET_MAX_LUN_LENGTH;
    NvBootXusbProcessControlRequest(Context, &DeviceRequestVar);
    Context->stEnumerationInfo.LUN = *((NvU8 *)(BufferXusbData)); // read one byte
    return e;
}


static void XusbPadCtrlInit(const NvddkXusbBootParams *Params,
                            NvddkXusbContext *Context)
{
    NvU32 RootPortAttached, RegData;

    // read the Boot media register
    NV_XUSB_PADCTL_READ( BOOT_MEDIA, RootPortAttached);

    // enable USB boot media bit
    RootPortAttached = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, BOOT_MEDIA, \
                                    BOOT_MEDIA_ENABLE, YES, RootPortAttached);
    // set the root port value from params
    RootPortAttached |= NV_DRF_NUM( XUSB_PADCTL, BOOT_MEDIA, BOOT_PORT,\
                                    Params->RootPortNumber);
    // update the boot media register
    NV_XUSB_PADCTL_WRITE( BOOT_MEDIA, RootPortAttached);

    if ((Params->RootPortNumber == USB_BOOT_PORT_OTG0) ||
        (Params->RootPortNumber == USB_BOOT_PORT_OTG1))
    {
        // " Clear the following to disable PAD power down.
        // as part of NoFw_boot_#7.doc udpates
        // PD_CHG field in XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_0 and
        // XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_0
        NV_XUSB_PADCTL_READ( USB2_BATTERY_CHRG_OTGPAD0, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD0, \
                                                            PD_CHG, NO, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_BATTERY_CHRG_OTGPAD0, RegData);
        NV_XUSB_PADCTL_READ( USB2_BATTERY_CHRG_OTGPAD1, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_BATTERY_CHRG_OTGPAD1, \
                                                            PD_CHG, NO, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_BATTERY_CHRG_OTGPAD1, RegData);

        // PD_OTG field in XUSB_PADCTL_USB2_BATTERY_CHRG_BIASPAD_0
        NV_XUSB_PADCTL_READ( USB2_BATTERY_CHRG_BIASPAD, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_BATTERY_CHRG_BIASPAD, \
                                                            PD_OTG, NO, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_BATTERY_CHRG_BIASPAD, RegData);

        // PD_ZI, PD2, and PD fields in XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 and
        // XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0 registers
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD0_CTL_0, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD_ZI, \
                                                            SW_DEFAULT, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD2, \
                                                            SW_DEFAULT, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD, \
                                                            SW_DEFAULT, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD0_CTL_0, RegData);
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD1_CTL_0, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD_ZI, \
                                                            SW_DEFAULT, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD2, \
                                                            SW_DEFAULT, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD, \
                                                            SW_DEFAULT, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD1_CTL_0, RegData);

        // PD_DR field in XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 and
        // XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 registers
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD0_CTL_1, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, PD_DR, \
                                                            SW_DEFAULT, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD0_CTL_1, RegData);
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD1_CTL_1, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, PD_DR, \
                                                            SW_DEFAULT, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD1_CTL_1, RegData);

        //PD_TRK and PD fields in XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0
        NV_XUSB_PADCTL_READ( USB2_BIAS_PAD_CTL_0, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD_TRK,\
                                                            SW_DEFAULT, RegData);
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, PD, \
                                                            SW_DEFAULT, RegData);
        NV_XUSB_PADCTL_WRITE( USB2_BIAS_PAD_CTL_0, RegData);
    }

    // read Read FUSE_USB_CALIB_0 register to set HS_IREF_CAP, HS_SQUELCH_CFG,
    // HS_TERM_RANGE_ADJ, and HS_CURR_LEVEL fields in
    // XUSB_PADCTL_USB2_OTG_PADN_CTL_0 register, where N is determined by BOOT_PORT.
    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_USB_CALIB_0);

    // Following code is not required as the default values are good enough
    // for the XUSB pads
#if 0
    // TO DO upadate for relevant fields.
    HS_IREF_CAP = NV_DRF_VAL(FUSE, USB_CALIB, USB_CALIB, RegData); //HS_IREF_CAP
    HS_SQUELCH_CFG = NV_DRF_VAL(FUSE, USB_CALIB, USB_CALIB, RegData); //HS_SQUELCH_CFG
    HS_TERM_RANGE_ADJ = NV_DRF_VAL(FUSE, USB_CALIB, USB_CALIB, RegData); //HS_TERM_RANGE_ADJ
    HS_CURR_LEVEL = NV_DRF_VAL(FUSE, USB_CALIB, USB_CALIB, RegData); //HS_CURR_LEVEL

    if (Params->RootPortNumber == USB_BOOT_PORT_OTG0)
    {
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD0_CTL_0, RegData);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, HS_CURR_LEVEL, \
                                                                HS_CURR_LEVEL);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD0_CTL_0, RegData);

        // HS_IREF_CAP and TERM_RANGE_ADJ is split
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD0_CTL_1, RegData);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, HS_IREF_CAP, \
                                                                    HS_IREF_CAP);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, TERM_RANGE_ADJ,\
                                                            HS_TERM_RANGE_ADJ);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD0_CTL_1, RegData);
    }
    else if (Params->RootPortNumber == USB_BOOT_PORT_OTG1)
    {
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD1_CTL_0, RegData);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, HS_CURR_LEVEL, \
                                                                HS_CURR_LEVEL);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD1_CTL_0, RegData);

    // HS_IREF_CAP and TERM_RANGE_ADJ is split
        NV_XUSB_PADCTL_READ( USB2_OTG_PAD1_CTL_1, RegData);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, HS_IREF_CAP, \
                                                                    HS_IREF_CAP);
        RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, TERM_RANGE_ADJ,\
                                                            HS_TERM_RANGE_ADJ);
        NV_XUSB_PADCTL_WRITE( USB2_OTG_PAD1_CTL_1, RegData);
    }

    //update HS_SQUELCH_CFG as part of XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0
    NV_XUSB_PADCTL_READ( USB2_BIAS_PAD_CTL_0, RegData);
    RegData = NV_DRF_NUM( XUSB_PADCTL, USB2_BIAS_PAD_CTL_0, HS_SQUELCH_LEVEL, \
                                                                HS_SQUELCH_CFG);
    NV_XUSB_PADCTL_WRITE( USB2_BIAS_PAD_CTL_0, RegData);
#endif

    /*
    Read OC pin register in FUSE_BOOT_DEVICE_INFO_0 register to set PORTN_OC_PIN field in
    XUSB_PADCTL_USB2_OC_MAP_0 register, where N is determined by BOOT_PORT
    */
    NV_XUSB_PADCTL_READ( USB2_OC_MAP, RegData);

    // This needs to be verified
    if (Params->RootPortNumber == USB_BOOT_PORT_OTG0)
    {
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, USB2_OC_MAP, PORT0_OC_PIN, \
                                                        Params->OCPin, RegData);
    }
    else if (Params->RootPortNumber == USB_BOOT_PORT_OTG1)
    {
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, USB2_OC_MAP, PORT1_OC_PIN, \
                                                        Params->OCPin, RegData);
    }

    // Bug # 985773 [XUSB PADCTL] OC asserted spuriously on VBUSEN deassertion
    // OC detection should be disabled until VBUS is not enabled.
    NV_XUSB_PADCTL_WRITE( SNPS_OC_MAP, 0xFFFFFFFF);
    NV_XUSB_PADCTL_WRITE( USB2_OC_MAP, 0xFFFFFFFF);

    // Change HSIC pad slew Bug: http://nvbugs/965122
    NV_XUSB_PADCTL_READ(HSIC_PAD0_CTL_0, RegData);
    RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, HSIC_PAD0_CTL, 0_TX_SLEWP, 0, RegData);
    RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, HSIC_PAD0_CTL, 0_TX_SLEWN, 0, RegData);
    NV_XUSB_PADCTL_WRITE(HSIC_PAD0_CTL_0, RegData);
}

static void XusbClkRstInit(NvddkXusbContext *Context)
{
    NvU32 RegData = 0;
    NvU32 Value = 0;
    NvU32 RegOffset = 0;

    // As per Step#3 of NoFW doc

    // Enable the xusb clock.
    XusbClocksSetEnable(NvBootClocksClockId_XUsbId, NV_TRUE);
    // Enable the xusb Host clock.
    XusbClocksSetEnable(NvBootClocksClockId_XUsbHostId, NV_TRUE);

    /*
    a.      CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0:    0x20000006 (108M)

    b.      CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0:       0x20000002 (216M)

    c.      CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0:           0x40000000 (48M)

    f.       CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_HS_0:          0xC000000E (60M)
    */
    RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_CORE_HOST, \
                                                XUSB_CORE_HOST_CLK_DIVISOR, 6) |
              NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_CORE_HOST, \
                                            XUSB_CORE_HOST_CLK_SRC, PLLP_OUT0);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0, RegData);

    RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_FALCON, \
                                                    XUSB_FALCON_CLK_DIVISOR, 2) |
              NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_FALCON, \
                                                XUSB_FALCON_CLK_SRC, PLLP_OUT0);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                            CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0, RegData);

    RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_FS, \
                                                        XUSB_FS_CLK_DIVISOR, 0) |
              NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_FS, \
                                                        XUSB_FS_CLK_SRC, FO_48M);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                                CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0, RegData);

    RegData = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_SS, \
                                                    XUSB_SS_CLK_DIVISOR, 0x6) |
              NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_SS, \
                                                    XUSB_SS_CLK_SRC, HSIC_480);
    NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
               CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0, RegData);

    // Release reset to xusb host block
    XusbResetSetEnable(NvBootResetDeviceId_XUsbHostId, NV_FALSE);

    // As per Step#4 of NoFW doc

    // Set EN_FPCI of XUSB_HOST_CONFIGURATION_0 register to 1
    NV_XUSB_HOST_READ(CONFIGURATION, Value);
    Value |= NV_FLD_SET_DRF_NUM(XUSB_HOST, CONFIGURATION, EN_FPCI,1, Value);
    NV_XUSB_HOST_WRITE(CONFIGURATION, Value);

    //Set NV_PROJ__XUSB_CFG_4 to 0x7009_0000 (Note: bit[14:0] of this register is RO)
    NV_XUSB_CFG_WRITE(4, 0x70090000);

    //Set MEMORY_SPACE and BUS_MASTER bits in NV_PROJ__XUSB_CFG_1 to 0x1
    NV_XUSB_CFG_READ(1, RegData);
    RegData = NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, MEMORY_SPACE, ENABLED, RegData);
    RegData = NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, BUS_MASTER, ENABLED, RegData);
    NV_XUSB_CFG_WRITE(1, RegData);

    // As per Step#5 of NoFW doc

    // Need to initialize the following registers by setting them to all 0s:
    // NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD3
    // NV_PROJ__XUSB_CSB_HS_BI_WORKQ_DWRD43
    NV_XUSB_CSB_EXPAND(HS_BI_WORKQ_DWRD3, RegOffset);
    Value = 0;
    PciRegUpdate(Context, RegOffset, XUSB_WRITE , &Value);

    NV_XUSB_CSB_EXPAND(HS_BI_WORKQ_DWRD4, RegOffset);
    Value = 0;
    PciRegUpdate(Context, RegOffset, XUSB_WRITE , &Value);
}

static void XusbPadVbusEnable(const NvddkXusbBootParams *Params)
{
    NvU32 RegData;

    // Step#6 of NoFW doc
    // Enable the VBUS power to the port by setting VBUS_ENABLEN of the
    // XUSB_PADCTL_OC_DET_0 register, where N is determined by VBUS number
    // program OC detect register
    if (Params->VBusEnable == 0)
    {
        NV_XUSB_PADCTL_READ( OC_DET, RegData);
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, OC_DET, VBUS_ENABLE0_OC_MAP,\
                                                        Params->OCPin, RegData);
        NV_XUSB_PADCTL_WRITE( OC_DET, RegData);
        //vbus enable
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, OC_DET, VBUS_ENABLE0, YES, RegData);
        NV_XUSB_PADCTL_WRITE( OC_DET, RegData);
    }
    else
    {
        NV_XUSB_PADCTL_READ( OC_DET, RegData);
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, OC_DET, VBUS_ENABLE1_OC_MAP,\
                                                        Params->OCPin, RegData);
        NV_XUSB_PADCTL_WRITE( OC_DET, RegData);
        //vbus enable
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, OC_DET, VBUS_ENABLE1, YES, RegData);
        NV_XUSB_PADCTL_WRITE( OC_DET, RegData);
    }
    //Wait 100ms.
#if NVBOOT_TARGET_RTL
    NvOsWaitUS(TIMEOUT_1MS);
#else
    NvOsWaitUS(TIMEOUT_100MS);
#endif
}

static void XusbPadVbusDisable(const NvddkXusbBootParams *Params)
{
    NvU32 RegData;
    //vbus Disable
    NV_XUSB_PADCTL_READ( OC_DET, RegData);
    if (Params->VBusEnable == 0)
    {
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, OC_DET, VBUS_ENABLE0, NO, RegData);
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, OC_DET, VBUS_ENABLE0_OC_MAP, \
                                                        Params->OCPin, RegData);
    }
    else
    {
        RegData = NV_FLD_SET_DRF_DEF( XUSB_PADCTL, OC_DET, VBUS_ENABLE1, NO, RegData);
        RegData = NV_FLD_SET_DRF_NUM( XUSB_PADCTL, OC_DET, VBUS_ENABLE1_OC_MAP, \
                                                        Params->OCPin, RegData);
    }
    NV_XUSB_PADCTL_WRITE( OC_DET, RegData);
}

static NvError XusbResetPort(NvddkXusbContext *Context,
                             const NvddkXusbBootParams *Params)
{
    NvError ErrorCode = NvSuccess;
    NvU32 value = 0;
    NvU32 RegOffset = 0;
    NvU8 PortNumOffset = 0;
    NvU8 ControllerPortNumber = 0;

    if (!BootPortControllerMapping((Context->RootPortNum),&ControllerPortNumber))
    {
        return NvError_NotSupported;// not supported port.
    }
    PortNumOffset = (ControllerPortNumber * PORT_INSTANCE_OFFSET);

    //OTG ports disable vbus
    if ((Context->RootPortNum == USB_BOOT_PORT_OTG0) ||
        (Context->RootPortNum == USB_BOOT_PORT_OTG1))
    {
        XusbPadVbusDisable(Params);
    }
    else if ((Context->RootPortNum == USB_BOOT_PORT_ULPI)  ||
             (Context->RootPortNum == USB_BOOT_PORT_HSIC0) ||
             (Context->RootPortNum== USB_BOOT_PORT_HSIC1))
    {
        // HSIC/ULPI reset port
        // as part of NoFw_boot_#7.doc udpates
        // Write 'Reset' to USB3_CSB_FSPI_PVTPORTSC1_PLS_CNTRL register to reset the port.
        // Write '1' to NV_PROJ__XUSB_CSB_FSPI_PVTPORTSC1_PLS_VALID
        NV_XUSB_CSB_EXPAND(FSPI_PVTPORTSC1, RegOffset);
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_VALID, SET, value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, FSPI_PVTPORTSC1, PLS_CNTRL, RESET, value);
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);

        /*
        Write 'Reset' to USB3_CSB_HSPI_PVTPORTSC1_PLS_CNTRL register to reset the port.
        Write '1' to NV_PROJ__XUSB_CSB_HSPI_PVTPORTSC1_PLS_VALID
        The reset operation is required for USB2.0 devices.
        The plan is to support USB2.0 mode only during boot.
        This corresponds to one register write request.
        Wait 50ms.
        */
        NV_XUSB_CSB_EXPAND(HSPI_PVTPORTSC1, RegOffset);
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_READ, &value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_VALID, SET, value);
        value = NV_FLD_SET_DRF_DEF(XUSB_CSB, HSPI_PVTPORTSC1, PLS_CNTRL, RESET, value);
        PciRegUpdate(Context, (RegOffset + PortNumOffset), XUSB_WRITE , &value);
    }

    // Apply reset to xusb host controller
    XusbResetSetEnable(NvBootResetDeviceId_XUsbHostId, NV_TRUE);
    // Apply Reset to the xusb pad controller
    XusbResetSetEnable(NvBootResetDeviceId_XUsbId, NV_TRUE);

    return ErrorCode;
}


// Enable the clock, this corresponds generally to level 1 clock gating
void XusbClocksSetEnable(NvBootClocksClockId ClockId, NvBool Enable)
{
    NvU32 RegData;
    NvU8  BitOffset;
    NvU8  RegOffset;

    NVBOOT_CLOCKS_CHECK_CLOCKID(ClockId);
    NV_ASSERT(((int) Enable == 0) ||((int) Enable == 1));

    if (NVBOOT_CLOCKS_HAS_STANDARD_ENB(ClockId))
    {
        // there is a CLK_ENB bit to kick
        BitOffset = NVBOOT_CLOCKS_BIT_OFFSET(ClockId);
        RegOffset = NVBOOT_CLOCKS_REG_OFFSET(ClockId);
        if (NVBOOT_CLOCKS_HAS_VWREG_ENB(ClockId))
        {
            RegData = CAR_READ(CLK_OUT_ENB_V, RegOffset);
        }
        else
        {
            RegData = CAR_READ(CLK_OUT_ENB_L, RegOffset);
        }
        /* no simple way to use the access macro in this case */
        if (Enable)
            RegData |=  (1 << BitOffset);
        else
            RegData &= ~(1 << BitOffset);

        if (NVBOOT_CLOCKS_HAS_VWREG_ENB(ClockId))
        {
            CAR_WRITE(CLK_OUT_ENB_V, RegOffset, RegData);
        }
        else
        {
            CAR_WRITE(CLK_OUT_ENB_L, RegOffset, RegData);
        }
    }
    else
    {
        // there is no bit in CLK_ENB, less regular processing needed
        switch (ClockId)
        {
        case NvBootClocksClockId_SclkId:
            // there is no way to stop Sclk, for documentation purpose
            break;

        case NvBootClocksClockId_HclkId:
            RegData = CAR_READ(CLK_SYSTEM_RATE,0);
            RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                                         CLK_SYSTEM_RATE,
                                         HCLK_DIS,
                                         Enable,
                                         RegData);
            CAR_WRITE(CLK_SYSTEM_RATE, 0, RegData);
            break;

        case NvBootClocksClockId_PclkId:
            RegData = CAR_READ(CLK_SYSTEM_RATE, 0);
            RegData = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                                         CLK_SYSTEM_RATE,
                                         PCLK_DIS,
                                         Enable,
                                         RegData);
            CAR_WRITE(CLK_SYSTEM_RATE, 0, RegData);
            break;

        default :
            // nothing for other enums, make that explicit for compiler
            break;
        };
    };
    NvOsWaitUS(NVBOOT_CLOCKS_CLOCK_STABILIZATION_TIME);
}

void
XusbResetSetEnable(const NvBootResetDeviceId DeviceId, const NvBool Enable)
{

    NvU32 RegData ;
    NvU32 BitOffset ;
    NvU32 RegOffset ;

    NVBOOT_RESET_CHECK_ID(DeviceId) ;
    NV_ASSERT( ((int) Enable == 0) ||
              ((int) Enable == 1)   ) ;

    BitOffset = NVBOOT_RESET_BIT_OFFSET(DeviceId) ;
    RegOffset = NVBOOT_RESET_REG_OFFSET(DeviceId) ;

    if (NVBOOT_RESET_HAS_VWREG_ENB(DeviceId))
    {
        RegData = CAR_READ(RST_DEVICES_V, RegOffset);
    }
    else
    {
        RegData = CAR_READ(RST_DEVICES_L, RegOffset);
    }

    /* no simple way to use the access macro in this case */
    if (Enable)
        RegData |=  (1 << BitOffset) ;
    else
        RegData &= ~(1 << BitOffset) ;

    if (NVBOOT_RESET_HAS_VWREG_ENB(DeviceId))
        CAR_WRITE(RST_DEVICES_V, RegOffset, RegData);
    else
        CAR_WRITE(RST_DEVICES_L, RegOffset, RegData);

    // wait stabilization time (always)
    NvOsWaitUS(NVBOOT_RESET_STABILIZATION_DELAY);
    return ;
}

/*  The pin mux code supports run-time trace debugging of all updates to the
 *  pin mux & tristate registers by embedding strings (cast to NvU32s) into the
 *  control tables.
 */
#define NVRM_PINMUX_DEBUG_FLAG 0
#define NVRM_PINMUX_SET_OPCODE_SIZE_RANGE 3:1


#if NVRM_PINMUX_DEBUG_FLAG
NV_CT_ASSERT(sizeof(NvU32)==sizeof(const char*));
#endif

//  The extra strings bloat the size of Set/Unset opcodes
#define NVRM_PINMUX_SET_OPCODE_SIZE \
                    ((NVRM_PINMUX_DEBUG_FLAG)?NVRM_PINMUX_SET_OPCODE_SIZE_RANGE)

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef enum {
    PinMuxConfig_OpcodeExtend = 0,
    PinMuxConfig_Set = 1,
    PinMuxConfig_Unset = 2,
    PinMuxConfig_BranchLink = 3,
} PinMuxConfigStates;

typedef enum {
    PinMuxOpcode_ConfigEnd = 0,
    PinMuxOpcode_ModuleDone = 1,
    PinMuxOpcode_SubroutinesDone = 2,
} PinMuxConfigExtendOpcodes;

//
// Note to the RM team: The per-pin pin control registers collect all the
// relevant pin control state into single registers per pin.
// Therefore, a single offset is needed for both pinmux and tristrate controls.
// Further, the bit positions for the pinmux controls and tristates are
// are identical for all pins.
// Based on these observations:
//    * There is a single offset whose range has been expanded to handle
//      the larger number of entries needed.
//    * The TS_SHIFT, MUX_CTL_SHIFT, and MUX_CTL_MASK fields have been
//      deprecated.
//
// Presumably, the GPIO port-to-pin mapping goes away, but that
// is outside the purview of the Boot ROM.

// When the state is BranchLink, this is the number of words to increment
// the current "PC"
#define MUX_ENTRY_0_BRANCH_ADDRESS_RANGE 31:2

// The incr1 offset from PINMUX_AUX_ULPI_DATA0_0 to the pin's control register
#define MUX_ENTRY_0_OFFSET_RANGE 31:12

// This value to set PULLUP or not
#define MUX_ENTRY_0_PULL_CTL_SET_RANGE 11:8

// When a pad group needs to be owned (or disowned), this value is applied
#define MUX_ENTRY_0_MUX_CTL_SET_RANGE 7:5

// This value is compared against, to determine if the pad group should be
// disowned
#define MUX_ENTRY_0_MUX_CTL_UNSET_RANGE 4:2

// for extended opcodes, this field is set with the extended opcode
#define MUX_ENTRY_0_OPCODE_EXTENSION_RANGE 3:2

// The state for this entry
#define MUX_ENTRY_0_STATE_RANGE 1:0

#define MAX_NESTING_DEPTH 4

/*
 * This macro is used to generate 32b value to program the tristate & pad mux
 * control registers for config/unconfig for a padgroup
 */
#define PIN_MUX_ENTRY(PINOFFS,PUPDSET,MUXSET,MUXUNSET,STAT) \
    (NV_DRF_NUM(MUX, ENTRY, OFFSET, PINOFFS) | \
    NV_DRF_NUM(MUX, ENTRY, PULL_CTL_SET, PUPDSET) | \
    NV_DRF_NUM(MUX, ENTRY, MUX_CTL_SET, MUXSET) | \
    NV_DRF_NUM(MUX, ENTRY, MUX_CTL_UNSET,MUXUNSET) | \
    NV_DRF_NUM(MUX, ENTRY, STATE,STAT))

/*
 * This is used to program the tristate & pad mux control registers for a
 * pad group.
 */
#define CONFIG_VAL(REG, MUX, PUPD) \
    (PIN_MUX_ENTRY(((PINMUX_AUX_##REG##_0 - PINMUX_AUX_ULPI_DATA0_0)>>2), \
                PINMUX_AUX_##REG##_0_PUPD_##PUPD,PINMUX_AUX_##REG##_0_PM_##MUX, \
                0, PinMuxConfig_Set))

/*
 * This macro is used to compare a pad group against a potentially conflicting
 * enum (where the conflict is caused by setting a new config), and to resolve
 * the conflict by setting the conflicting pad group to a different,
 * non-conflicting option.
 * Read this as: if the pin's mux is equal to (CONFLICTMUX), replace it with
 * (RESOLUTIONMUX)
 */
#define UNCONFIG_VAL(REG, CONFLICTMUX, RESOLUTIONMUX) \
    (PIN_MUX_ENTRY(((PINMUX_AUX_##REG##_0 - PINMUX_AUX_ULPI_DATA0_0)>>2), \
                  PINMUX_AUX_##REG##_0_PM_##RESOLUTIONMUX, \
                  PINMUX_AUX_##REG##_0_PM_##CONFLICTMUX, \
                  PinMuxConfig_Unset))

#if NVRM_PINMUX_DEBUG_FLAG
#define CONFIG(REG, MUX) \
    (CONFIG_VAL(REG, MUX)), \
    (NvU32)(const void*)(#REG "_0_" "_PM to " #MUX),

#define UNCONFIG(REG, CONFLICTMUX, RESOLUTIONMUX) \
    (UNCONFIG_VAL(REG, CONFLICTMUX, RESOLUTIONMUX)), \
    (NvU32)(const void*)(#REG "_0_PM from " #CONFLICTMUX " to " #RESOLUTIONMUX), \
    (NvU32)(const void*)(NULL)
#else
#define CONFIG(REG, MUX, PUPD)     (CONFIG_VAL(REG, MUX,PUPD))

#define UNCONFIG(REG, CONFLICTMUX, RESOLUTIONMUX) \
    (UNCONFIG_VAL(REG, CONFLICTMUX, RESOLUTIONMUX))
#endif

/*  This macro is used for opcode entries in the tables */
#define PIN_MUX_OPCODE(_OP_) \
    (NV_DRF_NUM(MUX,ENTRY,STATE,PinMuxConfig_OpcodeExtend) | \
     NV_DRF_NUM(MUX,ENTRY,OPCODE_EXTENSION,(_OP_)))

/*
 * This is a dummy entry in the array which indicates that all
 * setting/unsetting for a configuration is complete.
 */
#define CONFIGEND() PIN_MUX_OPCODE(PinMuxOpcode_ConfigEnd)

/*
 * This is a dummy entry in the array which indicates that the last
 * configuration
 * for the module instance has been passed.
 */
#define MODULEDONE()  PIN_MUX_OPCODE(PinMuxOpcode_ModuleDone)

/*
 * This is a dummy entry in the array which indicates that all "extra"
 * configurations used by sub-routines have been passed.
 */
#define SUBROUTINESDONE() PIN_MUX_OPCODE(PinMuxOpcode_SubroutinesDone)

/*
 * This macro is used to insert a branch-and-link from one configuration
 * to another
 */
#define BRANCH(_ADDR_) \
     (NV_DRF_NUM(MUX,ENTRY,STATE,PinMuxConfig_BranchLink) | \
      NV_DRF_NUM(MUX,ENTRY,BRANCH_ADDRESS,(_ADDR_)))

/*
 * The below entries define the table format for GPIO Port/Pin-to-Tristate
 * register mappings. Each table entry is 16b, and one is stored for every
 * GPIO Port/Pin on the chip.
 */
#define MUX_GPIOMAP_0_OFFSET_RANGE 15:10

#define TRISTATE_ENTRY(PINOFFS) \
    ((NvU16)(NV_DRF_NUM(MUX,GPIOMAP,OFFSET,(PINOFFS))))

#define GPIO_TRISTATE(REG) \
    (TRISTATE_ENTRY(((PINMUX_AUX_##REG##_0 - PINMUX_AUX_ULPI_DATA_0_0)>>2)))


const NvU32 g_TegraMux_Usb3[] =
{
    CONFIGEND(), // config 1, otg0
    CONFIG(USB_VBUS_EN0, USB, PULL_UP), // vbus_0

    CONFIGEND(), // config 2, otg1
    CONFIG(USB_VBUS_EN1, USB, PULL_UP), // vbus_1

    CONFIGEND(), // config 3
    CONFIG(GPIO_PV0, USB, NORMAL), // Oc 0

    CONFIGEND(), // config 4
    CONFIG(GPIO_PU6, USB, NORMAL), // Oc 1

    CONFIGEND(), // config 5
    CONFIG(GMI_CS4_N, USB, NORMAL), // Oc 2

    CONFIGEND(), // config 6
    CONFIG(KB_COL0, USB, NORMAL), // Oc 3

    CONFIGEND(),
    SUBROUTINESDONE(),
    MODULEDONE(),
};

#define NV_ADDRESS_MAP_APB_MISC_BASE        0x70000000

/*
 *  FindConfigStart searches through an array of configuration data to find the
 *  starting position of a particular configuration in a module instance array.
 *  The stop position is programmable, so that sub-routines can be placed after
 *  the last valid true configuration
 */
static const NvU32*
NvBootPadFindConfigStart(const NvU32* Instance,
    NvU32 Config,
    NvU32 EndMarker);

static const NvU32*
NvBootPadFindConfigStart(const NvU32* Instance,
    NvU32 Config,
    NvU32 EndMarker)
{
    NvU32 Cnt = 0;
    while ((Cnt < Config) && (*Instance!=EndMarker))
    {
        switch (NV_DRF_VAL(MUX, ENTRY, STATE, *Instance))
        {
            case PinMuxConfig_BranchLink:
            case PinMuxConfig_OpcodeExtend:
                    if (*Instance==CONFIGEND())
                        Cnt++;
                    Instance++;
                 break;
            default:
                Instance += NVRM_PINMUX_SET_OPCODE_SIZE;
                 break;
        }
    }

    //  Ugly postfix.  In modules with bonafide subroutines, the last
    //  configuration CONFIGEND() will be followed by a MODULEDONE()
    //  token, with the first Set/Unset/Branch of the subroutine
    //  following that.  To avoid leaving the "PC" pointing to a
    //  MODULEDONE() in the case where the first subroutine should be
    //  executed, fudge the "PC" up by one, to point to the subroutine.
    if (EndMarker==SUBROUTINESDONE() && *Instance==MODULEDONE())
        Instance++;

    if (*Instance==EndMarker)
        Instance = NULL;

    return Instance;
}

void
NvBootPadSetTriStates(
    const NvU32* Module,
    NvU32 Config,
    NvBool EnableTristate);

void
NvBootPadSetTriStates(
    const NvU32* Module,
    NvU32 Config,
    NvBool EnableTristate)
{
    int          StackDepth = 0;
    const NvU32 *Instance = NULL;
    const NvU32 *ReturnStack[MAX_NESTING_DEPTH+1];

    Instance = NvBootPadFindConfigStart(Module, Config, MODULEDONE());
    /*
     * The first stack return entry is NULL, so that when a ConfigEnd is
     * encountered in the "main" configuration program, we pop off a NULL
     * pointer, which causes the configuration loop to terminate.
     */
    ReturnStack[0] = NULL;

    /*
     * This loop iterates over all of the pad groups that need to be updated,
     * and updates the reference count for each appropriately.
     */
    while (Instance)
    {
        switch (NV_DRF_VAL(MUX,ENTRY, STATE, *Instance))
        {
            case PinMuxConfig_OpcodeExtend:
              /*
               * Pop the most recent return address off of the return stack
               * (which will be NULL if no values have been pushed onto the
               * stack)
               */
              if (NV_DRF_VAL(MUX,ENTRY, OPCODE_EXTENSION, *Instance) ==
                  PinMuxOpcode_ConfigEnd)
              {
                  Instance = ReturnStack[StackDepth--];
              }
              /*
               * ModuleDone & SubroutinesDone should never be encountered
               * during execution, for properly-formatted tables.
               */
              else
              {
                  NV_ASSERT(0); // Logical entry in table
              }
              break;

            case PinMuxConfig_BranchLink:
                /*
                 * Push the next instruction onto the return stack if nesting
                 * space is available, and jump to the target.
                 */
                NV_ASSERT(StackDepth<MAX_NESTING_DEPTH);

                ReturnStack[++StackDepth] = Instance+1;

                Instance = NvBootPadFindConfigStart(
                    Module,
                    NV_DRF_VAL(MUX,ENTRY,BRANCH_ADDRESS,*Instance),
                    SUBROUTINESDONE());

                NV_ASSERT(Instance); // Invalid branch configuration in table
                break;

            case PinMuxConfig_Set:
                {
                    NvU32 Offs = NV_DRF_VAL(MUX, ENTRY, OFFSET, *Instance);

                    // Perform the udpate
                    NvU32 Curr = NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE +
                                           PINMUX_AUX_ULPI_DATA0_0 +
                                           4*Offs);

                    Curr = NV_FLD_SET_DRF_NUM(PINMUX_AUX,
                                              ULPI_DATA0,
                                              TRISTATE,
                                              EnableTristate?1:0,
                                              Curr);

                    // Clearing the IO_RESET
                    if((NV_DRF_VAL(MUX, ENTRY, PULL_CTL_SET, *Instance)) ||
                        (NV_DRF_VAL( PINMUX_AUX, SDMMC4_CLK, IO_RESET, Curr)))
                    {
                        if(NV_DRF_VAL( PINMUX_AUX, SDMMC4_CLK, IO_RESET, Curr))
                             Curr = NV_FLD_SET_DRF_DEF( PINMUX_AUX, SDMMC4_CLK,
                                        IO_RESET,NORMAL, Curr);
                    }

                    NV_WRITE32(NV_ADDRESS_MAP_APB_MISC_BASE +
                               PINMUX_AUX_ULPI_DATA0_0 + 4*Offs,
                               Curr);
                }

                //  fall through.

                /*
                 * The "Unset" configurations are not applicable to tristate
                 * configuration, so skip over them.
                 */
            case PinMuxConfig_Unset:
                Instance += NVRM_PINMUX_SET_OPCODE_SIZE;
                break;
        }
    }
}

/*  NvRmSetPinMuxCtl will apply new pin mux configurations to the pin mux
 *  control registers.  */
void
NvBootPadSetPinMuxCtl(
    const NvU32* Module,
    NvU32 Config,
    NvBool DoPinMux,
    NvBool DoPullUpPullDown);

void
NvBootPadSetPinMuxCtl(
    const NvU32* Module,
    NvU32 Config,
    NvBool DoPinMux,
    NvBool DoPullUpPullDown)
{
    NvU32 Curr;
    NvU32 MuxCtlSet;
    NvU32 PullCtlSet;
    NvU32 MuxCtlUnset;
    NvU32 Offset;
    NvU32 State;
    const NvU32 *ReturnStack[MAX_NESTING_DEPTH+1];
    const NvU32 *Instance;
    int StackDepth = 0;

    ReturnStack[0] = NULL;

    Instance = NvBootPadFindConfigStart(Module, Config, MODULEDONE());

    // Apply the new configuration, setting / unsetting as appropriate
    while (Instance)
    {
        State = NV_DRF_VAL(MUX,ENTRY, STATE, *Instance);
        switch (State)
        {
            case PinMuxConfig_OpcodeExtend:
                if (NV_DRF_VAL(MUX,ENTRY, OPCODE_EXTENSION, *Instance) ==
                    PinMuxOpcode_ConfigEnd)
                {
                    Instance = ReturnStack[StackDepth--];
                }
                else
                {
                    NV_ASSERT(0); //Logical entry in table
                }
                break;
            case PinMuxConfig_BranchLink:
                NV_ASSERT(StackDepth<MAX_NESTING_DEPTH);
                ReturnStack[++StackDepth] = Instance+1;
                Instance = NvBootPadFindConfigStart(Module,
                                                    NV_DRF_VAL(MUX,
                                                               ENTRY,
                                                               BRANCH_ADDRESS,
                                                               *Instance),
                                                    SUBROUTINESDONE());
                NV_ASSERT(Instance); // Invalid branch configuration in table
                break;

            default:
            {
                MuxCtlUnset = NV_DRF_VAL(MUX, ENTRY, MUX_CTL_UNSET, *Instance);
                MuxCtlSet   = NV_DRF_VAL(MUX, ENTRY, MUX_CTL_SET,   *Instance);
                PullCtlSet   = NV_DRF_VAL(MUX, ENTRY, PULL_CTL_SET,   *Instance);
                Offset      = NV_DRF_VAL(MUX, ENTRY, OFFSET, *Instance);

                // Perform the udpate
                Curr = NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE +
                                 PINMUX_AUX_ULPI_DATA0_0 +
                                 4*Offset);

                if (State == PinMuxConfig_Set)
                {
                    if(DoPinMux)
                    {
                        Curr = NV_FLD_SET_DRF_NUM(PINMUX_AUX,
                                                  ULPI_DATA0,
                                                  PM,
                                                  MuxCtlSet,
                                                  Curr);

                        // Clearing the ENABLE_OD
                        if((NV_DRF_VAL(MUX, ENTRY, PULL_CTL_SET, *Instance)) ||
                            (NV_DRF_VAL( PINMUX_AUX, USB_VBUS_EN0, OD, Curr)))
                        {
                            if(NV_DRF_VAL( PINMUX_AUX, USB_VBUS_EN0, OD, Curr))
                                 Curr = NV_FLD_SET_DRF_DEF( PINMUX_AUX, USB_VBUS_EN0,
                                            OD,DISABLE, Curr);
                        }
                    }

                    if(DoPullUpPullDown)
                    {
                        Curr = NV_FLD_SET_DRF_NUM(PINMUX_AUX,
                                                  ULPI_DATA0,
                                                  PUPD,
                                                  PullCtlSet,
                                                  Curr);
                    }
                }
                else if (NV_DRF_VAL(PINMUX_AUX, ULPI_DATA0, PM, Curr) ==
                         MuxCtlUnset)
                {

                    NV_ASSERT(State == PinMuxConfig_Unset);
                    if(DoPinMux)
                    {
                        Curr = NV_FLD_SET_DRF_NUM(PINMUX_AUX,
                                                  ULPI_DATA0,
                                                  PM,
                                                  MuxCtlSet,
                                                  Curr);
                    }
                }

                NV_WRITE32(NV_ADDRESS_MAP_APB_MISC_BASE +
                           PINMUX_AUX_ULPI_DATA0_0 + 4*Offset,
                           Curr);
            }

            Instance += NVRM_PINMUX_SET_OPCODE_SIZE;
            break;
        }
    }
}


static NvError
XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice BootDevice, NvU32 Config)
{
    const NvU32 *BootData;


    NV_ASSERT(BootDevice < NvBootFuseBootDevice_Max);

    // Select pin mux table.
    BootData = g_TegraMux_Usb3;

    // Set Pin Muxes only (DoPinMux == NV_TRUE, DoPullUpPullDown == NV_FALSE).
    NvBootPadSetPinMuxCtl(BootData, Config, NV_TRUE, NV_FALSE);

    // Set tristates for the device.
    NvBootPadSetTriStates(BootData, Config, NV_FALSE);

    // Set Pull-ups/Pull-downs only (DoPinMux == NV_FALSE,
    // DoPullUpPullDown == NV_TRUE).
    NvBootPadSetPinMuxCtl(BootData, Config, NV_FALSE, NV_TRUE);

    return NvSuccess;
}

static void NvBootUsb3Shutdown(void)
{
     // Apply reset to xusb host controller
     XusbResetSetEnable(NvBootResetDeviceId_XUsbHostId, NV_TRUE);

     // Stop the clock to USBH controller.
     // Disable the xusb Host clock.
     XusbClocksSetEnable(NvBootClocksClockId_XUsbHostId, NV_FALSE);
     // Disable the xusb clock.
     XusbClocksSetEnable(NvBootClocksClockId_XUsbId, NV_FALSE);

     // Apply Reset to the controller
     XusbResetSetEnable(NvBootResetDeviceId_XUsbId, NV_TRUE);

     return;
}

static NvError HwUsbInitController(NvddkXusbContext *Context,
                                   const NvddkXusbBootParams *Params)
{
    NvError e = NvSuccess;

    // reset port.
    XusbResetPort(Context, Params);

    // if Oc pin ==6/7 skip OC pin programming
    if ((Params->OCPin != 6) && (Params->OCPin != 7))
    {
        // 0x0 = OC Detected 0
        if (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED0)
        {
            XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                              NvBootPinmuxConfig_Usb3_Oc0);
        }
        else if (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED1)
        {
            XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                              NvBootPinmuxConfig_Usb3_Oc1);
        }
        else if (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED2)
        {
            XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                              NvBootPinmuxConfig_Usb3_Oc2);
        }
        else if (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED3)
        {
            XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                              NvBootPinmuxConfig_Usb3_Oc3);
        }
    }
    if ((Params->VBusEnable == VBUS_ENABLE_0) ||
        (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED_VBUS_PAD0))
    {
        XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                          NvBootPinmuxConfig_Usb3_Otg0);
    }
    else if ((Params->VBusEnable == VBUS_ENABLE_1) ||
        (Params->OCPin == XUSB_PADCTL_OC_DET_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED_VBUS_PAD1))
    {
        XusbNvBootPadsConfigForBootDevice(NvBootFuseBootDevice_Usb3,
                                          NvBootPinmuxConfig_Usb3_Otg1);
    }

    // Remove the controller from Reset.
    XusbResetSetEnable(NvBootResetDeviceId_XUsbId, NV_FALSE);

    XusbPadCtrlInit(Params, Context);
    XusbClkRstInit( Context);

    XusbPadVbusEnable(Params);
    // check /reset device connection
    NV_CHECK_ERROR(DevicePortInit(Context));

    //NvBootArcEnable();

    return e;
}

static void GetBootParams(NvddkXusbBootParams *pParams)
{
    // Need to verify this logic
    // Not sure what will happen if the fuse data is supposed to be
    // bypassed with strap data.

    NvU32 Regdata = 0;

    NvOsMemset(pParams, 0x0, sizeof(pParams));
    Regdata = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_BOOT_DEVICE_INFO_0);
    Regdata = NV_DRF_VAL(FUSE, BOOT_DEVICE_INFO, BOOT_DEVICE_CONFIG, Regdata);

    pParams->RootPortNumber = NV_DRF_VAL(USBH_DEVICE, CONFIG,ROOT_PORT, Regdata);
    pParams->OCPin = NV_DRF_VAL(USBH_DEVICE, CONFIG, OC_PIN, Regdata);
    pParams->VBusEnable = NV_DRF_VAL(USBH_DEVICE, CONFIG, VBUS_ENABLE, Regdata);
}


NvError
Nvddk_xusbh_init(NvddkXusbContext* Context)
{
    NvError e = NvSuccess;
    NvU32 Size;
    NvU32 BootDevice;
    NvddkXusbBootParams Params;

    NV_ASSERT(Context != NULL);

    Size = sizeof(NvU32);
    NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
            (void *)&BootDevice, (NvU32 *)&Size);
    if (BootDevice != NvBootDevType_Usb3)
    {
        e = NvError_XusbDeviceNotAttached;
        return e;
    }
    GetBootParams(&Params);

    InitializeContexts(Context, Params.RootPortNumber);

    //Usb3 page size of 512 bytes is fixed
    Context->PageSizeLog2  = MSC_MAX_PAGE_SIZE_LOG_2;
    Context->BlockSizeLog2 = NVBOOT_MAX_PAGE_SIZE_LOG2;

    // Initialize the Usb Host controller.
    e = HwUsbInitController(Context, &Params);

    // Check whether device is present
    // this step is already checked as part of device port init
    if ((e = HwUsbIsDeviceAttached(Context)) != NvSuccess)
    {
        NvBootUsb3Shutdown();
        e = NvError_XusbDeviceNotAttached;
    }
    else
    {
        // only MSD(BOT) is supported as boot media
        // enumerate the attached usb device
        // set configuration
        // Step 9,10 & 11
        NV_CHECK_ERROR(XusbEnumerateDevice( Context));
        // Step 12
        // Initialize scsi commands (inquiry, read capacity/capacities,
        // request sense, mode sense & test unit ready)
        NV_CHECK_ERROR(InitializeScsiMedia( Context));
    }
    // Log return value in BIT
    Context->Usb3Status.InitReturnVal = (NvU32)e;
    return e;
}

void Nvddk_xusbh_deinit()
{
}

NvError
NvddkXusbReadPage(NvddkXusbContext* Context,
                  const NvU32 Block,
                  const NvU32 Page,
                  NvU8 *Dest,
                  NvU32 Size)
{
    NvError ErrorCode = NvSuccess;
    //NvBootDeviceStatus DeviceStatus = NvBootDeviceStatus_ReadFailure;
    NvU32 RetryCount = USB_MAX_TXFR_RETRIES;

    NV_ASSERT(Page < (NvU32)(1 << ((Context->BlockSizeLog2) - (Context->PageSizeLog2))));
    NV_ASSERT(Dest != NULL);

    //**************************READ ONE PAGE*************************

    if (ErrorCode == NvSuccess)
    {
        //Issue Read command
        //It is assumed that usb device block size would be multiple of 2 and also
        //page size is more than usb device block size as returned by Read Capacity Command
        //blocksize == 32 pages,, (16384)

        while (RetryCount)
        {
            Context->LogicalBlkAddr =
                ((Block << (Context->BlockSizeLog2 - (Context->PageSizeLog2))) + Page );
            Context->TransferLen = Size/Context->Usb3Status.BlockLenInByte;

            //Make sure BufferData variable is initialized to requested destination
            //address before issuing READ command
            Context->BufferData = Dest;
            //Send Read command
            ErrorCode = NvBootXusbMscBotProcessRequest(Context, READ10_CMD_OPCODE);

            if (ErrorCode == NvSuccess)
            {
                //DeviceStatus = NvBootDeviceStatus_Idle;
                break;
            }
            else
            {
                // Update Device status
                //DeviceStatus = NvBootDeviceStatus_ReadFailure;
            }
            RetryCount--;
            NvOsWaitUS(TIMEOUT_1MS);
        }
    }
    else
    {
        //either phase error or command failed due to internal problems
        //For failed command try to get sense key and report read failure
        if(ErrorCode == NvError_XusbCswStatusCmdFail)
        {
            //Send Mode Sense command to know the type of problem
            //Sense key is being stored in BIT SecondaryDeviceStatus field
            ErrorCode = NvBootXusbMscBotProcessRequest(Context, REQUESTSENSE_CMD_OPCODE);
        }
    }
    Context->Usb3Status.ReadPageReturnVal = ErrorCode;
    //Context->Usb3Status.DeviceStatus = DeviceStatus;
    return ErrorCode;
}

NvError
NvddkXusbWritePage(NvddkXusbContext* Context,
                   const NvU32 Block,
                   const NvU32 Page,
                   NvU8 *Src,
                   NvU32 Size)
{
    NvError ErrorCode = NvSuccess;
    //NvBootDeviceStatus DeviceStatus = NvBootDeviceStatus_ReadFailure;
    NvU32 RetryCount = USB_MAX_TXFR_RETRIES;

    NV_ASSERT(Page < (NvU32)(1 << ((Context->BlockSizeLog2) - (Context->PageSizeLog2))));
    NV_ASSERT(Src != NULL);

    if (ErrorCode == NvSuccess)
    {
        //Issue Read command
        //It is assumed that usb device block size would be multiple of 2 and also
        //page size is more than usb device block size as returned by
        //Read Capacity Command
        //blocksize == 32 pages,, (16384)

        while(RetryCount)
        {
            Context->LogicalBlkAddr =
                ((Block << (Context->BlockSizeLog2 - (Context->PageSizeLog2))) + Page );
            Context->TransferLen = Size/Context->Usb3Status.BlockLenInByte;

            //Make sure BufferData variable is initialized to requested
            //destination address before issuing READ command
            Context->BufferData = Src;
            //Send Read command
            ErrorCode = NvBootXusbMscBotProcessRequest(Context, WRITE10_CMD_OPCODE);

            if (ErrorCode == NvSuccess)
            {
                //DeviceStatus = NvBootDeviceStatus_Idle;
                break;
            }
            else
            {
                // Update Device status
                //DeviceStatus = NvBootDeviceStatus_ReadFailure;
            }
            RetryCount--;
            NvOsWaitUS(TIMEOUT_1MS);
        }
    }
    else
    {
        //either phase error or command failed due to internal problems
        //For failed command try to get sense key and report read failure
        if(ErrorCode == NvError_XusbCswStatusCmdFail)
        {
            //Send Mode Sense command to know the type of problem
            //Sense key is being stored in BIT SecondaryDeviceStatus field
            ErrorCode = NvBootXusbMscBotProcessRequest(Context, REQUESTSENSE_CMD_OPCODE);
        }
    }
    Context->Usb3Status.ReadPageReturnVal = ErrorCode;
    return ErrorCode;
}
