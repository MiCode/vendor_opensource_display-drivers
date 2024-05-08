
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
#include <linux/of_irq.h>
#include "mi_disp_print.h"
#include "mi_ext_panel.h"
#include "hud.h"

int hud_msg_receive(struct hud_panel_device *data, struct hud_read_cmd* read_msg, size_t count)
{
	int ret;
	uint8_t checkCRC = 0, caculateCRC = 0;
	struct i2c_interface* i2c_inst = &data->base->i2c_instance;

	ret = i2c_inst->ops.i2c_read(i2c_inst, read_msg->cmd_id, read_msg->data, count);
	if (ret) {
		DISP_ERROR("hud_msg_read fail");
		return ret;
	}

	if (read_msg->data[0] == 0xff && read_msg->data[1] == 0xff) {
		DISP_ERROR("read cmd all 0xff");
		return -1;
	}

	if ((read_msg->data[0] != 0x17) && (read_msg->data[0] != 0x18)) {
		/* last byte crc */
		checkCRC = read_msg->data[count - 1];
		caculateCRC = panel_calculate_crc(read_msg->data, count - 2);

		if (checkCRC != caculateCRC) {
			DISP_ERROR("recv crc error");
			return -1;
		}
	}

    return 0;
}

int hud_msg_send(struct hud_panel_device *data, struct hud_write_cmd write_msg, size_t count)
{
	int ret;
	struct i2c_interface* i2c_inst = &data->base->i2c_instance;

	write_msg.data[count-1] = panel_calculate_crc(write_msg.data, count-2);
	ret = i2c_inst->ops.i2c_raw_write(i2c_inst, write_msg.data[0], &write_msg.data[1], count-1);
	return ret;
}

void dump_msg(uint8_t* data, int count, bool enable)
{
	int index;
	if (!enable)
		return;

	for (index=0; index < count; index++)
		DISP_INFO("data[%d] = 0x%02x", index, data[index]);
}

int32_t hud_req_dtc(struct hud_panel_device *data)
{
	int32_t count = HUD_STANRD_READ_FRAME_LEN;
	hud_read_cmd read_msg;
	int32_t ret = -1;

	memset(&read_msg, 0, sizeof(read_msg));
	read_msg.cmd_id = CMD_S2H_HUD_WORK_STATUS_FEEDBACK;

	ret = hud_msg_receive(data, &read_msg, count);
	dump_msg(read_msg.data, count, true);

	return ret;
}

void hud_send_standard_frame_init(hud_write_cmd *cmd)
{
    memset(cmd->data, 0xAA, HUD_STANRD_WRITE_FRAME_LEN);
    cmd->data[10] = 0x00;
    cmd->data[11] = 0x00;
}

int32_t hud_send_timestamp(struct hud_panel_device *data)
{
	struct timespec64 tv;
	int32_t ret = -1;
	struct hud_write_cmd write_msg;
	hud_send_standard_frame_init(&write_msg);

	ktime_get_real_ts64(&tv);
	write_msg.data[0] = CMD_H2S_HUD_TIMESTAMP;
	write_msg.data[1] = (tv.tv_sec >> 24) & 0xff;
	write_msg.data[2] = (tv.tv_sec >> 16) & 0xff;
	write_msg.data[3] = (tv.tv_sec >> 8) & 0xff;
	write_msg.data[4] = (tv.tv_sec >> 0) & 0xff;
	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);
	return ret;
}

int32_t hud_set_backlight_power(struct hud_panel_device *data, hud_backlight_mode mode)
{

	int32_t ret = -1;
	struct hud_write_cmd write_msg;
	hud_send_standard_frame_init(&write_msg);

	write_msg.data[0] = CMD_H2S_HUD_PGU_BACKLIGHT_ENABLE;
	write_msg.data[1] = (uint8_t)mode;
	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);
	DISP_INFO("hud set backlight power");
	return ret;
}

int32_t hud_set_switch_status(struct hud_panel_device *data, hud_switch_state state)
{
	int32_t ret = -1;
	struct hud_write_cmd write_msg;
	hud_send_standard_frame_init(&write_msg);

	write_msg.data[0] = CMD_H2S_HUD_SW_STATUS;
	write_msg.data[1] = (uint8_t)state;
	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);
	DISP_INFO("hud set switch status");
    return ret;
}

void hud_process_screen_status(struct hud_panel_device *data, uint8_t *buf)
{
	dump_msg(buf, HUD_STANRD_READ_FRAME_LEN, data->msg_log_enable);

	hud_send_timestamp(data);
	if (buf[12] == 1 && (data->backlight_enable == false ||
			buf[11] == 0 || buf[3] == 0)) {
		if (buf[11] == 0)
			hud_set_backlight_power(data, HUD_BACKLIGHT_MODE_ON);
		if (buf[3] == 0)
			hud_set_switch_status(data, HUD_SWITCH_STATUS_OPEN);
		hud_req_dtc(data);
		if (data->backlight_enable == false) {
			hrtimer_cancel(&data->safety_timer);
			hrtimer_start(&data->safety_timer, ms_to_ktime(100), HRTIMER_MODE_REL);
			data->backlight_enable = true;
			DISP_INFO("safety timer start running");
		}
	}
}

void hud_process_startup_done(struct hud_panel_device *data, uint8_t *buf)
{
	dump_msg(buf, HUD_STANRD_READ_FRAME_LEN, data->msg_log_enable);
	data->backlight_enable = false;
	if (buf[1] == 1) {
		hud_set_backlight_power(data, HUD_BACKLIGHT_MODE_ON);
		hud_set_switch_status(data, HUD_SWITCH_STATUS_OPEN);
		hud_req_dtc(data);
		if (!data->backlight_enable) {
			hrtimer_cancel(&data->safety_timer);
			hrtimer_start(&data->safety_timer, ms_to_ktime(100), HRTIMER_MODE_REL);
			data->backlight_enable = true;
			DISP_INFO("safety timer start running");
		}
	}
	DISP_INFO("hud startup done");
}

void hud_process_work_status(struct hud_panel_device *data, uint8_t *buf)
{
	dump_msg(buf, HUD_STANRD_READ_FRAME_LEN, true);
}

int32_t hud_set_luminance(struct hud_panel_device *data, int32_t luminance)
{
	int32_t ret = -1;
	struct hud_write_cmd write_msg;

	if (luminance < 0 || luminance > 0x0A) {
		return -1;
	}

	hud_send_standard_frame_init(&write_msg);

	write_msg.data[0] = CMD_H2S_HUD_SW_LUMINANCE;
	write_msg.data[1] = (uint8_t)luminance;
	DISP_INFO("luminance is %d", luminance);
	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);
	return ret;
}


static uint8_t hud_get_e2e_value(struct hud_panel_device* data, hud_e2e_type type)
{
	uint8_t value = 0;
	hud_safety_info_t* safe_info = &data->current_safe_info;

	switch (type) {
		case E2E_SENSOR_LIGHT_VALUE:
			value = safe_info->e2e_value.sensor_light_value;
			safe_info->e2e_value.sensor_light_value = (value == 0xe) ? 0 : (value+1);
			break;
		case E2E_SENSOR_LIGHT_STATUS:
			value = safe_info->e2e_value.sensor_light_status;
			safe_info->e2e_value.sensor_light_status = (value == 0xe) ? 0 : (value+1);
			break;
		case E2E_VEHICLE_SPEED:
			value = safe_info->e2e_value.vehicle_speed;
			safe_info->e2e_value.vehicle_speed = (value == 0xe) ? 0 : (value+1);
			break;
		default:
			DISP_ERROR("not support E2E type %d", type);
			break;
	}
	return value;
}

int32_t hud_send_rls_sensor_light_value(struct hud_panel_device* data,
    const rls_sensor_light_value *value)
{
	hud_write_cmd write_msg;
	int32_t ret = -1;

	hud_send_standard_frame_init(&write_msg);
	write_msg.data[0] = CMD_H2S_HUD_RLS_SENSOR_VAL;
	write_msg.data[1] = (value->RlsHudSnsrVal >> 8) & 0xff;
	write_msg.data[2] = value->RlsHudSnsrVal & 0xff;
	write_msg.data[3] = (value->RlsFwdLiSnsrVal >> 8) & 0xff;
	write_msg.data[4] = value->RlsFwdLiSnsrVal & 0xff;
	write_msg.data[5] = (value->RlsTopLiSnsrVal >> 8) & 0xff;
	write_msg.data[6] = value->RlsTopLiSnsrVal & 0xff;
	write_msg.data[7] = value->RlsIRVal;
	write_msg.data[8] = value->RlsOutdBriSts;
	/* ROLLING_COUNTER */
	write_msg.data[11] = hud_get_e2e_value(data, E2E_SENSOR_LIGHT_VALUE);

	dump_msg(write_msg.data, HUD_STANRD_READ_FRAME_LEN, data->msg_log_enable);

	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);

	return ret;
}

int32_t hud_send_rls_sensor_light_status(struct hud_panel_device* data,
														const rls_sensor_light_status *status)
{
	hud_write_cmd write_msg;
	int32_t ret = -1;

	hud_send_standard_frame_init(&write_msg);
	write_msg.data[0] = CMD_H2S_HUD_RLS_SENSOR_STATUS;
	write_msg.data[1] = status->RlsSts;
	write_msg.data[2] = status->RlsLiSnsrIRFlt;
	write_msg.data[3] = status->RlsLiSnsrTopFlt;
	write_msg.data[4] = status->RlsLiSnsrFwdFlt;
	/* ROLLING_COUNTER */
	write_msg.data[11] = hud_get_e2e_value(data, E2E_SENSOR_LIGHT_STATUS);

	dump_msg(write_msg.data, HUD_STANRD_READ_FRAME_LEN, data->msg_log_enable);

	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);

	return ret;
}

int32_t hud_send_vehicle_speed(struct hud_panel_device* data, vehicle_speed_value *value)
{
	hud_write_cmd write_msg;
	int32_t ret = -1;

	hud_send_standard_frame_init(&write_msg);
	write_msg.data[0] = (uint8_t)CMD_H2S_HUD_VEHSPD;
	write_msg.data[1] = (value->vehicle_speed >> 8) & 0xff;
	write_msg.data[2] = value->vehicle_speed & 0xff;
	write_msg.data[3] = value->vehicle_speed_valid;
	/* ROLLING_COUNTER */
	write_msg.data[11] = hud_get_e2e_value(data, E2E_VEHICLE_SPEED);

	dump_msg(write_msg.data, HUD_STANRD_READ_FRAME_LEN, data->msg_log_enable);

	ret = hud_msg_send(data, write_msg, HUD_STANRD_WRITE_FRAME_LEN);

    return ret;
}

int32_t hud_send_safety_msg(struct hud_panel_device* data, rls_sensor_light_value *val,
    								rls_sensor_light_status *status, vehicle_speed_value *speed) {
	int32_t ret = 0;

	ret = hud_send_rls_sensor_light_value(data, val);
	if (ret) {
		DISP_ERROR("send rls value to hud failed! ret is %d", ret);
		goto end;
	}
	ret = hud_send_rls_sensor_light_status(data, status);
	if (ret) {
		DISP_ERROR("send rls status to hud failed! ret is %d", ret);
		goto end;
	}
	ret = hud_send_vehicle_speed(data, speed);
	if (ret) {
	  DISP_ERROR("send rls value to hud failed! ret is %d", ret);
	  goto end;
	}
end:
	if (ret) {
		hrtimer_cancel(&data->safety_timer);
		data->backlight_enable = false;
	}
	return ret;
}

static void hud_safety_work(struct work_struct *work)
{
	struct hud_panel_device *data =
		container_of(work, struct hud_panel_device, safety_work);
	hud_safety_info_t* safe_info = &data->current_safe_info;
	hud_send_safety_msg(data, &safe_info->val, &safe_info->status, &safe_info->speed);
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

static ssize_t msg_rw_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hud_panel_device* data = dev_get_drvdata(device);
	uint8_t cmd = 0;
	int value = 0;

	sscanf(buf, "0x%x %d", &cmd, &value);
	if(!data)
		DISP_ERROR("input data invalid");

	switch (cmd) {
		case CMD_H2S_HUD_PGU_BACKLIGHT_ENABLE:
			hud_set_backlight_power(data, (hud_backlight_mode)value);
			break;
		case CMD_H2S_HUD_SW_STATUS:
			hud_set_switch_status(data, (hud_switch_state)value);
			break;
		case CMD_S2H_HUD_WORK_STATUS_FEEDBACK:
			hud_req_dtc(data);
			break;
		default:
			break;
	}

	return count;
}
static DEVICE_ATTR_RO(irq);

static ssize_t msg_rw_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct hud_panel_device* data = dev_get_drvdata(device);
	if(!data)
		DISP_ERROR("hud data is null");
	return 0;
}
static DEVICE_ATTR_RW(msg_rw);

static ssize_t debug_log_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hud_panel_device* data = dev_get_drvdata(device);
	int log_enable = 0;

	if(!data)
		DISP_ERROR("input data invalid");

	sscanf(buf, "%d", &log_enable);

	if (log_enable)
		data->msg_log_enable = true;
	else
		data->msg_log_enable = false;	

	return count;
}
static DEVICE_ATTR_WO(debug_log);

static ssize_t safety_enable_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hud_panel_device* data = dev_get_drvdata(device);
	int safety_send = 0;

	if(!data)
		DISP_ERROR("input data invalid");

	sscanf(buf, "%d", &safety_send);

	if (safety_send) {
		hrtimer_cancel(&data->safety_timer);
		hrtimer_start(&data->safety_timer, ms_to_ktime(100), HRTIMER_MODE_REL);
	} else
		hrtimer_cancel(&data->safety_timer);

	return count;
}
static DEVICE_ATTR_WO(safety_enable);


static struct attribute *hud_attrs[] = {
	&dev_attr_irq.attr,
	&dev_attr_msg_rw.attr,
	&dev_attr_debug_log.attr,
	&dev_attr_safety_enable.attr,
	NULL
};

static const struct attribute_group hud_group = {
	.attrs = hud_attrs,
};

static const struct attribute_group *hud_groups[] = {
	&hud_group,
	NULL
};

static int32_t hud_process_irq(struct hud_panel_device *data, int irq_index)
{
	int32_t count = HUD_STANRD_READ_FRAME_LEN;
	hud_read_cmd read_msg;
	uint8_t cmd_id;
	int32_t ret = -1;

	memset(&read_msg, 0, sizeof(read_msg));
	read_msg.cmd_id = 0xfe;

	ret = hud_msg_receive(data, &read_msg, count);
	if (ret) {
		return -EINTR;
	}

	cmd_id = read_msg.data[0];
	EXT_PANEL_INFO(data->base, "cmd_id = 0x%x", cmd_id);

	switch (cmd_id) {
		case CMD_S2H_HUD_STARTUP_DONE:
			hud_process_startup_done(data, read_msg.data);
			break;
		case CMD_S2H_HUD_CURRENT_SCREEN_STATUS:
			hud_process_screen_status(data, read_msg.data);
			break;
		case CMD_S2H_HUD_WORK_STATUS_FEEDBACK:
			hud_process_work_status(data, read_msg.data);
			break;
	}

    return 0;
}

static irqreturn_t hud_irq_handler(int irq, void *arg)
{
	struct hud_panel_device *data = (struct hud_panel_device *)arg;

	if (data)
		hud_process_irq(data, irq);

	return IRQ_HANDLED;
}

#if 0
static irqreturn_t hud_i2c_irq1_handler(int irq, void *arg)
{
	struct hud_i2c_data *data = (struct hud_i2c_data *)arg;

	if (data)
		hud_process_irq(data, 1);

	return IRQ_HANDLED;
}
#endif

static enum hrtimer_restart hud_safety_timer_handler(struct hrtimer *timer)
{
	struct hud_panel_device *data =
						container_of(timer, struct hud_panel_device, safety_timer);
	if (!data)
		DISP_ERROR("hud private data is null");

	schedule_work(&data->safety_work);
	hrtimer_forward_now(timer, ms_to_ktime(100));

	return HRTIMER_RESTART;
}


void init_safety_info(hud_safety_info_t* safe_info)
{
	memset(safe_info, 0, sizeof(hud_safety_info_t));

    safe_info->val.RlsHudSnsrVal = 0;
    safe_info->val.RlsFwdLiSnsrVal = HUD_RLS_DEFAULT_VAL;
    safe_info->val.RlsTopLiSnsrVal = HUD_RLS_DEFAULT_VAL;
    safe_info->val.RlsIRVal = 0;
    safe_info->val.RlsOutdBriSts = 0;

    safe_info->status.RlsSts = 0x0;
    safe_info->status.RlsLiSnsrIRFlt = 0x0;
    safe_info->status.RlsLiSnsrTopFlt = 0x0;
    safe_info->status.RlsLiSnsrFwdFlt = 0x0;

    safe_info->speed.vehicle_speed = 0;
    safe_info->speed.vehicle_speed_valid = 0;
}

int32_t hud_set_power_mode(panel_device *device, int power_mode) {
	DISP_INFO("hud set power mode %d", power_mode);
	return 0;
}

static struct panel_function panel_function_ops = {
	.set_power_mode = hud_set_power_mode,
};

void* hud_panel_device_init(struct panel_device *panel_base) {
	int ret = 0;
	struct hud_panel_device *data = NULL;

	DISP_INFO("hud panel device init start");
	data = kzalloc(sizeof(struct hud_panel_device), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}

	data->base = panel_base;

	data->irq = irq_of_parse_and_map(panel_base->client->dev.of_node, 0);

	ret = request_threaded_irq(data->irq, NULL, hud_irq_handler,
				   IRQF_TRIGGER_FALLING|IRQF_ONESHOT, panel_base->device_name, data);
	if (ret) {
		DISP_ERROR("unable to request hud irq");
		goto err_free_data;
	}

	data->irq_gpio = of_get_named_gpio(panel_base->client->dev.of_node, "irq-gpio", 0);
	if ((!gpio_is_valid(data->irq_gpio))) {
		data->irq_gpio = -EINVAL;
		DISP_INFO("irq-gpio is invalid");
	}

	INIT_WORK(&data->safety_work, hud_safety_work);
	hrtimer_init(&data->safety_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->safety_timer.function = hud_safety_timer_handler;
	init_safety_info(&data->current_safe_info);

	panel_base->function = panel_function_ops;
	ret = panel_create_sysfs_device((void*)panel_base, data, hud_groups);
	data->backlight_enable = false;

	DISP_INFO("hud probe success !!!");
	return (void*)data;
err_free_data:
	kfree(data);
err:
	DISP_INFO("hud probe fail !!!");
	return NULL;
}
