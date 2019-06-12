/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         ODM Services API</b>
 *
 * @b Description: Defines the abstraction to SOC resources used by
 *                 external peripherals.
 */

#ifndef INCLUDED_NVODM_SERVICES_H
#define INCLUDED_NVODM_SERVICES_H

// Using addtogroup when defgroup resides in another file
/**
 * @addtogroup nvodm_services
 * @{
 */

#include "nvcommon.h"
#include "nvodm_modules.h"
#include "nvassert.h"
#include "nvcolor.h"
#include "nvodm_query.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_audiocodec.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** Indicates that NvOdmOsStatType has mtime.
*/
#define NVODM_HAS_MTIME 1

/*
 * This header is split into two sections: OS abstraction APIs and basic I/O
 * driver APIs.
 */

/** @name OS Abstraction APIs
 * The Operating System APIs are portable to any NVIDIA-supported operating
 * system and will appear in all of the engineering sample code.
 */
/*@{*/

/**
 * Outputs a message to the console, if present. Do not use this for
 * interacting with a user from an application.
 *
 * @param format A pointer to the format string. The format string and variable
 * parameters exactly follow the posix printf standard.
 */
void
NvOdmOsPrintf( const char *format, ...);

/**
 * Outputs a message to the debugging console, if present. Do not use this for
 * interacting with a user from an application.
 *
 * @param format A pointer to the format string. The format string and variable
 * parameters exactly follow the posix printf standard.
 */
void
NvOdmOsDebugPrintf( const char *format, ... );

/**
 * Dynamically allocates memory. Alignment, if desired, must be done by the
 * caller.
 *
 * @param size The size, in bytes, of the allocation request.
 */
void *
NvOdmOsAlloc(size_t size);

/**
 * Frees a dynamic memory allocation.
 *
 * Freeing a NULL value is supported.
 *
 * @param ptr A pointer to the memory to free, which should be from NvOdmOsAlloc().
 */
void
NvOdmOsFree(void *ptr);

typedef struct NvOdmOsMutexRec *NvOdmOsMutexHandle;
typedef struct NvOdmOsSemaphoreRec *NvOdmOsSemaphoreHandle;
typedef struct NvOdmOsThreadRec *NvOdmOsThreadHandle;

/**
 * Copies a specified number of bytes from a source memory location to
 * a destination memory location.
 *
 *  @param dest A pointer to the destination of the copy.
 *  @param src A pointer to the source memory.
 *  @param size The length of the copy in bytes.
 */
void
NvOdmOsMemcpy(void *dest, const void *src, size_t size);

/** Compares two memory regions.
 *
 *  @param s1 A pointer to the first memory region.
 *  @param s2 A pointer to the second memory region.
 *  @param size The length to compare.
 *
 *  @retval 0 If the memory regions are identical.
 */
int
NvOdmOsMemcmp(const void *s1, const void *s2, size_t size);


/**
 * Sets a region of memory to a value.
 *
 *  @param s A pointer to the memory region.
 *  @param c The value to set.
 *  @param size The length of the region in bytes.
 */
void
NvOdmOsMemset(void *s, NvU8 c, size_t size);

/** Compares two strings.
 *
 *  @param s1 A pointer to the first string.
 *  @param s2 A pointer to the second string.
 *
 *  @return 0 If the strings are identical.
 */
int
NvOdmOsStrcmp(const char *s1, const char *s2);

/** Compares two strings up to the given length.
 *
 *  @param s1 A pointer to the first string.
 *  @param s2 A pointer to the second string.
 *  @param size The length to compare.
 *
 *  @return 0 If the strings are identical.
 */
int
NvOdmOsStrncmp(const char *s1, const char *s2, size_t size);

/**
 * Create a new mutex.
 *
 * @note Mutexes can be locked recursively; if a thread owns the lock,
 * it can lock it again as long as it unlocks it an equal number of times.
 *
 * @return NULL on failure.
 */
NvOdmOsMutexHandle
NvOdmOsMutexCreate( void );

/**
 * Locks the given unlocked mutex.
 *
 * @note This is a recursive lock.
 *
 * @param mutex The mutex to lock.
 */
void
NvOdmOsMutexLock( NvOdmOsMutexHandle mutex );

/**
 * Unlock a locked mutex.
 *
 * A mutex must be unlocked exactly as many times as it has been locked.
 *
 * @param mutex The mutex to unlock.
 */
void
NvOdmOsMutexUnlock( NvOdmOsMutexHandle mutex );

/**
 * Frees the resources held by a mutex.
 *
 * @param mutex The mutex to destroy. Passing a NULL mutex is supported.
 */
void
NvOdmOsMutexDestroy( NvOdmOsMutexHandle mutex );

/**
 * Creates a counting semaphore.
 *
 * @param value The initial semaphore value.
 *
 * @return NULL on failure.
 */
NvOdmOsSemaphoreHandle
NvOdmOsSemaphoreCreate( NvU32 value );

/**
 * Waits until the semaphore value becomes non-zero.
 *
 * @param semaphore The semaphore for which to wait.
 */
void
NvOdmOsSemaphoreWait( NvOdmOsSemaphoreHandle semaphore );

/**
 * Waits for the given semaphore value to become non-zero with timeout.
 *
 * @param semaphore The semaphore for which to wait.
 * @param msec The timeout value in milliseconds. Use \c NV_WAIT_INFINITE
 * to wait forever.
 *
 * @return NV_FALSE if the wait expires.
 */
NvBool
NvOdmOsSemaphoreWaitTimeout( NvOdmOsSemaphoreHandle semaphore, NvU32 msec );

/**
 * Increments the semaphore value.
 *
 * @param semaphore The semaphore to signal.
 */
void
NvOdmOsSemaphoreSignal( NvOdmOsSemaphoreHandle semaphore );

/**
 * Frees resources held by the semaphore.
 *
 * @param semaphore The semaphore to destroy. Passing in a NULL semaphore
 * is supported (no op).
 */
void
NvOdmOsSemaphoreDestroy( NvOdmOsSemaphoreHandle semaphore );

/**
 * Entry point for a thread.
 */
typedef void (*NvOdmOsThreadFunction)(void *args);

/**
 * Creates a thread.
 *
 *  @param function The thread entry point.
 *  @param args The thread arguments.
 *
 * @return The thread handle, or NULL on failure.
 */
NvOdmOsThreadHandle
NvOdmOsThreadCreate(
    NvOdmOsThreadFunction function,
    void *args);

/**
 * Waits for the given thread to exit.
 *
 *  The joined thread will be destroyed automatically. All OS resources
 *  will be reclaimed. There is no method for terminating a thread
 *  before it exits naturally.
 *
 *  Passing in a NULL thread ID is ok (no op).
 *
 *  @param thread The thread to wait for.
 */
void
NvOdmOsThreadJoin(NvOdmOsThreadHandle thread);

/**
 *  Unschedules the calling thread for at least the given
 *      number of milliseconds.
 *
 *  Other threads may run during the sleep time.
 *
 *  @param msec The number of milliseconds to sleep. This API should not be
 *  called from an ISR, can be called from the IST though!
 */
void
NvOdmOsSleepMS(NvU32 msec);


/**
 * Stalls the calling thread for at least the given number of
 * microseconds. The actual time waited might be longer, so you cannot
 * depend on this function for precise timing.
 *
 * @note It is safe to use this function at ISR time.
 *
 * @param usec The number of microseconds to wait.
 */
void
NvOdmOsWaitUS(NvU32 usec);

/**
 * Gets the system time in milliseconds.
 * The returned values are guaranteed to be monotonically increasing,
 * but may wrap back to zero (after about 50 days of runtime).
 *
 * @return The system time in milliseconds.
 */
NvU32
NvOdmOsGetTimeMS(void);

/// Defines possible operating system types.
typedef enum
{
    NvOdmOsOs_Unknown,
    NvOdmOsOs_Windows,
    NvOdmOsOs_Linux,
    NvOdmOsOs_Aos,
    NvOdmOsOs_Force32 = 0x7fffffffUL,
} NvOdmOsOs;

/// Defines possible operating system SKUs.
typedef enum
{
    NvOdmOsSku_Unknown,
    NvOdmOsSku_CeBase,
    NvOdmOsSku_Mobile_SmartFon,
    NvOdmOsSku_Mobile_PocketPC,
    NvOdmOsSku_Android,
    NvOdmOsSku_Force32 = 0x7fffffffUL,
} NvOdmOsSku;

/// Defines the OS information record.
typedef struct NvOdmOsOsInfoRec
{
    NvOdmOsOs  OsType;
    NvOdmOsSku Sku;
    NvU16   MajorVersion;
    NvU16   MinorVersion;
    NvU32   SubVersion;
    NvU32   Caps;
} NvOdmOsOsInfo;

/**
 * Gets the current OS version.
 *
 * @param pOsInfo A pointer to the OS version.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmOsGetOsInformation( NvOdmOsOsInfo *pOsInfo );

/*@}*/
/** @name Basic I/O Driver APIs
 * The basic I/O driver APIs are a set of common input/outputs
 * that can be used to extend the functionality of the software stack
 * for new devices that aren't explicity handled by the stack.
 * GPIO, I2C, and SPI are currently supported.
*/
/*@{*/

/**
 * Defines an opaque handle to the ODM Services GPIO rec interface.
 */
typedef struct NvOdmServicesGpioRec *NvOdmServicesGpioHandle;
/**
 * Defines an opaque handle to the ODM Services GPIO intr interface.
 */
typedef struct NvOdmServicesGpioIntrRec *NvOdmServicesGpioIntrHandle;
/**
 * Defines an opaque handle to the ODM Services SPI interface.
 */
typedef struct NvOdmServicesSpiRec *NvOdmServicesSpiHandle;
/**
 * Defines an opaque handle to the ODM Services I2C interface.
 */
typedef struct NvOdmServicesI2cRec *NvOdmServicesI2cHandle;
/**
 * Defines an opaque handle to the ODM Services PMU interface.
 */
typedef struct NvOdmServicesPmuRec *NvOdmServicesPmuHandle;
/**
 * Defines an opaque handle to the ODM Services PWM interface.
 */
typedef struct NvOdmServicesPwmRec *NvOdmServicesPwmHandle;
/**
 * Defines an opaque handle to the ODM Services one-wire (OWR) interface.
 */
typedef struct NvOdmServicesOwrRec *NvOdmServicesOwrHandle;
/**
 * Defines an opaque handle to the ODM Services key list interface.
 */
typedef struct NvOdmServicesKeyList *NvOdmServicesKeyListHandle;

/**
 * Defines an opaque handle to the ODM Services audio connection interface.
 */
typedef struct NvOdmAudioConnectionRec *NvOdmAudioConnectionHandle;

/**
 * Defines an interrupt handler.
 */
typedef void (*NvOdmInterruptHandler)(void *args);

/**
 * @brief Defines the possible GPIO pin modes.
 */
typedef enum
{
    /// Specifies that that the pin is tristated, which will consume less power.
    NvOdmGpioPinMode_Tristate = 1,

    /// Specifies input mode with active low interrupt.
    NvOdmGpioPinMode_InputInterruptLow,

    /// Specifies input mode with active high interrupt.
    NvOdmGpioPinMode_InputInterruptHigh,

    /// Specifies input mode with no events.
    NvOdmGpioPinMode_InputData,

    /// Specifies output mode.
    NvOdmGpioPinMode_Output,

    /// Specifies special function.
    NvOdmGpioPinMode_Function,

    /// Specifies input and interrupt on any edge.
    NvOdmGpioPinMode_InputInterruptAny,

    /// Specifies input and interrupt on rising edge.
    NvOdmGpioPinMode_InputInterruptRisingEdge,

    /// Specifies output and interrupt on falling edge.
    NvOdmGpioPinMode_InputInterruptFallingEdge,

    /// Specifies to enable interrupt.
    NvOdmGpioPinMode_InterruptEnable,

    /// Specifies to disable interrupt.
    NvOdmGpioPinMode_InterruptDisable,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmGpioPinMode_Force32 = 0x7fffffff

} NvOdmGpioPinMode;

/**
 * Defines the opaque handle to the GPIO pin.
 */
typedef struct NvOdmGpioPinRec *NvOdmGpioPinHandle;

/**
 * Creates and opens a GPIO handle. The handle can then be used to
 * access GPIO functions.
 *
 * @see NvOdmGpioClose
 *
 * @return The handle to the GPIO controller, or NULL if an error occurred.
 */
NvOdmServicesGpioHandle NvOdmGpioOpen(void);

/**
 * Closes the GPIO handle. Any pin settings made while this handle is
 * open will remain. All events enabled by this handle are
 * disabled.
 *
 * @see NvOdmGpioOpen
 *
 * @param hOdmGpio The GPIO handle.
 */
void NvOdmGpioClose(NvOdmServicesGpioHandle hOdmGpio);

/**
 * Acquires a pin handle to be used in subsequent calls to
 * access the pin.
 *
 * @see NvOdmGpioClose
 *
 * @param hOdmGpio The GPIO handle.
 * @param port The port.
 * @param Pin The pin for which to return the handle.
 *
 * @return The pin handle, or NULL if an error occurred.
 */
NvOdmGpioPinHandle
NvOdmGpioAcquirePinHandle(NvOdmServicesGpioHandle hOdmGpio,
        NvU32 port, NvU32 Pin);

/**
 * Releases the pin handle that was acquired by NvOdmGpioAcquirePinHandle()
 * and used by the rest of the GPIO ODM APIs.
 *
 * @see NvOdmGpioAcquirePinHandle
 *
 * @param hOdmGpio The GPIO handle.
 * @param hPin The pin handle to release.
 */
void
NvOdmGpioReleasePinHandle(NvOdmServicesGpioHandle hOdmGpio,
        NvOdmGpioPinHandle hPin);
/**
 * Sets the output state of a set of GPIO pins.
 *
 * @see NvOdmGpioOpen, NvOdmGpioGetState
 *
 * @param hOdmGpio The GPIO handle.
 * @param hGpioPin The pin handle.
 * @param PinValue The pin state to set. 0 means drive low, 1 means drive high.
 */
void
NvOdmGpioSetState(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 PinValue);

/**
 * Gets the output state of a specified set of GPIO pins in the port.
 *
 * @see NvOdmGpioOpen, NvOdmGpioSetState
 *
 * @param hOdmGpio The GPIO handle.
 * @param hGpioPin The pin handle.
 * @param pPinStateValue A pointer to the returned current state of the pin.
 */
void
NvOdmGpioGetState(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 *pPinStateValue);

/**
 * Configures the GPIO to specific mode. Don't use this API to configure the pin
 * as interrupt pin, instead use the NvOdmGpioInterruptRegister
 *  and NvOdmGpioInterruptUnregister APIs which internally call this function.
 *
 * @param hOdmGpio  The GPIO handle.
 * @param hGpioPin  The pin handle.
 * @param Mode      The mode type to configure.
 */
void
NvOdmGpioConfig(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode Mode);

/**
 * Registers an interrupt callback function and the mode of interrupt for the
 * GPIO pin specified.
 *
 * Callback uses the interrupt thread and the interrupt stack on Linux
 * and IST on Windows CE; so, care should be taken on all the APIs used in
 * the callback function.
 *
 * Interrupts are masked when they are triggered. It is up to the caller to
 * re-enable the interrupts by calling NvOdmGpioInterruptDone().
 *  
 * @param hOdmGpio  The GPIO handle.
 * @param hGpioIntr A pointer to the GPIO interrupt handle. Use this 
 *  handle while unregistering the interrupt. On failure to hook
 *  up the interrupt, a NULL handle is returned.
 * @param hGpioPin  The pin handle.
 * @param Mode      The mode type to configure. Allowed mode values are:
 *  - NvOdmGpioPinMode_InputInterruptFallingEdge
 *  - NvOdmGpioPinMode_InputInterruptRisingEdge
 *  - NvOdmGpioPinMode_InputInterruptAny
 *  - NvOdmGpioPinMode_InputInterruptLow
 *  - NvOdmGpioPinMode_InputInterruptHigh
 *  
 * @param Callback The callback function that is called when 
 *  the interrupt triggers.
 * @param arg The argument used when the callback is called by the ISR. 
 * @param DebounceTime The debounce time in milliseconds.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmGpioInterruptRegister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmServicesGpioIntrHandle *hGpioIntr,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode Mode,
    NvOdmInterruptHandler Callback,
    void *arg,
    NvU32 DebounceTime);

/**
 *  Client of GPIO interrupt to re-enable the interrupt after
 *  the handling the interrupt.
 *
 *  @param handle GPIO interrupt handle returned by a sucessfull call to
 *  NvOdmGpioInterruptRegister().
 */
void NvOdmGpioInterruptDone( NvOdmServicesGpioIntrHandle handle );

/**
 * Mask/Unmask a gpio interrupt.
 *
 * Drivers can use this API to fend off interrupts. Mask means interrupts are
 * not forwarded to the CPU. Unmask means, interrupts are forwarded to the CPU.
 * In case of SMP systems, this API masks the interrutps to all the CPU, not
 * just the calling CPU.
 *
 *
 * @param handle    Interrupt handle returned by NvOdmGpioInterruptRegister API.
 * @param mask      NV_FALSE to forrward the interrupt to CPU. NV_TRUE to 
 * mask the interupts to CPU.
 */
void
NvOdmGpioInterruptMask(NvOdmServicesGpioIntrHandle handle, NvBool mask);

/**
 * Unregisters the GPIO interrupt handler.
 *
 * @param hOdmGpio  The GPIO handle.
 * @param hGpioPin  The pin handle.
 * @param handle The interrupt handle returned by a successfull call to
 * NvOdmGpioInterruptRegister().
 *
 */
void
NvOdmGpioInterruptUnregister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmServicesGpioIntrHandle handle);

/**
 * Obtains a handle that can be used to access one of the serial peripheral
 * interface (SPI) controllers.
 *
 * There may be one or more instances of the SPI, depending upon the SOC,
 * and these instances start from 0.
 *
 * @see NvOdmSpiClose
 *
 * @param OdmIoModule  The ODM I/O module for the SFLASH, SPI, or SLINK.
 * @param ControllerId  The SPI controlled ID for which a handle is required.
 *     Valid SPI channel IDs start from 0.
 *
 *
 * @return The handle to the SPI controller, or NULL if an error occurred.
 */
NvOdmServicesSpiHandle NvOdmSpiOpen(NvOdmIoModule OdmIoModule, NvU32 ControllerId);

/**
 * Obtains a handle that can be used to access one of the serial peripheral
 * interface (SPI) controllers, for SPI controllers which are multiplexed
 * between multiple pin mux configurations. The SPI controller's pin mux
 * will be reset to the specified value every transaction, so that two handles
 * to the same controller may safely interleave across pin mux configurations.
 *
 * The ODM pin mux query for the specified controller must be
 * NvOdmSpiPinMap_Multiplexed in order to create a handle using this function.
 *
 * There may be one or more instances of the SPI, depending upon the SOC,
 * and these instances start from 0.
 *
 * Currently, this function is only supported for OdmIoModule_Spi, instance 2.
 *
 * @see NvOdmSpiClose
 *
 * @param OdmIoModule  The ODM I/O module for the SFLASH, SPI, or SLINK.
 * @param ControllerId  The SPI controlled ID for which a handle is required.
 *     Valid SPI channel IDs start from 0.
 * @param PinMap The pin mux configuration to use for every transaction.
 *
 * @return The handle to the SPI controller, or NULL if an error occurred.
 */
NvOdmServicesSpiHandle 
NvOdmSpiPinMuxOpen(NvOdmIoModule OdmIoModule,
                   NvU32 ControllerId,
                   NvOdmSpiPinMap PinMap);


/**
 * Releases a handle to an SPI controller. This API must be called once per
 * successful call to NvOdmSpiOpen().
 *
 * @param hOdmSpi A SPI handle allocated in a call to \c NvOdmSpiOpen.  If \em hOdmSpi
 *     is NULL, this API has no effect.
 */
void NvOdmSpiClose(NvOdmServicesSpiHandle hOdmSpi);

/**
 * Performs an SPI controller transaction. Every SPI transaction is by
 * definition a simultaneous read and write transaction, so there are no
 * separate APIs for read versus write. However, if you only need to do a read or
 * write, this API allows you to declare that you are not interested in the read
 * data, or that the write data is not of interest.
 *
 * This is a blocking API. When it returns, all of the data has been sent out
 * over the pins of the SOC (the transaction). This is true even if the read data
 * is being discarded, as it cannot merely have been queued up.
 *
 * Several SPI transactions may be performed in a single call to this API, but
 * only if all of the transactions are to the same chip select and have the same
 * packet size.
 *
 * Transaction sizes from 1 bit to 32 bits are supported. However, all
 * of the buffers in memory are byte-aligned. To perform one transaction,
 * the \em Size argument should be:
 *
 * <pre>
 *   (PacketSize + 7)/8
 * </pre>
 *
 * To perform n transactions, \em Size should be:
 *
 * <pre>
 *   n*((PacketSize + 7)/8)
 * </pre>
 *
 * Within a given transaction with the packet size larger than 8 bits,
 * the bytes are stored in the order of the most significant byte (MSB) first.
 * The packet is formed with the first byte in MSB and then the next byte
 * will be in the next MSB towards the least significant byte (LSB).
 *
 * For the example, if one packet must be sent and its size is 20 bits
 * then it will require the 3 bytes in the \a pWriteBuffer and arrangement of
 * the data would be as follows:
 * - The packet is 0x000ABCDE (Packet with length of 20 bit).
 * - pWriteBuff[0] = 0x0A
 * - pWriteBuff[1] = 0xBC
 *  -pWtriteBuff[2] = 0xDE
 *
 * The MSB will be transmitted first, i.e., bit 20 is
 * transmitted first and bit 0 is transmitted last.
 *
 * If the transmitted packet (command + receive data) is greater than 32
 * (such as 33) and wants to transfer in the single call (CS should be active)
 * then it can be transmitted in following way:
 *
 * The transfer is the following:
 * <pre>
 * command(8 bit)+Dummy(1bit)+Read (24 bit) = 33 bit
 * </pre>
 *
 * Send 33 bits as 33 bytes and each byte has the 1 valid bit, so the
 * packet bit length = 1 and bytes requested = 33.
 *
 * @code
 * NvU8 pSendData[33], pRecData[33];
 *  pSendData[0] = (Comamnd >>7) & 0x1;
 *  pSendData[1] = (Command >> 6)& 0x1; 
 * ::::::::::::::
 * pSendData[8] = DummyBit;
 * pSendData[9] to pSendData[32] = 0;
 * @endcode
 *
 * Call:
 *
 * @code
 * NvOdmSpiTransaction(hRmSpi,ChipSelect,ClockSpeedInKHz,pRecData, pSendData, 33,1);
 * @endcode
 *
 * Now you will get the read data from \a pRecData[9] to \a pRecData[32] on
 * bit 0 on each byte.
 *
 * The 33-bit transfer can be also done as 11 bytes and each byte has the
 * 3 valid bits. This must rearrange the command in the \a pSendData in such
 * a way that each byte has the 3 valid bits.
 *
 * @code
 * NvU8 pSendData[11], pRecData[11];
 *  pSendData[0] = (Comamnd >>4) & 0x7;
 *  pSendData[1] = (Command >> 1)& 0x7; 
 *  pSendData[2] = (((Command)& 0x3) <<1) | DummyBit; 
 * pSendData[3] to pSendData[10] = 0;
 * @endcode
 *
 * Call:
 *
 * @code
 * NvOdmSpiTransaction(hRmSpi, ChipSelect,ClockSpeedInKHz,pRecData, pSendData, 11,3);
 * @endcode
 *
 * Now You will get the read data from \a pRecData[4] to \a pRecData[10]
 * on lower 3 bits on each byte.
 *
 * Similarly the 33-bit transfer can also be done as 6 bytes and each
 * 2 bytes contain the 11 valid bits. Call:
 *
 * @code
 * NvOdmSpiTransaction(hRmSpi, ChipSelect,ClockSpeedInKHz,pRecData, pSendData, 6,11);
 * @endcode
 *
 * \em ReadBuf and \em WriteBuf may be the same pointer, in which case the write
 * data is destroyed as we read in the read data. Unless they are identical pointers,
 * however, \em ReadBuf and \em WriteBuf must not overlap.
 *
 * @param hOdmSpi The SPI handle allocated in a call to NvOdmSpiOpen().
 * @param ChipSelect Select with which of the several external devices (attached
 *     to a single controller) we are communicating. Chip select indices
 *     start at 0.
 * @param ClockSpeedInKHz The speed in kHz on which the device can communicate.
 * @param ReadBuf A pointer to buffer to be filled in with read data. If this
 *     pointer is NULL, the read data will be discarded.
 * @param WriteBuf A pointer to a buffer from which to obtain write data. If this
 *     pointer is NULL, the write data will be all zeros.
 * @param Size The size of \em ReadBuf and \em WriteBuf buffers in bytes.
 * @param PacketSize The packet size in bits of each SPI transaction.
 */
void
NvOdmSpiTransaction(
    NvOdmServicesSpiHandle hOdmSpi,
    NvU32 ChipSelect,
    NvU32 ClockSpeedInKHz,
    NvU8 *ReadBuf,
    const NvU8 *WriteBuf,
    NvU32 Size,
    NvU32 PacketSize);

/**
 * Sets the signal mode for the SPI communication for a given chip select.
 * After calling this API, further communication happens with the newly 
 * configured signal modes.
 * The default value of the signal mode is taken from ODM Query, and this
 * API overrides the signal mode that is read from the query.
 *
 * @param hOdmSpi The SPI handle allocated in a call to NvOdmSpiOpen().
 * @param ChipSelectId The chip select ID to which the device is connected.
 * @param SpiSignalMode The ODM signal mode to be set.
 *
 */
void
NvOdmSpiSetSignalMode(
    NvOdmServicesSpiHandle hOdmSpi,
    NvU32 ChipSelectId,
    NvOdmQuerySpiSignalMode SpiSignalMode);

/// Contains the return status for the OWR transaction.
typedef enum
{
    NvOdmOwrStatus_Success = 0,
    NvOdmOwrStatus_NotSupported,
    NvOdmOwrStatus_InvalidState,
    NvOdmOwrStatus_InternalError,
    NvOdmOwrStatus_Force32 = 0x7FFFFFFF
} NvOdmOwrStatus;

/// Flag to indicate the OWR read unique address of the device.
#define NVODM_OWR_ADDRESS_READ      0x00000001
/// Flag to indicate the OWR memory read operation.
#define NVODM_OWR_MEMORY_READ       0x00000002
/// Flag to indicate the OWR memory write operation.
#define NVODM_OWR_MEMORY_WRITE      0x00000004
/// Contians the OWR transaction details.
typedef struct
{
    /// Flags to indicate the transaction details, like write/read operation.
    NvU32 Flags;

    /// Offset in the OWR device to perform memory read from or memory write to.
    NvU32 Offset;

    /// Number of bytes to be transferred.
    NvU32 NumBytes;

    /// OWR device ROM ID. This can be zero, if there is a single OWR device on
    /// the bus.
    NvU32 Address;

    /// Write/read buffer. For OWR write operation this buffer should be
    /// filled with the data to be sent to the OWR device. For OWR read
    /// operation this buffer is filled with the data received from OWR device.
    NvU8 *Buf;
} NvOdmOwrTransactionInfo;

/**
 * Initializes and opens the OWR channel. This function allocates the
 * handle for the OWR channel and provides it to the client.
 *
 * @see NvOdmOwrClose
 *
 * @param instance The instance of the OWR driver to be opened.
 *
 * @return The handle to the OWR controller, or NULL if an error occurred.
 */
NvOdmServicesOwrHandle NvOdmOwrOpen(NvU32 instance);

/**
 * Obtains a handle that can be used to access one of the OWR controllers, 
 * for OWR controllers that are multiplexed between multiple pin mux 
 * configurations. The OWR controller's pin mux will be reset to the specified 
 * value every transaction, so that two handles to the same controller may 
 * safely interleave across pin mux configurations.
 *
 * @see NvOdmOwrClose
 *
 * @param ControllerId  The OWR controlled ID for which a handle is required.
 *     Valid OWR controller IDs start from 0.
 * @param PinMap The pin mux configuration to use for every transaction.
 *
 * @return The handle to the OWR controller, or NULL if an error occurred.
*/
NvOdmServicesOwrHandle
NvOdmOwrPinMuxOpen(NvU32 ControllerId,
                   NvOdmOwrPinMap PinMap);

/**
 * Closes the OWR channel. This function frees the memory allocated for
 * the OWR handle and de-initializes the OWR ODM channel.
 *
 * @see NvOdmOwrOpen
 *
 * @param hOdmOwr The handle to the OWR channel.
 */
void NvOdmOwrClose(NvOdmServicesOwrHandle hOdmOwr);

/**
 * Does multiple OWR transaction. Each transaction can be a read or write.
 *
 * @param hOdmOwr The handle to the OWR channel.
 * @param TransactionInfo A pointer to the array of OWR transaction structures.
 * @param NumberOfTransactions The number of OWR transactions.
 *
 * @retval NvOdmOwrStatus_Success If successful, or the appropriate error code.
*/
NvOdmOwrStatus NvOdmOwrTransaction(
    NvOdmServicesOwrHandle hOdmOwr,
    NvOdmOwrTransactionInfo *TransactionInfo,
    NvU32 NumberOfTransactions);

/// Contains the return status for the I2C transaction.
typedef enum
{
    NvOdmI2cStatus_Success = 0,
    NvOdmI2cStatus_Timeout,
    NvOdmI2cStatus_SlaveNotFound,
    NvOdmI2cStatus_InvalidTransferSize,
    NvOdmI2cStatus_ReadFailed,
    NvOdmI2cStatus_WriteFailed,
    NvOdmI2cStatus_InternalError,
    NvOdmI2cStatus_ArbitrationFailed,
    NvOdmI2cStatus_Force32 = 0x7FFFFFFF
} NvOdmI2cStatus;

/// Flag to indicate the I2C write/read operation.
#define NVODM_I2C_IS_WRITE            0x00000001
/// Flag to indicate the I2C slave address type as 10-bit or not.
#define NVODM_I2C_IS_10_BIT_ADDRESS   0x00000002
/// Flag to indicate the I2C transaction with repeat start.
#define NVODM_I2C_USE_REPEATED_START  0x00000004
/// Flag to indicate that the I2C slave will not generate ACK.
#define NVODM_I2C_NO_ACK              0x00000008
/// Flag to indicate software I2C transfer using GPIO.
#define NVODM_I2C_SOFTWARE_CONTROLLER 0x00000010

/// Macro to convert the 7-bit address to the compatible with the transaction
/// address.
#define NVODM_I2C_7BIT_ADDRESS(Addr7Bit) ((Addr7Bit) << 1) 

/// Macro to convert the 10-bit address to the compatible with the transaction
/// address.
#define NVODM_I2C_10BIT_ADDRESS(Addr10Bit) (Addr10Bit)

/// Contians the I2C transaction details.
typedef struct
{
    /// Flags to indicate the transaction details, like write/read operation,
    /// slave address type 10-bit or 7-bit and the transaction uses repeat
    /// start or a normal transaction.
    NvU32 Flags;
    /// I2C slave device address. The address can be initialized as:
    /// @code
    /// x.Address = NVODM_I2C_7BIT_ADDRESS(7BitAddress)
    /// x.Address = NVODM_I2C_7BIT_ADDRESS(8BitAddress)
    /// @endcode
    /// If the address is initialized without using the macro then it will be
    /// assumed that 7-bit address is right shifted by 1 if the address is 7 bit
    /// or 10 bit based on flag. For example, if the
    /// device is EEPROM type and the device address is 0x50 (7 bit address)
    /// then pass this as:
    /// <pre>
    /// x.address = NVODM_I2C_7BIT_ADDRESS(0x50) or
    /// x.address = 0xA0;
    /// </pre>
    NvU32 Address;
    /// Number of bytes to be transferred.
    NvU32 NumBytes;
    /// Send/receive buffer. For I2C send operation this buffer should be
    /// filled with the data to be sent to the slave device. For I2C receive
    /// operation this buffer is filled with the data received from the slave device.
    NvU8 *Buf;
} NvOdmI2cTransactionInfo;

/**
 * Initializes and opens the I2C channel. This function allocates the
 * handle for the I2C channel and provides it to the client.
 *
 * @see NvOdmI2cClose
 *
 * @param OdmIoModuleId  The ODM I/O module for I2C.
 * @param instance The instance of the I2C driver to be opened.
 *
 * @return The handle to the I2C controller, or NULL if an error occurred.
 */
NvOdmServicesI2cHandle
NvOdmI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance);

/**
 * Obtains a handle that can be used to access one of the I2C controllers, 
 * for I2C controllers which are multiplexed between multiple pin mux 
 * configurations. The I2C controller's pin mux will be reset to the specified 
 * value every transaction, so that two handles to the same controller may 
 * safely interleave across pin mux configurations.
 *
 * The ODM pin mux query for the specified controller must be
 * NvOdmI2cPinMap_Multiplexed in order to create a handle using this function.
 *
 * There may be one or more instances of the I2C, depending upon the SOC,
 * and these instances start from 0.
 *
 * Currently, this function is only supported for OdmIoModule_I2C, instance 1.
 *
 * @see NvOdmI2cClose
 *
 * @param OdmIoModule  The ODM I/O module for the I2C.
 * @param ControllerId  The I2C controlled ID for which a handle is required.
 *     Valid I2C controller IDs start from 0.
 * @param PinMap The pin mux configuration to use for every transaction.
 *
 * @return The handle to the I2C controller, or NULL if an error occurred.
 */
NvOdmServicesI2cHandle 
NvOdmI2cPinMuxOpen(NvOdmIoModule OdmIoModule,
                   NvU32 ControllerId,
                   NvOdmI2cPinMap PinMap);

/**
 * Closes the I2C channel. This function frees the memory allocated for
 * the I2C handle and de-initializes the I2C ODM channel.
 *
 * @see NvOdmI2cOpen
 *
 * @param hOdmI2c The handle to the I2C channel.
 */
void NvOdmI2cClose(NvOdmServicesI2cHandle hOdmI2c);

/**
 * Does the I2C send or receive transactions with the slave deivces. This is a
 * blocking call (with timeout). This API works for both the normal I2C transactions
 * or I2C transactions in repeat start mode.
 *
 * For the I2C transactions with slave devices, a pointer to the list of required
 * transactions must be passed and the corresponding number of transactions must
 * be passed.
 *
 * The transaction information structure contains the flags (to indicate the
 * transaction information, such as read or write transaction, transaction is with
 * repeat-start or normal transaction and the slave device address type is 7-bit or
 * 10-bit), slave deivce address, buffer to be transferred and number of bytes
 * to be transferred.
 *
 * @param hOdmI2c The handle to the I2C channel.
 * @param TransactionInfo A pointer to the array of I2C transaction structures.
 * @param NumberOfTransactions The number of I2C transactions.
 * @param ClockSpeedKHz Specifies the clock speed for the I2C transactions.
 * @param WaitTimeoutInMilliSeconds The timeout in milliseconds.
 *  \c NV_WAIT_INFINITE specifies to wait forever.
 *
 * @retval NvOdmI2cStatus_Success If successful, or the appropriate error code.
 */
NvOdmI2cStatus
NvOdmI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *TransactionInfo,
    NvU32 NumberOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMilliSeconds);

/**
 *  Defines the PMU VDD rail capabilities.
 */
typedef struct NvOdmServicesPmuVddRailCapabilitiesRec
{
    /// Specifies ODM protection attribute; if \c NV_TRUE PMU hardware
    ///  or ODM Kit would protect this voltage from being changed by NvDdk client.
    NvBool RmProtected;

    /// Specifies the minimum voltage level in mV.
    NvU32 MinMilliVolts;

    /// Specifies the step voltage level in mV.
    NvU32 StepMilliVolts;

    /// Specifies the maximum voltage level in mV.
    NvU32 MaxMilliVolts;

    /// Specifies the request voltage level in mV.
    NvU32 requestMilliVolts;

    /// Specifies the maximum current it can draw in mA.
    NvU32 MaxCurrentMilliAmp;

} NvOdmServicesPmuVddRailCapabilities;

/// Special level to indicate voltage plane is disabled.
#define NVODM_VOLTAGE_OFF (0UL)

/**
 * Initializes and opens the PMU driver. The handle that is returned by this
 * driver is used for all the other PMU operations.
 *
 * @see NvOdmPmuClose
 *
 * @return The handle to the PMU driver, or NULL if an error occurred.
 */
NvOdmServicesPmuHandle NvOdmServicesPmuOpen(void);

/**
 * Closes the PMU handle.
 *
 * @see NvOdmServicesPmuOpen
 *
 * @param handle The handle to the PMU driver.
 */
void NvOdmServicesPmuClose(NvOdmServicesPmuHandle handle);

/**
 * Gets capabilities for the specified PMU rail.
 *
 * @param handle The handle to the PMU driver.
 * @param vddId The ODM-defined PMU rail ID.
 * @param pCapabilities A pointer to the targeted
 *  capabilities returned by the ODM.
 *
 */
void NvOdmServicesPmuGetCapabilities(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvOdmServicesPmuVddRailCapabilities * pCapabilities );

/**
 * Gets current voltage level for the specified PMU rail.
 *
 * @param handle The handle to the PMU driver.
 * @param vddId The ODM-defined PMU rail ID.
 * @param pMilliVolts A pointer to the voltage level returned
 *  by the ODM.
 */
void NvOdmServicesPmuGetVoltage(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvU32 * pMilliVolts );

/**
 * Sets new voltage level for the specified PMU rail.
 *
 * @param handle The handle to the PMU driver.
 * @param vddId The ODM-defined PMU rail ID.
 * @param MilliVolts The new voltage level to be set in millivolts (mV).
 *  Set to ::NVODM_VOLTAGE_OFF to turn off the target voltage.
 * @param pSettleMicroSeconds A pointer to the settling time in microseconds (uS),
 *  which is the time for supply voltage to settle after this function
 *  returns; this may or may not include PMU control interface transaction time,
 *  depending on the ODM implementation. If NULL this parameter is ignored.
 */
void NvOdmServicesPmuSetVoltage(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvU32 MilliVolts,
        NvU32 * pSettleMicroSeconds );

/**
 * Configures SoC power rail controls for the upcoming PMU voltage transition.
 *
 * @note Should be called just before PMU rail On/Off, or Off/On transition.
 *  Should not be called if rail voltage level is changing within On range.
 * 
 * @param handle The handle to the PMU driver.
 * @param vddId The ODM-defined PMU rail ID.
 * @param Enable Set NV_TRUE if target voltage is about to be turned On, or
 *  NV_FALSE if target voltage is about to be turned Off.
 */
void NvOdmServicesPmuSetSocRailPowerState(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId, 
        NvBool Enable );

/**
 * Defines battery instances.
 */
typedef enum
{
    /// Specifies main battery.
    NvOdmServicesPmuBatteryInst_Main,

    /// Specifies backup battery.
    NvOdmServicesPmuBatteryInst_Backup,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmServicesPmuBatteryInstance_Force32 = 0x7FFFFFFF
} NvOdmServicesPmuBatteryInstance;

/**
 * Gets the battery status.
 *
 * @param handle The handle to the PMU driver.
 * @param batteryInst The battery type.
 * @param pStatus A pointer to the battery
 *  status returned by the ODM.
 * 
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool 
NvOdmServicesPmuGetBatteryStatus(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU8 * pStatus);

/**
 * Defines battery data.
 */
typedef struct NvOdmServicesPmuBatteryDataRec
{
    /// Specifies battery life percent.
    NvU32 batteryLifePercent;

    /// Specifies battery life time.
    NvU32 batteryLifeTime;

    /// Specifies voltage.
    NvU32 batteryVoltage;
    
    /// Specifies battery current.
    NvS32 batteryCurrent;

    /// Specifies battery average current.
    NvS32 batteryAverageCurrent;

    /// Specifies battery interval.
    NvU32 batteryAverageInterval;

    /// Specifies the mAH consumed.
    NvU32 batteryMahConsumed;

    /// Specifies battery temperature.
    NvU32 batteryTemperature;

    /// Specifies battery State Of Charge [0 - 100]%
    NvU32 batterySoc;
} NvOdmServicesPmuBatteryData;

/**
 * Gets the battery data.
 *
 * @param handle The handle to the PMU driver.
 * @param batteryInst The battery type.
 * @param pData A pointer to the battery
 *  data returned by the ODM.
 * 
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmServicesPmuGetBatteryData(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryData * pData);

/**
 * Gets the battery full lifetime.
 *
 * @param handle The handle to the PMU driver.
 * @param batteryInst The battery type.
 * @param pLifeTime A pointer to the battery
 *  full lifetime returned by the ODM.
 */
void
NvOdmServicesPmuGetBatteryFullLifeTime(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU32 * pLifeTime);

/**
 * Defines battery chemistry.
 */
typedef enum
{
    /// Specifies an alkaline battery.
    NvOdmServicesPmuBatteryChemistry_Alkaline,

    /// Specifies a nickel-cadmium (NiCd) battery.
    NvOdmServicesPmuBatteryChemistry_NICD,

    /// Specifies a nickel-metal hydride (NiMH) battery.
    NvOdmServicesPmuBatteryChemistry_NIMH,

    /// Specifies a lithium-ion (Li-ion) battery.
    NvOdmServicesPmuBatteryChemistry_LION,

    /// Specifies a lithium-ion polymer (Li-poly) battery.
    NvOdmServicesPmuBatteryChemistry_LIPOLY,

    /// Specifies a zinc-air battery.
    NvOdmServicesPmuBatteryChemistry_XINCAIR,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmServicesPmuBatteryChemistry_Force32 = 0x7FFFFFFF
} NvOdmServicesPmuBatteryChemistry;

/**
 * Gets the battery chemistry.
 *
 * @param handle The handle to the PMU driver.
 * @param batteryInst The battery type.
 * @param pChemistry A pointer to the battery
 *  chemistry returned by the ODM.
 */
void
NvOdmServicesPmuGetBatteryChemistry(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryChemistry * pChemistry);

/**
 * Defines the charging path.
 */
typedef enum
{
    /// Specifies external wall plug charger.
    NvOdmServicesPmuChargingPath_MainPlug, 

    /// Specifies external USB bus charger.
    NvOdmServicesPmuChargingPath_UsbBus,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmServicesPmuChargingPath_Force32 = 0x7FFFFFFF
} NvOdmServicesPmuChargingPath;

/** 
* Sets the charging current limit. 
* 
* @param handle The Rm device handle.
* @param ChargingPath The charging path. 
* @param ChargingCurrentLimitMa The charging current limit in mA. 
* @param ChargerType The charger type.
*/
void 
NvOdmServicesPmuSetChargingCurrentLimit( 
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuChargingPath ChargingPath,
    NvU32 ChargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

/**
 * Obtains a handle to set or get state of keys, for example, the state of the
 * hold switch.
 *
 * @see NvOdmServicesKeyListClose()
 *
 * @return A handle to the key-list, or NULL if this open call fails.
 */
NvOdmServicesKeyListHandle
NvOdmServicesKeyListOpen(void);

/**
 * Releases the handle obtained during the NvOdmServicesKeyListOpen() call and
 * any other resources allocated.
 *
 * @param handle The handle returned from the \c NvOdmServicesKeyListOpen call.
 */
void NvOdmServicesKeyListClose(NvOdmServicesKeyListHandle handle);

/**
 * Searches the list of keys present and returns the value of the appropriate
 * key.
 * @param handle The handle obtained from NvOdmServicesKeyListOpen().
 * @param KeyID The ID of the key whose value is required.
 *
 * @return The value of the corresponding key, or 0 if the key is not
 * present in the list.
 */
NvU32
NvOdmServicesGetKeyValue(
            NvOdmServicesKeyListHandle handle,
            NvU32 KeyID);

/**
 * Searches the list of keys present and sets the value of the key to the value
 * given. If the key is not present, it adds the key to the list and sets the
 * value.
 * @param handle The handle obtained from NvOdmServicesKeyListOpen().
 * @param Key The ID of the key whose value is to be set.
 * @param Value The value to be set for the corresponding key.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmServicesSetKeyValuePair(
            NvOdmServicesKeyListHandle handle,
            NvU32 Key,
            NvU32 Value);

/**
 * @brief Defines the possible PWM modes.
 */

typedef enum
{
    /// Specifies Pwm disabled mode.
    NvOdmPwmMode_Disable = 1,

    /// Specifies Pwm enabled mode.
    NvOdmPwmMode_Enable,

    /// Specifies Blink LED enabled mode
    NvOdmPwmMode_Blink_LED,

    /// Specifies Blink output 32KHz clock enable mode
    NvOdmPwmMode_Blink_32KHzClockOutput,

    /// Specifies Blink disabled mode
    NvOdmPwmMode_Blink_Disable,

    NvOdmPwmMode_Force32 = 0x7fffffffUL

} NvOdmPwmMode;

/**
 * @brief Defines the possible PWM output pin.
 */

typedef enum
{
    /// Specifies PWM Output-0.
    NvOdmPwmOutputId_PWM0 = 1,

    /// Specifies PWM Output-1.
    NvOdmPwmOutputId_PWM1,

    /// Specifies PWM Output-2.
    NvOdmPwmOutputId_PWM2,

    /// Specifies PWM Output-3.
    NvOdmPwmOutputId_PWM3,

    /// Specifies PMC Blink LED.
    NvOdmPwmOutputId_Blink, 

    NvOdmPwmOutputId_Force32 = 0x7fffffffUL

} NvOdmPwmOutputId;

/**
 * Creates and opens a PWM handle. The handle can be used to
 * access PWM functions.
 *
 * @note Only the service client knows when the service can go idle,
 * like in the case of vibrator, so the client suspend entry code
 * must call NvOdmPwmClose() to close the PWM service.
 *
 * @return The handle to the PWM controller, or NULL if an error occurred.
 */
NvOdmServicesPwmHandle NvOdmPwmOpen(void);

/**
 * Releases a handle to a PWM controller. This API must be called once per
 * successful call to NvOdmPwmOpen().
 *
 * @param hOdmPwm The handle to the PWM controller.
 */
void NvOdmPwmClose(NvOdmServicesPwmHandle hOdmPwm);

/**
 *  @brief Configures PWM module as disable/enable. This API is also
 *  used to set the PWM duty cycle and frequency. Beside that, it is 
 *  used to configure PMC' blinking LED if OutputId is 
 *  NvOdmPwmOutputId_Blink
 *
 *  @param hOdmPwm The PWM handle obtained from NvOdmPwmOpen().
 *  @param OutputId The PWM output pin to configure. Allowed values are
 *   defined in ::NvOdmPwmOutputId.
 *  @param Mode The mode type to configure. Allowed values are
 *   defined in ::NvOdmPwmMode.
 *  @param DutyCycle The duty cycle is an unsigned 15.16 fixed point
 *   value that represents the PWM duty cycle in percentage range from
 *   0.00 to 100.00. For example, 10.5 percentage duty cycle would be
 *   represented as 0x000A8000. This parameter is ignored if NvOdmPwmMode
 *   is NvOdmMode_Blink_32KHzClockOutput or NvOdmMode_Blink_Disable
 *  @param pRequestedFreqHzOrPeriod A pointer to the request frequency in Hz
 *   or period in second
 *   A requested frequency value beyond the maximum supported value will be
 *   clamped to the maximum supported value. If \em pRequestedFreqHzOrPeriod 
 *   is NULL, it returns the maximum supported frequency. This parameter is 
 *   ignored if NvOdmPwmMode is NvOdmMode_Blink_32KHzClockOutput or
 *   NvOdmMode_Blink_Disable
 *  @param pCurrentFreqHzOrPeriod A pointer to the returned frequency of 
 *   that mode. If PMC Blink LED is used then it is the pointer to the returns 
 *   period time. This parameter is ignored if NvOdmPwmMode is
 *   NvOdmMode_Blink_32KHzClockOutput or NvOdmMode_Blink_Disable
 */
void
NvOdmPwmConfig(NvOdmServicesPwmHandle hOdmPwm,
    NvOdmPwmOutputId OutputId, 
    NvOdmPwmMode Mode,
    NvU32 DutyCycle,
    NvU32 *pRequestedFreqHzOrPeriod,
    NvU32 *pCurrentFreqHzOrPeriod);

/**
 * Enables and disables external clock interfaces (e.g., CDEV and CSUS pins)
 * for the specified peripheral. External clock sources should be enabled
 * prior to programming peripherals reliant on them. If multiple peripherals use
 * the same external clock source, it is safe to call this API multiple times.
 *
 * @note For Tegra 3, please use ::NvOdmExternalPheriphPinConfig API.
 *
 * @param Guid The ODM-defined GUID of the peripheral to be configured. The
 *             peripheral should have an @see NvOdmIoAddress entry for the
 *             NvOdmIoModule_ExternalClock device interface. If multiple
 *             external clock interfaces are specified, all will be
 *             enabled (disabled).
 *
 * @param EnableTristate NV_TRUE will tristate the specified clock sources,
 *             NV_FALSE will drive them.
 *  
 * @param pInstances Returns the list of clocks that were enabled.
 *  
 * @param pFrequencies Returns the frequency, in kHz, that is 
 *                     being output on each clock pin
 *  
 * @param pNum Returns the number of clocks that were enabled.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmExternalClockConfig(
    NvU64 Guid,
    NvBool EnableTristate,
    NvU32 *pInstances,
    NvU32 *pFrequencies,
    NvU32 *pNum);


#define NVODM_STANDARD_AUDIO_FREQ        0x1

/**
 * Enables and disables external clock interfaces (e.g., CDEV and CSUS pins)
 * for the specified peripheral. External clock sources must be enabled
 * prior to programming peripherals reliant on them. If multiple peripherals use
 * the same external clock source, it is safe to call this API multiple times.
 *
 * @param Guid The ODM-defined GUID of the peripheral to be configured. The
 *             peripheral should have an ::NvOdmIoAddress entry for the
 *             ::NvOdmIoModule_ExternalClock device interface. If multiple
 *             external clock interfaces are specified, all will be
 *             enabled (disabled).
 *
 * @param Instance External peripheral instance number.
 *
 * @param EnableTristate NV_TRUE tristates the specified clock sources,
 *             NV_FALSE drives them.
 *
 * @param MinFreq Requested minimum frequency for hardware module operation.
 *
 * @param MaxFreq Requested maximum frequency for hardware module operation.
 *
 * @param TargetFreq Preferred frequency.
 *
 * @param CurrentFreq Returns a pointer to the current clock frequency of that
 *      module. NULL is a valid value for this parameter.
 * @param flags Module-specific flags. This flag must be set  to ::NVODM_STANDARD_AUDIO_FREQ
 *             if targeted frequency is audio standard frequency.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */

 NvBool NvOdmExternalPheriphPinConfig(
    NvU64 Guid,
    NvU32 Instance,
    NvBool EnableTristate,
    NvU32 MinFreq,
    NvU32 MaxFreq,
    NvU32 TargetFreq,
    NvU32 * CurrentFreq,
    NvU32 flags);


/**
 *  Defines SoC strap groups.
 */
typedef enum
{
    /// Specifies the ram_code strap group.
    NvOdmStrapGroup_RamCode = 1,

    NvOdmStrapGroup_Num,
    NvOdmStrapGroup_Force32 = 0x7FFFFFFF
} NvOdmStrapGroup;

/**
 * File input/output.
 */
typedef void* NvOdmOsFileHandle;

/**
 *  Defines the OS file types.
 */
typedef enum
{
    NvOdmOsFileType_Unknown = 0,
    NvOdmOsFileType_File,
    NvOdmOsFileType_Directory,
    NvOdmOsFileType_Fifo,

    NvOdmOsFileType_Force32 = 0x7FFFFFFF
} NvOdmOsFileType;

/**
 *  Defines the OS status type.
 */
typedef struct NvOdmOsStatTypeRec
{
    NvU64 size;
    NvOdmOsFileType type;
    NvU64 mtime;
} NvOdmOsStatType;

/** Open a file with read permissions. */
#define NVODMOS_OPEN_READ    0x1

/** Open a file with write persmissions. */
#define NVODMOS_OPEN_WRITE   0x2

/** Create a file if is not present on the file system. */
#define NVODMOS_OPEN_CREATE  0x4

/**
 *  Opens a file stream.
 *
 *  If the ::NVODMOS_OPEN_CREATE flag is specified, ::NVODMOS_OPEN_WRITE must also
 *  be specified.
 *
 *  If ::NVODMOS_OPEN_WRITE is specified the file will be opened for write and
 *  will be truncated if it was previously existing.
 *
 *  If ::NVODMOS_OPEN_WRITE and ::NVODMOS_OPEN_READ is specified the file will not
 *  be truncated.
 *
 *  @param path A pointer to the path to the file.
 *  @param flags ORed flags for the open operation (NVODMOS_OPEN_*).
 *  @param file [out] A pointer to the file that will be opened, if successful.
 *
 *  @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmOsFopen(const char *path, NvU32 flags, NvOdmOsFileHandle *file);

/**
 *  Closes a file stream.
 *  Passing in a NULL handle is okay.
 *
 *  @param stream The file stream to close.
 */
void NvOdmOsFclose(NvOdmOsFileHandle stream);

/**
 *  Writes to a file stream.
 *
 *  @param stream The file stream.
 *  @param ptr A pointer to the data to write.
 *  @param size The length of the write.
 *
 *  @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmOsFwrite(NvOdmOsFileHandle stream, const void *ptr, size_t size);

/**
 *  Reads a file stream.
 *
 *  To detect short reads (less that specified amount), pass in \a bytes
 *  and check its value to the expected value. The \a bytes parameter may
 *  be NULL.
 *
 *  @param stream The file stream.
 *  @param ptr A pointer to the buffer for the read data.
 *  @param size The length of the read.
 *  @param bytes [out] A pointer to the number of bytes read -- may be NULL.
 *
 *  @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmOsFread(NvOdmOsFileHandle stream, void *ptr, size_t size, size_t *bytes);

/**
 *  Gets file information.
 *
 *  @param filename A pointer to the file to get information about.
 *  @param stat [out] A pointer to the information structure.
 *
 *  @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmOsStat(const char *filename, NvOdmOsStatType *stat);

/**
 * Defines a half buffer playback function callback.
 */
typedef void (*NvOdmWaveHalfBufferCompleteFxn)(NvBool IsFirstHalf, void *pContext);

/**
 * The property of the wave data which needs to be played.
 */
typedef struct
{
    /// Indicates that the wave data is single channel data or stereo data.
    NvBool IsMonoDataFormat;

    /// Indicates that the wave data is placed in the packed format like left
    /// and right channel data are in the same word (32 bits) or in different
    /// words.
    NvBool IsPackedFormat;

    /// The sampling rate of the wave data in Hz, i.e., for 8 kHz it is 8000.
    NvU32 SamplingRateInHz;

    /// The PCM size of the wave data in bits like 16 for the 16-bit PCM data or
    /// 24 for the 24-bit PCM data.
    NvU32 PcmSampleSizeInBits;

    /// The total size of the wave data in bytes including both channels that
    /// need to be played.
    NvU32 TotalWaveDataSizeInBytes;
} NvOdmWaveDataProp;

/**
 * Plays the wave data. This API is available for the boot loader only. This is
 * not supported in the main image.
 *
 * The wave data will be played through the I2S channel in double buffering
 * continuous type. This means the same buffer will be transferred continuously and
 * there will be notification after every half transfer completes, and after
 * every half of the buffer playback it will call the callback function
 * HalfBuffCompleteCallback() to inform the client that buffer playback is
 * complete and he can copy the new wave data into the played buffer.
 * The client will copy the new chunk of the wave data into the played buffer.
 * The data will be played till the complete transfer is over indicated by the
 * NvOdmWaveDataProp::TotalWaveDataSizeInBytes.
 * The client is advised to set the BufferSizeInBytes such a way that
 * <pre> TotalWaveDataSizeInBytes = n x BufferSizeInBytes </pre> where n is integer.
 * Also the size of the buffer, i.e., BufferSizeInBytes should be sifficient so that
 * during the playback, client can copy the new chunk of the data into the played
 * buffer. The datacopy should be done in the callback function.
 *
 *
 * @param pWaveDataBuffer A pointer to the buffer where the wave data will be
 * copied.
 * @param BufferSizeInBytes The size of the buffer in bytes.
 * @param HalfBuffCompleteCallback The callback function pointer that will be
 * called after completion of the half of the buffer transfer is over. It will
 * be also called after the second half of the buffer transfer is complete.
 * @param pWaveDataProp A pointer to the structure contains the property of
 * the data to be played.
 * @param pContext A pointer to the context.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPlayWave(
    NvU32 *pWaveDataBuffer,
    NvU32 BufferSizeInBytes,
    NvOdmWaveHalfBufferCompleteFxn HalfBuffCompleteCallback,
    NvOdmWaveDataProp *pWaveDataProp,
    void *pContext);

/**
 * Creates and opens an audio connection from a diffenet device to the codec. 
 * This API is only available in the bootloader (OAL) level.
 *
 * @see NvOdmAudioConnectionClose
 *
 * @param hAudioCodec The audio codec handle.
 * @param ConnectionType The type of required connection based on the use 
 * case, or specify ::NvOdmDapConnectionIndex_Unknown if connection index is
 *      used.
 * @param SignalType The source type used for recording or any 
 * special case.
 * @param PortIndex The port index of the source used in the \a Signaltype.
 * @param IsEnable Specifies to enable or disable the connection line.
 *
 * @return The handle to the audio connection, or NULL if an error occurred.
 */
NvOdmAudioConnectionHandle 
NvOdmAudioConnectionOpen(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmDapConnectionIndex ConnectionType,
    NvOdmAudioSignalType SignalType,
    NvU32 PortIndex,
    NvBool IsEnable);

/**
 * Closes the audio connection.
 * This API is only available in the bootloader (OAL) level.
 *
 * @see NvOdmAudioConnectionOpen
 *
 * @param hAudioConnection The handle of the audio connection.
 */
void 
NvOdmAudioConnectionClose(
    NvOdmAudioConnectionHandle hAudioConnection);

/**
 * Pixel color format of the display buffer.
 *
 * NVIDIA Note: when adding new formats always append to bottom of the
 * list. This format identifier is usually read directly from the bitmap 
 * data header and therefore backwards compatibility is important!
 */
typedef enum
{
    // Color channels: R5G6B5 (packed in short)
    NvOdmDispBufferFormat_16Bit = 1,
    // Color channels: A8R8G8B8 (packed in int)
    NvOdmDispBufferFormat_32Bit,
    NvOdmDispBufferFormat_Force32 = 0x7FFFFFFF
} NvOdmDispBufferFormat;

/** Attributes of a display buffer. */
typedef struct
{
    NvU32 StartX;
    NvU32 StartY;
    NvU32 Width;
    NvU32 Height;
    NvU32 Pitch;
    NvOdmDispBufferFormat PixelFormat;
} NvOdmDispBufferAttributes;

typedef NvU32 *NvOdmServicesDispHandle;

/**
 * Opens the display driver.
 *
 * @param hDisplay A pointer to the returned display driver handle.
 * @param bufAttributes A pointer to information about the buffer to be sent
 *      to the display.
 * @param pBuffer A pointer to the buffer to be sent to the display.
 */
void
NvOdmDispOpen(NvOdmServicesDispHandle *hDisplay,
              const NvOdmDispBufferAttributes *bufAttributes,
              const NvU8 *pBuffer);

/**
 * Closes the display driver.
 *
 * @param hDisplay The display driver handle.
 */
void
NvOdmDispClose(NvOdmServicesDispHandle hDisplay);

/**
 * Sends a buffer to the display driver.
 *
 * @param hDisplay The display driver handle.
 * @param bufAttributes A pointer to information about the buffer to be sent
 *      to the display.
 * @param pBuffer A pointer to the buffer to be sent to the display.
 */
void
NvOdmDispSendBuffer(const NvOdmServicesDispHandle hDisplay,
                    const NvOdmDispBufferAttributes *bufAttributes,
                    const NvU8 *pBuffer);

/**
 * Enables or disables USB OTG circuitry.
 *
 * @param Enable NV_TRUE to enable, or NV_FALSE to disable.
 */
void NvOdmEnableOtgCircuitry(NvBool Enable);

/**
 * Checks whether or not USB is connected.
 *
 * @pre The USB circuit is enabled by calling NvOdmEnableOtgCircuitry().
 * To reduce power consumption, disable the USB circuit when not connected
 * by calling \c NvOdmEnableOtgCircuitry(NV_FALSE).
 *
 * @return NV_TRUE if USB is successfully connected, otherwise NV_FALSE.
 */
NvBool NvOdmUsbIsConnected(void);

/**
 * Checks the current charging type.
 *
 * @pre The USB circuit is enabled by calling NvOdmEnableOtgCircuitry().
 * To reduce power consumption, disable the USB circuit when not connected
 * by calling \c NvOdmEnableOtgCircuitry(NV_FALSE).
 *
 * @param Instance Set to 0 by default.
 * @return The current charging type.
 */
NvOdmUsbChargerType NvOdmUsbChargingType(NvU32 Instance);

#if defined(__cplusplus)
}
#endif

/*@}*/
/** @} */

#endif // INCLUDED_NVODM_SERVICES_H
