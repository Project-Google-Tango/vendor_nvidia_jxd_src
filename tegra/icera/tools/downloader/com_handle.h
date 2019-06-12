/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

 /**
  * @defgroup downloader.
  *
  * @{
  */

 /**
  * @file com_handle.h Shared datastructure between com and at parts of this library
  *
  */

#ifndef COM_HANDLE_H
#define COM_HANDLE_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/
#include "Globals.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Constants
 ************************************************************************************************/
#ifdef _WIN32
#define INVALID_HANDLE      INVALID_HANDLE_VALUE
#else
#if defined(__unix__) || defined(__MAC_OS__) || defined(ANDROID)
#define INVALID_HANDLE      -1
#endif
#endif

#define COM_MAX_HANDLES         10
#define AT_DEFAULT_TIMEOUT      5   /* seconds */
#define TX_BUFFER_SIZE          0x10000
#define RX_BUFFER_SIZE          0x10000

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
#define HDATA(_index)       com_handle_array[(_index) - 1]

#define PRINT(H, M)         if (HDATA(H).verbose_level >= VERBOSE_STD_ERR_LEVEL) fprintf M

#define DUMP(H, O, B, S)    int _DUMP_i; if (HDATA(H).verbose_level >= VERBOSE_DUMP_LEVEL)                            \
                                for (_DUMP_i = 0 ; _DUMP_i < (S) ; _DUMP_i++)                           \
                                    fprintf((O), "[%d] 0x%02x '%c'\n", _DUMP_i, ((char*)(B))[_DUMP_i],      \
                                            isalnum(((char*)(B))[_DUMP_i]) ? ((char*)(B))[_DUMP_i] : '.');


/*************************************************************************************************
 * Private Types
 ************************************************************************************************/
/* Operations available through communication interface */
typedef struct
{
    int (*close)(COM_Handle handle);
    int (*write)(COM_Handle handle, void *buf, int sz);
    int (*read)(COM_Handle handle, void *buf, int sz);
    int (*poll)(COM_Handle handle, void *buf, int sz, int timeout);
    int (*update_speed)(COM_Handle handle, int speed);
    int (*clear_dtr)(COM_Handle handle);
    int (*set_cts)(COM_Handle handle, int value);
    int (*abort)(COM_Handle handle);
} COM_operations;

typedef struct COM_Handle_tag
{
#ifdef _WIN32
    HANDLE com_hdl;
    char bufferError[80];
#else
#if defined(__unix__) || defined(__MAC_OS__) || defined(ANDROID)
    int com_hdl;
#endif
#endif

    /* I/O buffers AT commands */
    char tx_buf[TX_BUFFER_SIZE];
    char rx_buf[RX_BUFFER_SIZE];
    int  at_cmd_timeout;

    /* Output call back */
    void (*at_OutputCB)(unsigned long arg, const char *format, ...);
    unsigned long at_OutputCBArg;

    int     verbose_level;
    unsigned int used;

    /* COM operations */
    COM_operations *ops;

    /* COM mode */
    COM_Mode mode;
} COM_Handle_s;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/
extern COM_Handle_s com_handle_array[COM_MAX_HANDLES];

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
COM_Handle COM_CreateHandle(void);
void COM_FreeHandle(COM_Handle handle);

#endif

/** @} END OF FILE */
