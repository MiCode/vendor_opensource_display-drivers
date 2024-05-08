/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Maxim MAX96789 serializer driver 
 *
 * This driver & most of the hard coded values are based on the reference
 * application delivered by Maxim for this device.
 *
 * Copyright (C) 2016 Maxim Integrated Products
 * Copyright (C) 2017 Renesas Electronics Corporation
 */

#ifndef _MAX967XX_H_
#define _MAX967XX_H_
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include "mi_disp_print.h"
#include "mi_ext_panel.h"
#include <drm/drm_bridge.h>
#include <drm/drm_drv.h>


/*kept same as dts */
#define MAX96755_DRV_STR "maxin,max96755"
#define MAX96781_DRV_STR "maxin,max96781"
#define MAX96789_DRV_STR "maxin,max96789"
#define MAX967XX_DEV_ID "max967xx"
#define MAX96755_DEBUG 0
#define MAX96781_DEBUG 0
#define MAX96789_DEBUG 0

enum serdes_i2c_address {
	IS_HOST_I2C_ADDRESS = 0,
	IS_DEVICE_I2C_A_ADDRESS,
	IS_DEVICE_I2C_B_ADDRESS,
	I2C_DEVICE_COUNT,
};

enum serdes_type {
	SERIALIZER = 0x0,
	DESERIALIZER_A,
	DESERIALIZER_B,
};

typedef enum {
	SINGLE_A = 0,
	SINGLE_B,
	MAX_LINK_COUNT,
} gmsl_link_mode;

typedef enum {
	NO_LINK_CFG  = 0,
	DUAL_LINK_CFG = 1,
	AUTO_LINK_CFG,
	SINGLE_LINK_A_CFG,
	SINGLE_LINK_B_CFG,
	SPLIT_LINK_CFG,
} GMSL_LINK_CFG;

typedef enum {
	NO_LINK  = 0,
	DUAL_LINK = 1,
	AUTO_LINK,
	SINGLE_LINK_A,
	SINGLE_LINK_B,
	SPLIT_LINK,
} GMSL_LINK_STATE;

struct gmsl_link_t {
	GMSL_LINK_STATE link_state;
	GMSL_LINK_CFG link_cfg;
};

typedef enum {
	SERDES_STATE_OPEN = 1,
	SERDES_STATE_CLOSE,
	SERDES_STATE_SINGLE_LINK,
	SERDES_STATE_DUAL_LINK,
	SERDES_STATE_NO_LINK,
	SERDES_STATE_CONFIG_REGISTER,
	SERDES_STATE_CHECK_WORK_STATE,
	SERDES_STATE_NOT_CARE,
} serdes_work_state;

typedef enum {
	SERDES_MSG_POWER_STATE = 1,
	SERDES_MSG_LINK_STATE,
} serdes_msg_type;

typedef struct {
	struct list_head node;
	serdes_msg_type type;
	serdes_work_state state;
} serdes_msg;

struct max967xx_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct max967xx_data {
	struct i2c_client *client[I2C_DEVICE_COUNT];
	//struct i2c_client *des_client[DUAL_LINK];

	struct regmap *regmap[I2C_DEVICE_COUNT];
	void *priv_data;
	int gpio_pwdn;
	int lock_gpio;
	int lock_irq;
	const char *lock_irq_name;
	int errb_irq;
	int errb_gpio;
	const char *errb_irq_name;
	struct mutex mutex;
	struct delayed_work check_work;
	struct blocking_notifier_head notifier_list;
	struct device *class_dev;
	struct task_struct *hot_plug_thread;
	struct list_head msg_head;
	wait_queue_head_t pending_wq;
	spinlock_t spinlock;
	serdes_work_state current_work_state;
	int mi_ext_panel_count;
	struct panel_device* mi_ext_panel[MAX_MI_EXT_PANEL_COUNT_PER_DSI];
	struct max967xx_pinctrl pinctrl;
};

struct cmd_16bit {
	u8 addr;
	u16 reg_addr;
	u8 reg_value;
	u32 delay; //usleep
};

enum GMSL_TYPE {
	MAX96755 = 0x0,
	MAX96781 = 0x1,
	MAX96789 = 0x2,
	MAX967XX_COUNT,
};

struct max967xx_priv {
	int (*max967xx_check_dev_id)(struct max967xx_data *);
	int (*max967xx_is_locked)(struct max967xx_data *, int port);
	int (*max967xx_dump_reg)(struct max967xx_data *, char *buffer);
	int (*max967xx_test_pattern)(struct max967xx_data *, char *buffer);
	int (*max967xx_force_enable)(struct max967xx_data *);
	int (*max967xx_pre_init)(struct max967xx_data *);
	int (*max967xx_init)(struct max967xx_data *);
	int (*max967xx_post_init)(struct max967xx_data *);
	int (*max967xx_power_ctrl)(struct max967xx_data *, int mode);
	GMSL_LINK_STATE (*max967xx_get_link_state)(struct max967xx_data *);
	int (*max967xx_colorbar_enable)(struct max967xx_data *, int enable);
	bool post_init;
	bool debug;
	enum GMSL_TYPE type;
	int deserializer_count;
	struct drm_bridge bridge;
	unsigned char deserializer_i2c_addresses[MAX_LINK_COUNT];
	char *deserializer_name[MAX_LINK_COUNT];
};
int max967xx_write(struct max967xx_data *data, int reg, int value,
		   struct regmap *regmap);
int max967xx_write_regs(struct max967xx_data *data, const struct cmd_16bit *cmd,
			int len, struct regmap *regmap);
int max967xx_read(int reg, int *val, struct regmap *regmap);
int max967xx_register_notifier(struct notifier_block *nb, enum GMSL_TYPE type);
int max967xx_unregister_notifier(struct notifier_block *nb,
				 enum GMSL_TYPE type);
int max967xx_i2c_dev_init(void);
void max967xx_i2c_dev_exit(void);
struct max967xx_data* max967xx_data_get(struct device_node *of_node);
int max967xx_set_power_mode(struct max967xx_data *data, int mode);
int32_t max967xx_send_ctrl_info(struct max967xx_data *data, serdes_msg_type type, serdes_work_state state);
int32_t max967xx_recv_ctrl_info(struct max967xx_data *data, serdes_msg *recv_msg, int32_t time_out);
struct max967xx_data* max967xx_data_get(struct device_node *of_node);
int max967xx_set_power_mode(struct max967xx_data *data, int mode);
#endif //_MAX967XX_H_
