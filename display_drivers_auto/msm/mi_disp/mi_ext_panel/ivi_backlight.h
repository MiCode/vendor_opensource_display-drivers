#ifndef _IVI_BACKLIGHT_H_
#define _IVI_BACKLIGHT_H_
#include <linux/backlight.h>

#define PANEL_BACKLIGHT_LEVEL_MAX (1024)
#define PANEL_BACKLIGHT_LEVEL_INIT (300)

typedef enum {
    PANEL_BACKLIGHT_MODE_OFF,
    PANEL_BACKLIGHT_MODE_ON,
} panel_backlight_mode;

struct ivi_backlight;

struct ivi_backlight_ops {
	int (*set_brightness)(struct ivi_backlight* bl_data,
			int32_t brightness);
	int (*get_brightness)(struct ivi_backlight* bl_data);
	int (*set_default_brightness)(struct ivi_backlight* bl_data);
};

struct ivi_backlight {
	struct backlight_device *bl_dev;
	struct mutex backlight_mutex;

	uint8_t reg_address;
	int32_t max_brightness;
	int32_t init_brightness;
	int32_t current_brightness;

	struct ivi_backlight_ops backlight_ops;
};

int register_brightness_notifier(struct notifier_block *nb);
int ivi_backlight_init(struct ivi_backlight *bl_data);
int ivi_backlight_remove(struct ivi_backlight *bl_data);
#endif
