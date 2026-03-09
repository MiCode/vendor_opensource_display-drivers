/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"mi-disp-parse:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "mi_disp_print.h"
#include "dsi_panel.h"
#include "dsi_parser.h"
#include "mi_panel_id.h"
#include <linux/soc/qcom/smem.h>

#define DEFAULT_MAX_BRIGHTNESS_CLONE      ((1 << 14) -1)
#define SMEM_SW_DISPLAY_LOCKDOWN_TABLE 498

int mi_dsi_panel_parse_esd_gpio_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->esd_err_irq_gpio = of_get_named_gpio(
 			utils->data, "mi,esd-err-irq-gpio", 0);
	if (gpio_is_valid(mi_cfg->esd_err_irq_gpio)) {
		mi_cfg->esd_err_irq = gpio_to_irq(mi_cfg->esd_err_irq_gpio);
		rc = gpio_request(mi_cfg->esd_err_irq_gpio, "esd_err_irq_gpio");
		if (rc)
			DISP_ERROR("Failed to request esd irq gpio %d, rc=%d\n",
				mi_cfg->esd_err_irq_gpio, rc);
		else
			gpio_direction_input(mi_cfg->esd_err_irq_gpio);
	} else {
		rc = -EINVAL;
	}

	rc = utils->read_u32(utils->data, "mi,esd-err-irq-gpio-flag", &mi_cfg->esd_err_irq_flags);
	if (mi_cfg->esd_err_irq_flags)
			DISP_DEBUG("mi,esd-err-irq-gpio-flag is defined\n");

	return rc;
}

static void mi_dsi_panel_parse_round_corner_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->ddic_round_corner_enabled =
			utils->read_bool(utils->data, "mi,ddic-round-corner-enabled");
	if (mi_cfg->ddic_round_corner_enabled)
		DISP_DEBUG("mi,ddic-round-corner-enabled is defined\n");
}


static void mi_dsi_panel_parse_flat_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->flat_sync_te = utils->read_bool(utils->data, "mi,flat-need-sync-te");
	if (mi_cfg->flat_sync_te)
		DISP_INFO("mi,flat-need-sync-te is defined\n");
	else
		DISP_DEBUG("mi,flat-need-sync-te is undefined\n");
	mi_cfg->flatmode_default_on_enabled = utils->read_bool(utils->data,
				"mi,flatmode-default-on-enabled");
	if (mi_cfg->flatmode_default_on_enabled)
		DISP_INFO("flat mode is  default enabled\n");

#ifdef CONFIG_FACTORY_BUILD
	mi_cfg->flat_sync_te = false;
#endif

	mi_cfg->flat_update_flag = utils->read_bool(utils->data, "mi,flat-update-flag");
	if (mi_cfg->flat_update_flag) {
		DISP_INFO("mi,flat-update-flag is defined\n");
	} else {
		DISP_DEBUG("mi,flat-update-flag is undefined\n");
	}
}

static int mi_dsi_panel_parse_dc_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	const char *string;

	mi_cfg->dc_feature_enable = utils->read_bool(utils->data, "mi,dc-feature-enabled");
	if (!mi_cfg->dc_feature_enable) {
		DISP_DEBUG("mi,dc-feature-enabled not defined\n");
		return rc;
	}
	DISP_INFO("mi,dc-feature-enabled is defined\n");

	mi_cfg->default_dc_flag = utils->read_bool(utils->data, "mi,default-dc-flag");
	if (mi_cfg->default_dc_flag) {
		DISP_INFO("mi,default-dc-flag is defined,set dc_state to true\n");
		mi_cfg->feature_val[DISP_FEATURE_DC] = true;
		mi_cfg->real_dc_state = true;
	}

	rc = utils->read_string(utils->data, "mi,dc-feature-type", &string);
	if (rc){
		DISP_ERROR("mi,dc-feature-type not defined!\n");
		return -EINVAL;
	}
	if (!strcmp(string, "crc_skip_backlight")) {
		mi_cfg->dc_type = TYPE_CRC_SKIP_BL;
	} else {
		DISP_ERROR("No valid mi,dc-feature-type string\n");
		return -EINVAL;
	}
	DISP_DEBUG("mi, dc type is %s\n", string);

	mi_cfg->dc_update_flag = utils->read_bool(utils->data, "mi,dc-update-flag");
	if (mi_cfg->dc_update_flag) {
		DISP_INFO("mi,dc-update-flag is defined\n");
	} else {
		DISP_DEBUG("mi,dc-update-flag not defined\n");
	}

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-threshold", &mi_cfg->dc_threshold);
	if (rc) {
		mi_cfg->dc_threshold = 440;
		DISP_DEBUG("default dc threshold is %d\n", mi_cfg->dc_threshold);
	} else {
		DISP_INFO("dc threshold is %d \n", mi_cfg->dc_threshold);
	}

	return rc;
}

static int mi_dsi_panel_parse_demura_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-dc-demura-threshold", &mi_cfg->dc_demura_threshold);
	if (rc) {
		mi_cfg->dc_threshold = 607;
		DISP_INFO("default dc demura threshold is %d\n", mi_cfg->dc_demura_threshold);
	} else {
		DISP_INFO("dc demura threshold is %d \n", mi_cfg->dc_demura_threshold);
	}

	return rc;
}

static int mi_dsi_panel_parse_pwm_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-pwm-dbv-threshold", &mi_cfg->pwm_dbv_threshold);
	if (rc) {
		mi_cfg->pwm_dbv_threshold = 0;
		DISP_DEBUG("default dc threshold is %d\n", mi_cfg->pwm_dbv_threshold);
	} else {
		DISP_INFO("dc threshold is %d \n", mi_cfg->pwm_dbv_threshold);
	}

	return rc;
}

static int mi_dsi_panel_parse_backlight_config(struct dsi_panel *panel)
{
	int rc = 0;
	u32 val = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	rc = utils->read_u32(utils->data, "mi,panel-on-dimming-delay", &mi_cfg->panel_on_dimming_delay);
	if (rc) {
		mi_cfg->panel_on_dimming_delay = 0;
		DISP_DEBUG("mi,panel-on-dimming-delay not specified\n");
	} else {
		DISP_DEBUG("mi,panel-on-dimming-delay is %d\n", mi_cfg->panel_on_dimming_delay);
	}

	rc = utils->read_u32(utils->data, "mi,doze-hbm-dbv-level", &mi_cfg->doze_hbm_dbv_level);
	if (rc) {
		mi_cfg->doze_hbm_dbv_level = 0;
		DISP_DEBUG("mi,doze-hbm-dbv-level not specified\n");
	} else {
		DISP_INFO("mi,doze-hbm-dbv-level is %d\n", mi_cfg->doze_hbm_dbv_level);
	}

	rc = utils->read_u32(utils->data, "mi,doze-lbm-dbv-level", &mi_cfg->doze_lbm_dbv_level);
	if (rc) {
		mi_cfg->doze_lbm_dbv_level = 0;
		DISP_DEBUG("mi,doze-lbm-dbv-level not specified\n");
	} else {
		DISP_INFO("mi,doze-lbm-dbv-level is %d\n", mi_cfg->doze_lbm_dbv_level);
	}
	rc = utils->read_u32(utils->data, "mi,max-brightness-clone", &mi_cfg->max_brightness_clone);
	if (rc) {
		mi_cfg->max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
	}
	DISP_DEBUG("max_brightness_clone=%d\n", mi_cfg->max_brightness_clone);
	rc = utils->read_u32(utils->data, "mi,max-brightness-normal", &val);
	if (rc) {
		panel->bl_config.bl_normal_max = 0;
	} else {
		panel->bl_config.bl_normal_max = val;
	}
	DISP_INFO("bl_normal_max=%d\n", panel->bl_config.bl_normal_max);
#ifdef CONFIG_FACTORY_BUILD
	rc = utils->read_u32(utils->data, "mi,mdss-dsi-fac-bl-max-level", &val);
	if (rc) {
		rc = 0;
		DSI_DEBUG("[%s] factory bl-max-level unspecified\n", panel->name);
	} else {
		panel->bl_config.bl_max_level = val;
	}

	rc = utils->read_u32(utils->data, "mi,mdss-fac-brightness-max-level",&val);
	if (rc) {
		rc = 0;
		DSI_DEBUG("[%s] factory brigheness-max-level unspecified\n", panel->name);
	} else {
		panel->bl_config.brightness_max_level = val;
	}
	DISP_INFO("bl_max_level is %d, brightness_max_level is %d\n",
		panel->bl_config.bl_max_level, panel->bl_config.brightness_max_level);
#endif

	mi_cfg->disable_ic_dimming = utils->read_bool(utils->data, "mi,disable-ic-dimming-flag");
	if (mi_cfg->disable_ic_dimming) {
		DISP_DEBUG("disable_ic_dimming\n");
	}
	mi_cfg->sf_set_doze_brightness = utils->read_bool(utils->data, "mi,sf-set-doze-brightness");
	if (mi_cfg->sf_set_doze_brightness) {
		DISP_DEBUG("sf set doze brightness\n");
	}

	return rc;
}

static void mi_dsi_panel_parse_lhbm_config(struct dsi_panel *panel)
{
	int rc = 0;

	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->local_hbm_enabled =
			utils->read_bool(utils->data, "mi,local-hbm-enabled");
	if (mi_cfg->local_hbm_enabled)
		DISP_INFO("local hbm feature enabled\n");

	rc = utils->read_u32(utils->data, "mi,local-hbm-ui-ready-delay-num-frame",
			&mi_cfg->lhbm_ui_ready_delay_frame);
	if (rc)
		mi_cfg->lhbm_ui_ready_delay_frame = 0;
	DISP_INFO("local hbm ui_ready delay %d frame\n",
			mi_cfg->lhbm_ui_ready_delay_frame);

	rc = utils->read_u32(utils->data, "mi,local-hbm-update-with-hbm-mode-threshold",
			&mi_cfg->lhbm_hbm_mode_threshold);
	if (rc)
		mi_cfg->lhbm_hbm_mode_threshold = 2047;
	DISP_INFO("local hbm lhbm_hbm_mode_bl_value is %d \n",
			mi_cfg->lhbm_hbm_mode_threshold);

	rc = utils->read_u32(utils->data, "mi,local-hbm-update-to-lbl-mode-threshold",
			&mi_cfg->lhbm_lbl_mode_threshold);
	if (rc)
		mi_cfg->lhbm_lbl_mode_threshold = 0;
	DISP_INFO("local hbm lhbm_lbl_mode_threshold  is %d \n",
			mi_cfg->lhbm_lbl_mode_threshold);

	mi_cfg->need_fod_animal_in_normal =
			utils->read_bool(utils->data, "mi,need-fod-animal-in-normal-enabled");
	if (mi_cfg->need_fod_animal_in_normal)
		DISP_INFO("need fod animal in normal enabled\n");

	mi_cfg->lhbm_off_sync_te =
			utils->read_bool(utils->data, "mi,lhbm-off-need-sync-te");
	if (mi_cfg->lhbm_off_sync_te)
		DISP_INFO("mi,lhbm-off-need-sync-te is defined\n");
}

int mi_dsi_panel_parse_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_parser_utils *utils = &panel->utils;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	mi_cfg->panel_build_id_read_needed =
		utils->read_bool(utils->data, "mi,panel-build-id-read-needed");
	if (mi_cfg->panel_build_id_read_needed) {
		rc = dsi_panel_parse_build_id_read_config(panel);
		if (rc) {
			mi_cfg->panel_build_id_read_needed = false;
			DSI_ERR("[%s] failed to get panel build id read infos, rc=%d\n",
					panel->name, rc);
		}
	}
	rc = dsi_panel_parse_cell_id_read_config(panel);
	if (rc) {
		DSI_ERR("[%s] failed to get panel cell id read infos, rc=%d\n",
			panel->name, rc);
		rc = 0;
	}
	rc = dsi_panel_parse_wp_reg_read_config(panel);
	if (rc) {
		DSI_ERR("[%s] failed to get panel wp read infos, rc=%d\n",
			panel->name, rc);
		rc = 0;
	}

	mi_cfg->pmic_reset_flag = utils->read_bool(utils->data, "mi,pmic-reset-gpio-always-on-flag");
	DISP_INFO("pmic reset flag is %d \n",mi_cfg->pmic_reset_flag);

	mi_cfg->is_tddi_flag = false;
	mi_cfg->panel_dead_flag = false;
	mi_cfg->tddi_gesture_flag = false;
	mi_cfg->is_tddi_flag = utils->read_bool(utils->data, "mi,is-tddi-flag");
	if (mi_cfg->is_tddi_flag)
		pr_info("panel is tddi\n");

	mi_dsi_panel_parse_round_corner_config(panel);
	mi_dsi_panel_parse_lhbm_config(panel);
	mi_dsi_panel_parse_flat_config(panel);
	mi_dsi_panel_parse_dc_config(panel);
	mi_dsi_panel_parse_backlight_config(panel);
	mi_dsi_panel_parse_demura_config(panel);
	mi_dsi_panel_parse_pwm_config(panel);

	rc = utils->read_u32(utils->data, "mi,panel-hbm-backlight-threshold", &mi_cfg->hbm_backlight_threshold);
	if (rc)
		mi_cfg->hbm_backlight_threshold = 8192;
	DISP_DEBUG("panel hbm backlight threshold %d\n", mi_cfg->hbm_backlight_threshold);

	mi_cfg->count_hbm_by_backlight = utils->read_bool(utils->data, "mi,panel-count-hbm-by-backlight-flag");
	if (mi_cfg->count_hbm_by_backlight)
		DISP_DEBUG("panel count hbm by backlight\n");

	return rc;
}

