// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/amlogic/pm.h>

struct radar_wakeup {
	struct input_dev *input;
	u32 report_type;
	u32 report_value;
};

static int __maybe_unused radar_wakeup_resume(struct device *dev)
{
	struct radar_wakeup *data = dev_get_drvdata(dev);

	if (get_resume_method() == RADAR_GPIO_WAKEUP) {
		input_event(data->input, data->report_type, data->report_value, 1);
		input_sync(data->input);
		input_event(data->input, data->report_type, data->report_value, 0);
		input_sync(data->input);
		dev_info(dev, "Radar wakeup\n");
	}

	return 0;
}

static int __maybe_unused radar_wakeup_suspend(struct device *dev)
{
	/* Do nothing */
	return 0;
}

static int radar_wakeup_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct radar_wakeup *priv;
	const char *const name = dev_name(dev);
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		dev_err_probe(dev, -ENOMEM, "Failed to allocate memory\n");

	ret = of_property_read_u32(dev->of_node, "report-type", &priv->report_type);
	if (ret)
		return dev_err_probe(dev, -EINVAL, "Property not found: report-type\n");

	ret = of_property_read_u32(dev->of_node, "report-value", &priv->report_value);
	if (ret)
		return dev_err_probe(dev, -EINVAL, "Property not found: report-value\n");

	priv->input = devm_input_allocate_device(dev);
	if (!priv->input)
		dev_err_probe(dev, -ENOMEM, "Failed to allocate input device\n");

	dev_set_drvdata(dev, priv);

	priv->input->name = name;
	priv->input->phys = "radar-wakeup/input0";
	priv->input->id.bustype = BUS_HOST;
	priv->input->id.vendor = 0x0001;
	priv->input->id.product = 0x0001;
	priv->input->id.version = 0x0100;

	__set_bit(priv->report_type, priv->input->evbit);
	__set_bit(priv->report_value, priv->input->keybit);
	__set_bit(EV_REP, priv->input->evbit);

	ret = input_register_device(priv->input);
	if (ret)
		return dev_err_probe(dev, ret, "Unable to register input device\n");

	dev_info(dev, "Radar wakeup driver ready\n");

	return 0;
}

static const struct dev_pm_ops radar_wakeup_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(radar_wakeup_suspend, radar_wakeup_resume)
};

static const struct of_device_id radar_wakeup_of_match[] = {
	{ .compatible = "radar-wakeup", },
	{ /* sentinel */ },
};

static struct platform_driver radar_wakeup_driver = {
	.probe = radar_wakeup_probe,
	.driver = {
		.name = "radar_wakeup",
		.pm = &radar_wakeup_pm_ops,
		.of_match_table = radar_wakeup_of_match,
	},
};

module_platform_driver(radar_wakeup_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Radar wakeup driver");
