# 骨架之外必须改的三处

`skeleton` 本身能独立编译,但要让它真的跑起来(服务被注册、被 system_server 加载),
需要在 `vendor/agent/` 之外改三处。这三处属于 **LineageOS 主树**,不在本仓库里,
所以记录在这里,`repo sync` 后按需重新应用。

---

## 1. `frameworks/base/services/Android.bp`

在 `services.impl` 的 `static_libs` 列表里加一行:

```bp
    static_libs: [
        "services.core",
+       "agent-service",
        "services.accessibility",
        ...
```

作用:把 `agent-service` 这个 `java_library` 编进 `services.jar`,
这样 system_server 的 classloader 才能加载 `AgentService`。

---

## 2. `frameworks/base/services/java/com/android/server/SystemServer.java`

加 import:

```java
import com.android.server.agent.AgentService;
```

在 `startOtherServices()` 里合适位置(建议靠近其他自研服务)加:

```java
mSystemServiceManager.startService(AgentService.class);
```

⚠️ 不要放在 `mActivityManagerService` 初始化之前 —— `AgentService` 的动作执行器
依赖 AMS 就绪。

---

## 3. device tree

`device/xiaomi/venus/device.mk` 末尾:

```makefile
$(call inherit-product-if-exists, vendor/agent/agent.mk)
```

`device/xiaomi/sm8350-common/BoardConfigCommon.mk`:

```makefile
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += vendor/agent/sepolicy
```

---

## 验证顺序

```bash
m android.system.agent.control-ndk agentd Agent agent-service   # 都应通过
m services                                                       # 确认接线没问题
mka bacon
```

刷机后:

```bash
adb shell service list | grep -E 'agent$|agent_core'
```

两行都要有。只有一行说明另一层的 sepolicy 或注册代码有问题。
