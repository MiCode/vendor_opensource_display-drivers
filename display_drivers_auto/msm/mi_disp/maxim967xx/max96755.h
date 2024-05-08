#ifndef _MAX96755_H_
#define _MAX96755_H_

#include "max967xx.h"

#define MAX96755_DEV_ID_REG 0x0D
#define MAX96755_DEV_ID 0x9B
#define MAX96755_REG1 0x01
#define MAX96755_MANDATORY_REG 0x302
#define MAX96755_MANDATORY_REG_VAL 0x10
#define MAX96755_CTRL3_REG 0x13
#define MAX96755_INTR3_REG 0x1B
#define MAX96755_INTR5_REG 0x1D
#define MAX96755_INTR7_REG 0x1F
#define MAX96755_LOCK_MASK BIT(3)
#define MAX96755_LOCK_A_MASK BIT(4)
#define MAX96755_LOCK_B_MASK BIT(5)

#define MAX96755_DISABLE_REMOTE_CONT BIT(4)
int max96755_is_locked(struct max967xx_data *data, int port);
int max96755_check_dev_id(struct max967xx_data *data);
int max96755_pre_init(struct max967xx_data *data);
int max96755_initialize(struct max967xx_data *data);
int max96755_dump_regs(struct max967xx_data *data, char *buffer);
int max96755_test_pattern(struct max967xx_data *data, char *buffer);
GMSL_LINK_STATE max96755_get_link_state(struct max967xx_data *);
int max96755_configure_single_link(struct max967xx_data *data);
int max96755_configure_dual_link(struct max967xx_data *data);
int max96755_configure_remote_control(struct max967xx_data *data, bool enable);
int max96755_power_ctrl(struct max967xx_data *data, int mode);
int max96755_colorbar_enable(struct max967xx_data *data, int enable);
#endif
