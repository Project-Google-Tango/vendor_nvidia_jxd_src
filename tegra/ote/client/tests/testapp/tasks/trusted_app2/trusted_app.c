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
#include <unistd.h>
#include <string.h>

#include <service/ote_service.h>
#include <service/ote_memory.h>
#include <ext_nv/ote_ext_nv.h>
#include <common/ote_command.h>

#define PRINT_UUID(u)                                           \
	te_fprintf(TE_INFO, "  uuid: 0x%x 0x%x 0x%x 0x%x%x 0x%x%x%x%x%x%x\n",\
		(u)->time_low, (u)->time_mid,                   \
		(u)->time_hi_and_version,                       \
		(u)->clock_seq_and_node[0],                     \
		(u)->clock_seq_and_node[1],                     \
		(u)->clock_seq_and_node[2],                     \
		(u)->clock_seq_and_node[3],                     \
		(u)->clock_seq_and_node[4],                     \
		(u)->clock_seq_and_node[5],                     \
		(u)->clock_seq_and_node[6],                     \
		(u)->clock_seq_and_node[7]);

static uint32_t ta_refcnt;

te_error_t te_init(void)
{
	uint32_t *regs;
	uint64_t paddr;
	te_error_t err;

	/* get virtual address of first mapping specified in manifest */
	err = te_ext_nv_get_map_addr(1, (void **)&regs);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: te_ext_nv_get_map_addr failure: 0x%x\n",
			__func__, err);
		return err;
	}

	te_fprintf(TE_INFO, "%s: id#1:buffer addr: %p\n", __func__, regs);
	te_fprintf(TE_INFO, "%s: id#1:hid: 0x%x\n", __func__, regs[0x201]);

	/* get virtual address of second mapping specified in manifest */
	err = te_ext_nv_get_map_addr(2, (void **)&regs);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: te_ext_nv_get_map_addr failure: 0x%x\n",
			__func__, err);
		return err;
	}
	te_fprintf(TE_INFO, "%s: id#2:buffer addr: %p\n", __func__, regs);
	te_fprintf(TE_INFO, "%s: id#2:hid: 0x%x\n", __func__, regs[0]);

	/* convert virtual address back to physical */
	err = te_ext_nv_virt_to_phys((void *)regs, &paddr);
	if (err != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: te_ext_nv_virt_to_phys failure: 0x%x\n",
			__func__, err);
		return err;
	}

	te_fprintf(TE_INFO, "%s: vtop: vaddr 0x%p paddr 0x%p\n", __func__, regs, paddr);

	return OTE_SUCCESS;
}

te_error_t te_create_instance_iface(void)
{
	te_error_t result = OTE_SUCCESS;

	te_fprintf(TE_INFO, "%s: entry\n", __func__);

	return result;
}

void te_destroy_instance_iface(void)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);

	return;
}

te_error_t te_open_session_iface(void **session_context, te_operation_t *te_op)
{
	uint32_t *session_data;
	te_error_t result = OTE_SUCCESS;
	uint32_t a2, b2;
	te_service_id_t uuid;
	te_identity_t identity;

	/* get and print uuid of caller */
	memset(&identity, 0, sizeof(identity));
	result = te_get_client_ta_identity(&identity);
	if (result == OTE_SUCCESS) {
		te_fprintf(TE_INFO, "TA2 %s: identity of client:\n", __func__);
		PRINT_UUID(&identity.uuid);
	} else if (result == OTE_ERROR_NOT_IMPLEMENTED) {
		te_fprintf(TE_ERR, "%s: client uuid not implemented.\n", __func__);
		return result;
	} else {
		te_fprintf(TE_ERR, "%s: failed to get client's identity. (err=%d)\n",
				__func__, result);
	}

	session_data = (uint32_t *)te_mem_alloc(4);
	if (session_data == NULL) {
		return OTE_ERROR_OUT_OF_MEMORY;
	}
	*session_data = 0xdead0000 + ta_refcnt++;
	*session_context = (void *)session_data;

	te_fprintf(TE_INFO, "ta2: session_data 0x%x\n", *session_data);

	if (te_oper_get_num_params(te_op) == 2) {
		te_oper_get_param_int(te_op, 1, &a2);
		te_oper_get_param_int(te_op, 2, &b2);
		te_fprintf(TE_INFO, "ta2: a2=0x%x b2=0x%x\n", a2, b2);

		te_oper_set_param_int_rw(te_op, 1, 0x12341234);
		te_oper_set_param_int_rw(te_op, 2, 0x87658765);

		te_oper_get_param_int(te_op, 1, &a2);
		te_oper_get_param_int(te_op, 2, &b2);
		te_fprintf(TE_INFO, "ta2: updated: a2=0x%x b2=0x%x\n", a2, b2);
	}

	/* get and print our own uuid */
	memset(&uuid, 0, sizeof(uuid));
	result = te_get_current_ta_uuid(&uuid);
	if (result == OTE_SUCCESS) {
		te_fprintf(TE_INFO, "TA2 %s: our identity:\n", __func__);
		PRINT_UUID(&uuid);
	} else {
		te_fprintf(TE_ERR, "TA2 %s: failed to get our identity. (err=%d)\n",
				__func__, result);
	}


	return result;
}

void te_close_session_iface(void *session_context)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);

	te_mem_free(session_context);

}

te_error_t te_receive_operation_iface(void *session_context,
		te_operation_t * te_op)
{
	uint32_t cmd;
	te_error_t result = OTE_SUCCESS;

	cmd = te_oper_get_command(te_op);

	te_fprintf(TE_INFO, "%s: entry: cmd: 0x%x param_count: %d \n", __func__, cmd,
		te_oper_get_num_params(te_op));


	return result;
}
