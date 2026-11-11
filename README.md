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

## daemon/ 内部结构(推理服务)

```
main.cpp              addService("agent_core");binder 线程池 = 4
AgentCoreService.*    binder 层:参数校验 / 入队 / 取消表 / 回调派发
Worker.*              单线程串行队列。binder 线程只入队,立刻返回
Engine.h              抽象接口,纯 C++,不认识 binder/JNI/Android
MockEngine.*          规则引擎,产出符合契约的意图 JSON(当前默认)
LlamaEngine.*         llama.cpp 实现,pimpl 隔离,-DAGENT_WITH_LLAMA 切换
```

### 数据流

```
Agent.apk            AgentService              agentd
   │                     │                       │
   │ processText(id,txt) │                       │
   ├────────────────────►│                       │
   │                     │ parseIntent(id,txt)   │
   │                     ├──────────────────────►│  Worker 线程
   │                     │                       │  ┌──────────────┐
   │                     │                       │  │MockEngine /  │
   │                     │                       │  │LlamaEngine   │
   │                     │                       │  └──────┬───────┘
   │                     │   onIntent(id, json)  │◄────────┘
   │  onResult(id, json) │◄──────────────────────┤
   │◄────────────────────┤                       │
```

### 为什么 binder 层必须入队

`parseIntent` 可能要跑几秒(尤其接了 1.5B 模型)。
如果在 binder 线程里直接跑:

- 4 个 binder 线程会被占满,AgentService 侧全部超时
- 没法 `cancel()` —— 调用已经被卡住了

所以 binder 方法只做「校验 + post」,立刻返回;真正的推理在 worker 上串行跑。

### 取消

`cancel(id)` 只是往 `mCancelled` 里插一条。worker 在**开始执行前**检查,
已取消就直接跳过并清理。接 llama.cpp 之后要再加 abort callback 支持中途打断。

### 切换引擎

`daemon/Android.bp` 里:

```bp
cflags: ["-DAGENT_WITH_LLAMA"],
shared_libs: ["libllama", "libggml"],
```

其余代码零改动。`MockEngine` 里那张关键字表可以保留当"快速路径"
(规则命中就不必惊动模型)。

### 当前的能力边界

| 能力 | 状态 |
|---|---|
| 意图分类 | ✅ MockEngine 规则版可用 |
| 意图分类(真模型) | ⏸ `LlamaEngine::parseIntent` 待接 grammar 约束采样 |
| 语音识别 | ❌ `kErrUnsupported`(sherpa-onnx 未接) |
| 取消 | ✅ 入队前生效 |
| 中途打断 | ❌ 需要 llama.cpp abort callback |

## 外部接线(已应用,可通过 patch 重放)

三处改动在 `vendor/agent/` 之外,已应用并导出成 patch:

```
patches/
├── apply.sh                                        repo sync 后重放
├── 0001-frameworks-base-agent-service.patch        services.impl + SystemServer
├── 0002-device-venus-product.patch                 device.mk inherit
├── 0003-device-sm8350-sepolicy.patch               BOARD policy dir
└── framework-wiring.md                             说明 + 坑位记录
```

```bash
bash vendor/agent/patches/apply.sh     # 幂等
```

### ⚠️ 必须刷两个镜像

policy 走 `SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS`,编译进 **system_ext 分区**:

```bash
m systemimage        # system/bin/agentd、Agent.apk、services.jar
m systemextimage     # SELinux 策略
```

只刷 `system.img` → `agentd` 会被 SELinux 拦住,进程闪一下就没,`service list` 里空无一物。

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

### 2026-11 第二轮:推理服务写完

```
m android.system.agent.control-ndk android.system.agent.control-java   ✅
m agentd                                                               ✅ (54 targets)
m agent-service                                                        ✅
m Agent                                                                ✅
```

新增/改动:

- `IAgentCore` 增加 `loadModel / unloadModel / transcribe / parseIntent`
- `AgentStatus` 增加 `queueDepth / modelBytes / lastErrorCode`
- `daemon/` 从 3 个文件扩到 9 个:Worker、Engine 抽象、Mock、Llama
- `AgentService` 真正连上 `agent_core`,带死亡通知与重连

### 2026-11 第三轮:接线完成 + 镜像产出

```
m services            ✅  services.jar 内含 com/android/server/agent/AgentService
m systemimage         ✅  system.img          1.05 GB
m systemextimage      ✅  system_ext.img     448 MB(含 agent SELinux 策略)
```

刷机后验证:

```bash
adb shell service list | grep -E 'agent$|agent_core'
```

两行都在 = 三层全部注册成功。

**踩到的坑:`coredomain` 访问 `/data/misc/*` 必须给类型加 `core_data_file_type`**,
否则 `full_treble_only` 的 neverallow(domain.te:1069/1091)直接拦。

```te
type agent_data_file, file_type, data_file_type, core_data_file_type;
```
