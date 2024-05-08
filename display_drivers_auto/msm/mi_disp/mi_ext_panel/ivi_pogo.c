#include "ivi.h"
#include "mi_ext_panel.h"
#include <linux/of_irq.h>

static ssize_t i2c_rw_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct ivi_pogo_device* data = dev_get_drvdata(device);
	struct i2c_interface* i2c_inst = &data->common.base->i2c_instance;
	uint8_t cmd = 0, value = 0;
	int ret, type;

	sscanf(buf, "%d 0x%x %d", &type, &cmd, &value);
	if(!data)
		DISP_ERROR("input data invalid");

	DISP_INFO("type: %d reg 0x%x, value: %d", type, cmd, value);
	if (type)
		ret = i2c_inst->ops.i2c_write(i2c_inst, cmd, value);
	else
		ret = i2c_inst->ops.i2c_read(i2c_inst, cmd, &value, 1);
	if (ret) {
		DISP_ERROR("i2c_store fail ret = %d", ret);
		return ret;
	}

	return count;
}

static ssize_t i2c_rw_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct ivi_pogo_device* data = dev_get_drvdata(device);

	if(!data)
		DISP_ERROR("ivi data is null");

	return 0;
}

static DEVICE_ATTR_RW(i2c_rw);

static struct attribute *ivi_pogo_attrs[] = {
	&dev_attr_i2c_rw.attr,
	NULL
};

static const struct attribute_group ivi_pogo_group = {
	.attrs = ivi_pogo_attrs,
};

static const struct attribute_group *ivi_pogo_groups[] = {
	&ivi_pogo_group,
	NULL
};

static int32_t process_ivi_pogo_screen_status(struct ivi_pogo_device *pogo_data, uint8_t *reg_data)
{
	struct ivi_common_device* ivi_common_data = &pogo_data->common;

	switch (reg_data[1]) {
		case STATUS_NORMAL_MODE:
			DISP_INFO("panel is normal mode");
			ivi_send_timestamp(ivi_common_data);
			ivi_request_screeninfo(ivi_common_data);
			break;
		case STATUS_UPGRADE_MODE:
			DISP_INFO("panel is upgrade mode");
			break;
		default:
			break;
	}

	return 0;
}

static int32_t process_pogo_version_info(struct ivi_pogo_device *pogo_data, uint8_t *reg_data)
{
	struct ivi_version_info version;
	struct ivi_common_device* ivi_common_data = &pogo_data->common;

	version.hw_ver_major = reg_data[1];
	version.hw_ver_minor = reg_data[2];
	version.sw_ver_major = reg_data[3];
	version.sw_ver_minor = reg_data[4];
	version.ts_ver_major = reg_data[5];
	version.ts_ver_middle = reg_data[6];
	version.ts_ver_minor = reg_data[7];

	EXT_PANEL_INFO(ivi_common_data->base, "ivi pogo version: %02x.%02x/%02x.%02x/%02x.%02x.%02x",
		version.hw_ver_major, version.hw_ver_minor, version.sw_ver_major, version.sw_ver_minor,
		version.ts_ver_major, version.ts_ver_middle, version.ts_ver_minor);

	return 0;
}

static int32_t ivi_pogo_process_irq(struct ivi_pogo_device *pogo_data)
{
	int32_t count = IVI_STANRD_READ_FRAME_LEN;
	int32_t retry = IVI_MCU_READ_CMD_RETRY_TIMES;
	uint8_t cmd_id;
	struct ivi_read_msg read_msg;
	int32_t ret = -1;
	uint8_t std_buf[IVI_STANRD_READ_FRAME_LEN] = {};
	struct ivi_common_device* ivi_common_data = &pogo_data->common;

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
		case CMD_S2H_SCREEN_STATUS:
			process_ivi_pogo_screen_status(pogo_data, read_msg.data);
			break;
		case CMD_S2H_VERSION_INFO:
			process_pogo_version_info(pogo_data, read_msg.data);
			break;
		default:
			break;
	}
	return 0;
}

static irqreturn_t ivi_pogo_irq_handler(int irq, void *arg)
{
	struct ivi_pogo_device *data = (struct ivi_pogo_device *)arg;

	if (!data)
		DISP_ERROR("ivi pogo data is NULL !!!");
	else
		ivi_pogo_process_irq(data);
	return IRQ_HANDLED;
}

void* ivi_pogo_device_init(struct panel_device *panel_base) {
	int ret = 0;
	struct ivi_pogo_device *data = NULL;
	struct ivi_common_device *ivi_common_data = NULL;

	DISP_INFO("ivi pogo device init start");
	data = kzalloc(sizeof(struct ivi_pogo_device), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	ivi_common_data = &(data->common);
	ivi_common_device_init(ivi_common_data, panel_base);

	ret = request_threaded_irq(ivi_common_data->irq, NULL, ivi_pogo_irq_handler,
							IRQF_TRIGGER_FALLING|IRQF_ONESHOT, panel_base->device_name, data);
	if (ret) {
		DISP_ERROR("unable to request ivi pogo irq");
		goto err_free_data;
	}

	ret = panel_create_sysfs_device((void*)panel_base, data, ivi_pogo_groups);

	DISP_INFO("ivi pogo init success !!!");

	return (void*)data;

err_free_data:
	kfree(data);
err:
	DISP_INFO("ivi pogo init fail !!!");
	return NULL;
}
