/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc. All rights reserved.
*/

/**
* @{
*/

/**
* @file at_channel_config.h The interface of the AT channel configuration module
*
*/
#ifndef __AT_CHANNEL_CONFIG_H_
#define __AT_CHANNEL_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*************************************************************************************************
* Macros
************************************************************************************************/

/** maximum number of charakters per line in the configuration file */
#define ATCC_MAX_LINE_LEN 64

/*************************************************************************************************
* Public Types
************************************************************************************************/

typedef unsigned char at_channel_id_t;  /**< zero based index of the AT channels. */

/*************************************************************************************************
* Public function declarations
************************************************************************************************/

/**
 * This function opens the configuration file, reads all information and
 *  closes the file afterwards. Can be called again at runtime if the
 *  configuration data has been changed
 *
 * @param config_file [IN] zero-terminated string, containing the /path/to/config/file
 *
 * @return non-zero if successfull, zero if an error occured
 */
int at_channel_config_init( const char* config_file );

/**
 * Free all resources allocated for the configuration maintenance
 */
void at_channel_config_exit( void );

/**
 * This function returns the number of _additional_ AT channels.
 *
 * @return Returns the number of addiotnal AT channels. If zero is returned,
 *      only the default channel has to be used.
 */
at_channel_id_t at_channel_config_get_num_channels( void );

/**
 * This function retrieves the zero based index of at_channel, on which a RIL
 * request has to be handled. Note that this funciton has no knowledge about
 * the status of the different channels. This means, even if an error occured
 * while opening the second, third etc channel, it will return it's channel
 * ID for the RIL request and the outer layers have to handle it.
 *
 * @param ril_request [IN] RIL Request ID for AT channel lookup
 *
 * @return valid AT channel id if successfull, zero otherwise. Zero would be
 *     the id of the default channel, which is not handled by this configuration
 *     component, but always allows all RIL request to pass.
 */
at_channel_id_t at_channel_config_get_channel_for_request( const int ril_request );

/**
 * This function retrieves the device name of the AT channel with the given ID.
 *
 * @param channel_id [IN] AT Channel ID for which the device name is returned
 *
 * @return zero terminated containing the valid device name if successfull, NULL otherwise
 */
const char* at_channel_config_get_device_name( const at_channel_id_t channel_id );

/**
 * This function retrieves the AT command timeout of the AT channel with the given ID.
 *
 * @param channel_id [IN] AT Channel ID for which the timeout is returned
 *
 * @return nonzero timeout value, or zero (blocking forever/no timeout)
 */
long long at_channel_config_get_at_cmd_timeout( const at_channel_id_t channel_id );

/**
 * This function retrieves the default AT command timeout of the AT channel with the given ID.
 *
 * @param channel_id [IN] AT Channel ID for which the default timeout is returned
 *
 * @return nonzero default timeout value, or zero (blocking forever/no timeout)
 */
long long at_channel_config_get_default_at_cmd_timeout( const at_channel_id_t channel_id );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AT_CHANNEL_CONFIG_H_ */

/** @} END OF FILE */
