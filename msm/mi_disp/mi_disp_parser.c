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
#define SMEM_SW_DISPLAY_GAMMA_TABLE 498
#define SMEM_SW_DISPLAY_LHBM_TABLE 500

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

static void mi_dsi_panel_parse_gamma_read_config(struct dsi_panel *panel)
{
	int i = 0, tmp = 0;
	size_t item_size;
	void *gamma_ptr = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O2_PANEL_PA || mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PA) {
		gamma_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_GAMMA_TABLE, &item_size);
		if (!IS_ERR(gamma_ptr) && item_size > 0) {
			DSI_INFO("gamma data size %lu\n", item_size);
			memcpy(mi_cfg->gamma_rgb_param, gamma_ptr, item_size);
			for (i = 1; i < item_size; i += 2) {
				tmp = ((mi_cfg->gamma_rgb_param[i-1]) << 8) | mi_cfg->gamma_rgb_param[i];
				DSI_INFO("gamma index %d = 0x%04X\n", i, tmp);
				if (tmp == 0x0000) {
					DSI_INFO("uefi read gamma data failed, use default param!\n");
					mi_cfg->read_gamma_success = false;
				} else {
					mi_cfg->read_gamma_success = true;
					break;
				}
			}
		}
	}
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
	int i  = 0, tmp = 0;
	size_t item_size;
	void *lhbm_ptr = NULL;

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

	if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O10U_PANEL_PA ||
		mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O10U_PANEL_PB) {
		lhbm_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_LHBM_TABLE, &item_size);
		if (!IS_ERR(lhbm_ptr) && item_size > 0) {
			DSI_INFO("lhbm data size %zu\n", item_size);
			memcpy(mi_cfg->lhbm_rgb_param, lhbm_ptr, item_size);
			for (i = 1; i < item_size; i += 2) {
				tmp = ((mi_cfg->lhbm_rgb_param[i-1]) << 8) | mi_cfg->lhbm_rgb_param[i];
				DSI_INFO("lhbm index %d = 0x%04X\n", i, tmp);
				if (tmp == 0x0000) {
					DSI_INFO("uefi read lhbm data failed, use default param!\n");
					mi_cfg->read_lhbm_gamma_success = false;
				} else {
					mi_cfg->read_lhbm_gamma_success = true;
					break;
				}
			}
		}
	}
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
	mi_dsi_panel_parse_gamma_read_config(panel);

	rc = utils->read_u32(utils->data, "mi,panel-hbm-backlight-threshold", &mi_cfg->hbm_backlight_threshold);
	if (rc)
		mi_cfg->hbm_backlight_threshold = 8192;
	DISP_DEBUG("panel hbm backlight threshold %d\n", mi_cfg->hbm_backlight_threshold);

	mi_cfg->count_hbm_by_backlight = utils->read_bool(utils->data, "mi,panel-count-hbm-by-backlight-flag");
	if (mi_cfg->count_hbm_by_backlight)
		DISP_DEBUG("panel count hbm by backlight\n");

	return rc;
}

