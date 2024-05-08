
/*
 * mi ivi related module driver
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/regmap.h>
#include <linux/jiffies.h>
#include "ivi_touch.h"
#include "ivi_backlight.h"
#include <linux/of_irq.h>
#include "mi_disp_print.h"
#include "mi_ext_panel.h"

static int32_t process_ivi_screen_status(struct ivi_panel_device *ivi_data, uint8_t *reg_data)
{
	struct ivi_common_device* ivi_common_data = &ivi_data->common;
	switch (reg_data[1]) {
		case STATUS_NORMAL_MODE:
			DISP_INFO("panel is normal mode");
			ivi_send_timestamp(ivi_common_data);
			ivi_request_screeninfo(ivi_common_data);
			break;
		case STATUS_UPGRADE_MODE:
			DISP_INFO("panel is upgrade mode");
			break;
		case STATUS_SELF_TEST_MODE:
			DISP_INFO("panel is self test mode");
			break;
		default:
			break;
	}

	return 0;
}

static int32_t process_version_info(struct ivi_panel_device *ivi_data, uint8_t *reg_data)
{
	struct ivi_version_info version;

	version.hw_ver_major = reg_data[1];
	version.hw_ver_minor = reg_data[2];
	version.sw_ver_major = reg_data[3];
	version.sw_ver_minor = reg_data[4];
	version.ts_ver_major = reg_data[5];
	version.ts_ver_middle = reg_data[6];
	version.ts_ver_minor = reg_data[7];

	DISP_INFO("ivi version: %02x.%02x/%02x.%02x/%02x.%02x.%02x", version.hw_ver_major, version.hw_ver_minor,
		version.sw_ver_major, version.sw_ver_minor, version.ts_ver_major, version.ts_ver_middle,
		version.ts_ver_minor);

	return 0;
}

static int32_t ivi_process_irq(struct ivi_panel_device *ivi_data)
{
	int32_t count = IVI_STANRD_READ_FRAME_LEN;
	int32_t retry = IVI_MCU_READ_CMD_RETRY_TIMES;
	uint8_t cmd_id;
	struct ivi_read_msg read_msg;
	int32_t ret = -1;
	struct ivi_backlight* backlight_data;
	uint8_t std_buf[IVI_STANRD_READ_FRAME_LEN] = {};
	struct ivi_common_device* ivi_common_data = &ivi_data->common;

	memset(&read_msg, 0, sizeof(read_msg));
	read_msg.cmd_id = 0xfe;
	read_msg.num = IVI_STANRD_READ_FRAME_LEN;
	read_msg.data = std_buf;

	while (retry--) {
		ret = ivi_msg_receive(ivi_common_data, &read_msg, count);
		if (!ret) {
			break;
		}
		DISP_INFO("read cmd retry %d times", retry);
		if (retry <= 0) {
			DISP_ERROR("irq read cmd failed");
			return -EINTR;
		}
	}

	cmd_id = read_msg.data[0];

	EXT_PANEL_INFO(ivi_common_data->base, "cmd_id = %x", cmd_id);

	switch (cmd_id) {
		case CMD_S2H_TP_STANDARD_INFO:
			process_touch_info(&ivi_data->touch_data, read_msg.data);
			break;
		case CMD_S2H_TP_EXTEND_INFO:
			break;
		case CMD_S2H_SCREEN_STATUS:
			process_ivi_screen_status(ivi_data, read_msg.data);
			break;
		case CMD_S2H_BL_REQUEST:
			backlight_data = &(ivi_data->backlight_data);
			backlight_data->backlight_ops.set_default_brightness(backlight_data);
			break;
		case CMD_S2H_WAIT_TIMEOUT:
			//process_res_timeout_cfg(device, cmd.val);
			break;
		case CMD_S2H_SCREEN_INFO_RESPOND:
			//process_screen_info(device, cmd.val);
			break;
		case CMD_S2H_PRODUCT_ID:
			//process_product_id(device, cmd.val);
			break;
		case CMD_S2H_VERSION_INFO:
			process_version_info(ivi_data, read_msg.data);
			backlight_data = &(ivi_data->backlight_data);
			backlight_data->backlight_ops.set_default_brightness(backlight_data);
			break;
		case CMD_S2H_DIAGNOSE_INFO:
			//process_diagnose_info(device, cmd.val);
			break;
		case CMD_S2H_UPGRADE:
			//ivi_uds_msg_recv(device, cmd.val);
			break;
		case CMD_S2H_TIMESTAMP_REQUEST:
			ivi_send_timestamp(ivi_common_data);
			break;
		case CMD_S2H_ECU_SN_STANDARD:
			//process_sn_info(device, cmd.val);
			break;
		case CMD_S2H_ECU_PN_STANDARD:
			//process_pn_info(device, cmd.val);
			break;
		case CMD_S2H_POGO_ACCESS_STATUS:
			//process_pogo_access_status(device, cmd.val);
			break;
		default:
			DISP_ERROR("not support cmd id %d", cmd_id);
			break;
	}

	return 0;
}

static ssize_t msg_rw_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct ivi_panel_device* data = dev_get_drvdata(device);
	uint8_t cmd = 0;
	int value = 0;
	struct ivi_backlight* backlight_data;

	sscanf(buf, "0x%x %d", &cmd, &value);
	if(!data)
		DISP_ERROR("input data invalid");

	switch (cmd) {
		case CMD_H2S_SCREEN_INFO_REQUEST:
			ivi_request_screeninfo(&data->common);
			break;
		case CMD_H2S_SEND_BL:
			backlight_data = &(data->backlight_data);
			backlight_data->backlight_ops.set_brightness(backlight_data, value);
			break;
		case CMD_H2S_TIMESTAMP_RESPOND:
			ivi_send_timestamp(&data->common);
			break;
		default:
			DISP_ERROR("not support cmd id %d", cmd);
			break;
	}

	return count;
}

static ssize_t msg_rw_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct ivi_panel_device* data = dev_get_drvdata(device);
	if(!data)
		DISP_ERROR("ivi data is null");
	return 0;
}

static ssize_t irq_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct ivi_panel_device* data = dev_get_drvdata(device);
	int gpio_state;

	if (data->irq_gpio < 0)
		return 0;

	gpio_state = gpio_get_value(data->irq_gpio);
	DISP_INFO("irq_gpio_state is %d", gpio_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", gpio_state);
}


static DEVICE_ATTR_RW(msg_rw);
static DEVICE_ATTR_RO(irq);


static struct attribute *ivi_attrs[] = {
	&dev_attr_msg_rw.attr,
	&dev_attr_irq.attr,
	NULL
};

static const struct attribute_group ivi_group = {
	.attrs = ivi_attrs,
};

static const struct attribute_group *ivi_groups[] = {
	&ivi_group,
	NULL
};

static irqreturn_t ivi_irq_handler(int irq, void *arg)
{
	struct ivi_panel_device *data = (struct ivi_panel_device *)arg;
	if (data)
		ivi_process_irq(data);
	return IRQ_HANDLED;
}

int32_t ivi_set_power_mode(panel_device *device, int power_mode) {
	DISP_INFO("ivi panel set power mode %d", power_mode);
	return 0;
}

static struct panel_function panel_function_ops = {
	.set_power_mode = ivi_set_power_mode,
};

void* ivi_panel_device_init(struct panel_device *panel_base) {
	int ret = 0;
	struct ivi_panel_device *data = NULL;
	struct ivi_common_device *ivi_common_data = NULL;

	DISP_INFO("ivi panel device init start");
	data = kzalloc(sizeof(struct ivi_panel_device), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ivi_common_data = &data->common;
	ivi_common_device_init(ivi_common_data, panel_base);

	ivi_touch_init(&data->touch_data);
	ivi_backlight_init(&data->backlight_data);
	data->irq_gpio = of_get_named_gpio(panel_base->client->dev.of_node, "irq-gpio", 0);
	if ((!gpio_is_valid(data->irq_gpio))) {
		data->irq_gpio = -EINVAL;
		DISP_INFO("irq-gpio is invalid");
	}

	ret = request_threaded_irq(ivi_common_data->irq, NULL, ivi_irq_handler,
				   IRQF_TRIGGER_FALLING|IRQF_ONESHOT, panel_base->device_name, data);
	if (ret) {
		DISP_ERROR("unable to request ivi irq");
		goto err_free_data;
	}

	panel_base->function = panel_function_ops;
	ret = panel_create_sysfs_device((void*)panel_base, data, ivi_groups);
	DISP_INFO("ivi init success !!!");

	return (void*)data;
err_free_data:
	kfree(data);
err:
	DISP_INFO("ivi init fail !!!");
	return NULL;

}

