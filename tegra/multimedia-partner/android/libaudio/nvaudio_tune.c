/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "nvaudio_tune"
//#define LOG_NDEBUG 0

#define DUAL_PATH_GAIN 1

#include <tinyalsa/asoundlib.h>
#include "nvaudio_tune.h"

#define ALLOC_DEVICE_GAIN(g,d) \
do {                            \
    g = malloc(sizeof(struct device_gain));\
    if (!g) {    \
        ALOGE("Failed to allocate device_gain");\
        break;    \
    }    \
    g->ctls = list_create();\
    g->device = d;\
} while (0)

/*****************************************************************************
 * Declaration of extern functions
 *****************************************************************************/
/*****************************************************************************
 * Forward Declaration of static functions
 *****************************************************************************/
/*****************************************************************************
 * XML config file parsing
 *****************************************************************************/
static void config_parse_start(void *data, const XML_Char *elem,
                              const XML_Char **attr)
{
    struct config_tune_parse_state *state = data;
    struct tune_mode_gain *tune = state->tune;
    uint32_t i;

    ALOGV("%s: elem %s", __FUNCTION__, elem);

    if (!strcmp(elem, "normal")) {
        state->cur_mode    = TUNE_MODE_NORMAL;
        tune->gains[TUNE_MODE_NORMAL] = list_create();
    }
    else if (!strcmp(elem, "voip")) {
        state->cur_mode    = TUNE_MODE_VOIP;
        tune->gains[TUNE_MODE_VOIP] = list_create();
    }
    else if (!strcmp(elem, "vr")) {
        state->cur_mode    = TUNE_MODE_VR;
        tune->gains[TUNE_MODE_VR] = list_create();
    }
    else if (!strcmp(elem, "out_speaker"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_OUT_SPEAKER);
    else if (!strcmp(elem, "out_headset"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_OUT_HEADSET);
    else if (!strcmp(elem, "out_headphone"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_OUT_HEADPHONE);
    else if (!strcmp(elem, "out_spk_hp"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_OUT_SPK_HP);
    else if (!strcmp(elem, "in_main"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_IN_MAIN);
    else if (!strcmp(elem, "in_headset"))
        ALLOC_DEVICE_GAIN(state->cur_dev, TUNE_IN_HEADSET);
    else if (!strcmp(elem, "ctl")) {
        struct gain_ctl *ctl;
        const XML_Char *name = NULL, *val = NULL;

        if(!state->cur_dev) {
            ALOGE("ctl for invalid device");
            return;
        }
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];

            if (!strcmp(attr[i], "val"))
                val = attr[i + 1];
        }

        if (!name) {
            ALOGE("Unnamed control\n");
            return;
        }

        if (!val) {
            ALOGE("No value specified for %s\n", name);
            return;
        }

        ALOGV("Parsing control %s => %s\n", name, val);

        ctl = malloc(sizeof(struct gain_ctl));
        if (!ctl) {
            ALOGE("Out of memory handling %s => %s\n", name, val);
            return;
        }
        memset(ctl, 0, sizeof(*ctl));

        ctl->ctl_name = strdup(name);
        ctl->str_val = NULL;

        /* This can be fooled but it'll do */
        if (val[0] == '0' && (val[1] == 'X' || val[1] == 'x')) {
            unsigned int hex_val = 0;

            for (i = 2; i < strlen(val); i++) {
                char ch = val[i];

                hex_val <<= 4;
                if (ch >= '0' && ch <='9')
                    hex_val += ((unsigned int)ch - '0');
                else if (ch >= 'a' && ch <= 'f')
                    hex_val += 10 + ((unsigned int)ch - 'a');
                else if (ch >= 'A' && ch <= 'F')
                    hex_val += 10 + ((unsigned int)ch - 'A');
                else
                    break;
            }
            ctl->int_val = hex_val;
        }
        else {
            for (i = 0; i < strlen(val); i++) {
                char ch = val[i];

                if (ch < '0' || ch > '9') {
                    ctl->str_val = strdup(val);
                    break;
                }
            }
            if (!ctl->str_val)
                ctl->int_val = atoi(val);
        }
        if (state->cur_dev)
            list_push_data(state->cur_dev->ctls, ctl);
    }
}

static void config_parse_end(void *data, const XML_Char *name)
{
    struct config_tune_parse_state *state = data;
    struct tune_mode_gain *tune = state->tune;

    ALOGV("%s: elem %s",__FUNCTION__, name);

    if (!strcmp(name, "normal") ||
        !strcmp(name, "voip") ||
        !strcmp(name, "vr")) {
        /* do nothing for now */
    }
    else if (!strcmp(name, "out_speaker") ||
                !strcmp(name, "out_headset") ||
                !strcmp(name, "out_headphone") ||
                !strcmp(name, "out_spk_hp") ||
                !strcmp(name, "in_main") ||
                !strcmp(name, "in_headset")) {
        /* add device gains */
        if (state->cur_dev && tune->gains[state->cur_mode]) {
            list_push_data(tune->gains[state->cur_mode],
                            state->cur_dev);
            state->cur_dev = NULL;
        }
    }
}

int config_tune_parse(struct tune_mode_gain *tune)
{
    struct config_tune_parse_state state;
    int fd;
    XML_Parser parser;
    char data[80];
    int ret = 0;
    bool eof = false;
    int len;

    ALOGV("%s", __FUNCTION__);

    fd = open(XML_TUNE_CONFIG_FILE_PATH_LBH, O_RDONLY);
    if (fd <= 0) {
        ALOGI("Failed to open %s, trying %s\n", XML_TUNE_CONFIG_FILE_PATH_LBH,
                                                XML_TUNE_CONFIG_FILE_PATH);
        fd = open(XML_TUNE_CONFIG_FILE_PATH, O_RDONLY);
        if (fd <= 0) {
            ALOGE("Failed to open %s\n", XML_TUNE_CONFIG_FILE_PATH);
            return -ENODEV;
        }
    }

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        ALOGE("Failed to create XML parser\n");
        ret = -ENOMEM;
        goto err_exit;
    }

    memset(&state, 0, sizeof(state));
    tune->in_source = AUDIO_SOURCE_DEFAULT;
    state.tune = tune;
    XML_SetUserData(parser, &state);

    XML_SetElementHandler(parser, config_parse_start, config_parse_end);

    while (!eof) {
        len = read(fd, data, sizeof(data));
        if (len < 0) {
            ALOGE("I/O error reading config\n");
            ret = -EIO;
            goto err_parser;
        } else if (!len) {
           eof = 1;
        }

        if (XML_Parse(parser, data, len, eof) == XML_STATUS_ERROR) {
            ALOGE("Parse error at line %u:\n%s\n",
                 (unsigned int)XML_GetCurrentLineNumber(parser),
                 XML_ErrorString(XML_GetErrorCode(parser)));
            ret = -EINVAL;
            goto err_parser;
        }
    }

 err_parser:
    XML_ParserFree(parser);
 err_exit:
    close(fd);

    return ret;
}

void config_tune_dump(struct tune_mode_gain *tune)
{
    struct device_gain *gain;
    struct gain_ctl *ctl;
    node_handle n, m;
    int i;

    if (!tune) {
        ALOGE("tune is NULL");
        return;
    }
    for (i=0;i < TUNE_MODE_MAX;i++) {
        if (tune->gains[i]) {
            ALOGV("Tune mode: %d", i);
            n = list_get_head(tune->gains[i]);
            while (n) {
                gain = list_get_node_data(n);
                ALOGV("\tdevice: 0x%x", gain->device);
                m = list_get_head(gain->ctls);
                while (m) {
                    ctl = list_get_node_data(m);
                    ALOGV("\t\tctl: %s, str_val: %s, int_val: %d",
                        ctl->ctl_name, ctl->str_val, ctl->int_val);
                    m = list_get_next_node(m);
                }
                n = list_get_next_node(n);
            }
        }
    }
}

/*****************************************************************************
 * apply tuning gain
 *****************************************************************************/
static int set_gain_by_array(struct mixer *mixer, list_handle gain_list)
{
    struct mixer_ctl *ctl;
    uint32_t i, j, ret;
    node_handle n = list_get_head(gain_list);

    ALOGV("%s", __FUNCTION__);

    /* Go through the gain array and set each value */
    while (n) {
        struct gain_ctl* g = list_get_node_data(n);
        ctl = mixer_get_ctl_by_name(mixer, g->ctl_name);
        if (!ctl) {
            ALOGE("Unknown control '%s'\n", g->ctl_name);
            return -EINVAL;
        }

        if (g->str_val) {
            ret = mixer_ctl_set_enum_by_string(ctl, g->str_val);
            if (ret != 0) {
                ALOGE("Failed to set '%s' to '%s'\n",g->ctl_name, g->str_val);
            } else {
                ALOGV("Set '%s' to '%s'", g->ctl_name, g->str_val);
            }
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                ret = mixer_ctl_set_value(ctl, j, g->int_val);
                if (ret != 0)
                    ALOGE("Failed to set '%s'.%d to %d\n",
                         g->ctl_name, j, g->int_val);
                else
                    ALOGV("Set '%s' to '%s'", g->ctl_name, g->str_val);
            }
        }
        n = list_get_next_node(n);
    }

    return 0;
}

void set_tune_gain(struct tune_mode_gain *tune,
                   struct mixer *mixer, int mode, int devices)
{
    struct nvaudio_stream_in *in;
    enum tune_mode tmode;
    enum tune_device tdev;
    node_handle n, m;

    ALOGV("%s", __func__);

    if( devices & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_DEFAULT))
        tdev = TUNE_IN_MAIN;
    else if( devices & AUDIO_DEVICE_IN_WIRED_HEADSET )
        tdev = TUNE_IN_HEADSET;
    else if( devices & AUDIO_DEVICE_OUT_SPEAKER ){
#ifdef DUAL_PATH_GAIN
        if (devices & (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE))
            tdev = TUNE_OUT_SPK_HP;
        else
            tdev = TUNE_OUT_SPEAKER;
#else
            tdev = TUNE_OUT_SPEAKER;
#endif
    }
    else if (devices & AUDIO_DEVICE_OUT_WIRED_HEADSET )
        tdev = TUNE_OUT_HEADSET;
    else if ( devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE )
        tdev = TUNE_OUT_HEADPHONE;
    else{
        ALOGV("not supported device 0x%x", devices);
        return;
    }

    switch (mode){
        case AUDIO_MODE_NORMAL:
            if (tune->in_source == AUDIO_SOURCE_VOICE_RECOGNITION)
                tmode = TUNE_MODE_VR;
            else
                tmode = TUNE_MODE_NORMAL;
            break;
        case AUDIO_MODE_IN_COMMUNICATION:
            tmode = TUNE_MODE_VOIP;
            break;
        default:
            ALOGV("not supported mode 0x%x", mode);
            return;
    }
    ALOGV("tune mode: %d, device: 0x%x", tmode, tdev);

    /* If we have gain table for this mode */
    if (tune->gains[tmode]) {
        n = list_get_head(tune->gains[tmode]);
        while (n) {
            struct device_gain *dgain =
                (struct device_gain *)list_get_node_data(n);
            if (dgain && (tdev & dgain->device))
                set_gain_by_array(mixer, dgain->ctls);
            n = list_get_next_node(n);
        }
    }

    return;
}

void close_tune(struct tune_mode_gain *tune)
{
    struct device_gain *gain;
    node_handle n;
    int i;

    if (!tune) {
        ALOGE("tune is NULL");
        return;
    }
    for (i=0;i < TUNE_MODE_MAX;i++) {
        if (tune->gains[i]) {
            ALOGV("Tune mode: %d", i);
            n = list_get_head(tune->gains[i]);
            while (n) {
                gain = list_get_node_data(n);
                list_destroy(gain->ctls);
                n = list_get_next_node(n);
            }
            list_destroy(tune->gains[i]);
        }
    }
}
