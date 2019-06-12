/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef _NVAUDIOFX_INTERFACE_H
#define _NVAUDIOFX_INTERFACE_H

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

float fx_vol;
char fx_set_param[PROPERTY_VALUE_MAX];
char fx_get_param[PROPERTY_VALUE_MAX];
char fx_value[PROPERTY_VALUE_MAX];


void nvaudiofx_parse_config(void);

bool nvaudiofx_configure(void);

bool nvaudiofx_init(void);

bool nvaudiofx_default(void);

void nvaudiofx_process(void*buffer,int samples,float fx_vol);

void nvaudiofx_delete(void);

void nvaudiofx_set_param(uint16_t param, int32_t value);

int32_t nvaudiofx_get_param(uint16_t param);

void nvaudiofx_enable_mdrc(uint32_t param);

void nvaudiofx_setoutdevice(uint32_t device);

void nvaudiofx_set_rate(uint32_t rate);


#endif
/** @} END OF FILE */

