/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup downloader
 * @{
 */

/**
 * @file com_linux.c COM port functions for LINUX.
 *
 * A longer description of template.c here, which can be multiline.
 */

#if defined(__unix__) || defined(__MAC_OS__) || defined(ANDROID)

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "Globals.h"

#include <stdarg.h>
#include <string.h>

#include "com_api.h"
#include "com_handle.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/
#define MAX_RETRIES 3

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
static int baudrate[] = {
    50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,9600, 19200, 38400,
    57600, 115200, 230400,
#if !defined(__MAC_OS__)
    460800, 500000, 576000, 921600, 1000000, 1152000, 1500000,
    2000000, 2500000, 3000000, 3500000, 4000000,
#endif
    -1
};
static int baudcode[] = {
    B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600, B19200, B38400,
    B57600, B115200, B230400,
#if !defined(__MAC_OS__)
    B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000,
    B2000000, B2500000, B3000000, B3500000, B4000000
#endif
};

static int rate_to_code(int baud)
{
    int i;
    for(i = 0; baudrate[i] > 0; i++)
        if (baudrate[i] == baud)
            return baudcode[i];
    return -1;
}

static int serial_setbaud(COM_Handle handle, int rate)
{
    struct termios buf;
    int rate_flag = 0;

    // Get the current settings for the serial port.
    if(tcgetattr(HDATA(handle).com_hdl, &buf))
    {
        PRINT(handle, (stderr, "tcgetattr failed\n"));
        return AT_ERR_COM_SPEED;
    }

    // Get the baud rate.
    rate_flag = rate_to_code(rate);
    if (rate_flag >= 0)
    {
        cfsetispeed(&buf, rate_flag);
        cfsetospeed(&buf, rate_flag);
    }
    else
    {
        PRINT(handle, (stderr, "Baud %d not supported", rate));
        return AT_ERR_COM_SPEED;
    }

    // Set the new settings for the serial port.
    if(tcsetattr(HDATA(handle).com_hdl, TCSADRAIN, &buf) < 0)
    {
        PRINT(handle, (stderr, "tcsetattr failed (%d)\n", errno));
        return AT_ERR_COM_SPEED;
    }

    // Wait until the output buffer is empty.
    tcflush(HDATA(handle).com_hdl, TCIOFLUSH);
    return AT_ERR_SUCCESS;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int com_Close(COM_Handle handle)
{
    if (handle)
    {
        if (HDATA(handle).com_hdl != INVALID_HANDLE)
        {
#ifndef ANDROID
            if (tcdrain(HDATA(handle).com_hdl) == -1)
            {
                PRINT(handle, (stderr, "Error waiting for drain - %s(%d).\n", strerror(errno), errno));
            }
#endif
            close(HDATA(handle).com_hdl);
            HDATA(handle).com_hdl = INVALID_HANDLE;
        }
        COM_FreeHandle(handle);
    }
    return AT_ERR_SUCCESS;
}

COM_Handle com_Open(const char *dev_name)
{
    COM_Handle handle = COM_CreateHandle();
    if (handle)
    {
        int com_hdl = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
        HDATA(handle).com_hdl = INVALID_HANDLE;
        while (com_hdl > INVALID_HANDLE)
        {
            fcntl(com_hdl, F_SETFL, 0);
            struct termios options;
            memset(&options, 0, sizeof(options));
            /* Raw mode */
            options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
            options.c_oflag &= ~OPOST;
            options.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
            options.c_cflag &= ~(CSIZE|PARENB);
            options.c_cflag |= CS8 | CLOCAL | CREAD;
            cfsetispeed(&options, rate_to_code(115200));
            cfsetospeed(&options, rate_to_code(115200));
            /* Immediat return */
            options.c_cc[VTIME]    = 0; /* x100ms */
            options.c_cc[VMIN]     = 0;
            tcflush(com_hdl, TCIOFLUSH);
            tcsetattr(com_hdl, TCSANOW, &options);
            HDATA(handle).com_hdl = com_hdl;
            break;
        }
        if ((HDATA(handle).com_hdl) == -1 && (com_hdl > INVALID_HANDLE))
        {
            PRINT(handle, (stderr, "Error opening serial port %s - %s(%d).\n", dev_name, strerror(errno), errno));
            close(com_hdl);
            COM_FreeHandle(handle);
            handle = 0;
        }
    }
    return handle;
}

int com_UpdateSpeed(COM_Handle handle, int speed)
{
    return serial_setbaud(handle, speed);
}

int com_SetCTS(COM_Handle handle, int value)
{
    return AT_ERR_NOTSUPPORTED;
}

int com_ClearDTR(COM_Handle handle)
{
    return AT_ERR_NOTSUPPORTED;
}

int com_Write(COM_Handle handle, void *buffer, int length)
{
    int nb_wrote = -1, tries;
    if (HDATA(handle).com_hdl >= 0)
    {
        nb_wrote = 0;
        for (tries = 0 ; tries < MAX_RETRIES ; tries++)
        {
            nb_wrote += write(HDATA(handle).com_hdl, &((char *)buffer)[nb_wrote], length - nb_wrote);
            if (nb_wrote == length)
            {
#ifndef ANDROID
                tcdrain(HDATA(handle).com_hdl); /* Block until write is really done */
#endif
                break;
            }
            if (nb_wrote < 0)
            {
                PRINT(handle, (stderr, "Error writing - %s(%d).\n", strerror(errno), errno));
                break;
            }
        }
    }
    return nb_wrote;
}

int com_Read(COM_Handle handle, void *buffer, int length)
{
    int nb_read = -1;
    if (HDATA(handle).com_hdl >= 0)
    {
        nb_read = read(HDATA(handle).com_hdl, buffer, length);
        if (nb_read < 0)
        {
            PRINT(handle, (stderr, "Error reading - %s(%d).\n", strerror(errno), errno));
        }
        else
        {
            DUMP(handle, stderr, buffer, nb_read);
        }
    }
    return nb_read;
}

int com_Poll(COM_Handle handle, void *buffer, int length, int timeout)
{
    fd_set set;
    struct timeval t;
    int res, nb_read = -1;

    FD_ZERO(&set);
    FD_SET(HDATA(handle).com_hdl, &set);

    t.tv_usec = timeout * 1000;
    t.tv_sec = t.tv_usec / 1000000;
    t.tv_usec = t.tv_usec % 1000000;

    res = select(HDATA(handle).com_hdl+1, &set, NULL, NULL, (timeout == -1) ? NULL:&t);

    if (res == 0)
    {
        PRINT(handle, (stderr, "Error read timeout.\n"));
    }
    else if (res == -1)
    {
        PRINT(handle, (stderr, "Select error. %s (%d).\n", strerror(errno), errno));
    }
    return res;
}

#endif  /* _unix_ */

/** @} END OF FILE */
