# vendor/agent —— 端侧 Agent 三层骨架

```
Agent.apk (UI)  ──IAgentService──►  AgentService (system_server)
                                        │
                                        └──IAgentCore──►  agentd (native)
```

## 目录

| 路径 | 层 | 产物 |
|---|---|---|
| `aidl/` | 三方唯一共享契约 | `android.system.agent.control-java/-ndk` |
| `app/` | UI | `/system/priv-app/Agent/` |
| `service/` | 动作执行(system_server) | 编进 `services.jar` |
| `daemon/` | 推理 | `/system/bin/agentd` |
| `sepolicy/` | SELinux | `/system/etc/selinux/` |

## 现在还缺的三处外部接线

骨架本身能编(`m Agent` / `m agentd` / `m android.system.agent.control-ndk`),
但要让 `AgentService` 真的跑起来,需要动三处:

### 1. `frameworks/base/services/Android.bp`

在 `services.impl` 的 `static_libs` 里加一行:

```
"agent-service",
```

### 2. `frameworks/base/services/java/com/android/server/SystemServer.java`

在 `startOtherServices()` 里合适位置加:

```java
mSystemServiceManager.startService(AgentService.class);
```
并加 import:

```java
import com.android.server.agent.AgentService;
```

### 3. `device/xiaomi/venus/device.mk` 末尾

```makefile
$(call inherit-product-if-exists, vendor/agent/agent.mk)
```

以及 `device/xiaomi/sm8350-common/BoardConfigCommon.mk`:

```makefile
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += vendor/agent/sepolicy
```

## 构建顺序(别跳)

| 步 | 命令 | 验收 |
|---|---|---|
| 1 | `m android.system.agent.control-ndk android.system.agent.control-java` | AIDL 编译通过 |
| 2 | `m agentd` | 产 `/system/bin/agentd` |
| 3 | 接上面第 1、2 处,`m services` | 编过 |
| 4 | 刷 system.img,重启 | `service list \| grep -E 'agent$|agent_core'` 两条都有 |
| 5 | `m Agent` | 装到 `/system/priv-app/Agent/` |
| 6 | 打开 Agent(默认 HOME) | 界面显示 `AgentService v1 / state=1` 和 skeleton 回显 |
| 7 | 修 sepolicy | `dmesg \| grep avc` 无输出 |

## 调试

```bash
adb logcat -s AgentService Agent.UI agentd
adb shell service list | grep agent
adb shell dmesg | grep avc
```

`agentd` 启动失败时:

```bash
adb shell logcat | grep -i agentd
adb shell dmesg | grep -E 'agentd|avc'
```

## 下一步

骨架通了之后,按这个顺序长功能:

1. `AgentService` 里补 `AgentActionRegistry`(actionId → 风险等级 + 执行器)
2. `agentd` 里把 `Engine` 换成 llama.cpp / sherpa-onnx
3. `IAgentCore` 加 `transcribe` / `parseIntent`
4. `Agent.apk` 接麦克风 + 确认弹窗

---

## 已验证(2026-11)

下面四个模块都已在本机构建通过:

```
m android.system.agent.control-ndk android.system.agent.control-java   ✅
m agentd                                                               ✅
m Agent                                                                ✅
m agent-service                                                        ✅
```

产物:

```
system/bin/agentd                              101 KB
system/priv-app/Agent/Agent.apk                 25 KB
system/framework/android.system.agent.control-java.jar
system/lib64/android.system.agent.control-ndk.so
system/etc/init/agentd.rc
```

## 两个 AIDL 生成的坑(写 C++ 时会撞)

**1. 头文件路径多一层 `aidl/`**

因为 `unstable: true`,生成的头文件在 `gen/include/aidl/android/system/agent/`,所以:

```cpp
#include <aidl/android/system/agent/BnAgentCore.h>   // ✅
#include <android/system/agent/BnAgentCore.h>        // ❌ 找不到
```

**2. 命名空间是 `::aidl::` 前缀**

```cpp
namespace asa = ::aidl::android::system::agent;      // ✅
namespace asa = ::android::system::agent;            // ❌
```

两者合起来就容易记了:**`::aidl::` 命名空间 + `aidl/` 头文件前缀。**

## 生成的 C++ 方法签名(供参考)

```cpp
virtual ::ndk::ScopedAStatus getVersion(int32_t* _aidl_return) = 0;
virtual ::ndk::ScopedAStatus getStatus(
        ::aidl::android::system::agent::AgentStatus* _aidl_return) = 0;
virtual ::ndk::ScopedAStatus ping(int64_t in_requestId,
        const std::string& in_message,
        const std::shared_ptr<::aidl::android::system::agent::IAgentCoreCallback>& in_cb) = 0;
virtual ::ndk::ScopedAStatus cancel(int64_t in_requestId) = 0;
```
