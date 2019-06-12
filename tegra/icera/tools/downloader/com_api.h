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
  * @file com_api.h communication interfaces
  *
  * Supported interfaces:
  *  - serial COM port
  *  - TCP socket
  *
  */
#ifndef COM_API_H
#define COM_API_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#include <stdarg.h>

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
typedef unsigned long COM_Handle;

/* COM settings required to be persistent for a given COM_Handle */
typedef struct
{
    int flow_control; // COM port flow control enabled or disabled.
} COM_Settings;

/* Supported communication modes */
typedef enum
{
    COM_MODE_SERIAL
    ,COM_MODE_SOCKET
	,COM_MODE_DEVICE_SERVICE
} COM_Mode;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
/* Generic communication API */
COM_Handle COM_open(const char *dev_name);
int COM_updateSpeed(COM_Handle comport, int speed);
int COM_clearDtr(COM_Handle comport);
int COM_setCts(COM_Handle comport, int value);
int COM_close(COM_Handle comport);
int COM_write(COM_Handle comport, void *buf, int sz);
int COM_read(COM_Handle comport, void *buf, int sz);
int COM_poll(COM_Handle comport, void *buf, int sz, int timeout);
int COM_VerboseLevel(COM_Handle handle, int level);
int COM_mode(COM_Handle handle);
int COM_abort(COM_Handle comport);

/* Serial COM port interface */
COM_Handle com_Open(const char *dev_name);
int com_UpdateSpeed(COM_Handle comport, int speed);
int com_ClearDTR(COM_Handle comport);
int com_SetCTS(COM_Handle comport, int value);
int com_Close(COM_Handle comport);
int com_Write(COM_Handle comport, void *buf, int sz);
int com_Read(COM_Handle comport, void *buf, int sz);
int com_Poll(COM_Handle comport, void *buf, int sz, int timeout);

/* Socket interface */
COM_Handle sock_Open(char *addr, int port);
int sock_Close(COM_Handle comport);
int sock_Write(COM_Handle comport, void *buf, int sz);
int sock_Read(COM_Handle comport, void *buf, int sz);

#ifdef ENABLE_MBIM_DSS     /* MBIM DSS is only supported in OS >= Win8 */
/* MBIM Device Service interface */
COM_Handle device_service_Open(char *interfaceId);
int device_service_Close(COM_Handle comport);
int device_service_Write(COM_Handle comport, void *buf, int sz);
int device_service_Read(COM_Handle comport, void *buf, int sz);
int device_service_Abort(COM_Handle comport);
#endif /* ENABLE_MBIM_DSS */

/* AT cmd API */
int at_SendCmdAP(COM_Handle handle, const char *format, va_list ap);
int at_SendCmdAndWaitResponseAP(COM_Handle handle, const char *ok, const char *error, const char *format, va_list ap);
void at_SetDefaultCommandTimeout(COM_Handle handle);
void at_SetCommandTimeout(COM_Handle handle, long timeout);
int at_SendCmd(COM_Handle handle, const char *format, ...);
int at_SendCmdAndWaitResponse(COM_Handle handle, const char *ok, const char *error, const char *format, ...);
const char *at_GetResponse(COM_Handle handle);
void at_SetOutputCB(COM_Handle handle, void (*cb)(unsigned long arg, const char *format, ...), unsigned long cb_arg);

#endif /* #ifndef COM_API_H */

/** @} END OF FILE */
