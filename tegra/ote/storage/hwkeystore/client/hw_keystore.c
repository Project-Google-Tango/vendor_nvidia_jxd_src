/*
* Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "Tegra_Keystore"

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <hardware/hardware.h>
#include <hardware/keymaster.h>

#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

#include <cutils/log.h>

#include <client/ote_client.h>

#define HWKEYSTORE_IMPORT_KEYPAIR               11
#define HWKEYSTORE_GET_PUBLIC_KEY_LEN           12
#define HWKEYSTORE_GET_PUBLIC_KEY               13
#define HWKEYSTORE_SIGN_KEY_SIZE                14
#define HWKEYSTORE_SIGN_DATA                    15
#define HWKEYSTORE_VERIFY_DATA                  16
#define HWKEYSTORE_DELETE_KEYPAIR               17
#define HWKEYSTORE_GENERATE_KEYPAIR_RSA         18
#define HWKEYSTORE_GENERATE_KEYPAIR_DSA         19
#define HWKEYSTORE_GENERATE_KEYPAIR_EC          20

#define HWKEYSTORE_UUID \
        { 0x23457890, 0x1234, 0x4a6f, \
                { 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xee } }

static int wrap_key(uint32_t privHandle, uint32_t generation_type,
                uint8_t **keyBlob, size_t *keyBlobLength)
{
        uint32_t *temp = (uint32_t *)malloc(sizeof(uint32_t) * 2);
        if (temp == NULL)
                return -1;

        *keyBlob = (uint8_t *)temp;

        /* store private key handle */
        *temp = privHandle;

        /* key type */
        *(++temp) = generation_type;

        *keyBlobLength = sizeof(uint32_t) * 2;

        return 0;
}

static uint32_t unwrap_key(const uint8_t *keyBlob, const size_t keyBlobLength,
                        uint32_t *generation_type)
{
        uint32_t privhandle;

        if (keyBlobLength < sizeof(uint32_t) *2)
                return 0;

        memcpy(&privhandle, keyBlob, sizeof(uint32_t));
        memcpy(generation_type, keyBlob + 4, sizeof(uint32_t));

        return privhandle;
}

static int hwkeystore_generate_keypair(const keymaster_device_t *dev,
        const keymaster_keypair_t key_type, const void *key_params,
        uint8_t **keyBlob, size_t *keyBlobLength)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t privhandle = 0;
        int ret = 0;
        uint32_t generation_type = 0;
        uint8_t *gen = NULL;
        uint8_t *prime_p = NULL;
        uint8_t *prime_q = NULL;

        if (key_type != TYPE_RSA && key_type != TYPE_DSA && key_type != TYPE_EC) {
                ALOGW("Unsupported key type %d", key_type);
                return -1;
        } else if (key_params == NULL) {
                ALOGW("key_params == null");
                return -1;
        } else if (!keyBlob || !keyBlobLength) {
                ALOGW("keyBlob or keyBlobLength == null");
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGE("te_open_session failed with err (%d)\n", err);
                ret = -1;
                goto error;
        }

        te_operation_reset(&operation);
        te_oper_set_param_mem_ro(&operation, 0, (void *)key_params, sizeof(key_params));
        te_oper_set_param_int_rw(&operation, 1, privhandle);
        te_oper_set_param_int_rw(&operation, 2, generation_type);

        if (key_type == TYPE_DSA) {
                const keymaster_dsa_keygen_params_t* dsa_params =
                        (const keymaster_dsa_keygen_params_t*) key_params;

                gen = (uint8_t *)malloc(dsa_params->generator_len);
                if (gen == NULL) {
                        ALOGE("Failed to allocate memory to gen\n");
                        ret = -1;
                        goto error;
                }
                memcpy((void *)gen, (void *)dsa_params->generator,
                        dsa_params->generator_len);
                te_oper_set_param_mem_ro(&operation, 3,
                        (void *)gen, dsa_params->generator_len);

                prime_p = (uint8_t *)malloc(dsa_params->prime_p_len);
                if (prime_p == NULL) {
                        ALOGE("Failed to allocate memory to p\n");
                        ret = -1;
                        goto error;
                }
                memcpy((void *)prime_p, (void *)dsa_params->prime_p,
                        dsa_params->prime_p_len);
                te_oper_set_param_mem_ro(&operation, 4,
                        (void *)prime_p, dsa_params->prime_p_len);

                prime_q = (uint8_t *)malloc(dsa_params->prime_q_len);
                if (prime_q == NULL) {
                        ALOGE("Failed to allocate memory to q\n");
                        ret = -1;
                        goto error;
                }
                memcpy((void *)prime_q, (void *)dsa_params->prime_q,
                        dsa_params->prime_q_len);
                te_oper_set_param_mem_ro(&operation, 5,
                        (void *)prime_q, dsa_params->prime_q_len);

                te_oper_set_command(&operation, HWKEYSTORE_GENERATE_KEYPAIR_DSA);
        } else if (key_type == TYPE_EC)
                te_oper_set_command(&operation, HWKEYSTORE_GENERATE_KEYPAIR_EC);
        else if (key_type == TYPE_RSA)
                te_oper_set_command(&operation, HWKEYSTORE_GENERATE_KEYPAIR_RSA);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: launch_operation fail (err %d)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_oper_get_param_int(&operation, 1, &privhandle);
        te_oper_get_param_int(&operation, 2, &generation_type);

        if (wrap_key(privhandle, generation_type, keyBlob, keyBlobLength)) {
                ALOGE("%s: wrap_key failed\n", __func__);
                ret = -1;
        }

error:
        if(key_type == TYPE_DSA) {
            if (gen)
                free(gen);
            if (prime_p)
                free(prime_p);
            if (prime_q)
                free(prime_q);
        }
        te_close_session(&hwsession);
        te_operation_reset(&operation);

        return ret;
}

static int hwkeystore_import_keypair(const keymaster_device_t *dev,
        const uint8_t *key, const size_t key_length,
        uint8_t **key_blob, size_t *key_blob_length)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t privhandle;
        int ret = 0;
        uint32_t generation_type = 0;

        if (key == NULL) {
                ALOGW("input key == NULL");
                return -1;
        } else if (key_blob == NULL || key_blob_length == NULL) {
                ALOGW("output key blob or length == NULL");
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %x)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_oper_set_param_mem_ro(&operation, 0, (void *)key, key_length);
        te_oper_set_param_int_rw(&operation, 1, privhandle);
        te_oper_set_param_int_rw(&operation, 2, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_IMPORT_KEYPAIR);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: launch_operation fail (err %x)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_oper_get_param_int(&operation, 1, &privhandle);
        te_oper_get_param_int(&operation, 2, &generation_type);

        if (wrap_key(privhandle, generation_type, key_blob, key_blob_length)) {
                ALOGE("%s: wrap_key failed\n", __func__);
                ret = -1;
        }

error:
        te_close_session(&hwsession);
        te_operation_reset(&operation);

        return ret;
}

static int hwkeystore_get_keypair_public(const struct keymaster_device *dev,
        const uint8_t *key_blob, const size_t key_blob_length,
        uint8_t **x509_data, size_t *x509_data_length)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t public_keylen, keyhandle;
        void *data = NULL;
        int ret = 0;
        uint32_t generation_type = 0;

        if (!x509_data || !x509_data_length || !key_blob)
                return -1;

        /* get our opaque handle */
        keyhandle = unwrap_key(key_blob, key_blob_length, &generation_type);
        if (!keyhandle) {
                ALOGW("%s: keyhandle = NULL", __func__);
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %d)\n", __func__, err);
                ret = -1;
                goto error;
        }

        /* get public key len */
        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_param_int_rw(&operation, 1, public_keylen);
        te_oper_set_param_int_rw(&operation, 2, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_GET_PUBLIC_KEY_LEN);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: fail to get pub key len (err %d)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_oper_get_param_int(&operation, 1, &public_keylen);

        /* get the actual public key */
        data = malloc(public_keylen);
        if (!data) {
                ret = -1;
                goto error;
        }

        te_operation_reset(&operation);
        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_param_mem_rw(&operation, 1, (void *)data, public_keylen);
        te_oper_set_param_int_rw(&operation, 2, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_GET_PUBLIC_KEY);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: fail to get pub key (err %d)\n", __func__, err);
                ret = -1;
                free(data);
                goto error;
        }

        *x509_data_length = (size_t)public_keylen;
        *x509_data = data;

error:
        te_close_session(&hwsession);
        te_operation_reset(&operation);

        return ret;
}

static int hwkeystore_sign_data(const keymaster_device_t *dev,
        const void *params, const uint8_t *keyBlob, const size_t keyBlobLength,
        const uint8_t *data, const size_t dataLength,
        uint8_t **signedData, size_t *signedDataLength)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t keyhandle, signdatalen, sign_len;
        void *signdata = NULL;
        int ret = 0;
        uint32_t generation_type = 0;

        if (data == NULL) {
                ALOGW("input data to sign == NULL");
                return -1;
        } else if (signedData == NULL || signedDataLength == NULL) {
                ALOGW("output signature buffer == NULL");
                return -1;
        }

        /* get our opaque handle */
        keyhandle = unwrap_key(keyBlob, keyBlobLength, &generation_type);
        if (!keyhandle) {
                ALOGW("%s: keyhandle = NULL", __func__);
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %d)\n", __func__, err);
                ret = -1;
                goto error;
        }

        /* get sign data length len */
        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_param_int_rw(&operation, 1, sign_len);
        te_oper_set_param_int_rw(&operation, 2, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_SIGN_KEY_SIZE);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: fail to get sign data len (err %d)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_oper_get_param_int(&operation, 1, &sign_len);

        /* sign data */
        signdata = malloc(sign_len);
        if (!signdata) {
                ALOGW("%s: signdata = NULL", __func__);
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %x)\n", __func__, err);
                ret = -1;
                goto error;
        }

        te_operation_reset(&operation);
        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_param_mem_rw(&operation, 1, (void *)data, dataLength);
        te_oper_set_param_mem_rw(&operation, 2, (void *)signdata, sign_len);
        te_oper_set_param_mem_rw(&operation, 3, (void *)params, sizeof(params));
        te_oper_set_param_int_rw(&operation, 4, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_SIGN_DATA);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: launch_operation fail (err %x)\n", __func__, err);
                te_operation_reset(&operation);
                ret = -1;
                goto error;
        }

        *signedDataLength = sign_len;
        *signedData = signdata;

error:
        te_operation_reset(&operation);
        te_close_session(&hwsession);

        return ret;
}

static int hwkeystore_verify_data(const keymaster_device_t *dev,
        const void *params, const uint8_t *keyBlob, const size_t keyBlobLength,
        const uint8_t *signedData, const size_t signedDataLength,
        const uint8_t *signature, const size_t signatureLength)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t keyhandle;
        uint32_t generation_type = 0;

        if (signedData == NULL || signature == NULL) {
                ALOGW("data or signature buffers == NULL");
                return -1;
        }

        /* get our opaque handle */
        keyhandle = unwrap_key(keyBlob, keyBlobLength, &generation_type);
        if (!keyhandle) {
                ALOGW("%s: keyhandle = NULL", __func__);
                return -1;
        }

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %d)\n", __func__, err);
                te_operation_reset(&operation);
                return -1;
        }

        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_param_mem_rw(&operation, 1, (void *)signedData, signedDataLength);
        te_oper_set_param_mem_rw(&operation, 2, (void *)signature, signatureLength);
        te_oper_set_param_mem_rw(&operation, 3, (void *)params, sizeof(params));
        te_oper_set_param_int_rw(&operation, 4, generation_type);
        te_oper_set_command(&operation, HWKEYSTORE_VERIFY_DATA);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: launch_operation fail (err %d)\n", __func__, err);
                te_close_session(&hwsession);
                te_operation_reset(&operation);
                return -1;
        }

        te_operation_reset(&operation);
        te_close_session(&hwsession);

        return 0;
}

static int hwkeystore_delete_keypair(const struct keymaster_device *dev,
        const uint8_t *key_blob, const size_t key_blob_length)
{
        te_error_t err = OTE_SUCCESS;
        te_session_t hwsession;
        te_operation_t operation;
        te_service_id_t uuid = HWKEYSTORE_UUID;
        uint32_t keyhandle;
        int ret = 0;
        uint32_t generation_type = 0;

        if (!key_blob)
                return -1;

        /* get our opaque handle */
        keyhandle = unwrap_key(key_blob, key_blob_length, &generation_type);
        if (!keyhandle)
                return -1;

        te_init_operation(&operation);
        err = te_open_session(&hwsession, &uuid, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: open_session fail (err %d)\n", __func__, err);
                te_operation_reset(&operation);
                return -1;
        }

        /* delete key pair */
        te_oper_set_param_int_ro(&operation, 0, keyhandle);
        te_oper_set_command(&operation, HWKEYSTORE_DELETE_KEYPAIR);

        err = te_launch_operation(&hwsession, &operation);
        if (err != OTE_SUCCESS) {
                ALOGW("%s: launch_operation fail (err %d)\n", __func__, err);
                ret = -1;
        }

        /* remove key from the key_blob */
        memset((void *)key_blob, 0, key_blob_length);

        te_operation_reset(&operation);
        te_close_session(&hwsession);

        return ret;
}

static int hwkeystore_close(hw_device_t *dev)
{
        free(dev);
        dev = NULL;

        return 0;
}

static int hwkeystore_open(const hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
        if (strcmp(name, KEYSTORE_KEYMASTER) != 0)
                return -1;

        struct keymaster_device *dev = malloc(sizeof(struct keymaster_device));
        memset(dev, 0, sizeof(*dev));

        if (dev == NULL)
                return -1;

        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 1;
        dev->common.module = (struct hw_module_t *)module;
        dev->common.close = hwkeystore_close;

        dev->generate_keypair = hwkeystore_generate_keypair;
        dev->import_keypair = hwkeystore_import_keypair;
        dev->get_keypair_public = hwkeystore_get_keypair_public;
        dev->delete_keypair = hwkeystore_delete_keypair;
        dev->sign_data = hwkeystore_sign_data;
        dev->verify_data = hwkeystore_verify_data;

        *device = (struct hw_device_t *)dev;

        return 0;
}

static struct hw_module_methods_t keystore_module_methods = {
        open: hwkeystore_open,
};

struct keystore_module HAL_MODULE_INFO_SYM
__attribute__ ((visibility ("default"))) = {
        common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: KEYMASTER_MODULE_API_VERSION_0_2,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: KEYSTORE_HARDWARE_MODULE_ID,
        name: "Tegra key storage HAL",
        author: "NVIDIA Corporation",
        methods: &keystore_module_methods,
        dso: 0,
        reserved: {},
        },
};
