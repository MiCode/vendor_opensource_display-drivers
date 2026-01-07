load(":display_modules.bzl", "display_driver_modules")
load(":display_driver_build.bzl", "define_target_variant_modules")
load("//msm-kernel:target_variants.bzl", "get_all_la_variants", "get_arch_of_target")

def define_parrot():
    pairs = []
    for (t, v) in get_all_la_variants():
        arch = get_arch_of_target(t)
        if arch == t:
            pairs.append((t, v))
    for (t, v) in pairs:
        if t == "parrot":
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
            #"CONFIG_DRM_MSM_DP_MST",
            "CONFIG_DSI_PARSER",
            "CONFIG_DRM_SDE_WB",
            "CONFIG_DRM_SDE_RSC",
            "CONFIG_DRM_MSM_REGISTER_LOGGING",
            "CONFIG_QCOM_MDSS_PLL",
            "CONFIG_HDCP_QSEECOM",
            "CONFIG_DRM_SDE_VM",
            "CONFIG_THERMAL_OF",
            #"CONFIG_MSM_MMRM",
            "CONFIG_MSM_EXT_DISPLAY",
            "CONFIG_QCOM_FSA4480_I2C",
            "CONFIG_DYNAMIC_DEBUG",
            "CONFIG_MSM_SDE_ROTATOR",
            "CONFIG_MSM_SDE_ROTATOR_EVTLOG_DEBUG",
            "CONFIG_DEBUG_FS",
            "MI_DISPLAY_MODIFY",
            "LONG_PRESS_TO_FORCE_FASTBOOT",
        ],
)
