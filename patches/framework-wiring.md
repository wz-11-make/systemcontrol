# 骨架之外必须改的三处

`skeleton` 本身能独立编译,但要让它真的跑起来(服务被注册、被 system_server 加载、
policy 进到镜像里),需要在 `vendor/agent/` 之外改三处。

**这些改动已经应用了**,并且导出成本目录下的 `.patch` 文件。
`repo sync` 之后跑一次:

```bash
bash vendor/agent/patches/apply.sh
```

脚本是幂等的:已打过会跳过,有冲突会提示。

---

## 1. `frameworks/base` — `0001-frameworks-base-agent-service.patch`

两处:

- `services/Android.bp`:`services.impl` 的 `static_libs` 加 `"agent-service"`
- `services/java/com/android/server/SystemServer.java`:
  import + 在 `startOtherServices()` 末尾 `startService(AgentService.class)`

放在 `startOtherServices` 末尾是因为它依赖 AMS / PMS 已就绪。

## 2. `device/xiaomi/venus` — `0002-device-venus-product.patch`

`device.mk` 末尾:

```makefile
$(call inherit-product-if-exists, vendor/agent/agent.mk)
```

## 3. `device/xiaomi/sm8350-common` — `0003-device-sm8350-sepolicy.patch`

`BoardConfigCommon.mk`:

```makefile
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += vendor/agent/sepolicy
```

---

## ⚠️ 重要:必须刷两个镜像

policy 走的是 `SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS`,所以它编译进的是
**system_ext 分区**,不是 system。

```bash
m systemimage        # system/bin/agentd、Agent.apk、services.jar
m systemextimage     # SELinux 策略(system_ext_sepolicy.cil)
```

只刷 `system.img` 的话,`agentd` 会在 SELinux 层面起不来 ——
而且现象是「进程闪一下就没了」,`service list` 里什么也没有,很难查。

## 验证

```bash
adb root && adb shell
service list | grep -E 'agent$|agent_core'
getprop | grep -i agent   # 应该没有
dmesg | grep avc          # 应该没有 agentd 相关
```

两个服务都在 = 接线成功。

## 坑位记录

**`coredomain` + `/data/misc/*` 必须带 `core_data_file_type`**

```te
# 错:会被 neverallow 拦(domain.te:1069/1091)
type agent_data_file, file_type, data_file_type;

# 对
type agent_data_file, file_type, data_file_type, core_data_file_type;
```

`full_treble_only` 的 neverallow 规定:coredomain 只能访问带
`core_data_file_type`(或 `app_data_file_type`)的 `/data` 类型。
