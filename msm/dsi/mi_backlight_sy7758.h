/*
 * sy7758.h - sy7758 LEDs Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SY7758_H__
#define __SY7758_H__

/* SY7758 寄存器定义 */
#define SY7758_BRT_MODE_BL_CTL_FAST   0x01
#define SY7758_STATUS                 0x02
#define SY7758_DEVICE_ID              0x03
#define SY7758_DIRECT_CONTROL         0x04
#define SY7758_STATUS2                0x05
#define SY7758_REG_BRIGHTNESS_LSB     0x10
#define SY7758_REG_BRIGHTNESS_MSB     0x11
#define SY7758_LED_ENABLE             0x16
#define SY7758_IBOOST_LIM_2X          0x98
#define SY7758_HEADROOM_OFFSET        0x9E
#define SY7758_CURRENT                0xA0
#define SY7758_CURRENT_MAX            0xA1
#define SY7758_UVLO_BL_ON_FSET_EN     0xA2
#define SY7758_SLOPE_PWM_HYSTERESIS   0xA3
#define SY7758_PWM_TO_I_TH            0xA4
#define SY7758_PS_PWM_FREQ            0xA5
#define SY7758_VBOOST_FREQ            0xA6
#define SY7758_IBOOST_LIM             0xA7
#define SY7758_VBOOST_MAX_JUMP        0xA9
#define SY7758_HEADROOM               0xAA
#define SY7758_STEP_HYST_HCOMP        0xAE
/* SY7758 配置值 */
#define SY7758_BRT_MODE_BL_CTL_FAST_VAL   0x83 /* 1.bit[8]: 0b0 Fast=1 　2.Bit[2:1]: 0b10 配置调光模式为I2C&PWM调光  3.Bit[0]:ob0 关闭背光 */
#define SY7758_LED_ENABLE_VAL             0x1F /* 5 led */
#define SY7758_IBOOST_LIM_2X_VAL          0x81 /* Boost的CS 3.1A */
#define SY7758_CURRENT_VAL                0xFF /* Current_Max: 30mA */
#define SY7758_CURRENT_MAX_VAL            0xEF /* Current_Max: 30mA */
#define SY7758_UVLO_BL_ON_FSET_EN_VAL     0x20 /* 关闭背光 */
#define SY7758_SLOPE_PWM_HYSTERESIS_VAL   0x4E /* Slope time 64-128ms */
#define SY7758_PWM_TO_I_TH_VAL            0x02 /* DC调光 */
#define SY7758_VBOOST_MAX_JUMP_VAL        0x80 /* 配置boost输出电压最大为：100  21V/30V */
#define SY7758_BRT_MODE_BL_ON_VAL         0x82 /* 打开背光 */
/* AW99076 寄存器定义 */
#define AW99076_REG_PWM_CTRL		  0x00
#define AW99076_REG_LED_CURRENT		  0x02
#define AW99076_REG_BIT_CTRL		  0x03
#define AW99076_REG_BRIGHTNESS_MSB	  0x04
#define AW99076_REG_BRIGHTNESS_LSB	  0x05
#define AW99076_REG_MODE_CTRL		  0x07
#define AW99076_REG_LED_ENABLE		  0x0D
#define AW99076_REG_DEVICE_ID		  0x11
#define AW99076_REG_STATUS1		      0x10
#define AW99076_REG_STATUS2		      0x12
#define AW99076_REG_STATUS3		      0x13
/* AW99076 配置值 */
#define AW99076_REG_MODE_CTRL_VAL		0x07  /* I2C&PWM */
#define AW99076_REG_LED_CURRENT_VAL		0x32  /* 30ma */
#define AW99076_REG_PWM_CTRL_VAL		0x65  /* DC调光 */
#define AW99076_REG_BIT_CTRL_VAL		0x04  /* 12bit */
#define AW99076_REG_LED_ENABLE_VAL		0xBE  /* 5 led */
#define TM_BL_LEVEL_MIN 12
#define HX_BL_LEVEL_MIN 16
#define BL_LEVEL_MAX 2047
#define BL_LEVEL_MAX_HBM 4095
#define BL_LEVEL_MAX_RMAP 3071 /* 22.5ma */
#define BL_LEVEL_MAX_HBM_RMAP 3685 /* 27ma */


struct sy7758_led;
struct sy_ops {
	int (*update_status)(struct  sy7758_led *, unsigned int level);
	int (*get_brightness)(struct  sy7758_led *, unsigned int reg, unsigned int *data);
};

/**
 * struct sy7758_led -
 * @lock - Lock for reading/writing the device
 * @level - setting backlight level
 * @level - setting backlight status
**/
struct sy7758_led {
		int level;
		bool sy7758_status;
		bool dimming_status;
		struct regmap *regmap;
		struct list_head entry;
		const struct sy_ops *ops;
};

int mi_backlight_sy7758_init(void);
int sy7758_backlight_update_status(unsigned int backlight);

#endif
