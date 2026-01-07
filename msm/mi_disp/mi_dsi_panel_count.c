/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#include <linux/notifier.h>
#include "mi_hwconf_manager.h"
#include "dsi_panel.h"

#define HWCONPONENT_NAME "display"
#define HWCONPONENT_KEY_LCD "LCD"
//#define HWMON_CONPONENT_NAME "display"
#define HWMON_KEY_ACTIVE "panel_active"
#define HWMON_KEY_DOZE_ACTIVE "panel_doze_active"
#define HWMON_KEY_DAILY_ACTIVE "panel_daily_active"
#define HWMON_KEY_DOZE_DAILY_ACTIVE "panel_doze_daily_active"
#define HWMON_KEY_REFRESH "panel_refresh"
#define HWMON_KEY_BOOTTIME "kernel_boottime"
#define HWMON_KEY_DAYS "kernel_days"
#define HWMON_KEY_PANEL_ID "panel_id"
#define HWMON_KEY_POWERON_COST_AVG "poweron_cost_avg"
#define HWMON_KEY_ESD_TIMES "esd_times"
#define HWMON_KEY_TE_LOST_TIMES "te_lost_times"
#define HWMON_KEY_UNDERRUN_TIMES "underrun_times"
#define HWMON_KEY_OVERFLOW_TIMES "overflow_times"
#define HWMON_KEY_PINGPONG_TIMEOUT_TIMES "pingpong_timeout_times"
#define HWMON_KEY_COMMIT_LONG_TIMES "commit_long_times"
#define HWMON_KEY_CMDQ_TIMEOUT_TIMES "cmdq_timeout_times"

static char *hwmon_key_fps[FPS_MAX_NUM] = {"1fps_times","10fps_times","24fps_times","30fps_times",
"40fps_times", "50fps_times", "60fps_times", "90fps_times", "120fps_times", "144fps_times"};
static const u32 dynamic_fps[FPS_MAX_NUM] = {1, 10, 24, 30, 40, 50, 60, 90, 120, 144};
static char stored_system_build_version[100] = "null";
static char system_build_version[100] = "null";

char HWMON_CONPONENT_NAME[64];
int disp_index = MI_DISP_PRIMARY;

#define DAY_SECS (60*60*24)
static struct dsi_panel * global_panel[MI_DISP_MAX];

void mi_dsi_panel_state_count(struct dsi_panel *panel, PANEL_COUNT_EVENT event, int value)
{
	if (!panel) {
		pr_err("invalid input\n");
		return;
	}

	if (!strcmp(panel->type, "primary")) {
		disp_index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		disp_index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	switch (event)
	{
	case PANEL_ACTIVE:
		mi_dsi_panel_active_count(panel, value);
		break;
	case PANEL_DOZE_ACTIVE:
		mi_dsi_panel_doze_active_count(panel, value);
		break;
	case PANEL_FPS:
		mi_dsi_panel_fps_count(panel, value);
		break;
	case PANEL_POWERON_COST:
		mi_dsi_panel_power_on_cost_count(panel, value);
		break;
	case PANEL_ESD:
		mi_dsi_panel_esd_count(panel, value);
		break;
	case PANEL_TE_LOST:
		mi_dsi_panel_te_lost_count(panel, value);
		break;
	case UNDERRUN:
		mi_dsi_panel_underrun_count(panel, value);
		break;
	case OVERLOW:
		mi_dsi_panel_overflow_count(panel, value);
		break;
	case PINGPONG_TIMEOUT:
		mi_dsi_panel_pingpong_timeout_count(panel, value);
		break;
	case COMMIT_LONG:
		mi_dsi_panel_commit_long_count(panel, value);
		break;
	case CMDQ_TIMEOUT:
		break;
	default:
		break;
	}
	return;
}

void mi_dsi_panel_active_count(struct dsi_panel *panel, int enable)
{
	static u32 on_times[MI_DISP_MAX];
	static u32 off_times[MI_DISP_MAX];
	static u64 timestamp_panelon[MI_DISP_MAX];
	char ch[64] = {0};

	if (enable && !panel->mi_count.panel_active_count_enable) {
		timestamp_panelon[disp_index] = get_jiffies_64();
		on_times[disp_index]++;
		panel->mi_count.panel_active_count_enable = true;
	} else if (!enable && panel->mi_count.panel_active_count_enable){
		ktime_t boot_time;
		u32 delta_days = 0;
		u64 jiffies_time = 0;
		struct timespec64 rtctime;

		off_times[disp_index]++;
		/* caculate panel active duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_panelon[disp_index])) {
			panel->mi_count.panel_active += (jiffies_time - timestamp_panelon[disp_index]);
			panel->mi_count.panel_daily_active += (jiffies_time - timestamp_panelon[disp_index]);
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.panel_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.panel_daily_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAILY_ACTIVE, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.kickoff_count / 60);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, ch);

		boot_time = ktime_get_boottime();
		do_div(boot_time, NSEC_PER_SEC);
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.boottime + boot_time);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, ch);

		ktime_get_real_ts64(&rtctime);

		if (panel->mi_count.bootRTCtime != 0) {
			if (rtctime.tv_sec > panel->mi_count.bootRTCtime) {
				if (rtctime.tv_sec - panel->mi_count.bootRTCtime > 10 * 365 * DAY_SECS) {
					panel->mi_count.bootRTCtime = rtctime.tv_sec;
				} else {
					if (rtctime.tv_sec - panel->mi_count.bootRTCtime > DAY_SECS) {
						delta_days = (rtctime.tv_sec - panel->mi_count.bootRTCtime) / DAY_SECS;
						panel->mi_count.bootdays += delta_days;
						panel->mi_count.bootRTCtime = rtctime.tv_sec -
							((rtctime.tv_sec - panel->mi_count.bootRTCtime) % DAY_SECS);
					}
				}
			} else {
				panel->mi_count.bootRTCtime = rtctime.tv_sec;
			}
		} else {
			panel->mi_count.bootRTCtime = rtctime.tv_sec;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bootdays);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, ch);

		panel->mi_count.panel_active_count_enable = false;
	}

	return;
};

void mi_dsi_panel_doze_active_count(struct dsi_panel *panel, int enable)
{
	static u64 timestamp_paneldozeon[MI_DISP_MAX];
	char ch[64] = {0};

	if (enable && !panel->mi_count.panel_doze_active_count_enable) {
		timestamp_paneldozeon[disp_index] = get_jiffies_64();
		panel->mi_count.panel_doze_active_count_enable = true;
	} else if (!enable && panel->mi_count.panel_doze_active_count_enable){
		u64 jiffies_time = 0;

		/* caculate panel active duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_paneldozeon[disp_index])) {
			panel->mi_count.panel_doze_active += (jiffies_time - timestamp_paneldozeon[disp_index]);
			panel->mi_count.panel_doze_daily_active += (jiffies_time - timestamp_paneldozeon[disp_index]);
		}

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.panel_doze_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_ACTIVE, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.panel_doze_daily_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_DAILY_ACTIVE, ch);

		panel->mi_count.panel_doze_active_count_enable = false;
	}

	return;
};

void mi_dsi_panel_fps_count(struct dsi_panel *panel, u32 fps)
{
	static u32 timming_index[MI_DISP_MAX] = {0}; 
	static u64 timestamp_fps[MI_DISP_MAX];
	static bool first_into_flag[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=true};
	u64 jiffies_time = 0;
	int change_fps_index = 0;
	char ch[64] = {0};

	for(change_fps_index = 0; change_fps_index < FPS_MAX_NUM; change_fps_index++) {
		if (fps == dynamic_fps[change_fps_index])
			break;
	}

	if (first_into_flag[disp_index]) {
		timestamp_fps[disp_index] = get_jiffies_64();
		first_into_flag[disp_index] = false;
		timming_index[disp_index] = change_fps_index;
		return;
	}

	if (change_fps_index != timming_index[disp_index]) {
		jiffies_time = get_jiffies_64();

		if (timming_index[disp_index] != FPS_MAX_NUM) {
			if (time_after64(jiffies_time, timestamp_fps[disp_index]))
				panel->mi_count.fps_times[timming_index[disp_index]] += (jiffies_time - timestamp_fps[disp_index]);

			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.fps_times[timming_index[disp_index]] / HZ);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[timming_index[disp_index]], ch);
		}
		timestamp_fps[disp_index] = get_jiffies_64();
	}
	timming_index[disp_index] = change_fps_index;

	return;
}

void mi_dsi_panel_set_build_version(struct dsi_panel *panel, char * build_verison, u32 size)
{
	memcpy(system_build_version, build_verison, size);
	pr_info("%s: last - %s to  new - %s\n",__func__,  stored_system_build_version, system_build_version);

	if (strcmp(stored_system_build_version, "null") && strcmp(stored_system_build_version, system_build_version)) {
		mi_dsi_panel_clean_data();
	}
}

void mi_dsi_panel_clean_data(void)
{
	int i = MI_DISP_PRIMARY;
	char count_str[64] = {0};

	for(i = MI_DISP_PRIMARY; i < MI_DISP_MAX; i ++) {
		if (global_panel[i] == NULL)
			continue;
		if (!strcmp(global_panel[i] ->type, "primary")) {
			disp_index = MI_DISP_PRIMARY;
			strcpy(HWMON_CONPONENT_NAME, "display");
		} else {
			disp_index = MI_DISP_SECONDARY;
			strcpy(HWMON_CONPONENT_NAME, "display2");
		}

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.panel_daily_active = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.panel_daily_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAILY_ACTIVE, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.panel_doze_daily_active = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.panel_doze_daily_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_DAILY_ACTIVE, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.esd_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.te_lost_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.underrun_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.overflow_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.pingpong_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.commit_long_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.cmdq_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	}
}

/*
* This function just caculate recent 100 times power on time cost
* For too many times may affect time effectiveness, and it's not
* good for check in debug version.
*/
void mi_dsi_panel_power_on_cost_count(struct dsi_panel *panel, int is_start)
{
	char ch[64] = {0};
	u64 jiffies_time = 0;
	u64 poweron_temp_time = 0;
	static u64 timestamp_power_start[MI_DISP_MAX];
	static int poweron_count_times[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=0};
	static int temp_poweron_cost_avg_times[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=0};
	static bool power_flag_get[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=false};
	static bool first_into_flag[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=true};

	if (first_into_flag[disp_index]) {
		if (poweron_count_times[disp_index] == 1) {
			first_into_flag[disp_index] = false;
			poweron_count_times[disp_index] = 0;
			return;
		}
		++poweron_count_times[disp_index];
		return;
	}

	if (is_start) {
		power_flag_get[disp_index] = true;
		timestamp_power_start[disp_index] = get_jiffies_64();
		//pr_info("poweron_temp_time start\n");
	} else if (power_flag_get[disp_index]) {
		power_flag_get[disp_index] = false;
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_power_start[disp_index]))
			poweron_temp_time = jiffies_to_msecs(jiffies_time - timestamp_power_start[disp_index]);
		//pr_info("poweron_temp_time = %lld, count: %d, disp_Index: %d\n", poweron_temp_time, poweron_count_times[disp_index], disp_index);
		if (poweron_count_times[disp_index] < 100) {
			++poweron_count_times[disp_index];
			temp_poweron_cost_avg_times[disp_index] =
				(temp_poweron_cost_avg_times[disp_index] *  (poweron_count_times[disp_index] - 1) + poweron_temp_time *10)
				 / poweron_count_times[disp_index];
			panel->mi_count.poweron_cost_avg = temp_poweron_cost_avg_times[disp_index] /10;
		} else {
			poweron_count_times[disp_index] = 1;
			temp_poweron_cost_avg_times[disp_index] = poweron_temp_time * 10;
			panel->mi_count.poweron_cost_avg = poweron_temp_time;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.poweron_cost_avg);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, ch);
	}

	return;
}

void mi_dsi_panel_esd_count(struct dsi_panel *panel, int is_irq)
{
	char ch[64] = {0};

	panel->mi_count.esd_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.esd_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, ch);

	return;
}

void mi_dsi_panel_te_lost_count(struct dsi_panel *panel, u32 fps)
{
	char ch[64] = {0};

	panel->mi_count.te_lost_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.te_lost_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, ch);

	return;
}

void mi_dsi_panel_underrun_count(struct dsi_panel *panel, u32 fps)
{
	char ch[64] = {0};

	panel->mi_count.underrun_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.underrun_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, ch);

	return;
}

void mi_dsi_panel_overflow_count(struct dsi_panel *panel, u32 fps)
{
	char ch[64] = {0};

	panel->mi_count.overflow_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.overflow_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, ch);

	return;
}

void mi_dsi_panel_pingpong_timeout_count(struct dsi_panel *panel, u32 fps)
{
	char ch[64] = {0};

	panel->mi_count.pingpong_timeout_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.pingpong_timeout_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, ch);

	return;
}

void mi_dsi_panel_commit_long_count(struct dsi_panel *panel, u32 value)
{
	char ch[64] = {0};

	panel->mi_count.commit_long_times =panel->mi_count.commit_long_times + value;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", panel->mi_count.commit_long_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, ch);

	return;
}

void mi_dsi_panel_count_init(struct dsi_panel *panel)
{
	int i = 0;
	char count_str[64] = {0};

	if (!strcmp(panel->type, "primary")) {
		disp_index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		disp_index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}
	global_panel[disp_index] = panel;

	memset(&panel->mi_count, 0, sizeof(struct mi_dsi_panel_count));

	if (strstr(panel->name, " 0a ")) {
		panel->mi_count.panel_id = 1;
	} else if (strstr(panel->name, " 0b "))
		panel->mi_count.panel_id = 2;
	else if (strstr(panel->name, " 0c "))
		panel->mi_count.panel_id = 3;
	else if (strstr(panel->name, " 0d "))
		panel->mi_count.panel_id = 4;
	else {
		pr_info("panel name: %s, use 5 as default, please check!\n", panel->name);
		panel->mi_count.panel_id = 5;
	}

	register_hw_monitor_info(HWMON_CONPONENT_NAME);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAILY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_DAILY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, "0");
	for (i = 0; i < FPS_MAX_NUM; i++)
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], "0");

	memset(count_str, 0, sizeof(count_str));
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_id);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PANEL_ID, count_str);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, "0");

	mi_dsi_panel_state_count(panel, PANEL_ACTIVE, 1);

	return;
}

int mi_dsi_panel_set_disp_count(struct dsi_panel *panel, const char *buf)
{
	char count_str[64] = {0};
	int i = 0;
	u64 panel_active = 0;
	u64 panel_doze_active = 0;
	u64 panel_daily_active = 0;
	u64 panel_doze_daily_active = 0;
	u64 kickoff_count = 0;
	u64 kernel_boottime = 0;
	u64 kernel_rtctime = 0;
	u64 kernel_days = 0;
	u64 record_end = 0;
	u32 delta_days = 0;
	u64 fps_times[FPS_MAX_NUM] = {0};
	u64 panel_id = 1;
	u64 poweron_cost_avg = 0;
	u64 esd_times = 0;
	u64 te_lost_times = 0;
	u64 underrun_times = 0;
	u64 overflow_times = 0;
	u64 pingpong_timeout_times = 0;
	u64 commit_long_times = 0;
	u64 cmdq_timeout_times = 0;
	ssize_t result;
	struct timespec64 rtctime;

	pr_info("[LCD] %s: begin\n", __func__);

	if (!panel) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	if (!strcmp(panel->type, "primary")) {
		disp_index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		disp_index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	result = sscanf(buf,
		"panel_active=%llu\n"
		"panel_doze_active=%llu\n"
		"panel_daily_active=%llu\n"
		"panel_doze_daily_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"fps1_times=%llu\n"
		"fps10_times=%llu\n"
		"fps24_times=%llu\n"
		"fps30_times=%llu\n"
		"fps40_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"panel_id=%llu\n"
		"poweron_cost_avg=%llu\n"
		"esd_times=%llu\n"
		"te_lost_times=%llu\n"
		"underrun_times=%llu\n"
		"overflow_times=%llu\n"
		"pingpong_timeout_times=%llu\n"
		"commit_long_times=%llu\n"
		"cmdq_timeout_times=%llu\n"
		"system_build_version=%s\n"
		"record_end=%llu\n",
		&panel_active,
		&panel_doze_active,
		&panel_daily_active,
		&panel_doze_daily_active,
		&kickoff_count,
		&kernel_boottime,
		&kernel_rtctime,
		&kernel_days,
		&fps_times[FPS_1],
		&fps_times[FPS_10],
		&fps_times[FPS_24],
		&fps_times[FPS_30],
		&fps_times[FPS_40],
		&fps_times[FPS_50],
		&fps_times[FPS_60],
		&fps_times[FPS_90],
		&fps_times[FPS_120],
		&fps_times[FPS_144],
		&panel_id,
		&poweron_cost_avg,
		&esd_times,
		&te_lost_times,
		&underrun_times,
		&overflow_times,
		&pingpong_timeout_times,
		&commit_long_times,
		&cmdq_timeout_times,
		stored_system_build_version,
		&record_end);

	if (result != 29) {
		pr_err("sscanf buf error!\n");
		return -EINVAL;
	}
#if 0
	if (panel_active < panel.panel_active) {
		pr_err("Current panel_active < panel_info.panel_active!\n");
		return -EINVAL;
	}

	if (kickoff_count < panel.kickoff_count) {
		pr_err("Current kickoff_count < panel_info.kickoff_count!\n");
		return -EINVAL;
	}
#endif

	ktime_get_real_ts64(&rtctime);

	if (rtctime.tv_sec > kernel_rtctime) {
		if (rtctime.tv_sec - kernel_rtctime > 10 * 365 * DAY_SECS) {
			panel->mi_count.bootRTCtime = rtctime.tv_sec;
		} else {
			if (rtctime.tv_sec - kernel_rtctime > DAY_SECS) {
				delta_days = (rtctime.tv_sec - kernel_rtctime) / DAY_SECS;
				panel->mi_count.bootRTCtime = rtctime.tv_sec - ((rtctime.tv_sec - kernel_rtctime) % DAY_SECS);
			} else {
				panel->mi_count.bootRTCtime = kernel_rtctime;
			}
		}
	} else {
		pr_err("RTC time rollback!\n");
		panel->mi_count.bootRTCtime = kernel_rtctime;
	}

	panel->mi_count.panel_active = panel_active;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, count_str);

	panel->mi_count.panel_doze_active = panel_doze_active;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_doze_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_ACTIVE, count_str);

	panel->mi_count.panel_daily_active = panel_daily_active;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_daily_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAILY_ACTIVE, count_str);

	panel->mi_count.panel_doze_daily_active = panel_doze_daily_active;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_doze_daily_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DOZE_DAILY_ACTIVE, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.kickoff_count = kickoff_count;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.kickoff_count/60);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.boottime = kernel_boottime;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.boottime);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.bootdays = kernel_days + delta_days;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.bootdays);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, count_str);

	for (i = 0; i < FPS_MAX_NUM; i++) {
		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.fps_times[i] = fps_times[i];
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.fps_times[i] / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], count_str);
	}

	if (panel_id != panel->mi_count.panel_id) {
		pr_err("panel id changed from %llu to %llu\n", panel_id, panel->mi_count.panel_id);
	}

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.poweron_cost_avg = poweron_cost_avg;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.poweron_cost_avg);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, count_str);

	if (strcmp(stored_system_build_version, "null") && strcmp(system_build_version, "null")
			&& strcmp(stored_system_build_version, system_build_version)) {
		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.esd_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.te_lost_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.underrun_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.overflow_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.pingpong_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.commit_long_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.cmdq_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	} else {
		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.esd_times = esd_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.te_lost_times = te_lost_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.underrun_times = underrun_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.overflow_times = overflow_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.pingpong_timeout_times = pingpong_timeout_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.commit_long_times = commit_long_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.cmdq_timeout_times = cmdq_timeout_times;
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	}

	pr_info("[LCD] %s: end\n", __func__);

	return 0;
}

ssize_t mi_dsi_panel_get_disp_count(struct dsi_panel *panel,
			char *buf, size_t size)
{
	int ret = -1;
	ktime_t boot_time;
	u64 record_end = 0;
	/* struct timespec64 rtctime; */

	if (!panel) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		pr_err("dsi_panel_disp_count_get buffer is NULL!\n");
		return -EINVAL;
	}

	boot_time = ktime_get_boottime();
	do_div(boot_time, NSEC_PER_SEC);
	/* ktime_get_real_ts64(&rtctime); */

	ret = scnprintf(buf, size,
		"panel_active=%llu\n"
		"panel_doze_active=%llu\n"
		"panel_daily_active=%llu\n"
		"panel_doze_daily_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"fps1_times=%llu\n"
		"fps10_times=%llu\n"
		"fps24_times=%llu\n"
		"fps30_times=%llu\n"
		"fps40_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"panel_id=%llu\n"
		"poweron_cost_avg=%llu\n"
		"esd_times=%llu\n"
		"te_lost_times=%llu\n"
		"underrun_times=%llu\n"
		"overflow_times=%llu\n"
		"pingpong_timeout_times=%llu\n"
		"commit_long_times=%llu\n"
		"cmdq_timeout_times=%llu\n"
		"system_build_version=%s\n"
		"record_end=%llu\n",
		panel->mi_count.panel_active,
		panel->mi_count.panel_doze_active,
		panel->mi_count.panel_daily_active,
		panel->mi_count.panel_doze_daily_active,
		panel->mi_count.kickoff_count,
		panel->mi_count.boottime + boot_time,
		panel->mi_count.bootRTCtime,
		panel->mi_count.bootdays,
		panel->mi_count.fps_times[FPS_1],
		panel->mi_count.fps_times[FPS_10],
		panel->mi_count.fps_times[FPS_24],
		panel->mi_count.fps_times[FPS_30],
		panel->mi_count.fps_times[FPS_40],
		panel->mi_count.fps_times[FPS_50],
		panel->mi_count.fps_times[FPS_60],
		panel->mi_count.fps_times[FPS_90],
		panel->mi_count.fps_times[FPS_120],
		panel->mi_count.fps_times[FPS_144],
		panel->mi_count.panel_id,
		panel->mi_count.poweron_cost_avg,
		panel->mi_count.esd_times,
		panel->mi_count.te_lost_times,
		panel->mi_count.underrun_times,
		panel->mi_count.overflow_times,
		panel->mi_count.pingpong_timeout_times,
		panel->mi_count.commit_long_times,
		panel->mi_count.cmdq_timeout_times,
		system_build_version,
		record_end);

	return ret;
}
