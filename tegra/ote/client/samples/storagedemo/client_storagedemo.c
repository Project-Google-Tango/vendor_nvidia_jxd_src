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
#include <stdlib.h>

#include <client/ote_client.h>

#include "nvos.h"

/** Service Identifier */
/* {32456890-8572-4a6f-a1f1-03aa9b05f9ff} */
#define SERVICE_STORAGE_DEMO_UUID \
	{ 0x32456890, 0x8572, 0x4A6F, \
		{ 0xA1, 0xF1, 0x03, 0xAA, 0x9B, 0x05, 0xF9, 0xff } }

#define MAX_ITERATIONS 100

static void secure_client_test(void)
{
	te_error_t err = OTE_SUCCESS;
	te_session_t session;
	te_operation_t operation;
	te_service_id_t uuid = SERVICE_STORAGE_DEMO_UUID;
	uint32_t return_origin = 0;
	int i;

	NvOsDebugPrintf("Storage demo: Create/Read/Write/Delete file from secure TA\n");

	te_init_operation(&operation);
	te_oper_set_param_int_rw(&operation, 0, 0);

	for (i = 1; i <= MAX_ITERATIONS; i++) {
		err = te_open_session(&session, &uuid, &operation);
		if (err != OTE_SUCCESS) {
			NvOsDebugPrintf("Storage demo: Iteration %d fail (%x)\n\n",
					i, err);
			break;
		}

		te_close_session(&session);
		NvOsDebugPrintf("Storage demo: Iteration %d passed\n\n", i);
	}

	te_operation_reset(&operation);

	if (err == OTE_SUCCESS)
		NvOsDebugPrintf("***** Storage demo: passed *****\n\n");
	else
		NvOsDebugPrintf("***** Storage demo: failed *****\n\n");
}

int main(void)
{
	secure_client_test();
	return 0;
}
