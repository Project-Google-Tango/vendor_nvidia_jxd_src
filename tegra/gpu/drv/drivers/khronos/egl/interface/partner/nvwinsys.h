/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVWINSYS_H
#define INCLUDED_NVWINSYS_H

#include "nvcommon.h"
#include "nverror.h"

/**
 * @brief Simple window system abstraction for test applications.  Allows you
 * to create an app that properly interoperates with the OS's window system.
 * Inspired in large part by other toolkits such as GLUT.
 *
 * Note that this API assumes that a desktop (and any associated resources,
 * like a primary surface) already exists and that a means already exists to
 * view it and interact with it.  Creating and configuring a desktop (which
 * might involve, for example, editing an XF86 config file and launching an X
 * server process) is beyond the scope of this API.
 */

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Opaque handle to a desktop.  A desktop is a place where one or more windows
 * can exist and a user can see or interact with them.  Desktops may not have a
 * one-to-one mapping to physical consoles -- you might access a desktop
 * remotely (X protocol, VNC, Remote Desktop), or a computer might have more
 * than one desktop locally (multiple monitors, Fast User Switching, or the
 * Ctrl-Alt-Del login desktop).
 */
typedef struct NvWinSysDesktopRec *NvWinSysDesktopHandle;

/**
 * Opaque handle to a window.  A window is the basic unit of user interaction
 * on a desktop.  A window, if visible, is associated with a rectangle of
 * pixels on the desktop.  As the user interacts with the desktop, the various
 * windows receive a stream of events -- keypresses, mouse clicks, etc.  Each
 * event goes only to the subset of windows that are supposed to be aware of it
 * -- for example, a mouse click is sent to the window that owns the pixel in
 * question, while a keyboard press goes to the window that currently has
 * keyboard focus.
 */
typedef struct NvWinSysWindowRec *NvWinSysWindowHandle;

/**
 * Opaque handle to a pixmap.
 */
typedef struct NvWinSysPixmapRec *NvWinSysPixmapHandle;

/**
 * Typedef for function that is called when the window needs to be repainted.
 *
 * @param hWindow The window that needs to be repainted
 */
typedef void NvWinSysRepaintProc(NvWinSysWindowHandle hWindow);

/**
 * Typedef for function that is called when the user has resized the window.
 * If the window is no longer visible (e.g. it has been minimized or is
 * completely occluded), both the width and height will be given as zero, as a
 * hint that the application should cease rendering.
 *
 * @param hWindow The window that was resized
 */
typedef void NvWinSysResizeProc(NvWinSysWindowHandle hWindow, NvSize *pSize);

/**
 * Typedef for function that is called when the user presses a key and
 * the window currently has keyboard focus.
 *
 * @param hWindow The window that received the keypress.
 * @param c The ASCII code for the keypress, or for non-ASCII characters, a
 *     special NvWinSysKeyCode_* code.
 */
typedef void NvWinSysKeyboardProc(NvWinSysWindowHandle hWindow, char c);

// XXX Should add a mouse API that can be used for mouse/pen/touchscreen, but
// leaving it out until someone actually demands it

/**
 * Struct containing callbacks that can be associated with a window.  All of
 * the callbacks are optional and can be left as NULL if not needed.  Because
 * more callbacks will be added in the future, you must always memset this
 * structure to zero before using it to ensure forward compatibility.
 *
 * For any given window, you can assume that all the callbacks are synchronous.
 * That is, you will not receive two callbacks simultaneously from two
 * different threads.
 */
typedef struct
{
    NvWinSysRepaintProc *pRepaintProc;
    NvWinSysResizeProc *pResizeProc;
    NvWinSysKeyboardProc *pKeyboardProc;
} NvWinSysWindowCallbacks;

typedef enum NvWinSysDesktopOrientationEnum
{
    NvWinSysDesktopOrientation_0       = 0,
    NvWinSysDesktopOrientation_90      = 1,
    NvWinSysDesktopOrientation_180     = 2,
    NvWinSysDesktopOrientation_270     = 3
}NvWinSysDesktopOrientation;

/**
 * Special keyboard codes for non-ASCII characters.  We use codes starting with
 * 128 so that these cannot collide with any normal ASCII character.
 */
enum
{
    // Function keys
    NvWinSysKeyCode_F1 = 128,
    NvWinSysKeyCode_F2,
    NvWinSysKeyCode_F3,
    NvWinSysKeyCode_F4,
    NvWinSysKeyCode_F5,
    NvWinSysKeyCode_F6,
    NvWinSysKeyCode_F7,
    NvWinSysKeyCode_F8,
    NvWinSysKeyCode_F9,
    NvWinSysKeyCode_F10,
    NvWinSysKeyCode_F11,
    NvWinSysKeyCode_F12,

    // Arrow keys and family
    NvWinSysKeyCode_LeftArrow,
    NvWinSysKeyCode_RightArrow,
    NvWinSysKeyCode_UpArrow,
    NvWinSysKeyCode_DownArrow,
    NvWinSysKeyCode_Home,
    NvWinSysKeyCode_End,
    NvWinSysKeyCode_PageUp,
    NvWinSysKeyCode_PageDown,

    // These other keys have real ASCII codes, but better to put them here in
    // a centralized place than to have magic numbers floating around.
    NvWinSysKeyCode_Escape = 27,
    NvWinSysKeyCode_Delete = 127,

    // We don't actually provide the following enums, but just as a reminder:
    //NvWinSysKeyCode_Enter = '\n',
    //NvWinSysKeyCode_Tab = '\t',
    //NvWinSysKeyCode_Backspace = '\b',
};

typedef enum NvWinSysWinParamEnum
{
    NvWinSysWinParam_Position       = 0x00000001,
    NvWinSysWinParam_Size           = 0x00000002,
    NvWinSysWinParam_Stacking       = 0x00000004,
    NvWinSysWinParam_Depth          = 0x00000008,

    NvWinSysWinParam_Force32        = 0x7fffffff
} NvWinSysWinParam;

typedef enum NvWinSysInterfaceEnum
{
    NvWinSysInterface_Default       = 0x00000001,
    NvWinSysInterface_Android,
    NvWinSysInterface_KD,
    NvWinSysInterface_Null,
    NvWinSysInterface_Win32,
    NvWinSysInterface_X11,

    NvWinSysInterface_Force32       = 0x7fffffff
} NvWinSysInterface;

/**
 * Capability for WinSys backends definition.
 *
 * A value of 1 indicates that the backend is defined, but
 * does not imply that it is supported. Instead, a runtime
 * query should be used to check if it is supported.
 */
#ifndef NV_WINSYS_CAPS_API_BACKEND_ANDROID
#define NV_WINSYS_CAPS_API_BACKEND_ANDROID 1
#endif
#ifndef NV_WINSYS_CAPS_API_BACKEND_KD
#define NV_WINSYS_CAPS_API_BACKEND_KD 1
#endif
#ifndef NV_WINSYS_CAPS_API_BACKEND_NULL
#define NV_WINSYS_CAPS_API_BACKEND_NULL 1
#endif
#ifndef NV_WINSYS_CAPS_API_BACKEND_WIN32
#define NV_WINSYS_CAPS_API_BACKEND_WIN32 1
#endif
#ifndef NV_WINSYS_CAPS_API_BACKEND_X11
#define NV_WINSYS_CAPS_API_BACKEND_X11 1
#endif

/**
 * Describes the pixel format of a window, queried using
 * NvWinSysWindowGetFormat.
 */
typedef struct
{
    char redSize;
    char greenSize;
    char blueSize;
    char alphaSize;
} NvWinSysWindowFormat;

/**
 * Sets the default window system.
 *
 * On a system which supports multiple window systems, explicitly
 * choose the window system to be used.  This affects subsequent calls
 * to NvWinSysDesktopOpen and typically the first API call an
 * application will make.
 *
 * @param System Identifies the window system.
 * @returns NvSuccess if the specified window system is supported by
 * the implementation, or an error code otherwise.
 */
NvError NvWinSysInterfaceSelect(NvWinSysInterface Interface);

/**
 * Opens a handle to a pre-existing desktop.
 *
 * For most window systems, if the named desktop does not already exist, this
 * function will fail.  (As mentioned earlier, doing things like launching an X
 * server process is well beyond the scope of this API.)  However, some very
 * simplistic window systems lack any concept of a "window system server" that
 * must be running before any GUI applications can be started.
 * For such systems, this function might automatically create the default
 * desktop.  This function does not provide any way to specify any parameters
 * for that new desktop, so its parameters (width, height, color format,
 * display output, etc.) would have to be obtained through some other mechanism
 * (perhaps through nvos configuration variables).
 *
 * @param Name Identifies the desired desktop.  The interpretation of this
 *     string is OS-dependent.  If Name is NULL, this requests a handle to the
 *     "default" desktop (typically the local system's console/GUI).
 * @param phDesktop Output parameter that returns a handle to the desktop.
 * @returns NvSuccess if successful, or an error code otherwise.
 */
NvError NvWinSysDesktopOpen(
                const char *Name,
                NvWinSysDesktopHandle *phDesktop);

/**
 * Closes a previously opened desktop handle.
 *
 * When a desktop is closed, windows and pixmaps created on this desktop are
 * freed. These objects' winsys handles remain valid and must be closed by the
 * user. Any other operation using these handles is a nop.
 *
 * @param hDesktop Desktop handle from NvWinSysDesktopOpen.  This function is a
 *     nop if hDesktop is NULL.
 */
void NvWinSysDesktopClose(NvWinSysDesktopHandle hDesktop);

/**
 * Obtains the dimensions of a desktop.
 *
 * @param hDesktop Desktop handle to obtain the dimensions for.
 * @param pSize Output parameter where the desktop's dimensions are filled in.
 */
void NvWinSysDesktopGetSize(NvWinSysDesktopHandle hDesktop, NvSize *pSize);

/**
 * Get the orientation of a desktop.
 *
 * @param hDesktop Desktop handle to obtain the dimensions for.
 * @param pOrientation Output parameter where the desktop's orientation
 *        is filled in.
 *
 * @returns NvSuccess if successful, or an error code otherwise.
 *
 * NOTE: Currently it is supported only for Android.
 */

NvError NvWinSysDesktopGetOrientation(NvWinSysDesktopHandle hDesktop,
                                 NvWinSysDesktopOrientation *pOrientation);

/**
 * Set the orientation of a desktop.
 *
 * @param hDesktop Desktop handle to obtain the dimensions for.
 * @param orientation Input parameter to set the desktop's orientation.
 *        it is enumerator defined in NvWinSysDesktopOrientation enum.
 *
 * @returns NvSuccess if successful, or an error code otherwise.
 *
 * NOTE: Currently it is supported only for Android.
 */

NvError NvWinSysDesktopSetOrientation(NvWinSysDesktopHandle hDesktop,
                                      NvWinSysDesktopOrientation orientation);

/**
 * Obtains the "native" display handle associated with a desktop.  This native
 * display handle (with appropriate typecasts) can be passed to nonportable
 * window system APIs or to EGL functions.  For example, on X11, the native
 * display handle is the Display* of the desktop X server.
 *
 * @param hDesktop Desktop handle to get the native handle for
 */
void *NvWinSysDesktopGetNativeHandle(NvWinSysDesktopHandle hDesktop);

/**
 * Creates a window on a desktop using the default surface format.
 *
 * @param hDesktop Identifies which desktop the window is to be created on.
 * @param Name The name of the window, to be used for its title bar (if any).
 * @param pRect The desired initial position of the window on the desktop.  The
 *     actual window dimensions may be larger than this in order to provide for
 *     a title bar, etc. -- this is the usable region, not the total size.
 *     Passing in NULL as the pRect parameter will create a full screen
 *     window on desktop hDesktop without window status bars or decorations.
 * @param pCallbacks Callbacks for repaint, keyboard press, etc.
 * @param phWindow Output parameter that returns a handle to the window.
 *
 * @returns NvSuccess if successful, or an error code otherwise.
 */
NvError
NvWinSysWindowCreate(
    NvWinSysDesktopHandle hDesktop,
    const char *Name,
    const NvRect *pRect,
    const NvWinSysWindowCallbacks *pCallbacks,
    NvWinSysWindowHandle *phWindow);

/**
 * API capability indicating presence of formatted window creation function.
 */
#define NV_WINSYS_CAPS_API_WINDOW_FORMAT 1

/**
 * Creates a window on a desktop using requested surface format.
 *
 * @param hDesktop Identifies which desktop the window is to be created on.
 * @param pFormatARGB Optional window surface color format. If NULL, the
 *     default window format will be used. Otherwise, it points to an array
 *     of 4 NvS32 values which represent the requested bit counts for alpha,
 *     red, green and blue, in that order. A negative value indicates
 *     "don't care". The function will create the window using the best
 *     match available, according to some backend-specific metric, but
 *     is not required to match exactly, and may ignore the request and
 *     fall back to the default format.
 * @param Name The name of the window, to be used for its title bar (if any).
 * @param pRect The desired initial position of the window on the desktop.  The
 *     actual window dimensions may be larger than this in order to provide for
 *     a title bar, etc. -- this is the usable region, not the total size.
 *     Passing in NULL as the pRect parameter will create a full screen
 *     window on desktop hDesktop without window status bars or decorations.
 * @param pCallbacks Callbacks for repaint, keyboard press, etc.
 * @param phWindow Output parameter that returns a handle to the window.
 *
 * @returns NvSuccess if successful, or an error code otherwise.
 */
NvError
NvWinSysWindowCreateFormatted(
    NvWinSysDesktopHandle hDesktop,
    const NvS32 *pFormatARGB,
    const char *Name,
    const NvRect *pRect,
    const NvWinSysWindowCallbacks *pCallbacks,
    NvWinSysWindowHandle *phWindow);

/**
 * Destroys a previously created window.
 *
 * @param hWindow Window handle from NvWinSysWindowCreate.  This function is a
 *     nop if hWindow is NULL.
 */
void NvWinSysWindowDestroy(NvWinSysWindowHandle hWindow);

/**
 * Queries the pixel format for a given window.
 *
 * @param hWindow Handle to the window.
 * @param pFormat Output parameter that returns the pixel format of the window.
 *
 * @returns NvSuccess on success, error otherwise.
 */
NvError NvWinSysWindowGetFormat(
    NvWinSysWindowHandle hWindow,
    NvWinSysWindowFormat *pFormat);

/**
 * Obtains the "native" window handle associated with a window.  This native
 * window handle (with appropriate typecasts) can be passed to nonportable
 * window system APIs or to EGL functions.  For example, on Windows, the native
 * window handle is the window's HWND.
 *
 * @param hWindow Window handle to get the native handle for
 */
void *NvWinSysWindowGetNativeHandle(NvWinSysWindowHandle hWindow);

/**
 * This function moves and/or resizes the window.  It also allows adjusting the
 * stacking order of the window if that is supported.
 *
 * The <Flags> determine what (if anything) is modified.  It is a bitmask of
 * bits from the NvWinSysWinParam enum.
 *
 * If the NvWinSysWinParam_Position bit is set in <Flags>, then pRect->left and
 * pRect->top is used as the new position of the window.
 *
 * If the NvWinSysWinParam_Size bit is set in <Flags>, then pRect->right -
 * pRect->left is used as the new width, and pRect->bottom - pRect->top is used
 * as the new height of the window.
 *
 * If the NvWinSysWinParam_Stacking bit is set in <Flags>, then <Depth> is used
 * to specify the new stacking order as follows:
 *     <Depth>        New window depth
 *        1           bottom most window
 *        2           one position lower (more obscured)
 *        3           one position higher (less obscured)
 *        4           top most window (behind dialogs)
 *        5           top most window (above dialogs)
 *
 * If the NvWinSysWinParam_Depth bit is set in <Flags>, then <Depth> is used as
 * the new depth.  This value is window system specific.
 *
 * NvError_BadParameter is returned if both the NvWinSysWinParam_Depth and
 * NvWinSysWinParam_Stacking bits are set in <Flags>.
 *
 * NvError_BadValue is returned if one of the parameters is outside the allowed
 * range.
 *
 * NvError_NotSupported is returned if one of the parameters to be changed is
 * not supported for the current window system.
 *
 * If any error is returned, then the function has no affect on any parameters.
 *
 * @param hWindow The window to be repainted.
 */
NvError NvWinSysWindowRequestResize(
    NvWinSysWindowHandle hWindow,
    NvWinSysWinParam     Flags,
    const NvRect        *pRect,
    NvU32                Depth);

/**
 * Every window has a "void *" where you can stash whatever private data you'd
 * like to associate with that particular window.  This function sets the
 * private data pointer.
 *
 * @param hWindow Window handle to set the private data for
 * @param pPriv New value for window's private data pointer
 */
void NvWinSysWindowSetPriv(NvWinSysWindowHandle hWindow, void *pPriv);

/**
 * Obtains the private data pointer previously set by NvWinSysWindowSetPriv,
 * or NULL if NvWinSysWindowSetPriv has never been called for this particular
 * window yet.
 *
 * @param hWindow Window handle to get the private data for
 * @returns Value of window's private data pointer
 */
void *NvWinSysWindowGetPriv(NvWinSysWindowHandle hWindow);

/**
 * This function can be called form the window callback functions to request
 * a repaint of the window.  For example, you might call this in a 3D app after
 * a keypress event changes the viewer's position or orientation.  If the app
 * is continuously animating, you might call this at the end of your repaint
 * function to force another repaint at the soonest possible opportunity.
 *
 * @param hWindow The window to be repainted.
 */
void NvWinSysWindowRequestRepaint(NvWinSysWindowHandle hWindow);

/**
 * This function can be called from the window callback functions to request
 * that the window system shut down the application and close all its windows.
 * For example, your keyboard callback might call this function upon receiving
 * a keypress of NvWinSysKeyCode_Escape.
 *
 * @param ExitCode The desired exit code from the application.
 */
void NvWinSysRequestExit(int ExitCode);

/**
 * Once you have created all your windows, your application must call this
 * function, which will not return control until someone calls
 * NvWinSysRequestExit.  You can assume that none of your window callback
 * functions will be called before you call this function.
 *
 * @returns The value passed as an argument to NvWinSysRequestExit.
 */
int NvWinSysMainLoop(void);

/**
 * As an alternative to NvWinSysMainLoop, you may write your application to
 * control the mainloop itself and periodically poll the windowing system
 * for events. Calling this function will process all pending windowing
 * system events and relay them to the application via the window callback
 * functions passed in to the window creation function. Note that on many
 * windowing systems emptying the event queue periodically is required for
 * correct operation in a windowed environment so even if you don't handle
 * any of the events forwarded to the callback functions you should still
 * call this function once a frame.
 */

void NvWinSysPollEvents(void);

/**
 * Creates a pixmap on a desktop using the default format
 *
 *  @param hDesktop Identifies which desktop the pixmap is to be created on.
 *  @param size The desired size of the pixmap
 *  @param phPixmap Output parameter that returns a handle to the pixmap
 *
 *  @returns NvSuccess if successful, or an error code otherwise.
 */
NvError
NvWinSysPixmapCreate(
        NvWinSysDesktopHandle hDesktop,
        NvSize *pSize,
        NvWinSysPixmapHandle *phPixmap);

/**
 * Obtains the "native" pixmap handle associated with a pixmap.  This native
 * pixmap handle (with appropriate typecasts) can be passed to nonportable
 * window system APIs or to EGL functions.
 *
 * @param hPixmap Pixmap handle to get the native handle for
 */
void *NvWinSysPixmapGetNativeHandle(NvWinSysPixmapHandle hPixmap);

/**
 * Destroys a previously created pixmap.
 *
 * @param hPixmap Pixmap handle from NvWinSysPixmapCreate.  This function is a
 *     nop if hPixmap is NULL.
 */
void NvWinSysPixmapDestroy(NvWinSysPixmapHandle hPixmap);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVWINSYS_H
