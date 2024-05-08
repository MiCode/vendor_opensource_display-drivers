
/*
 * mi cluster module driver
 */

#include <linux/delay.h>
#include <linux/jiffies.h>
#include "cluster.h"
#include "mi_disp_print.h"
#include "mi_ext_panel.h"
#include "ivi_backlight.h"

static void cluster_detect_work(struct work_struct *work)
{
	int val = 0;
	struct cluster_panel_device *cluster_data =
		container_of(work, struct cluster_panel_device, check_work.work);

	struct i2c_interface* i2c_inst = &cluster_data->base->i2c_instance;
	struct regmap* regmap = i2c_inst->regmap;

	regmap_read(regmap, 0x04, &val);
	if (val != 0xa83)
		regmap_write(regmap, 0x04, 0xa83);

	//regmap_write(regmap, 0x00, 1000);

	schedule_delayed_work(&cluster_data->check_work, msecs_to_jiffies(2000));
}

static int cluster_set_brightness(struct notifier_block *nb,
	unsigned long event, void *data){
	struct cluster_panel_device *cluster_data = container_of(nb,
		struct cluster_panel_device,cluseter_brightness_notifier);
	struct i2c_interface* i2c_inst = &cluster_data->base->i2c_instance;
	struct regmap* regmap = i2c_inst->regmap;

	regmap_write(regmap, 0x00, (uint16_t)event);

	return 0;
}

void* cluster_panel_device_init(struct panel_device *panel_base) {
	int ret = 0;
	struct cluster_panel_device *data = NULL;

	DISP_INFO("cluster panel device init start");
	data = kzalloc(sizeof(struct cluster_panel_device), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	data->base = panel_base;
	INIT_DELAYED_WORK(&data->check_work, cluster_detect_work);
	schedule_delayed_work(&data->check_work, msecs_to_jiffies(5000));
	data->cluseter_brightness_notifier.notifier_call = cluster_set_brightness;
	register_brightness_notifier(&data->cluseter_brightness_notifier);

	ret = panel_create_sysfs_device((void*)panel_base, data, NULL);
	DISP_INFO("cluster init success !!!");
	return (void*)data;
err:
	DISP_INFO("cluster init fail !!!");
	return NULL;
}
