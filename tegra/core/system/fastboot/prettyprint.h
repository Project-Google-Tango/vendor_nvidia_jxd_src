#ifndef PRETTYPRINT_H
#define PRETTYPRINT_H

#include "nvcommon.h"
#include "nvrm_surface.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct PrettyPrintContextRec
{
    NvU32 x;
    NvU32 y;
} PrettyPrintContext;

#define FASTBOOT_ERROR 1
#define FASTBOOT_STATUS 2
#define FASTBOOT_SELECTED_MENU 3

/**
 * Prints a message to display. 'format' follows the printf standard.
 */
void
PrettyPrintf( PrettyPrintContext *context, NvRmSurface *surf,
    NvU32 condition, const char *format, ... );

#ifdef __cplusplus
}
#endif

#endif
