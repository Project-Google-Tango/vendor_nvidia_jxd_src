/*************************************************************************************************
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_transport.h Obex transport layer
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "obex.h"
#include "obex_transport.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

typedef struct
{
    bool initialised;
    obex_TransportFuncs funcs;
} obex_Transport;


/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

DXP_CACHED_UNI1 obex_Transport obex_transport = {
    false,                                       /* initially not initialised */
    {NULL,NULL,NULL,NULL}                        /* initial functional interface */
};

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

obex_ResCode obex_TransportRegister(const obex_TransportFuncs *funcs)
{
    /* sanity checks */
    if ((funcs == NULL) ||
        (funcs->read == NULL) ||
        (funcs->write == NULL))
    {
        ALOGE("%s: Invalid transport funcs", __FUNCTION__);
        return OBEX_ERR;
    }

    /* save Obex Transport functional interface */
    memcpy(&obex_transport.funcs, funcs, sizeof(obex_TransportFuncs) );

    /* remember that we are initialised */
    obex_transport.initialised = true;

    return OBEX_OK;
}

obex_ResCode obex_TransportOpen(void)
{
    obex_ResCode res = OBEX_NOT_READY;
    if ((obex_transport.initialised) && (obex_transport.funcs.open!=NULL))
    {
        res = obex_transport.funcs.open();
    }
    return res;
}

obex_ResCode obex_TransportClose(void)
{
    obex_ResCode res = OBEX_NOT_READY;
    if ((obex_transport.initialised) && (obex_transport.funcs.close!=NULL))
    {
        obex_transport.funcs.close();
        res = OBEX_OK;
    }
    return res;
}

obex_ResCode obex_TransportWrite(const char *buffer, int count)
{
    obex_ResCode res = OBEX_NOT_READY;

    /* sanity checks */
    if ((obex_transport.initialised) && (obex_transport.funcs.write!=NULL))
    {
        res = obex_transport.funcs.write(buffer, count);
    }

    return res;
}

obex_ResCode obex_TransportRead(char *buffer, int count)
{
    obex_ResCode res = OBEX_NOT_READY;

    /* sanity checks */
    if ((obex_transport.initialised) && (obex_transport.funcs.read!=NULL))
    {
        res = obex_transport.funcs.read(buffer, count);
    }

    return res;
}

/** @} END OF FILE */
