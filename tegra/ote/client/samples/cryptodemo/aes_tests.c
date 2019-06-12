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

#include <common/ote_common.h>
#include <service/ote_crypto.h>

uint8_t IV[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

te_error_t run_aes_cts_test(void)
{
	unsigned int i, size;
	unsigned char enckey[16] = {
		0x63, 0x68, 0x69, 0x63,
		0x6b, 0x65, 0x6e, 0x20,
		0x74, 0x65, 0x72, 0x69,
		0x79, 0x61, 0x6b, 0x69
	}; /// 128 bit encryption key

	unsigned int buffermax = 16 * 2;
	unsigned char buffer[] = {
		0xfc, 0x00, 0x78, 0x3e,
		0x0e, 0xfd, 0xb2, 0xc1,
		0xd4, 0x45, 0xd4, 0xc8,
		0xef, 0xf7, 0xed, 0x22,
		0x97, 0x68, 0x72, 0x68,
		0xd6, 0xec, 0xcc, 0xc0,
		0xc0, 0x7b, 0x25, 0xe2,
		0x5e, 0xcf, 0xe5,
	};
	unsigned char expect_out[] = {
		0x49, 0x20, 0x77, 0x6f,
		0x75, 0x6c, 0x64, 0x20,
		0x6c, 0x69, 0x6b, 0x65,
		0x20, 0x74, 0x68, 0x65,
		0x20, 0x47, 0x65, 0x6e,
		0x65, 0x72, 0x61, 0x6c,
		0x20, 0x47, 0x61, 0x75,
		0x27, 0x73, 0x20,
	};

	// 16 byte initialization vector if needed.
	unsigned char buffer_out[sizeof(expect_out)];
	// currently tested buffer length
	unsigned int bufferlen = sizeof(buffer);
	te_error_t err = OTE_SUCCESS;

	printf("%s enter\n", __func__);
	fflush(stdout);
	te_crypto_object_t object;
	err = te_allocate_object(&object);
	if (err != OTE_SUCCESS) {
		printf("crypto object allocation failed %x\n", err);
		fflush(stdout);
		return OTE_ERROR_GENERIC;
	}

	te_attribute_t sk;
	te_set_mem_attribute(&sk, OTE_ATTR_SECRET_VALUE, &enckey,
			sizeof(enckey));
	err = te_populate_object(object, &sk, 1);
	if (err != OTE_SUCCESS) {
		printf("populate error %x\n", err);
		fflush(stdout);
		return OTE_ERROR_GENERIC;
	}

	printf("%s allocate operation\n", __func__);
	fflush(stdout);

	te_crypto_operation_t enop;

	err = te_allocate_operation(&enop, OTE_ALG_AES_CTS,
			OTE_ALG_MODE_DECRYPT);
	if (err != OTE_SUCCESS) {
		printf("allocate Error %x\n", err);
		fflush(stdout);
		return 1;
	}
	err = te_set_operation_key(enop, object);
	if (err != OTE_SUCCESS) {
		printf("Set Key Error %x\n", err);
		fflush(stdout);
		return 1;
	}

	printf("%s call init\n", __func__);
	fflush(stdout);

	te_cipher_init(enop, IV, sizeof(IV));

	printf("%s call update\n", __func__);
	fflush(stdout);
	err = te_cipher_update(enop, buffer, sizeof(buffer), buffer_out,
			&bufferlen);
	if (err != OTE_SUCCESS) {
		printf("Cipher error %x\n", err);
		fflush(stdout);
		return 1;
	}

	uint32_t tempout;
	err = te_cipher_do_final(enop, buffer + sizeof(buffer), 0,
			buffer_out+bufferlen, &tempout);
	if (err != OTE_SUCCESS) {
		printf("Cipher error %x\n", err);
		fflush(stdout);
		return 1;
	}
	bufferlen += tempout;

	for (i = 0; i < bufferlen; ++i) {
		if (expect_out[i] != buffer_out[i]) {
			printf("AES CTS UNEXPECTED OUTPUT\n");
			fflush(stdout);
			return 1;
		}
	}
	printf("AES CTS CORRECT!\n");
	return OTE_SUCCESS;
}

te_error_t run_aes_cbc_test(void)
{
	unsigned int i, size;
	unsigned char enckey[] = {
		0x63, 0x68, 0x69, 0x63,
		0x6b, 0x65, 0x6e, 0x20,
		0x74, 0x65, 0x72, 0x69,
		0x79, 0x61, 0x6b, 0x69
	}; /// 128 bit encryption key

	unsigned int buffermax = 16 * 2;
	unsigned char buffer[] = {
		0x97, 0x68, 0x72, 0x68,
		0xd6, 0xec, 0xcc, 0xc0,
		0xc0, 0x7b, 0x25, 0xe2,
		0x5e, 0xcf, 0xe5, 0x84,
		0x39, 0x31, 0x25, 0x23,
		0xa7, 0x86, 0x62, 0xd5,
		0xbe, 0x7f, 0xcb, 0xcc,
		0x98, 0xeb, 0xf5, 0xa8,
	};
	unsigned char expect_out[] = {
		0x49, 0x20, 0x77, 0x6f,
		0x75, 0x6c, 0x64, 0x20,
		0x6c, 0x69, 0x6b, 0x65,
		0x20, 0x74, 0x68, 0x65,
		0x20, 0x47, 0x65, 0x6e,
		0x65, 0x72, 0x61, 0x6c,
		0x20, 0x47, 0x61, 0x75,
		0x27, 0x73, 0x20, 0x43
	};
	// 16 byte initialization vector if needed.
	unsigned char buffer_out[sizeof(expect_out)];
	// currently tested buffer length
	unsigned int bufferlen = sizeof(buffer);
	te_error_t err = OTE_SUCCESS;

	printf("%s enter\n", __func__);
	fflush(stdout);
	te_crypto_object_t object;
	err = te_allocate_object(&object);
	if (err != OTE_SUCCESS) {
		printf("object allocation failed %x\n", err);
		fflush(stdout);
		return 1;
	}

	te_attribute_t sk;
	te_set_mem_attribute(&sk, OTE_ATTR_SECRET_VALUE, &enckey,
			sizeof(enckey));
	err = te_populate_object(object, &sk, 1);
	if (err != OTE_SUCCESS) {
		printf("populate error %x\n", err);
		fflush(stdout);
		return 1;
	}

	printf("%s allocate operation\n", __func__);
	fflush(stdout);

	te_crypto_operation_t enop;
	err = te_allocate_operation(&enop, OTE_ALG_AES_CBC_NOPAD,
			OTE_ALG_MODE_DECRYPT);
	if (err != OTE_SUCCESS) {
		printf("operation allocation error%x\n", err);
		fflush(stdout);
		return 1;
	}
	err = te_set_operation_key(enop, object);
	if (err != OTE_SUCCESS) {
		printf("Set Key Error %x\n", err);
		fflush(stdout);
		return 1;
	}

	printf("%s call init\n", __func__);
	fflush(stdout);

	te_cipher_init(enop, IV, sizeof(IV));

	printf("%s call update\n", __func__);
	fflush(stdout);
	err = te_cipher_update(enop, buffer, sizeof(buffer), buffer_out,
			&bufferlen);
	if (err != OTE_SUCCESS) {
		printf("Cipher update error %x\n", err);
		fflush(stdout);
		return 1;
	}

	uint32_t tempout=0;
	err = te_cipher_do_final(enop, buffer+sizeof(buffer), 0,
			buffer_out+bufferlen, &tempout);
	if (err != OTE_SUCCESS) {
		printf("Cipher do_final error %x\n", err);
		fflush(stdout);
		return 1;
	}
	bufferlen += tempout;

	for (i = 0; i < bufferlen; ++i) {
		if (expect_out[i] != buffer_out[i]) {
			printf("AES CBC UNEXPECTED OUTPUT\n");
			fflush(stdout);
			return 1;
		}
	}
	printf("AES CBC CORRECT!\n");
	return OTE_SUCCESS;
}
