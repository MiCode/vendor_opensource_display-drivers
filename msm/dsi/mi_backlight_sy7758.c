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
extern char lcd_name[128];
bool lcd_bkl_pwm_set = true;

static const struct regmap_config sy7758_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

struct backlight_config {
	u32 min_bl;
	u32 normal_max_bl;
	u32 max_bl;
	u32 min_reg_value;
	u32 normal_max_reg_value;
	u32 hbm_max_reg_value;
};

static const struct backlight_config hx_config = {
	.min_bl = HX_BL_LEVEL_MIN,
	.normal_max_bl = BL_LEVEL_MAX,
	.max_bl = BL_LEVEL_MAX_HBM,
	.min_reg_value = HX_BL_LEVEL_MIN,
	.normal_max_reg_value = BL_LEVEL_MAX_RMAP,
	.hbm_max_reg_value = BL_LEVEL_MAX_HBM_RMAP,
};

static const struct backlight_config tm_config = {
	.min_bl = TM_BL_LEVEL_MIN,
	.normal_max_bl = BL_LEVEL_MAX,
	.max_bl = BL_LEVEL_MAX_HBM,
	.min_reg_value = TM_BL_LEVEL_MIN,
	.normal_max_reg_value = BL_LEVEL_MAX_RMAP,
	.hbm_max_reg_value = BL_LEVEL_MAX_HBM_RMAP,
};

/**
 * calculate_backlight_level_ex - 计算背光亮度等级
 */
static u32 calculate_backlight_level_ex(u32 bl_lvl, const struct backlight_config *config)
{
	if (bl_lvl <= config->min_bl) {
		return config->min_reg_value;
	} else if (bl_lvl <= config->normal_max_bl) {
		return config->min_reg_value +
		       (bl_lvl - config->min_bl) *
		       (config->normal_max_reg_value - config->min_reg_value) /
		       (config->normal_max_bl - config->min_bl);
	} else if (bl_lvl <= config->max_bl) {
		return config->normal_max_reg_value +
		       (bl_lvl - config->normal_max_bl) *
		       (config->hbm_max_reg_value - config->normal_max_reg_value) /
		       (config->max_bl - config->normal_max_bl);
	} else {
		return config->hbm_max_reg_value;
	}
}

static u32 calculate_backlight_level(u32 bl_lvl)
{

	if (!strcmp(lcd_name, "p85 36 02 0b wqxga dsc vid panel"))
		return calculate_backlight_level_ex(bl_lvl, &tm_config);
	else
		return calculate_backlight_level_ex(bl_lvl, &hx_config);
	
}

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
	} else {
		pr_err("write missing sy7758 \n ");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * bkl_sy7758_setup - 初始化SY7758背光芯片
 */
static int bkl_sy7758_setup(struct sy7758_led *sy)
{
	int ret;

	if (!sy)
		return -EINVAL;

	ret = sy7758_write(sy, SY7758_BRT_MODE_BL_CTL_FAST, SY7758_BRT_MODE_BL_CTL_FAST_VAL);
	if (ret < 0)
		return ret;

	ret = sy7758_write(sy, SY7758_UVLO_BL_ON_FSET_EN, SY7758_UVLO_BL_ON_FSET_EN_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_CURRENT_MAX, SY7758_CURRENT_MAX_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_CURRENT, SY7758_CURRENT_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_LED_ENABLE, SY7758_LED_ENABLE_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_PWM_TO_I_TH, SY7758_PWM_TO_I_TH_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_IBOOST_LIM_2X, SY7758_IBOOST_LIM_2X_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_VBOOST_MAX_JUMP, SY7758_VBOOST_MAX_JUMP_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_SLOPE_PWM_HYSTERESIS, SY7758_SLOPE_PWM_HYSTERESIS_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, SY7758_BRT_MODE_BL_CTL_FAST, SY7758_BRT_MODE_BL_ON_VAL);
	if (ret < 0)
	    return ret;

	pr_info("backlight SY7758 setup completed\n");
	return 0;
}

/**
 * bkl_aw99076_setup - 初始化AW99076背光芯片
 */
static int bkl_aw99076_setup(struct sy7758_led *sy)
{
	int ret;

	if (!sy)
		return -EINVAL;

	ret = sy7758_write(sy, AW99076_REG_BIT_CTRL, AW99076_REG_BIT_CTRL_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, AW99076_REG_MODE_CTRL, AW99076_REG_MODE_CTRL_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, AW99076_REG_LED_CURRENT, AW99076_REG_LED_CURRENT_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, AW99076_REG_PWM_CTRL, AW99076_REG_PWM_CTRL_VAL);
	if (ret < 0)
	    return ret;

	ret = sy7758_write(sy, AW99076_REG_LED_ENABLE, AW99076_REG_LED_ENABLE_VAL);
	if (ret < 0)
	    return ret;

	pr_info("backlight AW99076 setup completed\n");
	return 0;
}

/**
 * read_ic_status - 读取IC状态寄存器
 */
static int read_ic_status(struct sy7758_led *sy)
{
	u8 status1, status2, status3;
	int ret;

	if (!sy)
		return -EINVAL;

	if (main_bl) {
		/* 读取SY7758状态寄存器 */
		ret = sy7758_read(sy, SY7758_STATUS, &status1);
		if (ret == 0 && status1 != 0x30) {
			pr_info("backlight SY7758 status: 0x%02x\n", status1);
		}
	} else {
		/* 读取AW99076状态寄存器 */
		ret = sy7758_read(sy, AW99076_REG_STATUS1, &status1);
		if (ret == 0) {
			ret = sy7758_read(sy, AW99076_REG_STATUS2, &status2);
			if (ret == 0) {
				ret = sy7758_read(sy, AW99076_REG_STATUS3, &status3);
				if ((ret == 0) && (status1 != 0x00 || status2 != 0x00 || status3 != 0x00)){
					pr_info("backlight AW99076 status: 0x%02x 0x%02x 0x%02x\n",
						status1, status2, status3);
				}
			}
		}
	}

	return ret;
}

static int sy_update_status(struct sy7758_led *sy, unsigned int level)
{
	int brightness = 0;
	int ret = 0;

	if (!sy) {
		pr_err("device not exist\n");
		return -EINVAL;
	}

	/* remap */
	if (level > 0 && level <= BL_LEVEL_MAX_HBM)
		brightness = calculate_backlight_level(level);

	pr_info("backlight 0x%02x, exponential brightness %d\n", brightness, level);

	/* 检查亮度值是否有效且需要更新 */
	if (brightness < 0 || brightness > BL_LEVEL_MAX_HBM)
		return -EINVAL;

	if (brightness == sy->level)
		return 0;

	/* 背光使能 */
	if (!sy->sy7758_status && brightness > 0) {
		if (main_bl) {
			ret = bkl_sy7758_setup(sy);
		} else {
			ret = bkl_aw99076_setup(sy);
		}
		if (ret < 0)
			return ret;
		sy->sy7758_status = 1;
		lcd_bkl_pwm_set = false;
		pr_info("backlight enabled\n");
	}
	/* 背光关闭 */
	else if (brightness == 0) {
		sy->sy7758_status = 0;
		pr_info("backlight disabled\n");
	}

	/* 设置亮度 */
	if (main_bl) {
		ret = sy7758_write(sy, SY7758_REG_BRIGHTNESS_LSB, brightness & 0xFF);
		if (ret < 0)
	        return ret;
		ret = sy7758_write(sy, SY7758_REG_BRIGHTNESS_MSB, (brightness >> 8) & 0xF);
		if (ret < 0)
	        return ret;
	} else {
		ret = sy7758_write(sy, AW99076_REG_BRIGHTNESS_MSB, (brightness >> 8) & 0xF);
		if (ret < 0)
	        return ret;
		ret = sy7758_write(sy, AW99076_REG_BRIGHTNESS_LSB, brightness & 0xFF);
		if (ret < 0)
	        return ret;
	}

	sy->level = brightness;

	/* 读取状态寄存器用于调试 */
	if (sy->sy7758_status)
		read_ic_status(sy);

	return 0;
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

	mutex_lock(&sy7758_dev_list_mutex);
	list_for_each_entry(sy, &sy7758_dev_list, entry) {
		ret = sy->ops->update_status(sy, level);
		if (ret < 0)
			pr_err("failed to update status for device\n");
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
		pr_info("backlight this is sy7758 bl\n");
	} else {
		main_bl = false;
		i2c->addr = 0x76;
		pr_info("backlight this is AW99706 bl\n");
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