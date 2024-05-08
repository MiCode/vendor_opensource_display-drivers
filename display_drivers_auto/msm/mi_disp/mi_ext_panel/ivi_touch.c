#include "ivi_touch.h"
#include "mi_ext_panel.h"
#include "mi_disp_print.h"

#define IVI_TS_DRV_NAME "ivi_touch"
static volatile long touch_id = 0x0;
static bool work_pending = false;

void report_touch_down(struct input_dev *dev, struct touch_pointer *pointer)
{
	input_report_key(dev, BTN_TOUCH, 1);
	input_mt_slot(dev, pointer->slot);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
	input_report_abs(dev, ABS_MT_POSITION_X, pointer->position_x);
	input_report_abs(dev, ABS_MT_POSITION_Y, pointer->position_y);
	__set_bit(pointer->slot, &touch_id);
	input_sync(dev);
}

void report_finger_move(struct input_dev *dev, struct touch_pointer *pointer)
{
	input_report_key(dev, BTN_TOUCH, 1);
	input_mt_slot(dev, pointer->slot);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
	input_report_abs(dev, ABS_MT_POSITION_X, pointer->position_x);
	input_report_abs(dev, ABS_MT_POSITION_Y, pointer->position_y);
	__set_bit(pointer->slot, &touch_id);
	input_sync(dev);
}

void report_finger_up(struct input_dev *dev, struct touch_pointer *pointer)
{
	input_report_abs(dev, ABS_MT_SLOT, pointer->slot);
	input_report_abs(dev, ABS_MT_TRACKING_ID, pointer->tracking_id);
	__clear_bit(pointer->slot, &touch_id);
	input_sync(dev);
}

void report_touch_up(struct input_dev *dev)
{
	input_report_key(dev, BTN_TOUCH, 0);
	input_sync(dev);
}

void release_all(struct input_dev *dev)
{
	int i;
	for (i = 0; i < TOUCH_ID_MAX; i++) {
		input_report_abs(dev, ABS_MT_SLOT, i);
		input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
	}
	input_report_key(dev, BTN_TOUCH, 0);
	input_sync(dev);
}

void touch_delay_work_fun(struct work_struct *work)
{
	struct delayed_work *delay_work = container_of(work, struct delayed_work , work);
	struct ivi_touch *touch_info =  container_of(delay_work, struct ivi_touch , touch_delay_work);

	mutex_lock(&touch_info->touch_lock);
	release_all(touch_info->touch_input);
	touch_id = 0;
	mutex_unlock(&touch_info->touch_lock);
}

int process_touch_info(struct ivi_touch *touch_info, uint8_t *reg_data)
{
	int32_t i;
	int32_t pointer_status = 0;
	int32_t pointer_id = 0;
	int32_t touch_num = 0;
	int32_t read_count = 0;
	uint8_t ext_buf[IVI_EXT_READ_FRAME_LEN] = { 0 };
	int ret = 0;
	struct ivi_read_msg read_msg;
	struct ivi_panel_device* ivi_data = (struct ivi_panel_device*)(touch_info->ivi_data);

	if (work_pending) {
		cancel_delayed_work_sync(&touch_info->touch_delay_work);
		work_pending = false;
	}

	mutex_lock(&touch_info->touch_lock);
	touch_num = (reg_data[1] > TOUCH_ID_MAX) ? TOUCH_ID_MAX : reg_data[1];
	read_count = reg_data[IVI_STANRD_FRAME_EXT_LENGTH_INDEX];
	EXT_PANEL_INFO(ivi_data->common.base, "read touch count = %d num = %d", read_count, touch_num);

	if (touch_num > 2 && read_count > 0) {
		memset(&read_msg, 0, sizeof(read_msg));
		read_msg.cmd_id = 0xfe;
		read_msg.num = IVI_EXT_READ_FRAME_LEN;
		read_msg.data = ext_buf;
		ret = ivi_msg_receive(&ivi_data->common, &read_msg, read_count);
		if (ret) {
			DISP_ERROR("read touch failed");
			return -1;
		}
	}

	for (i = 0; i < touch_num; i++) {
		uint8_t *start = NULL;

		if (i == 0 || i == 1)
			start = &reg_data[2 + i * 6];
		else
			start = &ext_buf[1 + (i - 2) * 6];

		pointer_id = start[0];

		if (pointer_id < 1 || pointer_id > 10) {
			DISP_ERROR("touch pointer_id error= %d", pointer_id);
			continue;
		}

		pointer_status = start[1];
		if (pointer_status != POINTER_PRESSS && pointer_status != POINTER_MOVING &&
			pointer_status != POINTER_RELEASE) {
			DISP_ERROR("touch pointer_status error=0x%x", pointer_status);
			continue;
		}

		touch_info->pointer_arry[pointer_id - 1].position_x = (start[2] << 8) | start[3];
		touch_info->pointer_arry[pointer_id - 1].position_y = (start[4] << 8) | start[5];
		touch_info->pointer_arry[pointer_id - 1].slot = pointer_id - 1;
		touch_info->pointer_arry[pointer_id - 1].pointer_status = pointer_status;

		if (pointer_status == POINTER_PRESSS || pointer_status == POINTER_MOVING)
			report_touch_down(touch_info->touch_input, &touch_info->pointer_arry[pointer_id - 1]);
		else if (pointer_status == POINTER_RELEASE)
			report_finger_up(touch_info->touch_input, &touch_info->pointer_arry[pointer_id - 1]);
	}

	mutex_unlock(&touch_info->touch_lock);

	if (touch_id == 0) {
		if (work_pending) {
			cancel_delayed_work_sync(&touch_info->touch_delay_work);
			work_pending = false;
		}
		report_touch_up(touch_info->touch_input);
	} else if (touch_id) {
		queue_delayed_work(system_wq, &touch_info->touch_delay_work, msecs_to_jiffies(2000));
		work_pending = true;
	}

	return 0;
}

int ivi_touch_init(struct ivi_touch *touch_info)
{
	struct device *dev = NULL;
	struct input_dev *touch_input_dev = NULL;
	int ret = 0, i = 0;
	struct ivi_panel_device* ivi_data = NULL;

	ivi_data = container_of(touch_info, struct ivi_panel_device, touch_data);

	DISP_INFO("%s enter", __func__);

	dev = &ivi_data->common.base->client->dev;
	touch_input_dev = input_allocate_device();

	if (!touch_input_dev)
	{
		DISP_ERROR("input allocate fail\n");
		return -ENOMEM;
	}

	touch_info->ivi_data = ivi_data;
	touch_info->touch_input = touch_input_dev;
	touch_input_dev->dev.parent = dev;
	touch_input_dev->name = IVI_TS_DRV_NAME;
	touch_input_dev->id.bustype = BUS_I2C;
	touch_input_dev->id.vendor = 0x0001;
	touch_input_dev->id.product = 0x0002;
	touch_input_dev->id.version = 0x0100;
	input_set_drvdata(touch_input_dev, touch_info);

	input_set_capability(touch_input_dev, EV_KEY, BTN_TOUCH);
	input_set_capability(touch_input_dev, EV_KEY, BTN_TOOL_FINGER);

	input_set_capability(touch_input_dev, EV_ABS, ABS_X);
	input_set_capability(touch_input_dev, EV_ABS, ABS_Y);

	input_mt_init_slots(touch_input_dev, 5, INPUT_MT_DIRECT);

	input_set_abs_params(touch_input_dev, ABS_MT_POSITION_X, 0, 2880 - 1, 0, 0);
	input_set_abs_params(touch_input_dev, ABS_MT_POSITION_Y, 0, 1800 - 1, 0, 0);
	input_set_abs_params(touch_input_dev, ABS_MT_TOUCH_MAJOR, AREA_MIN, 2880, 0, 0);
	input_set_abs_params(touch_input_dev, ABS_MT_TOUCH_MINOR, AREA_MIN, 1800, 0, 0);
	input_set_abs_params(touch_input_dev, ABS_MT_WIDTH_MINOR, AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(touch_input_dev, ABS_MT_WIDTH_MAJOR, AREA_MIN, AREA_MAX, 0, 0);

	ret = input_register_device(touch_input_dev);
	if (ret)
	{
		DISP_ERROR("input_register_device fail\n");
		return ret;
	}

	for (i = 0; i < TOUCH_ID_MAX; i++)
	{
		touch_info->pointer_arry[i].slot = 0;
		touch_info->pointer_arry[i].tracking_id = -1;
	}

	INIT_DELAYED_WORK(&touch_info->touch_delay_work, touch_delay_work_fun);
	mutex_init(&touch_info->touch_lock);

	return 0;
}
int ivi_touch_remove(struct ivi_touch *data)
{
	DISP_INFO("%s enter", __func__);

	return 0;
}
