#include "samplewindow.h"
#include <windows.h>
#include <nvos.h>

static NvxWinCallbacks s_oCallbacks = {NULL, NULL, NULL, NULL};
static int s_oClosingWindow = NV_FALSE;

static LRESULT CALLBACK NvxWindowProc( HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        // for now, just do the default
        case WM_CLOSE:
          if (s_oClosingWindow || !s_oCallbacks.pWinCloseCallback)
              DestroyWindow (hwnd);
          else
              s_oCallbacks.pWinCloseCallback(s_oCallbacks.pClientData);
          return 0;

        case WM_DESTROY:
          PostQuitMessage (0);
          return 0;

        case WA_ACTIVE:
        case WM_ERASEBKGND:
        case WM_PAINT:
          {
              // For now, let's paint the canvas black for the background:
              RECT rcClient;
              PAINTSTRUCT oPaint;
              COLORREF colorBlack;
              HBRUSH oBrush;

              HDC hDC = BeginPaint( hwnd, &oPaint);
              if (GetDeviceCaps(hDC, BITSPIXEL)==32)
                  colorBlack = RGB(0,0,2);
              else
                  colorBlack = RGB(0,0,16);

              oBrush = CreateSolidBrush(colorBlack);
              GetClientRect(hwnd, &rcClient);
              FillRect(hDC, &rcClient, oBrush);
              EndPaint(hwnd, &oPaint);

              DeleteObject(oBrush);
          }
          break;

        case WM_MOVE:
        {
            int xPos = (int)(short)LOWORD(lParam);
            int yPos = (int)(short)HIWORD(lParam);
            if (s_oCallbacks.pWinMoveCallback)
            {
                s_oCallbacks.pWinMoveCallback(xPos, yPos, s_oCallbacks.pClientData);
            }
        }
        break;

        case WM_SIZE:
        {
            int xSize = (int)(short)LOWORD(lParam);
            int ySize = (int)(short)HIWORD(lParam);
            if (s_oCallbacks.pWinResizeCallback)
            {
                s_oCallbacks.pWinResizeCallback(xSize, ySize, s_oCallbacks.pClientData);
            }
        }
        break;

    }
    return DefWindowProc (hwnd, uMsg, wParam, lParam);
}

NvBool NvxCreateWindowReal(
    NvU32 nXOffset,
    NvU32 nYOffset,
    NvU32 nWidth,
    NvU32 nHeight,
    void **phWnd,
    NvxWinCallbacks *pCallbacks)
{
    ATOM atom;
    WNDCLASS wndClass;
    HWND hWindow;
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    *phWnd = NULL;

    NvOsMemset(&wndClass, 0, sizeof(WNDCLASS));
    wndClass.lpszClassName = _T("OMX Window");
    wndClass.lpfnWndProc = NvxWindowProc;
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.hInstance = (HINSTANCE)GetModuleHandle(NULL);
    wndClass.cbClsExtra     = 0;
    wndClass.cbWndExtra     = 0;
    wndClass.hIcon          = NULL;
    wndClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground  = (HBRUSH) GetStockObject (BLACK_BRUSH);
    wndClass.lpszMenuName   = NULL;

    atom = RegisterClass(&wndClass);
    if (!atom)
    {
        NV_DEBUG_PRINTF(("Unable to register window class\n"));
        // Class may already exist (created by previous instance or another module)
    }

    if (nWidth == 0)
        nWidth = nScreenWidth - nXOffset;
    if (nHeight == 0)
        nHeight = nScreenHeight - nYOffset;

    if (nWidth > (NvU32)nScreenWidth)
        nWidth = (NvU32)nScreenWidth;
    if (nHeight > (NvU32)nScreenHeight)
        nHeight = (NvU32)nScreenHeight;

    if (nHeight == nScreenHeight && nYOffset == 0)
    {
        RECT taskSize;
        HWND hTaskBarWin = FindWindow(_T("HHTaskBar"), NULL);
        if (GetWindowRect(hTaskBarWin, &taskSize))
        {
            int taskheight = taskSize.bottom - taskSize.top;
            if (taskheight < 30 && nHeight > 60)
            nHeight -= taskheight;
        }
    }
    else
    {
        nHeight += GetSystemMetrics(SM_CYCAPTION);
    }

    // Create the window:
    hWindow = CreateWindow(
        _T("OMX Window"),
        _T("OMX Video Display"),
        WS_VISIBLE|WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU,
        nXOffset, nYOffset,
        nWidth, nHeight,
        NULL, // no parent window
        NULL, // no menu handle
        (HINSTANCE)GetModuleHandle(NULL), // hInstance,
        NULL
        );
    if ( !hWindow )
    {
        NV_DEBUG_PRINTF(("Unable to create video window!\n"));
        return NV_FALSE;
    }

    // Set [out] parameter:
    *phWnd = hWindow;

    s_oCallbacks = *pCallbacks;

    // Set window to foreground upon creation
    if (!SetForegroundWindow(hWindow) )
    {
        NV_DEBUG_PRINTF(("Unable to set foreground window...\n"));
    }

    // show window
    if (!ShowWindow(hWindow, SW_SHOWNORMAL) )
    {
        NV_DEBUG_PRINTF(("Unable to show window...\n"));
    }

    // update window
    if (!UpdateWindow(hWindow) )
    {
        NV_DEBUG_PRINTF(("Unable to update window\n"));
    }

    return NV_TRUE;
}

static NvU32 NvxWindowFunc(void *pData)
{
    NvxWindowStruct *win = (NvxWindowStruct *)pData;
    MSG msg;

    NvxCreateWindowReal(win->nXOffset, win->nYOffset, win->nWidth, win->nHeight,
                        &win->hWnd, win->pCallbacks);

    NvOsSemaphoreSignal(win->hThreadStarted);

    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(win->hWnd);

    return 0;
}

NvBool NvxCreateWindow(
    NvU32 nXOffset,
    NvU32 nYOffset,
    NvU32 nWidth,
    NvU32 nHeight,
    void **phWnd,
    NvxWinCallbacks *pCallbacks)
{
    NvxWindowStruct *win = NvOsAlloc(sizeof(NvxWindowStruct));

    if (!phWnd)
        return NV_FALSE;
    *phWnd = NULL;

    win->nXOffset = nXOffset;
    win->nYOffset = nYOffset;
    win->nWidth = nWidth;
    win->nHeight = nHeight;
    win->hWnd = NULL;
    win->pCallbacks = pCallbacks;
    if (NvSuccess != NvOsSemaphoreCreate(&win->hThreadStarted, 0))
    {
        NvOsFree(win);
        return NV_FALSE;
    }

    if (NvSuccess != NvOsThreadCreate((NvOsThreadFunction)NvxWindowFunc,
                                      win, &win->hThread))
    {
        NvOsSemaphoreDestroy(win->hThreadStarted);
        NvOsFree(win);
        return NV_FALSE;
    }

    NvOsSemaphoreWait(win->hThreadStarted);

    *phWnd = win;
    return NV_TRUE;
}

NvBool NvxDestroyWindow(void *hWnd)
{
    NvxWindowStruct *win = (NvxWindowStruct*)hWnd;

    if (!hWnd || !win->hWnd)
        return NV_TRUE;

    s_oClosingWindow = NV_TRUE;
    PostMessage((HWND)(win->hWnd), WM_CLOSE, 0, 0);
    NvOsThreadJoin(win->hThread);
    win->hThread = NULL;

    s_oClosingWindow = NV_FALSE;
    NvOsSemaphoreDestroy(win->hThreadStarted);

    NvOsFree(win);

    return NV_TRUE;
}


