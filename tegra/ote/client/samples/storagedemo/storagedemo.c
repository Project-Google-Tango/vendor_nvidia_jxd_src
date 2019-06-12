/*
* Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/ote_error.h>
#include <common/ote_common.h>
#include <service/ote_service.h>
#include <service/ote_storage.h>
#include <service/ote_service.h>

#define TEST_STR "Hello, Hello, from secure storage demo app."

te_error_t te_create_instance_iface(void)
{
	return OTE_SUCCESS;
}

void te_destroy_instance_iface(void)
{
	return;
}

static te_error_t start_app(void)
{
	char *test_str = NULL, *file_name = NULL;
	te_error_t err;
	uint32_t bytes_read;
	te_storage_object_t file_handle;

	file_name = (char *)calloc(1, 64);
	test_str = (char *)calloc(1, 256);
	if (!file_name || !test_str) {
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	strcpy(file_name, "TADemo.blob");
	strcpy(test_str, TEST_STR);

	/* file write */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "SSDemo: wr: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* write to the file */
	err = te_write_storage_object(file_handle, test_str,
			strlen(test_str)+1);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "SSDemo: te_write_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	te_fprintf(TE_INFO, "SSDemo: file write complete\n", bytes_read);

	/* file read */
	free(test_str);
	test_str = NULL;

	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "SSDemo: rd: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "SSDemo: te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	te_fprintf(TE_INFO, "SSDemo: File size = %d bytes\n", bytes_read);

	test_str = (char *)calloc(1, bytes_read);
	if (!test_str) {
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, test_str, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "SSDemo: te_read_storage_object fail %x\n", err);
		goto fail;
	}

	te_fprintf(TE_INFO, "SSDemo: %d bytes read\n", bytes_read);

	if (strncmp(test_str, TEST_STR, sizeof(TEST_STR))) {
		te_fprintf(TE_ERR, "SSDemo: *** Files do not match!!! ***\n");
		err = OTE_ERROR_SECURITY;
		goto fail;
	}

	te_close_storage_object(file_handle);

	/* file delete */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE_META,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "SSDemo: del: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* delete file and close */
	te_delete_storage_object(file_handle);
	te_close_storage_object(file_handle);

	te_fprintf(TE_INFO, "SSDemo: file delete complete\n", bytes_read);

fail:
	if (file_name)
		free(file_name);
	if (test_str)
		free(test_str);
	return err;
}

te_error_t te_open_session_iface(void **sessionContext, te_operation_t *te_op)
{
	te_fprintf(TE_INFO, "SSDemo: %s\n", __func__);
	return start_app();
}

void te_close_session_iface(void *sessionContext)
{
	te_fprintf(TE_INFO, "SSDemo: %s\n", __func__);
	return;
}

te_error_t te_receive_operation_iface(void *sessionContext,
		te_operation_t *te_op)
{
	te_fprintf(TE_INFO, "SSDemo: %s\n", __func__);
	return OTE_SUCCESS;
}
