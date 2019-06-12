/* Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved. */

#include <icera-ril-extensions.h>

const char* rilExtRequestToString(
    int         request)
{
    (void) request;
    return NULL;
}

bool rilExtOnRequest(
    int         request,
    void*       data,
    size_t      datalen,
    RIL_Token   token)
{
    (void) request;
    (void) data;
    (void) datalen;
    (void) token;
    return false;
}

ril_request_copy_func_t rilExtRequestGetCopyFunc(
    int         request)
{
    (void) request;
    return NULL;
}

ril_request_free_func_t rilExtRequestGetFreeFunc(
    int         request)
{
    (void) request;
    return NULL;
}

bool rilExtOnUnsolicited(
    const char* s,
    const char* sms_pdu)
{
    return false;
}

void rilExtUnsolPhonebookLoaded(const char* param)
{
    (void) param;
}

void rilExtOnSIMReady(void)
{
}

void rilExtOnRadioPowerOn(void)
{
}

void rilExtOnRadioStateChanged(RIL_RadioState newState)
{
    (void) newState;
}

/**
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
void rilExtInitializeCallback(void)
{
    /** For OEM: initialization for the default AT channel here if needed */
}

void rilExtInitializeCallbackEx(void)
{
    /** For OEM: initialization for additional AT channel here if needed */
}

void rilExtGetChannelRequestConf(
    int***      channel_list_config,
    int*        size)
{
    /* For OEM: Allocation of request per channel.
     * If this primitive returns NULL for channel_list_config then
     * the default configuration is used.
     *
     * All request not allocated on any channel will be handled on channel 0
     *
     * Typical implementation:
     *
     *  static int s_chan_01_ril_request_list[] =
     *  {
     *       RIL_REQUEST_QUERY_AVAILABLE_NETWORKS,
     *       RIL_REQUEST_OEM_HOOK_RAW,
     *
     *       ** TODO: add more RIl requests here **
     *       0   <--always terminate this list with zero
     *  };
     *
     *  static int s_chan_02_ril_request_list[] =
     *  {
     *      RIL_REQUEST_xxx,
     *      0
     *  };
     *
     *
     *  static int* s_chan_list[] =
     *  {
     *      NULL,  <-- default channel has no RIL request list
     *      s_chan_01_ril_request_list,
     *      s_chan_02_ril_request_list
     *  };
     *
     *  *channel_list_config = s_chan_list;
     *  *size = (sizeof(s_chan_list) / sizeof(int*));
     *
     */

    * channel_list_config = NULL;
    * size = 0;
}

void rilExtOnScreenState(
    int         screenState)
{
    if (screenState == 1) {
        /* For OEM: Screen is on - enable unsolicited notifications again. */

    }
    else {
        /* For OEM: Screen is off - disable unrequired unsolicited notifications. */
    }
}

void rilExtOnSimReady(void)
{
}

void rilExtOnNetworkRegistrationError(AT_CME_Error cme_error_num)
{
}

int rilExtOnRadioPower(int onOff) {
    /* <0 -> error
     * 0 -> Success, run regular handler
     * >0 -> Success, don't run regular handler
     */
    return 0;
}

int rilExtOnConfigureSignalLevelFiltering(int UnsolSignalStrengthExt) {
    /*
     * false -> Don't apply default filtering
     * true -> Apply default filtering
     */
    return true;
}
