// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim MAX96789 serializer driver
 *
 * This driver & most of the hard coded values are based on the reference
 * application delivered by Maxim for this device.
 *
 * Copyright (C) 2016 Maxim Integrated Products
 * Copyright (C) 2017 Renesas Electronics Corporation
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "max96755.h"
#include "max96781.h"
#include "max96789.h"
#include <linux/regmap.h>
#include <linux/jiffies.h>


static struct blocking_notifier_head max967xx_notifier_list[MAX967XX_COUNT];
static struct class *max967xx_class = NULL;
#define CLASS_NAME "serdes"

struct max967xx_list_item {
	struct max967xx_data *data;
	struct list_head list;
};

static LIST_HEAD(max967xx_list);
static DEFINE_MUTEX(max967xx_list_lock);

static const struct regmap_config max967xx_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};

static struct max967xx_priv max96755_priv_data = {
	.max967xx_is_locked = max96755_is_locked,
	.max967xx_dump_reg = max96755_dump_regs,
	.max967xx_test_pattern = max96755_test_pattern,
	.max967xx_check_dev_id = max96755_check_dev_id,
	.max967xx_pre_init = max96755_pre_init,
	.max967xx_init = max96755_initialize,
	.max967xx_get_link_state = max96755_get_link_state,
	.max967xx_power_ctrl = max96755_power_ctrl,
	.max967xx_colorbar_enable = max96755_colorbar_enable,
	.type = MAX96755,
	.deserializer_count = 1,
	.deserializer_i2c_addresses[SINGLE_A] = 0x48,
	.deserializer_name[SINGLE_A] = "ivi_max96772",
	.post_init = false,
	.debug = MAX96755_DEBUG ? true : false,
};

static struct max967xx_priv max96781_priv_data = {
	.max967xx_is_locked = max96781_is_locked,
	.max967xx_dump_reg = max96781_dump_regs,
	.max967xx_test_pattern = max96781_test_pattern,
	.max967xx_force_enable = max96781_force_enable,
	.max967xx_check_dev_id = max96781_check_dev_id,
	.max967xx_pre_init = max96781_pre_init,
	.max967xx_init = max96781_initialize,
	.type = MAX96781,
	.deserializer_count = 2,
	.deserializer_i2c_addresses = { 0x48, 0x2a },
	.deserializer_name = { "clu_mst_max96752", "hud_mst_max96752" },
	.post_init = false,
	.debug = MAX96781_DEBUG ? true : false,
};

static struct max967xx_priv max96789_priv_data = {
	.max967xx_is_locked = max96789_is_locked,
	.max967xx_dump_reg = max96789_dump_regs,
	.max967xx_test_pattern = max96789_test_pattern,
	.max967xx_check_dev_id = max96789_check_dev_id,
	.max967xx_pre_init = max96789_pre_init,
	.max967xx_init = max96789_initialize,
	.type = MAX96789,
	.deserializer_count = 1,
	.deserializer_i2c_addresses = { 0x2a, 0x6c },
	 //0x2a is hud
	.deserializer_name[SINGLE_A] = "hud_max96752",
	.post_init = false,
	.debug = MAX96789_DEBUG ? true : false,
};

static int max967xx_set_pinctrl_state(struct max967xx_data *data, bool enable);

int max967xx_write(struct max967xx_data *data, int reg, int value,
		   struct regmap *regmap)
{
	return regmap_write(regmap, reg, value);
}

int max967xx_write_regs(struct max967xx_data *data, const struct cmd_16bit *cmd,
			int len, struct regmap *regmap)
{
	int i, ret = 0;

	for (i = 0; i < len; i++) {
		ret += max967xx_write(data, cmd[i].reg_addr, cmd[i].reg_value,
				      regmap);
		if (ret != 0) {
			DISP_ERROR("reg 0x%04x failed, cnt %d", cmd[i].reg_addr,
				   len);
			break;
		}
		if (cmd[i].delay) {
			DISP_INFO("udelay %d", cmd[i].delay);
			usleep_range(cmd[i].delay, cmd[i].delay + 50);
		}
	}

	return ((ret >= 0) ? 0 : -1);
}

int max967xx_read(int reg, int *val, struct regmap *regmap)
{
	int ret;
	ret = regmap_read(regmap, reg, val);
	if (ret < 0)
		return ret;

	return 0;
}

int max967xx_parse_dt(struct device *dev, struct max967xx_data *data)
{
	struct device_node *np = dev->of_node;
	if (!np) {
		DISP_ERROR("max967xx of_node NULL");
		return -EINVAL;
	}

	data->gpio_pwdn = -EINVAL;

	data->lock_irq = irq_of_parse_and_map(np, 0);
	data->errb_irq = irq_of_parse_and_map(np, 1);

	if (of_property_read_string_index(np, "interrupt-names", 0,
					  &data->lock_irq_name)) {
		DISP_ERROR("get lock irq name failed");
		return -EINVAL;
	}

	if (of_property_read_string_index(np, "interrupt-names", 1,
					  &data->errb_irq_name)) {
		DISP_ERROR("get lock irq name failed");
		return -EINVAL;
	}

	data->lock_gpio = of_get_named_gpio(np, "lock-irq-gpio", 0);
	if ((!gpio_is_valid(data->lock_gpio))) {
		data->lock_gpio = -EINVAL;
		DISP_INFO("lock-irq-gpio is invalid");
	}

	data->errb_gpio = of_get_named_gpio(np, "errb-irq-gpio", 0);
	if ((!gpio_is_valid(data->errb_gpio))) {
		data->errb_gpio = -EINVAL;
		DISP_INFO("errb-irq-gpio is invalid");
	}

	DISP_INFO("%s is %d,and %s is %d", data->lock_irq_name, data->lock_irq,
		  data->errb_irq_name, data->errb_irq);

	data->gpio_pwdn = of_get_named_gpio(np, "pwdn", 0);
	if ((!gpio_is_valid(data->gpio_pwdn))) {
		DISP_ERROR("max967xx gpio pwdn invalid %d", data->gpio_pwdn);
		return data->gpio_pwdn;
	}
	DISP_INFO("max967xx pwdn %d", data->gpio_pwdn);

	return 0;
}

static int max967xx_state_machine(struct max967xx_data *data)
{
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;
	priv_data->max967xx_dump_reg(data, NULL);
	return 0;
}

static void max967xx_detect_work(struct work_struct *work)
{
	struct max967xx_data *data =
		container_of(work, struct max967xx_data, check_work.work);
	max967xx_state_machine(data);

	schedule_delayed_work(&data->check_work, msecs_to_jiffies(2000));
}

int max967xx_register_notifier(struct notifier_block *nb, enum GMSL_TYPE type)
{
	return blocking_notifier_chain_register(&max967xx_notifier_list[type],
						nb);
}
EXPORT_SYMBOL_GPL(max967xx_register_notifier);

int max967xx_unregister_notifier(struct notifier_block *nb, enum GMSL_TYPE type)
{
	return blocking_notifier_chain_unregister(&max967xx_notifier_list[type],
						  nb);
}
EXPORT_SYMBOL_GPL(max967xx_unregister_notifier);

static irqreturn_t lock_irq_handler(int irq, void *arg)
{
	struct max967xx_data *data = (struct max967xx_data *)arg;
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;
	DISP_INFO("%s enter", __func__);
	if ((priv_data->max967xx_is_locked(data, 0)) > 0 &&
	    (priv_data->post_init == false)) {
		//for initing after dp irq setup
		if (priv_data->type == MAX96781)
			usleep_range(2000000,2000000);
		priv_data->max967xx_init(data);
		priv_data->post_init = true;
	}

	return IRQ_HANDLED;
}

static irqreturn_t errb_irq_handler(int irq, void *arg)
{
	//do not use this irq
	DISP_INFO("enter");
	return IRQ_HANDLED;
}

//only act as a interface for enabling dp without actual panel
static ssize_t force_enable_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;

	if (priv_data->max967xx_force_enable)
		priv_data->max967xx_force_enable(data);

	return count;
}

static ssize_t reg_dump_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	int serdes_type = 0;
	int reg  = 0;
	int value = 0;

	sscanf(buf, "%d 0x%04x 0x%02x",&serdes_type, &reg, &value);
	DISP_ERROR("%d %04x %02x",serdes_type,reg,value);

	max967xx_write(data, reg, value, data->regmap[serdes_type]);

	return count;
}

static ssize_t reg_dump_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;
	char dump_buffer[PAGE_SIZE] = "\0";

	memset(dump_buffer, 0, PAGE_SIZE);
	priv_data->max967xx_dump_reg(data, dump_buffer);

	return snprintf(buf, PAGE_SIZE, "%s\n", dump_buffer);
}

static ssize_t test_pattern_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;
	char dump_buffer[PAGE_SIZE] = "\0";

	memset(dump_buffer, 0, PAGE_SIZE);
	priv_data->max967xx_test_pattern(data, dump_buffer);

	return snprintf(buf, PAGE_SIZE, "%s\n", dump_buffer);
}

static ssize_t pwdn_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	char dump_buffer[PAGE_SIZE] = "\0";
	int gpio_state;

	memset(dump_buffer, 0, PAGE_SIZE);

	if (data->gpio_pwdn < 0)
		return 0;

	gpio_state = gpio_get_value(data->gpio_pwdn);
	DISP_ERROR("gpio_pwdn_state is %d", gpio_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", gpio_state);
}

static ssize_t lock_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	char dump_buffer[PAGE_SIZE] = "\0";
	int gpio_state;

	memset(dump_buffer, 0, PAGE_SIZE);

	if (data->lock_gpio < 0)
		return 0;

	gpio_state = gpio_get_value(data->lock_gpio);
	DISP_ERROR("lock_gpio_state is %d", gpio_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", gpio_state);
}


static ssize_t errb_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	char dump_buffer[PAGE_SIZE] = "\0";
	int gpio_state;

	memset(dump_buffer, 0, PAGE_SIZE);

	if (data->errb_gpio < 0)
		return 0;

	gpio_state = gpio_get_value(data->errb_gpio);
	DISP_ERROR("errb_gpio_state is %d", gpio_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", gpio_state);
}

static ssize_t colorbar_store(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct max967xx_data* data = dev_get_drvdata(device);
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;
	int enable = 0;

	sscanf(buf, "%d", &enable);

	if (priv_data->max967xx_colorbar_enable)
		priv_data->max967xx_colorbar_enable(data, enable);

	return count;
}

static DEVICE_ATTR_WO(force_enable);
static DEVICE_ATTR_RW(reg_dump);
static DEVICE_ATTR_RO(test_pattern);
static DEVICE_ATTR_RO(pwdn);
static DEVICE_ATTR_RO(lock);
static DEVICE_ATTR_RO(errb);
static DEVICE_ATTR_WO(colorbar);


static struct attribute *max9677xx_attrs[] = {
	&dev_attr_force_enable.attr,
	&dev_attr_reg_dump.attr,
	&dev_attr_test_pattern.attr,
	&dev_attr_pwdn.attr,
	&dev_attr_lock.attr,
	&dev_attr_errb.attr,
	&dev_attr_colorbar.attr,
	NULL
};

static const struct attribute_group max9677xx_group = {
	.attrs = max9677xx_attrs,
};

static const struct attribute_group *max9677xx_groups[] = {
	&max9677xx_group,
	NULL
};

int max967xx_create_device(struct max967xx_data *data)
{
	int ret = 0;
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;

	switch(priv_data->type) {
		case MAX96755:
			data->class_dev = device_create(max967xx_class, NULL, 'T', data, "max96755");
			break;
		case MAX96781:
			data->class_dev = device_create(max967xx_class, NULL, 'T', data, "max96781");
			break;
		case MAX96789:
			data->class_dev = device_create(max967xx_class, NULL, 'T', data, "max96789");
			break;
		default:
			break;
	}
	ret = sysfs_create_groups(&data->class_dev->kobj, max9677xx_groups);
	return ret;
}

static void max976xx_parse_mi_ext_panel(struct max967xx_data *data)
{
	struct device_node *of_node = data->client[IS_HOST_I2C_ADDRESS]->dev.of_node;
	struct device_node *mi_ext_panel_node = NULL;

	data->mi_ext_panel_count = of_property_count_u32_elems(of_node, "mi,ext-panel-num");

	DISP_INFO("mi ext panel count=%d\n", data->mi_ext_panel_count);

	if (data->mi_ext_panel_count <= 0) {
		DISP_ERROR("mi,ext-panel-num not found");
		return;
	}
	{
		int num;
		display_for_each_mi_ext_panel(num, data->mi_ext_panel_count) {
			struct panel_device *panel_dev = NULL;
			int index;

			of_property_read_u32_index(of_node, "mi,ext-panel-num",
					num, &index);
			mi_ext_panel_node = of_parse_phandle(of_node,
					"mi,ext-panel", index);
			of_node_put(mi_ext_panel_node);
			panel_dev = mi_ext_panel_get(mi_ext_panel_node);
			if (IS_ERR_OR_NULL(panel_dev)) {
				DISP_ERROR("mi ext panel get fail");
				panel_dev = NULL;
				return;
			}
			data->mi_ext_panel[num] = panel_dev;
		}
	}
	DISP_INFO("parse mi ext panel success");
	return;
}

struct max967xx_data* max967xx_data_get(struct device_node *parent_node)
{
	struct list_head *pos, *tmp;
	struct max967xx_data *data = NULL;
	struct device_node *max967xx_node = NULL;

	if (!parent_node)
		return data;

	max967xx_node = of_parse_phandle(parent_node,
			"mi,max967xx", 0);
	of_node_put(max967xx_node);
	if (!max967xx_node)
		return data;

	mutex_lock(&max967xx_list_lock);
	list_for_each_safe(pos, tmp, &max967xx_list) {
		struct max967xx_list_item *n;

		n = list_entry(pos, struct max967xx_list_item, list);
		if (n->data->client[IS_HOST_I2C_ADDRESS]->dev.of_node == max967xx_node) {
			data = n->data;
			break;
		}
	}
	mutex_unlock(&max967xx_list_lock);

	if (!data) {
		DISP_ERROR("Device with of node not found");
		return NULL;
	}

	return data;
}

int max967xx_set_power_mode(struct max967xx_data *data, int mode) {
	int num;
	struct max967xx_priv *priv_data = (struct max967xx_priv *)data->priv_data;

	DISP_INFO("max967xx set power mode %d", mode);
	display_for_each_mi_ext_panel(num, data->mi_ext_panel_count) {
		if (data->mi_ext_panel[num])
			mi_ext_panel_set_power_mode(data->mi_ext_panel[num], mode);
	}

	if (priv_data->max967xx_power_ctrl)
		priv_data->max967xx_power_ctrl(data, mode);
	if (mode == SERDES_STATE_OPEN)
		max967xx_set_pinctrl_state(data, true);
	else
		max967xx_set_pinctrl_state(data, false);

	return 0;
}

int32_t max967xx_send_ctrl_info(struct max967xx_data *data, serdes_msg_type type, serdes_work_state state)
{
	serdes_msg *msg;
	unsigned long flags;

	msg = kzalloc(sizeof(serdes_msg), GFP_KERNEL);
	if (msg == NULL) {
		DISP_ERROR("serdes_msg kzalloc failed");
		return -1;
	}

	msg->state = state;
	msg->type = type;
	spin_lock_irqsave(&data->spinlock, flags);
	INIT_LIST_HEAD(&msg->node);
	list_add_tail(&msg->node, &data->msg_head);
	wake_up_interruptible(&data->pending_wq);
	spin_unlock_irqrestore(&data->spinlock, flags);

	return 0;
}

int32_t max967xx_recv_ctrl_info(struct max967xx_data *data, serdes_msg *recv_msg, int32_t time_out)
{
	serdes_msg *entry = NULL, *entry_next = NULL;
	int32_t ret = 0;
	unsigned long flags;

	if (recv_msg == NULL) {
		DISP_ERROR("recv_msg invalid");
		return -EINVAL;
	}
	memset(recv_msg, 0, sizeof(*recv_msg));
	ret = wait_event_interruptible_timeout(data->pending_wq,
		!list_empty(&data->msg_head), msecs_to_jiffies(time_out));
	if (ret < 0) {
		DISP_ERROR("unexpected error, ret = %d", ret);
		return ret;
	}
	if (!ret) {
		DISP_DEBUG("wait timeout");
		return ret;
	}

	spin_lock_irqsave(&data->spinlock, flags);
	list_for_each_entry_safe(entry, entry_next, &data->msg_head, node) {
		recv_msg->type = entry->type;
		recv_msg->state = entry->state;
		list_del(&entry->node);
		kfree(entry);
		break;
	}
	spin_unlock_irqrestore(&data->spinlock, flags);

	return ret;
}

static inline struct max967xx_priv *bridge_to_max967xx(struct drm_bridge *bridge)
{
	return container_of(bridge, struct max967xx_priv, bridge);
}

static int max967xx_bridge_attch(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	//struct max967xx_data *data = bridge_to_max967xx(bridge);
	DISP_ERROR(" enter");

	return 0;
}

static void max967xx_bridge_disable(struct drm_bridge *bridge)
{
	DISP_ERROR(" enter");
}

static void max967xx_bridge_enable(struct drm_bridge *bridge)
{
	DISP_ERROR(" enter");
}


static const struct drm_bridge_funcs max967xx_bridge_funcs = {
	.attach = max967xx_bridge_attch,
	.disable = max967xx_bridge_disable,
	.enable = max967xx_bridge_enable,
};

static int max967xx_set_pinctrl_state(struct max967xx_data *data, bool enable)
{
	int rc = 0;
	struct pinctrl_state *state;

	if (!data->pinctrl.pinctrl)
		return 0;

	if (enable)
		state = data->pinctrl.active;
	else
		state = data->pinctrl.suspend;

	rc = pinctrl_select_state(data->pinctrl.pinctrl, state);
	if (rc)
		DISP_ERROR("failed to set pin state, rc=%d\n", rc);

	return rc;
}

static int max967xx_pinctrl_init(struct max967xx_data *data)
{
	int rc = 0;
	char active_pinctrl_name[20] = {0}, suspend_pinctrl_name[20] = {0};
	struct max967xx_priv *priv_data = (struct max967xx_priv *)data->priv_data;

	data->pinctrl.pinctrl = devm_pinctrl_get(&data->client[IS_HOST_I2C_ADDRESS]->dev);
	if (IS_ERR_OR_NULL(data->pinctrl.pinctrl)) {
		rc = PTR_ERR(data->pinctrl.pinctrl);
		DISP_ERROR("failed to get pinctrl, rc=%d\n", rc);
		goto error;
	}

	switch (priv_data->type) {
	case MAX96755:
		snprintf(active_pinctrl_name, 20, "max96755_active");
		snprintf(suspend_pinctrl_name, 20, "max96755_suspend");
		break;
	case MAX96781:
		snprintf(active_pinctrl_name, 20, "max96781_active");
		snprintf(suspend_pinctrl_name, 20, "max96781_suspend");
		break;
	case MAX96789:
		snprintf(active_pinctrl_name, 20, "max96789_active");
		snprintf(suspend_pinctrl_name, 20, "max96789_suspend");
		break;
	default:
		goto error;
		break;
	};

	data->pinctrl.active = pinctrl_lookup_state(data->pinctrl.pinctrl,
						       active_pinctrl_name);
	if (IS_ERR_OR_NULL(data->pinctrl.active)) {
		rc = PTR_ERR(data->pinctrl.active);
		DISP_ERROR("cyb-test failed to get pinctrl active state, rc=%d\n", rc);
		goto error;
	}

	data->pinctrl.suspend = pinctrl_lookup_state(data->pinctrl.pinctrl,
								suspend_pinctrl_name);
	if (IS_ERR_OR_NULL(data->pinctrl.suspend)) {
		rc = PTR_ERR(data->pinctrl.suspend);
		DISP_ERROR("cyb-test failed to get pinctrl suspend state, rc=%d\n", rc);
		goto error;
	}

	return rc;
error:
	data->pinctrl.pinctrl = NULL;
	return rc;
}

int max967xx_i2c_dev_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret = 0;
	int i;
	struct regmap *regmap = NULL;
	struct max967xx_data *data = NULL;
	struct max967xx_priv *priv_data = NULL;
	struct max967xx_list_item *item;

	data = kzalloc(sizeof(struct max967xx_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	data->client[IS_HOST_I2C_ADDRESS] = client;
	data->priv_data =
		(struct max967xx_priv *)of_device_get_match_data(&client->dev);

	priv_data = (struct max967xx_priv *)data->priv_data;
	priv_data->bridge.funcs = &max967xx_bridge_funcs;
	priv_data->bridge.of_node = client->dev.of_node;
	drm_bridge_add(&priv_data->bridge);

	DISP_INFO(" enter,type %d", priv_data->type);
	/* retrieve details of gpios from dt */
	ret = max967xx_parse_dt(&client->dev, data);
	if (ret) {
		DISP_ERROR("failed to parse dt");
		goto err_free_max967xx_data;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DISP_ERROR("need I2C_FUNC_I2C");
		ret = -ENODEV;
		goto err_free_max967xx_data;
	}
	regmap = devm_regmap_init_i2c(client, &max967xx_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	data->regmap[IS_HOST_I2C_ADDRESS] = regmap;

	ret = gpio_request(data->gpio_pwdn, "max967xx_pwdn");
	if (ret) {
		DISP_ERROR(" unable to request max967xx_gpio[%d]",
			   data->gpio_pwdn);
		goto err_free_max967xx_data;
	}

	ret = gpio_direction_output(data->gpio_pwdn, 1);
	if (ret) {
		DISP_ERROR("unable to request nfc gpio [%d]", data->gpio_pwdn);
		goto err_free_pwdn_gpio;
	}
	usleep_range(500, 1000);
	if (priv_data->max967xx_check_dev_id(data) < 0) {
		DISP_ERROR("max967xx_check_dev_id failed,ser type %d",
			   priv_data->type);
		goto err_free_pwdn_gpio;
	}

	mutex_init(&data->mutex);

	INIT_DELAYED_WORK(&data->check_work, max967xx_detect_work);
	if (priv_data->debug)
		schedule_delayed_work(&data->check_work,
				      msecs_to_jiffies(5000));

	for (i = 0; i < MAX967XX_COUNT; i++) {
		BLOCKING_INIT_NOTIFIER_HEAD(&max967xx_notifier_list[i]);
	}
	i2c_set_clientdata(client, data);

	for (i = 0; i < priv_data->deserializer_count; i++) {
		//the first is serializer
		data->client[i+1] = i2c_new_dummy_device(
			client->adapter,
			priv_data->deserializer_i2c_addresses[i]);
		if (IS_ERR(data->client[i+1])) {
			DISP_ERROR("i2c_new_dummy_device 0x%x failed",
				   priv_data->deserializer_i2c_addresses[i]);
			goto err_free_pwdn_gpio;
		}
		DISP_INFO("i2c_new_dummy_device success\n");
		data->regmap[i + 1] = devm_regmap_init_i2c(
			data->client[i+1], &max967xx_regmap_config);
		if (IS_ERR(data->regmap[i+1])) {
			DISP_ERROR("devm_regmap_init_i2c failed");
			goto err_unregister_i2c_dummy_clients;
		}
	}

	max967xx_create_device(data);

	item = kzalloc(sizeof(struct max967xx_list_item), GFP_KERNEL);
	if (!item) {
		ret = -ENOMEM;
		goto err_unregister_i2c_dummy_clients;
	}
	item->data = data;
	mutex_lock(&max967xx_list_lock);
	list_add(&item->list, &max967xx_list);
	mutex_unlock(&max967xx_list_lock);

	max976xx_parse_mi_ext_panel(data);

	if (priv_data->type == MAX96755) {
		//if locked in uefi, ignore the pre_init
		if (priv_data->max967xx_get_link_state(data) != DUAL_LINK)
			priv_data->max967xx_pre_init(data);
		else
			priv_data->post_init = true;
		priv_data->max967xx_init(data);
	} else
		priv_data->max967xx_pre_init(data);

	max967xx_pinctrl_init(data);
	//max967xx_set_pinctrl_state(data, true);

	ret = request_threaded_irq(data->lock_irq, NULL, lock_irq_handler,
				   IRQF_TRIGGER_RISING | IRQF_ONESHOT |
					   IRQF_TRIGGER_FALLING,
				   data->lock_irq_name, data);
	if (ret) {
		DISP_ERROR("unable to request_irq[%d] err [0x%x]",
			   data->lock_irq, ret);
		goto err_free_pwdn_gpio;
	}

	ret = request_threaded_irq(data->errb_irq, NULL, errb_irq_handler,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   data->errb_irq_name, data);
	if (ret) {
		DISP_ERROR("unable to request_irq[%d]", data->errb_irq);
		goto err_free_pwdn_gpio;
	}

	DISP_INFO("probing max967xx i2c success");

	return 0;
err_unregister_i2c_dummy_clients:
	for (i = 0; i < priv_data->deserializer_count; i++) {
		//the first is serializer
		i2c_unregister_device(data->client[i+1]);
	}
err_free_pwdn_gpio:
	gpio_free(data->gpio_pwdn);
err_free_max967xx_data:
	kfree(data);
err:
	DISP_ERROR("probing not successful, check hardware");
	return ret;
}

void max967xx_i2c_dev_remove(struct i2c_client *client)
{
	int i;
	struct max967xx_data *data = i2c_get_clientdata(client);
	struct max967xx_priv *priv_data =
		(struct max967xx_priv *)data->priv_data;

	DISP_INFO("remove device");
	cancel_delayed_work_sync(&data->check_work);
	#if 0
	if (!data) {
		DISP_ERROR("device doesn't exist anymore\n");
		ret = -ENODEV;
		return ret;
	}
	#endif
	gpio_free(data->gpio_pwdn);
	for (i = 0; i < priv_data->deserializer_count; i++) {
		i2c_unregister_device(data->client[i+1]);
	}
	kfree(data);
	return;
}

static const struct of_device_id max967xx_i2c_dev_match_table[] = {
	{
		.compatible = MAX96755_DRV_STR,
		.data = &max96755_priv_data,
	},
	{
		.compatible = MAX96781_DRV_STR,
		.data = &max96781_priv_data,
	},
	{
		.compatible = MAX96789_DRV_STR,
		.data = &max96789_priv_data,
	},
	{}
};

static struct i2c_driver max967xx_i2c_dev_driver = {
	.probe = max967xx_i2c_dev_probe,
	.remove = max967xx_i2c_dev_remove,
	.driver = {
		.name = "max967xx",
		//.pm = &max967xx_i2c_dev_pm_ops,
		.of_match_table = max967xx_i2c_dev_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

MODULE_DEVICE_TABLE(of, max967xx_i2c_dev_match_table);

int max967xx_i2c_dev_init(void)
{
	int ret = 0;
	DISP_INFO("loading max967xx I2C driver");
	mi_ext_panel_data_register();

	max967xx_class = class_create(THIS_MODULE, CLASS_NAME);
	ret = i2c_add_driver(&max967xx_i2c_dev_driver);
	if (ret != 0)
		DISP_ERROR("max967xx add driver error ret %d\n", ret);
	return ret;
}

void max967xx_i2c_dev_exit(void)
{
	DISP_INFO("unloading MAX967XX driver");
	i2c_del_driver(&max967xx_i2c_dev_driver);
}
