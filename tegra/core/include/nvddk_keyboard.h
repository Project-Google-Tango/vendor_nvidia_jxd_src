/*
 * Copyright (c) 2013  NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_KEYBOARD_H
#define INCLUDED_NVDDK_KEYBOARD_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
#include "nvos.h"

typedef enum
{
    KEY_RELEASE_FLAG = 1,
    KEY_PRESS_FLAG,
    KEY_HOLD_FLAG,
    KEY_REPEAT_FLAG,
    KEY_EVENT_IGNORE = 0xFF
}NvddkKeyboardEvent;

typedef enum
{
    KEY_DOWN = 1,
    KEY_UP,
    KEY_UP_DOWN,
    KEY_ENTER,
    KEY_DOWN_ENTER,
    KEY_UP_ENTER,
    KEY_UP_DOWN_ENTER,
    KEY_HOLD,
    KEY_POWER,
    KEY_IGNORE = 0xFF
}NvddkKeyboardCode;

/**
 * Initializes the keyboard.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvError NvDdkKeyboardInit(NvBool *IsKeyboardInitialised);

/**
 * Releases the keyboard resources that were acquired during the
 * call to keyboard init.
 */
void NvDdkKeyboardDeinit(void);

/**
 * Gets the key data from the keyboard.
 * @return KeyCode of pressed key
 */
NvU8 NvDdkKeyboardGetKeyEvent(NvU8 *KeyEvent);

/**
 * Initialises the keyboard before going into suspend.
 */
void NvDdkLp0ConfigureKeyboard(void);

/**
 * Enables clock to keyboard after resume.
 */
void NvDdkLp0KeyboardResume(void);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_KEYBOARD_H
