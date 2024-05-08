#ifndef _IVI_H_
#define _IVI_H_
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/backlight.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "i2c_interface.h"
#include "ivi_common.h"
#include "ivi_backlight.h"
#include "ivi_touch.h"


typedef enum {
	STATUS_RESERVED = 0x0,
	STATUS_NORMAL_MODE = 0x01,
	STATUS_UPGRADE_MODE = 0x02,
	STATUS_SELF_TEST_MODE = 0x03,
} ivi_mode_status;

struct ivi_version_info {
	uint8_t hw_ver_major;
	uint8_t hw_ver_minor;
	uint8_t sw_ver_major;
	uint8_t sw_ver_minor;
	uint8_t ts_ver_major;
	uint8_t ts_ver_middle;
	uint8_t ts_ver_minor;
};

struct ivi_panel_device {
	struct ivi_common_device common;
	int irq_gpio;
	struct ivi_touch touch_data;
	struct ivi_backlight backlight_data;
};

struct ivi_pogo_device {
	struct ivi_common_device common;
};

void* ivi_panel_device_init(struct panel_device *panel_base);
void* ivi_pogo_device_init(struct panel_device *panel_base);
#endif
