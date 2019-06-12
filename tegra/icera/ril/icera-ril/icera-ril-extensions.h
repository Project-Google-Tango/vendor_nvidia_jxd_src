/* Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved. */

/*!
 * \brief This file implements the hooks necessary to add proprietary extensions
 * to our otherwise generic RIL.
 */

#ifndef ICERA_RIL_EXTENSIONS_H
#define ICERA_RIL_EXTENSIONS_H

#include <stdlib.h>
#include <stdbool.h>
#include <telephony/ril.h>
#include <ril_request_object.h>
#include "atchannel.h"

/* redefine function names as defined in ril_commands.h */
#define dispatchCallForward         ril_request_copy_call_forward
#define dispatchDial                ril_request_copy_dial
#define dispatchSIM_IO              ril_request_copy_sim_io
#define dispatchSmsWrite            ril_request_copy_sms_write
#define dispatchString              ril_request_copy_string
#define dispatchStrings             ril_request_copy_strings
#define dispatchRaw                 ril_request_copy_raw
#define dispatchInts                ril_request_copy_raw
#define dispatchVoid                ril_request_copy_void
#define dispatchGsmBrSmsCnf         ril_request_copy_gsm_br_sms_cnf
#define dispatchCdmaSms             ril_request_copy_dummy
#define dispatchCdmaSmsAck          ril_request_copy_dummy
#define dispatchCdmaBrSmsCnf        ril_request_copy_dummy
#define dispatchRilCdmaSmsWriteArgs ril_request_copy_dummy
#define dispatchDataCall            ril_request_copy_strings
#define responseSetupDataCall       ril_request_response_dummy
#define dispatchCdmaSubscriptionSource  ril_request_copy_dummy
#define dispatchVoiceRadioTech      ril_request_copy_dummy
/*from RIL V9*/
#define dispatchSetInitialAttachApn ril_request_copy_set_initial_attach_apn
#define dispatchImsSms              ril_request_copy_dummy
/*from RIL v10 (placeholder for the time being)*/
#define dispatchSIM_APDU            ril_request_copy_dummy

/*from RIL v10 (placeholder for the time being)*/
#define dispatchSIM_APDU            ril_request_copy_dummy
/* CDMA only: these requests shouldn't be received, stub for compilation */
#define dispatchNVReadItem          ril_request_copy_dummy
#define dispatchNVWriteItem         ril_request_copy_dummy

#define responseCallForwards        ril_request_response_dummy
#define responseCallList            ril_request_response_dummy
#define responseCellList            ril_request_response_dummy
#define responseContexts            ril_request_response_dummy
#define responseInts                ril_request_response_dummy
#define responseRaw                 ril_request_response_dummy
#define responseSIM_IO              ril_request_response_dummy
#define responseSMS                 ril_request_response_dummy
#define responseString              ril_request_response_dummy
#define responseStrings             ril_request_response_dummy
#define responseVoid                ril_request_response_dummy
#define responseSimStatus           ril_request_response_dummy
#define responseRilSignalStrength   ril_request_response_dummy
#define responseDataCallList        ril_request_response_dummy
#define responseGsmBrSmsCnf         ril_request_response_dummy
#define responseCdmaBrSmsCnf        ril_request_response_dummy
#define responseCellInfoList        ril_request_response_dummy
#define responseCallControl         ril_request_response_dummy

/*!
 * \brief Compare beginning of string with another string.
 * \param s             the string to compare
 * \param start         the expected beginning, MUST BE AN ARRAY OF CHARS
 * \return true if the beginning of the string matches, false otherwise
 */
#define STR_STARTS_WITH(s, start) \
        (strncmp((s), (start), sizeof(start) - 1) == 0)

/*!
 * \brief Convert request number to string.
 * \param request       request number
 * \return the string on success, NULL otherwise
 */
extern const char* rilExtRequestToString(
    int         request);

/*!
 * \brief Deal with proprietary request.
 * \param request       request number
 * \param data          binary data
 * \param datalen       binary data length
 * \param token
 * \return true if the request was dealt with, false otherwise
 */
extern bool rilExtOnRequest(
    int         request,
    void*       data,
    size_t      datalen,
    RIL_Token   token);

/*!
 * \brief Get copy function for given request.
 * \param request       request number
 * \return the function pointer on success, NULL otherwise
 */
extern ril_request_copy_func_t rilExtRequestGetCopyFunc(
    int         request);

/*!
 * \brief Get free function for given request.
 * \param request       request number
 * \return the function pointer on success, NULL otherwise
 */
extern ril_request_free_func_t rilExtRequestGetFreeFunc(
    int         request);

/*!
 * \brief Deal with proprietary unsolicited.
 * \param string        command string
 * \param sms_pdu       binary data
 * \return true if the unsolicited was dealt with, false otherwise
 */
extern bool rilExtOnUnsolicited (
    const char *string,
    const char *sms_pdu);

/*!
 * \brief Report that SIM and phonebook are loaded.
 * \param param         request number
 */
extern void rilExtUnsolPhonebookLoaded(
    const char* param);

extern void rilExtOnSimReady(void);

/** do post-AT+CFUN=1 initialization */
extern void rilExtOnRadioPowerOn(void);
/** do post- SIM ready initialization */
extern void rilExtOnSIMReady(void);
/** More generic RadioState changed event.
 *  Can handle all RadioState here if needed. */
extern void rilExtOnRadioStateChanged(RIL_RadioState newState);
extern void rilExtOnScreenState(int screenState);
/** channel management */
extern void rilExtInitializeCallback(void);
extern void rilExtInitializeCallbackEx(void);
extern void rilExtGetChannelRequestConf(int*** channel_list_config, int* size);
/** Network Registration */
extern void rilExtOnNetworkRegistrationError(AT_CME_Error cme_error_num);

/*!
 * \brief Hook called on radio power transitions,
 * \param int onOff 0 radio is going off, 1 radio is turning on
 * \return true if the default behaviour should take place, false if it shouldn't
 */
extern int rilExtOnRadioPower(int onOff);

/*!
 * \brief Hook called to allow customisation of the signal level filtering
 * \param  UnsolSignalStrengthExt true if AT%ICESQU is supported
 * \return true if the default behaviour should take place, false if it shouldn't
 */
int rilExtOnConfigureSignalLevelFiltering(int UnsolSignalStrengthExt);

#endif /* ICERA_RIL_EXTENSIONS_H */
