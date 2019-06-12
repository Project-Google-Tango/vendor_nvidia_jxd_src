/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_GPIO_H
#define INCLUDED_NVRM_GPIO_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_init.h"

/** @file
 * @brief <b>NVIDIA Driver Development Kit: Resrouce Manager GPIO APIs</b>
 *
 * @b Description: This file declares the interface for NvRm GPIO module.
 */

// Docs: Doxygen needs the filename as a scope-resolution operator
// when generating documentation for IDL files due to colliding defs in other
// headers.
 /**
 * @defgroup nvrm_gpio RM GPIO Services
 *
 * This is the Resource Manager interface to general-purpose input-output
 * (GPIO) services. Fundamental abstraction of this API is a "pin handle", which
 * is of type nvrm_gpio::NvRmGpioPinHandle. A pin handle is acquired by making a
 * call to nvrm_gpio::NvRmGpioAcquirePinHandle API. This API returns a pin
 * handle that is subsequently used by the rest of the GPIO APIs.
 *
 * @ingroup nvddk_rm
 * @{
 */

#include "nvcommon.h"
#include "nvos.h"

/**
 *  An opaque handle to the GPIO device on the chip.
 */
typedef struct NvRmGpioRec *NvRmGpioHandle;

/**
 * @brief GPIO pin handle that describes the physical pin. This value should
 * not be cached or hardcoded by the drivers. This can vary from chip to chip
 * and board to board.
 */
typedef NvU32 NvRmGpioPinHandle;

/**
 * @brief Defines the possible GPIO pin modes.
 */
typedef enum
{
    /**
     * Specifies the GPIO pin as not in use. When in this state, the RM or
     * ODM Kit may park the pin in a board-specific state in order to
     * minimize current leakage.
     */
    NvRmGpioPinMode_Inactive = 1,

    /// Specifies GPIO pin mode as input and enables interrupt for level low.
    NvRmGpioPinMode_InputInterruptLow,

    /// Specifies GPIO pin mode as input and enables interrupt for level high.
    NvRmGpioPinMode_InputInterruptHigh,

    /// Specifies GPIO pin mode as input and no interrupt configured.
    NvRmGpioPinMode_InputData,

    /// Specifies GPIO pin mode as output.
    NvRmGpioPinMode_Output,

    /// Specifies GPIO pin mode as a special function.
    NvRmGpioPinMode_Function,

    /// Specifies the GPIO pin as input and interrupt configured to any edge,
    /// i.e., semaphore will be signaled for both the rising and failling edges.
    NvRmGpioPinMode_InputInterruptAny,

    /// Specifies the GPIO pin as input and interrupt configured to rising edge.
    NvRmGpioPinMode_InputInterruptRisingEdge,

    /// Sepcifies the GPIO pin as input and interrupt configured to falling
    /// edge.
    NvRmGpioPinMode_InputInterruptFallingEdge,

    /// Specified the gpio pin interrupt is enabled
    NvRmGpioPinMode_InterruptEnable,

    /// Specified the gpio pin interrupt is disabled
    NvRmGpioPinMode_InterruptDisable,
    NvRmGpioPinMode_Num,
    NvRmGpioPinMode_Force32 = 0x7FFFFFFF
} NvRmGpioPinMode;

/**
 * @brief Defines the pin state.
 */
typedef enum
{

   /// Pin state is low.
    NvRmGpioPinState_Low = 0,

    /// Pin state is high.
    NvRmGpioPinState_High,

    /// Pin is in tri-state.
    NvRmGpioPinState_TriState,
    NvRmGpioPinState_Num,
    NvRmGpioPinState_Force32 = 0x7FFFFFFF
} NvRmGpioPinState;

// Generates a constructor for the pin handle until the NvRmGpioAcquirePinHandle
// API is implemented.
#define GPIO_MAKE_PIN_HANDLE(inst, port, pin)  (0x80000000 | (((NvU32)(pin) & 0xFF)) | (((NvU32)(port) & 0xff) << 8) | (((NvU32)(inst) & 0xff)<< 16))
#define NVRM_GPIO_CAMERA_PORT (0xfe)
#define NVRM_GPIO_CAMERA_INST (0xfe)

/**
 * Creates and opens a GPIO handle. The handle can then be used to
 * access GPIO functions.
 *
 * @param hRmDevice The RM device handle.
 * @param phGpio A pointer to the GPIO handle where the
 * allocated handle is stored. The memory for the handle is allocated
 * inside this API.
 *
 * @retval NvSuccess GPIO initialization is successful.
 */
NvError NvRmGpioOpen(
    NvRmDeviceHandle hRmDevice,
    NvRmGpioHandle * phGpio);

/**
 * Closes the GPIO handle. Any pin settings made while this handle was
 * open will remain. All events enabled by this handle will be
 * disabled.
 *
 * @param hGpio A handle from \c NvRmGpioOpen(). If \a hGpio is NULL, this
 *     API does nothing.
 */
void NvRmGpioClose(
    NvRmGpioHandle hGpio);

/** Gets nvrm_gpio::NvRmGpioPinHandle from the physical port and pin number. If
 * a driver acquires a pin handle, another driver will not be able to use this
 * until the pin is released.
 *
 * @param hGpio A handle from \c NvRmGpioOpen().
 * @param port Physical GPIO ports that are chip-specific.
 * @param pin Pin number in that port.
 * @param phPin Pointer to the GPIO pin handle.
 */
NvError NvRmGpioAcquirePinHandle(
    NvRmGpioHandle hGpio,
    NvU32 port,
    NvU32 pin,
    NvRmGpioPinHandle * phPin);

/** Releases the pin handles acquired by nvrm_gpio::NvRmGpioAcquirePinHandle
 *   API.
 *
 * @param hGpio A handle from \c NvRmGpioOpen().
 * @param hPin Array of pin handles from nvrm_gpio::NvRmGpioAcquirePinHandle().
 * @param pinCount Size of pin handles array.
 */
void NvRmGpioReleasePinHandles(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle * hPin,
    NvU32 pinCount);

/**
 * Sets the state of array of pins.
 *
 * @note When multiple pins are specified (\a pinCount is greater than
 * 1), ODMs should not make assumptions about the order in which
 * pins are updated. The implementation attempts to coalesce
 * updates to occur atomically; however, this can not be guaranteed in
 * all cases, and may not occur if the list of pins includes pins from
 * multiple ports.
 *
 * @param hGpio Specifies the GPIO handle.
 * @param pin Array of pin handles.
 * @param pinState Array of elements specifying the pin state (of type
 * nvrm_gpio::NvRmGpioPinState).
 * @param pinCount Number of elements in the array.
 */
void NvRmGpioWritePins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle * pin,
    NvRmGpioPinState * pinState,
    NvU32 pinCount);

/**
 * Reads the state of array of pins.
 *
 * @param hGpio The GPIO handle.
 * @param pin Array of pin handles.
 * @param pPinState Array of elements specifying the pin state (of type
 * nvrm_gpio::NvRmGpioPinState).
 * @param pinCount Number of elements in the array.
 */
void NvRmGpioReadPins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle * pin,
    NvRmGpioPinState * pPinState,
    NvU32 pinCount);

/**
 * Configures a set of GPIO pins to a specified mode. Do not use this API for
 * the interrupt modes. For interrupt modes, use
 * nvrm_gpio::NvRmGpioInterruptRegister and
 * nvrm_gpio::NvRmGpioInterruptUnregister APIs.
 *
 * @param hGpio The GPIO handle.
 * @param pin Pin handle array returned by a call to
 *  nvrm_gpio::NvRmGpioAcquirePinHandle().
 * @param pinCount Number elements in the pin handle array.
 * @param Mode Pin mode of type nvrm_gpio::NvRmGpioPinMode.
 *
 * @retval NvSuccess Requested operation is successful.
 */
NvError NvRmGpioConfigPins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle * pin,
    NvU32 pinCount,
    NvRmGpioPinMode Mode);

/**
 *  Gets the IRQs associated with the pin handles, so that the client can
 *  register the interrupt callback for that using interrupt APIs.
 *
 * @param hRmDevice The RM device handle.
 * @param pin Pin handle array returned by a call to
 *  nvrm_gpio::NvRmGpioAcquirePinHandle().
 * @param Irq IRQ.
 * @param pinCount Number elements in the pin handle array.
 *
 * @retval NvSuccess Requested operation is successful.
 */
NvError NvRmGpioGetIrqs(
    NvRmDeviceHandle hRmDevice,
    NvRmGpioPinHandle * pin,
    NvU32 * Irq,
    NvU32 pinCount);

/**
 *   Opaque handle to the GPIO interrupt.
 */
typedef struct NvRmGpioInterruptRec *NvRmGpioInterruptHandle;

/*
 * @note Use the 2 APIs below to configure the gpios to interrupt mode and to
 * have callabck functions. For the test case of how to use this APIs refer to
 * the nvrm_gpio_unit_test applicaiton.
 *
 *  Since the ISR is written by the clients of the API, care should be taken to
 *  clear the interrupt before the ISR is returned. If one fails to do that,
 *  interrupt will be triggered soon after the ISR returns.
 */

/**
 * Registers an interrupt callback function and the mode of interrupt for the
 * specified GPIO pin.
 *
 * @note Use this API to configure the GPIOs to interrupt mode and to
 * have callabck functions. For the test case of how to use this API refer to
 * the nvrm_gpio_unit_test applicaiton. In addition,
 * as the ISR is written by the clients of the API, care should be taken to
 * clear the interrupt before the ISR is returned. If one fails to do that,
 * the interrupt will be triggered soon after the ISR returns.
 *
 * Callback will be using the interrupt thread an the interrupt stack on Linux
 * and IST on Windows. So, care should be taken on which APIs can be used on the
 * callback function. Not all the NvOS functions are available in the interrupt
 * context. Check the nvos.h header file for the functions available.
 * When the callback is called, the interrupt on the pin is disabled. As soon as
 * the callback exits, the interrupt is re-enabled. So, external interrupts
 * should be cleared and then only the callback should be returned.
 *
 * @param hGpio The GPIO handle.
 * @param hRm The RM device handle.
 * @param hPin The handle to a GPIO pin.
 * @param Callback Callback function that will be called when the interrupt
 * triggers.
 * @param Mode Interrupt mode. See nvrm_gpio::NvRmGpioPinMode.
 * @param CallbackArg Argument used when the callback is called by the ISR.
 * @param hGpioInterrupt Interrupt handle for this registered intterrupt. This
 * handle should be used while calling nvrm_gpio::NvRmGpioInterruptUnregister
 * for unregistering the interrupt.
 * @param DebounceTime The debounce time in milliseconds.
 * @retval NvSuccess requested operation is successful.
 */
NvError
NvRmGpioInterruptRegister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioPinHandle hPin,
    NvOsInterruptHandler Callback,
    NvRmGpioPinMode Mode,
    void *CallbackArg,
    NvRmGpioInterruptHandle *hGpioInterrupt,
    NvU32 DebounceTime);

/**
 * Unregister the GPIO interrupt handler.
 *
 * @param hGpio The GPIO handle.
 * @param hRm The RM device handle.
 * @param handle The interrupt handle returned by a successfull call to the
 * nvrm_gpio::NvRmGpioInterruptRegister API.
 *
 */
void
NvRmGpioInterruptUnregister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioInterruptHandle handle);

/**
 * Enable the GPIO interrupt handler.
 *
 * @param handle The interrupt handle returned by a successfull call to the
 * nvrm_gpio::NvRmGpioInterruptRegister API.
 *
 * @retval NvError_BadParameter If handle is not valid.
 * @retval NvError_InsufficientMemory If interupt enable fails.
 * @retval NvSuccess If registration is successfull.
*/
NvError
NvRmGpioInterruptEnable(NvRmGpioInterruptHandle handle);

/*
 * Callback used to re-enable the interrupts.
 *
 * @param handle The interrupt handle returned by a successfull call to the
 * nvrm_gpio::NvRmGpioInterruptRegister API.
 */
void
NvRmGpioInterruptDone(NvRmGpioInterruptHandle handle);

/**
 * Mask/unmask a GPIO interrupt.
 *
 * Drivers can use this API to fend off interrupts. Mask means interrupts are
 * not forwarded to the CPU. Unmask means, interrupts are forwarded to the CPU.
 * In the case of SMP systems, this API masks the interrutps to all the CPUs,
 * not just the calling CPU.
 *
 * @param hGpioInterrupt Interrupt handle returned by
 *    nvrm_gpio::NvRmGpioInterruptRegister.
 * @param mask NV_FALSE to forward the interrupt to CPU; NV_TRUE to
 * mask the interupts to CPU.
 */
void
NvRmGpioInterruptMask(NvRmGpioInterruptHandle hGpioInterrupt, NvBool mask);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
