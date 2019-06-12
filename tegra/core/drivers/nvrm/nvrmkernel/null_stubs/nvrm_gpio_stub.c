
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_gpio.h"

static inline NvU32 nr_to_handle(int gpio_nr)
{
    if (gpio_nr < 0)
        return 0;

    return gpio_nr | 0x80000000ul;
}

static inline int handle_to_nr(NvU32 handle)
{
    if (!handle)
        return -1;

    return (int)(handle & ~0x80000000ul);
}

struct gpio_data {
    int value;
    int fd;
};

static struct gpio_data pin_data[28*8];

static NvError ExportGpios(
    NvRmGpioPinHandle *pin,
    NvU32 pinCount,
    NvBool export)
{
    int fd;
    NvError e = NvSuccess;
    char name[32];
    unsigned int i;

    NvOsSnprintf(name, sizeof(name), "/sys/class/gpio/%s",
                 export ? "export" : "unexport");
    fd = open(name, O_WRONLY);
    if (fd < 0) {
        NV_DEBUG_PRINTF(("failed to open %s (%s)\n", name, strerror(errno)));
        return NvError_AccessDenied;
    }

    for (i=0; i<pinCount; i++)
    {
        int gpio_nr = handle_to_nr(pin[i]);

        if (gpio_nr < 0 || gpio_nr >= (int)NV_ARRAY_SIZE(pin_data)) {
            e = NvError_BadValue;
            break;
        }

        if ((!export && pin_data[gpio_nr].fd < 0) ||
            (export && pin_data[gpio_nr].fd >= 0))
            continue;

        NvOsSnprintf(name, sizeof(name), "%d", gpio_nr);
        if (!export)
        {
            close(pin_data[gpio_nr].fd);
            pin_data[gpio_nr].fd = -1;
        }

        if (write(fd, name, NvOsStrlen(name)+1) < 0)
            NV_ASSERT(!"ExportGpios: write() failed");

        if (export)
        {
            NvOsSnprintf(name, sizeof(name),
                         "/sys/class/gpio/gpio%d/value", gpio_nr);
            pin_data[gpio_nr].fd = open(name, O_RDWR);
            if (pin_data[gpio_nr].fd < 0)
            {
                NV_DEBUG_PRINTF(("failed to open %s %s\n",
                                 name, strerror(errno)));
                e = NvError_AccessDenied;
                break;
            }
        }
    }

    close(fd);
    return e;
}

NvError NvRmGpioConfigPins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle *pins,
    NvU32 pinCount,
    NvRmGpioPinMode Mode)
{
    int fd;
    char name[40];
    NvU32 i;
    NvError e;

    const char *config_array[][2] =
    {
        [NvRmGpioPinMode_InputData] = { "in", "none" },
        [NvRmGpioPinMode_InputInterruptLow] = { "in", "falling" },
        [NvRmGpioPinMode_InputInterruptHigh] = { "in", "rising" },
        [NvRmGpioPinMode_InputInterruptRisingEdge] = { "in", "rising" },
        [NvRmGpioPinMode_InputInterruptFallingEdge] = { "in", "falling" },
        [NvRmGpioPinMode_InputInterruptAny] = { "in", "both" },
        [NvRmGpioPinMode_Output] = { "out", "none" },
    };

    if (!pins)
        return NvError_BadParameter;

    if (Mode==NvRmGpioPinMode_Function || Mode==NvRmGpioPinMode_Inactive)
        return ExportGpios(pins, pinCount, NV_FALSE);
    else
    {
        e = ExportGpios(pins, pinCount, NV_TRUE);
        if (e != NvSuccess)
            return e;
    }

    for (i=0; i<pinCount; i++)
    {
        int gpio_nr = handle_to_nr(pins[i]);
        struct gpio_data *pin;

        if (gpio_nr<0 || gpio_nr>=(int)NV_ARRAY_SIZE(pin_data))
            return NvError_BadParameter;

        pin = &pin_data[gpio_nr];
        NvOsSnprintf(name, sizeof(name),
                     "/sys/class/gpio/gpio%d/direction", gpio_nr);

        fd = open(name, O_WRONLY);
        if (fd < 0)
            return NvError_AccessDenied;

        if (Mode==NvRmGpioPinMode_Output && pin->value)
        {
            if (write(fd, "high", NvOsStrlen("high")) < 0)
                NV_ASSERT(!"NvRmGpioConfigPins: write() failed");
        }
        else
        {
            if (write(fd, config_array[Mode][0],
                      NvOsStrlen(config_array[Mode][0])+1) < 0)
                NV_ASSERT(!"NvRmGpioConfigPins: write() failed");
        }

        close(fd);
        NvOsSnprintf(name, sizeof(name),
                     "/sys/class/gpio/gpio%d/edge", gpio_nr);
        fd = open(name, O_WRONLY);
        if (fd < 0)
            return NvError_AccessDenied;
        if (write(fd, config_array[Mode][1],
                  NvOsStrlen(config_array[Mode][1])+1) < 0)
            NV_ASSERT(!"NvRmGpioConfigPins: write() failed");
        close(fd);
    }
    return NvSuccess;
}

void NvRmGpioReadPins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle *pin,
    NvRmGpioPinState *pPinState,
    NvU32 pinCount)
{
    NvU32 i;
    char buff[4];
    ssize_t count;

    for (i=0; i<pinCount; i++)
    {
        int gpio_nr = handle_to_nr(pin[i]);
        if (gpio_nr < 0 || gpio_nr >= (int)NV_ARRAY_SIZE(pin_data))
            continue;

        if (pin_data[gpio_nr].fd < 0)
            continue;

        count = read(pin_data[gpio_nr].fd, buff, sizeof(buff));
        if (count <= 0)
            continue;

        if (buff[0]=='1')
            pPinState[i] = NvRmGpioPinState_High;
        else
            pPinState[i] = NvRmGpioPinState_Low;
    }
}

void NvRmGpioWritePins(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle *pin,
    NvRmGpioPinState *pinState,
    NvU32 pinCount)
{
    NvU32 i;
    const char *value[2] = { "0", "1" };

    for (i=0; i<pinCount; i++)
    {
        int gpio_nr = handle_to_nr(pin[i]);
        if (gpio_nr<0 || gpio_nr>=(int)NV_ARRAY_SIZE(pin_data))
            continue;

        pin_data[gpio_nr].value = (pinState[i]==NvRmGpioPinState_High) ? 1 : 0;

        if (pin_data[gpio_nr].fd < 0)
            continue;

        if (write(pin_data[gpio_nr].fd, value[pin_data[gpio_nr].value], 2) < 0)
            NV_ASSERT(!"NvRmGpioWritePins: write() failed");
    }
}

void NvRmGpioReleasePinHandles(
    NvRmGpioHandle hGpio,
    NvRmGpioPinHandle *hPin,
    NvU32 pinCount)
{
    ExportGpios(hPin, pinCount, NV_FALSE);
}

NvError NvRmGpioAcquirePinHandle(
    NvRmGpioHandle hGpio,
    NvU32 port,
    NvU32 pin,
    NvRmGpioPinHandle *phPin)
{
    NvError e;
    NvRmGpioPinHandle h;
    int gpio_nr = (int)(port*8 + pin);

    if (!phPin)
        return NvError_BadParameter;

    if (gpio_nr<0 || gpio_nr>=(int)NV_ARRAY_SIZE(pin_data))
        return NvError_BadParameter;

    h = nr_to_handle(gpio_nr);
    e = ExportGpios(&h, 1, NV_TRUE);

    if (e == NvSuccess)
        *phPin = h;

    return e;
}

void NvRmGpioClose(NvRmGpioHandle hGpio)
{
}

NvError NvRmGpioOpen(NvRmDeviceHandle hRmDevice, NvRmGpioHandle *phGpio)
{
    static NvBool first = NV_TRUE;

    if (!phGpio)
        return NvError_BadParameter;

    if (first)
    {
        unsigned int i;
        for (i=0; i<NV_ARRAY_SIZE(pin_data); i++) {
            pin_data[i].value = 0;
            pin_data[i].fd = -1;
        }
        first = NV_FALSE;
    }

    *phGpio = (void*)0xdeadbeef;
    return NvSuccess;
}

NvError NvRmGpioInterruptRegister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioPinHandle hPin,
    NvOsInterruptHandler Callback,
    NvRmGpioPinMode Mode,
    void *CallbackArg,
    NvRmGpioInterruptHandle *hGpioInterrupt,
    NvU32 DebounceTime)
{
    *hGpioInterrupt = (NvRmGpioInterruptHandle)0x1;
    return NvSuccess;
}

NvError NvRmGpioInterruptEnable(NvRmGpioInterruptHandle hGpioInterrupt)
{
    return NvSuccess;
}

void NvRmGpioInterruptMask(NvRmGpioInterruptHandle hGpioInterrupt, NvBool mask)
{
}

void NvRmGpioInterruptUnregister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioInterruptHandle handle)
{
}

void NvRmGpioInterruptDone(NvRmGpioInterruptHandle h)
{
}
