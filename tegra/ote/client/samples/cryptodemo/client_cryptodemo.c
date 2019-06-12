/*
* Copyright (c) 2012-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>

#include <common/ote_command.h>
#include <client/ote_client.h>

#define BUF_SIZE 128

/** Service Identifier */
/* {23457890-8572-4a6f-a1f1-03aa9b05f9ff} */
/* For testing used  CRYPTO DEMO UUID*/

#define SERVICE_CRYPTO_DEMO_UUID \
	{ 0x23457890, 0x8572, 0x4A6F, \
		{ 0xA1, 0xF1, 0x03, 0xAA, 0x9B, 0x05, 0xF9, 0xff } }

int main(void)
{
	te_error_t err = OTE_SUCCESS;
	te_session_t session;
	te_operation_t operation;

	te_service_id_t uuid = SERVICE_CRYPTO_DEMO_UUID;

	te_init_operation(&operation);

	err = te_open_session(&session, &uuid, &operation);
	if (err != OTE_SUCCESS) {
		printf("te_open_session failed with err (%d)\n", err);
		goto exit;
	}

	te_oper_set_command(&operation, 1);
	err = te_launch_operation(&session, &operation);

	te_close_session(&session);

exit:
	return err;
}
