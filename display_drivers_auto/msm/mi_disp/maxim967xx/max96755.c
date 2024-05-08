#include "max96755.h"

static const struct cmd_16bit pre_init_config[] = {
	//disable ETOP
	//{ IS_HOST_I2C_ADDRESS, 0x0389, 0x12, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x0332, 0x4e, 0 },
};

static const struct cmd_16bit single_link_config[] = {
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x31, 0 },
};

static const struct cmd_16bit dual_link_config[] = {
	{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x26, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2d3, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2d4, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2d5, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x20, 0 },
};

static const struct cmd_16bit dump_regs_config[] = {
	{ IS_HOST_I2C_ADDRESS, 0x02, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x13, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x100, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x308, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x311, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x332, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x333, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55d, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55e, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55f, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x56a, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x380, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x388, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x389, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3a0, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1e5, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1c8, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0d, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x03, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1DC, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2BC, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x013, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1DC, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0d, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x03, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1DC, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x3a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1b, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1c, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1d, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1e, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1f, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x20, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x21, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x22, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x23, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x26, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x29, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x100, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x50, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2c, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01, 0x0, 0 },
};

static const struct cmd_16bit test_pattern_regs[] = {
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x10A, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x112, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x3A0, 0x0, 0 },//DSI controller 0 error
	{ IS_HOST_I2C_ADDRESS, 0x3A2, 0x0, 0 },//DSI controller 1 error
	{ IS_HOST_I2C_ADDRESS, 0x55D, 0x0, 0 },//DSI HS VS DE
	{ IS_HOST_I2C_ADDRESS, 0x55E, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55F, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x56A, 0x0, 0 },
};

static const struct cmd_16bit colorbar_enable[] = {
	{ IS_HOST_I2C_ADDRESS, 0x53, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x57, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x5B, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x5F, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1C8, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CA, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CB, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CC, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CD, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CE, 0x18, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1CF, 0x60, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D0, 0x57, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D1, 0x99, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D2, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D3, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D4, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D5, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D6, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D7, 0x3C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D8, 0x0B, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1D9, 0xF4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DA, 0x07, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DB, 0x32, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DC, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DD, 0x7A, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DE, 0x58, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1DF, 0x0B, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E0, 0x40, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E1, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E2, 0xF0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E3, 0x07, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E4, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E5, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E6, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x07, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1C8, 0xE3, 0 },
};

static const struct cmd_16bit colorbar_disable[] = {
	{ IS_HOST_I2C_ADDRESS, 0x1C8, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x1E5, 0x00, 0 },
};

static int max96755_hot_plug_thread_fn(void *arg);

int max96755_check_dev_id(struct max967xx_data *data)
{
	int i;
	int ret;
	int val = 0x0;

	for (i = 0; i < 1; i++) {
		ret = max967xx_read(MAX96755_DEV_ID_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret < 0) {
			DISP_ERROR("fail to get device id, retry = %d", i);
			return -1;
		}
		if (val == MAX96755_DEV_ID) {
			DISP_INFO("device id = 0x%02x", val);
			return 0;
		}
	}

	DISP_ERROR("fail to check device id finally");
	return -1;
}

int max96755_is_locked(struct max967xx_data *data, int port)
{
	int lock_gpio_state;

	if (data->lock_gpio < 0)
		return 0;

	lock_gpio_state = gpio_get_value(data->lock_gpio);
	DISP_INFO("lock_gpio_state is %d", lock_gpio_state);

	if (lock_gpio_state) {
		max967xx_send_ctrl_info(data, SERDES_MSG_LINK_STATE, SERDES_STATE_NOT_CARE);
	} else {
		max967xx_send_ctrl_info(data, SERDES_MSG_LINK_STATE, SERDES_STATE_NO_LINK);
	}

	return 0;
}

int max96755_pre_init(struct max967xx_data *data)
{
	int ret;
	DISP_INFO("enter");
	ret = max967xx_write_regs(data, pre_init_config,
				   ARRAY_SIZE(pre_init_config),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	return ret;
}

int max96755_initialize(struct max967xx_data *data)
{
	int ret = 0;
	DISP_INFO("enter");
	data->current_work_state = SERDES_STATE_OPEN;
	INIT_LIST_HEAD(&data->msg_head);
	spin_lock_init(&data->spinlock);
	init_waitqueue_head(&data->pending_wq);
	data->hot_plug_thread = kthread_run(max96755_hot_plug_thread_fn, data, "max96755_hot_plug");
	return ret;
}

static int max96755_hot_plug_thread_fn(void *arg)
{
	struct max967xx_data *data = (struct max967xx_data *)arg;
	serdes_msg msg;
	serdes_work_state *work_state = &data->current_work_state;
	int32_t ret;

	while (!kthread_should_stop()) {
		DISP_INFO("work state = %d", *work_state);
		switch (*work_state) {
			case SERDES_STATE_CLOSE:
				ret = max967xx_recv_ctrl_info(data, &msg, 10000);
				if (ret <=0 || msg.type != SERDES_MSG_POWER_STATE)
					continue;
				*work_state = msg.state;
				break;
			case SERDES_STATE_OPEN:
				max96755_configure_remote_control(data, false);
				max96755_configure_single_link(data);
				*work_state = SERDES_STATE_SINGLE_LINK;
				break;
			case SERDES_STATE_SINGLE_LINK:
				ret = max967xx_recv_ctrl_info(data, &msg, 10000);
				if (ret <= 0)
					continue;
				if (msg.type == SERDES_MSG_LINK_STATE) {
					if (msg.state == SERDES_STATE_NO_LINK)
						continue;
					if (msg.state == SERDES_STATE_NOT_CARE) {
						max96755_configure_dual_link(data);
						*work_state = SERDES_STATE_DUAL_LINK;
					}
				} else if (msg.type == SERDES_MSG_POWER_STATE)
					*work_state = msg.state;
				break;
			case SERDES_STATE_DUAL_LINK:
				ret = max967xx_recv_ctrl_info(data, &msg, 2000);
				if (ret < 0)
					continue;

				if (ret ==0) {
					if (max96755_get_link_state(data) == DUAL_LINK)
						*work_state = SERDES_STATE_CHECK_WORK_STATE;
					else
						*work_state = SERDES_STATE_OPEN;
					DISP_ERROR("connect timeout work state = %d", *work_state);
					continue;
				}

				if (msg.type == SERDES_MSG_LINK_STATE) {
					if (msg.state == SERDES_STATE_NO_LINK) {
						continue;
					} else if (msg.state == SERDES_STATE_NOT_CARE) {
						if (max96755_get_link_state(data) == DUAL_LINK)
							*work_state = SERDES_STATE_CHECK_WORK_STATE;
					}
				} else if (msg.type == SERDES_MSG_POWER_STATE) {
					*work_state = msg.state;
				}
				break;
			case SERDES_STATE_CHECK_WORK_STATE:
				ret = max967xx_recv_ctrl_info(data, &msg, 10000);
				if (ret <=0)
					continue;
				if (msg.type == SERDES_MSG_LINK_STATE) {
					if (msg.state == SERDES_STATE_NO_LINK) {
						*work_state = SERDES_STATE_OPEN;
					}
				} else if (msg.type == SERDES_MSG_POWER_STATE) {
					*work_state = msg.state;
				}
				break;
			default:
				break;
		};
	}

	DISP_ERROR("thread abnormal !!!");
	return 0;
}

int max96755_dump_regs(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96755_dump_regs:\n");

	for (i = 0; i < ARRAY_SIZE(dump_regs_config); i++) {
		ret = max967xx_read(dump_regs_config[i].reg_addr, &val,
				    data->regmap[dump_regs_config[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   dump_regs_config[i].reg_addr);
			return -1;
		}
		DISP_INFO("dump reg 0x%04x val 0x%02x",
			  dump_regs_config[i].reg_addr, val);

		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					dump_regs_config[i].addr,dump_regs_config[i].reg_addr, val);
	}

	return 0;
}

int max96755_test_pattern(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96755_test_pattern:\n");

	for (i = 0; i < ARRAY_SIZE(test_pattern_regs); i++) {
		ret = max967xx_read(test_pattern_regs[i].reg_addr, &val,
				    data->regmap[test_pattern_regs[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   test_pattern_regs[i].reg_addr);
			return -1;
		}
		DISP_INFO("dump reg 0x%04x val 0x%02x",
			  test_pattern_regs[i].reg_addr, val);

		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					test_pattern_regs[i].addr,test_pattern_regs[i].reg_addr, val);
	}

	return 0;
}

int max96755_colorbar_enable(struct max967xx_data *data, int enable)
{
	int ret;

	if (enable)
		ret = max967xx_write_regs(data, colorbar_enable,
						ARRAY_SIZE(colorbar_enable),
						data->regmap[IS_HOST_I2C_ADDRESS]);
	else
		ret = max967xx_write_regs(data, colorbar_disable,
						ARRAY_SIZE(colorbar_disable),
						data->regmap[IS_HOST_I2C_ADDRESS]);
	return ret;
}

GMSL_LINK_STATE max96755_get_link_state(struct max967xx_data *data)
{
	int i;
	int ret;
	int val;
	int bit_mask = MAX96755_LOCK_MASK;

	for (i = 0; i < 1; i++) {
		ret = max967xx_read(MAX96755_CTRL3_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret < 0) {
			DISP_ERROR("fail to get lock status");
			return NO_LINK;
		}
		if (val & bit_mask) {
			if ((val & MAX96755_LOCK_A_MASK) && (val & MAX96755_LOCK_B_MASK)){
				DISP_ERROR("%s: is locked, link mode:spilitter\n",__func__);
				return SPLIT_LINK;
			}else if (val & MAX96755_LOCK_A_MASK){
				DISP_ERROR("%s: is locked, link mode:link A\n",__func__);
				return SINGLE_LINK_A;
			}else if (val & MAX96755_LOCK_B_MASK){
				DISP_ERROR("%s: is locked, link mode:link B\n",__func__);
				return SINGLE_LINK_B;
			} else {
				DISP_ERROR("%s: is locked, link mode:dual link\n",__func__);
				return DUAL_LINK;
			}
		}
	}
	return NO_LINK;
}

int max96755_configure_single_link(struct max967xx_data *data)
{
	int ret;
	ret = max967xx_write_regs(data, single_link_config,
				   ARRAY_SIZE(single_link_config),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	return ret;
}

int max96755_configure_dual_link(struct max967xx_data *data)
{
	int ret;
	ret = max967xx_write_regs(data, dual_link_config,
				   ARRAY_SIZE(dual_link_config),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	return ret;
}

int max96755_configure_remote_control(struct max967xx_data *data, bool enable)
{
	return max967xx_write(data, MAX96755_REG1, (enable ? 0x08 : 0x18),
		data->regmap[IS_HOST_I2C_ADDRESS]);
}

int max96755_power_ctrl(struct max967xx_data *data, int mode)
{
	DISP_INFO("set power mode %d", mode);
	if (mode == SERDES_STATE_OPEN) {
		gpio_direction_output(data->gpio_pwdn, 1);
		msleep(5);
		max96755_pre_init(data);
		enable_irq(data->lock_irq);
		enable_irq(data->errb_irq);
		max967xx_send_ctrl_info(data, SERDES_MSG_POWER_STATE, SERDES_STATE_OPEN);
	} else if (mode == SERDES_STATE_CLOSE) {
		max967xx_send_ctrl_info(data, SERDES_MSG_POWER_STATE, SERDES_STATE_CLOSE);
		disable_irq(data->lock_irq);
		disable_irq(data->errb_irq);
		gpio_direction_output(data->gpio_pwdn, 0);
	}

	return 0;
}
