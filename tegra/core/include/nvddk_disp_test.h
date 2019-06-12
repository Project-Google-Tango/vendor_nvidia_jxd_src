/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: Display Serial Interface (DSI) transport
 *  Interface</b>
 *
 * @b Description: This file declares the NvDDK interface for DSI transport
 *  functions.
 */

/**
 * @defgroup nvbdk_ddk_disp_test Display Interface
 *
 * NvDdkDispDsi is the DSI display transport funtions for interfacing with the
 * DSI display devices.
 *
 * The interface is part of the NVIDIA Driver Development Kit (NvDDK).
 *
 * @ingroup nvddk_modules
 * @{
 */
#ifndef INCLUDED_NVDDK_DISP_TEST_H
#define INCLUDED_NVDDK_DISP_TEST_H

#include "nvddk_disp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * DSI controller modes.
 */
typedef enum
{
    /** Indicates DSI command mode.
     * In this mode, DSI driver forms the commands and sends the commands to
     * the display device by writing to the DSI controller FIFO.
     */
    NvDdkDispDsiMode_CommandMode = 0,

    /** Display Controller (DC) driven command mode.
     * In this mode, DSI controller receives data from the DC and forms the
     * memory write start and memory write continue commands to write the
     * data to the embedded memory of the smart display device.
     */
    NvDdkDispDsiMode_DcDrivenCommandMode,

    /** Indicates DSI video mode. */
    NvDdkDispDsiMode_VideoMode,

    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkDispDsiMode_Force32 = 0x7FFFFFFF,
} NvDdkDispDsiMode;

/**
 * DSI command information.
 */
typedef struct NvDdkDispDsiCommandRec
{
    /* DSI command type. */
    NvU32 CommandType;

    /* DSI command. */
    NvU32 Command;

    /* DSI packet format. */
    NvBool bIsLongPacket;
} NvDdkDispDsiCommand;

/**
 * Initialize the DSI Module and allocates the required resources.
 *
 * @pre This must be called before any other NvDdkDispDsi methods can be used.
 *
 * @param hDisp Handle to the display API.
 * @param DsiInstance DSI controller instance.
 * @param flags 32-bit value containing one or more bit flags
 *      (reserved - set to zero).
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvDdkDispTestDsiInit(
    NvDdkDispDisplayHandle hDisp,
    NvU32 DsiInstance,
    NvU32 flags );

/**
 * Sets the controller mode, like command mode, video mode etc.
 *
 * @param hDisp Handle to the display API.
 * @param DsiInstance DSI controller instance.
 * @param Mode DSI controller mode, see NvDdkDispDsiMode.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvDdkDispTestDsiSetMode(
    NvDdkDispDisplayHandle hDisp,
    NvU32 DsiInstance,
    NvDdkDispDsiMode Mode );

/**
 * Sends the requested commands to the panel on the DSI bus.
 *
 * NOTE: DSI commands can be sent to the panel only in command mode.
 * So, dsi controller needs to be configured for command mode using
 * @NvDdkDispDsiSetMode function before sending any commands to the panel.
 *
 * @param hDisp Handle to the display API.
 * @param DsiInstance DSI controller instance.
 * @param pCommand Pointer to the DSI command info struct, see NvDdkDispDsiCommand.
 * @param DataSize Number of bytes to be written to the display device.
 * @param pData Pointer to the data to be written to the display device.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvDdkDispTestDsiSendCommand(
    NvDdkDispDisplayHandle hDisp,
    NvU32 DsiInstance,
    NvDdkDispDsiCommand *pCommand,
    NvU32 DataSize,
    NvU8 *pData );

/**
 * Reads data from the panel on the DSI bus.
 *
 * This function reads the requested number of bytes from the specified
 * panel address to the requested memory location.
 *
 * NOTE: DSI panel registers can be read only in command mode. So, before
 * reading any data from the panel, dsi controller needs to be configured
 * for command mode using @NvDdkDispDsiSetMode function.
 *
 * @param hDisp Handle to the display API.
 * @param DsiInstance DSI controller instance.
 * @param Addr Address of register or memory on the panel from where
 *       data is read.
 * @param DataSize Number of bytes to be read from the display device.
 * @param pData Pointer to the memory location where data read from the
 *       display device is stored.
 *
 * @retval NvSuccess, If read is successful and the response type is
 *      Generic long/short or DCS long/short response. In this case, payload
 *      of the read response is copied into the read buffer.
 * NvError_DispDsiReadAckError, If the read response is Acknowledgement &
 *      Error report. In this case, payload of the Acknowledgement & Error
 *      report response is copied into the read buffer.
 * NvError_DispDsiReadInvalidResp, If the read response is not in the format
 *      specified by the MIPI DSI specification. In this case, the read
 *      response received fromt the device is directly copied into the read
 *      buffer.
 * NvError_Timeout, If there is no response from the display device.
 */
NvError
NvDdkDispTestDsiRead(
    NvDdkDispDisplayHandle hDisp,
    NvU32 DsiInstance,
    NvU32 Addr,
    NvU32 DataSize,
    NvU8 *pData );

/**
 * Updates the embedded memory of a smart display device with the requested
 * surface data.
 *
 * NOTE: Smart panel updates can be done in command mode or other modes like
 * "DC driven command mode". For eg, if the updates are done in command mode,
 * the DSI controller must be configured for command mode using
 * @NvDdkDispDsiSetMode function before doing the update.
 *
 * @param hDisp Handle to the display API.
 * @param DsiInstance DSI controller instance.
 * @param surf Handle to the surface used as a source for the update.
 * @param pSrc A pointer into the source surface.
 * @param pUpdate Rectangular region on the display to update from the
 *      source surface.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvDdkDispTestDsiUpdate(
    NvDdkDispDisplayHandle hDisp,
    NvU32 DsiInstance,
    NvRmSurface *surf,
    NvPoint *pSrc,
    NvRect *pUpdate );

/**
 * Reads a raw EDID from the display device.
 *
 * Two calls should be made to this function:
 * -# On the first call, the function populates size of the raw edid
 * from the display device in \a size. When this call is issued \a edid
 * (and \a *size) should be NULL.
 * -# On the second call, the length of the \a edid array is indicated by
 * the value of \a *size. When \a *size is less than the number bytes of raw
 * edid the function populates the \a edid array with bytes in \a *size.
 * When \a *size is more than the number bytes of raw edid, the function
 * populates the \a edid array with the number bytes of raw edid and writes
 * the number bytes of raw edid into \a size.
 *
 * @param hDisplay Handle to a display.
 * @param edid A pointer to an array of raw edid
 * @param size Unsigned integer value indicating the size of
 *        raw edid in the array \a edid. This value is populated
 *        with raw edid on return.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvDdkDispTestGetEdid(
    NvDdkDispDisplayHandle hDisplay,
    NvU8 *edid,
    NvU32 *size );

#if defined(__cplusplus)
}
#endif
/** @} */

#endif // INCLUDED_NVDDK_DISP_TEST_H
