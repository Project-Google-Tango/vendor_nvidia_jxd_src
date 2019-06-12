/*
 * Copyright (c) 2008 - 2013, NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml.h"

NvU32 AppletPrivate_BitLoadAddress = 0x40000000;

NvU8
NvBootStrapSdramConfigurationIndex(void) {
    NvU32 regVal ;

#ifdef PMC_BASE
    regVal = NV_READ32(PMC_BASE + APBDEV_PMC_STRAPPING_OPT_A_0);

    return (( NV_DRF_VAL(APBDEV_PMC, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK);
#else
    regVal = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + APB_MISC_PP_STRAPPING_OPT_A_0);

    return (( NV_DRF_VAL(APB_MISC_PP, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK);
#endif
}


NvU8
NvBootStrapDeviceConfigurationIndex(void) 
{
    NvU32 regVal ;

#ifdef PMC_BASE
    regVal = NV_READ32(PMC_BASE + APBDEV_PMC_STRAPPING_OPT_A_0);

    return (( NV_DRF_VAL(APBDEV_PMC, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK);
#else

    regVal = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + APB_MISC_PP_STRAPPING_OPT_A_0);

    return (( NV_DRF_VAL(APB_MISC_PP, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK);
#endif
}

void ReadBootInformationTable(NvBootInfoTable **ppBitTable)
{
    NvBootInfoTable *pBitTable;

    // the BIT is loaded at 0x4000:0000. So it can be read directly from there
    pBitTable = (NvBootInfoTable *)AppletPrivate_BitLoadAddress;   

#if defined(BOOTROM_ZERO_BIT_WAR_390163)
    // Workaround for missing BIT in Boot ROM version 1.000
    //
    // Rev 1.000 of the Boot ROM incorrectly clears the BIT when a Forced
    // Recovery is performed.  Fortunately, very few fields of the BIT are
    // set as a result of a Forced Recovery, so most everything can be
    // reconstructed after the fact.
    //
    // One piece of information that is lost is the source of the
    // Forced Recovery -- by strap or by flag (PMC Scratch0).  If the
    // flag was set, then it is cleared and the ClearedForceRecovery
    // field is set to NV_TRUE.  Since the BIT has been cleared by
    // time an applet is allowed to execute, there's no way to
    // determine if the flag in PMC Scratch0 has been cleared.  If the
    // strap is not set, though, then one can infer that the flag must
    // have been set (and subsequently cleared).  However, if the
    // strap is not set, there's no way to know whether or not the
    // flag was also set.
 
    if (pBitTable->BootRomVersion == 0x0
        && pBitTable->DataVersion == 0x0
        && pBitTable->RcmVersion == 0x0)
    {
        NvOsMemset( (void*) pBitTable, 0, sizeof(NvBootInfoTable) ) ;
 
        pBitTable->BootRomVersion = NVBOOT_VERSION(1,0);
        pBitTable->DataVersion = NVBOOT_VERSION(1,0);
        pBitTable->RcmVersion = NVBOOT_VERSION(1,0);
        pBitTable->BootType = NvBootType_Recovery;
        pBitTable->PrimaryDevice = NvBootDevType_Irom;
 
        pBitTable->OscFrequency = NvBootClocksGetOscFreq();

        // TODO -- check Force Recovery strap to partially infer whether PMC
        //         Scratch0 flag was cleared (see note above for details)

        pBitTable->ClearedForceRecovery = NV_FALSE;
        pBitTable->SafeStartAddr = (NvU32)pBitTable + sizeof(NvBootInfoTable);
    }
#endif

    *ppBitTable = pBitTable;
}
