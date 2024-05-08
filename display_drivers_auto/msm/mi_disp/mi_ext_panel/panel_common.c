#include "panel_common.h"
#include "mi_ext_panel.h"

uint8_t panel_calculate_crc(uint8_t *buf, uint8_t length)
{
	uint8_t lub_crc = 0xFF, i;
	while (length--) {
		lub_crc ^= *buf++;
		for (i = 8; i > 0; --i) {
			if (lub_crc & 0x80) {
				lub_crc = (lub_crc << 1) ^ 0x1d;
			} else {
				lub_crc = (lub_crc << 1);
			}
		}
	}
	return lub_crc;
}

uint8_t panel_calculate_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t lub_crc = 0;
	while (length--) {
		lub_crc += *buf++;
	}
	return lub_crc;
}

int panel_create_sysfs_device(void *device, void *drvdata,
											const struct attribute_group **groups) {
	struct panel_device *panel_dev = (struct panel_device *)device;
	if (!panel_dev || !panel_dev->panel_class)
		return -EINVAL;

	panel_dev->dev = device_create(panel_dev->panel_class, NULL, 'T', drvdata, panel_dev->device_name);
	return sysfs_create_groups(&panel_dev->dev->kobj, groups);
}

