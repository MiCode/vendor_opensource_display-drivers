/*
 * sy7758.h - sy7758 LEDs Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SY7758_H__
#define __SY7758_H__

/* SY7758 backlight I2C driver */
#define SY7758_backlight_EN_MASK         0x40
#define SY7758_backlight_EN_SHIFT        6
#define SY7758_backlight_DISABLE         0
#define SY7758_backlight_ENABLE          1
#define SY7758_LCD_DRV_HW_EN             6       //GPIO for Active high hardware enable pin
#define SY7758_LCD_DRV_I2C_SCL           5       //Clock of the I 2 C interface.
#define SY7758_LCD_DRV_I2C_SDA           4       //Bi-directional data pin of the I 2 C interface.
#define SY7758_DISP_ID                   0x01
#define SY7758_DISP_BC1                  0x02
#define SY7758_DISP_BC2                  0x03
#define SY7758_DISP_BB_LSB               0x10
#define SY7758_DISP_BB_MSB               0x11
#define SY7758_DISP_BL_ENABLE            0xa2
#define SY7758_DISP_FLAGS                0x0f
#define SY7758_DISP_OPTION1              0x10
#define SY7758_DISP_OPTION2              0x11
#define SY7758_DISP_PTD_LSB              0x12
#define SY7758_DISP_PTD_MSB              0x13
#define SY7758_DISP_DIMMING              0x14
#define SY7758_DISP_FULL_CURRENT         0x15
#define BL_LEVEL_MAX 2047
#define BL_LEVEL_MAX_HBM 4095


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
