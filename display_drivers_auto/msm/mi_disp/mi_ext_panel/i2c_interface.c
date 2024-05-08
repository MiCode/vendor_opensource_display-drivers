#include "i2c_interface.h"
#include "mi_ext_panel.h"
#include "mi_disp_print.h"
#include <linux/mutex.h>

static int panel_i2c_raw_write(struct i2c_interface *i2c_inst, unsigned int reg, const void *val,
		   size_t val_len)
{
	int ret = 0;

	mutex_lock(&i2c_inst->i2c_mutex);
	ret = regmap_raw_write(i2c_inst->regmap, reg, val, val_len);
	mutex_unlock(&i2c_inst->i2c_mutex);

	return ret;
}

static int panel_i2c_write(struct i2c_interface *i2c_inst, int reg, int value)
{
	int ret = 0;

	mutex_lock(&i2c_inst->i2c_mutex);
	ret = regmap_write(i2c_inst->regmap, reg, value);
	mutex_unlock(&i2c_inst->i2c_mutex);

	return ret;
}

static int panel_i2c_read(struct i2c_interface *i2c_inst, int reg, uint8_t *val,
		  size_t val_len)
{
	int ret;
	mutex_lock(&i2c_inst->i2c_mutex);
	if (val_len == 1) {
		ret = regmap_read(i2c_inst->regmap, reg, (unsigned int*)val);
	} else {
		ret = regmap_raw_read(i2c_inst->regmap, reg, val, val_len);
	}

	mutex_unlock(&i2c_inst->i2c_mutex);

	return ret;
}

int panel_i2c_resource_int(void *device) {
	struct panel_device *panel_dev = (struct panel_device *)device;
	struct i2c_interface* i2c_inst;
	const struct regmap_config *i2c_cfg;

	if (!panel_dev || !panel_dev->client)
		return -ENODEV;

	i2c_inst = &panel_dev->i2c_instance;
	i2c_cfg = &panel_dev->spec_info->i2c_cfg;

	i2c_inst->regmap = devm_regmap_init_i2c(panel_dev->client, i2c_cfg);
	if (IS_ERR(i2c_inst->regmap)) {
		DISP_ERROR("i2c regmap init fail !");
		return PTR_ERR(i2c_inst->regmap);
	}

	mutex_init(&i2c_inst->i2c_mutex);
	i2c_inst->ops.i2c_raw_write = panel_i2c_raw_write;
	i2c_inst->ops.i2c_write = panel_i2c_write;
	i2c_inst->ops.i2c_read = panel_i2c_read;

	return 0;
}


