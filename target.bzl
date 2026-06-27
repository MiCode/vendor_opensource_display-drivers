load(":display_modules.bzl", "display_driver_modules")
load(":display_driver_build.bzl", "define_consolidate_gki_modules")

def define_monaco():
    define_consolidate_gki_modules(
        target = "monaco",
        registry = display_driver_modules,
        modules = [
            "msm_drm",
        ],
        config_options = [
            "CONFIG_DRM_MSM_SDE",
            "CONFIG_SYNC_FILE",
            "CONFIG_DRM_MSM_DSI",
            "CONFIG_DRM_MSM_REGISTER_LOGGING",
            "CONFIG_QCOM_MDSS_PLL",
            "CONFIG_THERMAL_OF",
        ],
#-------To add deps in other target-----------
#       deps = [
#       "//vendor/qcom/opensource/mm-drivers/hw_fence:%b_msm_hw_fence",
#       ]
        deps_l = [],
)
