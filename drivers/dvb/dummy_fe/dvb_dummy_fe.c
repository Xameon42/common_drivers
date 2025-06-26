// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "aml_demod_common.h"
#include "dummy_fe_wrapper.h"

#define LOG_ERROR      0
#define LOG_DBG        1

#define fe_err(fmt, ...)  \
	do {\
		if (LOG_ERROR <= (debug_dummy_fe)) \
			pr_err("dummy-fe:" fmt, ##__VA_ARGS__); \
	} while (0)

#define fe_debug(fmt, ...)   \
	do {\
		if (LOG_DBG <= (debug_dummy_fe)) \
			pr_info("dummy-fe:" fmt, ##__VA_ARGS__); \
	} while (0)

#define DUMMY_FE_INVALID_ID -1
#define DUMMY_FE_MAX_DEV_CNT 4
#define DEVICE_NAME "dummy_fe"
#define CLASS_NAME "dummyfe"

struct dummy_fe_ctx_t {
	struct dvb_frontend frontend;
	struct file *filep;
	int dummy_fe_id;
	int tsin;
	enum fe_status status;
	enum dummy_fe_state_t state;
	//struct mutex mutex;
	wait_queue_head_t wq;
};

static int major_number;

static struct dummy_fe_ctx_t *fe_ctx[DUMMY_FE_MAX_DEV_CNT] = {
	NULL, NULL, NULL, NULL
};

struct dvb_frontend *dvb_dummy_fe_attach(const struct demod_config *config);

MODULE_PARM_DESC(debug_dummy_fe, "\n\t\t Enable dummy fe debug information");
static int debug_dummy_fe;
module_param(debug_dummy_fe, int, 0644);

static int _dummy_fe_property_process_set(struct dummy_fe_ctx_t *ctx,
								struct file *file,
								u32 cmd, u32 data)
{
	int ret = 0;

	switch (cmd) {
	case DUMMY_FE_STATE:
		ctx->state = data;
		break;
	case DUMMY_FE_STATUS:
		ctx->status = data;
		break;
	default:
		fe_err("%s invalid setprop, cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}

	return ret;
}

static int _dummy_fe_property_process_get(struct dummy_fe_ctx_t *ctx,
								u32 cmd, u32 *data)
{
	int ret = 0;
	struct dtv_frontend_properties *c = NULL;

	switch (cmd) {
	case DUMMY_FE_STATE:
		*data = ctx->state;
		break;
	case DUMMY_FE_FREQUENCY:
		c = &ctx->frontend.dtv_property_cache;
		*data = c->frequency;
		/* frequency in Hz for terrestrial/cable or in kHz for Satellite  */
		if (c->delivery_system == SYS_DVBS || c->delivery_system == SYS_DVBS2)
			*data = c->frequency * 1000;
		break;
	case DUMMY_FE_MODULATION:
		break;
	case DUMMY_FE_SYMBOL_RATE:
		break;
	case DUMMY_FE_BANDWIDTH_HZ:
		break;
	case DUMMY_FE_DELIVERY_SYSTEM:
		c = &ctx->frontend.dtv_property_cache;
		*data = c->delivery_system;
		break;
	default:
		fe_err("%s invalid getprop, cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}

	return ret;
}

static long dummy_fe_wrapper_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i;
	int ret = 0;
	struct dummy_fe_ctx_t *ctx = NULL;
	struct dummy_fe_property prop;

	ret = copy_from_user(&prop, (void __user *)arg, sizeof(prop));
	if (ret)
		return -EFAULT;

	if (cmd == DUMMY_FE_SET_PROPERTY && prop.cmd == DUMMY_FE_ID) {
		for (i = 0; i < DUMMY_FE_MAX_DEV_CNT; i++) {
			ctx = fe_ctx[i];
			if (ctx)
				fe_debug("%s dummy_fe_id: %d, prop.data: %d\n", __func__,
					ctx->dummy_fe_id, prop.data);
			if (ctx &&
				(ctx->dummy_fe_id == prop.data ||
				ctx->dummy_fe_id == DUMMY_FE_INVALID_ID)) {
				ctx->dummy_fe_id = prop.data;
				ctx->filep = file;
				fe_debug("%s set fe_id: %d, filep: %p\n",
					__func__, ctx->dummy_fe_id, file);
				return 0;
			}
		}
		return -1;
	}

	for (i = 0; i < DUMMY_FE_MAX_DEV_CNT; i++) {
		ctx = fe_ctx[i];
		if (ctx && ctx->filep == file)
			break;
	}
	if (i >= DUMMY_FE_MAX_DEV_CNT) {
		fe_err("%s Failed to find dummy fe context, file:%p\n", __func__, file);
		return -1;
	}

	if (cmd == DUMMY_FE_SET_PROPERTY) {
		return _dummy_fe_property_process_set(ctx, file, prop.cmd, prop.data);
	} else if (cmd == DUMMY_FE_GET_PROPERTY) {
		ret = _dummy_fe_property_process_get(ctx, prop.cmd, &prop.data);
		if (ret < 0)
			return -EFAULT;

		ret = copy_to_user((void __user *)arg,
				&prop,
				sizeof(struct dummy_fe_property));
		if (ret)
			return -EFAULT;
	} else {
		return -EOPNOTSUPP;
	}
	return 0;
}

static int dummy_fe_wrapper_open(struct inode *inodep, struct file *filep)
{
	fe_debug("%s filep:%p\n", __func__, filep);
	return 0;
}

static int dummy_fe_wrapper_release(struct inode *inodep, struct file *filep)
{
	fe_debug("%s filep:%p\n", __func__, filep);
	return 0;
}

/* condition needs support multiple instances*/
static unsigned int dummy_fe_wrapper_poll(struct file *filep, poll_table *wait)
{
	int i;
	struct dummy_fe_ctx_t *ctx = NULL;
	unsigned int mask = 0;

	for (i = 0; i < DUMMY_FE_MAX_DEV_CNT; i++) {
		ctx = fe_ctx[i];
		if (ctx && ctx->filep == filep)
			break;
	}
	if (i >= DUMMY_FE_MAX_DEV_CNT) {
		pr_warn("%s Failed to find dummy fe context, file:%p\n",
			__func__, filep);
		return -1;
	}

	poll_wait(filep, &ctx->wq, wait);
	if (ctx->state == DUMMY_FE_LOCK)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

#ifdef CONFIG_COMPAT
static long _dummy_fe_wrapper_compat_ioctl(struct file *filp,
				  unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = dummy_fe_wrapper_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations fops = {
	.open = dummy_fe_wrapper_open,
	.poll = dummy_fe_wrapper_poll,
	.unlocked_ioctl = dummy_fe_wrapper_ioctl,
	.release = dummy_fe_wrapper_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = _dummy_fe_wrapper_compat_ioctl,
#endif
};

static struct class *dummy_class;
static struct device *dummy_device;
static struct cdev dummy_cdev;
static int __init dummy_fe_wrapper_init(void)
{
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0) {
		fe_err("dummy-fe: Failed to register a major number\n");
		return major_number;
	}
	fe_debug("dummy-fe: Registered correctly with major number %d\n",
		major_number);

	dummy_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(dummy_class)) {
		unregister_chrdev(major_number, DEVICE_NAME);
		fe_err("dummy-fe: Failed to register device class\n");
		return PTR_ERR(dummy_class);
	}

	fe_debug("dummy-fe: Device class registered correctly\n");

	dummy_device = device_create(dummy_class, NULL,
					MKDEV(major_number, 0),
					NULL, DEVICE_NAME);
	if (IS_ERR(dummy_device)) {
		class_destroy(dummy_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		fe_err("dummy-fe: Failed to create the device\n");
		return PTR_ERR(dummy_device);
	}

	fe_debug("dummy-fe: Device created\n");

	cdev_init(&dummy_cdev, &fops);
	if (cdev_add(&dummy_cdev, MKDEV(major_number, 0), 1) < 0) {
		device_destroy(dummy_class, MKDEV(major_number, 0));
		class_destroy(dummy_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		fe_err("dummy-fe: Failed to add cdev\n");
		return -1;
	}

	demod_attach_register_cb(AM_DTV_DEMOD_DUMMY, dvb_dummy_fe_attach);
	fe_debug("dummy-fe: register demod attach cb\n");

	return 0;
}

static void __exit dummy_fe_wrapper_exit(void)
{
	cdev_del(&dummy_cdev);
	device_destroy(dummy_class, MKDEV(major_number, 0));
	class_destroy(dummy_class);
	unregister_chrdev(major_number, DEVICE_NAME);
	fe_debug("Goodbye from the LKM!\n");
}

static int dvb_dummy_fe_read_status(struct dvb_frontend *fe,
				    enum fe_status *status)
{
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;

	if (ctx->state == DUMMY_FE_LOCKED)
		ctx->status = FE_HAS_LOCK;
	else
		ctx->status = FE_TIMEDOUT;

	fe_debug("%s status: %d\n", __func__, ctx->status);
	*status = ctx->status;

	return 0;
}

static int dvb_dummy_fe_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;

	if (ctx->state == DUMMY_FE_LOCKED)
		*ber = 0;
	else
		*ber = 100;

	return 0;
}

static int dvb_dummy_fe_read_signal_strength(struct dvb_frontend *fe,
	u16 *strength)
{
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;

	if (ctx->state == DUMMY_FE_LOCKED)
		*strength = 90;
	else
		*strength = 0;
	return 0;
}

static int dvb_dummy_fe_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;

	if (ctx->state == DUMMY_FE_LOCKED)
		*snr = 40;
	else
		*snr = 0;
	return 0;
}

static int dvb_dummy_fe_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
static int dvb_dummy_fe_get_property(struct dvb_frontend *fe,
							struct dtv_property *tvp)
{
	int i;

	if (tvp->cmd != DTV_TS_INPUT)
		return -ECANCELED;

	for (i = 0; i < DUMMY_FE_MAX_DEV_CNT; i++) {
		if (&fe_ctx[i]->frontend == fe) {
			tvp->u.data = fe_ctx[i]->tsin;
			return 0;
		}
	}

	tvp->u.data = 3;
	fe_err("%s failed\n", __func__);

	return 0;
}
#endif

/*
 * Should only be implemented if it actually reads something from the hardware.
 * Also, it should check for the locks, in order to avoid report wrong data
 * to userspace.
 */
static int dvb_dummy_fe_get_frontend(struct dvb_frontend *fe,
				     struct dtv_frontend_properties *p)
{
	// do nothing
	return 0;
}

static int dvb_dummy_fe_set_frontend(struct dvb_frontend *fe)
{
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	fe_debug("%s freq: %d, delivery_system: %d, fe_id: %d",
		__func__, c->frequency, c->delivery_system, fe->id);

	if (!ctx) {
		fe_err("%s dummy fe context is null\n", __func__);
		return -1;
	}

	ctx->dummy_fe_id = fe->id;
	ctx->state = DUMMY_FE_LOCK;
	wake_up(&ctx->wq);

	return 0;
}

static int dvb_dummy_fe_sleep(struct dvb_frontend *fe)
{
	// do nothing
	return 0;
}

static int dvb_dummy_fe_init(struct dvb_frontend *fe)
{
	struct dvb_device *dvbdev = *(struct dvb_device **)fe->frontend_priv;
	struct dummy_fe_ctx_t *ctx = fe->demodulator_priv;

	fe_debug("%s called, ctx: %p, fe_id: %d, dvbdev id: %d\n", __func__,
		ctx, fe->id, dvbdev->id);
	fe->id = dvbdev->id;
	ctx->state = DUMMY_FE_INIT;
	ctx->dummy_fe_id = fe->id;

	return 0;
}

static int dvb_dummy_fe_set_tone(struct dvb_frontend *fe,
				 enum fe_sec_tone_mode tone)
{
	return 0;
}

static int dvb_dummy_fe_set_voltage(struct dvb_frontend *fe,
				    enum fe_sec_voltage voltage)
{
	return 0;
}

static void dvb_dummy_fe_release(struct dvb_frontend *fe)
{
	// do nothing
}

static int _record_ctx(struct dummy_fe_ctx_t *ctx)
{
	int i;

	for (i = 0; i < DUMMY_FE_MAX_DEV_CNT; i++) {
		if (!fe_ctx[i])
			break;
	}
	if (i >= DUMMY_FE_MAX_DEV_CNT) {
		fe_err("Failed to record dummy fe context\n");
		return -1;
	}

	ctx->filep = NULL;
	ctx->status = FE_NONE;
	ctx->dummy_fe_id = DUMMY_FE_INVALID_ID;
	//mutex_init(&ctx->mutex);
	init_waitqueue_head(&ctx->wq);

	fe_ctx[i] = ctx;

	return i;
}

static const struct dvb_frontend_ops dvb_dummy_fe_ops;
struct dvb_frontend *dvb_dummy_fe_attach(const struct demod_config *config)
{
	struct dummy_fe_ctx_t *ctx = NULL;

	/* allocate memory for the internal state */
	//ctx = kzalloc(sizeof(struct dummy_fe_ctx_t), GFP_KERNEL);
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	/* create dvb_frontend */
	memcpy(&ctx->frontend.ops,
		&dvb_dummy_fe_ops,
		sizeof(struct dvb_frontend_ops));
	ctx->frontend.demodulator_priv = ctx;
	ctx->tsin = config->ts;

	fe_debug("%s called, ctx: %p, tsin: %d\n", __func__, ctx, config->ts);
	/* record dummy fe ctx */
	_record_ctx(ctx);

	return &ctx->frontend;
}

static const struct dvb_frontend_ops dvb_dummy_fe_ops = {
	.delsys = { SYS_DVBT, SYS_DVBC_ANNEX_A, SYS_DVBS },
	.info = {
		.name			= "Dummy FE",
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
				FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
				FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
				FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
				FE_CAN_TRANSMISSION_MODE_AUTO |
				FE_CAN_GUARD_INTERVAL_AUTO |
				FE_CAN_HIERARCHY_AUTO,
		.frequency_min_hz = 40000000,
		.frequency_max_hz = 2147000000,
	},

	.release = dvb_dummy_fe_release,

	.init = dvb_dummy_fe_init,
	.sleep = dvb_dummy_fe_sleep,

	.set_frontend = dvb_dummy_fe_set_frontend,
	.get_frontend = dvb_dummy_fe_get_frontend,

	.read_status = dvb_dummy_fe_read_status,
	.read_ber = dvb_dummy_fe_read_ber,
	.read_signal_strength = dvb_dummy_fe_read_signal_strength,
	.read_snr = dvb_dummy_fe_read_snr,
	.read_ucblocks = dvb_dummy_fe_read_ucblocks,
	.set_voltage = dvb_dummy_fe_set_voltage,
	.set_tone = dvb_dummy_fe_set_tone,

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	.get_property = dvb_dummy_fe_get_property,
#endif
};

module_init(dummy_fe_wrapper_init);
module_exit(dummy_fe_wrapper_exit);

MODULE_DESCRIPTION("AML DVB DUMMY Frontend");
MODULE_AUTHOR("Amlogic");
MODULE_LICENSE("GPL");
