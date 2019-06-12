#ifndef __OMX_SAMPLE_WINDOW_H__
#define __OMX_SAMPLE_WINDOW_H__

#include "nvos.h"

typedef void (*WinMoveCallback)(int xpos, int ypos, void *pClientData);
typedef void (*WinResizeCallback)(int width, int height, void *pClientData);
typedef void (*WinCloseCallback)(void *pClientData);

typedef struct NvxWinCallbacks
{
    WinMoveCallback pWinMoveCallback;
    WinResizeCallback pWinResizeCallback;
    WinCloseCallback pWinCloseCallback;
    void *pClientData;
} NvxWinCallbacks;

typedef struct NvxWindowStruct
{
    NvU32 nXOffset;
    NvU32 nYOffset;
    NvU32 nWidth; 
    NvU32 nHeight;
    void *hWnd;
    NvxWinCallbacks *pCallbacks;
    NvOsSemaphoreHandle hThreadStarted;
    NvOsThreadHandle hThread;
} NvxWindowStruct;

NvBool NvxCreateWindow(
    NvU32 nXOffset,
    NvU32 nYOffset,
    NvU32 nWidth, 
    NvU32 nHeight,
    void **phWnd,
    NvxWinCallbacks *pCallbacks);

NvBool NvxDestroyWindow(void *hWnd);


#endif  // __OMX_SAMPLE_WINDOW_H__
