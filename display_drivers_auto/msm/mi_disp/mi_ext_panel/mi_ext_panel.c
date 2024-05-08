#include "mi_ext_panel.h"
#include "mi_disp_print.h"

#define CLASS_NAME "mi_ext_panel"
struct class *panel_class;

struct mi_ext_panel_list_item {
	struct panel_device *panel_dev;
	struct list_head list;
};

static LIST_HEAD(mi_ext_panel_list);
static DEFINE_MUTEX(mi_ext_panel_list_lock);

static struct panel_spec_info ivi_spec_info = {
	.type = PANEL_TYPE_IVI,
	.i2c_cfg = {.reg_bits = 8, .val_bits = 8},
};

static struct panel_spec_info ivi_pogo_spec_info = {
	.type = PANEL_TYPE_IVI_POGO,
	.i2c_cfg = {.reg_bits = 8, .val_bits = 8},
};

static struct panel_spec_info hud_spec_info = {
	.type = PANEL_TYPE_HUD,
	.i2c_cfg = {.reg_bits = 8, .val_bits = 8},
};

static struct panel_spec_info cluster_spec_info = {
	.type = PANEL_TYPE_CLUSTER,
	.i2c_cfg = {.reg_bits = 8, .val_bits = 16},
};

static const struct of_device_id mi_ext_panel_match_table[] = {
	{
		.compatible = "mi,ivi_panel",
		.data = &ivi_spec_info,
	},
	{
		.compatible = "mi,ivi_panel_pogo",
		.data = &ivi_pogo_spec_info,
	},
	{
		.compatible = "mi,hud_panel",
		.data = &hud_spec_info,
	},
	{
		.compatible = "mi,cluster_panel",
		.data = &cluster_spec_info,
	},
	{}
};

struct panel_device* mi_ext_panel_get(struct device_node *of_node)
{
	struct list_head *pos, *tmp;
	struct panel_device *panel_dev = NULL;

	mutex_lock(&mi_ext_panel_list_lock);
	list_for_each_safe(pos, tmp, &mi_ext_panel_list) {
		struct mi_ext_panel_list_item *n;

		n = list_entry(pos, struct mi_ext_panel_list_item, list);
		if (n->panel_dev->client->dev.of_node == of_node) {
			panel_dev = n->panel_dev;
			break;
		}
	}
	mutex_unlock(&mi_ext_panel_list_lock);

	if (!panel_dev) {
		DISP_ERROR("Device with of node not found\n");
		return NULL;
	}

	return panel_dev;
}

int mi_ext_panel_set_power_mode(struct panel_device *panel_dev, int power_mode) {
	if (panel_dev && panel_dev->function.set_power_mode)
		panel_dev->function.set_power_mode(panel_dev, power_mode);
	return 0;
}

int mi_ext_panel_i2c_dev_probe(struct i2c_client *client,
			   const struct i2c_device_id *id) {
	int ret = 0;
	const struct panel_spec_info *spec_info;
	const struct of_device_id *node_id;
	struct panel_device *panel_dev = NULL; 
	u32 temp_u32_value = 0;
	struct mi_ext_panel_list_item *item;

	if (!client || !client->dev.of_node) {
		DISP_ERROR("i2c_client not found");
		return -ENODEV;
	}

	node_id = of_match_node(mi_ext_panel_match_table, client->dev.of_node);
	if (!node_id)
		return -ENODEV;

	spec_info = node_id->data;

	item = kzalloc(sizeof(struct mi_ext_panel_list_item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	panel_dev = kzalloc(sizeof(struct panel_device), GFP_KERNEL);
	if (!panel_dev) {
		ret = -ENOMEM;
		goto err_free_panel_item;
	}

	panel_dev->spec_info = spec_info;
	panel_dev->client = client;

	ret = of_property_read_u32(client->dev.of_node, "index", &temp_u32_value);
	if (!ret) {
		panel_dev->device_index = temp_u32_value;
	} else {
		panel_dev->device_index = 0;
	}

	if (panel_i2c_resource_int((void *)panel_dev))
		goto err_free_panel_dev;

	panel_dev->panel_class = panel_class;
	i2c_set_clientdata(client, panel_dev);

	item->panel_dev = panel_dev;
	mutex_lock(&mi_ext_panel_list_lock);
	list_add(&item->list, &mi_ext_panel_list);
	mutex_unlock(&mi_ext_panel_list_lock);

	switch (spec_info->type) {
		case PANEL_TYPE_IVI:
			snprintf(panel_dev->device_name, PANEL_NAME_SIZE, "ivi%d", panel_dev->device_index);
			panel_dev->private_dev = ivi_panel_device_init(panel_dev);
			break;
		case PANEL_TYPE_IVI_POGO:
			snprintf(panel_dev->device_name, PANEL_NAME_SIZE, "ivi_pogo%d", panel_dev->device_index);
			panel_dev->private_dev = ivi_pogo_device_init(panel_dev);
			break;
		case PANEL_TYPE_HUD:
			snprintf(panel_dev->device_name, PANEL_NAME_SIZE, "hud%d", panel_dev->device_index);
			panel_dev->private_dev = hud_panel_device_init(panel_dev);
			break;
		case PANEL_TYPE_CLUSTER:
			snprintf(panel_dev->device_name, PANEL_NAME_SIZE, "cluster%d", panel_dev->device_index);
			panel_dev->private_dev = cluster_panel_device_init(panel_dev);
			break;
	};	

	DISP_INFO("mi ext panel probe success");
	return 0;
err_free_panel_dev:
	kfree(panel_dev);
err_free_panel_item:
	kfree(item);
	DISP_ERROR("mi ext panel probe fail");
	return ret;
}

void  mi_ext_panel_i2c_dev_remove(struct i2c_client *client) {
	struct panel_device *panel_dev = i2c_get_clientdata(client);
	if (panel_dev->function.panel_deinit)
		panel_dev->function.panel_deinit(panel_dev);
	if (panel_dev)
		kfree(panel_dev);
	return;
}

static struct i2c_driver mi_ext_panel_i2c_dev_driver = {
	.probe = mi_ext_panel_i2c_dev_probe,
	.remove = mi_ext_panel_i2c_dev_remove,
	.driver = {
		.name = "mi_ext_panel",
		.of_match_table = mi_ext_panel_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

void mi_ext_panel_data_register(void) {
	DISP_INFO("register mi ext panel driver");
	panel_class = class_create(THIS_MODULE, CLASS_NAME);
	if (i2c_add_driver(&mi_ext_panel_i2c_dev_driver))
		DISP_ERROR("add mi ext panel i2c driver fail !!!");
}

void mi_ext_panel_data_unregister(void) {
	DISP_INFO("unregister mi ext panel driver");
	i2c_del_driver(&mi_ext_panel_i2c_dev_driver);
}
