/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software, related documentation and any
 * modifications thereto.  Any use, reproduction, disclosure or distribution of
 * this software and related documentation without an express license agreement
 * from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __NVDC_H__
#define __NVDC_H__


#if defined(__cplusplus)
extern "C"
{
#endif


#include <stddef.h>
#include <errno.h>
#include <time.h>

#include <nvrm_surface.h>
#include <nvcolor.h>

/* Opaque to clients */
struct nvdcState;
typedef struct nvdcState *nvdcHandle;

struct nvdcDisplay;
typedef struct nvdcDisplay *nvdcDisplayHandle;

/*
 * Open the relevant files and initialize internal state.  nvmapFd should be
 * set to the file descriptor with which the client has allocated any surfaces
 * it wishes to display with nvdcFlip().  It is valid to pass -1 if the client
 * does not wish to display any surfaces.
 */
nvdcHandle nvdcOpen(int nvmapFd);
/*
 * Close any open files and free internal state.  After calling this,
 * nvdcHandle and any event file descriptors are no longer valid.
 */
void nvdcClose(nvdcHandle);


/* Query display information */

int nvdcQueryNumHeads(nvdcHandle);

struct nvdcHeadStatus {
    int enabled;
};

int nvdcQueryHeadStatus(nvdcHandle, int head, struct nvdcHeadStatus *);

/* Query DC feature support */

int nvdcGetCapabilities(nvdcHandle, unsigned int *caps);

/*
 * Returns a pointer to an array of nvdcDisplayHandles, and the length of the
 * array.  The array was allocated with malloc(), and the client is expected to
 * free it with free() when it's done with the array.
 */
int nvdcQueryDisplays(nvdcHandle, nvdcDisplayHandle **, int *nDisplays);

enum nvdcDisplayType {
    /* XXX what options do we need here */
    NVDC_DSI,
    NVDC_LVDS,
    NVDC_VGA,
    NVDC_HDMI,
    NVDC_DVI,
    NVDC_DP,
};
struct nvdcDisplayInfo {
    enum nvdcDisplayType type;
    int boundHead;
    int connected;
    unsigned int headMask;
};
int nvdcQueryDisplayInfo(nvdcHandle, nvdcDisplayHandle, struct nvdcDisplayInfo *);
/*
 * Returns a pointer to a memory buffer with raw EDID data, and the length of the
 * buffer.  The buffer was allocated with malloc(), and the client is expected to
 * free it with free() when it's done with the buffer.
 */
int nvdcQueryDisplayEdid(nvdcHandle, nvdcDisplayHandle, void **data, size_t *len);

/*
 * Bind displays to heads
 *
 * A display "binding" is a mapping from a display to a head.  One display may
 * be bound to at most one head at a time, but a head may have multiple
 * displays bound to it simultaneously.
 *
 * The headMask field in the struct nvdcDisplayInfo returned from
 * nvdcQueryDisplayInfo() is a bitmask of the possible heads that a display may
 * be bound to.  The boundHead field is the head that the display is currently
 * bound to, or -1 if not currently bound.
 *
 * It is not necessary to rebind a display if it's already bound to the desired
 * head.
 */
int nvdcDisplayBind(nvdcHandle, nvdcDisplayHandle, int head);
int nvdcDisplayUnbind(nvdcHandle, nvdcDisplayHandle, int head);

/*
 * Get the autoincrementing vblank syncpoint id for the given head.  This
 * synpoint id is static for the life of the nvdcHandle.
 */
int nvdcQueryVblankSyncpt(nvdcHandle, int head, unsigned int *syncpt);

/* Query the current display CRC checksum */
int nvdcEnableCrc(nvdcHandle, int head);
int nvdcDisableCrc(nvdcHandle, int head);
int nvdcGetCrc(nvdcHandle, int head, unsigned int *crc);

/* Modesetting */
struct nvdcMode {
    unsigned int hActive;
    unsigned int vActive;
    unsigned int hSyncWidth;
    unsigned int vSyncWidth;
    unsigned int hFrontPorch;
    unsigned int vFrontPorch;
    unsigned int hBackPorch;
    unsigned int vBackPorch;
    unsigned int hRefToSync;
    unsigned int vRefToSync;
    unsigned int pclkKHz;
    unsigned int bitsPerPixel;
    unsigned int vmode;
};

int nvdcSetMode(nvdcHandle, int head, struct nvdcMode *);
int nvdcGetMode(nvdcHandle, int head, struct nvdcMode *);

/*
 * Get the current mode database for this head from DC.  The modes
 * array will be allocated using malloc(3), and the caller should free
 * it with free(3).
 */
int nvdcGetModeDB(struct nvdcState *state, int head,
                  struct nvdcMode **modes, int *nmodes);
int nvdcValidateMode(nvdcHandle, int head, struct nvdcMode *mode);

enum nvdcDPMSmode {
    NVDC_DPMS_NORMAL,
    NVDC_DPMS_VSYNC_SUSPEND,
    NVDC_DPMS_HSYNC_SUSPEND,
    NVDC_DPMS_POWERDOWN,
};

int nvdcDPMS(nvdcHandle, int head, enum nvdcDPMSmode);

/*
 * Reserve the specified window for use.  This must be called before attempting
 * to issue flips on the window.
 */
int nvdcGetWindow(nvdcHandle, int head, int num);
/* Release a previous reservation made with nvdcGetWindow(). */
int nvdcPutWindow(nvdcHandle, int head, int num);

enum nvdcBlend {
    NVDC_BLEND_NONE,
    NVDC_BLEND_PREMULT,
    NVDC_BLEND_COVERAGE,
    NVDC_BLEND_COLORKEY,
};

/* Fixed-point: 20 bits of integer (MSB) and 12 bits of fractional (LSB) */
typedef unsigned int ufixed_20_12_t;
static inline ufixed_20_12_t pack_ufixed_20_12(unsigned int integer,
                                               unsigned int frac)
{
    return ((integer & ((1 << 20) - 1)) << 12) |
           (frac & ((1 << 12) - 1));
}

ufixed_20_12_t pack_ufixed_20_12_f(double x);

typedef enum {
    nvdcRGB_AUTO        = ~0,
    /* NOTE: these enums need to be kept in sync with the kernel defines */
    nvdcB4G4R4A4        = 4,
    nvdcB5G5R5A         = 5,
    nvdcB5G6R5          = 6,
    nvdcAB5G5R5         = 7,
    nvdcB8G8R8A8        = 12,
    nvdcR8G8B8A8        = 13,
    nvdcB6x2G6x2R6x2A8  = 14,
    nvdcR6x2G6x2B6x2A8  = 15,
    nvdcYCbCr422        = 16,
    nvdcYUV422          = 17,
    nvdcYCbCr420P       = 18,
    nvdcYUV420P         = 19,
    nvdcYCbCr422P       = 20,
    nvdcYUV422P         = 21,
    nvdcYCbCr422R       = 22,
    nvdcYUV422R         = 23,
    nvdcYCbCr422RA      = 24,
    nvdcYUV422RA        = 25,
    nvdcYCbCr444P       = 41,
    nvdcYCrCb420SP      = 42,
    nvdcYCbCr420SP      = 43,
    nvdcYCrCb422SP      = 44,
    nvdcYCbCr422SP      = 45,
    nvdcYUV444P         = 52,
    nvdcYVU420SP        = 53,
    nvdcYUV420SP        = 54,
    nvdcYVU422SP        = 55,
    nvdcYUV422SP        = 56,
    nvdcYVU444SP        = 59,
    nvdcYUV444SP        = 60,
} nvdcFormat;

#define NVDC_HAS_ORIENTATION 1
typedef enum {
    nvdcOrientation_0   = 0,
    nvdcOrientation_180 = 1,
    nvdcOrientation_90  = 2,
    nvdcOrientation_270 = 3,
} nvdcOrientation;

#define NVDC_HAS_REFLECTION 1
typedef enum {
    nvdcReflection_none = 0,
    nvdcReflection_X    = 1,
    nvdcReflection_Y    = 2,
    nvdcReflection_XY   = 3,
} nvdcReflection;

enum {
    NVDC_FLIP_SURF_RGB  = 0,
    NVDC_FLIP_SURF_Y    = 0,
    NVDC_FLIP_SURF_U    = 1,
    NVDC_FLIP_SURF_V    = 2,
    NVDC_FLIP_SURF_NUM  = 3,
};

#define NVDC_GLOBAL_ALPHA_ENABLE 0x80000000
#define NVDC_GLOBAL_ALPHA_MASK   0xFF

struct nvdcFlipWinArgs {
    /*
     * index is the hardware window instance.  Note that it must have been
     * reserved by nvdcGetWindow before use.
     */
    int             index;
    /*
     * surfaces is an array of up to 3 NvRmSurfaces:
     * 0: RGB surface (Luma (Y) surface if YUV or YCbCr)
     * 1: NULL for RGB (U or Cb surface if YUV or YCbCr)
     * 2: NULL for RGB (V or Cr surface if YUV or YCbCr)
     * The NVDC_FLIP_SURF_* enumerants above may be convenient.
     *
     * The following parameters will be used from the NvRmSurface:
     * - Layout (tiled vs. not tiled)
     * - Pitch
     * - hMem
     * - Offset
     * - ColorFormat
     *
     * Specifying NULL for surfaces[0] will disable the given overlay.
     * (A note about disabling overlays: specifying NULL is currently the ONLY
     * way to disable an overlay.  nvdcPutWindow() or client termination
     * (abnormal or otherwise) does not disable a window; it will remain in its
     * current configuration until another client uses nvdcFlip to a new
     * surface or to NULL.  This behavior is is intended to allow for smooth
     * transitions between, e.g., a splash screen and subsequent application.
     * Resources are refcounted properly; a surface being displayed is pinned
     * until nvdc flips away from it, so no need to worry about leaks in this
     * scenario.  This behavior can be somewhat confusing and is unfortunate
     * for abnormal app termination, so may become optional in the future.)
     */
    NvRmSurface     *surfaces[NVDC_FLIP_SURF_NUM];
    /*
     * format is a special display format that describes how the surfaces are
     * interpreted.
     *
     * For RGB surfaces, if format is nvdcRGB_AUTO, libnvdc will attempt to
     * guess an appropriate format given
     * surfaces[NVDC_FLIP_SURF_RGB]->ColorFormat.
     *
     * Note that since it isn't well-defined how to translate a YUV surface's
     * NvColorFormat to a YUV nvdcFormat those aren't automatically handled in
     * the library -- it is assumed that the client will choose an appropriate
     * nvdcFormat in that case.
     */
    nvdcFormat      format;
    enum nvdcBlend  blend;
    ufixed_20_12_t  x;
    ufixed_20_12_t  y;
    ufixed_20_12_t  w;
    ufixed_20_12_t  h;
    unsigned int    outX;
    unsigned int    outY;
    unsigned int    outW;
    unsigned int    outH;
    unsigned int    z;
    unsigned int    swapInterval;
    unsigned int    preSyncptId;
    unsigned int    preSyncptVal;
    unsigned int    cursorMode;
    /*
     * Lower and higher ranges of RGB colorkey.
     */
    unsigned int    colorKeyLower;
    unsigned int    colorKeyUpper;
    nvdcOrientation orientation;
    nvdcReflection  reflection;
    unsigned int    globalAlpha;

    /*
     * The timestamp specifies the minimum presentation time for the given
     * buffer.  The flip will not occur until the current time is >= the given
     * timestamp.  If multiple flips have been queued that satisfy this
     * timestamp criterion (and the rest of the criteria for the flip such as
     * its preSyncPoint are met), all but the last queued flip will be collapsed
     * and the last queued buffer will be displayed.  At the time that the last
     * queued buffer is displayed, its postSyncPoint will be incremented along
     * with the postSyncPoint of the flips that were collapsed, if any.
     *
     * A timestamp of 0 means "present immediately", i.e., don't use timestamp
     * flipping.
     *
     * XXX: For QNX, this is work in progress. Currently, not supported under
     * Linux.
     */
    struct timespec timestamp;
};

struct nvdcFlipArgs {
    struct nvdcFlipWinArgs *win;
    int numWindows;
    unsigned int postSyncptId;
    unsigned int postSyncptValue;
};
int nvdcFlip(nvdcHandle, int head, struct nvdcFlipArgs *);


/* Cursor manipulation */

/*
 * Cursor image format:
 * - Tegra hardware supports 2Bit cursor which contains two colors: foreground
 *   and background, specified by the client in RGB8. The image should be
 *   specified as two 1bpp bitmaps immediately following each other in memory.
 *   Each pixel in the final cursor will be constructed from the bitmaps with
 *   the following logic:
 *           bitmap1 bitmap0
 *           (mask)  (color)
 *             1        0      transparent
 *             1        1      inverted
 *             0        0      background color
 *             0        1      foreground color
 *
 * - Tegra hardware from Tegra 4 supports RGB cursor (T_R8G8B8A8). Alpha channel
 *   is at the LSB byte. Specify the color format explicitly to select it.
 */
struct nvdcCursorImage {
    struct {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    } foreground, background;
    unsigned int bufferId;
    enum {
        NVDC_CURSOR_IMAGE_SIZE_256x256,
        NVDC_CURSOR_IMAGE_SIZE_128x128,
        NVDC_CURSOR_IMAGE_SIZE_64x64,
        NVDC_CURSOR_IMAGE_SIZE_32x32,
    } size;
    enum {
        NVDC_CURSOR_IMAGE_FORMAT_2BIT = 0,
        NVDC_CURSOR_IMAGE_FORMAT_RGBA_NON_PREMULT_ALPHA = 1,
        NVDC_CURSOR_IMAGE_FORMAT_RGBA =
            NVDC_CURSOR_IMAGE_FORMAT_RGBA_NON_PREMULT_ALPHA,
        NVDC_CURSOR_IMAGE_FORMAT_RGBA_PREMULT_ALPHA = 2,
    } format;
    int visible;
    int mode;
};
/*
* Reserve the cursor for use. This must be called before attempting
* to set the cursor position or image.
*/
int nvdcGetCursor(nvdcHandle, int head);
/* Release a previous reservation made with nvdcGetCursor(). */
int nvdcPutCursor(nvdcHandle, int head);
int nvdcSetCursorImage(nvdcHandle, int head, struct nvdcCursorImage *);
int nvdcSetCursor(nvdcHandle, int head, int x, int y, int visible);
int nvdcCursorClip(struct nvdcState *state, int head, int win_clip);
int nvdcSetCursorImageLowLatency(nvdcHandle, int head, struct nvdcCursorImage *, int visiable);
int nvdcSetCursorLowLatency(nvdcHandle, int head, int x, int y, struct nvdcCursorImage *);

struct nvdcCmu {
	NvU16 cmu_enable;
	NvU16 csc[9];
	NvU16 lut1[256];
	NvU16 lut2[960];
};

int nvdcSetCmu(nvdcHandle, int head,  struct nvdcCmu *cmu);

struct nvdcCsc {
    enum {
        NVDC_COLORIMETRY_601,
        NVDC_COLORIMETRY_709,
    } colorimetry;
    float brightness;   /* -0.5 - 0.5 */
    float contrast;     /*  0.1 - 2.0 */
    float saturation;   /*  0.0 - 2.0 */
    float hue;          /*  -pi - pi  */
};
int nvdcSetCsc(struct nvdcState *state, int head, int window,
               const struct nvdcCsc *csc);

/*
 * RGB Lookup Table
 *
 * In true-color and YUV modes this is used for post-CSC RGB->RGB lookup, i.e.
 * gamma-correction. In palette-indexed RGB modes, this table designates the
 * mode's color palette.
 *
 * To convert 8-bit per channel RGB values to 16-bit, duplicate the 8 bits
 * in low and high byte, e.g. r=r|(r<<8)
 *
 * Current Tegra DC hardware supports 8-bit per channel to 8-bit per channel,
 * and each hardware window (overlay) uses its own lookup table.
 */
struct nvdcLut {
    unsigned int start; /* start index */
    unsigned int len;   /* start size of r/g/b arrays */
    unsigned int flags; /* see NVDC_LUT_FLAGS_* */
    NvU16* r;           /* array of 16-bit red values, 0 to reset */
    NvU16* g;           /* array of 16-bit green values, 0 to reset */
    NvU16* b;           /* array of 16-bit blue values, 0 to reset */
};

/* nvdcLut.flags - override fb device palette. Default is multiply. */
#define NVDC_LUT_FLAGS_FBOVERRIDE 0x01

/*
 * Convenience function to allocate struct nvdcLut
 *
 * It will initialize struct nvdcLut, and allocate len entries for r/g/b,
 * and fill them with n->n unity lookup. Use nvdcFreeLut to free this structure.
 */
int nvdcAllocLut(struct nvdcLut* lut, unsigned int len);

/* Convenience function to free struct nvdcLut */
void nvdcFreeLut(struct nvdcLut* lut);

/* Set RGB Lookup Table to hardware */
int nvdcSetLut(nvdcHandle, int head, int window, const struct nvdcLut *lut);


/*
 * Event Support
 *
 * Events are delivered by reading from a file.  There are two basic ways this
 * can be integrated into the client, depending on the application architecture
 * of the client.
 * 1. Manual I/O: The client can query the file descriptor(s) used to deliver
 *    events and handle reading from the file(s) itself.  This is useful if the
 *    client already has a file-polling main loop.
 * 2. The client can let the nvdc library handle all file I/O internally.  This
 *    will generally involve spawning a dedicated thread that will block until
 *    events are ready to be consumed.
 *
 * Using either mechanism, the client should register handlers for any events
 * it wishes to be notified about.  In the first case, the nvdc library will
 * only call the client's event handler from the context of the
 * nvdcEventHandle() function; in the second case, the event handler may be
 * called asynchronously at any time, so suitable locking should be implemented
 * by the client.
 */

/*
 * Manual I/O
 *
 * nvdcEventFds returns an array of num file descriptors to poll.  The array
 * was allocated with malloc(), and the client is expected to free it with
 * free() when it's done with the array.
 *
 * When there is data to be read from one of the file descriptors (i.e., read()
 * would not block), the client should call nvdcEventData with the file
 * descriptor that has pending data.
 */
int nvdcEventFds(nvdcHandle, int **fds, int *numFds);
int nvdcEventData(nvdcHandle, int fd);

/* Asynchronous I/O */
int nvdcInitAsyncEvents(nvdcHandle);

/*
 * Event handler callback registering
 *
 * The hotplug handler is called any time a display's connected state changes.
 * The affected display's handle is passed to the hotplug handler function; the
 * client is expected to requery any state about the display.
 */
typedef void (*nvdcEventHotplugHandler)(nvdcDisplayHandle);
int nvdcEventHotplug(nvdcHandle, nvdcEventHotplugHandler);
/* The mode change handler is called any time the mode is changed. */
typedef void (*nvdcEventModeChangeHandler)(int head);
int nvdcEventModeChange(nvdcHandle, nvdcEventModeChangeHandler);


#if defined(__cplusplus)
}
#endif


#endif /* __NVDC_H__ */
