#ifndef PANEL_COMMON_H_
#define PANEL_COMMON_H_
#include <linux/i2c.h>
#include "mi_disp_print.h"

#define EXT_PANEL_INFO(device, fmt, ...)							\
	do {												\
		pr_info("[mi_ext_panel][%s]%s: " fmt, (device->device_name)?device->device_name:"NULL",	\
						__FUNCTION__, ##__VA_ARGS__);	\
	} while (0)

#define EXT_PANEL_ERROR(device, fmt, ...)							\
	do {												\
		pr_err("[mi_ext_panel][%s]%s: " fmt, (device->device_name)?device->device_name:"NULL", \
						__FUNCTION__, ##__VA_ARGS__);	\
	} while (0)


#define PANEL_NAME_SIZE 32

uint8_t panel_calculate_crc(uint8_t *buf, uint8_t length);
uint8_t panel_calculate_checksum(uint8_t *buf, uint8_t length);
int panel_create_sysfs_device(void *device, void *drvdata, 
											const struct attribute_group **groups);
#endif /* PANEL_COMMON_H_ */
