/* Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved. */
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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "icera-ril.h"
#include "icera-ril-power.h"
#include "icera-util.h"
#include "atchannel.h"
#include "at_tok.h"

#define LOG_TAG rilLogTag
#include <utils/Log.h>
#include <cutils/properties.h>

/* maximum number of EDP states */
#define MAX_NUM_EDP_STATES (10)

/* global variable to track EDP initialization state */
int s_EDP_initialized = 0;

/* static functions */

/**
 * EDP state update
 *
 * @param state Index of state to move to
 * @return 0 on success or strictly negative value on error
 */
static int EDPSetState(int state)
{
    int err = -1;
    /* are we initialized yet? */
    if (s_EDP_initialized)
    {
        /* update state in EDP */
        err = IceraEDPSetState(state);
    }

    return err;
}

/* public functions */

void rilEDPInit(void)
{
    ATResponse *p_response = NULL;
    char *line;
    int param;
    ATLine *nextATLine = NULL;
    int err;
    int *pThresh=NULL;
    int *pMax=NULL;
    int nStates = 0;

    /* initialize EDP */
    if (IceraEDPInit() == 0)
    {
        /* disable unsollicited response */
        at_send_command("AT%IEDP=1,0",NULL);

        /*
        * get list of EDP states
        *
        * expected response is in the format: index,pThresh,pMax
        *
        * AT%IEDP=0
        * %IEDP: 0,5856,7665
        * %IEDP: 1,1640,7665
        * %IEDP: 2,700,7665
        * %IEDP: 3,5856,7665
        * %IEDP: 4,4953,7665
        * %IEDP: 5,3000,7665
        * %IEDP: 6,4676,7665
        * %IEDP: 7,1800,7665
        * %IEDP: 8,40,600
        * OK
        *
        * Note that the values are ordered from highest to lowest pMax.
        * On entering aeroplane mode, the modem requests the lowest mode.
        * Before asking the modem to exit aeroplane mode, we shall request the
        * highest mode, i.e. index = 0, then let the modem update when ready.
        */
        err = at_send_command_multiline ("AT%IEDP=0", "%IEDP:", &p_response);

        if (err != 0 || p_response->success == 0)
        {
            goto end;
        }
        else
        {
            /* allocate arrays of integers */
            pThresh = (int*)calloc(MAX_NUM_EDP_STATES,sizeof(*pThresh));
            pMax = (int*)calloc(MAX_NUM_EDP_STATES,sizeof(*pMax));

            if ((!pThresh) || (!pMax))
            {
                goto end;
            }

            /* parse the answer */
            nextATLine = p_response->p_intermediates;

            while (nextATLine != NULL)
            {
                line = nextATLine->line;

                /* skip prefix */
                err = at_tok_start(&line);
                if (err < 0)
                    break;

                /* first parameter is state ID */
                err = at_tok_nextint(&line,&param);
                if (err < 0)
                    break;

                /* make sure state ID is as expected */
                if (param != nStates)
                    break;

                /* first parameter is pThresh */
                err = at_tok_nextint(&line,&param);
                if (err < 0)
                    break;
                pThresh[nStates] = param;

                /* second parameter is pMax */
                err = at_tok_nextint(&line,&param);
                if (err < 0)
                    break;
                pMax[nStates] = param;

                nStates++;

                nextATLine = nextATLine->p_next;
            }

            if (nStates>0)
            {
                /* register to EDP */
                IceraEDPConfigureStates(pThresh, pMax, nStates);

                /* remember we're initialized */
                s_EDP_initialized = 1;

                /* set default state */
                EDPSetState(0);

                /* enable unsollicited response */
                at_send_command("AT%IEDP=1,1",NULL);
            }
        }
    }

end:
    /* clean up */
    at_response_free(p_response);
    free(pThresh);
    free(pMax);
}

int rilEDPSetFullPower(void)
{
    return EDPSetState(0);
}

void parseIedpu(const char *s)
{
    int err;
    int state;
    char *line = strdup((char*)s);
    char *p = line;

    /*
       expected format:
          %IEDPU: <stateID>
    */

    err = at_tok_start(&line);
    if (err < 0) goto end;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto end;

    /* set new state */
    err = EDPSetState(state);
    if (err < 0) goto end;

    /* acknowledge */
    at_send_command("AT%IEDP=2", NULL);

end:
    free(p);
}
