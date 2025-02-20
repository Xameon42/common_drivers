// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/key_manage.h>

extern struct meson_rsv_handler_t *rsv_handler;
static struct meson_rsv_info_t *meson_rsv_key;
struct unifykey_type uk_nand = {
	.storage_type = UNIFYKEY_STORAGE_TYPE_NAND,
	.unifykey_mutex = __MUTEX_INITIALIZER(uk_nand.unifykey_mutex),
	.unifykey_notifier_list = RAW_NOTIFIER_INIT(uk_nand.unifykey_notifier_list),
	.notifier_flag  = false,
};
EXPORT_SYMBOL_GPL(uk_nand);

/*
 * This function reads the u-boot keys.
 */
s32 amlnf_key_read(u8 *buf, u32 len, u32 *actual_length)
{
	struct meson_rsv_info_t *aml_key = meson_rsv_key;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	u8 *key_ptr = NULL;
	u32 keysize = 0;

	if (!meson_rsv_key) {
		pr_info("%s(): amlnf key not ready yet!",
			__func__);
		return -EFAULT;
	}

	keysize = aml_key->size;
	*actual_length = keysize;

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("%s read key len exceed,len 0x%x,keysize 0x%x\n",
			__func__, len, keysize);
		memset(buf + keysize, 0, len - keysize);
	}

	key_ptr = kzalloc(aml_key->size, GFP_KERNEL);
	if (!key_ptr)
		return -ENOMEM;

	rsv_ops->_get_device(rsv_handler->mtd);
	meson_rsv_key_read(key_ptr, keysize);
	rsv_ops->_release_device(rsv_handler->mtd);
	memcpy(buf, key_ptr, min_t(int, keysize, len));

	kfree(key_ptr);

	return 0;
}
EXPORT_SYMBOL(amlnf_key_read);

/*
 * This function write the keys.
 */
s32 amlnf_key_write(u8 *buf, u32 len, u32 *actual_length)
{
	struct meson_rsv_info_t *aml_key = meson_rsv_key;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	/*struct mtd_info *mtd = aml_chip->mtd;*/
	u8 *key_ptr = NULL;
	u32 keysize = 0;
	int error = 0;

	if (!meson_rsv_key) {
		pr_info("%s(): amlnf key not ready yet!",
			__func__);
		return -EFAULT;
	}

	keysize = aml_key->size;
	*actual_length = keysize;

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("%s write key len exceed,len 0x%x,keysize 0x%x\n",
			__func__, len, keysize);
		memset(buf + keysize, 0, len - keysize);
	}

	key_ptr = kzalloc(aml_key->size, GFP_KERNEL);
	if (!key_ptr)
		return -ENOMEM;

	memcpy(key_ptr, buf, keysize);
	rsv_ops->_get_device(rsv_handler->mtd);
	error = meson_rsv_key_write(key_ptr, len);
	rsv_ops->_release_device(rsv_handler->mtd);

	kfree(key_ptr);

	return error;
}
EXPORT_SYMBOL(amlnf_key_write);

int amlnf_key_erase(void)
{
	int ret = 0;

	if (!meson_rsv_key) {
		pr_info("%s amlnf not ready yet!\n", __func__);
		return -1;
	}

	return ret;
}

struct unifykey_storage_ops nand_ops = {
	.read = amlnf_key_read,
	.write = amlnf_key_write
};

int meson_rsv_register_unifykey(struct meson_rsv_info_t *key)
{
	int ret = 0;

	/* avoid null */
	meson_rsv_key = key;

	mutex_lock(&uk_nand.unifykey_mutex);
	uk_nand.ops = &nand_ops;
	/*
	 *pr_info("%s key func read: 0x%px, write: 0x%px\n",
	 *__func__, uk_type->ops->read, uk_type->ops->write);
	 */
	if (uk_nand.unifykey_notifier_list.head)
		raw_notifier_call_chain(&uk_nand.unifykey_notifier_list, 0, &uk_nand);
	uk_nand.notifier_flag = true;
	mutex_unlock(&uk_nand.unifykey_mutex);

	return ret;
}

