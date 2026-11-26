/*
 * KTZ Semiconductor KTZ8866 LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 *
 * Contact: Zhang Teng <zhangteng3@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)	"ktz8866:[%s:%d] " fmt, __func__, __LINE__
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include "mi_backlight_ktz8866.h"
#define u8	unsigned int
static struct list_head ktz8866_dev_list;
static struct mutex ktz8866_dev_list_mutex;
static const struct regmap_config ktz8866_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
static int ktz8866_write(struct ktz8866_led *ktz, u8 reg, u8 data)
{
	int ret;
	if(NULL != ktz && NULL != ktz->regmap) {
		ret = regmap_write(ktz->regmap, reg, data);
		if(ret < 0)
			pr_err("ktz  write failed to access registers \n");
	} else {
		pr_err("write missing ktz8866 \n ");
		ret = -EINVAL;
	}
	return ret;
}
static int ktz8866_read(struct ktz8866_led *ktz, u8 reg, u8 *data)
{
	int ret;
	if(NULL != ktz && NULL != ktz->regmap) {
		ret = regmap_read(ktz->regmap, reg, data);
		if(ret < 0)
			pr_err("ktz  read failed to access registers \n");
	} else {
		pr_err("write missing ktz8866 \n ");
		ret = -EINVAL;
	}
	return ret;
}

static struct ktz8866_led *ktz8866_find_device_locked(int ktz8866_id)
{
	struct ktz8866_led *ktz;

	list_for_each_entry(ktz, &ktz8866_dev_list, entry) {
		if (ktz->ktz8866_id == ktz8866_id)
			return ktz;
	}
	return NULL;
}

int mi_ktz8866_set_mode(struct ktz8866_config *ktz_config)
{
	struct ktz8866_led *ktz_device;
	int ret = 0;

	if (!ktz_config) {
		pr_err("ktz8866: invalid config\n");
		return -EINVAL;
	}

	mutex_lock(&ktz8866_dev_list_mutex);
	ktz_device = ktz8866_find_device_locked(ktz_config->ktz8866_id);
	if (!ktz_device) {
		mutex_unlock(&ktz8866_dev_list_mutex);
		pr_err("ktz8866: device id %d not found\n", ktz_config->ktz8866_id);
		return -ENODEV;
	}

	switch (ktz_config->mode) {
	case KTZ_WRITE:
		ret = ktz8866_write(ktz_device, ktz_config->reg, ktz_config->value);
		pr_info("ktz_id %d write reg=0x%02x, data=0x%02x, ret=%d\n",
			ktz_device->ktz8866_id, ktz_config->reg, ktz_config->value, ret);
		break;
	case KTZ_HW_EN:
		if (!gpio_is_valid(ktz_device->hw_en_gpio)) {
			pr_err("ktz8866: HW_EN gpio invalid\n");
			ret = -EINVAL;
			break;
		}
		gpio_set_value(ktz_device->hw_en_gpio, !!ktz_config->value);
		pr_info("ktz8866 HW_EN set value %d\n", !!ktz_config->value);
		break;
	default:
		pr_err("ktz8866: unsupported mode %d\n", ktz_config->mode);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&ktz8866_dev_list_mutex);

	return ret;
}

ssize_t mi_ktz8866_reg_dump(int ktz8866_id, char *buf, size_t size)
{
	struct ktz8866_led *ktz_device;
	unsigned int val = 0;
	ssize_t len = 0;
	int i;

	mutex_lock(&ktz8866_dev_list_mutex);
	ktz_device = ktz8866_find_device_locked(ktz8866_id);
	if (!ktz_device) {
		mutex_unlock(&ktz8866_dev_list_mutex);
		return snprintf(buf, size, "ktz8866: device id %d not found\n", ktz8866_id);
	}

	for (i = 0; i < ARRAY_SIZE(ktz8866_all_regs); i++) {
		ktz8866_read(ktz_device, ktz8866_all_regs[i], &val);
		len += snprintf(buf + len, size - len, "reg=0x%02x, data=0x%02x\n",
			ktz8866_all_regs[i], val);
		if (len >= size)
			break;
	}
	mutex_unlock(&ktz8866_dev_list_mutex);

	return len;
}

static int ktz_update_status(struct ktz8866_led *ktz, unsigned int level)
{
	if(!ktz) {
		pr_err("ktz8866 not exit, return !! \n ");
		return -EINVAL;
	}

	if (!ktz->ktz8866_status && level > 0) {
		ktz8866_write(ktz, KTZ8866_DISP_BL_ENABLE, 0x7f);
		ktz->ktz8866_status = 1;
		pr_info("ktz8866 backlight enable,dimming close");
	} else if (level == 0) {
		ktz8866_write(ktz, KTZ8866_DISP_BL_ENABLE, 0x1f);
		ktz->ktz8866_status = 0;
		pr_info( "ktz8866 backlight disable,dimming close");
	}

	ktz8866_write(ktz,KTZ8866_DISP_BB_LSB, level & 0x7);
	ktz8866_write(ktz,KTZ8866_DISP_BB_MSB, level >> 3);

	return 0;
}
static int ktz_get_brightness(struct ktz8866_led *ktz, u8 reg, u8 *data)
{
	return ktz8866_read(ktz, reg, data);
}
static const struct ktz_ops ops = {
	.update_status	= ktz_update_status,
	.get_brightness	= ktz_get_brightness,
};
int ktz8866_backlight_update_status(unsigned int level)
{
	int ret = 0;
	int brightness = 0;
	static struct ktz8866_led *ktz;

	if (level < 0 || level > BL_LEVEL_MAX)
		return 0;

	brightness = mi_bl_level_remap[level];
	pr_info("ktz8866 backlight %d ,exponential brightness %d \n", brightness, level);

	mutex_lock(&ktz8866_dev_list_mutex);
	list_for_each_entry(ktz, &ktz8866_dev_list, entry) {
		ret |= ktz->ops->update_status(ktz, brightness);
	}
	mutex_unlock(&ktz8866_dev_list_mutex);

	return ret;
}
static int ktz8866_probe(struct i2c_client *i2c)
{
	struct ktz8866_led *pdata;
	int ret = 0;
	if (!i2c_check_functionality(i2c->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&i2c->dev, "ktz8866 I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}
	pdata = devm_kzalloc(&i2c->dev,
			     sizeof(struct ktz8866_led), GFP_KERNEL);
	if (!pdata){
		pr_err("failed: out of memory \n");
		return -ENOMEM;
	}
	pdata->regmap = devm_regmap_init_i2c(i2c, &ktz8866_i2c_regmap_config);
	if(!pdata->regmap) {
		pr_err("init regmap failed\n");
		return -EINVAL;
	}
	pdata->hw_en_gpio = of_get_named_gpio(i2c->dev.of_node, "ktz8866,hwen-gpio", 0);
	if (gpio_is_valid(pdata->hw_en_gpio)) {
		ret = devm_gpio_request(&i2c->dev, pdata->hw_en_gpio, "ktz8866_hw_en");
		if (ret) {
			pr_err("ktz8866: failed to request HW_EN gpio, ret=%d\n", ret);
			pdata->hw_en_gpio = -EINVAL;
		} else {
			gpio_direction_output(pdata->hw_en_gpio, 1);
		}
	}
	ret = of_property_read_u32(i2c->dev.of_node, "ktz8866,id", &pdata->ktz8866_id);
	if (ret) {
		pr_err("ktz8866: read ktz8866,id failed, ret=%d\n", ret);
		return ret;
	}

	pdata->ops = &ops;
	i2c_set_clientdata(i2c, pdata);
	mutex_lock(&ktz8866_dev_list_mutex);
	list_add(&pdata->entry, &ktz8866_dev_list);
	mutex_unlock(&ktz8866_dev_list_mutex);
	dev_info(&i2c->dev, "probe success end\n");
	return 0;
}

static void ktz8866_remove(struct i2c_client *i2c)
{
	struct ktz8866_led *pdata = i2c_get_clientdata(i2c);

	if (pdata) {
		mutex_lock(&ktz8866_dev_list_mutex);
		list_del(&pdata->entry);
		mutex_unlock(&ktz8866_dev_list_mutex);
	}
}
static struct of_device_id ktz8866_match_table[] = {
	{ .compatible = "ktz,ktz8866",},
	{ },
};
static struct i2c_driver ktz8866_driver = {
	.driver = {
		.name = "ktz8866",
		.of_match_table = ktz8866_match_table,
	},
	.probe = ktz8866_probe,
	.remove = ktz8866_remove,
};
int mi_backlight_ktz8866_init(void)
{
	INIT_LIST_HEAD(&ktz8866_dev_list);
	mutex_init(&ktz8866_dev_list_mutex);
	return i2c_add_driver(&ktz8866_driver);
}