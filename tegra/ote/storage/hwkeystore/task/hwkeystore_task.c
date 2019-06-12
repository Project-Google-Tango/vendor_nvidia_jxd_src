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
#include <common/ote_command.h>

#include <hardware/hardware.h>
#include <hardware/keymaster.h>

#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <common/ote_error.h>
#include <common/ote_common.h>
#include <service/ote_service.h>
#include <service/ote_storage.h>
#include <service/ote_crypto.h>
#include <service/ote_memory.h>

#include <cutils/log.h>

#define HWKEYSTORE_IMPORT_KEYPAIR		11
#define HWKEYSTORE_GET_PUBLIC_KEY_SIZE		12
#define HWKEYSTORE_GET_PUBLIC_KEY		13
#define HWKEYSTORE_SIGN_DATA_SIZE		14
#define HWKEYSTORE_SIGN_DATA			15
#define HWKEYSTORE_VERIFY_DATA			16
#define HWKEYSTORE_DELETE_KEYPAIR		17
#define HWKEYSTORE_GENERATE_KEYPAIR_RSA		18
#define HWKEYSTORE_GENERATE_KEYPAIR_DSA		19
#define HWKEYSTORE_GENERATE_KEYPAIR_EC		20

static te_error_t _Generate_KeyPair_DSA(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t key_params_length, generator_len, prime_p_len, prime_q_len;
	te_error_t err = OTE_SUCCESS;
	const void* key_params = NULL;
	EVP_PKEY *pkey = NULL;
	char *file_name = NULL;
	te_storage_object_t file_handle;
	DSA *dsa = NULL;
	unsigned char *key = NULL;
	uint8_t *generator = NULL;
	uint8_t *prime_p = NULL;
	uint8_t *prime_q = NULL;

	te_oper_get_param_mem(te_op, 0, (void *)&key_params, &key_params_length);
	te_oper_get_param_mem(te_op, 3, (void *)&generator, &generator_len);
	te_oper_get_param_mem(te_op, 4, (void *)&prime_p, &prime_p_len);
	te_oper_get_param_mem(te_op, 5, (void *)&prime_q, &prime_q_len);

	const keymaster_dsa_keygen_params_t* dsa_params =
			(const keymaster_dsa_keygen_params_t*) key_params;

	if (dsa_params->key_size < 512) {
		te_fprintf(TE_ERR, "Requested DSA key size is too small (<512)\n");
		return OTE_ERROR_BAD_PARAMETERS;
	}

	dsa = DSA_new();
	if (dsa == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate DSA\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	if (dsa_params->generator_len == 0 ||
	    dsa_params->prime_p_len == 0 ||
	    dsa_params->prime_q_len == 0 ||
	    dsa_params->generator == NULL||
	    dsa_params->prime_p == NULL ||
	    dsa_params->prime_q == NULL) {
		if (DSA_generate_parameters_ex(dsa, dsa_params->key_size,
			NULL, 0, NULL, NULL, NULL) != 1) {
			te_fprintf(TE_ERR, "Failed DSA_generate_parameters_ex\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}
	} else {
		dsa->g = BN_bin2bn(generator, generator_len, NULL);
		if (dsa->g == NULL) {
			te_fprintf(TE_ERR, "Failed dsa->g BN_bin2bn\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}

		dsa->p = BN_bin2bn(prime_p, prime_p_len, NULL);
		if (dsa->p == NULL) {
			te_fprintf(TE_ERR, "Failed dsa->p BN_bin2bn\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}

		dsa->q = BN_bin2bn(prime_q, prime_q_len, NULL);
		if (dsa->q == NULL) {
			te_fprintf(TE_ERR, "Failed dsa->q BN_bin2bn\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}
	}

	/* RNG needs to be initialized before calling DSA_generate_key */
	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	if (DSA_generate_key(dsa) != 1) {
		te_fprintf(TE_ERR, "Failed to Generate DSA Keypair\n");
		err = OTE_ERROR_SECURITY;
		goto fail;
	}

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	if (EVP_PKEY_assign_DSA(pkey, dsa) == 0) {
		te_fprintf(TE_ERR, "Failed to assign DSA to PKEY\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int privateLen = i2d_PrivateKey(pkey, NULL);
	if (privateLen <= 0) {
		te_fprintf(TE_ERR, "private key size was too big\n");
		err = OTE_ERROR_EXCESS_DATA;
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(privateLen);
	if (key == NULL) {
		te_fprintf(TE_ERR, "could not allocate memory for private data\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	unsigned char *p = key;
	if (i2d_PrivateKey(pkey, &p) != privateLen) {
		te_fprintf(TE_ERR, "Failed i2d_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	uint32_t fname = 0;
	te_generate_random(&fname, sizeof(fname));

	/* Storing Data to a File in data/tlk */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory for filename\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			fname);
	file_name[bytes + 1] = '\0';

	/* pass handle/type back to the NS world */
	te_oper_set_param_int_rw(te_op, 1, fname);
	te_oper_set_param_int_rw(te_op, 2, EVP_PKEY_type(pkey->type));

	/* file write */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "wr: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* write to the file */
	err = te_write_storage_object(file_handle, key, privateLen);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_write_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	te_mem_free(key);
	te_mem_free(file_name);

	return err;
}

static te_error_t _Generate_KeyPair_EC(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t key_params_length;
	te_error_t err = OTE_SUCCESS;
	const void* key_params = NULL;
	EVP_PKEY *pkey = NULL;
	unsigned char *key = NULL;
	char *file_name = NULL;
	te_storage_object_t file_handle;
	EC_GROUP *group = NULL;
	EC_KEY *eckey = NULL;
	unsigned int degree;

	te_oper_get_param_mem(te_op, 0, (void *)&key_params, &key_params_length);

	const keymaster_ec_keygen_params_t* ec_params =
		(const keymaster_ec_keygen_params_t*) key_params;

	switch (ec_params->field_size) {
	case 192:
		group = EC_GROUP_new_by_curve_name(NID_X9_62_prime192v1);
		break;
	case 224:
		group = EC_GROUP_new_by_curve_name(NID_secp224r1);
		break;
	case 256:
		group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
		break;
	case 384:
		group = EC_GROUP_new_by_curve_name(NID_secp384r1);
		break;
	case 521:
		group = EC_GROUP_new_by_curve_name(NID_secp521r1);
		break;
	default:
		group = NULL;
		break;
	}

	if (group == NULL) {
		te_fprintf(TE_ERR, "GROUP is NULL\n");
		return OTE_ERROR_NO_DATA;
	}

	EC_GROUP_set_point_conversion_form(group, POINT_CONVERSION_UNCOMPRESSED);
	EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);

	/* initialize EC key */
	eckey = EC_KEY_new();
	if (eckey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate EC\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	if (EC_KEY_set_group(eckey, group) != 1) {
		te_fprintf(TE_ERR, "Failed EC_KEY_set_group\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	if (EC_KEY_generate_key(eckey) != 1 || EC_KEY_check_key(eckey) < 0) {
		te_fprintf(TE_ERR, "Failed to Generate EC Keypair\n");
		err = OTE_ERROR_SECURITY;
		goto fail;
	}

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	if (EVP_PKEY_assign_EC_KEY(pkey, eckey) == 0) {
		te_fprintf(TE_ERR, "Failed to assign EC to PKEY\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int privateLen = i2d_PrivateKey(pkey, NULL);
	if (privateLen <= 0) {
		te_fprintf(TE_ERR, "private key size was too big\n");
		err = OTE_ERROR_EXCESS_DATA;
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(privateLen);
	if (key == NULL) {
		te_fprintf(TE_ERR, "could not allocate memory for private data\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	unsigned char *p = key;
	if (i2d_PrivateKey(pkey, &p) != privateLen) {
		te_fprintf(TE_ERR, "Failed i2d_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	uint32_t fname = 0;
	te_generate_random(&fname, sizeof(fname));

	/* Storing Data to a File in data/tlk */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory for filename\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			fname);
	file_name[bytes + 1] = '\0';

	/* pass handle/type back to the NS world */
	te_oper_set_param_int_rw(te_op, 1, fname);
	te_oper_set_param_int_rw(te_op, 2, EVP_PKEY_type(pkey->type));

	/* file write */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "wr: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* write to the file */
	err = te_write_storage_object(file_handle, key,
			privateLen);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_write_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (group)
		EC_GROUP_free(group);
	te_mem_free(key);
	te_mem_free(file_name);

	return err;
}

static te_error_t _Generate_KeyPair_RSA(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t key_params_length;
	te_error_t err = OTE_SUCCESS;
	const void* key_params = NULL;
	EVP_PKEY *pkey = NULL;
	unsigned char *key = NULL;
	char *file_name = NULL;
	te_storage_object_t file_handle;
	BIGNUM *bn = NULL;
	RSA *rsa = NULL;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_mem(te_op, 0, (void *)&key_params, &key_params_length);

	const keymaster_rsa_keygen_params_t* rsa_params =
			(const keymaster_rsa_keygen_params_t*) key_params;

	bn = BN_new();
	if (bn == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate BIGNUM\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	if (BN_set_word(bn, rsa_params->public_exponent) == 0) {
		te_fprintf(TE_ERR, "Failed BN_set_word\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	rsa = RSA_new();
	if (rsa == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate RSA\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* RNG needs to be initialized before calling RSA_generate_key_ex */
	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	if (!RSA_generate_key_ex(rsa, rsa_params->modulus_size, bn, NULL) ||
	    (RSA_check_key(rsa) < 0)) {
		te_fprintf(TE_CRITICAL, "Failed to Generate RSA Keypair\n");
		err = OTE_ERROR_SECURITY;
		goto fail;
	}

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	if (EVP_PKEY_assign_RSA(pkey, rsa) == 0) {
		te_fprintf(TE_ERR, "Failed to assign RSA to PKEY\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int privateLen = i2d_PrivateKey(pkey, NULL);
	if (privateLen <= 0) {
		te_fprintf(TE_ERR, "private key size was too big\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(privateLen);
	if (key == NULL) {
		te_fprintf(TE_ERR, "could not allocate memory for private data\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	unsigned char *p = key;
	if (i2d_PrivateKey(pkey, &p) != privateLen) {
		te_fprintf(TE_ERR, "Failed i2d_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	uint32_t fname = 0;
	te_generate_random(&fname, sizeof(fname));

	/* Storing Data to a File in data/tlk */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory for filename\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			fname);
	file_name[bytes + 1] = '\0';

	/* pass handle/type back to the NS world */
	te_oper_set_param_int_rw(te_op, 1, fname);
	te_oper_set_param_int_rw(te_op, 2, EVP_PKEY_type(pkey->type));

	/* file write */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "wr: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* write to the file */
	err = te_write_storage_object(file_handle, key,
			privateLen);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_write_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (bn)
		BN_free(bn);
	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

static te_error_t _Import_KeyPair(te_session_t *hwSessionContext,
		te_operation_t *te_op)
{
	const size_t key_length;
	uint32_t prikeyHandle;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	const uint8_t *key = NULL;
	PKCS8_PRIV_KEY_INFO *pkcs8 = NULL;
	EVP_PKEY *pkey = NULL;
	char *file_name = NULL;
	unsigned char *hkey = NULL;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_mem(te_op, 0, (void *)&key, (uint32_t *)&key_length);

	pkcs8 = d2i_PKCS8_PRIV_KEY_INFO(NULL, &key, key_length);
	if (pkcs8 == NULL) {
		te_fprintf(TE_ERR, "Failed to create PKCS8\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	pkey = (EVP_PKEY *)EVP_PKCS82PKEY(pkcs8);
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKCS82PKEY\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int privateLen = i2d_PrivateKey(pkey, NULL);
	if (privateLen <= 0) {
		te_fprintf(TE_ERR, "private key size was too big\n");
		err = OTE_ERROR_EXCESS_DATA;
		goto fail;
	}

	hkey = (unsigned char *)te_mem_calloc(privateLen);
	if (hkey == NULL) {
		te_fprintf(TE_ERR, "could not allocate memory for key blob\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	unsigned char *p = hkey;
	if (i2d_PrivateKey(pkey, &p) != privateLen) {
		te_fprintf(TE_ERR, "could not allocate memory for key blob");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	uint32_t fname = 0;
	te_generate_random(&fname, sizeof(fname));

	/* Storing Data to a File in data/tlk */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory for filename\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			fname);
	file_name[bytes + 1] = '\0';

	/* pass handle/type back to the NS world */
	te_oper_set_param_int_rw(te_op, 1, fname);
	te_oper_set_param_int_rw(te_op, 2, EVP_PKEY_type(pkey->type));

	/* file write */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "wr: open_storage_object fail %x\n", err);
		goto fail;
	}

	/* write to the file */
	err = te_write_storage_object(file_handle, (void *)hkey, privateLen);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_write_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (pkcs8)
		PKCS8_PRIV_KEY_INFO_free(pkcs8);

	te_mem_free(hkey);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit (%x) %s\n", err, __func__);
	return err;
}

static te_error_t _Get_Public_Key_Size(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t prikeyHandle, public_length, bytes_read;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	char *file_name = NULL;
	uint8_t* key = NULL;
	EVP_PKEY *pkey = NULL;
	uint32_t generation_type = 0;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);
	te_oper_get_param_int(te_op, 2, &generation_type);

	te_fprintf(TE_ERR, "generation type: %d \n",generation_type);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory for filename\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			(uint32_t)prikeyHandle);
	file_name[bytes + 1] = '\0';

	/* file read */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(bytes_read);
	if (!key) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, (void *)key, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "%s: te_read_storage_object fail %x\n",
			__func__, err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	EVP_PKEY *tmp = pkey;
	const unsigned char *p = key;
	if (d2i_PrivateKey(generation_type, &tmp, &p, bytes_read) == NULL) {
		te_fprintf(TE_ERR, "Failed d2i_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	public_length = (uint32_t)i2d_PUBKEY(pkey, NULL);
	if (public_length <= 0) {
		te_fprintf(TE_ERR, "Failed to get the public Length\n");
		err = OTE_ERROR_NO_DATA;
		goto fail;
	}

	te_oper_set_param_int_rw(te_op, 1, public_length);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

static te_error_t _Get_Public_Key(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint8_t *publicdata;
	uint32_t prikeyHandle, public_length, bytes_read;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	EVP_PKEY *pkey = NULL;
	uint8_t* key = NULL;
	char *file_name = NULL;
	uint32_t generation_type = 0;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);
	te_oper_get_param_mem(te_op, 1, (void *)&publicdata, &public_length);
	te_oper_get_param_int(te_op, 2, &generation_type);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}
	snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x", prikeyHandle);

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			(uint32_t)prikeyHandle);
	file_name[bytes + 1] = '\0';

	/* file read */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(bytes_read);
	if (!key) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, key, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_read_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	EVP_PKEY *tmp = pkey;
	const unsigned char *pvt = key;
	if (d2i_PrivateKey(generation_type, &tmp, &pvt, bytes_read) == NULL) {
		te_fprintf(TE_ERR, "Failed d2i_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	unsigned char *pub = publicdata;
	if (i2d_PUBKEY(pkey, &pub) != (int)public_length) {
		te_fprintf(TE_ERR, "Failed i2d_PUBKEY\n");
		err = OTE_ERROR_NO_DATA;
	}

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

static te_error_t _Sign_Data_DSA(EVP_PKEY* pkey,
			keymaster_dsa_sign_params_t* sign_params,
			const uint8_t* data, const size_t dataLength,
			uint8_t** signedData, size_t* signedDataLength)
{
	te_error_t err = OTE_SUCCESS;

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	}

	DSA *dsa = EVP_PKEY_get1_DSA(pkey);
	if (dsa == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_DSA\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	unsigned char *temp = (unsigned char *)signedData;
	if (DSA_sign(0, data, dataLength, temp, signedDataLength, dsa) <= 0) {
		te_fprintf(TE_ERR, "Failed DSA_sign\n");
		return OTE_ERROR_SECURITY;
	}

	DSA_free(dsa);

	return OTE_SUCCESS;
}

static te_error_t _Sign_Data_EC(EVP_PKEY* pkey,
			keymaster_ec_sign_params_t* sign_params,
			const uint8_t* data, const size_t dataLength,
			uint8_t** signedData, size_t* signedDataLength)
{
	te_error_t err = OTE_SUCCESS;

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	}

	EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (eckey == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_EC_KEY\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	unsigned char *temp = (unsigned char *)signedData;
	if (ECDSA_sign(0, data, dataLength, temp, signedDataLength, eckey) <= 0) {
		te_fprintf(TE_ERR, "Failed ECDSA_sign\n");
		return OTE_ERROR_SECURITY;
	}

	EC_KEY_free(eckey);

	return OTE_SUCCESS;
}

static te_error_t _Sign_Data_RSA(EVP_PKEY* pkey,
			keymaster_rsa_sign_params_t* sign_params,
			const uint8_t* data, const size_t dataLength,
			uint8_t** signedData, size_t* signedDataLength)
{
	te_error_t err = OTE_SUCCESS;

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	} else if (sign_params->padding_type != PADDING_NONE) {
		te_fprintf(TE_ERR, "Cannot handle padding type %d",
			sign_params->padding_type);
		return OTE_ERROR_BAD_PARAMETERS;
	}

	RSA *rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_RSA\n");
		return OTE_ERROR_GENERIC;
	}

	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	unsigned char *temp = (unsigned char *)signedData;
	if (RSA_private_encrypt(dataLength, data, temp, rsa, RSA_NO_PADDING) <= 0) {
		te_fprintf(TE_ERR, "Failed RSA_private_encrypt\n");
		return OTE_ERROR_SECURITY;
	}

	RSA_free(rsa);

	return OTE_SUCCESS;
}

static te_error_t _Sign_Data(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint8_t **sign_data = NULL;
	uint8_t *data = NULL;
	uint32_t sign_data_length, data_length, bytes_read, params_length;
	uint32_t prikeyHandle;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	char *file_name = NULL;
	EVP_PKEY *pkey = NULL;
	unsigned char *key = NULL;
	const void* params = NULL;
	uint32_t generation_type = 0;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);
	te_oper_get_param_mem(te_op, 1, (void *)&data, &data_length);
	te_oper_get_param_mem(te_op, 2, (void **)&sign_data, &sign_data_length);
	te_oper_get_param_mem(te_op, 3, (void *)&params, &params_length);
	te_oper_get_param_int(te_op, 4, &generation_type);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to alloc memory for filename\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			(uint32_t)prikeyHandle);
	file_name[bytes + 1] = '\0';

	/* file read */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(bytes_read);
	if (!key) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, (void *)key, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "%s: te_read_storage_object fail %x\n",
			__func__, err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	EVP_PKEY *tmp = pkey;
	const unsigned char *p = key;
	if (d2i_PrivateKey(generation_type, &tmp, &p, bytes_read) == NULL) {
		te_fprintf(TE_ERR, "Failed d2i_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int type = EVP_PKEY_type(pkey->type);
	if (type == EVP_PKEY_DSA) {
		keymaster_dsa_sign_params_t* sign_params = (keymaster_dsa_sign_params_t*) params;
		err = _Sign_Data_DSA(pkey, sign_params, data, data_length,
				sign_data, &sign_data_length);
	} else if (type == EVP_PKEY_EC) {
		keymaster_ec_sign_params_t* sign_params = (keymaster_ec_sign_params_t*) params;
		err = _Sign_Data_EC(pkey, sign_params, data, data_length,
				sign_data, &sign_data_length);
	} else if (type == EVP_PKEY_RSA) {
		keymaster_rsa_sign_params_t* sign_params = (keymaster_rsa_sign_params_t*) params;
		err = _Sign_Data_RSA(pkey, sign_params, data, data_length,
				sign_data, &sign_data_length);
	}

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit : %s\n", __func__);
	return err;
}

static te_error_t _Sign_Data_Size(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t prikeyHandle, sign_data_length, bytes_read;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	char *file_name = NULL;
	EVP_PKEY *pkey = NULL;
	unsigned char *key = NULL;
	uint32_t generation_type = 0;

	te_fprintf(TE_ERR, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);
	te_oper_get_param_int(te_op, 2, &generation_type);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			(uint32_t)prikeyHandle);
	file_name[bytes + 1] = '\0';

	/* file read */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(bytes_read);
	if (!key) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, key, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_read_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	EVP_PKEY *tmp = pkey;
	const unsigned char *p = key;
	if (d2i_PrivateKey(generation_type, &tmp, &p, bytes_read) == NULL) {
		te_fprintf(TE_ERR, "Failed d2i_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int type = EVP_PKEY_type(pkey->type);
	if (type == EVP_PKEY_DSA) {

		DSA *dsa = EVP_PKEY_get1_DSA(pkey);
		if (dsa == NULL) {
			te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_DSA\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}

		sign_data_length = DSA_size(dsa);

	} else if (type == EVP_PKEY_EC) {

		EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
		if (eckey == NULL) {
			te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_EC_KEY\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}

		sign_data_length = ECDSA_size(eckey);

	} else if (type == EVP_PKEY_RSA) {

		RSA *rsa = EVP_PKEY_get1_RSA(pkey);
		if (rsa == NULL) {
			te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_RSA\n");
			err = OTE_ERROR_GENERIC;
			goto fail;
		}

		sign_data_length = RSA_size(rsa);

	} else {
		te_fprintf(TE_ERR, "Unsupported key type");
		err = OTE_ERROR_NOT_SUPPORTED;
		goto fail;
	}

	te_oper_set_param_int_rw(te_op, 1, sign_data_length);

fail:
	if (pkey)
		EVP_PKEY_free(pkey);
	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

static te_error_t _Verify_Data_DSA(EVP_PKEY* pkey,
			keymaster_dsa_sign_params_t* sign_params,
			const uint8_t* signedData,
			const size_t signedDataLength, const uint8_t* signature,
			const size_t signatureLength)
{
	te_error_t err = OTE_SUCCESS;

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	}

	DSA *dsa = EVP_PKEY_get1_DSA(pkey);
	if (dsa == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_DSA\n");
		return OTE_ERROR_GENERIC;
	}

	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	if (DSA_verify(0, signedData, signedDataLength, signature,
		signatureLength, dsa) <= 0) {
		te_fprintf(TE_ERR, "Failed DSA_verify\n");
		return OTE_ERROR_SECURITY;
	}

	return OTE_SUCCESS;
}

static te_error_t _Verify_Data_EC(EVP_PKEY* pkey,
			keymaster_ec_sign_params_t* sign_params,
			const uint8_t* signedData,
			const size_t signedDataLength, const uint8_t* signature,
			const size_t signatureLength)
{
	te_error_t err = OTE_SUCCESS;
	te_fprintf(TE_CRITICAL, "enter %s\n", __func__);

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	}

	EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (eckey == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_EC_KEY\n");
		return OTE_ERROR_GENERIC;
	}

	if (ECDSA_verify(0, signedData, signedDataLength, signature,
		signatureLength, eckey) <= 0) {
		te_fprintf(TE_ERR, "Failed ECDSA_verify\n");
		return OTE_ERROR_SECURITY;
	}

	return OTE_SUCCESS;
}

static te_error_t _Verify_Data_RSA(EVP_PKEY* pkey,
			keymaster_rsa_sign_params_t* sign_params,
			const uint8_t* signedData,
			const size_t signedDataLength, const uint8_t* signature,
			const size_t signatureLength)
{
	te_error_t err = OTE_SUCCESS;
	uint8_t *checkdata = NULL;

	if (sign_params->digest_type != DIGEST_NONE) {
		te_fprintf(TE_ERR, "Cannot handle digest type %d",
			sign_params->digest_type);
		return OTE_ERROR_BAD_PARAMETERS;
	} else if (sign_params->padding_type != PADDING_NONE) {
		te_fprintf(TE_ERR, "Cannot handle padding type %d",
			sign_params->padding_type);
		return OTE_ERROR_BAD_PARAMETERS;
	} else if (signatureLength != signedDataLength) {
		te_fprintf(TE_ERR, "signed data length != signature length");
		return OTE_ERROR_BAD_PARAMETERS;
	}

	RSA *rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_RSA\n");
		return OTE_ERROR_GENERIC;
	}

	checkdata = (uint8_t *)calloc(1, signedDataLength);
	if (checkdata == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}

	static uint8_t rnd_seed[128];
	te_generate_random(rnd_seed, sizeof(rnd_seed));
	RAND_seed(rnd_seed, sizeof(rnd_seed));

	unsigned char* tmp = checkdata;
	if (!RSA_public_decrypt(signatureLength, signature, tmp, rsa,
		RSA_NO_PADDING)) {
		te_fprintf(TE_ERR, "Failed EVP_PKEY_get1_RSA\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	size_t i;
	int result = 0;
	for (i = 0; i < signedDataLength; i++)
		result |= tmp[i] ^ signedData[i];

	if (result != 0) {
		te_fprintf(TE_ERR, "Data didn't match\n");
		err = OTE_ERROR_SECURITY;
	}

fail:
	if (checkdata)
		free(checkdata);

	return err;
}

static te_error_t _Verify_Data(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint8_t *sign_data;
	uint8_t *signature;
	uint32_t prikeyHandle, sign_data_length, signature_length, bytes_read, sign_params_length;
	te_error_t err = OTE_SUCCESS;
	te_storage_object_t file_handle;
	EVP_PKEY *pkey = NULL;
	unsigned char *key = NULL;
	char *file_name = NULL;
	const void* params = NULL;
	uint32_t generation_type = 0;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);
	te_oper_get_param_mem(te_op, 1, (void *)&sign_data, &sign_data_length);
	te_oper_get_param_mem(te_op, 2, (void *)&signature, &signature_length);
	te_oper_get_param_mem(te_op, 3, (void *)&params, &sign_params_length);
	te_oper_get_param_int(te_op, 4, &generation_type);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}
	snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x", prikeyHandle);

	int bytes = snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x",
			(uint32_t)prikeyHandle);
	file_name[bytes + 1] = '\0';

	/* file read */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_READ,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		goto fail;
	}

	/* get file size */
	err = te_get_storage_object_size(file_handle, &bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_get_storage_object_size fail %x\n", err);
		goto fail;
	}

	key = (unsigned char *)te_mem_calloc(bytes_read);
	if (!key) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		te_close_storage_object(file_handle);
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	/* read from the file */
	err = te_read_storage_object(file_handle, key, bytes_read,
			&bytes_read);
	if (err != OTE_SUCCESS) {
		te_close_storage_object(file_handle);
		te_fprintf(TE_ERR, "te_read_storage_object fail %x\n", err);
		goto fail;
	}

	te_close_storage_object(file_handle);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		te_fprintf(TE_ERR, "Failed to allocate PKEY\n");
		err = OTE_ERROR_OUT_OF_MEMORY;
		goto fail;
	}

	EVP_PKEY *tmp = pkey;
	const unsigned char *p = key;
	if (d2i_PrivateKey(generation_type, &tmp, &p, bytes_read) == NULL) {
		te_fprintf(TE_ERR, "Failed d2i_PrivateKey\n");
		err = OTE_ERROR_GENERIC;
		goto fail;
	}

	int type = EVP_PKEY_type(pkey->type);
	if (type == EVP_PKEY_DSA) {
		keymaster_dsa_sign_params_t* sign_params = (keymaster_dsa_sign_params_t*) params;
		err = _Verify_Data_DSA(pkey, sign_params, sign_data,
				sign_data_length, signature, signature_length);
	} else if (type == EVP_PKEY_RSA) {
		keymaster_rsa_sign_params_t* sign_params = (keymaster_rsa_sign_params_t*) params;
		err = _Verify_Data_RSA(pkey, sign_params, sign_data,
				sign_data_length, signature, signature_length);
	} else if (type == EVP_PKEY_EC) {
		keymaster_ec_sign_params_t* sign_params = (keymaster_ec_sign_params_t*) params;
		err = _Verify_Data_EC(pkey, sign_params, sign_data,
				sign_data_length, signature, signature_length);
	} else {
		err = OTE_ERROR_NOT_SUPPORTED;
		goto fail;
	}

fail:
	if (pkey)
		EVP_PKEY_free(pkey);

	te_mem_free(key);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

static te_error_t _Delete_KeyPair(te_session_t *hwSessionContext,
		te_operation_t * te_op)
{
	uint32_t prikeyHandle;
	te_storage_object_t file_handle;
	te_error_t err = OTE_SUCCESS;
	char *file_name = NULL;

	te_fprintf(TE_INFO, "enter %s\n", __func__);

	te_oper_get_param_int(te_op, 0, &prikeyHandle);

	/* Reading file to get the public key length */
	file_name = (char *)te_mem_calloc(TE_STORAGE_OBJID_MAX_LEN);
	if (!file_name) {
		te_fprintf(TE_ERR, "Failed to allocate memory to key\n");
		return OTE_ERROR_OUT_OF_MEMORY;
	}
	snprintf(file_name, TE_STORAGE_OBJID_MAX_LEN, "%x", prikeyHandle);

	/* open file to delete */
	err = te_open_storage_object_private((void *)file_name,
			strlen(file_name)+1, OTE_STORE_FLAG_ACCESS_WRITE_META,
			&file_handle);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "open_storage_object fail %x\n", err);
		return err;
	}

	/* delete file and close */
	te_delete_storage_object(file_handle);

	te_close_storage_object(file_handle);
	te_mem_free(file_name);

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return err;
}

te_error_t te_receive_operation_iface(void *hSession,
		te_operation_t * operation)
{
	if (!operation)
		return OTE_ERROR_BAD_PARAMETERS;

	te_fprintf(TE_INFO, "enter %s %d\n", __func__, operation->command);

	switch (operation->command) {

	case HWKEYSTORE_IMPORT_KEYPAIR:
		return _Import_KeyPair(hSession, operation);

	case HWKEYSTORE_GET_PUBLIC_KEY_SIZE:
		return _Get_Public_Key_Size(hSession, operation);

	case HWKEYSTORE_GET_PUBLIC_KEY:
		return _Get_Public_Key(hSession, operation);

	case HWKEYSTORE_SIGN_DATA_SIZE:
		return _Sign_Data_Size(hSession, operation);

	case HWKEYSTORE_SIGN_DATA:
		return _Sign_Data(hSession, operation);

	case HWKEYSTORE_VERIFY_DATA:
		return _Verify_Data(hSession, operation);

	case HWKEYSTORE_DELETE_KEYPAIR:
		return _Delete_KeyPair(hSession, operation);

	case HWKEYSTORE_GENERATE_KEYPAIR_RSA:
		return _Generate_KeyPair_RSA(hSession, operation);

	case HWKEYSTORE_GENERATE_KEYPAIR_DSA:
		return _Generate_KeyPair_DSA(hSession, operation);

	case HWKEYSTORE_GENERATE_KEYPAIR_EC:
		return _Generate_KeyPair_EC(hSession, operation);

	default:
		te_fprintf(TE_INFO, "%s: unsupported command (%d)\n", __func__,
			operation->command);
		return OTE_ERROR_NOT_SUPPORTED;
	}

	te_fprintf(TE_INFO, "exit %s\n", __func__);
	return OTE_SUCCESS;
}

te_error_t te_create_instance_iface(void)
{
	return OTE_SUCCESS;
}

void te_destroy_instance_iface(void)
{
	; /* do nothing */
}

te_error_t te_open_session_iface(void **sessionContext,
		te_operation_t *operation)
{
	return OTE_SUCCESS;
}

void te_close_session_iface(void *sessionContext)
{
	return;
}
