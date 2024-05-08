#include "ivi_common.h"
#include "mi_ext_panel.h"
#include <linux/of_irq.h>

int ivi_msg_send(struct ivi_common_device* ivi_common, struct ivi_write_msg write_msg, size_t count)
{
	int ret;
	struct i2c_interface* i2c_inst = &ivi_common->base->i2c_instance;
	u64 interval, current_timestamp;
	mutex_lock(&ivi_common->msg_mutex);

	current_timestamp = ktime_get_ns();
	interval = (current_timestamp - ivi_common->last_send_timestamp)/1000000;
	if (interval < IVI_MCU_SEND_CMD_KEEP_INTERVAL_MS)
		msleep(IVI_MCU_SEND_CMD_KEEP_INTERVAL_MS-interval);

	write_msg.data[count-1] = panel_calculate_crc(write_msg.data, count-1);
	ret = i2c_inst->ops.i2c_raw_write(i2c_inst, write_msg.data[0], &write_msg.data[1], count-1);

	ivi_common->last_send_timestamp = ktime_get_ns();
	mutex_unlock(&ivi_common->msg_mutex);
	return ret;
}

int ivi_msg_receive(struct ivi_common_device *ivi_common, struct ivi_read_msg* read_msg, size_t count)
{
	int ret;
	uint8_t checkCRC = 0;
	struct i2c_interface* i2c_inst = &ivi_common->base->i2c_instance;

	ret = i2c_inst->ops.i2c_read(i2c_inst, read_msg->cmd_id, read_msg->data, count);
	if (ret) {
		DISP_ERROR("ivi_msg_read fail");
		return ret;
	}

#if 0
	for (num = 0; num < count; num++)
		DISP_INFO("ivi_msg_receive %x", read_msg->data[num]);
#endif

	if (read_msg->data[0] == 0xff && read_msg->data[1] == 0xff) {
		DISP_ERROR("read cmd all 0xff");
		return -1;
	}

	if ((read_msg->data[0] != 0x17) && (read_msg->data[0] != 0x18)) {
		/* last byte crc */
		checkCRC = read_msg->data[count - 1];
		if (checkCRC != panel_calculate_crc(read_msg->data, count - 1)) {
			DISP_ERROR("recv crc error");
			return -1;
		}
	}

    return 0;
}

int32_t ivi_request_screeninfo(struct ivi_common_device *ivi_common)
{
	int32_t ret = -1;
	struct ivi_write_msg write_msg;

	write_msg.data[0] = CMD_H2S_SCREEN_INFO_REQUEST;
	ret = ivi_msg_send(ivi_common, write_msg, IVI_STANRD_WRITE_FRAME_LEN);
	return ret;
}

int32_t ivi_send_timestamp(struct ivi_common_device *ivi_common)
{
	struct timespec64 tv;
	int32_t ret = -1;
	struct ivi_write_msg write_msg;

	ktime_get_real_ts64(&tv);
	write_msg.data[0] = CMD_H2S_TIMESTAMP_RESPOND;
	write_msg.data[1] = (tv.tv_sec >> 24) & 0xff;
	write_msg.data[2] = (tv.tv_sec >> 16) & 0xff;
	write_msg.data[3] = (tv.tv_sec >> 8) & 0xff;
	write_msg.data[4] = (tv.tv_sec >> 0) & 0xff;
	ret = ivi_msg_send(ivi_common, write_msg, IVI_STANRD_WRITE_FRAME_LEN);
	return ret;
}

void ivi_common_device_init(struct ivi_common_device *ivi_common_data,
															struct panel_device *panel_base)
{
	ivi_common_data->irq = irq_of_parse_and_map(panel_base->client->dev.of_node, 0);
	ivi_common_data->base = panel_base;
	ivi_common_data->last_send_timestamp = 0;
	mutex_init(&ivi_common_data->msg_mutex);
}
