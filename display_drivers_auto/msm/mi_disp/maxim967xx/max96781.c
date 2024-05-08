#include "max96781.h"
#define DP_HUD_CLUSTER
//#define DP_IVI
#ifdef DP_IVI
static const struct cmd_16bit dual_link_cfg[] = {
	/* Reset GMSL link */
	{ IS_HOST_I2C_ADDRESS, 0x0029, 0x01, 10 },
	/* Dual link */
	{ IS_HOST_I2C_ADDRESS, 0x0010, 0x00, 10 },
	/* Release GMSL link */
	{ IS_HOST_I2C_ADDRESS, 0x0029, 0x00, 10 },
};

static const struct cmd_16bit disable_remote_cfg[] = {
	{ IS_HOST_I2C_ADDRESS, 0x0076, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x00, 0 },
};

static const struct cmd_16bit serializer_init_configs[] = {
	/* Ensure the VID enabled */
	{ IS_HOST_I2C_ADDRESS, 0x0100, 0x61, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x00, 0 },
	/* Disable LINK_ENABLE */
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x00, 100000 },
	/* config hpd source */
	{ IS_HOST_I2C_ADDRESS, 0x702d, 0x7e, 0 },
	/* Disable reporting MST Capability */
	{ IS_HOST_I2C_ADDRESS, 0x7019, 0x00, 0 },
	/* Set AUX_RD_INTERVAL to 16ms */
	{ IS_HOST_I2C_ADDRESS, 0x70a0, 0x04, 0 },
	/* Set MAX_LINK_RATE to 5.4Gb/s */
	{ IS_HOST_I2C_ADDRESS, 0x7074, 0x1e, 0 },
	/* Set MAX_LINK_COUNT to 2 */
	{ IS_HOST_I2C_ADDRESS, 0x7070, 0x02, 0 },
	/* Enable LINK_ENABLE */
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x01, 0 },

	/* GPIO mapping TP INT */
	{ IS_HOST_I2C_ADDRESS, 0x0270, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x0271, 0x26, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x0272, 0x46, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x0273, 0x22, 0 },

	/* muc2 INT */
	{ IS_HOST_I2C_ADDRESS, 0x0208, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x0209, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x020a, 0x44, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x020b, 0x22, 0 },

	/* Enable I2C1 */
	{ IS_HOST_I2C_ADDRESS, 0x0079, 0x01, 0 },

	/* Enable remote contro */
	{ IS_HOST_I2C_ADDRESS, 0x0076, 0x18, 10 },
};
#endif
#ifdef DP_HUD_CLUSTER
static const struct cmd_16bit dual_link_splitting_cfg[] = {
	/* Reset GMSL link */
	{ IS_HOST_I2C_ADDRESS, 0x0029, 0x01, 10 },
	/* Dual link */
	{ IS_HOST_I2C_ADDRESS, 0x0010, 0x03, 10 },
	/* Release GMSL link */
	{ IS_HOST_I2C_ADDRESS, 0x0029, 0x02, 10 },
};

static const struct cmd_16bit disable_remote_cfg[] = {
	//{ IS_HOST_I2C_ADDRESS, 0x0076, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x00, 0 },
};

//use this to config cluster deserializer
static const struct cmd_16bit deserializer_A_init_configs[] = {
	/* GPIO03 BL_ON */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0209, 0x00, 0 },
	/* GPIO10 LCD_STBY_EN */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x021E, 0x00, 0 },
	/* GPIO06 LCD_RST */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0212, 0x00, 50000 },
	/* GPIO05 LCD3V3 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x020F, 0x00, 50000 },
	/* MAX96752F link margin improvement set */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0242, 0x10, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0458, 0x28, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0459, 0x68, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x043E, 0xB3, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x043F, 0x72, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0558, 0x28, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0559, 0x68, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x053E, 0xB3, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x053F, 0x72, 100 },
	/* Disable max 96755 and max96752 audio channel */
	/* Turn off video pipe */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x43, 0 },
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0140, 0x20, 0 },

	/* MAX96781 GPIO18 MAPPING TO MAX96752 GPIO10 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0215, 0x05, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0216, 0x32, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0217, 0x12, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0218, 0x04, 0 },

	/* Turn off video pipe */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x03, 1000 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01ce, 0x46, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01ea, 0x05, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0449, 0x75, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0549, 0x75, 0 },
	//TODO change stream id
	/* set stream id */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0050, 0x01, 0 },

	/* GPIO04 BISTEN */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x020C, 0x10, 0 },
	/* GPIO07 LCD_FAIL_T */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0215, 0x10, 0 },
	/* GPIO05 LCD3V3 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x020F, 0x10, 5 * 1000 },
	/* GPIO06 LCD_RST */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0212, 0x10, 40 * 1000 },
	/* GPIO10 LCD_STBY_EN */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x021E, 0x10, 0 },
	/* turn on video */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x43, 5 },
	/* GPIO03 BL_ON */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0209, 0x10, 100 },
	/* GPIO9 BL_PWM */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0206, 0x10, 0 },
};

static const struct cmd_16bit serializer_init_configs[] = {
    /* enable I2C remote control */
    { IS_HOST_I2C_ADDRESS,  0x0076, 0x18, 0 },
    /* Video transmit port enabled */
    { IS_HOST_I2C_ADDRESS,  0x0100, 0x61, 0 },

    /* Set pclks to run continuously */
    { IS_HOST_I2C_ADDRESS,  0x6421, 0x0F, 0 },
    /* Enable MST mode on GM03 - DEBUG */
    { IS_HOST_I2C_ADDRESS,  0x7F11, 0x05, 0 },

    /* Set video payload ID 1 for video output port 1 on GM03 */
    { IS_HOST_I2C_ADDRESS,  0x7904, 0x02, 0 },
    /* Set video payload ID 2 for video output port 1 on GM03 */
    { IS_HOST_I2C_ADDRESS,  0x7908, 0x01, 0 },

    /* Turn off video */
    { IS_HOST_I2C_ADDRESS,  0x6420, 0x10, 0 },
    /* Disable MST_VS0_DTG_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7A14, 0x00, 0 },
    /* Disable MST_VS1_DTG_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7B14, 0x00, 0 },
    /* Disable LINK_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7000, 0x00, 0 },
    /* Reset DPRX core (VIDEO_INPUT_RESET) */
    { IS_HOST_I2C_ADDRESS,  0x7054, 0x01, 100 },

    /* Set MAX_LINK_RATE to 5.4Gb/s */
    { IS_HOST_I2C_ADDRESS,  0x7074, 0x14, 0 },
    /* Set MAX_LINK_COUNT to 2 */
    { IS_HOST_I2C_ADDRESS,  0x7070, 0x02, 100 },

    /* A Stream ID */
    { IS_HOST_I2C_ADDRESS,  0x00A3, 0x20, 0 },
    /* B Stream ID */
    { IS_HOST_I2C_ADDRESS,  0x00A7, 0x11, 0 },
    /* Enable LINK_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7000, 0x01, 1000 },

    /* Disable MSA reset */
    { IS_HOST_I2C_ADDRESS,  0x7A18, 0x05, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7B18, 0x05, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7C18, 0x05, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7D18, 0x05, 0 },
    /* Adjust VS0_DMA_HSYNC */
    { IS_HOST_I2C_ADDRESS,  0x7A28, 0xFF, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7A2A, 0xFF, 0 },
    /* Adjust VS0_DMA_VSYNC */
    { IS_HOST_I2C_ADDRESS,  0x7A24, 0xFF, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7A27, 0x0F, 0 },
    /* Adjust VS1_DMA_HSYNC */
    { IS_HOST_I2C_ADDRESS,  0x7B28, 0xFF, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7B2A, 0xFF, 0 },
    /* Adjust VS1_DMA_VSYNC */
    { IS_HOST_I2C_ADDRESS,  0x7B24, 0xFF, 0 },
    { IS_HOST_I2C_ADDRESS,  0x7B27, 0x0F, 0 },
    /* Enable MST_VS0_DTG_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7A14, 0x01, 0 },
    /* Enable MST_VS1_DTG_ENABLE */
    { IS_HOST_I2C_ADDRESS,  0x7B14, 0x01, 0 },
	/* Enable VS0_Enable */
    { IS_HOST_I2C_ADDRESS,  0x7A00, 0x01, 0 },
    /* Enable VS1_Enable */
    { IS_HOST_I2C_ADDRESS,  0x7B00, 0x01, 0 },
    /* Turn on video */
    { IS_HOST_I2C_ADDRESS,  0x6420, 0x13, 0 },
    /* Turn off video */
    { IS_HOST_I2C_ADDRESS,  0x6420, 0x10, 0 },
    /* Turn on video */
    { IS_HOST_I2C_ADDRESS,  0x6420, 0x13, 0 },
    /* Link config, 3G */
    { IS_HOST_I2C_ADDRESS,  0x0028, 0x88, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0032, 0x80, 0 },

    { IS_HOST_I2C_ADDRESS,  0x0270, 0x08, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0271, 0x24, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0272, 0x44, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0273, 0x22, 0 },

    { IS_HOST_I2C_ADDRESS,  0x0290, 0x08, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0291, 0x28, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0292, 0x48, 0 },
    { IS_HOST_I2C_ADDRESS,  0x0293, 0x22, 0 },

    /* MAX96781 link A one short reset */
    //{ IS_HOST_I2C_ADDRESS, 0x0010, 0x03, 0 },
    //{ IS_HOST_I2C_ADDRESS, 0x0029, 0x02,  200 },
};
#endif

static const struct cmd_16bit dump_regs_config[] = {
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x21, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x7000, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x112, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x13, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x6444, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x6445, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x641c, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x641d, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x641e, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x641f, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x6446, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x03, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x7934, 0x0, 0 },
	{ IS_DEVICE_I2C_B_ADDRESS, 0x03, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x50, 0x0, 0 },
	{ IS_DEVICE_I2C_B_ADDRESS, 0x50, 0x0, 0 },
};

static const struct cmd_16bit test_pattern_regs[] = {
	{ IS_HOST_I2C_ADDRESS, 0x6418, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x6419, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x420, 0x0, 0 }, //Video input PCLK detect
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 }, //PCLK detect
	{ IS_HOST_I2C_ADDRESS, 0x641A, 0x0, 0 },//training status
	{ IS_HOST_I2C_ADDRESS, 0x743C, 0x0, 0 },//DPCD 202
	{ IS_HOST_I2C_ADDRESS, 0x7440, 0x0, 0 },//DPCD 203
	{ IS_HOST_I2C_ADDRESS, 0x7500, 0x0, 0 },//MSA timing H resolution L bit
	{ IS_HOST_I2C_ADDRESS, 0x7501, 0x0, 0 },//MSA timing H resolution Hbit
	{ IS_HOST_I2C_ADDRESS, 0x7514, 0x0, 0 },//MSA timing V resolution L bit
	{ IS_HOST_I2C_ADDRESS, 0x7515, 0x0, 0 },//MSA timing V resolution Hbit
};

int max96781_check_dev_id(struct max967xx_data *data)
{
	int i;
	int ret;
	int val = 0x0;

	for (i = 0; i < 20; i++) {
		ret = max967xx_read(MAX96781_DEV_ID_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret <= 0) {
			DISP_ERROR("fail to get device id, retry = %d", i);
		}
		if (val == MAX96781_DEV_ID) {
			DISP_INFO("device id = 0x%02x", val);
			return 0;
		}
		usleep_range(500, 1000);
	}

	DISP_ERROR("fail to check device id finally");
	return -1;
}

int max96781_is_locked(struct max967xx_data *data, int port)
{
	int i;
	int ret;
	int val;
	int bit_mask = 0x0;
	bit_mask = MAX96781_LOCK_MASK;

	for (i = 0; i < 1; i++) {
		ret = max967xx_read(MAX96781_LOCK_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret < 0) {
			DISP_ERROR("fail to get lock status");
		}

		if (val & bit_mask) {
			pr_err("%s: is locked\n", __func__);
			return 1;
		}
		//TODO
		usleep_range(500, 1000);
	}

	return -1;
}

int max96781_pre_init(struct max967xx_data *data)
{
	DISP_ERROR("enter");
	#ifdef DP_IVI
	return max967xx_write_regs(data, disable_remote_cfg,
			    ARRAY_SIZE(disable_remote_cfg),
			    data->regmap[IS_HOST_I2C_ADDRESS]);
	#elif defined (DP_HUD_CLUSTER)
		max967xx_write_regs(data, disable_remote_cfg,
			    ARRAY_SIZE(disable_remote_cfg),
			    data->regmap[IS_HOST_I2C_ADDRESS]);
	return max967xx_write_regs(data, dual_link_splitting_cfg,
				   ARRAY_SIZE(dual_link_splitting_cfg),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	#else
	return 0;
	#endif
}

int max96781_initialize(struct max967xx_data *data)
{
	DISP_ERROR("enter");
	#ifdef DP_IVI
	max967xx_write_regs(data, dual_link_cfg,
				   ARRAY_SIZE(dual_link_cfg),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	return max967xx_write_regs(data, serializer_init_configs,
				   ARRAY_SIZE(serializer_init_configs),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
	#elif defined (DP_HUD_CLUSTER)
	max967xx_write_regs(data, serializer_init_configs,
				   ARRAY_SIZE(serializer_init_configs),
				   data->regmap[IS_HOST_I2C_ADDRESS]);

	return max967xx_write_regs(data, deserializer_A_init_configs,
				   ARRAY_SIZE(deserializer_A_init_configs),
				   data->regmap[IS_DEVICE_I2C_A_ADDRESS]);
	#else
	return 0;
	#endif
}

int max96781_dump_regs(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96781_dump_regs:\n");

	for (i = 0; i < ARRAY_SIZE(dump_regs_config); i++) {
		ret = max967xx_read(dump_regs_config[i].reg_addr, &val,
				    data->regmap[dump_regs_config[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   dump_regs_config[i].reg_addr);
			return -1;
		}
		DISP_ERROR("dump reg 0x%04x val 0x%02x",
			   dump_regs_config[i].reg_addr, val);
		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					dump_regs_config[i].addr,dump_regs_config[i].reg_addr, val);
	}

	return 0;
}

int max96781_test_pattern(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96781_test_pattern:\n");

	for (i = 0; i < ARRAY_SIZE(test_pattern_regs); i++) {
		ret = max967xx_read(test_pattern_regs[i].reg_addr, &val,
				    data->regmap[test_pattern_regs[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   test_pattern_regs[i].reg_addr);
			return -1;
		}
		DISP_ERROR("dump reg 0x%04x val 0x%02x",
			   test_pattern_regs[i].reg_addr, val);
		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					test_pattern_regs[i].addr,test_pattern_regs[i].reg_addr, val);
	}

	return 0;
}

int max96781_force_enable(struct max967xx_data *data){
	return max96781_initialize(data);
}

