// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/amlogic/efuse.h>
#include <linux/kallsyms.h>
#include "efuse.h"
#include <linux/amlogic/secmon.h>

static ssize_t meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned int cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);
	struct arm_smccc_res res;

	if (arg->cmd == EFUSE_HAL_API_READ)
		cmd = efuse_cmd.read_cmd;
	else if (arg->cmd == EFUSE_HAL_API_WRITE)
		cmd = efuse_cmd.write_cmd;
	else
		return -1;

	offset = arg->offset;
	size = arg->size;

	meson_sm_mutex_lock();

	if (arg->cmd == EFUSE_HAL_API_WRITE)
		memcpy((void *)sharemem_input_base,
		       (const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	arm_smccc_smc(cmd, offset, size, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	*retcnt = res.a0;

	if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0))
		memcpy((void *)arg->buffer,
		       (const void *)sharemem_output_base, ret);

	meson_sm_mutex_unlock();

	if (!ret)
		return -1;

	return 0;
}

static ssize_t meson_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	struct cpumask task_cpumask;

	if (!arg)
		return -1;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = meson_efuse_fn_smc(arg);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static unsigned long efuse_data_process(unsigned long type,
					unsigned long buffer,
					unsigned long length,
					unsigned long option)
{
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base,
	       (const void *)buffer, length);

	asm __volatile__("" : : : "memory");

	do {
		arm_smccc_smc((unsigned long)AML_DATA_PROCESS,
			      (unsigned long)type,
			      (unsigned long)get_secmon_phy_input_base(),
			      (unsigned long)length,
			      (unsigned long)option,
			      0, 0, 0, &res);
	} while (0);

	meson_sm_mutex_unlock();

	return res.a0;
}

int efuse_amlogic_cali_item_read(unsigned int item)
{
	struct arm_smccc_res res;

	if (efuse_cali_item_read != 1)
		return -EINVAL;

	/* range check */
	if (item < EFUSE_CALI_SUBITEM_WHOBURN ||
		item > EFUSE_CALI_SUBITEM_MAX)
		return -EINVAL;

	meson_sm_mutex_lock();

	do {
		arm_smccc_smc((unsigned long)EFUSE_READ_CALI_ITEM,
			(unsigned long)item,
			0, 0, 0, 0, 0, 0, &res);
	} while (0);

	meson_sm_mutex_unlock();
	return res.a0;
}
EXPORT_SYMBOL_GPL(efuse_amlogic_cali_item_read);

/*
 *return: 1: wrote, 0: not write, -1: fail or not support
 */
int efuse_amlogic_check_lockable_item(unsigned int item)
{
	struct arm_smccc_res res;

	/* range check */
	if (item < EFUSE_LOCK_SUBITEM_BASE ||
		item > EFUSE_LOCK_SUBITEM_MAX)
		return -EINVAL;

	meson_sm_mutex_lock();

	do {
		arm_smccc_smc((unsigned long)EFUSE_READ_CALI_ITEM,
			(unsigned long)item,
			0, 0, 0, 0, 0, 0, &res);
	} while (0);

	meson_sm_mutex_unlock();
	return res.a0;
}
EXPORT_SYMBOL_GPL(efuse_amlogic_check_lockable_item);

unsigned long efuse_amlogic_set(char *buf, size_t count)
{
	unsigned long ret;
	struct cpumask task_cpumask;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = efuse_data_process(AML_D_P_W_EFUSE_AMLOGIC,
				 (unsigned long)buf, (unsigned long)count, 0);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static u32 meson_efuse_obj_read(u32 obj_id, u8 *buff, u32 *size)
{
	u32 rc = EFUSE_OBJ_ERR_UNKNOWN;
	u32 len = 0;
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, *size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_READ,
			      (unsigned long)obj_id,
			      (unsigned long)*size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	rc = res.a0;
	len = res.a1;

	if (rc == EFUSE_OBJ_SUCCESS) {
		if (*size >= len) {
			memcpy(buff, (const void *)sharemem_output_base, len);
			*size = len;
		} else {
			rc = EFUSE_OBJ_ERR_SIZE;
		}
	}

	meson_sm_mutex_unlock();

	return rc;
}

static u32 meson_efuse_obj_write(u32 obj_id, u8 *buff, u32 size)
{
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_WRITE,
			      (unsigned long)obj_id,
			      (unsigned long)size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	meson_sm_mutex_unlock();

	return res.a0;
}

u32 efuse_obj_write(u32 obj_id, char *name, u8 *buff, u32 size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	if (size > sizeof(efuseinfo.data)) {
		set_cpus_allowed_ptr(current, &task_cpumask);
		return EFUSE_OBJ_ERR_SIZE;
	}
	efuseinfo.size = size;
	memcpy(efuseinfo.data, buff, efuseinfo.size);
	ret = meson_efuse_obj_write(obj_id, (uint8_t *)&efuseinfo, sizeof(efuseinfo));
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

u32 efuse_obj_enc_write(u32 obj_id, char *name, u8 *buff, u32 size)
{
	u32 ret;
	struct efuse_obj_enc_field_t efuseinfo;
	struct cpumask task_cpumask;
	size_t data_len;
	const size_t IV_LEN = 12;
	const size_t TAG_LEN = 16;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	if (size <= IV_LEN + TAG_LEN) {
		set_cpus_allowed_ptr(current, &task_cpumask);
		return EFUSE_OBJ_ERR_SIZE;
	}
	data_len = size - IV_LEN - TAG_LEN;
	if (data_len > sizeof(efuseinfo.data)) {
		set_cpus_allowed_ptr(current, &task_cpumask);
		return EFUSE_OBJ_ERR_SIZE;
	}

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	efuseinfo.size = data_len;
	memcpy(efuseinfo.data, buff, efuseinfo.size);
	memcpy(efuseinfo.iv, buff + data_len, IV_LEN);
	memcpy(efuseinfo.tag, buff + data_len + IV_LEN, TAG_LEN);
	ret = meson_efuse_obj_write(obj_id, (uint8_t *)&efuseinfo, sizeof(efuseinfo));
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

u32 efuse_obj_read(u32 obj_id, char *name, u8 *buff, u32 *size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	*size = sizeof(efuseinfo);
	ret = meson_efuse_obj_read(obj_id, (uint8_t *)&efuseinfo, size);
	memcpy(buff, efuseinfo.data, efuseinfo.size);
	*size = efuseinfo.size;
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_read);

static int char2hex(char *hex, void *bin, size_t hexlen)
{
	int i, c, n1, n2, k;

	k = 0;
	n1 = -1;
	n2 = -1;
	for (i = 0; i < hexlen; i++) {
		n2 = n1;
		c = hex[i];
		if (c >= '0' && c <= '9') {
			n1 = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			n1 = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			n1 = c - 'A' + 10;
		} else if (c == ' ') {
			n1 = -1;
			continue;
		} else {
			return -1;
		}
		if (n1 >= 0 && n2 >= 0) {
			((u8 *)bin)[k] = (n2 << 4) | n1;
			n1 = -1;
			k++;
		}
	}

	return k;
}

static DEFINE_MUTEX(efuse_field_mutex);

u32 efuse_obj_set_data(char *name, char *data)
{
	u32 ret;
	int dlen = strnlen(data, 64);
	u8 databuf[32] = {0};

	dlen = char2hex(data, databuf, dlen);
	if (dlen < 0) {
		pr_err("parse data char2hex error\n");
		return EFUSE_OBJ_ERR_INVALID_DATA;
	}
	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_write(EFUSE_OBJ_EFUSE_DATA, name, databuf, dlen);
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_set_data);

u32 efuse_obj_set_enc_data(char *name, char *data)
{
	u32 ret;
	int dlen = strnlen(data, 128);
	u8 databuf[64] = {0};

	dlen = char2hex(data, databuf, dlen);
	if (dlen < 0) {
		pr_err("parse data char2hex error\n");
		return EFUSE_OBJ_ERR_INVALID_DATA;
	}
	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_enc_write(EFUSE_OBJ_EFUSE_ENC_DATA, name, databuf, dlen);
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_set_enc_data);

u32 efuse_obj_set_license(char *name)
{
	u32 ret;
	u8 databuf = 0x01;

	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_write(EFUSE_OBJ_EFUSE_DATA, name, &databuf, 1);
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_set_license);

u32 efuse_obj_lock(char *name)
{
	u32 ret;
	u8 databuf = 0x01;

	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_write(EFUSE_OBJ_LOCK_STATUS, name, &databuf, 1);
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_lock);

u32 efuse_obj_get_data(char *name)
{
	u32 ret;
	u8 buff[32];
	u32 bufflen = sizeof(buff);

	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_read(EFUSE_OBJ_EFUSE_DATA, name, buff, &bufflen);
	if (ret == EFUSE_OBJ_SUCCESS) {
		memset(&efuse_field, 0, sizeof(efuse_field));
		strncpy(efuse_field.name, name, sizeof(efuse_field.name) - 1);
		memcpy(efuse_field.data, buff, bufflen);
		efuse_field.size = bufflen;
	}
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_get_data);

u32 efuse_obj_get_lock(char *name)
{
	u32 ret;
	u8 buff[32];
	u32 bufflen = sizeof(buff);

	mutex_lock(&efuse_field_mutex);
	ret = efuse_obj_read(EFUSE_OBJ_LOCK_STATUS, name, buff, &bufflen);
	if (ret == EFUSE_OBJ_SUCCESS) {
		memset(&efuse_field, 0, sizeof(efuse_field));
		strncpy(efuse_field.name, name, sizeof(efuse_field.name) - 1);
		memcpy(efuse_field.data, buff, bufflen);
		efuse_field.size = bufflen;
	}
	mutex_unlock(&efuse_field_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(efuse_obj_get_lock);

static ssize_t meson_trustzone_efuse_get_max(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	unsigned int cmd;
	struct arm_smccc_res res;

	if (arg->cmd != EFUSE_HAL_API_USER_MAX)
		return -1;

	meson_sm_mutex_lock();

	cmd = efuse_cmd.get_max_cmd;

	asm __volatile__("" : : : "memory");
	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);
	ret = res.a0;

	meson_sm_mutex_unlock();

	if (!ret)
		return -1;

	return ret;
}

ssize_t efuse_get_max(void)
{
	struct efuse_hal_api_arg arg;
	ssize_t ret;
	struct cpumask task_cpumask;

	arg.cmd = EFUSE_HAL_API_USER_MAX;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = meson_trustzone_efuse_get_max(&arg);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static ssize_t _efuse_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;
	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos += retcnt;
		return retcnt;
	}

	return ret;
}

static ssize_t _efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;

	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = retcnt;
		return retcnt;
	}

	return ret;
}

ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pos = *ppos;

	ret = _efuse_read(pdata, count, (loff_t *)&pos);

	memcpy(buf, pdata, count);
	kfree(pdata);

	return ret;
}

ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	memcpy(pdata, buf, count);
	pos = *ppos;

	ret = _efuse_write(pdata, count, (loff_t *)&pos);
	kfree(pdata);

	return ret;
}

uint32_t efuse_mrk_get_checknum(const char *name, uint32_t longmrk, uint32_t *result)
{
	u32 rc = EFUSE_OBJ_ERR_UNKNOWN;
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	strncpy((void *)sharemem_input_base, name, 16);
	do {
		arm_smccc_smc((unsigned long)EFUSE_MRK_GET_CHECKNUM,
			      (unsigned long)longmrk,
			      0, 0, 0, 0, 0, 0, &res);

	} while (0);

	rc = res.a0;
	if (longmrk == 1)
		memcpy((void *)result, (void *)sharemem_output_base, 16);
	else
		*result = res.a1;

	meson_sm_mutex_unlock();

	return rc;
}
