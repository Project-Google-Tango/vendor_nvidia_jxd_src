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
#include <common/ote_command.h>
#include <ext_nv/ote_ext_nv.h>

/** Service Identifier for test TA */
/* {23f616f9-8572-4a6f-a1f1-03aa9b05f9ff} */
#define SERVICE_TA2_SID \
	{ 0x23f616f9, 0x8572, 0x4a6f, \
		{ 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xff } }

static te_session_t ta2_session_id;

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
	te_operation_t ta2_operation;
	te_service_id_t ta2_sid = SERVICE_TA2_SID;
	uint32_t *session_data;
	te_error_t result = OTE_SUCCESS;
	uint32_t a, b, a2, b2;

	session_data = (uint32_t *)te_mem_alloc(4);
	if (session_data == NULL) {
		return OTE_ERROR_OUT_OF_MEMORY;
	}
	*session_data = 0xbeef0000 + ta_refcnt++;
	*session_context = (void *)session_data;

	te_fprintf(TE_INFO, "%s: session_data 0x%x\n", __func__, *session_data);

	result = te_oper_get_param_int(te_op, 1, &a);
	if (result != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "Failed to receive a in value (result: 0x%x)\n", result);
		return result;
	}

	result = te_oper_get_param_int(te_op, 2, &b);
	if (result != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "Failed to receive b in value (result: 0x%x)\n", result);
		return result;
	}

	te_fprintf(TE_INFO, "ta: params from testapp: a=0x%x b=0x%x\n", a, b);

	te_oper_set_param_int_rw(te_op, 1, a+1);
	te_oper_set_param_int_rw(te_op, 2, b+1);

	te_oper_get_param_int(te_op, 1, &a2);
	te_oper_get_param_int(te_op, 2, &b2);
	if (a + 1 != a2 || b + 1 != b2) {
		te_fprintf(TE_ERR, "Params didn't save\n");
		te_oper_dump_param_list(te_op);
	}

	te_fprintf(TE_INFO, "ta: new params to testapp: a=0x%x b=0x%x\n", a2, b2);

	/* open a session to ta2 */
	te_init_operation(&ta2_operation);
	te_oper_set_param_int_rw(&ta2_operation, 1, 0x12345678);
	te_oper_set_param_int_rw(&ta2_operation, 2, 0x87654321);

	te_oper_get_param_int(&ta2_operation, 1, &a2);
	te_oper_get_param_int(&ta2_operation, 2, &b2);

	te_fprintf(TE_INFO, "ta: params to ta2 a2=0x%x b2=0x%x\n", a2, b2);


	result = te_open_session(&ta2_session_id, &ta2_sid, &ta2_operation);
	if (result != OTE_SUCCESS) {
		te_fprintf(TE_ERR, "%s: failed to open session with ta2: 0x%x\n",
			__func__, result);
	}

	te_oper_get_param_int(&ta2_operation, 1, &a2);
	te_oper_get_param_int(&ta2_operation, 2, &b2);

	te_fprintf(TE_INFO, "ta: params from ta2 a2=0x%x b2=0x%x\n", a2, b2);


	return result;
}

void te_close_session_iface(void *session_context)
{
	te_fprintf(TE_INFO, "%s: entry\n", __func__);

	te_mem_free(session_context);
	te_close_session(&ta2_session_id);
}

te_error_t te_receive_operation_iface(void *session_context,
		te_operation_t *te_op)
{
	uint32_t *base, size;
	uint32_t val = 0;
	uint32_t value_a, value_b;
	uint32_t cmd, cnt;
	te_oper_param_type_t type;
	te_error_t result = OTE_SUCCESS;

	te_fprintf(TE_INFO, "%s: session_id 0x%x\n",
		__func__, *((uint32_t *)session_context));

	cmd = te_oper_get_command(te_op);
	cnt = te_oper_get_num_params(te_op);

	te_fprintf(TE_INFO, "%s: entry: cmd: 0x%x param_count: %d \n", __func__, cmd, cnt);

	switch (cmd) {
	case 101:
		break;
	case 102:
		if (cnt != 1) {
			te_fprintf(TE_ERR, "%s: bad param count\n", __func__);
			return OTE_ERROR_BAD_PARAMETERS;
		}

		result = te_oper_get_param_type(te_op, 0, &type);
		if (result != OTE_SUCCESS) {
			te_fprintf(TE_ERR, "%s: missing membuf parameter\n", __func__);
			return result;
		}

		if (type != TE_PARAM_TYPE_MEM_RW) {
			te_fprintf(TE_ERR, "%s: bad parameter type: 0x%x\n",
				__func__, type);
			return OTE_ERROR_BAD_PARAMETERS;
		}

		result = te_oper_get_param_mem(te_op, 0, (void *)&base, &size);
		if (result != OTE_SUCCESS) {
			te_fprintf(TE_ERR, "%s: bad membuf param: 0x%x\n",
				__func__, result);
			return result;
		}

		strcpy((char *)base, "writing_into_buf_from_LK_for_output");
		break;
	case 103:
		if (cnt != 2) {
			te_fprintf(TE_ERR, "%s: bad parameter count: 0x%x\n",
				__func__, type);
			return OTE_ERROR_BAD_PARAMETERS;
		}

		/* swap params */
		te_oper_get_param_int(te_op, 1, &value_a);
		te_oper_get_param_int(te_op, 2, &value_b);
		te_oper_set_param_int_rw(te_op, 1, value_b);
		te_oper_set_param_int_rw(te_op, 2, value_a);
		break;
	default:
		te_fprintf(TE_ERR, "%s: unknown command: 0x%x\n", __func__, cmd);
		result = OTE_ERROR_BAD_PARAMETERS;
		break;
	}

	return result;
}
