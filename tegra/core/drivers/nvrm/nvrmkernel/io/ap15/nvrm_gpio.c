/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvrm_gpio_private.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_pinmux_utils.h"
#include "t30/arapbpm.h"
#include "nvodm_gpio_ext.h"
#include "ap15rm_gpio_vi.h"


// Undefine this symbols so that if anyone ever tries to get to the masked
// offset without going through the capabilites table they will get a
// compile-time error. This will save people in the future from wasting
// as much time as I have on this.
#undef  GPIO_MSK_CNF_0

//  Treats GPIO pin handle releases like the pin is completely invalidated:
//  returned to SFIO state and tristated.  See the FIXME comment below
//  to see why this isn't enabled currently...
#define RELEASE_IS_INVALIDATE 1
#define NV_ENABLE_GPIO_POWER_RAIL 0

typedef struct NvRmGpioPinInfoRec {
    NvBool used;
    NvU32 port;
    NvU32 inst;
    NvU32 pin;
    NvRmGpioPinMode mode;
    /* Sets up a chain of pins associated by one semaphore. Usefull to parse the
     * pins when an interrupt is received. */
    NvU32 nextPin;
    NvU16 irqNumber;
} NvRmGpioPinInfo;

typedef struct NvRmGpioRec
{
    NvU32 RefCount;
    NvRmDeviceHandle hRm;
    NvRmGpioPinInfo *pPinInfo;
    NvRmGpioCaps *caps;
} NvRmGpio;


static NvRmGpioHandle s_hGpio = NULL;

static NvOsMutexHandle s_GpioMutex = NULL;

NvError
NvRmGpioOpen(
    NvRmDeviceHandle hRm,
    NvRmGpioHandle* phGpio)
{
    NvError err = NvSuccess;
    NvU32 total_pins;
    NvU32 i;

    NV_ASSERT(hRm);
    NV_ASSERT(phGpio);

    if (!s_GpioMutex)
    {
        err = NvOsMutexCreate(&s_GpioMutex);
        if (err != NvSuccess)
        {
            goto fail;
        }
    }

    NvOsMutexLock(s_GpioMutex);
    if (s_hGpio)
    {
        s_hGpio->RefCount++;
        goto exit;
    }

    s_hGpio = (NvRmGpio *)NvOsAlloc(sizeof(NvRmGpio));
    if (!s_hGpio)
    {
        err = NvError_InsufficientMemory;
        goto exit;
    }
    NvOsMemset(s_hGpio, 0, sizeof(NvRmGpio));
    s_hGpio->hRm = hRm;

    err = NvRmGpioGetCapabilities(hRm, (void **)&(s_hGpio->caps));
    if (err)
    {
        // Was a default supplied?
        if (s_hGpio->caps == NULL)
        {
            goto fail;
        }
    }

    total_pins = s_hGpio->caps->Instances * s_hGpio->caps->PortsPerInstances *
        s_hGpio->caps->PinsPerPort;

    s_hGpio->pPinInfo = NvOsAlloc(sizeof(NvRmGpioPinInfo) * total_pins);
    if (s_hGpio->pPinInfo == NULL)
    {
        NvOsFree(s_hGpio);
        goto exit;
    }
    NvOsMemset(s_hGpio->pPinInfo, 0, sizeof(NvRmGpioPinInfo) * total_pins);
    for (i=0; i<total_pins; i++)
    {
        s_hGpio->pPinInfo[i].irqNumber = NVRM_IRQ_INVALID;
    }
    s_hGpio->RefCount++;

exit:
    *phGpio = s_hGpio;
    NvOsMutexUnlock(s_GpioMutex);

fail:
    return err;
}

void NvRmGpioClose(NvRmGpioHandle hGpio)
{
    if (!hGpio)
        return;

    NV_ASSERT(hGpio->RefCount);

    NvOsMutexLock(s_GpioMutex);
    hGpio->RefCount--;
    if (hGpio->RefCount == 0)
    {
       NvOsFree(s_hGpio->pPinInfo);
       NvOsFree(s_hGpio);
       s_hGpio = NULL;
    }
    NvOsMutexUnlock(s_GpioMutex);
}


NvError
NvRmGpioAcquirePinHandle(
        NvRmGpioHandle hGpio,
        NvU32 port,
        NvU32 pinNumber,
        NvRmGpioPinHandle *phPin)
{
    NvU32 MaxPorts;

    NV_ASSERT(hGpio != NULL);

    NvOsMutexLock(s_GpioMutex);
    if (port == NVRM_GPIO_CAMERA_PORT)
    {
        // The Camera has dedicated gpio pins that must be controlled
        // through a non-standard gpio port control.
        NvRmPrivGpioViAcquirePinHandle(hGpio->hRm, pinNumber);
        *phPin = GPIO_MAKE_PIN_HANDLE(NVRM_GPIO_CAMERA_INST, port, pinNumber);
    }
    else if ((port >= NVODM_GPIO_EXT_PORT_0) &&
             (port <= NVODM_GPIO_EXT_PORT_F))
    {
        // Create a pin handle for GPIOs that are
        // sourced by external (off-chip) peripherals
        *phPin = GPIO_MAKE_PIN_HANDLE((port & 0xFF), port, pinNumber);
    }
    else
    {
        NV_ASSERT(4 == hGpio->caps->PortsPerInstances);
        MaxPorts = hGpio->caps->Instances * 4;

        if ((port > MaxPorts) || (pinNumber > hGpio->caps->PinsPerPort))
        {
            NV_ASSERT(!" Illegal port or pin  number. ");
        }

        *phPin = GPIO_MAKE_PIN_HANDLE(port >> 2, port & 0x3, pinNumber);
    }
    NvOsMutexUnlock(s_GpioMutex);
    return NvSuccess;
}

void NvRmGpioReleasePinHandles(
        NvRmGpioHandle hGpio,
        NvRmGpioPinHandle *hPin,
        NvU32 pinCount)
{
    NvU32 i;
    NvU32 port;
    NvU32 pin;
    NvU32 instance;

    if (hPin == NULL) return;

    for (i=0; i<pinCount; i++)
    {
        instance = GET_INSTANCE(hPin[i]);
        port = GET_PORT(hPin[i]);
        pin = GET_PIN(hPin[i]);

        NvOsMutexLock(s_GpioMutex);
        if (port == NVRM_GPIO_CAMERA_PORT)
        {
            NvRmPrivGpioViReleasePinHandles(hGpio->hRm, pin);
        }
        else if ((port >= NVODM_GPIO_EXT_PORT_0) &&
                 (port <= NVODM_GPIO_EXT_PORT_F))
        {
            // Do nothing for now...
        }
        else
        {
            NvU32 alphaPort;

            alphaPort = instance * s_hGpio->caps->PortsPerInstances + port;
            if (s_hGpio->pPinInfo[pin + alphaPort * s_hGpio->caps->PinsPerPort].used)
            {
                /*  FIXME:  What should be done here?  Some drivers (nvddk_audiomixer) are
                 *  assuming that pin configurations stay valid after releasing the pin
                 *  handle, but ideally we should be able to put the pin back into tristate
                 *  when it is released. */
                NV_DEBUG_PRINTF(("Warning: Releasing in-use GPIO pin handle GPIO_P%c.%02u (%c=%u)\n",
                    'A'+alphaPort,pin,'A'+alphaPort, alphaPort));
#if RELEASE_IS_INVALIDATE
                NvRmSetGpioTristate(hGpio->hRm, alphaPort, pin, NV_TRUE);
                GPIO_MASKED_WRITE(hGpio->hRm, s_hGpio->caps->MaskedOffset, instance, port, CNF, pin, 0);
                s_hGpio->pPinInfo[pin + alphaPort*s_hGpio->caps->PinsPerPort].used = NV_FALSE;
#endif
            }
        }
        NvOsMutexUnlock(s_GpioMutex);
    }

    return;
}


void NvRmGpioReadPins(
        NvRmGpioHandle hGpio,
        NvRmGpioPinHandle *hPin,
        NvRmGpioPinState *pPinState,
        NvU32 pinCount )
{
    NvU32 inst;
    NvU32 port;
    NvU32 pin;
    NvU32 RegValue;
    NvU32 i;

    NV_ASSERT(hPin != NULL);
    NV_ASSERT(hGpio != NULL);
    NV_ASSERT(hGpio->caps != NULL);

    for (i=0; i<pinCount; i++)
    {
        port = GET_PORT(hPin[i]);
        pin = GET_PIN(hPin[i]);
        inst = GET_INSTANCE(hPin[i]);

        if (port == NVRM_GPIO_CAMERA_PORT)
        {
            pPinState[i] = NvRmPrivGpioViReadPins(hGpio->hRm, pin);
        }
        else if ((port >= (NvU32)NVODM_GPIO_EXT_PORT_0) &&
                 (port <= (NvU32)NVODM_GPIO_EXT_PORT_F))
        {
            pPinState[i] = NvOdmExternalGpioReadPins(port, pin);
        }
        else
        {
            GPIO_REGR(hGpio->hRm, inst, port, OE, RegValue);
            if (RegValue & (1<<pin))
            {
                GPIO_REGR(hGpio->hRm, inst, port, OUT, RegValue);
            } else
            {
                GPIO_REGR(hGpio->hRm, inst, port, IN, RegValue);
            }
            pPinState[i] = (RegValue >> pin) & 0x1;
        }
    }
}

void NvRmGpioWritePins(
        NvRmGpioHandle hGpio,
        NvRmGpioPinHandle *hPin,
        NvRmGpioPinState *pPinState,
        NvU32 pinCount )
{
    NvU32 inst;
    NvU32 port;
    NvU32 pin;
    NvU32 i;
    NvU32 MaskedOffset;

    NV_ASSERT(hPin != NULL);
    NV_ASSERT(hGpio != NULL);
    NV_ASSERT(hGpio->caps != NULL);
    MaskedOffset = s_hGpio->caps->MaskedOffset;

    for (i=0; i<pinCount; i++)
    {
        inst = GET_INSTANCE(hPin[i]);
        port = GET_PORT(hPin[i]);
        pin = GET_PIN(hPin[i]);

        if (port == NVRM_GPIO_CAMERA_PORT)
        {
            NvRmPrivGpioViWritePins(hGpio->hRm, pin, pPinState[i]);
        }
        else if ((port >= (NvU32)NVODM_GPIO_EXT_PORT_0) &&
                 (port <= (NvU32)NVODM_GPIO_EXT_PORT_F))
        {
            NvOdmExternalGpioWritePins(port, pin, pPinState[i]);
        }
        else
        {
            //  When updating a contiguous set of pins that are
            //  all located in the same port, merge the register
            //  write into a single atomic update.
            NvU32 updateVec = 0;
            updateVec = (1<<(pin + GPIO_PINS_PER_PORT));
            updateVec |= ((pPinState[i] & 0x1)<<pin);
            while ((i+1<pinCount) &&
                   GET_INSTANCE(hPin[i+1])==inst &&
                   GET_PORT(hPin[i+1]==port))
            {
                pin = GET_PIN(hPin[i+1]);
                updateVec |= (1<<(pin + GPIO_PINS_PER_PORT));
                updateVec |= ((pPinState[i+1]&0x1)<<pin);
                i++;
            }
            NV_REGW(hGpio->hRm, NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, inst ),
                    (port*NV_GPIO_PORT_REG_SIZE)+MaskedOffset+GPIO_OUT_0,
                    updateVec);
        }
    }

    return;
}


NvError NvRmGpioConfigPins(
        NvRmGpioHandle hGpio,
        NvRmGpioPinHandle *hPin,
        NvU32 pinCount,
        NvRmGpioPinMode Mode)
{
    NvError err = NvSuccess;
    NvU32 i;
    NvU32 inst;
    NvU32 port;
    NvU32 pin;
    NvU32 pinNumber;
    NvU32 Reg;
    NvU32 alphaPort;
    NvU32 MaskedOffset;

    NvOsMutexLock(s_GpioMutex);

    for (i=0; i< pinCount; i++)
    {
        inst = GET_INSTANCE(hPin[i]);
        port = GET_PORT(hPin[i]);
        pin = GET_PIN(hPin[i]);

        if (port == NVRM_GPIO_CAMERA_PORT)
        {
            // If they are trying to do the wrong thing, assert.
            // If they are trying to do the only allowed thing,
            // quietly skip it, as nothing needs to be done.
            if (Mode != NvRmGpioPinMode_Output)
                NV_ASSERT(!"Only output is supported for camera gpio.\n");
            continue;
        }

        /* Absolute pin number to index into pPinInfo array and the alphabetic port names. */
        alphaPort = inst * s_hGpio->caps->PortsPerInstances + port;
        pinNumber = pin + alphaPort * s_hGpio->caps->PinsPerPort;

        s_hGpio->pPinInfo[pinNumber].mode = Mode;
        s_hGpio->pPinInfo[pinNumber].inst = inst;
        s_hGpio->pPinInfo[pinNumber].port = port;
        s_hGpio->pPinInfo[pinNumber].pin = pin;
        MaskedOffset = s_hGpio->caps->MaskedOffset;

        /* Don't try to collapse this switch as the ordering of the register
         * writes matter. */
        switch (Mode)
        {
            case NvRmGpioPinMode_Output:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 1);
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);

                break;

            case NvRmGpioPinMode_InputData:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);

                break;

            case NvRmGpioPinMode_InputInterruptLow:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);

                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_LVL, pin, 0);
                break;

            case NvRmGpioPinMode_InputInterruptHigh:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);

                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_LVL, pin, 1);
                break;

            case NvRmGpioPinMode_InputInterruptAny:
                if(hGpio->caps->Features & NVRM_GPIO_CAP_FEAT_EDGE_INTR)
                {
                    GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                    GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);
                    GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_LVL, pin, 1);
                }
                else
                {
                    NV_ASSERT(!"Not supported");
                }

                break;

            case NvRmGpioPinMode_InterruptEnable:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_ENB, pin, 1);
                break;

            case NvRmGpioPinMode_InterruptDisable:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_ENB, pin, 0);
                break;

            case NvRmGpioPinMode_Function:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 0);
                break;
            case NvRmGpioPinMode_Inactive:
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, INT_ENB, pin, 0);
                GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 0);
                break;
            case NvRmGpioPinMode_InputInterruptRisingEdge:
                    if(hGpio->caps->Features & NVRM_GPIO_CAP_FEAT_EDGE_INTR)
                    {
                            GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                            GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);
                            GPIO_REGW(hGpio->hRm, inst, port, INT_CLR, (1 << pin));
                            GPIO_REGR(hGpio->hRm, inst, port, INT_LVL, Reg);
                            //   see the # Bug ID:  359459
                            Reg =  (Reg & GPIO_INT_LVL_UNSHADOWED_MASK) |(Reg & GPIO_INT_LVL_SHADOWED_MASK);
                            Reg |=  (GPIO_INT_LVL_0_BIT_0_FIELD   << pin);
                            Reg |=  (GPIO_INT_LVL_0_EDGE_0_FIELD  << pin);
                            Reg &= ~(GPIO_INT_LVL_0_DELTA_0_FIELD << pin);
                            GPIO_REGW(hGpio->hRm, inst, port, INT_LVL, Reg);
                    }
                    else
                    {
                        NV_ASSERT(!"Not supported");
                    }
                    break;
           case NvRmGpioPinMode_InputInterruptFallingEdge:
                    if(hGpio->caps->Features & NVRM_GPIO_CAP_FEAT_EDGE_INTR)
                    {
                        GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, OE, pin, 0);
                        GPIO_MASKED_WRITE(hGpio->hRm, MaskedOffset, inst, port, CNF, pin, 1);
                        GPIO_REGW(hGpio->hRm, inst, port, INT_CLR, (1 << pin));
                        GPIO_REGR(hGpio->hRm, inst, port, INT_LVL, Reg);
                        // see the # Bug ID:  359459
                        Reg =  (Reg & GPIO_INT_LVL_UNSHADOWED_MASK)|(Reg & GPIO_INT_LVL_SHADOWED_MASK);
                        Reg &= ~(GPIO_INT_LVL_0_BIT_0_FIELD   << pin);
                        Reg |=  (GPIO_INT_LVL_0_EDGE_0_FIELD  << pin);
                        Reg &= ~(GPIO_INT_LVL_0_DELTA_0_FIELD << pin);
                        GPIO_REGW(hGpio->hRm, inst, port, INT_LVL, Reg);
                    }
                    else
                    {
                        NV_ASSERT(!"Not supported");
                    }
                  break;
            default:
                  NV_ASSERT(!"Invalid gpio mode");
                break;
        }

        /*  Pad group global tristates are only modified when the pin transitions
         *  from an inactive state to an active one.  Active-to-active and
         *  inactive-to-inactive transitions are ignored */
        if ((!s_hGpio->pPinInfo[pinNumber].used) && (Mode!=NvRmGpioPinMode_Inactive))
        {
#if NV_ENABLE_GPIO_POWER_RAIL
            err = NvRmGpioIoPowerConfig(hGpio->hRm, alphaPort, pin, NV_TRUE);
#endif
            NvRmSetGpioTristate(hGpio->hRm, alphaPort, pin, NV_FALSE);
        }
        else if ((s_hGpio->pPinInfo[pinNumber].used) && (Mode==NvRmGpioPinMode_Inactive))
        {
#if NV_ENABLE_GPIO_POWER_RAIL
            err = NvRmGpioIoPowerConfig(hGpio->hRm, alphaPort, pin, NV_FALSE);
#endif
            NvRmSetGpioTristate(hGpio->hRm, alphaPort, pin, NV_TRUE);
        }
        if (Mode != NvRmGpioPinMode_Inactive)
            s_hGpio->pPinInfo[pinNumber].used = NV_TRUE;
        else
            s_hGpio->pPinInfo[pinNumber].used = NV_FALSE;
    }

    NvOsMutexUnlock(s_GpioMutex);
    return err;
}

NvError NvRmGpioGetIrqs(
    NvRmDeviceHandle hRmDevice,
    NvRmGpioPinHandle * hPin,
    NvU32 * Irq,
    NvU32 pinCount )
{
    NvU32 i;
    for (i=0; i< pinCount; i++)
    {
        NvU32 port, pin, inst;

        port = GET_PORT(hPin[i]);
        pin = GET_PIN(hPin[i]);
        inst = GET_INSTANCE(hPin[i]);

        Irq[i] = NvRmGetIrqForLogicalInterrupt(hRmDevice,
                    NVRM_MODULE_ID(NvRmPrivModuleID_Gpio, inst),
                    pin + port * GPIO_PINS_PER_PORT);
    }
    return NvSuccess;
}

