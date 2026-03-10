load(":display_modules.bzl", "display_driver_modules")
load(":display_driver_build.bzl", "define_target_variant_modules")
load("//msm-kernel:target_variants.bzl", "get_all_la_variants", "get_all_le_variants", "get_all_lxc_variants", "get_arch_of_target")

def define_pineapple(t, v):
    define_target_variant_modules(
        target = t,
        variant = v,
        registry = display_driver_modules,
        modules = [
            "msm_drm",
        ],
        config_options = [
            "CONFIG_DRM_MSM_SDE",
            "CONFIG_SYNC_FILE",
            "CONFIG_DRM_MSM_DSI",
            "CONFIG_DRM_MSM_DP",
            "CONFIG_DRM_MSM_DP_MST",
            "CONFIG_DSI_PARSER",
            "CONFIG_DRM_SDE_WB",
            "CONFIG_DRM_SDE_RSC",
            "CONFIG_DRM_MSM_REGISTER_LOGGING",
            "CONFIG_QCOM_MDSS_PLL",
            "CONFIG_HDCP_QSEECOM",
            "CONFIG_DRM_SDE_VM",
            "CONFIG_QCOM_WCD939X_I2C",
            "CONFIG_THERMAL_OF",
            "CONFIG_MSM_MMRM",
            "CONFIG_QTI_HW_FENCE",
            "CONFIG_QCOM_SPEC_SYNC",
            "CONFIG_MSM_EXT_DISPLAY",
            "CONFIG_MI_DISP_ERPPANEL_INFO",
            "MI_DISPLAY_MODIFY",
            "MULTI_TIMING_MODIFY",
            "LONG_PRESS_TO_FORCE_RESTART",
        ],
)

def define_blair(t, v):
    define_target_variant_modules(
        target = t,
        variant = v,
        registry = display_driver_modules,
        modules = [
            "msm_drm",
        ],
         config_options = [
            "CONFIG_DRM_MSM_SDE",
            "CONFIG_DRM_MSM_DSI",
            "CONFIG_THERMAL_OF",
            "CONFIG_DSI_PARSER",
            "CONFIG_DRM_MSM_REGISTER_LOGGING",
            "CONFIG_QCOM_MDSS_PLL",
            "CONFIG_MSM_SDE_ROTATOR",
            "CONFIG_SYNC_FILE",
            "CONFIG_MSM_SDE_ROTATOR_EVTLOG_DEBUG",
            "CONFIG_DEBUG_FS",
            "CONFIG_MI_DISP_ERPPANEL_INFO",
            "MI_DISPLAY_MODIFY",
            "MULTI_TIMING_MODIFY",
            "LONG_PRESS_TO_FORCE_RESTART",
        ],
)

def define_display_target():
    pairs = []
    for (t, v) in get_all_la_variants() + get_all_le_variants() + get_all_lxc_variants():
        arch = get_arch_of_target(t)
        if arch == t:
            pairs.append((t, v))
    for t, v in pairs:
        if t == "blair":
            define_blair(t, v)
        else:
            define_pineapple(t, v)
