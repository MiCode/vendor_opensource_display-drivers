load(":display_drivers_auto/display_modules_auto.bzl", "display_driver_modules")
load(":display_drivers_auto/display_driver_build_auto.bzl", "define_target_variant_modules")
load("//msm-kernel:target_variants.bzl", "get_all_la_variants", "get_all_le_variants", "get_all_lxc_variants")

def define_pineapple_auto(t,v):
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
              "CONFIG_DRM_SDE_SHD",
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
              "CONFIG_MI_DISP_AUTO",
          ],
      )

