#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

#include <linux/i2c.h>
#include <linux/regmap.h>

struct i2c_interface;

struct i2c_interface_ops {
	int (*i2c_raw_write)(struct i2c_interface *i2c_inst, unsigned int reg, const void *val,
		   size_t val_len);
	int (*i2c_write)(struct i2c_interface *i2c_inst, int reg, int value);
	int (*i2c_read)(struct i2c_interface *i2c_inst, int reg, uint8_t *val, size_t val_len);
};

struct i2c_interface {
	struct regmap *regmap;
	struct mutex i2c_mutex;
	struct i2c_interface_ops ops;
};

int panel_i2c_resource_int(void *device);
#endif /* __I2C_INTERFACE_H__ */
