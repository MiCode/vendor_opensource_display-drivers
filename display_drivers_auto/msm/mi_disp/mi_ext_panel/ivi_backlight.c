#include "ivi_backlight.h"
#include "mi_disp_print.h"
#include "mi_ext_panel.h"

DEFINE_MUTEX(brightness_cb_chain_list_mutex);
static BLOCKING_NOTIFIER_HEAD(brightness_cb_chain_list);
int register_brightness_notifier(struct notifier_block *nb)
{
	mutex_lock(&brightness_cb_chain_list_mutex);
	blocking_notifier_chain_register(&brightness_cb_chain_list, nb);
	mutex_unlock(&brightness_cb_chain_list_mutex);
	return 0;
}
EXPORT_SYMBOL(register_brightness_notifier);

static int32_t ivi_set_brightness(struct ivi_backlight* bl_data, int32_t brightness)
{
	int32_t ret = -1;
	uint16_t brightlevel;
	struct ivi_write_msg write_msg;
	struct ivi_panel_device* ivi_data =
				container_of(bl_data, struct ivi_panel_device, backlight_data);
	struct ivi_common_device* ivi_common_data = &ivi_data->common;

	if (brightness < 0 || brightness > PANEL_BACKLIGHT_LEVEL_MAX) {
        DISP_ERROR("%s invalid backlight setting value:%d", __func__, brightness);
        return -EINVAL;
	}

	brightlevel = (brightness == 0) ? 0 : (0xFFFF * brightness / PANEL_BACKLIGHT_LEVEL_MAX);
	EXT_PANEL_INFO(ivi_common_data->base, "backlight is %d, convert brightness is %d", brightness, brightlevel);

	write_msg.data[0] = bl_data->reg_address;
	write_msg.data[1] = brightlevel & 0xff;
	write_msg.data[2] = (brightlevel >> 8) & 0xff;
	write_msg.data[3] = 0x01; //0x01:backlight on, 0x02:backlight off

	ret = ivi_msg_send(ivi_common_data, write_msg, IVI_STANRD_WRITE_FRAME_LEN);
	mutex_lock(&brightness_cb_chain_list_mutex);
	blocking_notifier_call_chain(&brightness_cb_chain_list, brightlevel, NULL);
	mutex_unlock(&brightness_cb_chain_list_mutex);

	return 0;
}

static int32_t ivi_set_default_brightness(struct ivi_backlight* bl_data)
{
	struct ivi_panel_device* ivi_data =
				container_of(bl_data, struct ivi_panel_device, backlight_data);
	struct ivi_common_device* ivi_common_data = &ivi_data->common;

	mutex_lock(&bl_data->backlight_mutex);
	EXT_PANEL_INFO(ivi_common_data->base, "brightness = %d %d",bl_data->current_brightness, bl_data->init_brightness);
	if (bl_data->current_brightness >=0)
		ivi_set_brightness(bl_data, bl_data->current_brightness);
	else
		ivi_set_brightness(bl_data, bl_data->init_brightness);
	mutex_unlock(&bl_data->backlight_mutex);

	return 0;
}

static int32_t ivi_get_brightness(struct ivi_backlight* bl_data)
{
	return bl_data->current_brightness;
}

static int ivi_backlight_update_status(struct backlight_device *bd)
{
	struct ivi_backlight *bl_data = bl_get_data(bd);
	int32_t brightness = backlight_get_brightness(bd);

	mutex_lock(&bl_data->backlight_mutex);
	ivi_set_brightness(bl_data, brightness);
	bl_data->current_brightness = brightness;
	mutex_unlock(&bl_data->backlight_mutex);

	return 0;
}

static int ivi_backlight_get_brightness(struct backlight_device *bd)
{
	struct ivi_backlight *bl_data = bl_get_data(bd);
	return ivi_get_brightness(bl_data);
}

static const struct backlight_ops backlight_ops = {
	.get_brightness	= ivi_backlight_get_brightness,
	.update_status	= ivi_backlight_update_status,
};

struct ivi_backlight_ops ivi_bl_ops = {
	.set_brightness = ivi_set_brightness,
	.get_brightness = ivi_get_brightness,
	.set_default_brightness = ivi_set_default_brightness,
};

int ivi_backlight_init(struct ivi_backlight* bl_data)
{
	struct backlight_properties props;
	struct backlight_device *bd = NULL;
	struct device *dev = NULL;
	struct device_node *ivi_dev_node = NULL;
	struct device_node *backlight_dev_node = NULL;
	u32 temp_u32_value = 0;
	int ret = 0;
	char bl_node_name[PANEL_NAME_SIZE];
	struct ivi_panel_device* ivi_data = NULL;
	struct ivi_common_device* ivi_common_data = NULL;

	ivi_data = container_of(bl_data, struct ivi_panel_device, backlight_data);
	ivi_common_data = &ivi_data->common;

	memset(bl_node_name, 0, sizeof(bl_node_name));
	dev = &ivi_common_data->base->client->dev;
	ivi_dev_node = dev->of_node;

	mutex_init(&bl_data->backlight_mutex);

	backlight_dev_node = of_get_child_by_name(ivi_dev_node, "backlight");
	if (NULL == backlight_dev_node) {
		DISP_ERROR("can't find backlight node");
		return -1;
	}

	ret = of_property_read_u32(backlight_dev_node, "reg-address", &temp_u32_value);
	if (!ret) {
		bl_data->reg_address = temp_u32_value;
	} else {
		bl_data->reg_address = CMD_H2S_SEND_BL;
	}
	DISP_INFO("backlight reg-address is 0x%x", bl_data->reg_address);

	ret = of_property_read_u32(backlight_dev_node, "max-brightness", &temp_u32_value);
	if (!ret) {
		bl_data->max_brightness = temp_u32_value;
	} else {
		bl_data->max_brightness = PANEL_BACKLIGHT_LEVEL_MAX;
	}
	DISP_INFO("max_brightness:%d", bl_data->max_brightness);

	ret = of_property_read_u32(backlight_dev_node, "init-brightness", &temp_u32_value);
	if (!ret) {
		bl_data->init_brightness = temp_u32_value;
	} else {
		bl_data->init_brightness = PANEL_BACKLIGHT_LEVEL_INIT;
	}
	DISP_INFO("init_brightness:%d", bl_data->init_brightness);

	bl_data->current_brightness = -1;
	bl_data->backlight_ops = ivi_bl_ops;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = bl_data->max_brightness;
	props.brightness = bl_data->init_brightness;
	snprintf(bl_node_name, PANEL_NAME_SIZE, "%s-backlight", ivi_common_data->base->device_name);

	bd = devm_backlight_device_register(&ivi_common_data->base->client->dev, bl_node_name,
					ivi_common_data->base->client->dev.parent, &ivi_data->backlight_data, 
					&backlight_ops, &props);
	if (IS_ERR(bd)) {
		pr_err("%s backlight device register failed", __func__);
		return PTR_ERR(bd);
	}

	bl_data->bl_dev = bd;

	return 0;
}

int ivi_backlight_remove(struct ivi_backlight* bl_data)
{
	mutex_destroy(&bl_data->backlight_mutex);
	return 0;
}
