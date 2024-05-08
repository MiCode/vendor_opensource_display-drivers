#ifndef _MAX96789_H_
#define _MAX96789_H_
#include "max967xx.h"

#define MAX96789_DEV_ID_REG 0x0D
#define MAX96789_DEV_ID 0x80
//for enable/disable remote control
#define MAX96789_REG1 0x01
#define MAX96789_CTRL3_REG 0x13
#define MAX96789_INTR3_REG 0x1B
#define MAX96789_INTR5_REG 0x1D
#define MAX96789_INTR7_REG 0x1F
#define MAX96789_LOCK_MASK BIT(3)

#define MAX96789_DSI_WIDTH (1280)
#define MAX96789_DSI_HFP (30)
#define MAX96789_DSI_HS (4)
#define MAX96789_DSI_HBP (30)
#define MAX96789_DSI_HEIGHT (720)
#define MAX96789_DSI_VFP (6)
#define MAX96789_DSI_VS (6)
#define MAX96789_DSI_VBP (6)

int max96789_is_locked(struct max967xx_data *data, int port);
int max96789_check_dev_id(struct max967xx_data *data);
int max96789_pre_init(struct max967xx_data *data);
int max96789_initialize(struct max967xx_data *data);
int max96789_dump_regs(struct max967xx_data *data, char *buffer);
int max96789_test_pattern(struct max967xx_data *data, char *buffer);
#endif
