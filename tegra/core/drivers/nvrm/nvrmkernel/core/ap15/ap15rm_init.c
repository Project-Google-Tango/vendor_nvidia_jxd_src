/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVRM_TRANSPORT_IN_KERNEL
#define NVRM_TRANSPORT_IN_KERNEL 0
#endif

#include "nvcommon.h"
#include "nvos.h"
#include "cpu.h"
#include "nvutil.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_init.h"
#include "nvrm_rmctrace.h"
#include "nvrm_configuration.h"
#include "nvrm_heap.h"
#include "nvrm_pmu_private.h"
#include "nvrm_processor.h"
#include "nvrm_xpc.h"
#include "ap15rm_private.h"
#include "nvrm_structure.h"
#include "ap15rm_private.h"
#include "ap15rm_clocks.h"
#include "nvodm_query.h"
#include "common/nvrm_hwintf.h"
#include "ap20/armc.h"
#include "ap20/aremc.h"
#include "ap20/project_relocation_table.h"
#include "ap20/arapb_misc.h"
#include "ap20/arapbpm.h"
#include "nvrm_pinmux_utils.h"
#include "nvrm_keylist.h"
#include "nvodm_keylist_reserved.h"
#include "nvrm_pinmux_utils.h"

// FIXME: this is defined in a header that's not released, etc
NvError
NvRmGraphicsOpen(NvRmDeviceHandle rm );
void
NvRmGraphicsClose(NvRmDeviceHandle handle);

static NvRmDevice gs_Rm;

extern NvRmCfgMap g_CfgMap[];

void NvRmPrivMemoryInfo( NvRmDeviceHandle hDevice );
extern NvError NvRmPrivMapApertures( NvRmDeviceHandle rm );
extern void NvRmPrivUnmapApertures( NvRmDeviceHandle rm );
extern NvError NvRmPrivPwmInit(NvRmDeviceHandle hRm);
extern void NvRmPrivPwmDeInit(NvRmDeviceHandle hRm);
extern NvU32 NvRmPrivGetBctCustomerOption(NvRmDeviceHandle hRm);
extern void NvRmPrivReadChipId( NvRmDeviceHandle rm );
extern NvU32 *NvRmPrivGetRelocationTable( NvRmDeviceHandle hDevice );
extern NvU32 nvaos_GetMaxMemorySize(void);
extern NvUPtr nvaos_GetMemoryStart(void);

NvError
NvRmOpen(NvRmDeviceHandle *pHandle, NvU32 DeviceId ) {
    return NvRmOpenNew(pHandle);
}

void NvRmInit(
    NvRmDeviceHandle * pHandle )
{
    NvU32 *table = 0;
    NvRmDevice *rm = 0;
    NvU32 BctCustomerOption = 0;
    rm = &gs_Rm;

    if( rm->bPreInit )
    {
        *pHandle = rm;
        return;
    }
    //check if keylist is already initialized
    BctCustomerOption = NvRmGetKeyValue(rm,
                        NvOdmKeyListId_ReservedBctCustomerOption);
    // Initializing the ODM-defined key list
    if(!BctCustomerOption)
    {
        BctCustomerOption = NvRmPrivGetBctCustomerOption(rm);
        //ignore errors
        NvRmPrivInitKeyList(rm, &BctCustomerOption, 1);
    }
    /* Read the chip Id and store in the Rm structure. */
    NvRmPrivReadChipId( rm );

    /* parse the relocation table */
    table = NvRmPrivGetRelocationTable( rm );
    NV_ASSERT(table != NULL);

    NV_ASSERT_SUCCESS(NvRmPrivModuleInit( &rm->ModuleTable, table ));

    NvRmPrivMemoryInfo( rm );

    NvRmPrivInterruptTableInit( rm );

    rm->bPreInit = NV_TRUE;
    *pHandle = rm;

    return;
}

NvError
NvRmOpenNew(NvRmDeviceHandle *pHandle)
{
    NvError err;
    NvRmDevice *rm = 0;
    NvU32 *table = 0;

    NvU32 CarveoutBaseAddr;
    NvU32 CarveoutSize = 0;
    NvU32 BctCustomerOption = 0;

    NvOsMutexHandle rmMutex = NULL;

    /* open the nvos trace file */
    NVOS_TRACE_LOG_START;

    // OAL does not support these mutexes
    if (gs_Rm.mutex == NULL)
    {
        err = NvOsMutexCreate(&rmMutex);
        if (err != NvSuccess)
            return err;

        if (NvOsAtomicCompareExchange32((NvS32*)&gs_Rm.mutex, 0,
                (NvS32)rmMutex) != 0)
            NvOsMutexDestroy(rmMutex);
    }

    NvOsMutexLock(gs_Rm.mutex);
    rm = &gs_Rm;

    if(rm->refcount )
    {
        rm->refcount++;
        *pHandle = rm;
        NvOsMutexUnlock(gs_Rm.mutex);
        return NvSuccess;
    }

    rmMutex = gs_Rm.mutex;
    gs_Rm.mutex = rmMutex;

    // create the memmgr mutex
    err = NvOsMutexCreate(&rm->MemMgrMutex);
    if (err)
        goto fail;

    // create mutex for the clock and reset r-m-w top level registers access
    err = NvOsMutexCreate(&rm->CarMutex);
    if (err)
        goto fail;

    /* NvRmOpen needs to be re-entrant to allow I2C, GPIO and KeyList ODM
     * services to be available to the ODM query.  Therefore, the refcount is
     * bumped extremely early in initialization, and if any initialization
     * fails the refcount is reset to 0.
     */
    rm->refcount = 1;

    if( !rm->bBasicInit )
    {
        /* get the default configuration */
        err = NvRmPrivGetDefaultCfg( g_CfgMap, &rm->cfg );
        if( err != NvSuccess )
        {
            goto fail;
        }

        /* get the requested configuration */
        err = NvRmPrivReadCfgVars( g_CfgMap, &rm->cfg );
        if( err != NvSuccess )
        {
            goto fail;
        }
    }

    /* open the RMC file */
    err = NvRmRmcOpen( rm->cfg.RMCTraceFileName, &rm->rmc );
    if( err != NvSuccess )
    {
        goto fail;
    }

    if( !rm->bPreInit )
    {
        /* Read the chip Id and store in the Rm structure. */
        NvRmPrivReadChipId( rm );

        /* parse the relocation table */
        table = NvRmPrivGetRelocationTable( rm );
        if( !table )
        {
            goto fail;
        }

        err = NvRmPrivModuleInit( &rm->ModuleTable, table );
        if( err != NvSuccess )
        {
            goto fail;
        }
        NvRmPrivMemoryInfo( rm );

        // Now populate the logical interrupt table.
        NvRmPrivInterruptTableInit( rm );
    }

    if (!rm->bBasicInit)
    {
        err = NvRmPrivMapApertures( rm );
        if( err != NvSuccess )
        {
            goto fail;
        }

        //check if keylist is already initialized
        BctCustomerOption = NvRmGetKeyValue(rm,
                            NvOdmKeyListId_ReservedBctCustomerOption);
        // Initializing the ODM-defined key list
        //  This gets initialized first, since the RMs calls into
        //  the ODM query may result in the ODM query calling
        //  back into the RM to get this value!
        if(!BctCustomerOption)
        {
            BctCustomerOption = NvRmPrivGetBctCustomerOption(rm);
            err = NvRmPrivInitKeyList(rm, &BctCustomerOption, 1);
            if (err != NvSuccess)
            {
                goto fail;
            }
        }
    }

    // prevent re-inits
    rm->bBasicInit = NV_TRUE;
    rm->bPreInit = NV_TRUE;

#if !NVOS_IS_QNX
    {
        NvU32 SecuredSize;
        NvU32 SecuredTop;
        NvU32 MemorySize;

        SecuredSize = NvOdmQuerySecureRegionSize();
        MemorySize = NV_MIN((TXX_EXT_MEM_END - TXX_EXT_MEM_START),
                                      NvOdmQueryMemSize(NvOdmMemoryType_Sdram));
        switch (rm->ChipId.Id)
        {
            case 0x30:
            case 0x35:
            case 0x40:
            case 0x14:
                SecuredTop = TXX_EXT_MEM_START + MemorySize;
                break;
            default:
                NV_ASSERT(!"Unsupported chip ID");
                return NvError_NotSupported;
        }
        CarveoutSize = NvOdmQueryCarveoutSize();
        CarveoutBaseAddr= SecuredTop - SecuredSize - CarveoutSize;
    }

    NvRmPrivHeapCarveoutInit(CarveoutSize, CarveoutBaseAddr);
    NvRmPrivHeapIramInit(rm->IramMemoryInfo.size, rm->IramMemoryInfo.base);
#endif

    // Initialize the GART heap (size & base address)
    NvRmPrivHeapGartInit( rm );

    NvRmPrivCheckBondOut( rm );

    /* bring modules out of reset */
    NvRmBasicReset( rm );

    /* initialize power manager before any other module that may access
     * clock or voltage resources
     */
    err = NvRmPrivPowerInit(rm);
    if( err != NvSuccess )
    {
        goto fail;
    }

    NvRmPrivInterruptStart( rm );

    // Initalize the module clocks.
    err = NvRmPrivClocksInit( rm );
    if( err != NvSuccess )
    {
        goto fail;
    }

#if !(NVOS_IS_LINUX || NVOS_IS_QNX) && !defined(BOOT_MINIMAL_BL)
    err = NvRmGraphicsOpen( rm );
    if( err != NvSuccess )
    {
        goto fail;
    }
#endif

    if (!NVOS_IS_WINDOWS_X86)
    {
        // FIXME: this crashes in simulation
        // Enabling only for the non simulation modes.
        if ((rm->ChipId.Major == 0) && (rm->ChipId.Netlist == 0))
        {
            // this is the csim case, so we don't do this here.
        }
        else
        {
            // Initializing the dma.
            err = NvRmPrivDmaInit(rm);
            if( err != NvSuccess )
            {
                goto fail;
            }

#ifdef CHIP_T30
            // Initializing the Spi and Slink.
            err = NvRmPrivSpiSlinkInit(rm);
            if( err != NvSuccess )
            {
                goto fail;
            }
#endif

            // Initializing the dfs
            err = NvRmPrivDfsInit(rm);
            if( err != NvSuccess )
            {
                goto fail;
            }
        }

        // Initializing the Pwm
        err = NvRmPrivPwmInit(rm);
        if (err != NvSuccess)
        {
            goto fail;
        }

        // PMU interface init utilizes ODM services that reenter NvRmOpen().
        // Therefore, it shall be performed after refcount is set so that
        // reentry has no side-effects except bumping refcount. The latter
        // is reset below so that RM can be eventually closed.
        err = NvRmPrivPmuInit(rm);
        if( err != NvSuccess )
        {
            goto fail;
        }
        // set the mc & emc tuning parameters
        NvRmPrivSetupMc(rm);

        // Configure PLL rails, boost core power and clocks
        // Initialize and start temperature monitoring
        NvRmPrivPllRailsInit(rm);
        NvRmPrivBoostClocks(rm);
        NvRmPrivDttInit(rm);

        // Asynchronous interrupts must be disabled until the very end of
        // RmOpen. They can be enabled just before releasing rm mutex after
        // completion of all initialization calls.
        NvRmPrivPmuInterruptEnable(rm);

#ifdef CHIP_T30
        // Start Memory Controller Error monitoring.
        err = NvRmPrivMcErrorMonitorStart(rm);
        if( err != NvSuccess )
        {
            goto fail;
        }
#endif
    }
#if NVRM_TRANSPORT_IN_KERNEL
    err = NvRmXpcInitArbSemaSystem(rm);
    if( err != NvSuccess )
    {
        goto fail;
    }
#endif  //NVRM_TRANSPORT_IN_KERNEL

    /* assign the handle pointer */
    *pHandle = rm;

    NvOsMutexUnlock(gs_Rm.mutex);
    return NvSuccess;

fail:
    // FIXME: free rm if it becomes dynamically allocated
    // BUG:  there are about ten places that we go to fail, and we make no
    // effort here to clean anything up.
    NvOsMutexUnlock(gs_Rm.mutex);
    NV_DEBUG_PRINTF(("RM init failed\n"));
    rm->refcount = 0;
    return err;
}

void
NvRmClose(NvRmDeviceHandle handle)
{
    if( !handle )
    {
        return;
    }

    NV_ASSERT( handle->mutex );

    /* decrement refcount */
    NvOsMutexLock( handle->mutex );
    handle->refcount--;

    /* do deinit if refcount is zero */
    if( handle->refcount == 0 )
    {
        // PMU and DTT deinit through ODM services reenters NvRmClose().
        // The refcount will wrap around and this will be the only reentry
        // side-effect, which is compensated after deint exit.
        NvRmPrivDttDeinit();
        handle->refcount = 0;
        NvRmPrivPmuDeinit(handle);
        handle->refcount = 0;

#if !(NVOS_IS_LINUX || NVOS_IS_QNX) && !defined(BOOT_MINIMAL_BL)
        NvRmGraphicsClose( handle );
#endif
        if (!NVOS_IS_WINDOWS_X86)
        {
            /* disable modules */
            // Enabling only for the non simulation modes.
            if ((handle->ChipId.Major == 0) && (handle->ChipId.Netlist == 0))
            {
                // this is the csim case, so we don't do this here.
            }
            else
            {
                NvRmPrivDmaDeInit();
#ifndef BOOT_MINIMAL_BL
                NvRmPrivSpiSlinkDeInit();
#endif
                NvRmPrivDfsDeinit(handle);
            }

            /* deinit clock manager */
            NvRmPrivClocksDeinit(handle);

            /* deinit power manager */
            NvRmPrivPowerDeinit(handle);

            NvRmPrivDeInitKeyList(handle);
            handle->bBasicInit = NV_FALSE;

            NvRmPrivPwmDeInit(handle);
#ifdef CHIP_T30
            // Stop Memory controller error monitoring.
            NvRmPrivMcErrorMonitorStop(handle);
#endif
            /* if anyone left an interrupt registered, this will clear it. */
            NvRmPrivInterruptShutdown(handle);

            /* unmap the apertures */
            NvRmPrivUnmapApertures( handle );
        }

#if !NVOS_IS_QNX
        NvRmPrivHeapCarveoutDeinit();
        NvRmPrivHeapIramDeinit();
#endif

        // De-Initialize the GART heap
        NvRmPrivHeapGartDeinit();

        NvRmRmcClose( &handle->rmc );

        /* deallocate the instance table */
        NvRmPrivModuleDeinit( &handle->ModuleTable );

#if NVRM_TRANSPORT_IN_KERNEL
        NvRmXpcDeinitArbSemaSystem(handle);
#endif

        /* free up the CAR mutex */
        NvOsMutexDestroy(handle->CarMutex);

        /* free up the memmgr mutex */
        NvOsMutexDestroy(handle->MemMgrMutex);

        /* close the nvos trace file */
        NVOS_TRACE_LOG_END;
    }
    NvOsMutexUnlock( handle->mutex );
}

void
NvRmShutdown()
{
    NvRmDevice *rm = 0;
    rm = &gs_Rm;

    /* Till the handle is closed completely (refcount = 0) */
    while(rm->refcount)
        NvRmClose(rm);
}

void
NvRmPrivMemoryInfo( NvRmDeviceHandle hDevice )
{
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;

    tbl = &hDevice->ModuleTable;

    /* Get External memory module info */
    inst = tbl->ModInst +
        (tbl->Modules)[NvRmPrivModuleID_ExternalMemory].Index;

    hDevice->ExtMemoryInfo.base = inst->PhysAddr;
    hDevice->ExtMemoryInfo.size = inst->Length;

    /* Get Iram Memory Module Info .Special handling since iram has 4 banks
     * and each has a different instance in the relocation table
     */

    inst = tbl->ModInst + (tbl->Modules)[NvRmPrivModuleID_Iram].Index;
    hDevice->IramMemoryInfo.base = inst->PhysAddr;
    hDevice->IramMemoryInfo.size = inst->Length;

    inst++;
    // Below loop works assuming that relocation table parsing compacted
    // scattered multiple instances into sequential list
    while(NvRmPrivDevToModuleID(inst->DeviceId) == NvRmPrivModuleID_Iram)
    {
        // The IRAM banks are contigous address of memory. Cannot handle
        // non-contigous memory for now
        NV_ASSERT(hDevice->IramMemoryInfo.base +
            hDevice->IramMemoryInfo.size == inst->PhysAddr);

        hDevice->IramMemoryInfo.size += inst->Length;
        inst++;
    }

    {
        /* Get GART memory module info */
        /* T30 and up do not have GART */
        NvU16 index = tbl->Modules[NvRmPrivModuleID_Gart].Index;
        hDevice->GartMemoryInfo.base = ~0;    // Invalidate by default
        if (index == NVRM_MODULE_INVALID)
        {/* T30 SMMU */
            enum t30rev {T30UNK, T30A01, T30A02} which = T30UNK;
            const NvU32 SMMUBase[] = {0, 0xe0000000, 0x01000000};
            const NvU32 SMMUSize[] = {0, 0x10000000, 0x40000000 - 0x01001000};
            NvRmChipId *cid = NvRmPrivGetChipId(hDevice);

            if (cid->Id == 0x30)
            {
                switch (cid->Major)
                {
                case 0:
                    switch (cid->Minor)
                    {
                    case 0:
                        // CSIM assumes latest revision
                        which = T30A02;
                        break;
                    case 1:
                        // FPGA
                        if (cid->Netlist == 12 && cid->Patch == 12)
                            which = T30A01;
                        else if (cid->Netlist == 12 && cid->Patch > 12)
                            which = T30A02;
                        else if (cid->Netlist > 12)
                            which = T30A02;

                        if (which != T30UNK)
                            break;
                        // else falls through
                    default:
                        NV_ASSERT(!"Tegra3 revision cannot be determined");
                    }
                    break;
                case 1:
                    break;
                default:
                    NV_ASSERT(!"Tegra3 revision cannot be determined");
                    break;
                }
            }
            else
            {
                // All successor chips to T30 use the T30A02 model.
                which = T30A02;
            }

            if (which == T30UNK)
            {
                switch (cid->Minor)
                {
                    case 1:
                        which = T30A01;
                        break;
                    default:
                        which = T30A02;
                        break;
                }
            }

            hDevice->GartMemoryInfo.base = SMMUBase[which];
            hDevice->GartMemoryInfo.size = SMMUSize[which];
        }
        else
        {
            inst = tbl->ModInst + index;
            hDevice->GartMemoryInfo.base = inst->PhysAddr;
            hDevice->GartMemoryInfo.size = inst->Length;
        }
    }
}

NvError
NvRmGetRmcFile( NvRmDeviceHandle hDevice, NvRmRmcFile **file )
{
    NV_ASSERT(hDevice);

    *file = &hDevice->rmc;
    return NvSuccess;
}

NvRmDeviceHandle NvRmPrivGetRmDeviceHandle()
{
    return &gs_Rm;
}

