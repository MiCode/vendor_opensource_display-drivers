/*
 * KTZ Semiconductor SY7758 LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 *
 * Contact: Zhang Teng <zhangteng3@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)	"dsi:[%s:%d] " fmt, __func__, __LINE__

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
#include "mi_backlight_sy7758.h"

#define u8	unsigned int
static struct list_head sy7758_dev_list;
static struct mutex sy7758_dev_list_mutex;
static bool main_bl;
extern char lcd_name[];
static const struct regmap_config sy7758_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int sy7758_write(struct sy7758_led *sy, u8 reg, u8 data)
{
	int ret;

	if(NULL != sy) {
		ret = regmap_write(sy->regmap, reg, data);
		if(ret < 0)
			pr_err("sy  write failed to access registers \n");
	} else {
		pr_err("write missing sy7758 \n ");
		ret = -EINVAL;
	}

	return ret;
}

static int sy7758_read(struct sy7758_led *sy, u8 reg, u8 *data)
{
	int ret;

	if(NULL != sy) {
		ret = regmap_read(sy->regmap, reg, data);
		if(ret < 0)
		    pr_err("sy  read failed to access registers \n");
		else if (*data != 0x30)
		    pr_info("sy  read reg = 0x%x, data = 0x%x", reg, *data);
	} else {
		pr_err("write missing sy7758 \n ");
		ret = -EINVAL;
	}

	return ret;
}

static int sy_update_status(struct sy7758_led *sy, unsigned int level)
{
	int exponential_bl = level;
	int brightness = 0;
	int ret = 0;
	if(!sy) {
		pr_err("sy7758 not exit, return !! \n ");
		ret = -EINVAL;
	}

#ifdef DISPLAY_FACTORY_BUILD
	exponential_bl = exponential_bl * 21 / 30;
#else
    if(NULL!=strstr(lcd_name,"xiaomi n83 35 02 0a cphy nt36532 boe")) {
        if (exponential_bl <= 2536)
            exponential_bl = exponential_bl * 1507 / 1000;
        else if (exponential_bl > 2536)
            exponential_bl = 3822;
    } else if(NULL!=strstr(lcd_name,"xiaomi n83 42 02 0b cphy nt36532 csot")) {
        if (exponential_bl <= 2469)
            exponential_bl = exponential_bl * 141 / 100;
        else if (exponential_bl > 2469)
            exponential_bl = 3483;
    }
#endif

	brightness = exponential_bl;

	if (brightness < 0 || brightness > BL_LEVEL_MAX_HBM || brightness == sy->level)
		return ret;

	pr_info("sy7758 backlight 0x%02x ,exponential brightness %d \n", brightness, level);

	if(32>=brightness) {
		mdelay(4);
		pr_info("[%s] low bl dimming\n",__func__);
	}
	if(15>=brightness && 0<brightness) {
		brightness=15;
		pr_info("[%s] min bl 15\n",__func__);
	}

	if (!sy->sy7758_status && brightness > 0) {
		if (main_bl) {
			sy7758_write(sy, 0x01, 0x83);
			sy7758_write(sy, SY7758_DISP_BL_ENABLE, 0x20);
			sy7758_write(sy, 0xa1, 0xef);
			sy7758_write(sy, 0x16, 0x0f);
			sy7758_write(sy, 0x10, brightness & 0xFF);// lsb
			sy7758_write(sy, 0x11, (brightness >> 8) & 0xF);// msb
			sy7758_write(sy, 0xa4, 0x02);
			sy7758_write(sy, SY7758_DISP_BL_ENABLE, 0x28);
		} else {
			sy7758_write(sy, 0x11, 0x65);
			sy7758_write(sy, 0x1b, 0x01);
			sy7758_write(sy, 0x1A, brightness & 0xF);// lsb
			sy7758_write(sy, 0x19, (brightness >> 4) & 0xFF);// msb
		}
		sy->sy7758_status = 1;
		pr_info("sy7758 backlight enable");
	} else if (brightness == 0) {
		if (main_bl) {
		sy7758_write(sy, 0x10, 0x00);// lsb
		sy7758_write(sy, 0x11, 0x00);// msb
		} else {
			sy7758_write(sy, 0x1A, 0x00);// lsb
			sy7758_write(sy, 0x19, 0x00);// msb
		}
		sy->sy7758_status = 0;
		usleep_range((10 * 1000),(10 * 1000) + 10);
		pr_info( "sy7758 backlight disable");
	}
	if (main_bl) {
		sy7758_write(sy, 0x10, brightness & 0xFF);// lsb
		sy7758_write(sy, 0x11, (brightness >> 8) & 0xF);// msb
	} else {
		sy7758_write(sy, 0x1A, brightness & 0xF);// lsb
		sy7758_write(sy, 0x19, (brightness >> 4) & 0xFF);// msb
	}
	sy->level = brightness;

	return ret;
}

static int sy_get_brightness(struct sy7758_led *sy, u8 reg, u8 *data)
{
	return sy7758_read(sy, reg, data);
}

static const struct sy_ops ops = {
	.update_status	= sy_update_status,
	.get_brightness	= sy_get_brightness,
};

int sy7758_backlight_update_status(unsigned int level)
{
	int ret = 0;
	static struct sy7758_led *sy;
	u8 read;

	mutex_lock(&sy7758_dev_list_mutex);
	list_for_each_entry(sy, &sy7758_dev_list, entry) {
		ret = sy->ops->update_status(sy, level);
		ret = sy->ops->get_brightness(sy, 0x02, &read);
	}
	mutex_unlock(&sy7758_dev_list_mutex);
	return ret;
}

static int sy7758_probe(struct i2c_client *i2c,
			  const struct i2c_device_id *id)
{
	struct sy7758_led *pdata;
	int ret = 0;
	u8 read;

	if (!i2c_check_functionality(i2c->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&i2c->dev, "sy7758 I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}
	pdata = devm_kzalloc(&i2c->dev,
			     sizeof(struct sy7758_led), GFP_KERNEL);
	if (!pdata){
		pr_err("failed: out of memory \n");
		return -ENOMEM;
	}
	pdata->regmap = devm_regmap_init_i2c(i2c, &sy7758_i2c_regmap_config);
	if(!pdata->regmap) {
		pr_err("init regmap failed\n");
		ret = -EINVAL;
	}
	pdata->ops = &ops;
	list_add(&pdata->entry, &sy7758_dev_list);
	sy7758_read(pdata, 0x03, &read);
	if (read == 0x63) {
		main_bl = true;
		i2c->addr = 0x2e;
		pr_info("sy7758 this is sy7758 bl\n");
	} else {
		main_bl = false;
		i2c->addr = 0x36;
		pr_info("sy7758 this is sgm37604a bl\n");
	}

	pdata->sy7758_status = 1;
	dev_info(&i2c->dev, "probe sucess end\n");
	return ret;
}

static int sy7758_remove(struct i2c_client *i2c)
{
	return 0;
}

static struct of_device_id sy7758_match_table[] = {
	{ .compatible = "sy,sy7758",},
	{ },
};

static struct i2c_driver sy7758_driver = {
	.driver = {
		.name = "sy7758",
		.of_match_table = sy7758_match_table,
	},
	.probe = sy7758_probe,
	.remove = sy7758_remove,
};

int mi_backlight_sy7758_init(void)
{
	INIT_LIST_HEAD(&sy7758_dev_list);
	mutex_init(&sy7758_dev_list_mutex);

	return i2c_add_driver(&sy7758_driver);
}