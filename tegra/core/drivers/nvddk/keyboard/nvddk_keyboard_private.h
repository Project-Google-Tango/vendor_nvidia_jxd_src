/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Driver Development Kit:
 *           KeyBoard Private Interface</b>
 *
 * @b Description: Declares Private interface for the Keyboard module.
 *
 */

#ifndef INCLUDED_NVDDK_KEYBOARD_PRIVATE_H
#define INCLUDED_NVDDK_KEYBOARD_PRIVATE_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_init.h"
#include "nvcommon.h"
#include "nvos.h"

/**
 * @defgroup nvddk_Keyboard Private Interface
 *
 * This is the interface to a hardware keyboard controller and Gpio keys.
 * This keeps track of the keys that are pressed. Only one
 * client is allowed at a time.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * An opaque context to the NvDdkKbcRec interface.
 */
typedef struct NvDdkKbcRec *NvDdkKbcHandle;

typedef enum
{

    /// Indicates the key press event.
    NvDdkKbcKeyEvent_KeyPress = 1,

    /// Indicates the key release event.
    NvDdkKbcKeyEvent_KeyRelease,

    /// Indicates key event none.
    NvDdkKbcKeyEvent_None,
    NvDdkKbcKeyEvent_Num,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkKbcKeyEvent_Force32 = 0x7FFFFFFF
} NvDdkKbcKeyEvent;

/**
 * Initializes the keyboard controller.
 * It allocates resources such as memory, mutexes, and sets up the
 * KBC handle.
 *
 * @param hDevice Handle to the Rm device that is required by NvDDK
 * to acquire the resources from RM.
 * @param phKbc A pointer to the KBC handle where the
 *       allocated handle is stored. The memory for the handle is
 *       allocated inside this API.
 *
 * @retval NvSuccess Open is successful.
 */
 NvError NvDdkKbcOpen(
    NvRmDeviceHandle hDevice,
    NvDdkKbcHandle * phKbc );

/**
 * Releases the KBC handle and releases any resources that
 *   are acquired during the NvDdkKbcOpen() call.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 */

 void NvDdkKbcClose(
    NvDdkKbcHandle hKbc );

/**
 * Enables the keyboard controller. This must be called once to
 *      receive the key events.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 * @param SemaphoreId Semaphore to be signaled on any key event.
 *
 * @retval NvSuccess KBC is enabled successfully.
 */
 NvError NvDdkKbcStart(
    NvDdkKbcHandle hKbc,
    NvOsSemaphoreHandle SemaphoreId );

/**
 * Disables the keyboard controller. This must be called to
 * stop receiving the key events.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 *
 * @retval NvSuccess KBC is disabled successfully.
 */
 NvError NvDdkKbcStop(
    NvDdkKbcHandle hKbc );

/**
 * Sets the repeat time period at which rows must be scanned for key
 * events.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 * @param RepeatTimeMs Repeat time period in milliseconds.
 */
 void NvDdkKbcSetRepeatTime(
    NvDdkKbcHandle hKbc,
    NvU32 RepeatTimeMs );

/**
 * Gets the key events. After calling this function, the caller must sleep
 * for the amount of time returned by this function before calling it again.
 * If the return value is 0, then the client must wait on sema before
 * calling this function again.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 * @param pKeyCount The returned key events count.
 * @param pKeyCodes The returned key codes.
 * @param pKeyEvents The returned key events( press/release).
 *
 * @return Wait time in milliseconds.
 */
 NvU32 NvDdkKbcGetKeyEvents(
    NvDdkKbcHandle hKbc,
    NvU32 * pKeyCount,
    NvU32 * pKeyCodes,
    NvDdkKbcKeyEvent * pKeyEvents );

/**
 * Part of static power management, the client must call this API to put
 * the KBC controller into suspend state. This API is a mechanism for the
 * client to augment OS power management policy. The h/w context of the KBC
 * controller is saved, clock is disabled, and power is also disabled
 * to the controller.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 * @retval NvSuccess If successful, or the appropriate error code.
 */
 NvError NvDdkKbcSuspend(
    NvDdkKbcHandle hKbc );

/**
 * Part of static power management, the client must call this API to
 * wake up the KBC controller from a suspended state. This API is
 * a mechanism for the client to augment OS power management policy.
 * The h/w context of the KBC controller is restored, clock is enabled,
 * and power is also enabled to the controller.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 * @retval NvSuccess If successful, or the appropriate error code.
 */
 NvError NvDdkKbcResume(
    NvDdkKbcHandle hKbc );

/**
 * Initialises KBC before going into suspend.
 *
 * @param hKbc A KBC handle that is allocated by NvDdkKbcOpen().
 */
void NvDdkLp0ConfigureKbc(NvDdkKbcHandle hKbc);

/**
 * This API needs to be called in resume path in BL.
 *
 * Enables clock to KBC after resume.
 */
void NvDdkLp0KbcResume(void);

/**
 * Initializes the GPIO keyboard.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvDdkGpioKeyboardInit(void);

/**
 * Releases the GPIO keyboard resources that were acquired during the
 * call to NvDdkGpioKeyboardInit().
 */
void NvDdkGpioKeyboardDeInit(void);

/**
 * Gets the key data from the GPIO keyboard. This function must be called in
 * an infinite loop to continue receiving the key scan codes.
 *
 * @param pKeyScanCode A pointer to the returned scan code of the key.
 * @param pScanCodeFlags A pointer to the returned value specifying scan code
 *  make/break flags (may be ORed for special code that combines make and
 *  break sequences).
 * @param Timeout (Optional) Specifies the timeout in msec. Can be set
 *  to zero if no timeout needs to be used.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvDdkGpioKeyboardGetKeyData(NvU32 *pKeyScanCode, NvU8 *pScanCodeFlags, NvU32 Timeout);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif
