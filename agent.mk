# SPDX-License-Identifier: Apache-2.0

# 三层:UI / 动作执行(system_server) / 推理(agentd)
PRODUCT_PACKAGES += \
    Agent \
    agentd

# Desktop Windowing:绕过 Mi 11 的 523dp < 600dp 大屏门槛
# (见方案文档 §4.3;不确定就先注释掉)
PRODUCT_PROPERTY_OVERRIDES += \
    persist.wm.debug.desktop_mode_enforce_device_restrictions=false
