/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef AP15RM_PRIVATE_H
#define AP15RM_PRIVATE_H

/*
 * ap15rm_private.h defines the private implementation functions for the
 * resource manager.
 */

#include "nvcommon.h"
#include "nvrm_structure.h"
#include "nvrm_power_private.h"
#include "nvodm_query.h"
#include "nvos.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// Enable this macro to catch spurious interrupts. By default this is disabled
// as we allow spurious interrupts from GPIO controller.
#if 0
#define NVRM_INTR_DECODE_ASSERT(x) NV_ASSERT(x)
#else
#define NVRM_INTR_DECODE_ASSERT(x)
#endif

/** Driver init for interrupts.
 */
void
NvRmPrivInterruptTableInit( NvRmDeviceHandle hDevice );

/**
 * Enable interrupt source for interrupt decoder.
 */
/**
 * Disable interrupt source for interrupt decoder.
 */

/**
 * Main controller interrupt enable/disable for sub-controllers.
 */

/**
 * Interrupt source enable/disable for AP15 main interrupt controllers.
 */

// Initialize/deinitialize for various RM submodules.
NvError NvRmPrivDmaInit(NvRmDeviceHandle hDevice);
void NvRmPrivDmaDeInit(void);

NvError NvRmPrivSpiSlinkInit(NvRmDeviceHandle hDevice);
void NvRmPrivSpiSlinkDeInit(void);

/**
 * Retrieves module instance record pointer given module ID
 *
 * @param hDevice The RM device handle
 * @param ModuleId The combined module ID and instance of the target module
 * @param out Output storage pointer for instance record pointer
 *
 * @retval NvSuccess if instance pointer was successfully retrieved
 * @retval NvError_BadValue if module ID is invalid
 */
NvError
NvRmPrivGetModuleInstance(
     NvRmDeviceHandle hDevice,
     NvRmModuleID ModuleId,
     NvRmModuleInstance **out);
/**
 * List module instance records for debugging
 *
 * @param hDevice The RM device handle
 */
void
NvRmPrivListModuleInstances(NvRmDeviceHandle hDevice);

/*
 *  OS specific interrupt initialization
 */
void
NvRmPrivInterruptStart(NvRmDeviceHandle hDevice);

/**
 * Clear out anything that registered for an interrupt but didn't clean up
 * afteritself.
 */

void
NvRmPrivInterruptShutdown(NvRmDeviceHandle hDevice);

/**
 * Initializes the RM's internal state for tracking the pin-mux register
 * configurations.  This is done by iteratively applying the pre-defined
 * configurations from ODM Query (see nvodm_query_pinmux.c).  This function
 * applies an "enable" setting when there's a match against the static
 * declarations (in ODM Query).
 *
 * As this function walks the configuration list defined in ODM Query, it does
 * *not* disable (apply tristate settings to) unused pin-groups for a given I/O
 * module's configuration.  That would be an exercise in futility, since the
 * current I/O module cannot know if another I/O module is using any unclaimed
 * pin-groups which the current I/O module configuration might otherwise use.
 * That system-wide view of pin-group resources is the responsibility of the
 * System Designer who selects pin-group combinations from the pin-mux
 * documentation (see //sw/mobile/docs/hw/ap15/pin_mux_configurations.xls).
 * The selected combination of pin-mux settings (which cannot be in conflict)
 * are then saved to the configuration tables in ODM Query.
 *
 * Further, this initialization routine enables the configuration identified by
 * the ODM Query tables.  Any pre-existing settings are not changed, except as
 * defined by the static configuration tables in ODM Query.  Therefore, the
 * System Designer *must* also account for pre-existing power-on-reset (POR)
 * values when determining the valid pin-mux configurations saved in ODM Query.
 *
 * Finally, any use of the pin-mux registers prior to RM initialization *must*
 * be consistent with the ODM Query tables, otherwise the system configuration
 * is not deterministic (and may violate the definition applied by the System
 * Designer).  Once RM initializes its pin-mux state, any direct access to the
 * pin-mux registers (ie, not using the RM PinMux API) is strictly prohibited.
 *
 * @param hDevice The RM device handle.
 */
void
NvRmPrivInitPinMux(NvRmDeviceHandle hDevice);

/**
 * Create a mapping from a list of physical sdram pages to the GART.  No error
 * checking is done here, so you can clobber your friend's mappings if you
 * want. Every map for itself!  This function programs registers and cannot
 * fail. Invalid parameters will result in asserts for debug purposes.
 *
 * @see NvRmPrivGartAlloc()
 *
 * @param hDevice The RM device handle.
 * @param pPhysAddrArray Points to an array of physical page addresses.  Each
 *  entry represents the base address for a 4KB page of memory.  These entries
 *  do not need to be contiguous memory blocks; after all, that's why you're
 *  using this API.
 * @param NumberOfPages Specifies the number of physical address entries.  A
 * value of 0 has no effect.
 * @param FirstGartPage Specifies the base address of the first available GART
 *  page.  This value should be obtained via a call to NvRmPrivGartAlloc().
 */
void
NvRmPrivCreateGARTMap(
    NvRmDeviceHandle hDevice,
    NvU32 *pPhysAddrArray,
    NvU32 NumberOfPages,
    NvU32 FirstGartPage);

/**
 * Suspend GART.
 */
void NvRmPrivGartSuspend( NvRmDeviceHandle hDevice );

/**
 * Resume GART.
 */

void NvRmPrivGartResume(NvRmDeviceHandle hDevice);

/**
 * Initializes the clock manager.
 *
 * @param hRmDevice The RM device handle
 *
 * @return NvSuccess if initialization completed successfully
 *  or one of common error codes on failure
 */
NvError
NvRmPrivClocksInit(NvRmDeviceHandle hRmDevice);

/**
 * Deinitializes the clock manager.
 *
 * @param hRmDevice The RM device handle
 */
void
NvRmPrivClocksDeinit(NvRmDeviceHandle hRmDevice);

/**
 * Increments a memory handle reference count.
 */
void
NvRmPrivMemIncrRef( NvRmMemHandle hMem );


/*** Private Interrupt API's ***/


/**
 * Performs primary interrupt decode for IRQ interrupts in the main
 * interrupt controllers.
 *
 * @param hRmDevice The RM device handle.
 * @returns The IRQ number of the interrupting device or NVRM_IRQ_INVALID
 * if no interrupting device was found.
 */


/**
 * Performs secondary IRQ interrupt decode for interrupting devices
 * that are interrupt sub-controllers.
 *
 * @param hRmDevice The RM device handle.
 * @param Irq Primary IRQ number returned from NvRmInterruptPrimaryDecodeIrq().
 * @returns The IRQ number of the interrupting device.
 */



/**
 * Performs primary interrupt decode for FIQ interrupts in the main
 * interrupt controllers.
 *
 * @param hRmDevice The RM device handle.
 * @returns The IRQ number of the interrupting device or NVRM_IRQ_INVALID
 * if no interrupting device was found.
 */



/**
 * Performs secondary FIQ interrupt decode for interrupting devices
 * that are interrupt sub-controllers.
 *
 * @param hRmDevice The RM device handle.
 * @param Fiq Primary FIQ number returned from NvRmInterruptPrimaryDecodeFiq().
 * @returns The FIQ number of the interrupting device.
 */


/**
 * Suspend the dma.
 */
NvError NvRmPrivDmaSuspend(void);

/**
 * Resume the dma.
 */
NvError NvRmPrivDmaResume(void);

/**
 * Check Bond Out to make a module/instance invalid.
 *
 * @param hRm The RM device handle
 */
void NvRmPrivCheckBondOut( NvRmDeviceHandle hDevice );

/** Returns bond out values and table for T30 */
void NvRmGetBondOut( NvRmDeviceHandle hDevice,
                        const NvU32 **pTable, NvU32 *bondOut );

/**
 * This API should be sapringly used. There is a bug in the chiplib where the
 * interrupt handler is not passed an argument. So, the handler will call this
 * function to get the Rm handle.
 */
NvRmDeviceHandle NvRmPrivGetRmDeviceHandle( void );

/** Returns the pointer to the relocation table of T30 chip */
NvU32 *NvRmPrivT30GetRelocationTable( NvRmDeviceHandle hDevice );

/** Returns the pointer to the relocation table of T11x chip */
NvU32 *NvRmPrivT11xGetRelocationTable( NvRmDeviceHandle hDevice );

/** Returns the pointer to the relocation table of T12X chip */
NvU32 *NvRmPrivT12XGetRelocationTable( NvRmDeviceHandle hDevice );

/** Basic reset of chip modules */
void NvRmBasicReset( NvRmDeviceHandle hDevice );

/** This API starts the memory controller error monitoring. */
NvError NvRmPrivMcErrorMonitorStart( NvRmDeviceHandle hDevice );

/** This API stops the memory controller error monitoring. */
void NvRmPrivMcErrorMonitorStop( NvRmDeviceHandle hDevice );

/** This API sets up the memory controller. */
void NvRmPrivSetupMc(NvRmDeviceHandle hRm);


/* init and deinit the keylist */
NvError NvRmPrivInitKeyList(NvRmDeviceHandle hRm, const NvU32*, NvU32);
void NvRmPrivDeInitKeyList(NvRmDeviceHandle hRm);

/**
 * @brief Query the max interface freq supported by the board for a given
 * Module.
 *
 * This API returns the max interface freq supported by the board based on the
 * ODM query.
 */
NvRmFreqKHz
NvRmPrivGetInterfaceMaxClock(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID ModuleId);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // AP15RM_PRIVATE_H
