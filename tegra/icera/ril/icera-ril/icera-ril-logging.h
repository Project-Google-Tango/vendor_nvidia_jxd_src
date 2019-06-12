/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @file ficera-ril-logging.h RIL modem logging header.
 *
 */

#ifndef ICERA_RIL_LOGGING_H
#define ICERA_RIL_LOGGING_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
/* Different types for modem logging */
enum
{
    RAM_LOGGING,        /** oneshot modem logging session */
    DEFERRED_LOGGING,   /** continuous modem logging allowing modem hibernate */
    DISABLED_LOGGING    /** disabled logging */
};

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
/**
 * Indicate if modem logging was correctly initialised.
 */
int IsLoggingInitialised(void);

/**
 * Indicate if modem logging is "deferred logging" type.
 */
int IsDeferredLogging(void);

/**
 * Start logging daemon that will handle deferred modem logging.
 */
void StartDeferredLogging(void);

/**
 * Triggers modem logging reception.
 *
 * @param type to indicate type of logging required
 */
void SetModemLoggingEvent(LoggingType type, const char *msg, int len);

/**
 * Init modem logging env
 */
void InitModemLogging(void);

#endif /* ICERA_RIL_LOGGING_H */
