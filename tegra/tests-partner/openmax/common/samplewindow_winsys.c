#include "samplewindow.h"

#include <nvwinsys.h>

static NvWinSysDesktopHandle s_hDesktop = NULL;

NvBool NvxCreateWindow(
    NvU32 nXOffset,
    NvU32 nYOffset,
    NvU32 nWidth,
    NvU32 nHeight,
    void **phWnd,
    NvxWinCallbacks *pCallbacks)
{
    NvError err;
    NvWinSysDesktopHandle hDesktop;
    NvWinSysWindowHandle hWin;
    NvRect Rect;
    NvRect *pRect = NULL;

    err = NvWinSysDesktopOpen(NULL, &hDesktop);
    if (NvSuccess != err)
        return NV_FALSE;

    Rect.left = 0;
    Rect.top = 0;
    Rect.right = nWidth;
    Rect.bottom = nHeight;

    if (Rect.right > 0 && Rect.bottom > 0)
        pRect = &Rect;

    err = NvWinSysWindowCreate(hDesktop, "OpenMAX IL", pRect, NULL, &hWin);
    if (NvSuccess != err)
        goto cleanup;

    *phWnd = hWin;

    return NV_TRUE;

cleanup:
    if (hDesktop)
        NvWinSysDesktopClose(hDesktop);
    return NV_FALSE;
}

NvBool NvxDestroyWindow(void *hWnd)
{
    if (!hWnd)
        return NV_FALSE;

   NvWinSysWindowDestroy((NvWinSysWindowHandle)hWnd);

    if (s_hDesktop)
        NvWinSysDesktopClose(s_hDesktop);

    return NV_TRUE;
}

