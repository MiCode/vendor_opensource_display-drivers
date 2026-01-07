/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mi_disp_sysfs:[%s:%d] " fmt, __func__, __LINE__

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gfp.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>

#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_disp_feature.h"

#define to_disp_display(d) dev_get_drvdata(d)

#ifdef DISPLAYFEATURE_V2_MODIFY
static ssize_t disp_param_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct disp_feature_ctl ctl;
	u32 tmp_data = 0;
	u32 param = 0;
	int ret = 0;
	memset(&ctl, 0, sizeof(struct disp_feature_ctl));
	sscanf(buf, "0x%x", &param);
	tmp_data = (param >> 16) & 0xFF;
	DISP_INFO("%s: tmp_data 1 =%d\n", __func__, tmp_data);
	if (tmp_data) {
		if (is_support_disp_feature_id(tmp_data)) {
		ctl.feature_id = tmp_data;
			DISP_ERROR("%s: ctl.feature_id =%d\n", __func__, ctl.feature_id);
		} else {
			DISP_ERROR("unsupported disp feature id\n");
			ret = -EAGAIN;
			goto exit;
		}
	}
	tmp_data = param & 0xF;
	ctl.feature_val = tmp_data;
	DISP_INFO("%s: tmp_data 2 =%d\n", __func__, tmp_data);
	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_set_disp_param(dd_ptr->display, &ctl);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}
exit:
	return ret ? ret : count;
}
#else
static ssize_t disp_param_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct disp_feature_ctl ctl;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	u32 tmp_data = 0;
	int ret = 0;

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		DISP_ERROR("can not allocate memory\n");
		ret = -ENOMEM;
		goto exit;
	}
	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);
	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		ret = kstrtoint(token, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			ret = -EAGAIN;
			goto exit_free;
		}
		if (is_support_disp_feature_id(tmp_data)) {
			ctl.feature_id = tmp_data;
		} else {
			DISP_ERROR("unsupported disp feature id\n");
			ret = -EAGAIN;
			goto exit_free;
		}
	}
	/* Removes leading whitespace from input_copy */
	if (input_copy) {
		input_copy = skip_spaces(input_copy);
	} else {
		DISP_ERROR("please check the number of parameters\n");
		ret = -EAGAIN;
		goto exit_free;
	}
	ret = kstrtoint(input_copy, 10, &tmp_data);
	if (ret) {
		DISP_ERROR("input buffer conversion failed\n");
		ret = -EAGAIN;
		goto exit_free;
	}
	ctl.feature_val = tmp_data;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_set_disp_param(dd_ptr->display, &ctl);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

exit_free:
	kfree(input_dup);
exit:
	return ret ? ret : count;
}
#endif

static ssize_t disp_param_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_show_disp_param(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t mipi_rw_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_set_mipi_rw(dd_ptr->display, (char *)buf);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t mipi_rw_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_show_mipi_rw(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t panel_info_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_read_panel_info(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t panel_build_id_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_read_panel_build_id_info(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_MI_DISP_ERPPANEL_INFO
extern char erppanel_info[];
static ssize_t erppanel_info_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret = -EINVAL;
	ret = sprintf(buf, "%s\n", erppanel_info);

	return ret;
}
#endif

extern char whitepoint_info_s[];
extern char lcd_name[];
static ssize_t wp_info_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret = -EINVAL;
	int index=0;
	char data[2]={0x00,0x00};
	int whitepoint_x=320;
	int whitepoint_y=320;
	data[0]=whitepoint_info_s[index]; ++index;
	data[1]=whitepoint_info_s[index]; ++index;
	sscanf(data,"%02x",&whitepoint_x);

	data[0]=whitepoint_info_s[index]; ++index;
	data[1]=whitepoint_info_s[index]; ++index;
	sscanf(data,"%02x",&whitepoint_y);

        DSI_INFO("origin read:%02x,%02x\n",whitepoint_x,whitepoint_y);

	whitepoint_x=300+whitepoint_x-0x80;
	whitepoint_y=320+whitepoint_y-0x80;

	ret = sprintf(buf, "p0=%04d,p1=%04d\n",whitepoint_x,whitepoint_y);
/*
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_read_wp_info(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}
*/
	return ret;
}

extern char lockdown_info[];
static ssize_t lockdown_info_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret = -EINVAL;
	ret = sprintf(buf, "%s\n", lockdown_info);

	return ret;
}

static ssize_t dynamic_fps_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct disp_fps_info fps_info;
	int rc = 0;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		rc = mi_dsi_display_get_fps(dd_ptr->display, &fps_info);
		if (rc) {
			snprintf(buf, PAGE_SIZE, "%s\n", "null");
			ret = -EINVAL;
		} else {
			ret = snprintf(buf, PAGE_SIZE, "%d\n", fps_info.fps);
		}
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t doze_brightness_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	u32 doze_brightness;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = kstrtoint(buf, 0, &doze_brightness);
		if (ret)
			return ret;
		ret = mi_dsi_display_set_doze_brightness(dd_ptr->display, doze_brightness);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t doze_brightness_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	u32 doze_brightness;
	int rc = 0;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		rc = mi_dsi_display_get_doze_brightness(dd_ptr->display, &doze_brightness);
		if (rc) {
			snprintf(buf, PAGE_SIZE, "%s\n", "null");
			ret = -EINVAL;
		} else {
			ret = snprintf(buf, PAGE_SIZE, "%d\n", doze_brightness);
		}
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t brightness_clone_store(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	unsigned long brightness;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = kstrtoul(buf, 0, &brightness);
		if (ret)
			return ret;
		ret = mi_dsi_display_set_brightness_clone(dd_ptr->display, brightness);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t brightness_clone_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int brightness_clone = 0;
	int rc = 0;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		rc = mi_dsi_display_get_brightness_clone(dd_ptr->display, &brightness_clone);
		if (rc) {
			snprintf(buf, PAGE_SIZE, "%s\n", "null");
			ret = -EINVAL;
		} else {
			ret = snprintf(buf, PAGE_SIZE, "%d\n", brightness_clone);
		}
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t max_brightness_clone_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int max_brightness_clone = 0;
	int rc = 0;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		rc = mi_dsi_display_get_max_brightness_clone(dd_ptr->display, &max_brightness_clone);
		if (rc) {
			snprintf(buf, PAGE_SIZE, "%s\n", "null");
			ret = -EINVAL;
		} else {
			ret = snprintf(buf, PAGE_SIZE, "%d\n", max_brightness_clone);
		}
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t hw_vsync_info_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_get_hw_vsync_info(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t cell_id_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_read_cell_id(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t disp_count_store(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_set_disp_count(dd_ptr->display, (char *)buf);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t disp_count_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_get_disp_count(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}

}

static ssize_t flatmode_check_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	bool status = false;

	if (dd_ptr->intf_type != MI_INTF_DSI)
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));

	mi_dsi_display_check_flatmode_status(dd_ptr->display, &status);
	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}



static DEVICE_ATTR_RW(disp_param);
static DEVICE_ATTR_RW(mipi_rw);
static DEVICE_ATTR_RO(panel_info);
static DEVICE_ATTR_RO(panel_build_id);
#ifdef CONFIG_MI_DISP_ERPPANEL_INFO
static DEVICE_ATTR_RO(erppanel_info);
#endif
static DEVICE_ATTR_RO(wp_info);
static DEVICE_ATTR_RO(lockdown_info);
static DEVICE_ATTR_RO(dynamic_fps);
static DEVICE_ATTR_RW(doze_brightness);
static DEVICE_ATTR_RW(brightness_clone);
static DEVICE_ATTR_RO(max_brightness_clone);
static DEVICE_ATTR_RO(hw_vsync_info);
static DEVICE_ATTR_RO(cell_id);
static DEVICE_ATTR_RW(disp_count);
static DEVICE_ATTR_RO(flatmode_check);


static struct attribute *disp_feature_attrs[] = {
	&dev_attr_disp_param.attr,
	&dev_attr_mipi_rw.attr,
	&dev_attr_panel_info.attr,
	&dev_attr_panel_build_id.attr,
#ifdef CONFIG_MI_DISP_ERPPANEL_INFO
	&dev_attr_erppanel_info.attr,
#endif
	&dev_attr_wp_info.attr,
	&dev_attr_lockdown_info.attr,
	&dev_attr_dynamic_fps.attr,
	&dev_attr_doze_brightness.attr,
	&dev_attr_brightness_clone.attr,
	&dev_attr_max_brightness_clone.attr,
	&dev_attr_hw_vsync_info.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_disp_count.attr,
	&dev_attr_flatmode_check.attr,
	NULL
};

static const struct attribute_group disp_feature_group = {
	.attrs = disp_feature_attrs,
};

static const struct attribute_group *disp_feature_groups[] = {
	&disp_feature_group,
	NULL
};

int mi_disp_create_device_attributes(struct device *dev)
{
	return sysfs_create_groups(&dev->kobj, disp_feature_groups);
}

void mi_disp_remove_device_attributes(struct device *dev)
{
	sysfs_remove_groups(&dev->kobj, disp_feature_groups);
}

