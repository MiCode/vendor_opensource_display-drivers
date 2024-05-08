#ifndef _MI_EXT_PANEL_H_
#define _MI_EXT_PANEL_H_
#include "ivi.h"
#include "hud.h"
#include "cluster.h"
#include "mi_disp_print.h"
#include <linux/of_device.h>


#define MAX_MI_EXT_PANEL_COUNT_PER_DSI                2

#define display_for_each_mi_ext_panel(index, count) \
	for (index = 0; (index < count) &&\
			(index < MAX_MI_EXT_PANEL_COUNT_PER_DSI); index++)

typedef struct panel_device panel_device;

typedef enum {
	PANEL_TYPE_IVI = 1,
	PANEL_TYPE_IVI_POGO = 2,
	PANEL_TYPE_HUD = 4,
	PANEL_TYPE_CLUSTER = 8,
} panel_type_id;

typedef struct panel_function {
	int32_t (*panel_init)(panel_device *device);
	void (*panel_deinit)(panel_device *device);
	int32_t (*set_brightness)(panel_device *device, int32_t brightness);
	//int32_t (*set_backlight_mode)(panel_device *device, panel_backlight_mode mode);
	int32_t (*set_power_mode)(panel_device *device, int mode);
	//int32_t (*panel_self_devctl)(panel_device *device, int32_t cmd, void *data, int32_t *nbytes);
	int32_t (*wait_panel_pthread_exit)(panel_device *device);
	int32_t (*show_panel_info)(panel_device *device);
	void (*dump_panel_status)(panel_device *device);
} panel_function;

struct panel_spec_info {
	panel_type_id type;
	struct regmap_config i2c_cfg;
};

struct panel_device {
	char device_name[PANEL_NAME_SIZE];
	int device_index;
	panel_function function;
	struct i2c_client *client;
	struct i2c_interface i2c_instance;
	const struct panel_spec_info *spec_info;
	struct class *panel_class;
	struct device *dev;
	void* private_dev;
};

struct panel_device* mi_ext_panel_get(struct device_node *of_node);
int mi_ext_panel_set_power_mode(struct panel_device *panel_dev, int mode);
void mi_ext_panel_data_register(void);
void mi_ext_panel_data_unregister(void);
#endif
