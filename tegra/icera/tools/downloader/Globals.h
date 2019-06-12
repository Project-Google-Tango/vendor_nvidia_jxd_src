/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup updater
 * @{
 */

 /**
  * @file globals.h The global definitions for Updater dll
  *
  */

#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef VISUAL_EXPORTS

#define snprintf _snprintf
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)

#endif

#ifdef _WIN32
#ifndef WINVER
#define WINVER 0x0500 /* From Win2K (included) */
#endif
/* Windows */
#include <windows.h>
#ifdef ICERA_EXPORTS
    #define ICERA_API __declspec(dllexport)
#else
    #define ICERA_API
#endif
#define DEFAULT_DEV             "COM1"

#define DEVCLASS_MODEM_GUID		{ 0x2c7089aaL, 0x2e0e, 0x11d1, { 0xb1, 0x14, 0x00, 0xc0, 0x4f, 0xc2, 0xaa, 0xe4 } }
#define DEVCLASS_COMPORT_GUID	{ 0x86e0d1e0L, 0x8089, 0x11d0, { 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73 } }
#define CLASS_MODEM_GUID        { 0x4D36E96DL, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } }
#define CLASS_COMPORT_GUID      { 0x4D36E978L, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } }

#define REG_ENUM            "SYSTEM\\CurrentControlSet\\Enum"
#define REG_SERVICES        "SYSTEM\\CurrentControlSet\\Services"
#define REG_CTRL_CLASS      "SYSTEM\\CurrentControlSet\\Control\\Class"
#define REG_CTRL_DEVCLASS   "SYSTEM\\CurrentControlSet\\Control\\DeviceClasses"

#else /* _WIN32 */
#if defined(__MAC_OS__)

/* MacOs */
#define ICERA_API
#define DEFAULT_DEV             "/dev/tty.usbmodem0.0.12"

#else /* __MAC_OS__ */

/* Linux */
#define ICERA_API
#define DEFAULT_DEV             "/dev/ttyACM1"

#endif /* __MAC_OS__ */
#endif /* _WIN32 */

#define DEFAULT_SPEED       115200

#define DLD_VERSION         "12.10"

#define DEFAULT_DELAY       5

#define MAX_BLOCK_REJECT    3
#define MIN_BLOCK_SZ        1
#define MAX_BLOCK_SZ        65532
#define DEFAULT_BLOCK_SZ    16384 /* Higher value trigg the watchdog */
#define DEFAULT_SEND_NVCLEAN true
#define DEFAULT_CHECK_PKGV   true

#ifdef ENABLE_MBIM_DSS
#define DEFAULT_UPDATE_IN_MODEM true
#else
#define DEFAULT_UPDATE_IN_MODEM false
#endif

#define EOS                 '\x00'

#define FILE_SEPARATOR      ";"

#define SHA1_STR_BYTES_SIZE 40 /* 40hexits for a SHA1 string */

#define AT_CMD_MODE         "AT%%MODE?\r\n"
#define AT_RESPONSE         ": "
#define AT_CMD_MODEx        "AT%%%%MODE=%d\r\n"

#define AT_CMD_MODE0        "AT%%MODE=0\r\n"
#define AT_CMD_MODE1        "AT%%MODE=1\r\n"
#define AT_CMD_MODE2        "AT%%MODE=2\r\n"
#define AT_CMD_MODE3        "AT%%MODE=3\r\n"
#define AT_CMD_IPR          "AT+IPR=%d\r\n"
#define AT_CMD_LOAD         "AT%%LOAD\r\n"
#define AT_CMD_GMR          "AT+GMR\r\n"
#define AT_CMD_ECHO_OFF     "ATE0\r\n"
#define AT_CMD_ECHO_ON      "ATE1\r\n"
#define AT_CMD_CHIPID       "AT%%CHIPID\r\n"
#define AT_CMD_PROG         "AT%%PROG=%d\r\n"
#define AT_CMD_IFLIST       "AT%%IFLIST\r\n"
#define AT_CMD_IFLIST_1     "AT%%IFLIST=1\r\n"
#define AT_CMD_IFLIST_2     "AT%%IFLIST=2\r\n"
#define AT_CMD_IFWR         "AT%%IFWR\r\n"
#define AT_CMD_ICOMPAT      "AT%%ICOMPAT\r\n"
#define AT_CMD_NVCLEAN      "AT%%NVCLEAN\r\n"
#define AT_CMD_IBACKUP      "AT%%IBACKUP\r\n"
#define AT_CMD_IRESTORE     "AT%%IRESTORE\r\n"


#define AT_RSP_OK           "\r\nOK\r\n"
#define AT_RSP_ERROR        "\r\nERROR\r\n"

#define VERBOSE_GET_DATA        -1
#define VERBOSE_SILENT_LEVEL    0
#define VERBOSE_INFO_LEVEL      1
#define VERBOSE_STD_ERR_LEVEL   2
#define VERBOSE_DUMP_LEVEL      3

#define VERBOSE_DEFAULT_LEVEL   VERBOSE_SILENT_LEVEL

#define MODE_INVALID        -1

#define MODE_MODEM          0
#define MODE_LOADER         1
#define MODE_FACTORY_TEST   2
#define MODE_MASS_STORAGE   3
#define MODE_MAX            4

#define MODE_DEFAULT        MODE_MODEM

#define AT_ERR_SUCCESS      1
#define AT_ERR_FAIL         0
#define AT_ERR_TIMEOUT      -1
#define AT_ERR_IO_WRITE     -2
#define AT_ERR_COM_SPEED    -3
#define AT_ERR_SIGNALS      -4
#define AT_ERR_NOTSUPPORTED -5
#define AT_ERR_CTS          -6

#define AT_SUCCESS(R)       ((R) == AT_ERR_SUCCESS)
#define AT_FAIL(R)          ((R) < AT_ERR_SUCCESS)
#define AT_WARN(R)          ((R) > AT_ERR_SUCCESS)

#define MAX(A, B)           (((A) > (B)) ? (A) : (B))
#define ARRAY_SIZE(A)       (sizeof(A) / sizeof((A)[0]))

#endif /* GLOBALS_H */
/** @} END OF FILE */
