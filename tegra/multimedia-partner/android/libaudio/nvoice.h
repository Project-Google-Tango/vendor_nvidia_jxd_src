/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NVOICE_H
#define _NVOICE_H
#include <cutils/properties.h>
#define NVOICE_SAMPLE_RATE 16000

#ifdef DECLARE_NVOICE_TYPES
char set_param[PROPERTY_VALUE_MAX];
char get_param[PROPERTY_VALUE_MAX];
char value[PROPERTY_VALUE_MAX];
bool nvoice_supported;
#endif

/**
 * This function is used to  open the nvoice handle
 * and configure the nvoice interfaces
 *
 */
bool nvoice_configure();

/**
 * The real function that handles RX signal of the
 * nvoicereceiver module. It's responsible to enhance speaker
 * quality as well as to suppress stationary noise if present in
 * DL
 *
 */
void nvoice_processrx(void *buffer,int bytes, int rate);

/**
 * The real function that handles the Tx signal of the nvoice
 * Acoustic Echo Canceler. It's responsible to remove echo and
 * suppress non-stationary noise in UL using dual micrphone
 * approach
 *
 */
void nvoice_processtx(void *buffer,int bytes, int rate);

/**
 * This function is used to delete nvoice handle and its
 * interfaces
 *
 */
void nvoice_delete();

/**
 * This function is used to set the echodelay
 *
 */
void nvoice_set_echo_delay(int deltadelay);

/**
 * This function is used to adjust the echodelay
 *
 */
void nvoice_adjust_echo_delay_line(int deltadelay);

/**
 * This function is used to parse the echoconfig file
 *
 */
void nvoice_parse_config();

/**
 * This function is used to set the indevice
 *
 */
void nvoice_setindevice(uint32_t device);

/**
 * This function is used to set the outdevice
 *
 */
void nvoice_setoutdevice(uint32_t device);

/**
 * This function is used to set the Nvoice parameters
 *
 */
void nvoice_set_param(uint16_t param, int16_t value);

/**
 * This function is used to get the Nvoice parameters
 *
 */
int16_t nvoice_get_param(uint16_t param);

#endif
