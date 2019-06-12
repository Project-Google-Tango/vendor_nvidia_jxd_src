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
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include "nvos.h"

#include <common/ote_command.h>
#include <client/ote_client.h>

#define BUF_SIZE 128

/** Service Identifier */
/* {130616f9-8572-4a6f-a1f1-03aa9b05f9ee} */
/* For testing only: using trusted_app/manifest.c UUID */

#define SERVICE_TRUSTEDAPP_UUID \
	{ 0x130616F9, 0x8572, 0x4A6F, \
		{ 0xA1, 0xF1, 0x03, 0xAA, 0x9B, 0x05, 0xF9, 0xEE } }

/** Service Identifier */
/* {23f616f9-8572-4a6f-a1f1-03aa9b05f9ff} */
/* For testing only: using trusted_app2/manifest.c UUID */

#define SERVICE_TRUSTEDAPP2_UUID \
	{ 0x23f616F9, 0x8572, 0x4A6F, \
		{ 0xA1, 0xF1, 0x03, 0xAA, 0x9B, 0x05, 0xF9, 0xFF } }

int main(void)
{
	int err = 0;
	te_session_t session;
	te_session_t session2;
	te_operation_t operation, operation2;
	int buffer[BUF_SIZE];
	char *buf = NULL, *op_buf = NULL;
	te_service_id_t uuid = SERVICE_TRUSTEDAPP_UUID;
	te_service_id_t uuid2 = SERVICE_TRUSTEDAPP2_UUID;
	uint32_t return_origin = 0, value_a, value_b, return_a, return_b;
	int session_opened = 0;

	buf = (char *)malloc(BUF_SIZE);
	if (!buf) {
		NvOsDebugPrintf("Memory allocation Failed\n");
		err = -ENOMEM;
		goto exit;
	}
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, "testing.....");

	op_buf = (char *)malloc(BUF_SIZE);
	if (!op_buf) {
		NvOsDebugPrintf("Memory allocation Failed\n");
		err = -ENOMEM;
		goto exit;
	}
	memset(op_buf, 0, BUF_SIZE);

	value_a = 0x55555555;
	value_b = 0x88888888;

	te_init_operation(&operation);
	te_oper_set_param_mem_ro(&operation, 0, buf, BUF_SIZE);
	te_oper_set_param_int_rw(&operation, 1, value_a);
	te_oper_set_param_int_rw(&operation, 2, value_b);

	err = te_open_session(&session, &uuid, &operation);
	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("te_open_session failed with err (%d)\n", err);
		goto exit;
	}
	session_opened = 1;

	te_oper_get_param_int(&operation, 1, &return_a);
	te_oper_get_param_int(&operation, 2, &return_b);

	if (return_a == (value_a + 1) && return_b == (value_b + 1)) {
		NvOsDebugPrintf("open_session test: Pass\n") ;
	} else {
		NvOsDebugPrintf("open_session test: Fail (mismatch values)\n");
		te_oper_dump_param_list(&operation);
	}

	te_operation_reset(&operation);
	te_oper_set_command(&operation, 101);
	err = te_launch_operation(&session, &operation);
	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("te_launch_operation failed with err (%d)\n",
				err);
		goto exit;
	}

	/* issue command 102 to get TA to fill op_buf */
	te_operation_reset(&operation);
	te_oper_set_param_mem_rw(&operation, 0, op_buf, BUF_SIZE);
	te_oper_set_command(&operation, 102);
	err = te_launch_operation(&session, &operation);
	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("te_launch_operation failed with err (%d)\n",
				err);
		goto exit;
	}
	if (!strcmp(op_buf, "writing_into_buf_from_LK_for_output"))
		NvOsDebugPrintf("launch_operation test for tmp buf: Pass\n");
	else
		NvOsDebugPrintf("launch_operation test for tmp buf: Fail\n");

	/* issue command 103 to get TA to swap params */
	te_operation_reset(&operation);
	value_a = 0x1111;
	value_b = 0x2222;
	te_oper_set_param_int_rw(&operation, 1, value_a);
	te_oper_set_param_int_rw(&operation, 2, value_b);
	te_oper_set_command(&operation, 103);

	err = te_launch_operation(&session, &operation);
	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("te_launch_operation failed with err (%d)\n",
				err);
		goto exit;
	}

	te_oper_get_param_int(&operation, 1, &return_a);
	te_oper_get_param_int(&operation, 2, &return_b);

	if ((return_a == value_b) && (return_b == value_a))
		NvOsDebugPrintf("te_launch_operation test to swap values: Pass\n");
	else
		NvOsDebugPrintf("te_launch_operation test to swap values: Fail\n");

	te_operation_reset(&operation);
	err = te_open_session(&session2, &uuid2, &operation);
	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("not able to open TA2 session as expected\n", err);
		err = OTE_SUCCESS;
	} else {
		NvOsDebugPrintf("open TA2 session should never pass\n", err);
		err = OTE_ERROR_ACCESS_CONFLICT;
		goto exit;
	}

exit:
	if (buf != NULL)
		free(buf);
	if (op_buf != NULL)
		free(op_buf);
	if (session_opened)
		te_close_session(&session);

	if (err != OTE_SUCCESS) {
		NvOsDebugPrintf("FAILURE\n");
	} else {
		NvOsDebugPrintf("SUCCESS\n");
	}
	return err;
}
