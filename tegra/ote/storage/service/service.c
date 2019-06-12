/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <common/ote_command.h>
#include <service/ote_memory.h>
#include <service/ote_crypto.h>
#include <service/ote_service.h>
#include <service/ote_storage.h>

#define AES_KEYSIZE		16
#define AES_BLOCK_SIZE		16

#define MAX_KEY_SIZE_BYTES	32
#define MAX_IV_LEN_BYTES	16

static te_device_unique_id device_unique_id;
static uint8_t zero_iv[MAX_IV_LEN_BYTES];
static uint8_t master_key[MAX_KEY_SIZE_BYTES];
static uint8_t content_key[MAX_KEY_SIZE_BYTES];

static void crypt_data(te_oper_crypto_algo_mode_t mode,
		te_oper_crypto_algo_t algo, uint8_t *key, uint8_t *iv,
		uint8_t *in, uint32_t len, uint8_t *out)
{
	int i;
	te_crypto_object_t object = NULL;
	te_crypto_operation_t enop = NULL;

	te_error_t err = te_allocate_object(&object);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "crypto object allocation failed %x\n", err);
		return;
	}

	/* allocate encrypt/decrypt operation */
	err = te_allocate_operation(&enop, algo, mode);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "allocate operation Error %x\n", err);
		goto exit;
	}

	/* set the key */
	te_attribute_t sk;
	err = te_set_mem_attribute(&sk, OTE_ATTR_SECRET_VALUE, key,
			AES_KEYSIZE);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "set_mem_attribute error %x\n", err);
		goto exit;
	}

	err = te_populate_object(object, &sk, 1);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "populate object error %x\n", err);
		goto exit;
	}

	err = te_set_operation_key(enop, object);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "Set Key Error %x\n", err);
		goto exit;
	}

	/* set iv */
	err = te_cipher_init(enop, iv, MAX_IV_LEN_BYTES);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "cipher init error %x\n", err);
		goto exit;
	}

	size_t destlen = len;
	err = te_cipher_update(enop, in, len, out, &destlen);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "cipher update error %x\n", err);
	}

exit:
	te_free_operation(enop);
	te_free_object(object);
}

static te_error_t calculate_mac(uint8_t *key, uint8_t *in, uint32_t len, uint8_t *out)
{
	te_crypto_object_t object = NULL;
	te_crypto_operation_t macop = NULL;

	te_error_t err = te_allocate_object(&object);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "crypto object allocation failed %x\n", err);
		return err;
	}

	/* allocate cmac operation */
	err = te_allocate_operation(&macop, OTE_ALG_AES_CMAC_128, OTE_ALG_MODE_DIGEST);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "allocate operation Error %x\n", err);
		goto exit;
	}

	/* set the key */
	te_attribute_t sk;
	err = te_set_mem_attribute(&sk, OTE_ATTR_SECRET_VALUE, key,
			AES_KEYSIZE);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "set_mem_attribute error %x\n", err);
		goto exit;
	}

	err = te_populate_object(object, &sk, 1);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "populate object error %x\n", err);
		goto exit;
	}

	err = te_set_operation_key(macop, object);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "Set Key Error %x\n", err);
		goto exit;
	}

	/* cmac init */
	err = te_cipher_init(macop, NULL, 0);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "cipher init error %x\n", err);
		goto exit;
	}

	/* calculate cmac */
	size_t destlen = AES_BLOCK_SIZE;
	te_mem_fill(out, 0, AES_BLOCK_SIZE);
	err = te_cipher_update(macop, in, len, out, &destlen);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "cipher update error %x\n", err);
		goto exit;
	}

	int i;
	te_fprintf(TE_SECURE, "dumping cmac ...\n");
	for (i = 0; i < AES_BLOCK_SIZE; i++)
		te_fprintf(TE_SECURE, "0x%x ", out[i]);
	te_fprintf(TE_SECURE, "\n");

exit:
	te_free_operation(macop);
	te_free_object(object);

	return err;
}

static te_error_t encrypt_then_mac(uint8_t *key, uint8_t *in, uint32_t len,
			uint8_t *out)
{
	te_error_t err;
	uint8_t mac[AES_BLOCK_SIZE];
	te_oper_crypto_algo_mode_t mode = OTE_ALG_MODE_ENCRYPT;
	te_oper_crypto_algo_t algo = OTE_ALG_AES_CTS;
	uint8_t cmac_key[MAX_KEY_SIZE_BYTES];

	te_mem_fill(zero_iv, 0, MAX_IV_LEN_BYTES);

	/* encrypt the file data */
	crypt_data(mode, algo, key, zero_iv, in, len, in);
	crypt_data(mode, algo, master_key, zero_iv, in, len, in);
	crypt_data(mode, algo, content_key, zero_iv, in, len, out);

	/* generate cmac key */
	crypt_data(OTE_ALG_MODE_ENCRYPT, OTE_ALG_AES_CBC, master_key, zero_iv,
		key, MAX_KEY_SIZE_BYTES, cmac_key);

	/* calculate MAC */
	err = calculate_mac(cmac_key, out, len, mac);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: calculate_mac fail (%d)\n", __func__, err);
		return err;
	}
	te_mem_move(out + len, mac, AES_BLOCK_SIZE);
	return OTE_SUCCESS;
}

static te_error_t validate_then_decyrpt(uint8_t *key, uint8_t *in, uint32_t len,
			uint8_t *out)
{
	te_error_t err;
	uint8_t mac[AES_BLOCK_SIZE];
	te_oper_crypto_algo_mode_t mode = OTE_ALG_MODE_DECRYPT;
	te_oper_crypto_algo_t algo = OTE_ALG_AES_CTS;
	uint8_t cmac_key[MAX_KEY_SIZE_BYTES];

	/* generate cmac key */
	crypt_data(OTE_ALG_MODE_ENCRYPT, OTE_ALG_AES_CBC, master_key, zero_iv,
		key, MAX_KEY_SIZE_BYTES, cmac_key);

	/* validate MAC */
	err = calculate_mac(cmac_key, in, len, mac);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: calculate_mac fail (%d)\n", __func__, err);
		return err;
	}
	if (te_mem_compare(mac, in + len, AES_BLOCK_SIZE)) {
		te_fprintf(TE_ERR, "%s: te_mem_compare fail (%d)\n", __func__, err);
		return OTE_ERROR_ACCESS_DENIED;
	}

	/* decrypt file data */
	te_mem_fill(zero_iv, 0, MAX_IV_LEN_BYTES);

	crypt_data(mode, algo, content_key, zero_iv, in, len, in);
	crypt_data(mode, algo, master_key, zero_iv, in, len, in);
	crypt_data(mode, algo, key, zero_iv, in, len, out);

	return OTE_SUCCESS;
}

#define MAX_ENCRYPT_ITERATIONS 5

te_error_t te_create_instance_iface(void)
{
	te_error_t err;
	int error, i;
	uint8_t some_key[MAX_KEY_SIZE_BYTES] = {
		0x60, 0x3d, 0xeb, 0x10,
		0x15, 0xca, 0x71, 0xbe,
		0x2b, 0x73, 0xae, 0xf0,
		0x85, 0x7d, 0x77, 0x81,
		0x1f, 0x35, 0x2c, 0x07,
		0x3b, 0x61, 0x08, 0xd7,
		0x2d, 0x98, 0x10, 0xa3,
		0x09, 0x14, 0xdf, 0xf4
	};
	uint8_t some_iv[MAX_IV_LEN_BYTES] = {
		'n', 'v', '-', 's', 't', 'o', 'r', 'a',
		'g', 'e', '-', 'd', 'u', 'm', 'm', 'y'
	};
	uint8_t iv[MAX_KEY_SIZE_BYTES];

	/* get device UID to generate master/content keys */
	err = te_get_device_unique_id(&device_unique_id);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "storage_service: get device uid failed (%d)\n", err);
		return err;
	}

	/* master_key = (device_key << 16bytes) | device_key */
	te_mem_move(master_key, &device_unique_id.id, DEVICE_UID_SIZE_BYTES);
	te_mem_move(&master_key[MAX_KEY_SIZE_BYTES >> 1], &device_unique_id.id,
		DEVICE_UID_SIZE_BYTES);

	/* generate iv for the next stages */
	crypt_data(OTE_ALG_MODE_ENCRYPT, OTE_ALG_AES_CBC, some_key, some_iv,
			master_key, MAX_KEY_SIZE_BYTES, iv);

	/* generate key for the next stages */
	crypt_data(OTE_ALG_MODE_ENCRYPT, OTE_ALG_AES_CBC, some_key, some_iv, iv,
			MAX_KEY_SIZE_BYTES, some_key);

	te_mem_move(some_iv, iv, MAX_IV_LEN_BYTES);

	/* generate master key and content key */
	for (i = 0; i < MAX_ENCRYPT_ITERATIONS; i++) {
		some_iv[0] += i;
		some_key[0] += i;

		crypt_data(OTE_ALG_MODE_ENCRYPT, OTE_ALG_AES_CBC, some_key,
				some_iv, master_key, MAX_KEY_SIZE_BYTES,
				master_key);

		/* snap a random content key in between */
		if (i == 3)
			te_mem_move(content_key, master_key,
					MAX_KEY_SIZE_BYTES);
	}

	return OTE_SUCCESS;
}

void te_destroy_instance_iface(void)
{
	te_mem_fill(content_key, 0, MAX_KEY_SIZE_BYTES);
	te_mem_fill(master_key, 0, MAX_KEY_SIZE_BYTES);
	te_mem_fill(&device_unique_id.id, 0, DEVICE_UID_SIZE_BYTES);
	return;
}

te_error_t te_receive_operation_iface(void *sessionContext,
		te_operation_t *te_op)
{
	te_error_t result = OTE_SUCCESS;
	char *filename = NULL;
	uint8_t *bufp = NULL;
	uint32_t len;
	te_storage_rw_req_args_t args;
	int error;
	uint32_t command;
	te_identity_t identity;

	/* get the client UUID */
	memset(&identity, 0, sizeof(identity));
	result = te_get_client_ta_identity(&identity);
	if (result != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "storage_service: %s: access conflict\n", __func__);
		return OTE_ERROR_ACCESS_CONFLICT;
	}

	command = te_oper_get_command(te_op);

	te_fprintf(TE_SECURE, "storage_service: %s: new command %d received\n", __func__,
			command);

	if (command == 301) { /* file write */

		result = te_oper_get_param_mem(te_op, 0, (void *)&filename,
				&len);
		if (result != OTE_SUCCESS && len > TE_STORAGE_OBJID_MAX_LEN)
			goto fail;
		uint32_t file_length = len;

		result = te_oper_get_param_mem(te_op, 1, (void *)&bufp, &len);
		if (result != OTE_SUCCESS)
			goto fail;

		if (!filename || !bufp)
			return OTE_ERROR_BAD_PARAMETERS;

		/* Adding filename and data to temp buffer and then encrypting */
		uint8_t *dst = te_mem_alloc(len + AES_BLOCK_SIZE
						+ TE_STORAGE_OBJID_MAX_LEN);
		if (!dst)
			return OTE_ERROR_OUT_OF_MEMORY;

		te_mem_move((void *)dst, (void *)filename, file_length);
		te_mem_move((void *)(dst + TE_STORAGE_OBJID_MAX_LEN), (void *)bufp, len);

		result = encrypt_then_mac((uint8_t *)&identity.uuid, dst,
				len + TE_STORAGE_OBJID_MAX_LEN, dst);

		if (result != OTE_SUCCESS) {
			te_mem_free(dst);
			te_oper_set_param_int_rw(te_op, 2, 0);
			goto fail;
		}


		/* write it out */
		args.filename = filename;
		args.buffer = dst;
		args.buf_len = len + AES_BLOCK_SIZE + TE_STORAGE_OBJID_MAX_LEN;
		args.result = 0;
		error = ioctl(1, OTE_IOCTL_FILE_WRITE, &args);
		te_mem_free(dst);
		if (error < 0) {
			result = OTE_ERROR_COMMUNICATION;
			goto fail;
		}

		/* bytes written */
		if ((uint32_t)args.result == len + AES_BLOCK_SIZE
						+ TE_STORAGE_OBJID_MAX_LEN)
			te_oper_set_param_int_rw(te_op, 2, len);
		else
			te_oper_set_param_int_rw(te_op, 2, 0);

	} else if (command == 302) { /* file read */

		result = te_oper_get_param_mem(te_op, 0, (void *)&filename,
				&len);
		if (result != OTE_SUCCESS)
			goto fail;

		result = te_oper_get_param_mem(te_op, 1, (void *)&bufp, &len);
		if (result != OTE_SUCCESS)
			goto fail;

		if (!filename || !bufp)
			return OTE_ERROR_BAD_PARAMETERS;

		uint8_t *src = te_mem_alloc(len + AES_BLOCK_SIZE
						+ TE_STORAGE_OBJID_MAX_LEN);
		if (!src)
			return OTE_ERROR_OUT_OF_MEMORY;

		args.filename = filename;
		args.buffer = src;
		args.buf_len = len + AES_BLOCK_SIZE + TE_STORAGE_OBJID_MAX_LEN;
		args.result = 0;
		error = ioctl(1, OTE_IOCTL_FILE_READ, &args);
		if (error < 0) {
			te_mem_free(src);
			return OTE_ERROR_COMMUNICATION;
		}

		result = validate_then_decyrpt((uint8_t *)&identity.uuid, src,
						len + TE_STORAGE_OBJID_MAX_LEN, src);
		if (result != OTE_SUCCESS) {
			te_mem_free(src);
			te_oper_set_param_int_rw(te_op, 2, 0);
			goto fail;
		}

		/* Comparing filename with the filename that was encrypted */
		if (strncmp((void *)src, (void *)filename,
					TE_STORAGE_OBJID_MAX_LEN) != 0) {
			te_mem_free(src);
			te_oper_set_param_int_rw(te_op, 2, 0);
			return OTE_ERROR_ACCESS_DENIED;
		}

		/* Sending only data to the user */
		te_mem_move((void *)bufp, (void *)(src + TE_STORAGE_OBJID_MAX_LEN),
				len);
		te_mem_free(src);

		/* bytes read */
		if (result != OTE_SUCCESS)
			te_oper_set_param_int_rw(te_op, 2, 0);
		else
			te_oper_set_param_int_rw(te_op, 2, len);

	} else if (command == 303) { /* get file size */

		result = te_oper_get_param_mem(te_op, 0, (void *)&filename,
				&len);
		if (result != OTE_SUCCESS)
			goto fail;

		if (!filename)
			return OTE_ERROR_BAD_PARAMETERS;

		/* use TE_IOCTL_FILE_READ with NULL param to get file size */
		args.filename = filename;
		args.buffer = NULL;
		args.buf_len = 0;
		args.result = 0;
		error = ioctl(1, OTE_IOCTL_FILE_READ, &args);
		if (error < 0)
			return OTE_ERROR_COMMUNICATION;

		/* file size */
		te_oper_set_param_int_rw(te_op, 1,
			args.result - AES_BLOCK_SIZE - TE_STORAGE_OBJID_MAX_LEN);

	} else if (command == 304) { /* file delete */

		result = te_oper_get_param_mem(te_op, 0, (void *)&filename,
				&len);
		if (result != OTE_SUCCESS)
			goto fail;

		if (!filename)
			return OTE_ERROR_BAD_PARAMETERS;

		args.filename = filename;
		args.buffer = NULL;
		args.buf_len = 0;
		(void)ioctl(1, OTE_IOCTL_FILE_DELETE, &args);

	}

fail:
	return result;
}

te_error_t te_open_session_iface(void **sessionContext, te_operation_t *te_op)
{
	te_fprintf(TE_INFO, "storage_service: %s\n", __func__);
	return OTE_SUCCESS;
}

void te_close_session_iface(void *sessionContext)
{
	te_fprintf(TE_INFO, "storage_service: %s\n", __func__);
	return;
}
