#ifndef _MAX96781_H_
#define _MAX96781_H_
#include "max967xx.h"

#define MAX96781_DEV_ID_REG 0x0D
#define MAX96781_DEV_ID 0xFF
#define MAX96781_LOCK_REG 0x13
#define MAX96781_LOCK_MASK BIT(3)
#define MAX96781_LOCK_A_MASK BIT(2)
#define MAX96781_LOCK_B_MASK BIT(3)

int max96781_is_locked(struct max967xx_data *data, int port);
int max96781_check_dev_id(struct max967xx_data *data);
int max96781_initialize(struct max967xx_data *data);
int max96781_pre_init(struct max967xx_data *data);
int max96781_dump_regs(struct max967xx_data *data, char *buffer);
int max96781_test_pattern(struct max967xx_data *data, char *buffer);
int max96781_force_enable(struct max967xx_data *data);
#endif
