/*
* Copyright (c) 2012-2014 NVIDIA CORPORATION.  All rights reserved.
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
#include <service/ote_memory.h>
#include <service/ote_service.h>

extern te_error_t run_aes_cts_test(void);
extern te_error_t run_aes_cbc_test(void);
extern te_error_t run_rsa_oaep_test(void);
extern te_error_t run_rsa_pss_test(void);

/* Simply disable RSA tests since these tests can't be
   passed by default but require special keys optional. */
te_error_t sanity_check()
{
	run_aes_cts_test();
	run_aes_cbc_test();
#if ENABLE_RSA_TESTS
	run_rsa_oaep_test();
	run_rsa_pss_test();
#endif
	return OTE_SUCCESS;
}

te_error_t te_create_instance_iface(void)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);
	return OTE_SUCCESS;
}

te_error_t te_open_session_iface(void **sessionContext,
		te_operation_t *operation)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);
	return OTE_SUCCESS;
}

void te_close_session_iface(void *sessionContext)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);
	return;
}

void te_destroy_instance_iface(void)
{
	; /* do nothing */
}

te_error_t te_receive_operation_iface(void *sessionContext,
		 te_operation_t *operation)
{

	te_error_t err;

	te_fprintf(TE_INFO, "%s: entry\n", __func__);
	te_fprintf(TE_INFO, "command ID (%x)\n", operation->command);

	switch (operation->command) {
		case 1:
			return sanity_check();
		default:
			te_fprintf(TE_ERR, "%s: invalid command ID: 0x%X\n", __func__,
					operation->command);
			return OTE_ERROR_BAD_PARAMETERS;
	}

	return OTE_SUCCESS;
}
