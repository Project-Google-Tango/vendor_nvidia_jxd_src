/* Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


#ifndef ICERA_RIL_NET_H
#define ICERA_RIL_NET_H 1

typedef enum {
    ANY = 0,
    EURO,
    US,
    JPN,
    AUS,
    AUS2,
    CELLULAR,
    PCS,
    BAND_CLASS_3,
    BAND_CLASS_4,
    BAND_CLASS_5,
    BAND_CLASS_6,
    BAND_CLASS_7,
    BAND_CLASS_8,
    BAND_CLASS_9,
    BAND_CLASS_10,
    BAND_CLASS_11,
    BAND_CLASS_15,
    BAND_CLASS_16,
    MAX_NUMBER_OF_BANDS
} AT_BandModes;

typedef struct {
    char *name;
    char *value;
} nameValuePair;

#define TECH_2G 1
#define TECH_3G 2
#define TECH_4G 4

#define ICERA_CELLID_TIMER_OFF  0
#define ICERA_CELLID_ONE_SHOT   1
#define ICERA_CELLID_TIMER_ON   2

typedef struct {
    int rat;          // 1, 2, 4
    int mcc;          // for all
    int mnc;          // for all
    int lac;          // only for wcdma & gsm
    int cid;          // for wcdma & gsm
    int psc;          // for wcdma
    unsigned int ci;  // for lte
    int pci;          // for lte
    int tac;          // for lte
    int registered;   // for all
}CellInfoBuf;

typedef struct {
    int signalStrength; /* Valid values are (0-31, 99) as defined in TS 27.007 8.5 */
    int bitErrorRate;   // only for wcdma & gsm
    // the following is for LTE.
    int rsrp;
    int rsrq;
    int timingAdvance;
    int cqi;
    int rssnr;
}signalStrengthBuf;

// Icera specific -- NITZ time
void unsolNetworkTimeReceived(const char *data);
void unsolSignalStrength(const char *s);
void unsolSignalStrengthExtended(const char *s);
void unsolRestrictedStateChanged(const char *s);
void requestSignalStrength(void *data, size_t datalen,
                                      RIL_Token t);
void requestSetBandMode(void *data, size_t datalen,
                                      RIL_Token t);
void requestQueryNetworkSelectionMode(void *data, size_t datalen,
                                      RIL_Token t);
void requestSetNetworkSelectionAutomatic(void *data, size_t datalen,
                                      RIL_Token t);
void requestSetNetworkSelectionManual(void *data, size_t datalen,
                                      RIL_Token t);
void requestQueryAvailableNetworks(void *data, size_t datalen,
                                   RIL_Token t);
void requestSetPreferredNetworkType(void *data, size_t datalen,
                                    RIL_Token t);
void requestGetPreferredNetworkType(void *data, size_t datalen,
                                    RIL_Token t);
void requestEnterNetworkDepersonalization(void *data, size_t datalen,
                                          RIL_Token t);
void requestRegistrationState(int request, void *data,
                              size_t datalen, RIL_Token t);
void requestNeighbouringCellIDs(void *data, size_t datalen, RIL_Token t);
void requestQueryAvailableBandMode(void *data, size_t datalen, RIL_Token t);
void requestSetLocationUpdates(void *data, size_t datalen, RIL_Token t);
void requestGetNeighboringCellIds(void *data, size_t datalen, RIL_Token t);
void requestOperator(void *data, size_t datalen, RIL_Token t);
void InitializeNetworkUnsols(int defaultLocUp);

void GetCurrentNetworkMode(int* mode, int* supportedRat);
int GetCurrentTechno(void);

void parseCreg(const char *s);
void parseCgreg(const char *s);
void parseCereg(const char *s);
void parseNwstate(const char *s);
void parseIcti(const char *s);
void InitNetworkCacheState(void);

int IsRegistered(void);

int GetLocupdateState(void);
int GetLteSupported(void);

int GetFDTimerEnabled(void);
void SetFDTimerEnabled(int enabled);

void requestVoiceRadioTech(void *data, size_t datalen, RIL_Token t);

void UnsolSignalStrengthInit(void);
void SetUnsolSignalStrength(int enabled);
const char* networkStatusToRilString(int state);
void setNetworkSelectionMode(int mode);

void parsePlusCxREG(char *line, int *p_RegState, int *p_Lac, int *p_CId, int *p_Rac);

typedef struct _regstate{
    int Reg;
    int Cid;
    int Lac;
    int Rac;
} regstate;

typedef struct _nwstate{
    int emergencyOrSearching;
    int mccMnc;
    char * rat;
    char * connState;
} nwstate;

enum regState {
    REG_STATE_NO_NETWORK = 0,
    REG_STATE_REGISTERED = 1,
    REG_STATE_SEARCHING  = 2,
    REG_STATE_REJECTED   = 3,
    REG_STATE_UNKNOWN    = 4,
    REG_STATE_ROAMING    = 5,
    REG_STATE_EMERGENCY  = 10,
    };

int accessTechToRat(char *rat, int request);
extern nwstate NwState;
extern regstate Creg, Cgreg, Cereg;
extern int CurrentTechnoCS, CurrentTechnoPS;
void requestGetCellInfoList(void *data, size_t datalen, RIL_Token t);
void requestSetUnsolCellInfoListRate(void *data, size_t datalen, RIL_Token t);
void unsolCellInfo(const char *s);
#endif
