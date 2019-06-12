/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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

#define LOG_TAG "nvaudio_hw"
//#define LOG_NDEBUG 0

#define DECLARE_NVOICE_TYPES

#include "nvaudio.h"
#include <cutils/properties.h>
#include "nvamhal_api.h"
#include <dlfcn.h>
#include <termios.h>
#ifdef NVAUDIOFX
#include "nvaudiofx_interface.h"
#endif

void* pamhalctx = NULL;
int spuv_started = 0;
int pcm_started = 0;
void *amhallib = NULL;

/*External ril HAL functions */
pFnamhal_open open_hal = NULL;
pFnamhal_close close_hal = NULL;
pFnamhal_sendto send_to_hal = NULL;

int out_stream_open = 0;

/*****************************************************************************
 * Declaration of extern functions
 *****************************************************************************/
extern void instantiate_nvaudio_service(audio_hw_device_t *dev);
extern void* get_wfd_audio_buffer();
extern int get_wfd_audio_params(uint32_t* sampleRate, uint32_t* channels,
                                uint32_t* bitsPerSample);

/*****************************************************************************
 * Forward Declaration of static functions
 *****************************************************************************/
static int set_route_by_array(struct mixer *mixer, list_handle route_list,
                              int mode, int srate);
static void close_alsa_dev(struct audio_stream *stream, struct alsa_dev *dev);
static uint32_t audio_rate_mask(uint32_t rate);
static int get_max_aux_channels (struct alsa_dev *dev);
static int shuffle_channels(int16_t *buffer, uint32_t bytes, uint32_t channels,
    uint32_t src_ch_map, uint32_t dst_ch_map);
static int  downmix(int16_t *src_buffer, uint32_t src_ch_map, uint32_t src_ch,
    int16_t *dest_buffer, uint32_t dst_ch_map, uint32_t dest_ch,
    uint32_t framecount);
static void downmix_to_stereo(int16_t *buffer, uint32_t channels,
    uint32_t frames);
static int upmix(int16_t *src_buffer, uint32_t src_ch_map, uint32_t src_ch,
    int16_t *dest_buffer, uint32_t dst_ch_map, uint32_t dest_ch,
    uint32_t framecount);
static void process_audio(struct audio_params *p_audio_params,
    size_t num_of_frames);
static uint32_t get_ch_map(uint32_t channels);
audio_stream_t *get_audio_stream(audio_hw_device_t *dev,
                                 audio_io_handle_t ioHandle);

/*****************************************************************************
 * XML config file parsing
 *****************************************************************************/
/* XML config parsing code */
static void convert_crlf(char * str)
{
    char *src, *dst;
    char c;

    src = dst = str;
    while ((c = *src++) != '\0') {
        if (c == '\\') {
            switch (*src++) {
                case 'n' : c = '\n'; break;
                case 'r' :  c = '\r'; break;
            }
        }
        *dst++ = c;
    }
    *dst = '\0';
}

static void config_parse_start(void *data, const XML_Char *elem,
                              const XML_Char **attr)
{
    struct config_parse_state *state = data;
    struct nvaudio_hw_device *hw_dev = state->hw_dev;
    struct alsa_dev *dev;
    uint32_t i, j;

    ALOGV("%s: elem %s",__FUNCTION__, elem);

    if (!strcmp(elem, "alsa_device")) {
        const XML_Char *name = NULL, *card = NULL, *device_id = NULL;
        const XML_Char *card_name = NULL;
        char temp_str[80];
        int fd = 0;
        int card_num = -1;

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];

            if (!strcmp(attr[i], "card_id"))
                card = attr[i + 1];

            if (!strcmp(attr[i], "device_id"))
                device_id = attr[i + 1];

            if (!strcmp(attr[i], "card_name"))
                card_name = attr[i + 1];
        }

        if (strcmp(name, alsa_dev_names[DEV_ID_WFD].name))  {
            if (!name || !card || !device_id || !card_name) {
                ALOGE("Incomplete alsa_device definition");
                return;
            }

            if (atoi(card) < 0) {
                card_num = snd_card_get_card_id_from_sub_string(card_name);
                if (card_num < 0) {
                    ALOGV("Invalid card ID");
                    return;
                }
                ALOGV("Found Card number %d for name %s \n", card_num, card_name );
            } else {
                sprintf(temp_str, "/proc/asound/card%d/id", atoi(card));
                fd = open(temp_str, O_RDONLY);
                if (fd <= 0) {
                    ALOGV("Invalid card ID");
                    return;
                }
                memset(temp_str, 0, sizeof(temp_str));

                read(fd, temp_str, strlen(card_name));
                if (strcmp(card_name, temp_str)) {
                    ALOGV("Invalid card name %s %s", card_name, temp_str);
                    close(fd);
                    return;
                }
                close(fd);
            }
        }

        dev = malloc(sizeof(*dev));
        if (!dev) {
            ALOGE("Failed to allocate alsa_dev");
            return;
        }
        memset(dev, 0, sizeof(*dev));
        dev->hw_dev = hw_dev;

        for (i = 0; i < ARRAY_SIZE(alsa_dev_names); i++) {
            if (!strcmp(name, alsa_dev_names[i].name)) {
                dev->id = i;
                break;
            }
        }

        if (i == ARRAY_SIZE(alsa_dev_names)) {
            ALOGE("Unsupported ALSA device name\n");
            free(dev);
            return;
        }

        ALOGV("alsa_device: name %s", name);

        if (DEV_ID_WFD != dev->id) {
            dev->card_name = strdup(card_name);
            dev->card = atoi(card) < 0 ? card_num : atoi(card);
            dev->device_id = atoi(device_id);

            dev->mixer = mixer_open(dev->card);
            if (!dev->mixer) {
                ALOGE("Failed to open mixer interface for card %d\n",
                      dev->card);
                free(dev);
                return;
            }
            ALOGV("alsa_device: card id %s", card);
            ALOGV("alsa_device: device id %s", device_id);
            ALOGV("alsa_device: card name %s", card_name);
        }

        state->dev = dev;
    } else if (!strcmp(elem, "pcm_config")) {
        struct pcm_config *conf;

        if (!state->dev) {
            ALOGV("PCM config for invalid alsa_device");
            return;
        }
        conf = &state->dev->default_config;

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "rate"))
                conf->rate = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "channels"))
                conf->channels = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "bps"))
                conf->format = (atoi(attr[i + 1]) == 16) ? PCM_FORMAT_S16_LE :
                                                     PCM_FORMAT_S32_LE;

            if (!strcmp(attr[i], "period_size"))
                conf->period_size = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "period_count"))
                conf->period_count = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "start_threshold"))
                conf->start_threshold = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "stop_threshold"))
                conf->stop_threshold = atoi(attr[i + 1]);

            if (!strcmp(attr[i], "avail_min"))
                conf->avail_min = atoi(attr[i + 1]);
        }
        memcpy(&state->dev->config, &state->dev->default_config,
               sizeof(struct pcm_config));

        ALOGV("PCM config : rate %d", conf->rate);
        ALOGV("PCM config : channels %d", conf->channels);
        ALOGV("PCM config : format %x", conf->format);
        ALOGV("PCM config : period_size %d", conf->period_size);
        ALOGV("PCM config : period_count %d", conf->period_count);
        ALOGV("PCM config : start_threshold %d", conf->start_threshold);
        ALOGV("PCM config : stop_threshold %d", conf->stop_threshold);
        ALOGV("PCM config : avail_min %d", conf->avail_min);
    } else if (!strcmp(elem, "param")) {
        const XML_Char *name = NULL, *val = NULL;

        if (!state->dev) {
            ALOGV("param element should be under alsa_device");
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];

            if (!strcmp(attr[i], "val"))
                val = attr[i + 1];
        }

        if (!name) {
            ALOGE("Incomplete param definition");
            return;
        }

        if (!strcmp(name, "ulp")) {
            if (atoi(val) == 1) {
                state->dev->ulp_supported = 1;
                ALOGV("ULP is supported");
            } else {
                state->dev->ulp_supported = 0;
                ALOGV("ULP is not supported");
            }
        }
        else if (!strcmp(name, "ulp formats")) {
            uint32_t len = strlen(val);
            uint32_t param_start = 0;

            hw_dev->ulp_formats = 0;
            i = 0;
            while (i < len) {
                while ((i < len) && val[i] == ' ') i++;
                param_start = i;
                while ((i < len) && val[i] != ' ') i++;
                if (!strncmp(&val[param_start],"mp3",(i - param_start)))
                    hw_dev->ulp_formats |= AUDIO_FORMAT_MP3;
                else if (!strncmp(&val[param_start],"aac",(i - param_start)))
                    hw_dev->ulp_formats |= AUDIO_FORMAT_AAC;
            }
        }
        else if (!strcmp(name, "ulp rates")) {
            if (strlen(val) < 80)
                strncpy(hw_dev->ulp_rates_str, val, strlen(val));
            else
                ALOGE("Too many ULP audio supported rates");
            ALOGV("Supported ULP audio rates are %s", hw_dev->ulp_rates_str);
        }
        else if (!strcmp(name, "HW Rates")) {
            uint32_t rate = 0;
            uint32_t len = strlen(val);

            state->dev->hw_rates = 0;
            i = 0;
            while (i < len) {
                state->dev->hw_rates |= audio_rate_mask(atoi(&val[i]));
                while ((i < len) && val[i] == ' ') i++;
                while ((i < len) && val[i] != ' ') i++;
            }
            ALOGV("Supported HW rates are %x", state->dev->hw_rates);
        }
        else if (!strcmp(name, "call capture rate")) {
            state->dev->call_capture_rate = atoi(val);
            ALOGV("Supported voice call capture rate  %d", state->dev->call_capture_rate);
        }
#ifdef NVOICE
        else if (!strcmp(name, "nvoice")) {
            if (atoi(val) == 1) {
                nvoice_supported = 1;
                ALOGV("NVOICE is supported");
            } else {
                nvoice_supported = 0;
                ALOGV("NVOICE is not supported");
            }
        }
#endif
        else if (!strcmp(name, "I2S Master")) {
            struct mixer_ctl *ctl = NULL;

            ctl = mixer_get_ctl_by_name(state->dev->mixer, "Set I2S Master");
            if (ctl) {
                if (atoi(val) == 1) {
                    state->dev->is_i2s_master = 1;
                    ALOGV("I2S is set as master");
                } else {
                    state->dev->is_i2s_master = 0;
                    ALOGV("I2S is set as slave");
                }
                if (mixer_ctl_set_value(ctl, 0, state->dev->is_i2s_master) < 0)
                    ALOGE("Failed to set mixer ctl Set I2S Master");
            }
            else
                ALOGE("Failed to get mixer ctl to Set I2S Master");
        } else {
            ALOGE("Invalid param element %s", name);
        }
    } else if (!strcmp(elem, "playback")) {
        if (!state->dev) {
            ALOGV("playback element should be under alsa_device");
            return;
        }
        state->dev->dev_cfg_list = list_create();
        state->dev->on_route_list = list_create();
        state->dev->off_route_list = list_create();
        state->dev->active_stream_list = list_create();
        state->dev_type = strdup("playback");
        state->dev->hw_dev = state->hw_dev;

        list_push_data(state->hw_dev->dev_list[STREAM_OUT], state->dev);
    } else if (!strcmp(elem, "capture")) {
        if (!state->dev) {
            ALOGV("capture element should be under alsa_device");
            return;
        }
        state->dev->dev_cfg_list = list_create();
        state->dev->on_route_list = list_create();
        state->dev->off_route_list = list_create();
        state->dev->active_stream_list = list_create();
        state->dev_type = strdup("capture");
        state->dev->hw_dev = state->hw_dev;

        list_push_data(state->hw_dev->dev_list[STREAM_IN], state->dev);
    } else if (!strcmp(elem, "call")) {
        if (!state->dev) {
            ALOGV("call element should be under alsa_device");
            return;
        }
        state->dev->dev_cfg_list = list_create();
        state->dev->on_route_list = list_create();
        state->dev->off_route_list = list_create();
        state->dev->at_cmd_list = list_create();
        state->dev->active_stream_list = list_create();
        state->dev_type = strdup("call");
        state->dev->hw_dev = state->hw_dev;

        list_push_data(state->hw_dev->dev_list[STREAM_CALL], state->dev);
    } else if (!strcmp(elem, "device")) {
        struct alsa_dev_cfg *dev_cfg;
        const XML_Char *name = NULL;

        if (!state->dev) {
            ALOGV("device for invalid alsa_device");
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];
        }

        if (!name) {
            ALOGV("Unnamed device\n");
            return;
        }

        for (i = 0; i < ARRAY_SIZE(audio_device_names); i++) {
            if (!strcmp(audio_device_names[i].name, name)) {
                dev_cfg = malloc(sizeof(*dev_cfg));
                if (!dev_cfg) {
                    ALOGE("Unable to allocate dev_cfg\n");
                    return;
                }
                memset(dev_cfg, 0, sizeof(*dev_cfg));

                dev_cfg->devices = audio_device_names[i].devices;
                dev_cfg->on_route_list = list_create();
                dev_cfg->off_route_list = list_create();
                dev_cfg->at_cmd_list = list_create();
                state->dev->devices |= dev_cfg->devices;

                dev_cfg->dev = state->dev;
                state->dev_cfg = dev_cfg;
                list_push_data(state->dev->dev_cfg_list, dev_cfg);
            }
        }
        ALOGV("device: name %s",name);
    } else if (!strcmp(elem, "path")) {
        const XML_Char *name = NULL;

        if (!state->dev) {
            ALOGV("path for invalid alsa_device");
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];
        }

        if (state->dev_type) {
            if (!strcmp(name, "on"))
                state->path_status = PATH_ON;
            else if (!strcmp(name, "off"))
                state->path_status = PATH_OFF;
            else {
                ALOGV("Unsupported path");
                return;
            }
        } else {
            if (name) {
                ALOGV("Glabal path should not have on/off attribute");
                return;
            }

            state->init_route_list = list_create();
            if (!state->init_route_list) {
                ALOGE("Failed to allocate init route list");
                return;
            }
            state->path_status = PATH_INIT;
        }
    } else if (!strcmp(elem, "ctl")) {
        struct route_ctls *route;
        const XML_Char *name = NULL, *val = NULL, *mode = NULL, *srate = NULL;

        if (!state->dev) {
            ALOGV("ctl for invalid alsa_device");
            return;
        }

        if (state->path_status == PATH_UNINIT) {
            ALOGV("ctl without path element");
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name"))
                name = attr[i + 1];

            if (!strcmp(attr[i], "val"))
                val = attr[i + 1];

            if (!strcmp(attr[i], "mode"))
                mode = attr[i + 1];

            if (!strcmp(attr[i], "srate"))
                srate = attr[i + 1];
        }

        if (!name) {
            ALOGV("Unnamed control\n");
            return;
        }

        if (!val) {
            ALOGV("No value specified for %s\n", name);
            return;
        }

        if (mode && srate)
            ALOGV("Parsing control %s => %s => %s => %s\n",
                  name, val, mode, srate);
        else if (mode)
            ALOGV("Parsing control %s => %s => %s\n", name, val, mode);
        else if (srate)
            ALOGV("Parsing control %s => %s => %s\n", name, val, srate);
        else
            ALOGV("Parsing control %s => %s\n", name, val);

        route = malloc(sizeof(*route));
        if (!route) {
            ALOGE("Out of memory handling %s => %s\n", name, val);
            return;
        }
        memset(route, 0, sizeof(*route));

        route->ctl_name = strdup(name);
        route->str_val = NULL;

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
            route->int_val = hex_val;
        }
        else {
            for (i = 0; i < strlen(val); i++) {
                char ch = val[i];

                if (ch < '0' || ch > '9') {
                    route->str_val = strdup(val);
                    break;
                }
            }

            if (!route->str_val)
                route->int_val = atoi(val);
        }

        if (!mode)
            route->mode = AUDIO_MODE_INVALID;
        else if (!strcmp(mode, "AUDIO_MODE_NORMAL"))
            route->mode = AUDIO_MODE_NORMAL;
        else if (!strcmp(mode, "AUDIO_MODE_RINGTONE"))
            route->mode = AUDIO_MODE_RINGTONE;
        else if (!strcmp(mode, "AUDIO_MODE_IN_CALL"))
            route->mode = AUDIO_MODE_IN_CALL;
        else if (!strcmp(mode, "AUDIO_MODE_IN_COMMUNICATION"))
            route->mode = AUDIO_MODE_IN_COMMUNICATION;
        else {
            ALOGV("Invalid mode %s for control %s", mode, name);
            return;
        }

        if (!srate)
            route->srate = 0;
        else if (!strcmp(srate, "48000"))
            route->srate = 48000;
        else if (!strcmp(srate, "44100"))
            route->srate = 44100;
        else if (!strcmp(srate, "16000"))
            route->srate = 16000;
        else if (!strcmp(srate, "8000"))
            route->srate = 8000;
        else {
            ALOGV("Invalid srate %s for control %s", srate, name);
            return;
        }

        if (state->path_status == PATH_INIT) {
            list_push_data(state->init_route_list, route);
        } else if (state->path_status == PATH_ON) {
            if (state->dev_cfg)
                list_push_data(state->dev_cfg->on_route_list, route);
            else
                list_push_data(state->dev->on_route_list, route);
        } else if (state->path_status == PATH_OFF) {
            if (state->dev_cfg)
                list_push_data(state->dev_cfg->off_route_list, route);
            else
                list_push_data(state->dev->off_route_list, route);
        }
    } else if (!strcmp(elem, "at")) {
        struct at_command *at_cmd;
        const XML_Char *type = NULL, *node = NULL, *command = NULL;

        if (!state->dev) {
            ALOGV("at command for invalid alsa_device");
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "type"))
                type = attr[i + 1];

            if (!strcmp(attr[i], "node"))
                node = attr[i + 1];

            if (!strcmp(attr[i], "cmd"))
                command = attr[i + 1];
        }

        if (!node) {
            ALOGV("Unnamed node for at command\n");
            return;
        }

        if (!command) {
            ALOGV("Unknown at command");
            return;
        }

        ALOGV("Parsing at command %s for node %s of type %s\n", command, node, type);

        at_cmd = malloc(sizeof(*at_cmd));
        if (!at_cmd) {
            ALOGE("Out of memory while parsing at command\n");
            return;
        }
        memset(at_cmd, 0, sizeof(*at_cmd));

        at_cmd->node = strdup(node);
        at_cmd->command = strdup(command);

        if (!type)
            at_cmd->type = AT_PROFILE;
        else if (!strcmp(type, "PROFILE"))
            at_cmd->type = AT_PROFILE;
        else if (!strcmp(type, "VOLUME"))
            at_cmd->type = AT_VOLUME;
        else if (!strcmp(type, "MUTE"))
            at_cmd->type = AT_MUTE;
        else if (!strcmp(type, "IN_CALL"))
            at_cmd->type = AT_IN_CALL;
        else if (!strcmp(type, "CALL_RECORD"))
            at_cmd->type = AT_CALL_RECORD;
        else {
            ALOGV("Invalid AT command type %s", type);
            if (at_cmd->node)
                free(at_cmd->node);
            if (at_cmd->command)
                free(at_cmd->command);
            free(at_cmd);
            return;
        }

        /* convert string format for "\r\n" into ascii value */
        convert_crlf(at_cmd->command);
        ALOGV("CRLF string in at command converted to ascii %s", at_cmd->command);

        if (state->dev_cfg)
            list_push_data(state->dev_cfg->at_cmd_list, at_cmd);
        else
            list_push_data(state->dev->at_cmd_list, at_cmd);
    }
}

static void config_parse_end(void *data, const XML_Char *name)
{
    struct config_parse_state *state = data;
    struct alsa_dev *dev;
    uint32_t i;

    ALOGV("%s: elem %s",__FUNCTION__, name);

    if (!strcmp(name, "playback") ||
        !strcmp(name, "capture") ||
        !strcmp(name, "call")) {
        if (!state->dev)
            return;

        if (state->dev_type) {
            free(state->dev_type);
            state->dev_type = NULL;
        }

        dev = malloc(sizeof(*dev));
        if (!dev) {
            ALOGE("Failed to allocate alsa_dev");
            return;
        }
        memset(dev, 0, sizeof(*dev));

        memcpy(&dev->default_config, &state->dev->default_config,
               sizeof(struct pcm_config));
        memcpy(&state->dev->config, &state->dev->default_config,
               sizeof(struct pcm_config));
        dev->id = state->dev->id;
        dev->mixer = state->dev->mixer;
        dev->hw_dev = state->dev->hw_dev;

        if (state->dev->card_name)
            dev->card_name = strdup(state->dev->card_name);

        dev->card = state->dev->card;
        dev->device_id = state->dev->device_id;

        state->dev = dev;
    } else if (!strcmp(name, "path")) {
        if (state->path_status == PATH_INIT)
            set_route_by_array(state->dev->mixer,
                               state->init_route_list,
                               state->hw_dev->mode,
                               0);
        state->path_status = PATH_UNINIT;
    } else if (!strcmp(name, "device")) {
        state->dev_cfg = NULL;
    }
    else if (!strcmp(name, "alsa_device")) {
        if (state->dev)
            free(state->dev);
        state->dev = NULL;
    }
}

static int config_parse(struct nvaudio_hw_device *hw_dev)
{
    struct config_parse_state state;
    int fd;
    XML_Parser parser;
    char data[80];
    int ret = 0;
    bool eof = false;
    int len;

    ALOGV("%s", __FUNCTION__);

    fd = open(XML_CONFIG_FILE_PATH_LBH, O_RDONLY);
    if (fd <= 0) {
        ALOGI("Failed to open %s, trying %s\n", XML_CONFIG_FILE_PATH_LBH,
                                                XML_CONFIG_FILE_PATH);
        fd = open(XML_CONFIG_FILE_PATH, O_RDONLY);
        if (fd <= 0) {
            ALOGE("Failed to open %s\n", XML_CONFIG_FILE_PATH);
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
    state.path_status = PATH_UNINIT;
    state.hw_dev = hw_dev;
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

#ifdef TFA
static void calibrate_speaker()
{
    int fd = 0, i = 0, n = 0, offset = 0;
    int Tcal, iCoef, tcoefA_int;
    char data[(sizeof(int) * 4)], calibdata[DATA_SIZE];
    float tCoefA, rel25, tCoef = 0.0033999;
    int T0, T1;

    T0 = TMP;
    T1 = SPKBST_TMP;
    fd = open(SPEAKERMODEL_SYSFS, O_RDWR);
    if (fd < 0) {
        ALOGE("open failed of sysfs entry %s\n", SPEAKERMODEL_SYSFS);
        goto err_exit;
    }

    if (read(fd, data, (sizeof(int) * 4)) > 0)
    {
       ALOGE("Calibrate TFA\n");
        for(i, n ; i < 2; i++) {
            memcpy(&tcoefA_int,&data[offset],sizeof(int));
            memcpy(&Tcal, &data[sizeof(int)+offset],sizeof(int));

            rel25 = (float)tcoefA_int / (1<<(23-T1));
            tCoefA = (tCoef * rel25)/(tCoef * (Tcal - T0) + 1);

           ALOGE("Resistance of speaker is %2.2f ohm @ %d degrees %d\n", rel25, Tcal, tcoefA_int);
           ALOGE("Calculated tCoefA %1.5f\n", tCoefA);
            iCoef = (int)(tCoefA*(1<<23));

            calibdata[n++] = (iCoef>>16) & 0XFF;
            calibdata[n++] = (iCoef>>8) & 0XFF;
            calibdata[n++] = (iCoef) & 0XFF;
            offset = OFFSET;
        }
       ALOGE("%d %d %d \n",calibdata[0],calibdata[1],calibdata[2]);
       ALOGE("%d %d %d \n",calibdata[3],calibdata[4],calibdata[5]);
    }
    write(fd, &calibdata, 6);
err_exit:
    if(fd)
        close(fd);
}
#endif

/*****************************************************************************
 * Utility Functions
 *****************************************************************************/

/* use emulated popcount optimization
   http://www.df.lth.se/~john_e/gems/gem002d.html */
static inline uint32_t pop_count(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

static inline bool is_non_pcm_format(int format)
{
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
    if ((format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_IEC61937)
        return true;
#endif
        return !!format && !(format & (AUDIO_FORMAT_PCM_16_BIT |
                                   AUDIO_FORMAT_PCM_8_BIT |
                                   AUDIO_FORMAT_PCM_32_BIT |
                                   AUDIO_FORMAT_PCM_8_24_BIT));
}

static inline uint32_t alsa_dev_frame_size(struct alsa_dev* dev)
{
    return dev->config.channels *
           ((dev->config.format == PCM_FORMAT_S16_LE) ? 2 : 4);
}

static inline uint32_t audio_rate_mask(uint32_t rate)
{
    uint32_t rate_mask = 0;
    switch (rate) {
        case 8000 : rate_mask |= RATE_8000; break;
        case 11025 : rate_mask |= RATE_11025; break;
        case 12000 : rate_mask |= RATE_12000; break;
        case 16000 : rate_mask |= RATE_16000; break;
        case 22050 : rate_mask |= RATE_22050; break;
        case 24000 : rate_mask |= RATE_24000; break;
        case 32000 : rate_mask |= RATE_32000; break;
        case 44100 : rate_mask |= RATE_44100; break;
        case 48000 : rate_mask |= RATE_48000; break;
        case 64000 : rate_mask |= RATE_64000; break;
        case 88200 : rate_mask |= RATE_88200; break;
        case 96000 : rate_mask |= RATE_96000; break;
        case 128000 : rate_mask |= RATE_128000; break;
        case 176400 : rate_mask |= RATE_176400; break;
        case 192000 : rate_mask |= RATE_192000; break;
        default : ALOGE("Unsupported hardware rate"); break;
    }

    return rate_mask;
}

static uint32_t audio_out_channel_mask(uint32_t channels)
{
    // ALOGV("%s : channels %d", __FUNCTION__, channels);

    switch (channels) {
        case 1:
            return AUDIO_CHANNEL_OUT_MONO;
        case 3:
            return AUDIO_CHANNEL_OUT_STEREO | AUDIO_CHANNEL_OUT_FRONT_CENTER;
        case 4:
            return AUDIO_CHANNEL_OUT_SURROUND;
        case 5:
            return AUDIO_CHANNEL_OUT_5POINT1 & ~AUDIO_CHANNEL_OUT_LOW_FREQUENCY;
        case 6:
            return AUDIO_CHANNEL_OUT_5POINT1;
        case 7:
            return AUDIO_CHANNEL_OUT_7POINT1 & ~AUDIO_CHANNEL_OUT_LOW_FREQUENCY;
        case 8:
            return AUDIO_CHANNEL_OUT_7POINT1;
        default:
            ALOGE("getAudioSystemChannels: unsupported channels %x", channels);
        case 2:
            return AUDIO_CHANNEL_OUT_STEREO;
    }
}

static uint32_t audio_in_channel_mask(uint32_t channels)
{
    // ALOGV("%s : channels %d", __FUNCTION__, channels);

    switch (channels) {
        case 1:
            return AUDIO_CHANNEL_IN_MONO;
        default:
            ALOGE("getAudioSystemChannels: unsupported channels %x", channels);
        case 2:
            return AUDIO_CHANNEL_IN_STEREO;
    }
}

/* mono to stereo conversion routine. Only supported for 16bit.*/
static void mono_to_stereo(int16_t *src, int16_t *dst, size_t frames)
{
    int i;

    for (i = frames - 1; i >= 0; i--) {
        dst[(i << 1) + 1] = src[i];
        dst[i << 1] = src[i];
    }
}

/* stereo to mono conversion routine  Only supported for 16bit.*/
static void stereo_to_mono(int16_t *src, int16_t *dst, size_t frames)
{
    uint32_t i;

    for (i = 0; i < frames; i++)
        dst[i] = (src[i << 1] >> 1) + (src[(i << 1) + 1] >> 1);
}

static uint32_t get_active_dev_rate(struct nvaudio_hw_device *hw_dev,
                                    uint32_t card_id,
                                    uint32_t device_id)
{
    struct alsa_dev *dev = NULL;
    node_handle n;
    int i;

    ALOGV("%s : card %d device %d", __FUNCTION__, card_id, device_id);
    for (i = 0; i < STREAM_MAX; i++) {
        n = list_get_head(hw_dev->dev_list[i]);
        while (n) {
            dev = (struct alsa_dev *)list_get_node_data(n);

            n = list_get_next_node(n);
            if (dev->pcm && (dev->card == card_id) &&
                (dev->device_id == device_id))
                return dev->config.rate;
        }
    }

    return 0;
}

static int is_ulp_supported(struct nvaudio_hw_device *hw_dev)
{
    /* ULP is supported only if all following conditions are satisfied:
       AVP device can be opened successfully.
       All active output alsa devices must support ULP.
       No android audio effect is enabled.
       Not in a voice call.
    */
    node_handle n;

    if (!hw_dev->avp_audio || hw_dev->effects_count ||
         (hw_dev->media_devices & ~NVAUDIO_ULP_OUT_DEVICES) ||
         (hw_dev->mode == AUDIO_MODE_IN_CALL) ||
         (nvaudio_is_submix_on((struct audio_hw_device*)hw_dev))) {
        ALOGV("ULP not supported:avp_audio %x dev %d effects %d",
                 (int)hw_dev->avp_audio, hw_dev->media_devices,
                 hw_dev->effects_count);
        return 0;
    }

    ALOGV("ULP supported: dev %x", hw_dev->media_devices);
    return 1;
}

audio_stream_t *get_audio_stream(audio_hw_device_t *dev,
                                 audio_io_handle_t ioHandle)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;

    ALOGV("%s : ioHandle %d", __FUNCTION__, ioHandle);

    n = list_get_head(hw_dev->stream_out_list);
    while (n) {
        struct nvaudio_stream_out *out =
                (struct nvaudio_stream_out *)list_get_node_data(n);

        n = list_get_next_node(n);
        if (out->io_handle == ioHandle)
            return &out->stream.common;
    }

    n = list_get_head(hw_dev->stream_in_list);
    while (n) {
        struct nvaudio_stream_in *in =
                (struct nvaudio_stream_in *)list_get_node_data(n);

        n = list_get_next_node(n);
        if (in->io_handle == ioHandle)
            return &in->stream.common;
    }

    return NULL;
}

/* SW output volume control support  Only supported for 16bit.*/
static void apply_volume(int16_t* buffer, size_t frames,
                         struct audio_params *p_audio_params)
{
    uint32_t s_channels = p_audio_params->src_channels;
    uint32_t src_ch_map = p_audio_params->src_ch_map;
    uint32_t d_channels = p_audio_params->dest_channels;
    uint32_t dst_ch_map = p_audio_params->dest_ch_map;
    uint32_t *vol = p_audio_params->vol;
    uint32_t temp[TEGRA_AUDIO_CHANNEL_ID_LAST] = {0};
    uint32_t src_ch_id;
    uint32_t dst_ch_id;
    uint32_t i, j, s_mask, d_mask;

    // ALOGV("%s ", __FUNCTION__);
    /* Shuffle Volume  based on ch map */
    if (src_ch_map != dst_ch_map) {
        for (i = 0, d_mask = 0xF; i < d_channels; i++, d_mask <<= 4) {
            dst_ch_id = (dst_ch_map & d_mask) >> (i << 2);
            for (j = 0, s_mask = 0xF; j < s_channels; j++, s_mask <<= 4) {
                src_ch_id = (src_ch_map & s_mask) >> (j << 2);
                if (dst_ch_id == src_ch_id)
                {
                    temp[i] = p_audio_params->vol[j];
                    break;
                }
            }
            if (dst_ch_id != src_ch_id)
                temp[i] = INT_Q15(1);
        }
        vol = temp;
    }

    do {
        for (i = 0; i < d_channels; i++)
            buffer[i] = (int16_t)(((int32_t)buffer[i] * vol[i]) >> SHIFT_Q15);
        buffer += d_channels;
    } while(--frames);
}

/*
 * Helper functions for managing low power modes
 */
static int save_lp_mode(struct nvaudio_hw_device *hw_dev)
{
    int fd;

    if (strcmp(hw_dev->lp_state_default, "\0"))
        return 0;

    fd = open(LP_STATE_SYSFS, O_RDWR);

    if (fd < 0) {
        ALOGE("open failed of sysfs entry %s\n", LP_STATE_SYSFS);
        return -EINVAL;
    }

    read(fd, hw_dev->lp_state_default, 3);
    close(fd);

    return 0;
}

static int set_lp_mode(char *lp_mode)
{
    int fd;

    fd = open(LP_STATE_SYSFS, O_RDWR);

    if (fd < 0) {
        ALOGE("open failed of sysfs entry %s\n", LP_STATE_SYSFS);
        return -EINVAL;
    }

    write(fd, lp_mode, strlen(lp_mode));
    close(fd);

    return 0;
}

static int restore_lp_mode(struct nvaudio_hw_device *hw_dev)
{
    int ret;

    if (!strcmp(hw_dev->lp_state_default, "\0"))
        return 0;

    ret =  set_lp_mode(hw_dev->lp_state_default);

    strcpy(hw_dev->lp_state_default, "\0");

    return ret;
}

/* Helper functions for sending AT commands to the RIL */
static int send_at_cmd(struct at_command *cmd)
{
    struct termios opt;
    int fd = 0;
    int ret = 0;

    ALOGV("%s: node %s command %s", __FUNCTION__, cmd->node, cmd->command);

    fd = open(cmd->node, O_RDWR);
    if (fd <= 0) {
        ALOGV("%s open failed\n", cmd->node);
        return -ENOMEM;
    }

    // put into RAW mode
    tcgetattr(fd, &opt);
    cfmakeraw(&opt);
    tcsetattr(fd, TCSANOW, &opt);
    // discard noisy data from modem
    tcflush(fd, TCIOFLUSH);

    // info modem to start a new line
    ret = write(fd, "\r\n", 2);
    if(ret < 0)
        ALOGE("write failed ret=%d",ret);

    ret = write(fd, cmd->command, strlen(cmd->command));
    if(ret < 0)
        ALOGV("write failed ret=%d",ret);

    // waiting until data written to modem
    usleep(20000);

    close(fd);
    return 0;
}

static int send_at_cmd_by_list(list_handle at_cmd_list,
        enum at_cmd_type type, char *val)
{
    int ret = -EINVAL;
    node_handle n = list_get_head(at_cmd_list);

    ALOGV("%s: type %d", __FUNCTION__, type);

    /* Go through the AT command list and send each command */
    while (n) {
        struct at_command* ac = list_get_node_data(n);

        n = list_get_next_node(n);
        if (ac) {
            if (ac->type == type) {
                struct at_command ac_new;
                char at_node[20], at_cmd[30];
                unsigned int cmd_len = 0;

                memset(at_node, 0, sizeof(at_node));
                memset(at_cmd, 0, sizeof(at_cmd));
                if (strlen(ac->node) < sizeof(at_node))
                    strcpy(at_node, ac->node);
                else {
                    ALOGE("%s: AT node size is insufficient", __FUNCTION__);
                    continue;
                }

                cmd_len = strlen(ac->command);
                if (val)
                    cmd_len += strlen(val);

                if (cmd_len < sizeof(at_cmd)) {
                    strcpy(at_cmd, ac->command);
                    if (val)
                        strcat(at_cmd, val);
                } else {
                    ALOGE("%s: AT command size is insufficient", __FUNCTION__);
                    continue;
                }

                ac_new.node = at_node;
                ac_new.command = at_cmd;
                convert_crlf(ac_new.command);
                ret = send_at_cmd(&ac_new);
            }
        }
    }
    return ret;
}

/* Helper functions for sending AT command for call record */
static int call_record_at_cmd(struct nvaudio_hw_device *hw_dev,
                              struct nvaudio_stream_in *in, int on)
{
    node_handle n;
    uint32_t ret;
    char val[5];

    ALOGV("%s", __FUNCTION__);

    memset(val, 0, sizeof(val));
    sprintf(val,"%d\r\n", on);
    n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);
        n = list_get_next_node(n);
        if (dev->at_cmd_list) {
            ret = send_at_cmd_by_list(dev->at_cmd_list, AT_CALL_RECORD, val);
            if(!ret)
               break;
        }
    }
    return 0;
}

/* routing */
static int set_route_by_array(struct mixer *mixer,
                              list_handle route_list,
                              int mode,
                              int srate)
{
    struct mixer_ctl *ctl;
    uint32_t i, j, ret;
    node_handle n = list_get_head(route_list);

    ALOGV("%s", __FUNCTION__);

    /* Go through the route array and set each value */
    while (n) {
        struct route_ctls* r = list_get_node_data(n);

        n = list_get_next_node(n);
        if(((r->mode == AUDIO_MODE_INVALID) && (r->srate == 0))
             || ((r->mode == AUDIO_MODE_INVALID) && (r->srate == srate))
             || ((r->mode == mode) && (r->srate == 0))
             || ((r->mode == mode) && (r->srate == srate))) {
                ctl = mixer_get_ctl_by_name(mixer, r->ctl_name);
                if (!ctl) {
                    ALOGE("Unknown control '%s'\n", r->ctl_name);
                    return -EINVAL;
                }

            if (r->str_val) {
                ret = mixer_ctl_set_enum_by_string(ctl, r->str_val);
                if (ret != 0) {
                    ALOGE("Failed to set '%s' to '%s'\n",
                          r->ctl_name, r->str_val);
                } else {
                    ALOGV("Set '%s' to '%s' mode = %d r->mode = %d\n",
                          r->ctl_name, r->str_val, mode, r->mode);
                }
            } else {
                /* This ensures multiple (i.e. stereo) values are set jointly */
                for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                    ret = mixer_ctl_set_value(ctl, j, r->int_val);
                    if (ret != 0) {
                        ALOGE("Failed to set '%s'.%d to %d\n",
                             r->ctl_name, j, r->int_val);
                    } else {
                        ALOGV("Set '%s' to '%d' mode = %d r->mode = %d"
                               " srate = %d r->srate = %d \n",
                               r->ctl_name, r->int_val, mode, r->mode,
                               srate, r->srate);
                    }
                }
            }
        }
    }

    return 0;
}

static void free_route_list(list_handle list)
{
    node_handle n = list_get_head(list);

    ALOGV("%s", __FUNCTION__);

    while (n) {
        struct route_ctls *r = list_get_node_data(n);

        if (r->ctl_name)
            free(r->ctl_name);
        if (r->str_val)
            free(r->str_val);

        n = list_get_next_node(n);
    }
    list_destroy(list);
}

static void free_at_cmd_list(list_handle list)
{
    node_handle n = list_get_head(list);

    ALOGV("%s", __FUNCTION__);

    while (n) {
        struct at_command *ac = list_get_node_data(n);

        if (ac) {
            if (ac->node)
                free(ac->node);
            if (ac->command)
                free(ac->command);
        }

        n = list_get_next_node(n);
    }
    list_destroy(list);
}

/* Call with device lock held */
static void set_route(struct alsa_dev* dev,
                      uint32_t new_devices,
                      uint32_t old_devices,
                      audio_stream_t *stream)
{
    node_handle n;

    ALOGV("%s : dev id %d ref %d devices 0x%x -> 0x%x",
          __FUNCTION__, dev->id, dev->ref_count, old_devices, new_devices);

    /* Enable new route */
    if (dev->devices & new_devices) {
        if (!dev->ref_count) {
            set_route_by_array(dev->mixer,
                               dev->on_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);
        }

        /* Turn off old route */
        n = list_get_head(dev->dev_cfg_list);
        while (n) {
            uint32_t ref_count = 0;
            struct alsa_dev_cfg *dev_cfg = list_get_node_data(n);

            if (dev_cfg->on && !(dev_cfg->devices & new_devices)) {

                if (stream && (dev->devices & AUDIO_DEVICE_OUT_ALL)) {
                /* Check if any other active stream is using this route */
                    node_handle n_j = list_get_head(
                                              dev->active_stream_list);

                    while (n_j) {
                        struct nvaudio_stream_out *out =
                                        list_get_node_data(n_j);

                        n_j = list_get_next_node(n_j);
                        if ((struct nvaudio_stream_out *)stream == out)
                            continue;

                        if (dev_cfg->devices & out->devices)
                            ref_count++;
                    }
                }

                ALOGV("%d devices is routed to dev_cfg 0x%x",
                              ref_count, dev_cfg->devices);

                set_route_by_array(dev->mixer,
                                   dev_cfg->off_route_list,
                                   dev->hw_dev->mode,
                                   dev->config.rate);
                dev_cfg->on = false;
            }
            n = list_get_next_node(n);
        }

        /* Turn on new route */
        n = list_get_head(dev->dev_cfg_list);
        while (n) {
            struct alsa_dev_cfg *dev_cfg = list_get_node_data(n);

            if (!dev_cfg->on && (dev_cfg->devices & new_devices)) {
                set_route_by_array(dev->mixer,
                                   dev_cfg->on_route_list,
                                   dev->hw_dev->mode,
                                   dev->config.rate);
                if (dev_cfg->at_cmd_list)
                    send_at_cmd_by_list(dev_cfg->at_cmd_list, AT_PROFILE, NULL);

                dev_cfg->on = true;
                if(send_to_hal)
                {
                    ALOGV("%s : dev->mode %d", __FUNCTION__, dev->mode);
                    if ((dev->mode == AUDIO_MODE_IN_CALL) &&
                        !(dev->devices & AUDIO_DEVICE_BIT_IN)) {
                        if (new_devices & (AUDIO_DEVICE_OUT_SPEAKER |
                            AUDIO_DEVICE_OUT_DEFAULT)) {
                                ALOGV("Send AMHAL_PARAMETERS for SPEAKER");
                                (*send_to_hal)(pamhalctx,AMHAL_PARAMETERS,(void *) 2);
                        }
                        if (new_devices & AUDIO_DEVICE_OUT_EARPIECE) {
                                ALOGV("Send AMHAL_PARAMETERS for earpiece");
                                (*send_to_hal)(pamhalctx,AMHAL_PARAMETERS,(void *) 0);
                        }
                        if (new_devices & (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
                                ALOGV("Send AMHAL_PARAMETERS for Headset");
                                (*send_to_hal)(pamhalctx,AMHAL_PARAMETERS,(void *) 1);
                        }
                    }
                }
#ifdef AUDIO_TUNE
                set_tune_gain(&dev->hw_dev->tune, dev->mixer,
                              dev->hw_dev->mode, new_devices);
#endif
            }
            n = list_get_next_node(n);
        }
        if (!(dev->devices & old_devices))
            dev->ref_count++;
    }

    /* Disable old routes */
    if (!(dev->devices & new_devices) && (dev->devices & old_devices)) {
        if (dev->ref_count > 0)
            dev->ref_count--;
        else
            ALOGE("dev reference count is -ve\n");

        if (!dev->ref_count) {
            n = list_get_head(dev->dev_cfg_list);

            while (n) {
                struct alsa_dev_cfg *dev_cfg = list_get_node_data(n);

                if (dev_cfg->on) {
                    set_route_by_array(dev->mixer,
                                       dev_cfg->off_route_list,
                                       dev->hw_dev->mode,
                                       dev->config.rate);
                    dev_cfg->on = false;
                }
                n = list_get_next_node(n);
            }

            set_route_by_array(dev->mixer,
                               dev->off_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);
        }
    }
}

/* Call with device lock held */
/* Call this function when mode or sample rate has changed
   but routing has not changed */
static void set_current_route(struct alsa_dev* dev)
{
    node_handle n;

    ALOGV("%s : dev id %d", __FUNCTION__, dev->id);

    set_route_by_array(dev->mixer,
                       dev->on_route_list,
                       dev->hw_dev->mode,
                       dev->config.rate);

    n = list_get_head(dev->dev_cfg_list);
    while (n) {
        struct alsa_dev_cfg *dev_cfg = list_get_node_data(n);

        if (dev_cfg->on) {
            set_route_by_array(dev->mixer,
                               dev_cfg->on_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);

            if (dev_cfg->at_cmd_list)
                send_at_cmd_by_list(dev_cfg->at_cmd_list, AT_PROFILE, NULL);
        }
        n = list_get_next_node(n);
    }
}

static int nvaudio_pcm_write(struct alsa_dev *dev, void *data, unsigned int count, uint32_t device)
{
#ifdef NVAUDIOFX
#if 0
    property_get("nvfx.set.param",fx_set_param,"0");
    property_get("nvfx.get.param",fx_get_param,"0");
    property_get("nvfx.value",fx_value,"0");
    if(atoi(fx_set_param))
    {
        nvaudiofx_set_param(atoi(fx_set_param),atol(fx_value));
    }
#endif

    //ALOGV("%s  device %x", __FUNCTION__, device);
    if (device & AUDIO_DEVICE_OUT_SPEAKER) {
        nvaudiofx_process(data, count/alsa_dev_frame_size(dev), fx_vol);
        //ALOGV("%s nvaudiofx_process", __FUNCTION__);
    }
#endif

#ifdef NVOICE
    if((nvoice_supported) && (dev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
        if (!(dev->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX)) {
            if(dev->config.channels == 2)
                stereo_to_mono((void *)data,(void*)data,count/alsa_dev_frame_size(dev));
            nvoice_processrx((void *)data, count/alsa_dev_frame_size(dev), dev->config.rate);
            if(dev->config.channels == 2)
                mono_to_stereo((void *)data,(void *)data,count/alsa_dev_frame_size(dev));
        } else  {
            if(dev->pcm_out_buf) {
               if(dev->config.channels == 2)
                  stereo_to_mono((void *)data,(void*)dev->pcm_out_buf,count/alsa_dev_frame_size(dev));
               else
                  memcpy(dev->pcm_out_buf, data, count);

            nvoice_processrx((void *)dev->pcm_out_buf, count/alsa_dev_frame_size(dev), dev->config.rate);
           }
        }
    }
#endif
    return pcm_write(dev->pcm, data, count);
}

static int nvaudio_avp_stream_write(avp_stream_handle_t hstrm, void *data,
                                    unsigned int count, uint32_t device, uint32_t format,
                                    struct nvaudio_stream_out *out,struct alsa_dev *dev)
{
#ifdef NVAUDIOFX
    //ALOGV("%s  device %x", __FUNCTION__, device);
#if 0
    property_get("nvfx.set.param",fx_set_param,"0");
    property_get("nvfx.get.param",fx_get_param,"0");
    property_get("nvfx.value",fx_value,"0");
    if(atoi(fx_set_param))
    {
        nvaudiofx_set_param(atoi(fx_set_param),atol(fx_value));
    }
#endif

    if ((device & AUDIO_DEVICE_OUT_SPEAKER) &&
        (!is_non_pcm_format(format))) {
            nvaudiofx_process(data, count/4, fx_vol);
            //ALOGV("%s  nvaudiofx_process", __FUNCTION__);
    }
#endif
#ifdef NVOICE
    if((!out->nvoice_stopped) && (nvoice_supported) && (out->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
        if (!(dev->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX)) {
            if(dev->config.channels == 2)
                stereo_to_mono((void *)data,(void*)data,count/4);
            nvoice_processrx((void *)data, count/4, dev->config.rate);
            if(dev->config.channels == 2)
                mono_to_stereo((void *)data,(void *)data,count/4);
        } else  {
            if(dev->pcm_out_avp_buf) {
               if(dev->config.channels == 2)
                  stereo_to_mono((void *)data,(void*)dev->pcm_out_avp_buf,count/4);
               else
                  memcpy(dev->pcm_out_avp_buf, data, count);

            nvoice_processrx((void *)dev->pcm_out_avp_buf, count/4, dev->config.rate);
            }
        }
    }
#endif
    return avp_stream_write(hstrm, data, count);
}


/* callback function to trigger i2s dma during start of ULP playback*/
static void trigger_dma_callback(void *args, void *dma_addr, int on)
{
    struct alsa_dev *dev = (struct alsa_dev *)args;
    struct mixer_ctl *ctl;
    uint32_t zero = 0;

    ALOGV("%s dma address 0x%08X on %d", __FUNCTION__, (uint32_t)dma_addr, on);
    if(!dev) {
        ALOGV("Invalid alsa_device handle");
        return;
    }

    pthread_mutex_lock(&dev->lock);

    ctl = mixer_get_ctl_by_name(dev->mixer,
                                "AVP DMA address");
    if (!ctl)
        ALOGW("Failed to get mixer ctl AVP DMA address");

    if (dev->pcm) {
        if (on) {
            if (ctl)
                mixer_ctl_set_value(ctl, 0, (uint32_t)dma_addr);
            nvaudio_pcm_write(dev, (void *)&zero, sizeof(zero),0);
            pcm_start(dev->pcm);
        } else {
            pcm_stop(dev->pcm);
            if (ctl)
                mixer_ctl_set_value(ctl, 0, 0);
        }
    }
    pthread_mutex_unlock(&dev->lock);
}

/* Call with device lock held */
static int set_avp_render_path(struct alsa_dev *dev,
                               avp_audio_handle_t avp_audio)
{
    struct mixer_ctl *ctl;
    int dma_ch_id;
    uint32_t dma_phys_addr;
    uint32_t offset, frames;
    int ret = 0;

    ALOGV("%s", __FUNCTION__);

    ctl = mixer_get_ctl_by_name(dev->mixer,
                                "AVP alsa device select");
    if (!ctl) {
        ALOGE("Failed to get mixer ctl AVP alsa device select");
        return -EINVAL;
    }

    ret = mixer_ctl_set_value(ctl, 0, dev->device_id);
    if (ret < 0) {
        ALOGE("Failed to set mixer ctl AVP alsa device select.err %d", ret);
        return -EINVAL;
    }

    ctl = mixer_get_ctl_by_name(dev->mixer,
                                "AVP DMA channel id");
    if (!ctl) {
        ALOGE("Failed to get mixer ctl AVP DMA channel id");
        return -EINVAL;
    }

    dma_ch_id = mixer_ctl_get_value(ctl, 0);
    if (dma_ch_id < 0) {
        ALOGE("Invalid AVP DMA channel id %d", dma_ch_id);
        return -EINVAL;
    }

    ctl = mixer_get_ctl_by_name(dev->mixer,
                                "AVP DMA address");
    if (!ctl) {
        ALOGE("Failed to get mixer ctl AVP DMA address");
        return -EINVAL;
    }

    dma_phys_addr = mixer_ctl_get_value(ctl, 0);
    if (!dma_phys_addr) {
        ALOGE("Invalid DMA physical address");
        return -EINVAL;
    }

    avp_audio_set_dma_params(avp_audio, dma_ch_id,
                             NULL, dma_phys_addr);
    avp_audio_set_rate(avp_audio, dev->config.rate);

    return 0;
}

/* Call with device lock held */
static void set_mute(struct alsa_dev* dev, uint32_t devices, bool mute)
{
    char val[5];
    node_handle n = list_get_head(dev->dev_cfg_list);

    ALOGV("%s : mute %d", __FUNCTION__, mute);

    if (dev->hw_dev->mode == AUDIO_MODE_IN_CALL) {
        memset(val, 0, sizeof(val));
        sprintf(val,"%d\r\n", mute);

        if (dev->at_cmd_list)
            send_at_cmd_by_list(dev->at_cmd_list, AT_MUTE, val);
    }

    while (n) {
        struct alsa_dev_cfg *dev_cfg = list_get_node_data(n);

        if (mute && dev_cfg->on) {
            /* disable active paths */
            set_route_by_array(dev->mixer,
                               dev_cfg->off_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);
            dev_cfg->on = false;
        } else if (!mute && (dev_cfg->devices & devices) &&
                   !dev_cfg->on) {
            /* enable muted paths */
            set_route_by_array(dev->mixer,
                               dev_cfg->on_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);
            dev_cfg->on = true;
        }
        n = list_get_next_node(n);
    }
}

/* Call with device and out stream lock held */
static int set_out_resampler(struct nvaudio_stream_out* out,
                            struct alsa_dev *dev,
                            uint32_t quality)
{
    uint32_t id = dev->id;
    uint32_t size = 0;
    int i, ret = 0;

    ALOGV("%s : quality %d", __FUNCTION__, quality);

    if (out->avp_stream) {
        ALOGV("Cannot add resampler for compressed stream.");
        return 0;
    }

    if (out->resampler[id]) {
        ALOGW("Resampler is already allocated. Cannot allocate a new one.");
        return 0;
    }

    if (out->rate == dev->config.rate) {
        ALOGV("No resampler required");
        /*Only for upmixing a buffer is required,
        downmixing can be done in place*/
        if (out->channel_count < dev->config.channels) {

            if (out->buffer[id])
                free(out->buffer[id]);

            size = dev->config.period_size * alsa_dev_frame_size(dev);

            out->buffer[id] = malloc(size);
            if (!out->buffer[id]) {
                ALOGE("Failed to allocate resmpler buffer");
                ret = -ENOMEM;
                goto err_exit;
            }
        }
        return 0;
    }

    ret = create_resampler(out->rate,
                           dev->config.rate,
                           out->channel_count,
                           quality,
                           NULL,
                           &out->resampler[id]);
    if (ret < 0) {
        ALOGE("Failed to create resampler");
        goto err_exit;
    }

    if (out->buffer[id])
        free(out->buffer[id]);

    size = dev->config.period_size * alsa_dev_frame_size(dev);
    if (out->channel_count > dev->config.channels)
        size = (size * out->channel_count) / dev->config.channels;

    out->buffer[id] = malloc(size);
    if (!out->buffer[id]) {
        ALOGE("Failed to allocate resmpler buffer");
        ret = -ENOMEM;
        goto err_exit;
    }

    for (i = 0; i < DEV_ID_NON_CALL_MAX; i++)
        out->buff_pos[i] = 0;

    return 0;

err_exit:
    if (out->resampler[id]) {
        release_resampler(out->resampler[id]);
        out->resampler[id] = NULL;
    }

    return ret;
}

/* Call with in stream lock held */
static int set_in_resampler(struct nvaudio_stream_in* in,
                            uint32_t quality)
{
    struct alsa_dev *dev = in->dev;
    uint32_t size = 0;
    int ret = 0;

    ALOGV("%s : quality %d", __FUNCTION__, quality);

    if ((in->rate == dev->config.rate) &&
        (in->channel_count == dev->config.channels)) {
        ALOGV("No resampler required");
        return 0;
    }

    if (!in->resampler && (in->rate != dev->config.rate)) {
        ret = create_resampler(dev->config.rate,
                               in->rate,
                               in->channel_count,
                               quality,
                               NULL,
                               &in->resampler);
        if (ret < 0) {
            ALOGE("Failed to create resampler");
            goto err_exit;
        }
    }

    if (in->buffer)
        free(in->buffer);

    size = dev->config.period_size * alsa_dev_frame_size(dev);
    if (in->channel_count > dev->config.channels)
        size = (size * in->channel_count) / dev->config.channels;

#ifdef NVOICE
    if ((nvoice_supported) && (in->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION)
             && (dev->config.channels == 1))
        size = size * 2;
#endif

    in->buffer = malloc(size);
    if (!in->buffer) {
        ALOGE("Failed to allocate resmpler buffer");
        ret = -ENOMEM;
        goto err_exit;
    }
    in->buffer_offset = 0;

    return 0;

err_exit:
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

    return ret;
}

/*****************************************************************************
*MultiChannel Utility Functions
*****************************************************************************/

static int get_max_aux_channels (struct alsa_dev *dev)
{
    struct mixer_ctl *ctl;
    int channels = 0;

    ctl = mixer_get_ctl_by_name(dev->mixer, "HDA Maximum PCM Channels");
    channels = mixer_ctl_get_value(ctl, 0);
    /*max channels is 0 if eld data not initialised*/
    if (channels == 0)
        channels = 2;

    ALOGV("max channels %d \n",channels);
    return channels;
}

static int shuffle_channels(int16_t *buffer, uint32_t bytes, uint32_t channels,
    uint32_t src_ch_map, uint32_t dst_ch_map)
{
    uint32_t frames = (bytes) /(channels*2);
    int16_t tmp[TEGRA_AUDIO_CHANNEL_ID_LAST] = {0};

    if (src_ch_map == dst_ch_map)
        return -EINVAL;

    do {
        uint32_t i, mask;
        for (i = 0, mask = 0xF; i < channels; i++, mask <<= 4) {
            uint32_t src_ch_id = src_ch_map & mask;
            if ((dst_ch_map & mask) == src_ch_id)
               continue;

            tmp[src_ch_id >> (i << 2)] = buffer[i];
        }

        for (i = 0, mask = 0xF; i < channels; i++, mask <<= 4) {
            uint32_t dst_ch_id = dst_ch_map & mask;
            if ((src_ch_map & mask) == dst_ch_id)
                continue;

            buffer[i] = tmp[dst_ch_id >> (i << 2)];
        }
        buffer += channels;
    } while(--frames);

    return 0;
}

static inline int16_t sat16(int32_t val)
{
    if ((val >> 15) ^ (val >> 31))
        val = 0x7FFF ^ (val >> 31);

    return val;
}

/*This function does inplace computation (src and dest buffers are same)
if src_buffer is equal to dest_buffer*/
static int  downmix(int16_t *src_buffer, uint32_t src_ch_map, uint32_t src_ch,
    int16_t *dest_buffer, uint32_t dst_ch_map, uint32_t dest_ch,
    uint32_t framecount)
{
    uint32_t i, mask;
    uint32_t j, mask2;
    int32_t tmp;

    if (src_ch <= dest_ch)
        return -EINVAL;

    do {
        for (i = 0, mask = 0xF; i < dest_ch; i++, mask <<= 4) {
            tmp = 0;
            if ((dst_ch_map & mask) == LFE)
                continue;

            for (j = 0, mask2 = 0xF; j < src_ch; j++, mask2 <<= 4) {
                if ((src_ch_map & mask2) != LFE)
                     tmp += src_buffer[j];
            }
            dest_buffer[i] = sat16(tmp);
        }
        src_buffer += src_ch;
        dest_buffer += dest_ch;
    } while(--framecount);

    return 0;
}

 /*This function does inplace computation (src and dest buffers are same)*/
static void downmix_to_stereo(int16_t *buffer, uint32_t channels,
    uint32_t frames)
{
    uint32_t i, count;

    /*3 channels to stereo downmix*/
    if(3 == channels)
    {
        count = 0;
        for(i = 0; i < frames*channels; i+=channels)
        {
            short cmix;
            short left;
            short right;
           //cmix = c*0.70703125
            cmix = ((buffer[i] * 23167) << 1) >> 16;
           // left = 0.58798 (left + cmix) = 0.58798*left + 0.41572*c
            left = ((19267*(buffer[i+1] + cmix)) << 1) >> 16;
           // right = 0.58798 (right + cmix) = 0.58798*right + 0.41572*c
            right = ((19267*(buffer[i+2] + cmix)) << 1) >> 16;
            buffer[count++] = left;
            buffer[count++] = right;
            }
    }

    /*4 channels to stereo downmix*/
    if(4 == channels)
    {
        count = 0;
        for(i = 0; i < frames*channels; i+=channels)
        {
            short cmix;
            short rearmix;
            short left;
            short right;
            //cmix = c*0.70703125
            cmix = ((buffer[i] * 23167) << 1) >> 16;
            //rearmix = rear*0.70703125
            rearmix = ((buffer[i+3] * 23167) << 1) >> 16;
            // left = 0.414 (left + cmix + rearmix) = 0.414*left
            //+ 0.2927*c + 0.2927*rear
            left = ((13566*(buffer[i+1] + cmix + rearmix)) << 1) >> 16;
            // right = 0.414 (right + cmix + rearmix) = 0.414*right
            //+ 0.2927*c + 0.2927*rear
            right = ((13566*(buffer[i+2] + cmix + rearmix)) << 1) >> 16;
            buffer[count++] = left;
            buffer[count++] = right;
        }
    }

    /*5 channels to stereo downmix*/
    if(5 == channels)
    {
        count = 0;
        for(i = 0; i < frames*channels; i+=channels)
        {
             short cmix;
             short l_rearmix;
             short r_rearmix;
             short left;
             short right;
             //cmix = c*0.70703125
             cmix = ((buffer[i] * 23167) << 1) >> 16;
             //l_rearmix = l_rear*0.70703125
             l_rearmix = ((buffer[i+3] * 23167) << 1) >> 16;
             //r_rearmix = r_rear*0.70703125
             r_rearmix = ((buffer[i+4] * 23167) << 1) >> 16;
            // left = 0.414 (left + cmix + rearmix) = 0.414*left
            //+ 0.2927*c + 0.2927*rear
             left = ((13566*(buffer[i+1] + cmix + l_rearmix)) << 1) >> 16;
             // right = 0.414 (right + cmix + rearmix) = 0.414*right
             //+ 0.2927*c + 0.2927*rear
             right = ((13566*(buffer[i+2] + cmix + r_rearmix)) << 1) >> 16;
             buffer[count++] = left;
             buffer[count++] = right;
        }
    }

    /*5.1 channels to stereo downmix (NOTE: LFE is not mixed)*/
    if(6 == channels)
    {
        count = 0;
        for(i = 0; i < frames*channels; i+=channels)
        {
            short cmix;
            short l_rearmix;
            short r_rearmix;
            short left;
            short right;
            //cmix = c*0.70703125
            cmix = ((buffer[i] * 23167) << 1) >> 16;
            //l_rearmix = l_rear*0.70703125
            l_rearmix = ((buffer[i+3] * 23167) << 1) >> 16;
            //r_rearmix = r_rear*0.70703125
            r_rearmix = ((buffer[i+4] * 23167) << 1) >> 16;
            // left = 0.414 (left + cmix + rearmix) = 0.414*left
            //+ 0.2927*c + 0.2927*rear
            left = ((13566*(buffer[i+1] + cmix + l_rearmix)) << 1) >> 16;
            // right = 0.414 (right + cmix + rearmix) = 0.414*right
            //+ 0.2927*c + 0.2927*rear
            right = ((13566*(buffer[i+2] + cmix + r_rearmix)) << 1) >> 16;
            buffer[count++] = left;
            buffer[count++] = right;
        }
    }

    /*7 channels to stereo downmix*/
    if(7 == channels)
    {
        count = 0;
        for(i = 0; i < frames*channels; i+=channels)
        {
            short cmix;
            short l_rearmix;
            short r_rearmix;
            short l_sidemix;
            short r_sidemix;
            short left;
            short right;
            //cmix = c*0.70703125
            cmix = ((buffer[i] * 23167) << 1) >> 16;
            //l_rearmix = l_rear*0.70703125
            l_rearmix = ((buffer[i+3] * 23167) << 1) >> 16;
            //r_rearmix = r_rear*0.70703125
            r_rearmix = ((buffer[i+4] * 23167) << 1) >> 16;
            //l_sidemix = l_side*0.70703125
            l_sidemix = ((buffer[i+5] * 23167) << 1) >> 16;
            //r_sidemix = r_side*0.70703125
            r_sidemix = ((buffer[i+6] * 23167) << 1) >> 16;
            // left = 0.320 (left + cmix + rearmix + sidemix) =
            //0.320*left + 0.2927*c + 0.2927*rear + 0.2265*side
            left = ((10486*(buffer[i+1] + cmix + l_rearmix + l_sidemix)) << 1)
                >> 16;
            // right = 0.320 (right + cmix + rearmix + sidemix) =
            //0.320*right + 0.2265*c + 0.2265*rear + 0.2265*side
            right = ((10486*(buffer[i+2] + cmix + r_rearmix + r_sidemix)) << 1)
                >> 16;
            buffer[count++] = left;
            buffer[count++] = right;
        }
    }

    /*7.1 channels to stereo downmix (NOTE: LFE is not mixed)*/
    if(8 == channels)
    {
    count = 0;
    for(i = 0; i < frames*channels; i+=channels)
    {
        short cmix;
        short l_rearmix;
        short r_rearmix;
        short l_sidemix;
        short r_sidemix;
        short left;
        short right;
        //cmix = c*0.70703125
        cmix = ((buffer[i] * 23167) << 1) >> 16;
        //l_rearmix = l_rear*0.70703125
        l_rearmix = ((buffer[i+3] * 23167) << 1) >> 16;
        //r_rearmix = r_rear*0.70703125
        r_rearmix = ((buffer[i+4] * 23167) << 1) >> 16;
        //l_sidemix = l_side*0.70703125
        l_sidemix = ((buffer[i+5] * 23167) << 1) >> 16;
        //r_sidemix = r_side*0.70703125
        r_sidemix = ((buffer[i+6] * 23167) << 1) >> 16;

        // left = 0.320 (left + cmix + rearmix + sidemix) = 0.320*left
        //+ 0.2927*c + 0.2927*rear + 0.2265*sid
        left = ((10486*(buffer[i+1] + cmix + l_rearmix + l_sidemix)) << 1)
           >> 16;

        // right = 0.320 (right + cmix + rearmix + sidemix) = 0.320*right
        //+ 0.2265*c + 0.2265*rear + 0.2265*side
        right = ((10486*(buffer[i+2] + cmix + r_rearmix + r_sidemix)) << 1)
            >> 16;

        buffer[count++] = left;
        buffer[count++] = right;
    }
    }
}

 /* If dest_buffer is NULL then  src_buffer must be large enough to fill the upmixed data*/
static int upmix(int16_t *src_buffer, uint32_t src_ch_map, uint32_t src_ch,
    int16_t *dest_buffer, uint32_t dst_ch_map, uint32_t dest_ch,
    uint32_t framecount)
{
    uint32_t i , mask;
    uint32_t j, mask2;
    uint32_t copy_upmix_flag = 0;
    int16_t *p_dest_buffer;
    int16_t *p_src_buffer;
    uint32_t dest_buff_size;

    if (dest_buffer == NULL) {
        p_dest_buffer = dest_buffer = malloc(framecount*dest_ch*2);
        p_src_buffer = src_buffer;
        dest_buff_size = framecount*dest_ch*2;
        copy_upmix_flag = 1;
        if (!dest_buffer) {
            ALOGE("Failed to allocate upmix buffer");
            return -ENOMEM;
        }
    }

    memset((void*)dest_buffer, 0, dest_ch*2*framecount);

    if ((dst_ch_map & ch_mask[src_ch - 1]) == src_ch_map) {
        do {
            memcpy(dest_buffer, src_buffer, src_ch * sizeof(int16_t));
            src_buffer += src_ch;
            dest_buffer += dest_ch;
        } while (--framecount);
    }  else {
        int dstIndexTab[8] = {0};
        for (i = 0, mask = 0xF; i < src_ch; i++, mask <<= 4) {
            if ((src_ch_map & mask) == (dst_ch_map & mask)) {
                dstIndexTab[i] = i;
                continue;
            }
            for (j = 0, mask2 = 0xF; j < dest_ch; j++, mask2 <<= 4) {
                if (((src_ch_map & mask) >> (i << 2)) ==
                ((dst_ch_map & mask2) >> (j << 2))) {
                    dstIndexTab[i] = j;
                    break;
                }
            }
        }

        do {
            for (i = 0; i < src_ch; i++)
                dest_buffer[dstIndexTab[i]] = src_buffer[i];

            src_buffer += src_ch;
            dest_buffer += dest_ch;
        } while(--framecount);
    }

    if(copy_upmix_flag == 1) {
        memcpy(p_src_buffer, p_dest_buffer, dest_buff_size);
        free(p_dest_buffer);
    }

    return 0;
}

static void process_audio(struct audio_params *p_audio_params,
    size_t num_of_frames)
{
    uint32_t dwnmix_stereo_ch_map;
    uint32_t src_channels = p_audio_params->src_channels;
    uint32_t src_ch_map = p_audio_params->src_ch_map;
    int16_t* psrc_buffer = p_audio_params->psrc_buffer;
    uint32_t dest_channels = p_audio_params->dest_channels;
    uint32_t dest_ch_map = p_audio_params->dest_ch_map;
    int16_t* pdest_buffer = p_audio_params->pdest_buffer;
    uint32_t *vol = p_audio_params->vol;
    audio_format_t format = p_audio_params->format;

    if ((dest_channels == 1) &&
    (src_channels == 2)) {
        stereo_to_mono(psrc_buffer,
            pdest_buffer,
            num_of_frames);
    } else if((dest_channels == 2) &&
    (src_channels == 1)) {
        mono_to_stereo(psrc_buffer,
            pdest_buffer,
            num_of_frames);
    } else {
        if (src_channels > dest_channels) { /*downmix*/
            if (dest_channels == 2) {/*downmix to stereo*/
                dwnmix_stereo_ch_map =
                tegra_stereo_dwn_mix_ch_map[src_channels -
                1];

                if (src_ch_map != dwnmix_stereo_ch_map)
                    shuffle_channels(psrc_buffer,
                        num_of_frames*src_channels*2, src_channels,
                        src_ch_map, dwnmix_stereo_ch_map);

                downmix_to_stereo(psrc_buffer,
                    src_channels, num_of_frames);
            } else {
                downmix(psrc_buffer, src_ch_map,
                    src_channels, pdest_buffer,
                    dest_ch_map, dest_channels,
                    num_of_frames);
            }
        } else if (src_channels
        < dest_channels) {
            /*upmix*/
            upmix(psrc_buffer,
            src_ch_map,
            src_channels,
            psrc_buffer == pdest_buffer ? NULL : pdest_buffer,
            dest_ch_map, dest_channels, num_of_frames);
        } else if ((src_ch_map != dest_ch_map) &&
            (src_channels == dest_channels)) {
            /*shuffle*/
            shuffle_channels(psrc_buffer,
            num_of_frames*src_channels*2, src_channels,
            src_ch_map, dest_ch_map);
        }
    }

    if ((vol[0] != INT_Q15(1) ||
    vol[1] != INT_Q15(1)) &&
    (format == AUDIO_FORMAT_PCM_16_BIT))
        apply_volume(pdest_buffer, num_of_frames, p_audio_params);
}

static uint32_t get_ch_map(uint32_t channels)
{
    uint32_t ch_map = 0;
    char prop_val[PROPERTY_VALUE_MAX] = "";

    property_get("media.tegra.out.channel.map", prop_val, "");

    if ((!strcmp(prop_val, "")) || (channels <= 2)) {
        ch_map = tegra_default_ch_map[channels - 1];
    } else {
        char chan_id[PROPERTY_VALUE_MAX] = "";
        uint32_t prop_str_pos = 0, ch_map_pos = 0, i;
        uint32_t prop_val_len = strlen(prop_val);

        do {
            sscanf(&prop_val[prop_str_pos], "%s", chan_id);
            for (i = LF; i < TEGRA_AUDIO_CHANNEL_ID_LAST; i++) {
                if (!strcmp(chan_id, tegra_audio_chid_str[i])) {
                    ch_map |= (i << ch_map_pos);
                    ch_map_pos += 4;
                    prop_str_pos += (strlen(chan_id) + 1);
                    break;
                }
            }

            if (i == TEGRA_AUDIO_CHANNEL_ID_LAST) {
                ALOGE("Invalid channel map prop str %s\n", prop_val);
                return tegra_default_ch_map[channels - 1];
            }
        } while(prop_str_pos < prop_val_len);
    }

    return ch_map;
}

static uint32_t get_num_of_pcm_streams_to_mix(list_handle stream_list)
{
    int pcm_stream_count = 0;
    node_handle n;

    n = list_get_head(stream_list);
    while (n) {
        struct nvaudio_stream_out *out = list_get_node_data(n);
        n = list_get_next_node(n);
        if (!(is_non_pcm_format(out->format)) &&
                              (out->flags != AUDIO_OUTPUT_FLAG_FAST))
            pcm_stream_count++;
    }

    return pcm_stream_count;
}

/*****************************************************************************
*WFD Audio Functions
*****************************************************************************/
static int get_buffer(struct alsa_dev *dev, void** shareBuffer, size_t bytes)
{
    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer *)dev->pcm;
    uint32_t avail = 0;
    uint32_t u = hpcm->user;
    uint32_t s = hpcm->server;
    uint32_t bufSize = hpcm->size;
    uint8_t* sharedBufferBase = (uint8_t*)hpcm + hpcm->offset;
    uint32_t waitCount = 0;
    struct pcm_config *conf = &(dev->config);
    uint32_t latency;

    latency = (uint32_t)(((uint64_t)hpcm->size * 1000000) /
                    (conf->rate * alsa_dev_frame_size(dev)));

    while(1) {
        avail = hpcm->isEmpty ? bufSize :
                    ((s > u) ? (bufSize - s + u) : (u - s));
        avail = avail > bytes ? bytes : avail;

        if (avail == 0) {
            if (waitCount > conf->period_count) {
                ALOGV("WFD stack is not consuming data");
                return 0;
            }
            usleep(latency / conf->period_count);
            u = hpcm->user;
            waitCount++;
            continue;
        } else if ((s + avail) > bufSize) {
            *shareBuffer = &sharedBufferBase[s];
            return (bufSize - s);
        } else {
            *shareBuffer = &sharedBufferBase[s];
            return avail;
        }
    }
}

static void wfd_close(struct alsa_dev *dev)
{
    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer*)dev->pcm;
    uint8_t* sharedBufferBase = NULL;

    ALOGV("wfd_close");

    if (hpcm) {
        sharedBufferBase = (uint8_t*)hpcm + hpcm->offset;
        hpcm->serverState = NVCAP_AUDIO_STATE_STOP;
        memset(sharedBufferBase, 0, hpcm->size);
        hpcm->server = hpcm->user = 0;
        hpcm->isEmpty = 1;
        dev->pcm = 0;
        hpcm->timeStampStart = 0;
    }
}

static int wfd_open(struct alsa_dev *dev)
{
    NvCapAudioBuffer *hpcm = NULL;
    uint32_t  sampleRate, channels, bitsPerSample;
    struct pcm_config *conf;

    ALOGV("wfd_open");

    if (dev->pcm)
        return 0;

    hpcm = (NvCapAudioBuffer*)get_wfd_audio_buffer();

     if (hpcm == NULL)
         return -EINVAL;

    if (get_wfd_audio_params(&sampleRate, &channels,&bitsPerSample) < 0)
        return -EINVAL;

    conf = &(dev->config);
    conf->rate = sampleRate;
    conf->channels = channels;
    conf->format = (bitsPerSample == 16) ? PCM_FORMAT_S16_LE :
                    PCM_FORMAT_S32_LE;
    dev->pcm = (struct pcm *)hpcm;

    return 0;
}

static int wfd_write(struct alsa_dev *dev,
                     void* buffer, size_t bytes)
{
    //ALOGV("NvAudioWFDWrite %d bytes", bytes);

    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer *)dev->pcm;
    void *shareBuffer = NULL;
    uint32_t avail = 0;
    uint32_t bytesWritten = 0;
    uint32_t bytesToWrite = bytes;
    uint32_t s = 0;

    if (dev->pcm == NULL) {
        ALOGE("NvAudioALSAWfdWrite: Device has not opened yet\n");
        return -EINVAL;
    }

    s = hpcm->server;

    if (hpcm->serverState != NVCAP_AUDIO_STATE_RUN) {
        hpcm->timeStampStart = NvOsGetTimeUS();
        hpcm->serverState = NVCAP_AUDIO_STATE_RUN;
    }

    do {
        avail = get_buffer(dev, &shareBuffer, bytes);
        if (avail <= 0)
           return (bytesWritten != bytesToWrite ?  -EINVAL : 0);

        memcpy(shareBuffer, (uint8_t*)buffer + bytesWritten, avail);
        s += avail;
        s = (s >= hpcm->size) ? (s - hpcm->size) : s;
        bytes -= avail;
        bytesWritten += avail;
        avail = 0;
        hpcm->server = s;
        hpcm->isEmpty = 0;
    } while (bytes > 0);

    return (bytesWritten != bytesToWrite ?  -EINVAL : 0);
}

/* Call with device and out stream lock held */
static int open_out_alsa_dev(struct nvaudio_stream_out* out,
                             struct alsa_dev *dev)
{
    uint32_t old_rate = dev->config.rate;
    uint32_t id = dev->id;
    int ret = 0, channels = 0, reset_resampler = 0;
    uint32_t active_dev_rate = 0;
    char prop_val[PROPERTY_VALUE_MAX];
    char prop_dump_value[PROPERTY_VALUE_MAX] = "";
    static int file_count = 0;
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
    int err = 0;
    struct snd_aes_iec958  iec958;
#endif
    ALOGV("%s", __FUNCTION__);

    property_get(TEGRA_PLAYBACK_DUMP_PROPERTY, prop_dump_value,"0");
    if(atoi(prop_dump_value)){
        if(out->fdump == NULL) {
            char fileName[100];
            int char_written = snprintf(fileName, 100, DUMP_PLAYBACK_FILE_PATH);
            snprintf(fileName+char_written, 100-char_written,"%d.pcm", file_count++);
            out->fdump = fopen(fileName,"w+");
            if (out->fdump == NULL)
                ALOGD("Cannot create file for dumping for playback path");
        }
    }

    if (out->pending_route & dev->devices) {
        set_route(dev, out->pending_route, out->devices, &out->stream.common);
        out->devices = out->pending_route & dev->devices;
        out->pending_route &= ~dev->devices;
    }

    if (dev->pcm) {
        bool is_format_changed = false;

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        if (dev->id == DEV_ID_AUX) {
           if (((out->format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_IEC61937) &&
                 !dev->is_pass_thru) {
              is_format_changed = true;
              dev->is_pass_thru = true;
              ALOGV("Format changed to Compressed 0x%x", out->format);
           }
       }
#endif
        /* For compressed stream set HW sample rate same as compressed stream
           rate if compressed stream rate is supported by HW. To achieve that
           close ulp alsa-devices opened from all other active streams */
        if (dev->ulp_supported && is_ulp_supported(out->hw_dev) &&
            is_non_pcm_format(out->format) && (out->rate != dev->config.rate) &&
            !dev->use_avp_src && (audio_rate_mask(out->rate) & dev->hw_rates) &&
            !list_find_data(dev->active_stream_list, &out->stream.common)) {
            avp_audio_lock(out->hw_dev->avp_audio, 1);
            if (dev->avp_stream) {
                pthread_mutex_unlock(&dev->lock);
                avp_stream_stop(dev->avp_stream);
                pthread_mutex_lock(&dev->lock);
            }

            if (dev->fast_avp_stream) {
                pthread_mutex_unlock(&dev->lock);
                avp_stream_stop(dev->fast_avp_stream);
                pthread_mutex_lock(&dev->lock);
            }

            pcm_close(dev->pcm);
            dev->pcm = NULL;
            reset_resampler = 1;
        } else if(is_format_changed) {
            ALOGV(" Closing device for format change 0x%x", out->format);
            pcm_close(dev->pcm);
            dev->pcm = NULL;
        } else {
            if (!list_find_data(dev->active_stream_list,
                                   &out->stream.common)) {
                list_push_data(dev->active_stream_list, &out->stream.common);
                if (!out->resampler[id])
                    set_out_resampler(out, dev, RESAMPLER_QUALITY_DEFAULT);
                if (out->avp_stream) {
                    out->hw_dev->avp_active_stream = out->avp_stream;
                    if (out->hw_dev->seek_offset[0] |
                                out->hw_dev->seek_offset[1]) {
                        uint64_t offset =
                                ((uint64_t)out->hw_dev->seek_offset[1] << 32) |
                                out->hw_dev->seek_offset[0];
                        avp_stream_seek(out->avp_stream, offset);
                        out->hw_dev->seek_offset[0] =
                                out->hw_dev->seek_offset[1] = 0;
                    }
                }
            }
            return 0;
        }
    }

    dev->pcm_flags = PCM_OUT;
    memcpy(&dev->config, &dev->default_config, sizeof(struct pcm_config));

    active_dev_rate = get_active_dev_rate(out->hw_dev,
                                          dev->card,
                                          dev->device_id);
    if (active_dev_rate > 0) {
        if ((!(active_dev_rate % 11025) && (dev->config.rate % 11025)) ||
            (!(active_dev_rate % 4000) && (dev->config.rate % 4000))) {
                if (!(active_dev_rate % 11025) &&
                            (dev->hw_rates & RATE_44100))
                    dev->config.rate = 44100;
                else if (!(active_dev_rate % 4000) &&
                            (dev->hw_rates & RATE_48000))
                    dev->config.rate = 48000;
                else
                    dev->config.rate = active_dev_rate;
        }
    }

    if (dev->ulp_supported) {
        dev->config.period_size = AVP_PCM_CONFIG_PERIOD_SIZE;
        dev->config.period_count = AVP_PCM_CONFIG_PERIOD_COUNT;
        dev->config.start_threshold = AVP_PCM_CONFIG_START_THRESHOLD;
        dev->config.stop_threshold = AVP_PCM_CONFIG_STOP_THRESHOLD;

        /* For non-pcm streams set stream sample rate as HW sample rate */
        if (is_non_pcm_format(out->format) && !dev->use_avp_src &&
            (active_dev_rate <= 0) &&
            (audio_rate_mask(out->rate) & dev->hw_rates))
            dev->config.rate = out->rate;
    }
#ifdef NVOICE
    if ((nvoice_supported) &&
       (out->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
        if (out->avp_stream) {
            reset_resampler = 1;
        } else {
            if (out->resampler[id] ) {
            release_resampler(out->resampler[id]);
            out->resampler[id] = NULL;
            }
        }
         if(!dev->pcm_out_buf) {
            dev->pcm_out_buf = (int16_t*)malloc(4096);
            if (!dev->pcm_out_buf) {
                ALOGE("Failed to allocate memory for pcm buffer");
                return -ENOMEM;
            }
         }
         if(!dev->pcm_out_avp_buf) {
            dev->pcm_out_avp_buf = (int16_t*)malloc(4096);
            if (!dev->pcm_out_avp_buf) {
                ALOGE("Failed to allocate memory for pcm buffer(avp)");
                return -ENOMEM;
            }
         }
        out_stream_open = 1;
    }
#endif

    if (dev->id != DEV_ID_WFD) {
        if (dev->id == DEV_ID_AUX) {
            channels = get_max_aux_channels(dev);
            dev->config.channels = channels;
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
            ALOGV(" out->format  0x%x  \n",out->format);
            if ((out->format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_IEC61937) {
                 dev->config.channels = 2;
                 if(audio_rate_mask(out->rate) & dev->hw_rates) {
                     dev->config.rate = out->rate;
                 } else {
                     ret = -EINVAL;
                     goto err_exit;
                 }
            }
#endif
            if (out->hw_dev->low_latency_mode &&
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
               ((out->format & AUDIO_FORMAT_MAIN_MASK) != AUDIO_FORMAT_IEC61937) &&
#endif
                (channels == 2)) {
                dev->config.period_size = AUX_LOW_LATENCY_PERIOD_SIZE;
                dev->config.period_count = AUX_LOW_LATENCY_PERIODS;
                dev->config.start_threshold = AUX_LOW_LATENCY_PERIOD_SIZE;
                dev->config.stop_threshold = AUX_LOW_LATENCY_PERIOD_SIZE *
                                 AUX_LOW_LATENCY_PERIODS;
            }
        }
        /* Ensure output is routed to proper device */
        set_route(dev, out->devices, out->devices, &out->stream.common);

        dev->pcm = pcm_open(dev->card,
                            dev->device_id,
                            dev->pcm_flags,
                            &dev->config);

        if (dev->ulp_supported)
            dev->config.period_size =
            dev->default_config.period_size;

        if (!pcm_is_ready(dev->pcm)) {
            ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
            dev->pcm = NULL;
            ret = -ENODEV;
            goto err_exit;
        } else if (dev->id == DEV_ID_AUX) {
             // AUX is plugged in
             struct mixer_ctl *ctl;

             if (channels != get_max_aux_channels(dev) ) {
                ALOGE("max channels is wrong for aux pcm device");
                dev->pcm = NULL;
                ret = -ENODEV;
                goto err_exit;
            }
            ctl = mixer_get_ctl_by_name (dev->mixer,"HDA Decode Capability");
            dev->rec_dec_capable = mixer_ctl_get_value(ctl, 0);

            ALOGV("dev->rec_dec_capable: 0x%x", dev->rec_dec_capable);

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
            if (out->format == AUDIO_FORMAT_PCM_16_BIT) {
                ctl = mixer_get_ctl_by_name
                      (dev->mixer, "IEC958 Playback Default");
                err = mixer_ctl_get_array(ctl, (void*)&iec958, 1);
                if (err < 0) {
                    ALOGE("Channel Status bit get has failed\n");
                }
                iec958.status[0] &= ~IEC958_AES0_NONAUDIO;
                err = mixer_ctl_set_array(ctl, (void*)&iec958, 1);
                if (err < 0) {
                    ALOGE("Channel Status bit set has failed\n");
                }
            } else if ((out->format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_IEC61937) {
                ctl = mixer_get_ctl_by_name
                      (dev->mixer,"IEC958 Playback Default");
                err = mixer_ctl_get_array(ctl, (void*)&iec958, 1);
                if (err < 0) {
                    ALOGV("Channel Status bit get has failed\n");
                }
                iec958.status[0] |= IEC958_AES0_NONAUDIO;
                err = mixer_ctl_set_array(ctl, (void*)&iec958, 1);
                if (err < 0) {
                    ALOGE("Channel Status bit set has failed\n");
                }
            }
#endif
        }
    } else {
        if (wfd_open(dev) < 0) {
            ALOGE("cannot open wfd device\n");
            return -ENODEV;
        }
    }
    ALOGV("Opened Alsa OUT device %s with pcm_config\n channels %d\n rate %d\n"
         " format %d\n period_size %d\n period_count %d\n start_threshold %d\n"
         " stop_threshold %d \nsilence_threshold %d\n avail_min %d\n",
         alsa_dev_names[id].name, dev->config.channels, dev->config.rate,
         dev->config.format, dev->config.period_size, dev->config.period_count,
         dev->config.start_threshold, dev->config.stop_threshold,
         dev->config.silence_threshold, dev->config.avail_min);

    /* Setup ALSA for AVP rendering */
    if (dev->ulp_supported) {
        ret = set_avp_render_path(dev, out->hw_dev->avp_audio);
        if (ret < 0) {
            ALOGE("Failed to set AVP render path");
            goto err_exit;
        }
    }
#ifdef NVAUDIOFX
	if (out->devices & AUDIO_DEVICE_OUT_SPEAKER)
		nvaudiofx_set_rate(dev->config.rate);
#endif

    if (out->resampler[id] && ((old_rate != dev->config.rate) ||
                               (out->rate == dev->config.rate))) {
        release_resampler(out->resampler[id]);
        out->resampler[id] = NULL;
    }

    if (!out->resampler[id])
        set_out_resampler(out, dev, RESAMPLER_QUALITY_DEFAULT);

    set_current_route(dev);

    if (out->avp_stream) {
        out->hw_dev->avp_active_stream = out->avp_stream;
        if (out->hw_dev->seek_offset[0] | out->hw_dev->seek_offset[1]) {
            uint64_t offset = ((uint64_t)out->hw_dev->seek_offset[1] << 32 ) |
                                         out->hw_dev->seek_offset[0];
            avp_stream_seek(out->avp_stream, offset);
            out->hw_dev->seek_offset[0] = out->hw_dev->seek_offset[1] = 0;
        }

        if (reset_resampler) {
            node_handle n;

            n = list_get_head(dev->active_stream_list);
            while (n) {
                struct nvaudio_stream_out *out_iter = list_get_node_data(n);

                n = list_get_next_node(n);
                if (is_non_pcm_format(out_iter->format))
                    continue;

                if (out_iter->resampler[id]) {
                    release_resampler(out_iter->resampler[id]);
                    out_iter->resampler[id] = NULL;
                }

                if (!out_iter->resampler[id])
                    set_out_resampler(out_iter, dev, RESAMPLER_QUALITY_DEFAULT);
            }
            avp_audio_lock(out->hw_dev->avp_audio, 0);
        }
    } else if (reset_resampler) {
        avp_audio_lock(out->hw_dev->avp_audio, 0);
    }

    /* Check is required to handle playback during voice call */
    if (!list_find_data(dev->active_stream_list, &out->stream.common))
        list_push_data(dev->active_stream_list, &out->stream.common);

    return 0;

err_exit:
    if (dev->pcm) {
        pcm_close(dev->pcm);
        dev->pcm = NULL;
    }

    return ret;
}

/* Call with device and in stream lock held */
static int open_in_alsa_dev(struct nvaudio_stream_in* in)
{
    struct alsa_dev *dev = in->dev;
    uint32_t old_rate = dev->config.rate;
    uint32_t active_dev_rate = 0;
    char prop_dump_value[PROPERTY_VALUE_MAX] = "";

    ALOGV("%s", __FUNCTION__);

    if ((in->source == AUDIO_SOURCE_CAMCORDER) &&
        (in->hw_dev->mode == AUDIO_MODE_IN_CALL))
            return -ENODEV;

    if (dev->pcm)
        goto exit;

    dev->pcm_flags = PCM_IN;
    memcpy(&dev->config, &dev->default_config, sizeof(struct pcm_config));

    active_dev_rate = get_active_dev_rate(in->hw_dev,
                                          dev->card,
                                          dev->device_id);
    if (active_dev_rate > 0) {
        if ((!(active_dev_rate % 11025) && (dev->config.rate % 11025)) ||
            (!(active_dev_rate % 4000) && (dev->config.rate % 8000)))
                dev->config.rate = active_dev_rate;
    }

    if (in->hw_dev->mode == AUDIO_MODE_IN_CALL) {
        if(dev->call_capture_rate && (DEV_ID_MUSIC == dev->id))
           dev->config.rate = dev->call_capture_rate;
    }

#ifdef NVOICE
    if ((nvoice_supported) &&
       (in->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) &&
       ((in->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) == 0)) {
        if(out_stream_open == 0)
            return -ENODEV;
    }
#endif

    dev->pcm = pcm_open(dev->card,
                        dev->device_id,
                        dev->pcm_flags,
                        &dev->config);
    if (!pcm_is_ready(dev->pcm)) {
        ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
        dev->pcm = NULL;
        return -ENODEV;
    }
    ALOGV("Opened Alsa IN device %s with pcm_config\n channels %d\n rate %d\n"
         " format %d\n period_size %d\n period_count %d\n start_threshold %d\n"
         " stop_threshold %d \nsilence_threshold %d\n avail_min %d\n",
         alsa_dev_names[dev->id].name, dev->config.channels, dev->config.rate,
         dev->config.format, dev->config.period_size, dev->config.period_count,
         dev->config.start_threshold, dev->config.stop_threshold,
         dev->config.silence_threshold, dev->config.avail_min);

    if (in->resampler && (old_rate != dev->config.rate)) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

    if (!in->resampler || (in->channel_count != dev->config.channels))
        set_in_resampler(in, RESAMPLER_QUALITY_DEFAULT);

exit:
    if (!list_find_data(dev->active_stream_list, &in->stream.common))
        list_push_data(dev->active_stream_list, &in->stream.common);

    property_get(TEGRA_RECORD_DUMP_PROPERTY, prop_dump_value, "0");
    if(atoi(prop_dump_value)){
        if(in->fdump  == NULL) {
            in->fdump  = fopen(DUMP_RECORD_FILE_PATH,"a+");
            if (in->fdump  == NULL)
                ALOGD("Cannot create file for dumping for record path");
        }
    }

    return 0;
}

/* Call with device lock and stream lock held */
static void close_alsa_dev(struct audio_stream *stream, struct alsa_dev *dev)
{
    uint32_t  i;
    struct nvaudio_hw_device *hw_dev =
        ((struct nvaudio_stream_out*)stream)->hw_dev;
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out*)stream;

    ALOGV("%s : active stream count %d", __FUNCTION__,
    list_get_node_count(dev->active_stream_list));

    if (!list_find_data(dev->active_stream_list, stream)) {
        ALOGE("Stream is not in active stream list");
        return;
    }

    if (dev->pcm) {
        if (!(dev->pcm_flags & PCM_IN)) {
            if (out->avp_stream || dev->avp_stream || dev->fast_avp_stream) {
                avp_stream_handle_t stream = out->avp_stream ?
                out->avp_stream : ((out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                dev->fast_avp_stream : dev->avp_stream);

                /* Release device and out stream mutex before calling avp stop
                   since avp stop may take long time for compressed stream and
                   we should not block other streams using same device during
                   this period */
                pthread_mutex_unlock(&dev->lock);
                pthread_mutex_unlock(&out->lock);
                avp_stream_stop(stream);
                pthread_mutex_lock(&out->lock);
                pthread_mutex_lock(&dev->lock);
            }
        }
        list_delete_data(dev->active_stream_list, stream);

        if (!list_get_node_count(dev->active_stream_list)) {
            if (dev->id != DEV_ID_WFD) {
                /*close the aux device only if it is disconnected*/
                if ((dev->id != DEV_ID_AUX) ||
                    !( hw_dev->dev_available & AUDIO_DEVICE_OUT_AUX_DIGITAL)
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
                    || (dev->is_pass_thru == true)
#endif
                    ) {
                    /*if (dev->id == DEV_ID_AUX)
                        ALOGV("@@@Close the AUX device\n");*/
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
                    dev->is_pass_thru = false;
#endif
                    pcm_close(dev->pcm);
                    dev->pcm = 0;
                } else {
                    /*flush the pcm buffer*/
                    memset(dev->mix_buffer, 0, dev->mix_buff_size);
                    pthread_mutex_unlock(&dev->lock);
                    pthread_mutex_unlock(&out->lock);
                    for (i = 0; i < dev->config.period_count; i++)
                        nvaudio_pcm_write(dev, (void*)dev->mix_buffer,
                        dev->mix_buff_size, out->devices);
                    pthread_mutex_lock(&out->lock);
                    pthread_mutex_lock(&dev->lock);
                    /*ALOGV("@@@Do Not close the AUX device\n");*/
                }
            } else {
                wfd_close(dev);
                dev->pcm = 0;
            }
        } else if ((is_non_pcm_format(out->format)) && (dev->is_pass_thru)) {
            dev->is_pass_thru = false;
            pcm_close(dev->pcm);
            dev->pcm = 0;
        }
    }
#ifdef NVOICE
    if (nvoice_supported) {
        if (dev->pcm_out_buf) {
            free(dev->pcm_out_buf);
            dev->pcm_out_buf = NULL;
        }
        if (dev->pcm_out_avp_buf) {
            free(dev->pcm_out_avp_buf);
            dev->pcm_out_avp_buf = NULL;
        }
    }
#endif
}

static int open_call_alsa_dev(struct alsa_dev *dev,  uint32_t pcm_flags)
{
    ALOGV("%s", __FUNCTION__);

    dev->pcm = pcm_open(dev->card,
                                                 dev->device_id,
                                                 pcm_flags,
                                                 &dev->config);

    if (!pcm_is_ready(dev->pcm)) {
        ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
        ALOGE("cannot open pcm: card=%d id=%d flags=%x", dev->card,dev->device_id,pcm_flags);
        dev->pcm = NULL;
        return -ENODEV;
    }

    ALOGV("Opened Alsa %s device %s with pcm_config\n channels %d\n rate %d\n"
         " format %d\n period_size %d\n period_count %d\n start_threshold %d\n"
         " stop_threshold %d \nsilence_threshold %d\n avail_min %d\n",
         pcm_flags == PCM_OUT ? "OUT" : "IN",
         alsa_dev_names[dev->id].name, dev->config.channels, dev->config.rate,
         dev->config.format, dev->config.period_size, dev->config.period_count,
         dev->config.start_threshold, dev->config.stop_threshold,
         dev->config.silence_threshold, dev->config.avail_min);

    pcm_start(dev->pcm);

    return 0;
}

static int close_call_alsa_dev(struct alsa_dev *dev)
{
    ALOGV("%s", __FUNCTION__);

    if (dev->pcm) {
        pcm_stop(dev->pcm);
        pcm_close(dev->pcm);
        dev->pcm = NULL;
    }

    return 0;
}

/*For Voice Call, all functions are called without holding the locks, as 2 voice call streams can never be opened simultaneously*/
static int set_call_route(struct nvaudio_hw_device *hw_dev,
        uint32_t new_devices, uint32_t old_devices)
{
    uint32_t call_devices = 0;
    uint32_t call_devices_prev = 0;
    uint32_t call_devices_cap = 0;
    uint32_t call_devices_cap_prev = 0;
    node_handle n;
    struct alsa_dev *alsa_call_pb_dev = NULL;
    struct alsa_dev *alsa_call_cap_dev = NULL;
    struct alsa_dev *alsa_call_pb_dev_prev = NULL;
    struct alsa_dev *alsa_call_cap_dev_prev = NULL;

    ALOGV("%s", __FUNCTION__);

    call_devices = new_devices & AUDIO_DEVICE_OUT_ALL;

    if (call_devices & AUDIO_DEVICE_OUT_ALL_SCO)
        call_devices_cap |= AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
    else if (call_devices & AUDIO_DEVICE_OUT_WIRED_HEADSET)
        call_devices_cap |= AUDIO_DEVICE_IN_WIRED_HEADSET;
    else if (call_devices & AUDIO_DEVICE_OUT_EARPIECE)
        call_devices_cap |= AUDIO_DEVICE_IN_BUILTIN_MIC |
                                         AUDIO_DEVICE_IN_BACK_MIC;
    else /*spk or default device*/
        call_devices_cap |= AUDIO_DEVICE_IN_BUILTIN_MIC;


    if (hw_dev->mic_mute)
        call_devices_cap = 0;
    else
        call_devices_cap &= ~AUDIO_DEVICE_BIT_IN;

    if ((hw_dev->call_devices == 0) && (hw_dev->call_devices_capture == 0)) {
        call_devices_prev = call_devices;
        call_devices_cap_prev = call_devices_cap;
    } else {
        call_devices_prev =  hw_dev->call_devices;
        call_devices_cap_prev = hw_dev->call_devices_capture;
    }

    n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);

        if (dev->devices & AUDIO_DEVICE_BIT_IN) {
            if (dev->devices & call_devices_cap)
                alsa_call_cap_dev = dev;
            if (dev->devices & call_devices_cap_prev)
                alsa_call_cap_dev_prev = dev;
        } else {
            if (dev->devices & call_devices)
                alsa_call_pb_dev = dev;
            if (dev->devices & call_devices_prev)
                alsa_call_pb_dev_prev = dev;
        }

        n = list_get_next_node(n);
    }

    if (!alsa_call_pb_dev || !alsa_call_cap_dev) {
        ALOGW("Ignore call routing for null playback or capture device");
        return 0;
    }

    if ((alsa_call_pb_dev_prev->id == alsa_call_pb_dev->id)
           && (alsa_call_cap_dev_prev->id == alsa_call_cap_dev->id)) {

        if (!(alsa_call_pb_dev->pcm))
            open_call_alsa_dev(alsa_call_pb_dev, PCM_OUT);

        if (!(alsa_call_cap_dev->pcm))
            open_call_alsa_dev(alsa_call_cap_dev, PCM_IN);

        if ((alsa_call_pb_dev->mode != AUDIO_MODE_IN_CALL)
               && (alsa_call_cap_dev->mode != AUDIO_MODE_IN_CALL)) {
           alsa_call_pb_dev->mode = AUDIO_MODE_IN_CALL;
           alsa_call_cap_dev->mode = AUDIO_MODE_IN_CALL;
           set_route(alsa_call_pb_dev, call_devices, 0, NULL);
           set_route(alsa_call_cap_dev, call_devices_cap, 0, NULL);
        } else {
            set_route(alsa_call_pb_dev, call_devices, call_devices_prev, NULL);
            set_route(alsa_call_cap_dev, call_devices_cap,
                               call_devices_cap_prev, NULL);
        }
    } else /*BT Call <-> Normal Call Switching*/ {
            alsa_call_pb_dev_prev->mode = AUDIO_MODE_NORMAL;
            alsa_call_cap_dev_prev->mode = AUDIO_MODE_NORMAL;

            if (alsa_call_pb_dev_prev->pcm)
                close_call_alsa_dev(alsa_call_pb_dev_prev);

            if (alsa_call_cap_dev_prev->pcm)
                close_call_alsa_dev(alsa_call_cap_dev_prev);

            set_route(alsa_call_pb_dev_prev, 0, call_devices_prev, NULL);
            set_route(alsa_call_cap_dev_prev, 0, call_devices_cap_prev, NULL);

            if (!(alsa_call_pb_dev->pcm))
                open_call_alsa_dev(alsa_call_pb_dev, PCM_OUT);

            if (!(alsa_call_cap_dev->pcm))
                open_call_alsa_dev(alsa_call_cap_dev, PCM_IN);

            alsa_call_pb_dev->mode = AUDIO_MODE_IN_CALL;
            alsa_call_cap_dev->mode = AUDIO_MODE_IN_CALL;
            set_route(alsa_call_pb_dev, call_devices, 0, NULL);
            set_route(alsa_call_cap_dev, call_devices_cap, 0, NULL);
    }

    hw_dev->call_devices = call_devices ;
    hw_dev->call_devices_capture = call_devices_cap;

    return 0;
}

/* Call this function with device lock held*/
static void restart_avp_audio(struct alsa_dev* dev, bool is_avp_src,
                              bool is_i2s_master)
{
    node_handle n;
    struct nvaudio_stream_out *out_avp = NULL, *out_pcm = NULL, *out_fast = NULL;
    struct nvaudio_hw_device *hw_dev = NULL;
    struct mixer_ctl *ctl = NULL;
    int ret;

    ALOGV("%s : avp_src %d i2s_master %d",
          __FUNCTION__, is_avp_src, is_i2s_master);

    if (!dev->ulp_supported || ((dev->use_avp_src == is_avp_src) &&
                                (dev->is_i2s_master == is_i2s_master)))
        return;

    ctl = mixer_get_ctl_by_name(dev->mixer, "Set I2S Master");
    if (!ctl)
        ALOGV("Failed to get mixer ctl to Set I2S Master");

    n = list_get_head(dev->active_stream_list);
    while (n) {
        struct nvaudio_stream_out *out = list_get_node_data(n);

        n = list_get_next_node(n);
        if (!out_avp && out->avp_stream)
            out_avp = out;
        else if (out->flags == AUDIO_OUTPUT_FLAG_FAST)
            out_fast = out;
        else if (!out_pcm && !out->avp_stream)
            out_pcm = out;
        if (!hw_dev)
            hw_dev = out->hw_dev;
    }

    if (hw_dev)
        avp_audio_lock(hw_dev->avp_audio, 1);

    if (dev->pcm) {
        pthread_mutex_unlock(&dev->lock);
        if (out_avp) {
            avp_stream_pause(out_avp->avp_stream, 1);
            avp_stream_stop(out_avp->avp_stream);
        }

        if (out_fast && dev->fast_avp_stream )
            avp_stream_stop(dev->fast_avp_stream);

        if (out_pcm && dev->avp_stream)
            avp_stream_stop(dev->avp_stream);

        pthread_mutex_lock(&dev->lock);
        /* Check validity of dev->pcm again as it could have already been
           closed by some other thread when we released the mutex */
        if (dev->pcm) {
            pcm_close(dev->pcm);
            dev->pcm = NULL;
        }
    }
    dev->use_avp_src = is_avp_src;
    if (ctl && (dev->is_i2s_master != is_i2s_master)) {
        ret = mixer_ctl_set_value(ctl, 0, is_i2s_master);
        if (ret < 0)
            ALOGE("Failed to set mixer ctl Set I2S Master.err %d", ret);
        else
            dev->is_i2s_master = is_i2s_master;
    }

    if (out_avp || out_pcm || out_fast) {
        if (out_avp && !dev->use_avp_src &&
            (audio_rate_mask(out_avp->rate) & dev->hw_rates))
            dev->config.rate = out_avp->rate;
        else
            dev->config.rate = dev->default_config.rate;
        dev->config.period_size = AVP_PCM_CONFIG_PERIOD_SIZE;
        dev->pcm = pcm_open(dev->card,
                            dev->device_id,
                            dev->pcm_flags,
                            &dev->config);
        if (!pcm_is_ready(dev->pcm)) {
            ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
            dev->pcm = NULL;
            return;
        }
        dev->config.period_size = dev->default_config.period_size;

        ALOGV("Opened Alsa OUT device %s with pcm_config\n channels %d\n"
              " rate %d\n format %d\n period_size %d\n period_count %d\n"
              " start_threshold %d\n stop_threshold %d \n"
              " silence_threshold %d\n avail_min %d\n",
              alsa_dev_names[dev->id].name, dev->config.channels,
              dev->config.rate, dev->config.format, dev->config.period_size,
              dev->config.period_count, dev->config.start_threshold,
              dev->config.stop_threshold, dev->config.silence_threshold,
              dev->config.avail_min);

        if (hw_dev) {
            /* Setup ALSA for AVP rendering */
            ret = set_avp_render_path(dev, hw_dev->avp_audio);
            if (ret < 0) {
                ALOGE("Failed to set AVP render path");
                return;
            }
        }
        pthread_mutex_unlock(&dev->lock);
        if (out_avp)
            nvaudio_avp_stream_write(out_avp->avp_stream, NULL, 0, 0, 0, out_avp, dev);

        if (dev->fast_avp_stream && out_fast)
            nvaudio_avp_stream_write(dev->fast_avp_stream, NULL, 0, 0, 0, out_fast, dev);

        if (out_pcm && dev->avp_stream)
            nvaudio_avp_stream_write(dev->avp_stream, NULL, 0, 0, 0, out_pcm, dev);
        pthread_mutex_lock(&dev->lock);
    }

    if (hw_dev)
        avp_audio_lock(hw_dev->avp_audio, 0);
}

/*****************************************************************************
 *****************************************************************************
 * NVAUDIO_STREAM_OUT Interface
 *****************************************************************************
 *****************************************************************************/

static uint32_t nvaudio_out_get_sample_rate(const struct audio_stream *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    // ALOGV("%s : rate %d", __FUNCTION__, out->rate);

    return out->rate;
}

static int nvaudio_out_set_sample_rate(struct audio_stream *stream,
                                       uint32_t rate)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    node_handle n;
    int ret = 0;

    ALOGV("%s : rate %d", __FUNCTION__, rate);

    pthread_mutex_lock(&out->lock);
    if (out->rate == rate)
        goto exit;

    out->rate = rate;
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
    if ((is_non_pcm_format(out->format)) &&
        ((out->format & AUDIO_FORMAT_MAIN_MASK) != AUDIO_FORMAT_IEC61937))
#else
    if (is_non_pcm_format(out->format))
#endif
    {
        if (out->avp_stream) {
            avp_stream_set_rate(out->avp_stream, rate);
            goto exit;
        } else {
            ret = -EINVAL;
            goto exit;
        }
    }

    n = list_get_head(out->dev_list);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);
        uint32_t id = dev->id;

        n = list_get_next_node(n);
        pthread_mutex_lock(&dev->lock);

        if (out->resampler[id]) {
            release_resampler(out->resampler[id]);
            out->resampler[id] = NULL;
        }
        set_out_resampler(out, dev, RESAMPLER_QUALITY_DEFAULT);
        pthread_mutex_unlock(&dev->lock);
    }

exit:
    pthread_mutex_unlock(&out->lock);
    return ret;
}

static size_t nvaudio_out_get_buffer_size(const struct audio_stream *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    node_handle n = list_get_head(out->dev_list);
    size_t buffer_size = 0;

    if (out->avp_stream) {
            buffer_size = NON_PCM_INPUT_BUF_SIZE;
    } else {
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);
            size_t temp;

            n = list_get_next_node(n);

            temp = (dev->config.period_size * out->rate) / dev->config.rate;
            if (!temp)
                continue;

            buffer_size = buffer_size ? MIN(buffer_size, temp) : temp;
        }
    }

    buffer_size = ((buffer_size + 15) / 16) * 16;
    buffer_size *= audio_stream_frame_size(&out->stream.common);

    ALOGV("%s: size %d", __FUNCTION__, buffer_size);
    return buffer_size;
}

static uint32_t nvaudio_out_get_channels(const struct audio_stream *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    // ALOGV("%s : channel %x", __FUNCTION__, out->channels);

    return out->channels;
}

static audio_format_t nvaudio_out_get_format(const struct audio_stream *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    // ALOGV("%s : format %x", __FUNCTION__, out->format);

    return out->format;
}

static int nvaudio_out_set_format(struct audio_stream *stream, audio_format_t format)
{
    ALOGV("%s : %d", __FUNCTION__, format);

    /* TODO : Not supported at this moment */

    return 0;
}

static int nvaudio_out_standby(struct audio_stream *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    struct nvaudio_hw_device *hw_dev = out->hw_dev;
    node_handle n = list_get_head(out->dev_list);

    ALOGV("%s", __FUNCTION__);

    if (out->standby)
        return 0;

    pthread_mutex_lock(&out->lock);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);
        uint32_t id = dev->id;

        n = list_get_next_node(n);
        pthread_mutex_lock(&dev->lock);

        if (hw_dev->mode != AUDIO_MODE_IN_CALL) {
            close_alsa_dev(stream, dev);
        } else {
            if (out->avp_stream || dev->avp_stream || dev->fast_avp_stream) {
                avp_stream_handle_t stream = out->avp_stream ?
                out->avp_stream : ((out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                              dev->fast_avp_stream : dev->avp_stream);

                pthread_mutex_unlock(&dev->lock);
                pthread_mutex_unlock(&out->lock);
                avp_stream_stop(stream);
                pthread_mutex_lock(&out->lock);
                pthread_mutex_lock(&dev->lock);
            }
        }

        if (out->resampler[id]) {
            int i;

            out->resampler[id]->reset(out->resampler[id]);
            for (i = 0; i < DEV_ID_NON_CALL_MAX; i++)
                out->buff_pos[i] = 0;
        }
        pthread_mutex_unlock(&dev->lock);
    }
    out->standby = true;
    out->standby_frame_count += out->frame_count;
    out->frame_count = 0;
    if(out->fdump){
        fclose(out->fdump);
        out->fdump = NULL;
    }

    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int nvaudio_out_dump(const struct audio_stream *stream, int fd)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

static int nvaudio_out_set_parameters(struct audio_stream *stream,
                                      const char *kvpairs)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    struct nvaudio_hw_device *hw_dev = out->hw_dev;
    struct str_parms *parms;
    node_handle n;
    char value[32];
    unsigned int key_val;
    int ret = 0;

    ALOGV("%s: %s", __FUNCTION__, kvpairs);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    if (ret >= 0) {
        uint32_t new_devices = atoi(value);

        ALOGV("Switch audio to device :0x%x -> 0x%x stream %d\n",
              out->devices, new_devices, out->io_handle);

        pthread_mutex_lock(&hw_dev->lock);
        pthread_mutex_lock(&out->lock);
#ifdef NVOICE
        if(nvoice_supported) {
            nvoice_setoutdevice(new_devices);
            out->nvoice_stopped = false;
        }
#endif
        new_devices &= AUDIO_DEVICE_OUT_ALL;

        if (new_devices) {
            out->pending_route = 0;
            if (out->avp_stream && ((new_devices & ~NVAUDIO_ULP_OUT_DEVICES) ||
                (hw_dev->mode == AUDIO_MODE_IN_CALL)))
                avp_audio_set_ulp(hw_dev->avp_audio, 0);

            n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
            while (n) {
                struct alsa_dev *dev = list_get_node_data(n);

                n = list_get_next_node(n);
                pthread_mutex_lock(&dev->lock);

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
                if (dev->id == DEV_ID_AUX) {
                    if ((new_devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) &&
                        (dev->pcm != NULL)){
                        struct mixer_ctl *ctl;

                        ctl = mixer_get_ctl_by_name
                              (dev->mixer, "HDA Decode Capability");
                        dev->rec_dec_capable = mixer_ctl_get_value(ctl, 0);

                        ALOGV("dev->rec_dec_capable:0x%x",dev->rec_dec_capable);

                    } else if (!(new_devices & AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
                        dev->rec_dec_capable = 0;
                    }
                }
#endif
                if ((dev->devices & new_devices) &&
                    !(dev->devices & out->devices)) {
                    if (hw_dev->mode != AUDIO_MODE_IN_CALL)
                        set_route(dev, new_devices, 0, &out->stream.common);
                    if (!list_find_data(out->dev_list, dev))
                        list_push_data(out->dev_list, dev);
                    if (!out->standby)
                        open_out_alsa_dev(out, dev);
                } else if (!(dev->devices & new_devices) &&
                            (dev->devices & out->devices)) {
                    list_delete_data(out->dev_list, dev);
                    if (!out->standby)
                        close_alsa_dev(stream, dev);
                    if (hw_dev->mode != AUDIO_MODE_IN_CALL)
                        set_route(dev, 0, out->devices, &out->stream.common);
                } else if ((hw_dev->mode != AUDIO_MODE_IN_CALL) ||
                           ((hw_dev->mode == AUDIO_MODE_IN_CALL) &&
                           (hw_dev->call_devices == 0) &&
                           (hw_dev->call_devices_capture == 0)))
                    set_route(dev, new_devices, out->devices, &out->stream.common);
                pthread_mutex_unlock(&dev->lock);
            }

            if (hw_dev->mode == AUDIO_MODE_IN_CALL) {
                n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
                while (n) {
                    struct alsa_dev *dev = list_get_node_data(n);

                    if (dev->id == DEV_ID_MUSIC && !dev->pcm) {
                        ret = open_out_alsa_dev(out, dev);
                        if (!list_find_data(out->dev_list, dev))
                           list_push_data(out->dev_list, dev);
                        break;
                    }
                    n = list_get_next_node(n);
                }
                set_call_route(hw_dev, new_devices, out->devices);
            }

            out->devices = new_devices;
            if (hw_dev->avp_audio)
                avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));
        }
        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_FORMAT,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set format to %d\n", key_val);

        if (out->format != (audio_format_t)key_val)
            nvaudio_out_set_format(&out->stream.common, key_val);

    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_CHANNELS,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set channels to %d\n", key_val);

        pthread_mutex_lock(&out->lock);
        if (out->channels != key_val) {
            n = list_get_head(out->dev_list);
            out->channels = key_val;

            while (n) {
                struct alsa_dev *dev = list_get_node_data(n);
                uint32_t id = dev->id;

                pthread_mutex_lock(&dev->lock);
                if (out->resampler[id]) {
                    release_resampler(out->resampler[id]);
                    out->resampler[id] = NULL;
                }
                set_out_resampler(out, dev, RESAMPLER_QUALITY_DEFAULT);
                pthread_mutex_unlock(&dev->lock);

                n = list_get_next_node(n);
            }
        }
        pthread_mutex_unlock(&out->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_FRAME_COUNT,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set frame count to %d\n", key_val);

        /* TODO : not sure what should be done here */
    }

    ret = str_parms_get_str(parms, "closing",
                            value, sizeof(value));
    if (ret >= 0) {
        ALOGV("closing %s %d", value,strcmp(value, "true"));
        if (!strcmp(value, "true")) {
            pthread_mutex_lock(&out->lock);
            if (!out->standby) {
                if (out->avp_stream)
                    avp_stream_pause(out->avp_stream, 1);
            }
            pthread_mutex_unlock(&out->lock);
        }
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_OUTPUT_AVAILABLE,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Available output device :0x%x\n", key_val);

        /* TODO : needed to send silence data to HDMI */
    }
#ifdef NVAUDIOFX
    ret = str_parms_get_str(parms, "fx_volume",
                            value, sizeof(value));
    if (ret >= 0) {
         fx_vol= atof(value);
        ALOGV("Set fx volume to %f\n", fx_vol);
    }

    ret = str_parms_get_str(parms, "mdrc_enable",
                            value, sizeof(value));
    if (ret >= 0) {
         key_val = atoi(value);
         nvaudiofx_enable_mdrc(key_val);
        ALOGV("mdrc %d\n", key_val);
    }

#endif
    str_parms_destroy(parms);
    return ret;
}

static char * nvaudio_out_get_parameters(const struct audio_stream *stream,
                                         const char *keys)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    char tmp[80];

    ALOGV("%s : %s", __FUNCTION__, keys);

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_ROUTING)) {
        sprintf(tmp, "%d", out->devices);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_FORMAT)) {
        sprintf(tmp, "%d", out->format);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_CHANNELS)) {
        sprintf(tmp, "%d", out->channels);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_FRAME_COUNT)) {
        /* TODO */
    }

    return strdup("");
}

static uint32_t nvaudio_out_get_latency(const struct audio_stream_out *stream)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    node_handle n = list_get_head(out->dev_list);
    uint32_t latency = 0;
    avp_stream_handle_t avp_stream;

    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);
        uint32_t temp;

        n = list_get_next_node(n);

        /* For ULP case we need to fool AF to prevent it from throwing error */
        if (out->avp_stream || dev->avp_stream ) {
            avp_stream = out->avp_stream ? out->avp_stream :
            (out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
             dev->fast_avp_stream : dev->avp_stream;

            temp = avp_stream_get_latency(avp_stream);
        }
        else
            temp = ((dev->config.period_size * dev->config.period_count)
                   * 1000) / dev->config.rate;

        if (!temp)
            continue;

        latency = latency ? MIN(latency, temp) : temp;
    }

    ALOGV("%s: latency %d flags = %d ", __FUNCTION__, latency,out->flags);
    return latency;
}

static int nvaudio_out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    struct nvaudio_hw_device *hw_dev = out->hw_dev;
    uint32_t i;

    ALOGV("%s : left vol %f right vol %f", __FUNCTION__, left, right);

    pthread_mutex_lock(&out->lock);

    out->stream_volume[0] = left;
    out->stream_volume[1] = right;

    left *= hw_dev->master_volume;
    right *= hw_dev->master_volume;

    if (out->avp_stream) {
        avp_stream_set_volume(out->avp_stream, left, right);
        goto exit;
    }

    out->volume[0] = INT_Q15(left);
    out->volume[1] = INT_Q15(right);

    switch (out->channel_count) {
        case 8:
            out->volume[7] = out->volume[1];
        case 7:
            out->volume[6] = out->volume[0];
        case 6:
            out->volume[5] = (out->volume[1] + out->volume[0]) >> 1;
        case 5:
            out->volume[4] = (out->volume[1] + out->volume[0]) >> 1;
        case 4:
            out->volume[3] = out->volume[1];
        case 3:
            out->volume[2] = out->volume[0];
        default:
            break;
    }

    for (i = out->channel_count; i < MAX_SUPPORTED_CHANNELS; i++)
        out->volume[i] = INT_Q15(1);

exit:
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static ssize_t nvaudio_out_write(struct audio_stream_out *stream,
                                 const void* buffer, size_t bytes)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    int32_t in_frame_size = audio_stream_frame_size(&stream->common);
    node_handle n;
    int ret = 0;
    uint32_t src_ch_map, dest_ch_map;
    uint32_t dev_stream_cnt;
    uint32_t i;
    int32_t mix;
    int16_t *src_buffer;
    uint32_t sleep_periods;
    double sleep_time;
    struct timespec timeout_ts;
    struct timeval  curr_time;
    int err;
    int16_t *pbuffer;
    size_t in_frames, in_frames_consumed, frames_to_process;
    struct audio_params audio_params;
    size_t bytes_written = bytes;
    const void *dump_buffer;
    size_t dump_bytes;
    //ALOGV("%s : %d bytes", __FUNCTION__, bytes);

    pthread_mutex_lock(&out->lock);

    n = list_get_head(out->dev_list);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);
        uint32_t id = dev->id;

        n = list_get_next_node(n);
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        if (dev->id == DEV_ID_AUX) {
            if (!(is_non_pcm_format(out->format)) &&
                dev->is_pass_thru) {
                if (1 == list_get_node_count(out->dev_list))
                    usleep((bytes*1000000)/(in_frame_size*dev->config.rate));

                continue;
            }
        }
#endif
        pthread_mutex_lock(&dev->lock);
        if (dev->restart_dev) {
            pcm_close(dev->pcm);
            dev->pcm = NULL;
            dev->restart_dev = 0;
        }
        if (out->standby || !dev->pcm) {
            ret = open_out_alsa_dev(out, dev);
        #ifdef TFA
            calibrate++;
            if (calibrate == 1)
                calibrate_speaker();
        #endif
        if (ret < 0) {
                pthread_mutex_unlock(&dev->lock);

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
                dev->is_pass_thru = false;
#endif
                if ((1 == list_get_node_count(out->dev_list))
                    &&  (DEV_ID_AUX == dev->id))
                    usleep((bytes*1000000)/(in_frame_size*dev->config.rate));

                continue;
            }
        }
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        if (dev->id == DEV_ID_AUX) {
           if (((out->format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_IEC61937) &&
                 !dev->is_pass_thru) {
               ret = open_out_alsa_dev(out, dev);
           }
       }
#endif

        dest_ch_map = tegra_dest_ch_map[dev->config.channels - 1];
        src_ch_map = get_ch_map(out->channel_count);
        audio_params.src_channels = out->channel_count;
        audio_params.src_ch_map = src_ch_map;
        audio_params.dest_channels = dev->config.channels;
        audio_params.dest_ch_map = dest_ch_map;
        audio_params.vol = out->volume;
        audio_params.format = out->format;

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        if ((out->format == AUDIO_FORMAT_IEC61937_AC3) &&
            !(dev->rec_dec_capable & REC_DEC_CAP_AC3)) {
            memset((void*)buffer, 0, bytes);
        } else if (((out->format == AUDIO_FORMAT_IEC61937_EAC3) &&
            !(dev->rec_dec_capable & REC_DEC_CAP_EAC3))) {
            memset((void*)buffer, 0, bytes);
        } else if (((out->format == AUDIO_FORMAT_IEC61937_DTS) &&
            !(dev->rec_dec_capable & REC_DEC_CAP_DTS))) {
            memset((void*)buffer, 0, bytes);
        } else if ((dev->id != DEV_ID_AUX) &&
           (out->format & AUDIO_FORMAT_IEC61937)) {
            memset((void*)buffer, 0, bytes);
        }

#endif

        if (!out->resampler[id]) {

        in_frames = bytes / in_frame_size;
        in_frames_consumed = 0;
        pbuffer =  (int16_t*)buffer;
        do {
            frames_to_process =
                (in_frames - in_frames_consumed) < dev->config.period_size
                ? (in_frames - in_frames_consumed) : dev->config.period_size;
            in_frames_consumed += frames_to_process;
            buffer =(void*)pbuffer;
            bytes = frames_to_process * in_frame_size;

             /*Do Work here*/
            if (audio_params.format == AUDIO_FORMAT_PCM_16_BIT) {
                audio_params.psrc_buffer = audio_params.pdest_buffer
                    = (int16_t *)buffer;
                if (out->channel_count < dev->config.channels) {
                    audio_params.pdest_buffer = out->buffer[id];
                    buffer = (void*)out->buffer[id];
                }
                bytes = (bytes*alsa_dev_frame_size(dev))/(in_frame_size);
                process_audio(&audio_params, frames_to_process);
            }

             dev_stream_cnt =
                 get_num_of_pcm_streams_to_mix(dev->active_stream_list);
             if ((dev_stream_cnt > 1) &&
                (!(is_non_pcm_format(out->format))) &&
                         (out->flags != AUDIO_OUTPUT_FLAG_FAST)) {
                /*multiple streams on this device*/
                src_buffer = (int16_t*)buffer;
                sleep_periods = dev->default_config.period_count > 2 ?
                    dev->default_config.period_count - 2 : 1;
                sleep_time = ((double)(bytes/alsa_dev_frame_size(dev))
                    *(1000000000))/(dev->config.rate);
                if (dev->buff_cnt + 1 < dev_stream_cnt) {
                    /*mix*/
                    for (i = 0; i < bytes/2; i++) {
                        mix = (int32_t)dev->mix_buffer[i] +
                            (int32_t)src_buffer[i];
                        dev->mix_buffer[i] = sat16(mix);
                    }
                    if(dev->bytes < bytes)
                        dev->bytes = bytes;
                    dev->buff_cnt++;
                    //ALOGV("@@@%d:Mixed and now entering sleep for %f
                    //ns and bufcnt = %d\n", out->channel_count,
                    //sleep_time*sleep_periods, dev->buff_cnt);
                    /*sleep*/
                    gettimeofday(&curr_time, NULL);
                    timeout_ts.tv_sec  = curr_time.tv_sec;
                    timeout_ts.tv_nsec = (curr_time.tv_usec * 1000)
                        + (sleep_time * sleep_periods);
                    pthread_mutex_unlock(&out->lock);
                    err = pthread_cond_timedwait(&dev->cond,
                        &dev->lock, &timeout_ts);
                    if (err == 0) {
                        pthread_mutex_lock(&out->lock);
                        //ALOGV("@@@%d: rendering done by some other thread\n",
                        //out->channel_count);
                        pbuffer += (frames_to_process*in_frame_size) >> 1;
                        continue;
                    } /*else {
                         if (err == ETIMEDOUT)
                             ALOGV("@@@%d: timedout\n",
                             out->channel_count);
                         else if (err == EINVAL)
                             ALOGE("@@@%d: something invalid\n",
                             out->channel_count);
                         else
                             ALOGE("@@@%d: unknow error\n",
                             out->channel_count);
                    }*/
                    pthread_mutex_lock(&out->lock);
                } else {
                    /*mix*/
                    for (i = 0; i < bytes/2; i++) {
                        mix = (int32_t)dev->mix_buffer[i]
                            + (int32_t)src_buffer[i];
                        dev->mix_buffer[i] = sat16(mix);
                    }
                    if(dev->bytes < bytes)
                        dev->bytes = bytes;
                    dev->buff_cnt++;
                    //ALOGV("@@@%d:Mixed and will be rendering as all buffers
                    //are received bufcnt = %d\n", out->channel_count,
                    //dev->buff_cnt);
                }

                if (dev->buff_cnt != 0) {
                    dev->buff_cnt = 0;
                    //ALOGV("@@@%d:Rendering now\n", out->channel_count);

                    pthread_mutex_lock(&dev->mix_lock);
                    memcpy(dev->mix_buffer_bup, dev->mix_buffer, dev->mix_buff_size);
                    dev->bytes_mixed = dev->bytes;
                    pthread_mutex_unlock(&dev->mix_lock);

                    dev->bytes = 0;
                    memset(dev->mix_buffer, 0, dev->mix_buff_size);
                    pthread_mutex_unlock(&dev->lock);
                    pthread_mutex_unlock(&out->lock);

                    pthread_mutex_lock(&dev->mix_lock);
                    if(out->fdump){
                        dump_buffer = (void*)dev->mix_buffer_bup;
                        dump_bytes = dev->bytes_mixed;
                    }

                    if (out->avp_stream || dev->avp_stream || dev->fast_avp_stream) {
                        avp_stream_handle_t stream = out->avp_stream ?
                        out->avp_stream : ((out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                                dev->fast_avp_stream : dev->avp_stream);

                        ret = nvaudio_avp_stream_write(stream, (void*)dev->mix_buffer_bup,
                            dev->bytes_mixed, out->devices, out->format, out, dev);
                    } else if (dev->id != DEV_ID_WFD) {
                        ret = nvaudio_pcm_write(dev, (void*)dev->mix_buffer_bup,dev->bytes_mixed,out->devices);
                        if (ret == -ESTRPIPE) {
                            ALOGE("ESTRPIPE : suspend state\n");
                            pcm_close(dev->pcm);
                            dev->pcm = pcm_open(dev->card,
                                                dev->device_id,
                                                dev->pcm_flags,
                                                &dev->config);
                            if (!pcm_is_ready(dev->pcm)) {
                                dev->pcm = 0;
                                usleep((bytes*1000000)/(in_frame_size
                                        *dev->config.rate));
                                pthread_mutex_unlock(&dev->mix_lock);
                                continue;
                            }
                            nvaudio_pcm_write(dev, (void*)dev->mix_buffer_bup, dev->bytes_mixed,out->devices);
                        }
                    } else {
                        ret = wfd_write(dev, (void*)dev->mix_buffer_bup, dev->bytes_mixed);
                    }
                    nvaudio_out_write_submix(stream, (void*)dev->mix_buffer_bup, dev->bytes_mixed, dev->config.rate);

                    pthread_mutex_unlock(&dev->mix_lock);
                    pthread_cond_signal(&dev->cond);
                    pthread_mutex_lock(&out->lock);
                    pthread_mutex_lock(&dev->lock);
                    if (ret != 0)
                        ALOGV("Failed to write to alsa_device %s err %d",
                        alsa_dev_names[id].name, ret);
                    //ALOGV("@@@%d:Rendering DONE\n", out->channel_count);
                }
            } else {/*single streams on this device*/
                //ALOGV("@@@%d:No Contention Rendering now\n",
                //out->channel_count);
                pthread_mutex_unlock(&dev->lock);
                pthread_mutex_unlock(&out->lock);
                if(out->fdump){
                    dump_buffer = buffer;
                    dump_bytes = bytes;
                }

                if (out->avp_stream || dev->avp_stream || dev->fast_avp_stream) {
                    avp_stream_handle_t stream = out->avp_stream ?
                    out->avp_stream : ((out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                               dev->fast_avp_stream : dev->avp_stream);

                    ret = nvaudio_avp_stream_write(stream, (void*)buffer, bytes, out->devices, out->format, out, dev);
                } else if (dev->id != DEV_ID_WFD) {
                    ret = nvaudio_pcm_write(dev, (void*)buffer, bytes,out->devices);
                    if (ret == -ESTRPIPE) {
                        ALOGE("ESTRPIPE : suspend state\n");
                        pcm_close(dev->pcm);
                        dev->pcm = pcm_open(dev->card,
                                            dev->device_id,
                                            dev->pcm_flags,
                                            &dev->config);
                        if (!pcm_is_ready(dev->pcm)) {
                            dev->pcm = 0;
                            usleep((bytes*1000000)/(in_frame_size
                                    *dev->config.rate));
                            continue;
                        }
                        nvaudio_pcm_write(dev, (void*)buffer, bytes, out->devices);
                    }
                } else
                    ret = wfd_write(dev, (void*)buffer, bytes);
                nvaudio_out_write_submix(stream, (void*)buffer, bytes, dev->config.rate);

                pthread_mutex_lock(&out->lock);
                pthread_mutex_lock(&dev->lock);
                if (ret != 0)
                    ALOGV("Failed to write to alsa_device %s err %d",
                    alsa_dev_names[id].name, ret);
            }

            pbuffer += (frames_to_process*in_frame_size) >> 1;
            if(out->fdump){
                fwrite(dump_buffer, 1, dump_bytes, out->fdump);
                fflush(out->fdump);
            }
        } while (in_frames_consumed < in_frames);
        } else {
            int16_t *resampling_buffer = NULL;
            int32_t max_frame_size;
            size_t in_frames, out_frames;
            size_t target_in_frames = bytes / in_frame_size;
            size_t target_out_frames;
            int32_t out_frame_size = alsa_dev_frame_size(dev);
            size_t resamp_buf_pos;
            uint32_t is_rendering_done;
            size_t resamp_buf_frames_avail, out_buf_frames_avail;
            size_t frames_to_copy;
            resamp_buf_frames_avail = out_buf_frames_avail = frames_to_copy = 0;
            resamp_buf_pos = is_rendering_done = 0;

            target_out_frames = (target_in_frames * dev->config.rate)/
                                                   (out->rate);
            if (out->resampling_buffer[id] == NULL) {
                // Allocate resampling buffer for first time
                max_frame_size = MAX(in_frame_size, out_frame_size);
                out->resampling_buffer[id] =
                        malloc(target_out_frames * max_frame_size);
                if (!out->resampling_buffer[id]) {
                    ALOGE("Unable to allocate resampling buffer\n");
                    continue;
                }
            }

            resampling_buffer = out->resampling_buffer[id];
            in_frames = target_in_frames;
            out_frames = target_out_frames;
            /*ALOGE("@@@Try to consume %d input frames and produce
           %d output frames\n" ,in_frames, out_frames);*/

            /* resample, downmix/upmix/shuffle and apply volume */

            ret = out->resampler[id]->resample_from_input(
                        out->resampler[id],
                        (int16_t*)buffer,
                        &in_frames,
                        (int16_t*)resampling_buffer,
                        &out_frames);
            /*ALOGE("@@@Resampler consumed %d input frames and
            produced %d output frames\n", in_frames, out_frames);*/

            if (audio_params.format == AUDIO_FORMAT_PCM_16_BIT) {
                audio_params.psrc_buffer = audio_params.pdest_buffer
                    = resampling_buffer;
                process_audio(&audio_params, out_frames);
            }

           /* copy data from resampling buffer to out buffer in period
           size units and render*/
           do {
               /*ALOGE("@@@Before Copying: resamp_buf_pos = %d
              out_buff_pos = %d\n", resamp_buf_pos, out->buff_pos[id]);*/
               resamp_buf_frames_avail = out_frames - resamp_buf_pos;
               out_buf_frames_avail = dev->config.period_size -
                                                       out->buff_pos[id];
               frames_to_copy = MIN(resamp_buf_frames_avail,
                                              out_buf_frames_avail);
               /*ALOGE("@@@Before Copying: resamp_buf_frames_avail =
              %d out_buf_frames_avail = %d, frames_to_copy = %d\n",
              resamp_buf_frames_avail, out_buf_frames_avail,
              frames_to_copy);*/

               if (frames_to_copy != 0) {
                   memcpy ((int16_t*)out->buffer[id] +
                     (out->buff_pos[id]*(out_frame_size >> 1)),
                     (int16_t*)resampling_buffer +
                     (resamp_buf_pos*(out_frame_size >> 1)),
                     frames_to_copy*out_frame_size);
                   resamp_buf_pos += frames_to_copy;
                   out->buff_pos[id] += frames_to_copy;
                   /*ALOGE("@@@After Copying: resamp_buf_pos = %d
                   out_buff_pos = %d\n", resamp_buf_pos,
                   out->buff_pos[id]);*/
               }

               if (out->buff_pos[id] != dev->config.period_size) {
                   /*ALOGE("@@@Out buff not full so do not render now\n");*/
                   continue;
               }

               /*ALOGE("@@@Rendering Out buff\n");*/

               out->buff_pos[id] = 0;

               /*****Mixing (Stalling if required) and Rendering*****/

               dev_stream_cnt =
                   get_num_of_pcm_streams_to_mix(dev->active_stream_list);
               if ((dev_stream_cnt > 1) &&
                    (!(is_non_pcm_format(out->format))) &&
                           (out->flags != AUDIO_OUTPUT_FLAG_FAST)) {
                    is_rendering_done = 0;
                    /*multiple streams on this device*/
                    src_buffer = (int16_t*)out->buffer[id];
                    sleep_periods = dev->default_config.period_count > 2 ?
                        dev->default_config.period_count - 2 : 1;
                    sleep_time = ((double)(dev->config.period_size) *
                        (1000000000))/(dev->config.rate);
                    if (dev->buff_cnt + 1 < dev_stream_cnt) {
                        /*mix*/
                        for (i = 0;
                                 i < (dev->config.period_size*out_frame_size)/2;
                                 i++) {
                            mix = (int32_t)dev->mix_buffer[i]
                               + (int32_t)src_buffer[i];
                            dev->mix_buffer[i] = sat16(mix);
                        }
                        if(dev->bytes < (dev->config.period_size*out_frame_size))
                            dev->bytes = (dev->config.period_size*out_frame_size);
                        dev->buff_cnt++;
                        //ALOGV("@@@%d:Mixed and now entering sleep for
                        //%f ns, bufcnt = %d  (Resampling)\n",
                        //out->channel_count, sleep_time*sleep_periods,
                        //dev->buff_cnt);
                        /*sleep*/
                        gettimeofday(&curr_time, NULL);
                        timeout_ts.tv_sec  = curr_time.tv_sec;
                        timeout_ts.tv_nsec = (curr_time.tv_usec * 1000)
                            + (sleep_time * sleep_periods);
                        pthread_mutex_unlock(&out->lock);
                        err = pthread_cond_timedwait(&dev->cond,
                        &dev->lock, &timeout_ts);
                        if (err == 0) {
                            is_rendering_done = 1;
                            //ALOGV("@@@%d: rendering done by
                            //some other thread\n", out->channel_count);
                        }/* else {
                            if (err == ETIMEDOUT)
                                ALOGV("@@@%d: timedout\n",
                                out->channel_count);
                            else if (err == EINVAL)
                                ALOGE("@@@%d: something invalid\n",
                                out->channel_count);
                            else
                                ALOGE("@@@%d: unknow error\n",
                                out->channel_count);
                        }*/
                        pthread_mutex_lock(&out->lock);
                    } else {
                        /*mix*/
                        for (i = 0;
                          i < (dev->config.period_size*out_frame_size)/2;
                          i++) {
                            mix = (int32_t)dev->mix_buffer[i]
                               + (int32_t)src_buffer[i];
                            dev->mix_buffer[i] = sat16(mix);
                        }
                        if(dev->bytes < (dev->config.period_size*out_frame_size))
                            dev->bytes = (dev->config.period_size*out_frame_size);
                        dev->buff_cnt++;
                        //ALOGV("@@@%d:Mixed and will be rendering
                        //as all buffers are received (Resampling)\n",
                        //out->channel_count);
                    }

                    if (dev->buff_cnt != 0 && is_rendering_done == 0) {
                        dev->buff_cnt = 0;
                        //ALOGV("@@@%d:Rendering now (Resampling)\n",
                        //out->channel_count);

                        pthread_mutex_lock(&dev->mix_lock);
                        memcpy(dev->mix_buffer_bup, dev->mix_buffer, dev->mix_buff_size);
                        dev->bytes_mixed = dev->bytes;
                        pthread_mutex_unlock(&dev->mix_lock);

                        dev->bytes = 0;
                        memset(dev->mix_buffer, 0, dev->mix_buff_size);
                        pthread_mutex_unlock(&dev->lock);
                        pthread_mutex_unlock(&out->lock);

                        pthread_mutex_lock(&dev->mix_lock);

                        if(out->fdump){
                            dump_buffer = (void*)dev->mix_buffer_bup;
                            dump_bytes = dev->bytes_mixed;
                        }

                        if (dev->avp_stream || dev->fast_avp_stream) {
                            avp_stream_handle_t stream = (out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                                 dev->fast_avp_stream : dev->avp_stream;

                            ret = nvaudio_avp_stream_write(stream,
                               (void*)dev->mix_buffer_bup,
                               dev->bytes_mixed, out->devices, out->format, out, dev);
                        } else if (dev->id != DEV_ID_WFD) {
                            ret = nvaudio_pcm_write(dev, (void*)dev->mix_buffer_bup,
                            dev->bytes_mixed, out->devices);
                            if (ret == -ESTRPIPE) {
                                ALOGE("ESTRPIPE : suspend state\n");
                                pcm_close(dev->pcm);
                                dev->pcm = pcm_open(dev->card,
                                                    dev->device_id,
                                                    dev->pcm_flags,
                                                    &dev->config);
                                if (!pcm_is_ready(dev->pcm)) {
                                    dev->pcm = 0;
                                    usleep((bytes*1000000)/(in_frame_size
                                            *dev->config.rate));
                                    pthread_mutex_unlock(&dev->mix_lock);
                                    continue;
                                }
                                nvaudio_pcm_write(dev, (void*)dev->mix_buffer_bup,
                                dev->bytes_mixed, out->devices);
                            }
                        } else
                        ret = wfd_write(dev,(void*)dev->mix_buffer_bup, dev->bytes_mixed);
                        nvaudio_out_write_submix(stream, (void*)dev->mix_buffer_bup, dev->bytes_mixed, dev->config.rate);

                        pthread_mutex_unlock(&dev->mix_lock);
                        pthread_cond_signal(&dev->cond);
                        pthread_mutex_lock(&out->lock);
                        pthread_mutex_lock(&dev->lock);
                        if (ret != 0) {
                            ALOGV("Failed to write to alsa_device"
                            " %s with err %d", alsa_dev_names[id].name, ret);
                            break;
                        }
                        //ALOGV("@@@%d:Rendering DONE (Resampling)\n",
                        //out->channel_count);
                    }
                } else {/*single streams on this device*/
                    pthread_mutex_unlock(&dev->lock);
                    pthread_mutex_unlock(&out->lock);

                    //ALOGV("@@@%d:No Contention Rendering
                    //now (Resampling)\n", out->channel_count);
                   //now (Resampling)\n", out->channel_count);

                    if(out->fdump){
                        dump_buffer = (void*)out->buffer[id];
                        dump_bytes = dev->config.period_size * out_frame_size;
                    }

                    if (dev->avp_stream || dev->fast_avp_stream) {
                        avp_stream_handle_t stream = (out->flags == AUDIO_OUTPUT_FLAG_FAST) ?
                               dev->fast_avp_stream : dev->avp_stream;

                        ret = nvaudio_avp_stream_write(stream,
                        (void*)out->buffer[id],
                        dev->config.period_size * out_frame_size, out->devices, out->format,out, dev);
                    } else if (dev->id != DEV_ID_WFD) {
                        ret = nvaudio_pcm_write(dev, (void*)out->buffer[id],
                        dev->config.period_size * out_frame_size, out->devices);
                        if (ret == -ESTRPIPE) {
                            ALOGE("ESTRPIPE : suspend state\n");
                            pcm_close(dev->pcm);
                            dev->pcm = pcm_open(dev->card,
                                                dev->device_id,
                                                dev->pcm_flags,
                                                &dev->config);
                            if (!pcm_is_ready(dev->pcm)) {
                                dev->pcm = 0;
                                usleep((bytes*1000000)/(in_frame_size
                                        *dev->config.rate));
                                continue;
                            }
                            nvaudio_pcm_write(dev, (void*)out->buffer[id],
                            dev->config.period_size * out_frame_size, out->devices);
                        }
                    } else
                        ret = wfd_write(dev,
                        (void*)out->buffer[id], dev->config.period_size *
                        out_frame_size);
                    nvaudio_out_write_submix(stream, (void*)out->buffer[id], dev->config.period_size * out_frame_size, dev->config.rate);


                    pthread_mutex_lock(&out->lock);
                    pthread_mutex_lock(&dev->lock);
                    if (ret != 0) {
                        ALOGV("Failed to write to alsa_device %s with err %d",
                        alsa_dev_names[id].name, ret);
                        break;
                    }
                }
                if(out->fdump){
                    fwrite(dump_buffer, 1, dump_bytes, out->fdump);
                    fflush(out->fdump);
                }
            } while (resamp_buf_pos != out_frames);
        }
        pthread_mutex_unlock(&dev->lock);
    }

    if (out->standby)
        out->standby = false;

    out->frame_count += (bytes_written / in_frame_size);

    pthread_mutex_unlock(&out->lock);

    return bytes_written;
}

static int nvaudio_out_get_render_position(
                                   const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    node_handle n = list_get_head(out->dev_list);

    if (out->avp_stream)
        *dsp_frames = avp_stream_frame_consumed(out->avp_stream);
    else {
        *dsp_frames =  out->frame_count;
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            n = list_get_next_node(n);

            if ((out->flags == AUDIO_OUTPUT_FLAG_FAST) && dev->fast_avp_stream) {
                *dsp_frames = avp_stream_frame_consumed(dev->fast_avp_stream);
                break;
            } else if (dev->avp_stream) {
                *dsp_frames = avp_stream_frame_consumed(dev->avp_stream);
                break;
            }
        }
    }

    ALOGV("%s : %d", __FUNCTION__, *dsp_frames);
    return 0;
}

static int nvaudio_out_get_presentation_position(
                                const struct audio_stream_out *stream,
                                uint64_t *frames,
                                struct timespec *timestamp)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    ALOGV("%s", __FUNCTION__);

    if (out->avp_stream) {
        // Not implemented for ULP
        return -1;
    } else {
        node_handle n = list_get_head(out->dev_list);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            n = list_get_next_node(n);

            pthread_mutex_lock(&dev->lock);

            if ((out->flags == AUDIO_OUTPUT_FLAG_FAST) && dev->fast_avp_stream) {
                *frames = out->standby_frame_count +
                            avp_stream_frame_consumed(dev->fast_avp_stream);
                pthread_mutex_unlock(&dev->lock);
                clock_gettime(CLOCK_MONOTONIC, timestamp);
                break;
            } else if (dev->avp_stream) {
                *frames = out->standby_frame_count +
                            avp_stream_frame_consumed(dev->avp_stream);
                pthread_mutex_unlock(&dev->lock);
                clock_gettime(CLOCK_MONOTONIC, timestamp);
                break;
            } else if (dev->id != DEV_ID_WFD) {
                if (dev->pcm) {
                    size_t avail;
                    if (pcm_get_htimestamp(dev->pcm, &avail, timestamp) == 0) {
                        // frames comsumed is (buffer size - space available)
                        *frames = out->standby_frame_count + out->frame_count -
                            (dev->config.period_size * dev->config.period_count) + avail;
                        clock_gettime(CLOCK_MONOTONIC, timestamp);
                    }
                }
                pthread_mutex_unlock(&dev->lock);
                break;
            }
            pthread_mutex_unlock(&dev->lock);
        }
    }
    return 0;
}

static int nvaudio_out_add_audio_effect(const struct audio_stream *stream,
                                        effect_handle_t effect)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

static int nvaudio_out_remove_audio_effect(const struct audio_stream *stream,
                                           effect_handle_t effect)
{
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

/*****************************************************************************
 *****************************************************************************
 * NVAUDIO_STREAM_IN Interface
 *****************************************************************************
 *****************************************************************************/
static uint32_t nvaudio_in_get_sample_rate(const struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    // ALOGV("%s : rate %d", __FUNCTION__, in->rate);

    return in->rate;
}

static int nvaudio_in_set_sample_rate(struct audio_stream *stream,
                                      uint32_t rate)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    struct alsa_dev *dev = in->dev;

    ALOGV("%s : rate %d", __FUNCTION__, rate);

    pthread_mutex_lock(&in->lock);
    if (in->rate == rate)
        goto exit;

    in->rate = rate;
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    set_in_resampler(in, RESAMPLER_QUALITY_DEFAULT);

exit:
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static size_t nvaudio_in_get_buffer_size(const struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    struct alsa_dev *dev = in->dev;
    size_t buffer_size = 0;
    uint32_t device_sample_rate = DEFAULT_CONFIG_RATE;


    buffer_size = (DEFAULT_PERIOD_SIZE * in->rate) / device_sample_rate;
    buffer_size = (buffer_size / 16) * 16;
    buffer_size *= audio_stream_frame_size(&in->stream.common);

    ALOGV("%s: size %d", __FUNCTION__, buffer_size);

    return buffer_size;
}

static uint32_t nvaudio_in_get_channels(const struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    //ALOGV("%s : channel mask %x", __FUNCTION__, in->channel_mask);

    return in->channels;
}

static audio_format_t nvaudio_in_get_format(const struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    //ALOGV("%s : format %x", __FUNCTION__, in->format);

    return in->format;
}

static int nvaudio_in_set_format(struct audio_stream *stream, audio_format_t format)
{
    ALOGV("%s : %d", __FUNCTION__, format);

    /* TODO : Not supported at this moment */

    return 0;
}

static int nvaudio_in_standby(struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    struct alsa_dev *dev = in->dev;

    ALOGV("%s", __FUNCTION__);

    if (in->standby)
        return 0;

    if((dev == NULL) && (in->source == AUDIO_SOURCE_REMOTE_SUBMIX)) {
        nvaudio_in_standby_submix(stream);
        return 0;
    }

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&dev->lock);

    close_alsa_dev(stream, dev);
    if (in->resampler) {
        in->resampler->reset(in->resampler);
        in->buffer_offset = 0;
    }
    in->standby = true;
    if(in->fdump){
        fclose(in->fdump);
        in->fdump = NULL;
    }

    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int nvaudio_in_dump(const struct audio_stream *stream, int fd)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

static int nvaudio_in_set_parameters(struct audio_stream *stream,
                                     const char *kvpairs)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    struct nvaudio_hw_device *hw_dev = in->hw_dev;
    struct str_parms *parms;
    node_handle n;
    char value[32];
    unsigned int key_val;
    int ret = 0;

    ALOGV("%s: %s", __FUNCTION__, kvpairs);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));

    if (ret >= 0) {
        uint32_t new_devices = atoi(value);
        new_devices &= ~AUDIO_DEVICE_BIT_IN;

        ALOGV("Switch audio to device :0x%x\n", new_devices);

        pthread_mutex_lock(&hw_dev->lock);
        pthread_mutex_lock(&in->lock);
#ifdef NVOICE
        if(nvoice_supported) {
            nvoice_setindevice(new_devices);
            in->nvoice_stopped = false;
        }
#endif

        if (new_devices && (in->devices != new_devices)) {
            n = list_get_head(hw_dev->dev_list[STREAM_IN]);

            while (n) {
                struct alsa_dev *dev = list_get_node_data(n);

                n = list_get_next_node(n);
                pthread_mutex_lock(&dev->lock);

                if ((dev->devices & new_devices) &&
                    !(dev->devices & in->devices)) {
                    in->dev = dev;
                    if (in->resampler) {
                        release_resampler(in->resampler);
                        in->resampler = NULL;
                    }

                    if (hw_dev->mode != AUDIO_MODE_IN_CALL)
                        set_route(dev, new_devices, 0, &in->stream.common);

                    if (!in->standby)
                        open_in_alsa_dev(in);
                } else if (!(dev->devices & new_devices) &&
                          (dev->devices & in->devices)) {
                    if (!in->standby)
                        close_alsa_dev(stream, dev);

                    if (hw_dev->mode != AUDIO_MODE_IN_CALL)
                        set_route(dev, 0, in->devices, &in->stream.common);
                } else if (hw_dev->mode != AUDIO_MODE_IN_CALL)
                    set_route(dev, new_devices, in->devices, &in->stream.common);

                if (hw_dev->mic_mute)
                    set_mute(dev, new_devices, true);

                pthread_mutex_unlock(&dev->lock);
            }
            in->devices = new_devices;
        }
#ifdef NVOICE
        if((nvoice_supported) &&
          (hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
                if(new_devices)
                    nvoice_configure();
                else
                {
                    n = list_get_head(hw_dev->dev_list[STREAM_IN]);
                    while (n) {
                        struct alsa_dev *dev = list_get_node_data(n);
                        n = list_get_next_node(n);
                        pthread_mutex_lock(&dev->lock);
                        set_route(dev, 0, in->devices, &in->stream.common);
                        pthread_mutex_unlock(&dev->lock);
                    }
                    in->devices = 0;
                }
        }
#endif

        if(hw_dev->mode == AUDIO_MODE_IN_CALL)
           call_record_at_cmd(hw_dev, in, 1);

        pthread_mutex_unlock(&in->lock);
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_FORMAT,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set format to %d\n", key_val);

        if (in->format != (audio_format_t)key_val)
            nvaudio_in_set_format(&in->stream.common, key_val);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_CHANNELS,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set channels to %d\n", key_val);

        pthread_mutex_lock(&in->lock);
        if (in->channels != key_val) {
            in->channels = key_val;

            if (in->resampler) {
                release_resampler(in->resampler);
                in->resampler = NULL;
            }
            set_in_resampler(in, RESAMPLER_QUALITY_DEFAULT);
        }
        pthread_mutex_unlock(&in->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set input source to %x\n", key_val);

        in->source = key_val;
#ifdef AUDIO_TUNE
        (*hw_dev).tune.in_source = key_val;
#endif
    }

    str_parms_destroy(parms);
    return ret;
}

static char * nvaudio_in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    char tmp[80];

    ALOGV("%s : %s", __FUNCTION__, keys);

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_ROUTING)) {
        sprintf(tmp, "%d", in->devices);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_FORMAT)) {
        sprintf(tmp, "%d", in->format);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_CHANNELS)) {
        sprintf(tmp, "%d", in->channels);
        return strdup(tmp);
    }

    if (!strcmp(keys, AUDIO_PARAMETER_STREAM_INPUT_SOURCE)) {
        sprintf(tmp, "%d", in->source);
        return strdup(tmp);
    }

    return strdup("");
}

static int nvaudio_in_set_gain(struct audio_stream_in *stream, float gain)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    ALOGV("%s : gain %f", __FUNCTION__, gain);

    /* TODO */

    return 0;
}

static ssize_t nvaudio_in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;
    struct alsa_dev *dev = in->dev;
    uint32_t out_frame_size = audio_stream_frame_size(&stream->common);
    int ret = 0;

    //ALOGV("%s : %d bytes for stream %p", __FUNCTION__, bytes, stream);

    if((dev == NULL) && (in->source == AUDIO_SOURCE_REMOTE_SUBMIX)) {
        return nvaudio_in_read_submix(stream, buffer, bytes);
    }

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&dev->lock);

    if (in->standby || !dev->pcm) {
        in->standby = false;
        ret = open_in_alsa_dev(in);
        if (ret < 0)
            goto err_exit;
    }

#ifdef NVOICE
    if((nvoice_supported) &&
        (in->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) &&
        ((in->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) == 0)) {
        property_get("nvoice.set.param",set_param,"0");
        property_get("nvoice.get.param",get_param,"0");
        property_get("nvoice.value",value,"0");
        if(atoi(set_param))
        {
            nvoice_set_param(atoi(set_param),atoi(value));
        }
        if(atoi(get_param))
        {
            nvoice_get_param(atoi(get_param));
        }
    }
#endif

    if (!in->resampler && (in->channel_count == dev->config.channels)) {
        ret = pcm_read(dev->pcm, (void*)buffer, bytes);
        if (ret < 0) {
            ALOGE("Failed to read from alsa_device %s with err %d",
                 alsa_dev_names[dev->id].name, ret);
            goto err_exit;
        }
        if(in->fdump){
            fwrite(buffer, 1, bytes, in->fdump);
            fflush(in->fdump );
        }
#ifdef NVOICE
        if((nvoice_supported) &&
          (in->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) &&
          ((in->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) == 0)) {
            nvoice_processtx((void*)buffer,dev->config.period_size, dev->config.rate);

        }
#endif
    } else {
        size_t in_frames, out_frames, out_buf_pos = 0;
        uint32_t in_frame_size = alsa_dev_frame_size(in->dev);
        size_t target_out_frames = bytes / out_frame_size;

        do {
            out_frames = target_out_frames - out_buf_pos;
            if (!in->buffer_offset) {
                ret = pcm_read(dev->pcm, (void*)in->buffer,
                               dev->config.period_size * in_frame_size);
                if (ret < 0) {
                    ALOGE("Failed to read from alsa_device %s with err %d",
                         alsa_dev_names[dev->id].name, ret);
                    goto err_exit;
                }
                if(in->fdump){
                    fwrite(in->buffer, 1, dev->config.period_size * in_frame_size, in->fdump);
                    fflush(in->fdump );
                }
#ifdef NVOICE
                if((!in->nvoice_stopped) && (nvoice_supported) &&
                  (in->hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) &&
                  ((in->devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) == 0)) {
                    if(dev->config.channels == 1) {
                        mono_to_stereo(in->buffer, in->buffer,
                                       dev->config.period_size);
                        nvoice_processtx((void*)in->buffer,dev->config.period_size, dev->config.rate);
                        stereo_to_mono(in->buffer, in->buffer,
                                       dev->config.period_size);
                    } else
                        nvoice_processtx((void*)in->buffer,dev->config.period_size, dev->config.rate);

                }
#endif
                if (dev->config.channels != in->channel_count) {
                    if ((dev->config.channels == 1) &&
                        (in->channel_count == 2))
                        mono_to_stereo(in->buffer, in->buffer,
                                       dev->config.period_size);
                    else if((dev->config.channels == 2) &&
                            (in->channel_count == 1))
                        stereo_to_mono(in->buffer, in->buffer,
                                       dev->config.period_size);
                    else
                        ALOGW("Unsupported channels. alsa_dev %d in %d",
                             dev->config.channels, in->channel_count);
                }
            }
            in_frames = (dev->config.period_size - in->buffer_offset);
            if (in->resampler) {
                ret = in->resampler->resample_from_input(in->resampler,
                            in->buffer +
                                (in->buffer_offset * (out_frame_size >> 1)),
                            &in_frames,
                            (int16_t*)buffer +
                                (out_buf_pos * (out_frame_size >> 1)),
                            &out_frames);
                if (ret < 0) {
                    ALOGE("Failed to resample input buffer");
                    goto err_exit;
                }
            } else {
                in_frames = MIN(in_frames, out_frames);
                memcpy((int16_t*)buffer + (out_buf_pos * (out_frame_size >> 1)),
                       in->buffer + (in->buffer_offset * (out_frame_size >> 1)),
                       in_frames * out_frame_size);
                out_frames = in_frames;
            }

            in->buffer_offset += in_frames;
            if (in->buffer_offset >= dev->config.period_size)
                in->buffer_offset = 0;

            out_buf_pos += out_frames;
        } while (out_buf_pos < target_out_frames);
    }

    /* Send zeroed buffer when mic mute is ON */
    if (in->hw_dev->mic_mute)
        memset(buffer, 0, bytes);

    goto exit;

err_exit:
    memset(buffer, 0, bytes);
    usleep((bytes * 1000000) / (out_frame_size * in->rate));

exit:
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&in->lock);

    return bytes;
}

static uint32_t nvaudio_in_get_input_frames_lost(struct audio_stream_in *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    // ALOGV("%s", __FUNCTION__);

    /* TODO : Not sure what is expected here */

    return 0;
}

static int nvaudio_in_add_audio_effect(const struct audio_stream *stream,
                                       effect_handle_t effect)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

static int nvaudio_in_remove_audio_effect(const struct audio_stream *stream,
                                          effect_handle_t effect)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

/*****************************************************************************
 *****************************************************************************
 * NVAUDIO_HW_DEVICE Interface
 *****************************************************************************
 *****************************************************************************/

static int nvaudio_dev_open_output_stream(struct audio_hw_device *dev,
                                          audio_io_handle_t handle,
                                          audio_devices_t devices,
                                          audio_output_flags_t flags,
                                          struct audio_config *config,
                                          struct audio_stream_out **stream_out)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    struct alsa_dev_cfg *dev_cfg;
    struct nvaudio_stream_out *out;
    node_handle n;
    int alsa_format = PCM_FORMAT_S16_LE;
    uint32_t i;
    int ret;
    pthread_mutexattr_t attr;

    ALOGV("%s:: dev %x fmt %d ch %d rate %d",
                __FUNCTION__, devices, config->format, config->channel_mask,
                    config->sample_rate);

    pthread_mutex_lock(&hw_dev->lock);

    out = (struct nvaudio_stream_out *)calloc(1, sizeof(*out));
    if (!out) {
        pthread_mutex_unlock(&hw_dev->lock);
        return -ENOMEM;
    }

    out->hw_dev = hw_dev;
    for (i = 0; i < MAX_SUPPORTED_CHANNELS; i++)
        out->volume[i] = INT_Q15(1);
    for (i = 0; i < 2; i++)
        out->stream_volume[i] = 1.0;

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&out->lock, &attr);
    out->flags = flags;
    out->stream.common.get_sample_rate = nvaudio_out_get_sample_rate;
    out->stream.common.set_sample_rate = nvaudio_out_set_sample_rate;
    out->stream.common.get_buffer_size = nvaudio_out_get_buffer_size;
    out->stream.common.get_channels = nvaudio_out_get_channels;
    out->stream.common.get_format = nvaudio_out_get_format;
    out->stream.common.set_format = nvaudio_out_set_format;
    out->stream.common.standby = nvaudio_out_standby;
    out->stream.common.dump = nvaudio_out_dump;
    out->stream.common.set_parameters = nvaudio_out_set_parameters;
    out->stream.common.get_parameters = nvaudio_out_get_parameters;
    out->stream.common.add_audio_effect = nvaudio_out_add_audio_effect;
    out->stream.common.remove_audio_effect = nvaudio_out_remove_audio_effect;
    out->stream.get_latency = nvaudio_out_get_latency;
    out->stream.set_volume = nvaudio_out_set_volume;
    out->stream.write = nvaudio_out_write;
    out->stream.get_render_position = nvaudio_out_get_render_position;
    out->stream.get_presentation_position = nvaudio_out_get_presentation_position;

    out->dev_list = list_create();

    n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);

        n = list_get_next_node(n);

        pthread_mutex_lock(&dev->lock);
        /* If one stream is already active don't overwrite route from here */
        if (!list_get_node_count(hw_dev->stream_out_list) &&
            (dev->devices & devices)) {
            set_route(dev, devices, 0, &out->stream.common);
            out->devices |= (dev->devices & devices);
        }
        if (devices & (dev->devices & devices))
            list_push_data(out->dev_list, dev);
        pthread_mutex_unlock(&dev->lock);
    }

    if ((is_non_pcm_format(config->format))
#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        && ((config->format & AUDIO_FORMAT_MAIN_MASK) != AUDIO_FORMAT_IEC61937)
#endif
    ) {
        if (hw_dev->avp_audio) {
            uint32_t stream_type;

            switch (config->format) {
                case AUDIO_FORMAT_MP3:
                    stream_type = STREAM_TYPE_MP3;
                    break;
                case AUDIO_FORMAT_AAC:
                    stream_type = STREAM_TYPE_AAC;
                    break;
                default:
                    ALOGE("Unsupported non-pcm format %d", config->format);
                    ret = -EINVAL;
                    goto err_exit;
            }

            out->avp_stream = avp_stream_open(hw_dev->avp_audio, stream_type,
                                             config->sample_rate);
            if (!out->avp_stream) {
                ALOGE("Failed to open avp audio stream");
                ret = -EINVAL;
                goto err_exit;
            }
        } else {
            ALOGE("Format %x not supported", config->format);
            ret = -EINVAL;
            goto err_exit;
        }
    }

    n = list_get_head(out->dev_list);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);

        n = list_get_next_node(n);
        if (!(config->format))
            config->format = ALSA_TO_AUDIO_FORMAT(dev->config.format);
        if (!(config->channel_mask))
            config->channel_mask = audio_out_channel_mask(dev->config.channels);
        /* For first output stream ignore samplerate set by AF */
        if (!list_get_node_count(hw_dev->stream_out_list) ||
            !(config->sample_rate))
            config->sample_rate = dev->config.rate;
    }

    out->io_handle = handle;
    out->pending_route = (~out->devices & devices);
    out->format = config->format;
    out->channels = config->channel_mask;
    out->channel_count = pop_count(config->channel_mask);
    out->rate = config->sample_rate;
    out->standby = true;

    list_push_data(hw_dev->stream_out_list, out);
    *stream_out = &out->stream;

    pthread_mutex_unlock(&hw_dev->lock);
    return 0;

err_exit:
    if (out->dev_list)
        list_destroy(out->dev_list);
    if (out)
        free(out);

    pthread_mutex_unlock(&hw_dev->lock);
    return ret;
}

static void nvaudio_dev_close_output_stream(struct audio_hw_device *dev,
                                            struct audio_stream_out *stream)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    struct nvaudio_stream_out *out = (struct nvaudio_stream_out *)stream;
    uint32_t i;
    node_handle n_j;

    ALOGV("%s ", __FUNCTION__);

    n_j = list_get_head(out->dev_list);
    pthread_mutex_lock(&out->lock);

    while (n_j) {
        struct alsa_dev *dev = list_get_node_data(n_j);

        pthread_mutex_lock(&dev->lock);
        set_route(dev, 0, out->devices, &out->stream.common);
        pthread_mutex_unlock(&dev->lock);

        n_j = list_get_next_node(n_j);
    }
    pthread_mutex_unlock(&out->lock);

    if (!out->standby)
        nvaudio_out_standby(&out->stream.common);

    pthread_mutex_lock(&hw_dev->lock);

    if (out->avp_stream) {
        avp_stream_close(out->avp_stream);
        if (out->avp_stream == hw_dev->avp_active_stream)
            hw_dev->avp_active_stream = 0;
        out->avp_stream = 0;
    }

    for (i = 0; i < DEV_ID_NON_CALL_MAX; i++) {
        if (out->resampler[i])
            release_resampler(out->resampler[i]);
        if (out->buffer[i])
            free(out->buffer[i]);
        if (out->resampling_buffer[i]) {
            free(out->resampling_buffer[i]);
            out->resampling_buffer[i] = NULL;
        }
    }

    pthread_mutex_destroy(&out->lock);
    list_destroy(out->dev_list);
    list_delete_data(hw_dev->stream_out_list, out);
    if(out->fdump){
        fclose(out->fdump);
        out->fdump = NULL;
    }
    free(out);

    pthread_mutex_unlock(&hw_dev->lock);
}

static int nvaudio_dev_set_parameters(struct audio_hw_device *dev,
                                      const char *kvpairs)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    struct str_parms *parms;
    audio_io_handle_t io_handle = 0;
    node_handle n;
    char value[32];
    unsigned int key_val;
    int ret = 0;

    ALOGV("%s : %s", __FUNCTION__, kvpairs);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_IO_HANDLE,
                            value, sizeof(value));
    if (ret >= 0) {
        io_handle = atoi(value);
        ALOGV("Param is for output :0x%x\n", io_handle);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    if (ret < 0)
        ret = str_parms_get_str(parms, "closing",
                                value, sizeof(value));
    if (ret >= 0) {
        pthread_mutex_lock(&hw_dev->lock);
        n = list_get_head(hw_dev->stream_out_list);
        while (n) {
            struct nvaudio_stream_out *out = list_get_node_data(n);

            n = list_get_next_node(n);
            if (out->io_handle == io_handle) {
                pthread_mutex_unlock(&hw_dev->lock);
                out->stream.common.set_parameters(&out->stream.common,
                                                  kvpairs);
                pthread_mutex_lock(&hw_dev->lock);
            }
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_AVP_DECODE_FLUSH,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Flush avp decode input buffers :0x%x\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_active_stream && key_val) {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);

                n = list_get_next_node(n);
                if (hw_dev->avp_active_stream == out->avp_stream) {
                    avp_stream_flush(out->avp_stream);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_AVP_DECODE_PAUSE,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Pause avp decode :0x%x\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_active_stream) {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);

                n = list_get_next_node(n);

                if (hw_dev->avp_active_stream == out->avp_stream) {
                    avp_stream_pause(out->avp_stream, !!key_val);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_AVP_DECODE_EOS,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("AVP stream EOS reached :0x%x\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_active_stream) {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);

                n = list_get_next_node(n);

                if (hw_dev->avp_active_stream == out->avp_stream) {
                    avp_stream_set_eos(out->avp_stream, !!key_val);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_MEDIA_ROUTING,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Media is routed to :%d\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        hw_dev->media_devices = key_val;
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));
        pthread_mutex_unlock(&hw_dev->lock);

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
        char prop[PROPERTY_VALUE_MAX];
        if ((hw_dev->media_devices & AUDIO_DEVICE_OUT_ALL_A2DP) ||
            (hw_dev->media_devices & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ||
            (hw_dev->media_devices & AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET) ||
            (hw_dev->media_devices & AUDIO_DEVICE_OUT_REMOTE_SUBMIX))
        {
            property_set(TEGRA_HW_PASSTHROUGH_PROPERTY,"0");
        } else {
            if (hw_dev->dev_available & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
                n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
                while (n) {
                    struct alsa_dev *dev = list_get_node_data(n);

                    n = list_get_next_node(n);
                    if (dev->id == DEV_ID_AUX) {
                       sprintf(prop,"%d",(dev->rec_dec_capable | 0x01));
                       property_set(TEGRA_HW_PASSTHROUGH_PROPERTY,prop);
                    }
                }
            }
        }
#endif
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_EFFECTS_COUNT,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Number of registered audio effects :%d\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        hw_dev->effects_count = key_val;
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_DEV_AVAILABLE,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Device available = %x\n", key_val);
        /*If AUX is disconnected and it is not closed previously
        then close it here*/
        if (!(key_val & AUDIO_DEVICE_OUT_AUX_DIGITAL) &&
            (hw_dev->dev_available & AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
            n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
            while (n) {
                struct alsa_dev *dev = list_get_node_data(n);

                n = list_get_next_node(n);
                if (dev->id != DEV_ID_AUX)
                    continue;

#ifdef FRAMEWORK_HAS_AC3_DTS_PASSTHROUGH_SUPPORT
                    dev->is_pass_thru = false;
                    dev->rec_dec_capable = 0;
#endif

                if (!list_get_node_count(dev->active_stream_list) && dev->pcm) {
                    /*ALOGV("@@@Close the AUX device from %s\n",
                    __FUNCTION__);*/
                    pcm_close(dev->pcm);
                    dev->pcm = 0;
                }
            }
        } else if ((key_val & AUDIO_DEVICE_OUT_AUX_DIGITAL) &&
                   !(hw_dev->dev_available & AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
            /*If AUX is connected, open alsa device to start eld retrieve*/
            n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
            while (n) {
                struct alsa_dev *dev = list_get_node_data(n);
                n = list_get_next_node(n);
                if (dev->id != DEV_ID_AUX)
                    continue;

                if (dev->pcm == 0) {
                    dev->pcm_flags = PCM_OUT;
                    memcpy(&dev->config, &dev->default_config,
                           sizeof(struct pcm_config));
                    dev->config.channels = get_max_aux_channels(dev);

                    dev->pcm = pcm_open(dev->card,
                                        dev->device_id,
                                        dev->pcm_flags,
                                        &dev->config);

                    if (!pcm_is_ready(dev->pcm)) {
                        ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
                        dev->pcm = NULL;
                    }
                }
            }
        }
        hw_dev->dev_available = key_val;
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_AVP_LOOPBACK,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Set AVP loopback %d\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_audio)
            avp_audio_dump_loopback_data(hw_dev->avp_audio,
                                         AVP_LOOPBACK_FILE_PATH, key_val);
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_ULP_SEEK,
                            value, sizeof(value));

    if (ret >= 0) {
        int64_t seek_offset = atoll(value);
        ALOGV("Seek AVP stream at offset :%lld ", seek_offset);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_active_stream) {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);

                n = list_get_next_node(n);

                if (hw_dev->avp_active_stream == out->avp_stream) {
                    avp_stream_seek(out->avp_stream, seek_offset);
                    break;
                }
            }
        } else {
            hw_dev->seek_offset[1] = (seek_offset & 0xFFFFFFFF00000000) >> 32;
            hw_dev->seek_offset[0] = seek_offset & 0xFFFFFFFF;
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_TRACK_AUDIO_LATENCY,
                            value, sizeof(value));
    if (ret >= 0) {
        key_val = atoi(value);
        ALOGV("Track audio latency %d\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);
        if (hw_dev->avp_audio)
            avp_audio_track_audio_latency(hw_dev->avp_audio, key_val);
        pthread_mutex_unlock(&hw_dev->lock);
    }

    ret = str_parms_get_str(parms, NVAUDIO_PARAMETER_LOW_LATENCY_PATH,
                            value, sizeof(value));
    if (ret >= 0) {
        node_handle n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
        key_val = atoi(value);
        ALOGV("Set LOW latency path %d\n", key_val);

        pthread_mutex_lock(&hw_dev->lock);

        hw_dev->low_latency_mode = key_val;
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            n = list_get_next_node(n);
            if (dev->id != DEV_ID_AUX)
                    continue;

            if (hw_dev->low_latency_mode &&
                (dev->is_pass_thru || (dev->config.channels > 2)))
                break;

            pthread_mutex_lock(&dev->lock);
            dev->restart_dev = 1;
            pthread_mutex_unlock(&dev->lock);
            break;
        }
        pthread_mutex_unlock(&hw_dev->lock);
    }

#ifdef NVAUDIOFX
    ret = str_parms_get_str(parms, "fx_volume",
                            value, sizeof(value));
    if (ret >= 0) {
         fx_vol= atof(value);
        ALOGV("Set fx volume to %f\n", fx_vol);
    }

    ret = str_parms_get_str(parms, "mdrc_enable",
                            value, sizeof(value));
    if (ret >= 0) {
         key_val = atoi(value);
         nvaudiofx_enable_mdrc(key_val);
        ALOGV("mdrc %d\n", key_val);
    }
#endif

#ifdef AUDIO_PLAYING_SETPROP
    n = list_get_head(hw_dev->stream_out_list);
    while (n) {
        struct nvaudio_stream_out *out = list_get_node_data(n);
        n = list_get_next_node(n);
        if(out->standby != true) {
            property_set("media.audio.playing","1");
        } else {
            property_set("media.audio.playing","0");
        }
    }
#endif

    str_parms_destroy(parms);
    return 0;
}

static char * nvaudio_dev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;
    char tmp[80];

    ALOGVV("%s : %s", __FUNCTION__, keys);

    if (!strcmp(keys, NVAUDIO_PARAMETER_ULP_SUPPORTED)) {
        sprintf(tmp, "%d", is_ulp_supported(hw_dev));
        return strdup(tmp);
    }

    if (!strcmp(keys, NVAUDIO_PARAMETER_ULP_ACTIVE)) {
        sprintf(tmp, "%d", hw_dev->avp_active_stream ? 1 : 0);
        return strdup(tmp);
    }

    if (!strcmp(keys, NVAUDIO_PARAMETER_AVP_DECODE_POSITION)) {
        uint64_t position = 0;

        if (hw_dev->avp_active_stream) {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);

                n = list_get_next_node(n);
                if (hw_dev->avp_active_stream == out->avp_stream) {
                    position = avp_stream_get_position(out->avp_stream);
                    if (position > 0)
                        break;
                }
            }
        } else if (hw_dev->seek_offset[0] | hw_dev->seek_offset[1]) {
            // Seek is pending while no active stream present
            position = ((uint64_t)hw_dev->seek_offset[1] << 32) |
                                hw_dev->seek_offset[0];
        }
        sprintf(tmp, "%llu", position);
        return strdup(tmp);
    }

    if (!strcmp(keys, NVAUDIO_PARAMETER_ULP_AUDIO_FORMATS)) {
        sprintf(tmp, "%d", hw_dev->ulp_formats);
        return strdup(tmp);
    }

    if (!strcmp(keys, NVAUDIO_PARAMETER_ULP_AUDIO_RATES)) {
        if (strlen(hw_dev->ulp_rates_str) < 80)
            sprintf(tmp, "%s", hw_dev->ulp_rates_str);
        else
            ALOGE("Invalid ulp rate string length: %d",
                  strlen(hw_dev->ulp_rates_str));

        ALOGV("Supported ULP rates %s",tmp);
        return strdup(tmp);
    }

    return strdup("");
}

static int nvaudio_dev_init_check(const struct audio_hw_device *dev)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    ALOGV("%s ", __FUNCTION__);

    /* Ensure at least one valid playback and capture alsa device is found */
    if (hw_dev && list_get_node_count(hw_dev->dev_list[STREAM_OUT]) &&
        list_get_node_count(hw_dev->dev_list[STREAM_IN]))
        return 0;

    return -EINVAL;
}

static int nvaudio_dev_set_voice_volume(struct audio_hw_device *dev,
                                        float volume)
{
    int ret = 0;
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;
    char vol[5];
    int vol_level = volume*15;

    ALOGV("%s : vol %f", __FUNCTION__, volume);

    if(send_to_hal)
    {
        vol_level = (volume>0)?volume*10:1;
        (*send_to_hal)(pamhalctx, AMHAL_VOLUME,(void *)vol_level);
        ALOGV("sending AMHAL_VOLUME");
    } else {
        /*Send at command to modem to control DL Volume*/
        sprintf(vol,"%d\r\n", vol_level);
        n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            if (dev->at_cmd_list) {
                ret = send_at_cmd_by_list(dev->at_cmd_list, AT_VOLUME, vol);
                if (!ret)
                    break;
            }
            n = list_get_next_node(n);
        }
    }
    return 0;
}

static int nvaudio_dev_set_master_volume(struct audio_hw_device *dev,
                                         float volume)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;

    ALOGV("%s : vol %f", __FUNCTION__, volume);

    pthread_mutex_lock(&hw_dev->lock);

    hw_dev->master_volume = volume;
    n = list_get_head(hw_dev->stream_out_list);
    while (n) {
        struct nvaudio_stream_out *out = list_get_node_data(n);
        nvaudio_out_set_volume((struct audio_stream_out *)out, out->stream_volume[0], out->stream_volume[1]);
        n = list_get_next_node(n);
    }

    pthread_mutex_unlock(&hw_dev->lock);
    return 0;
}

static int nvaudio_dev_set_mode(struct audio_hw_device *dev, int mode)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;
    uint32_t i;
    int ret = 0;

    ALOGV("%s : mode %d", __FUNCTION__, mode);

    if ((mode != AUDIO_MODE_NORMAL) &&
        (hw_dev->mode == AUDIO_MODE_IN_CALL)) {
        /*If ringtone or VOIP call received during voice call,
        * don't handle this case */
        ALOGW("Not handling ringtone or VOIP call during voice call");
        return 0;
    }

    pthread_mutex_lock(&hw_dev->lock);

    if ((mode == AUDIO_MODE_IN_COMMUNICATION) ||
        (hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
#ifdef NVOICE
        if (nvoice_supported)  {
            if (hw_dev->avp_audio  && mode == AUDIO_MODE_IN_COMMUNICATION) {
                avp_audio_set_ulp(hw_dev->avp_audio, 0);
            }
            if(hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) {
                out_stream_open = 0;
            }

            n = list_get_head(hw_dev->stream_in_list);
            while (n) {
                struct nvaudio_stream_in *in = list_get_node_data(n);

                pthread_mutex_lock(&in->lock);
                in->hw_dev->mode = mode;
                if (in->dev) {
                    pthread_mutex_lock(&in->dev->lock);
                    if (in->dev->pcm) {
                        close_alsa_dev(&in->stream.common, in->dev);
                    }
                    pthread_mutex_unlock(&in->dev->lock);
                }
                if (in->resampler) {
                    in->resampler->reset(in->resampler);
                    in->buffer_offset = 0;
                }
               in->nvoice_stopped = true;
               pthread_mutex_unlock(&in->lock);
               n = list_get_next_node(n);
            }

            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);
                node_handle n_j = list_get_head(out->dev_list);

                pthread_mutex_lock(&out->lock);
                out->hw_dev->mode = mode;
                while (n_j) {
                    struct alsa_dev *dev = list_get_node_data(n_j);
                    pthread_mutex_lock(&dev->lock);
                    close_alsa_dev(&out->stream.common, dev);
                    pthread_mutex_unlock(&dev->lock);
                    n_j = list_get_next_node(n_j);
                }
                out->nvoice_stopped = true;
                pthread_mutex_unlock(&out->lock);
                n = list_get_next_node(n);
            }
            if (hw_dev->mode == AUDIO_MODE_IN_COMMUNICATION) {
                nvoice_delete();
            }
            if (hw_dev->avp_audio  && mode != AUDIO_MODE_IN_COMMUNICATION) {
                avp_audio_set_ulp(hw_dev->avp_audio, 1);
            }
        } else
#endif
        {
            n = list_get_head(hw_dev->stream_out_list);
            while (n) {
                struct nvaudio_stream_out *out = list_get_node_data(n);
                node_handle n_j = list_get_head(out->dev_list);

                pthread_mutex_lock(&out->lock);
                out->hw_dev->mode = mode;

                while (n_j) {
                    struct alsa_dev *dev = list_get_node_data(n_j);

                    pthread_mutex_lock(&dev->lock);
                    if (dev->pcm)
                        set_current_route(dev);
                    pthread_mutex_unlock(&dev->lock);

                    n_j = list_get_next_node(n_j);
                }
                pthread_mutex_unlock(&out->lock);
                n = list_get_next_node(n);
            }
        }
    }

    if ((mode == AUDIO_MODE_IN_CALL) &&
        (hw_dev->mode != AUDIO_MODE_IN_CALL)) {
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, 0);
        /* Close already opened alsa devices in voice call */
        n = list_get_head(hw_dev->stream_out_list);
        while (n) {
            struct nvaudio_stream_out *out = list_get_node_data(n);
            node_handle n_j = list_get_head(out->dev_list);

            pthread_mutex_lock(&out->lock);
            while (n_j) {
                struct alsa_dev *dev = list_get_node_data(n_j);

                pthread_mutex_lock(&dev->lock);
                close_alsa_dev(&out->stream.common, dev);
                set_route(dev, 0, out->devices, NULL);
                out->devices = 0;
                pthread_mutex_unlock(&dev->lock);
                n_j = list_get_next_node(n_j);
                list_delete_data(out->dev_list, dev);
            }
            pthread_mutex_unlock(&out->lock);
            n = list_get_next_node(n);
        }

        n = list_get_head(hw_dev->stream_in_list);
        while (n) {
            struct nvaudio_stream_in *in = list_get_node_data(n);

            pthread_mutex_lock(&in->lock);
            pthread_mutex_lock(&in->dev->lock);
            if (in->dev->pcm) {
                close_alsa_dev(&in->stream.common, in->dev);
            }
            pthread_mutex_unlock(&in->dev->lock);
            pthread_mutex_unlock(&in->lock);
            n = list_get_next_node(n);
        }
        /* notify modem about in call mode using at command */
        n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            if (dev->at_cmd_list) {
                char val[5];
                memset(val, 0, sizeof(val));
                sprintf(val,"%d\r\n", 1);
                ret = send_at_cmd_by_list(dev->at_cmd_list, AT_IN_CALL, val);
                if (!ret)
                    break;
            }
            n = list_get_next_node(n);
        }

        /*Save default lp Mode and chnge lp mode to lp1*/
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, 0);
        if (save_lp_mode(hw_dev) < 0)
            ALOGE("Error in save_lp_mode\n");
        if (set_lp_mode(LP_STATE_VOICE_CALL) < 0)
            ALOGE("Error in set_lp_mode\n");
        if(send_to_hal)
        {
            if(!pcm_started)
            {
                if (!spuv_started)
                {
                    (*send_to_hal)(pamhalctx, AMHAL_SPUV_START,NULL);
                    spuv_started=1;
                    ALOGV("sending AMHAL_SPUV_START");
                }
                (*send_to_hal)(pamhalctx, AMHAL_PCM_START,NULL);
                pcm_started=1;
                ALOGV("sending AMHAL_PCM_START");
            }
        }
    } else  if ((mode != AUDIO_MODE_IN_CALL) &&
               (hw_dev->mode == AUDIO_MODE_IN_CALL)) {

        /* notify modem about call end using at command */
        n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            if (dev->at_cmd_list) {
                char val[5];
                memset(val, 0, sizeof(val));
                sprintf(val,"%d\r\n", 0);
                ret = send_at_cmd_by_list(dev->at_cmd_list, AT_IN_CALL, val);
                if (!ret)
                    break;
            }
            n = list_get_next_node(n);
        }

        /*Restore the default lp mode*/
        if (restore_lp_mode(hw_dev) < 0)
            ALOGE("Error in restore_lp_mode\n");
        if(send_to_hal)
        {
            if(pcm_started) {
                (*send_to_hal)(pamhalctx, AMHAL_PCM_STOP,NULL);
                pcm_started = 0;
                ALOGV("sending AMHAL_PCM_STOP");
            }
            if (spuv_started)
            {
                (*send_to_hal)(pamhalctx, AMHAL_SPUV_STOP,NULL);
                spuv_started=0;
                ALOGV("sending AMHAL_SPUV_STOP");
            }
        }

        /* Close all voice call devices, and disable the call routes */
        n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);
            uint32_t device_call;

            dev->mode = AUDIO_MODE_NORMAL;
            device_call =  dev->devices & AUDIO_DEVICE_BIT_IN ?
                                   hw_dev->call_devices_capture :
                                   hw_dev->call_devices;

            if (dev->devices & device_call) {
                pthread_mutex_lock(&dev->lock);
                set_route(dev, 0, device_call, NULL);
                close_call_alsa_dev(dev);
                pthread_mutex_unlock(&dev->lock);

            }
            n = list_get_next_node(n);
        }

        /* Delete streams from active_stream_list which are
           not present in the stream_out_list */
        n = list_get_head(hw_dev->dev_list[STREAM_OUT]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);
            node_handle n_j = list_get_head(dev->active_stream_list);

            while (n_j) {
                struct nvaudio_stream_out *out = list_get_node_data(n_j);
                n_j = list_get_next_node(n_j);

                if (!list_find_data(hw_dev->stream_out_list, out))
                    list_delete_data(dev->active_stream_list, out);
            }
            n = list_get_next_node(n);
        }

        /* Disable routes for out stream, routes will be re-enabled
           after closing all devices */
        n = list_get_head(hw_dev->stream_out_list);
        while (n) {
            struct nvaudio_stream_out *out = list_get_node_data(n);
            node_handle n_j = list_get_head(out->dev_list);

            pthread_mutex_lock(&out->lock);
            while (n_j) {
                struct alsa_dev *dev = list_get_node_data(n_j);

                pthread_mutex_lock(&dev->lock);
                set_route(dev, 0, out->devices, &out->stream.common);
                close_alsa_dev(&out->stream.common, dev);
                pthread_mutex_unlock(&dev->lock);

                n_j = list_get_next_node(n_j);
            }
            pthread_mutex_unlock(&out->lock);
            n = list_get_next_node(n);
        }
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));

        /* Re-enable routing */
        n = list_get_head(hw_dev->stream_out_list);
        while (n) {
            struct nvaudio_stream_out *out = list_get_node_data(n);
            node_handle n_j = list_get_head(out->dev_list);

            pthread_mutex_lock(&out->lock);
            while (n_j) {
                struct alsa_dev *dev = list_get_node_data(n_j);

                pthread_mutex_lock(&dev->lock);
                set_route(dev, out->devices, 0, &out->stream.common);
                pthread_mutex_unlock(&dev->lock);

                n_j = list_get_next_node(n_j);
            }
            pthread_mutex_unlock(&out->lock);
            n = list_get_next_node(n);
        }

        hw_dev->call_devices = 0;
        hw_dev->call_devices_capture = 0;
    }

    hw_dev->mode = mode;
    pthread_mutex_unlock(&hw_dev->lock);
    return 0;
}

static int nvaudio_dev_set_mic_mute(struct audio_hw_device *dev,
                                    bool state)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n;
    uint32_t in_devices;

    ALOGV("%s : state %d", __FUNCTION__, state);

    pthread_mutex_lock(&hw_dev->lock);

    if (hw_dev->mic_mute == state)
        goto exit;

    if (hw_dev->mode == AUDIO_MODE_IN_CALL) {
        n = list_get_head(hw_dev->dev_list[STREAM_CALL]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);
            uint32_t device_call;
            device_call =  dev->devices & AUDIO_DEVICE_BIT_IN ?
                                   hw_dev->call_devices_capture :
                                   0;
            if (dev->devices & device_call) {
                set_mute(dev, device_call, state);
            }
            n = list_get_next_node(n);
        }
    } else {
        n = list_get_head(hw_dev->stream_in_list);
        while (n) {
            struct nvaudio_stream_in *in = list_get_node_data(n);

            n = list_get_next_node(n);
            pthread_mutex_lock(&in->lock);
            in_devices = in->dev->devices & ~AUDIO_DEVICE_BIT_IN;
            set_mute(in->dev, in->devices, state);
            pthread_mutex_unlock(&in->lock);
        }
    }
    hw_dev->mic_mute = state;

exit:
    pthread_mutex_unlock(&hw_dev->lock);
    return 0;
}

static int nvaudio_dev_get_mic_mute(const struct audio_hw_device *dev,
                                    bool *state)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    *state = hw_dev->mic_mute;

    ALOGV("%s : mute %d", __FUNCTION__, *state);
    return 0;
}

static size_t nvaudio_dev_get_input_buffer_size(
                                         const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    node_handle n = list_get_head(hw_dev->dev_list[STREAM_IN]);
    size_t buffer_size = 1024;
    uint32_t sample_rate;
    audio_format_t format;
    int channel_count;
    uint32_t device_sample_rate = DEFAULT_CONFIG_RATE;

    sample_rate = config->sample_rate;
    format = config->format;
    channel_count =  pop_count(config->channel_mask);

    ALOGV("%s : rate %d fmt %d ch %d",
          __FUNCTION__, sample_rate, format, channel_count);


    buffer_size = (DEFAULT_PERIOD_SIZE * config->sample_rate) /
                           device_sample_rate;
    buffer_size = (buffer_size / 16) * 16;
    buffer_size *= (channel_count * ((format == AUDIO_FORMAT_PCM_16_BIT) ? 2 :
                        (format == AUDIO_FORMAT_PCM_32_BIT) ? 4 :
                        (format == AUDIO_FORMAT_PCM_8_BIT) ? 1 : 3));

    ALOGV("%s: size %d", __FUNCTION__, buffer_size);

    return buffer_size;
}

static int nvaudio_dev_open_input_stream(struct audio_hw_device *dev,
                                 audio_io_handle_t handle,
                                 audio_devices_t devices,
                                 struct audio_config *config,
                                 struct audio_stream_in **stream_in)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    struct nvaudio_stream_in *in;
    node_handle n;
    pthread_mutexattr_t attr;

    ALOGV("%s : fmt %d ch %d rate %d",
         __FUNCTION__, config->format, config->channel_mask, config->sample_rate);

    pthread_mutex_lock(&hw_dev->lock);

    in = (struct nvaudio_stream_in *)calloc(1, sizeof(*in));
    if (!in) {
        pthread_mutex_unlock(&hw_dev->lock);
        return -ENOMEM;
    }

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&in->lock, &attr);

    in->hw_dev = hw_dev;

    in->stream.common.get_sample_rate = nvaudio_in_get_sample_rate;
    in->stream.common.set_sample_rate = nvaudio_in_set_sample_rate;
    in->stream.common.get_buffer_size = nvaudio_in_get_buffer_size;
    in->stream.common.get_channels = nvaudio_in_get_channels;
    in->stream.common.get_format = nvaudio_in_get_format;
    in->stream.common.set_format = nvaudio_in_set_format;
    in->stream.common.standby = nvaudio_in_standby;
    in->stream.common.dump = nvaudio_in_dump;
    in->stream.common.set_parameters = nvaudio_in_set_parameters;
    in->stream.common.get_parameters = nvaudio_in_get_parameters;
    in->stream.common.add_audio_effect = nvaudio_in_add_audio_effect;
    in->stream.common.remove_audio_effect = nvaudio_in_remove_audio_effect;
    in->stream.set_gain = nvaudio_in_set_gain;
    in->stream.read = nvaudio_in_read;
    in->stream.get_input_frames_lost = nvaudio_in_get_input_frames_lost;

    devices &= ~AUDIO_DEVICE_BIT_IN;
    n = list_get_head(hw_dev->dev_list[STREAM_IN]);
    while (n) {
        struct alsa_dev *dev = list_get_node_data(n);

        n = list_get_next_node(n);

        pthread_mutex_lock(&dev->lock);
        if (hw_dev->mode != AUDIO_MODE_IN_CALL)
            set_route(dev, devices, 0, &in->stream.common);
        pthread_mutex_unlock(&dev->lock);

        if (dev->devices & devices) {
            in->dev = dev;
            break;
        }
    }

    if(in->dev == NULL && (devices & AUDIO_DEVICE_IN_REMOTE_SUBMIX))
    {
        nvaudio_dev_open_input_stream_submix(dev, config);
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, 0);
    }

    if (!(config->format))
        config->format = ALSA_TO_AUDIO_FORMAT(in->dev->config.format);
    if (!(config->channel_mask))
        config->channel_mask = audio_in_channel_mask(in->dev->config.channels);
    if (!(config->sample_rate))
        config->sample_rate = in->dev->config.rate;

    in->io_handle = handle;
    in->format = config->format;
    in->channels = config->channel_mask;
    in->channel_count = pop_count(config->channel_mask);
    in->rate = config->sample_rate;
    in->devices = devices;
    in->standby = true;

    hw_dev->mic_mute = false;

    list_push_data(hw_dev->stream_in_list, in);
    *stream_in = &in->stream;

    pthread_mutex_unlock(&hw_dev->lock);
    return 0;
}

static void nvaudio_dev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *in)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;
    struct nvaudio_stream_in *in_stream = (struct nvaudio_stream_in *)in;
    struct alsa_dev *adev = in_stream->dev;

    ALOGV("%s ", __FUNCTION__);

    pthread_mutex_lock(&hw_dev->lock);

    if((adev == NULL) && (in_stream->source == AUDIO_SOURCE_REMOTE_SUBMIX)) {
        nvaudio_dev_close_input_stream_submix(dev);
        if (hw_dev->avp_audio)
            avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));
    }
    else {
        if (hw_dev->mode != AUDIO_MODE_IN_CALL)
            set_route(adev, 0, in_stream->devices, &in_stream->stream.common);

        if (hw_dev->mode == AUDIO_MODE_IN_CALL)
            call_record_at_cmd(hw_dev, in_stream, 0);
    }

    if (in_stream->resampler)
        release_resampler(in_stream->resampler);
    if (in_stream->buffer)
        free(in_stream->buffer);

    pthread_mutex_destroy(&in_stream->lock);
    list_delete_data(hw_dev->stream_in_list, in_stream);
    if(in_stream->fdump){
        fclose(in_stream->fdump);
        in_stream->fdump = NULL;
    }
    free(in_stream);
    pthread_mutex_unlock(&hw_dev->lock);
}

static int nvaudio_dev_dump(const audio_hw_device_t *device, int fd)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)device;

    ALOGV("%s", __FUNCTION__);

    /* TODO */

    return 0;
}

static int nvaudio_dev_close(hw_device_t *device)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)device;
    node_handle n;
    uint32_t i;

    ALOGV("%s ", __FUNCTION__);

    pthread_mutex_lock(&hw_dev->lock);


    if (pamhalctx != NULL)
    {
        (*close_hal)(pamhalctx);
    }

    if (hw_dev->avp_audio)
        avp_audio_close(hw_dev->avp_audio);

    list_destroy(hw_dev->stream_in_list);
    list_destroy(hw_dev->stream_out_list);
#ifdef AUDIO_TUNE
    close_tune(&hw_dev->tune);
#endif

    for (i = 0; i < STREAM_MAX; i++) {
        n = list_get_head(hw_dev->dev_list[i]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);
            node_handle n_j;

            pthread_mutex_destroy(&dev->lock);
            pthread_mutex_destroy(&dev->mix_lock);
            pthread_cond_destroy(&dev->cond);

            /*free device mix buffer*/
            if (STREAM_OUT == i && dev->mix_buffer) {
                free(dev->mix_buffer);
                free(dev->mix_buffer_bup);
                dev->mix_buffer = NULL;
                dev->mix_buffer_bup = NULL;
            }

            if (dev->avp_stream)
                avp_stream_close(dev->avp_stream);
            if (dev->fast_avp_stream)
                avp_stream_close(dev->fast_avp_stream);
            if (dev->pcm)
                pcm_close(dev->pcm);
            if (dev->mixer)
                mixer_close(dev->mixer);
            if (dev->card_name)
                free(dev->card_name);

            free_route_list(dev->on_route_list);
            free_route_list(dev->off_route_list);
            free_at_cmd_list(dev->at_cmd_list);
            n_j = list_get_head(dev->dev_cfg_list);
            while (n_j) {
                struct alsa_dev_cfg *dev_cfg = list_get_node_data(n_j);

                free_route_list(dev_cfg->on_route_list);
                free_route_list(dev_cfg->off_route_list);
                free_at_cmd_list(dev_cfg->at_cmd_list);
                n_j = list_get_next_node(n_j);
            }
            list_destroy(dev->dev_cfg_list);
            free(dev);
            n_j = list_get_next_node(n_j);
        }
        list_destroy(hw_dev->dev_list[i]);
    }

    nvaudio_dev_close_submix((struct audio_hw_device*)hw_dev);

    pthread_mutex_unlock(&hw_dev->lock);
    pthread_mutex_destroy(&hw_dev->lock);
    free(hw_dev);
    return 0;
}

static int nvaudio_dev_set_master_mute(struct audio_hw_device *dev, bool mute)
{
    /* Anything other than 0 means that we don't support this API,
     * and it will be emulated */
    return -1;
}

static int nvaudio_dev_open(const hw_module_t* module, const char* name,
                            hw_device_t** device)
{
    struct nvaudio_hw_device *hw_dev;
    node_handle n;
    int ret, i;
    int channels;
    pthread_mutexattr_t attr;
    char prop_val[PROPERTY_VALUE_MAX] = "";

    ALOGV("%s : %s", __FUNCTION__, name);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE))
        return -EINVAL;

    hw_dev = calloc(1, sizeof(*hw_dev));
    if (!hw_dev)
        return -ENOMEM;

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&hw_dev->lock, &attr);
    hw_dev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    hw_dev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    hw_dev->hw_device.common.module = (struct hw_module_t *) module;
    hw_dev->hw_device.common.close = nvaudio_dev_close;

    hw_dev->hw_device.init_check = nvaudio_dev_init_check;
    hw_dev->hw_device.set_voice_volume = nvaudio_dev_set_voice_volume;
    hw_dev->hw_device.set_master_volume = nvaudio_dev_set_master_volume;
    hw_dev->hw_device.set_mode = nvaudio_dev_set_mode;
    hw_dev->hw_device.set_mic_mute = nvaudio_dev_set_mic_mute;
    hw_dev->hw_device.get_mic_mute = nvaudio_dev_get_mic_mute;
    hw_dev->hw_device.set_parameters = nvaudio_dev_set_parameters;
    hw_dev->hw_device.get_parameters = nvaudio_dev_get_parameters;
    hw_dev->hw_device.get_input_buffer_size = nvaudio_dev_get_input_buffer_size;
    hw_dev->hw_device.open_output_stream = nvaudio_dev_open_output_stream;
    hw_dev->hw_device.close_output_stream = nvaudio_dev_close_output_stream;
    hw_dev->hw_device.open_input_stream = nvaudio_dev_open_input_stream;
    hw_dev->hw_device.close_input_stream = nvaudio_dev_close_input_stream;
    hw_dev->hw_device.set_master_mute = nvaudio_dev_set_master_mute;
    hw_dev->hw_device.dump = nvaudio_dev_dump;
    strcpy(hw_dev->lp_state_default, "\0");

    for (i = 0; i < STREAM_MAX; i++)
        hw_dev->dev_list[i] = list_create();

    hw_dev->stream_out_list = list_create();
    hw_dev->stream_in_list = list_create();
    hw_dev->mode = AUDIO_MODE_NORMAL;
    hw_dev->ulp_rates_str[0] = '\0';
    hw_dev->master_volume = 1.0;

    ret = config_parse(hw_dev);
    if (ret != 0) {
        ALOGE("Failed to parse XML config file");
        ret = -ENOMEM;
        goto err_exit;
    }
    instantiate_nvaudio_service(&hw_dev->hw_device);

    property_get("ro.boot.modem", prop_val, "");
    ALOGV("ro.boot.modem = %s",prop_val);

    nvaudio_dev_open_submix((struct audio_hw_device*)hw_dev);

    if(!strcmp(prop_val, "rmc"))
    {
        amhallib = dlopen(HAL_LIB, RTLD_LAZY);
        ALOGV("load amhal =%x",(int)amhallib);

        if(amhallib)
        {
            open_hal = dlsym(amhallib, HAL_OPEN);
            ALOGV("amhal_open =%x",(int)open);

            close_hal = dlsym(amhallib, HAL_CLOSE);
            ALOGV("amhal_close =%x",(int)close_hal);

            send_to_hal = dlsym(amhallib,HAL_SENDTO);
            ALOGV("amhal_sendto =%x",(int)send_to_hal);

            if(open_hal)
            {
                pamhalctx = (*open_hal)(NULL);
                if (pamhalctx == NULL)
                {
                  return -1;
                }
            }
        }
    } else {
        amhallib = NULL;
        open_hal = NULL;
        close_hal = NULL;
        send_to_hal = NULL;
    }

    for (i = 0; i < STREAM_MAX; i++) {
        n = list_get_head(hw_dev->dev_list[i]);
        while (n) {
            struct alsa_dev *dev = list_get_node_data(n);

            n = list_get_next_node(n);

            pthread_mutex_init(&dev->lock, &attr);
            pthread_mutex_init(&dev->mix_lock, &attr);
            pthread_cond_init(&dev->cond, NULL);

            set_route_by_array(dev->mixer,
                               dev->off_route_list,
                               dev->hw_dev->mode,
                               dev->config.rate);
            node_handle n_j = list_get_head(dev->dev_cfg_list);
            while (n_j) {
                struct alsa_dev_cfg *dev_cfg = list_get_node_data(n_j);

                set_route_by_array(dev->mixer,
                                   dev_cfg->off_route_list,
                                   dev->hw_dev->mode,
                                   dev->config.rate);
                n_j = list_get_next_node(n_j);
            }

            /*allocate device mix buffer*/
            if (STREAM_OUT == i) {
                channels =
                dev->id != DEV_ID_AUX ? dev->default_config.channels : 8;

                dev->mix_buff_size =
                (dev->config.period_size)*(channels)*(2);
                dev->mix_buffer = malloc(dev->mix_buff_size);
                if (!dev->mix_buffer) {
                    ALOGE("Failed to allocate device mix buffer");
                    ret = -ENOMEM;
                    goto err_exit;
                }
                dev->mix_buffer_bup = malloc(dev->mix_buff_size);
                if (!dev->mix_buffer_bup) {
                    ALOGE("Failed to allocate device mix buffer");
                    ret = -ENOMEM;
                    goto err_exit;
                }
                memset(dev->mix_buffer, 0, dev->mix_buff_size);
            }

            /* Initialize AVP audio driver if ULP is supported */
            if (dev->ulp_supported) {
                hw_dev->avp_audio = avp_audio_open();
                if (!hw_dev->avp_audio) {
                    ALOGE("Failed to initialize avp audio driver");
                    dev->ulp_supported = 0;
                    continue;
                }
                avp_audio_register_dma_trigger_cb(hw_dev->avp_audio, dev,
                                                  trigger_dma_callback);

                dev->avp_stream = avp_stream_open(hw_dev->avp_audio,
                                                  STREAM_TYPE_PCM,
                                                  dev->config.rate);
                if (!dev->avp_stream) {
                    ALOGE("Failed to initialize avp audio stream");
                }

                dev->fast_avp_stream = avp_stream_open(hw_dev->avp_audio,
                                                  STREAM_TYPE_FAST_PCM,
                                                  dev->config.rate);
                if (!dev->fast_avp_stream) {
                    ALOGE("Failed to initialize avp audio stream");
                }

                if (!dev->fast_avp_stream || !dev->avp_stream) {
                    avp_audio_close(hw_dev->avp_audio);
                    hw_dev->avp_audio = 0;
                    dev->ulp_supported = 0;
                    continue;
                }

                /* Ensure all relevant alsa-ctls are present */
                dev->pcm = pcm_open(dev->card,
                                    dev->device_id,
                                    dev->pcm_flags,
                                    &dev->config);
                if (!pcm_is_ready(dev->pcm)) {
                    ALOGE("cannot open pcm: %s", pcm_get_error(dev->pcm));
                    dev->pcm = NULL;
                    continue;
                }

                ret = set_avp_render_path(dev, hw_dev->avp_audio);
                if (ret < 0) {
                    ALOGE("Failed to setup avp render path");
                    avp_stream_close(dev->avp_stream);
                    avp_stream_close(dev->fast_avp_stream);
                    avp_audio_close(hw_dev->avp_audio);
                    dev->avp_stream = 0;
                    dev->fast_avp_stream = 0;
                    hw_dev->avp_audio = 0;
                    dev->ulp_supported = 0;
                }

                if (dev->pcm) {
                    pcm_close(dev->pcm);
                    dev->pcm = NULL;
                }

                if (hw_dev->avp_audio)
                    avp_audio_set_ulp(hw_dev->avp_audio, is_ulp_supported(hw_dev));
            }
        }
    }
#ifdef NVAUDIOFX
    //Initialize the audio effects to derfault
    nvaudiofx_default();
#endif

    *device = &hw_dev->hw_device.common;
#ifdef NVOICE
    if(nvoice_supported)
        nvoice_parse_config();
#endif
#ifdef AUDIO_TUNE
    ret = config_tune_parse(&hw_dev->tune);
    if (ret != 0)
        ALOGE("Failed to tune parse XML config file");
    else
        config_tune_dump(&hw_dev->tune);
#endif
    return 0;

err_exit:
    free(hw_dev);
    return ret;
}

static struct hw_module_methods_t nvaudio_module_methods = {
    .open = nvaudio_dev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Nvidia Audio HW HAL",
        .author = "NVIDIA Corporation",
        .methods = &nvaudio_module_methods,
    },
};
