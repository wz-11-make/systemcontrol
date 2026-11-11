#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 错误码,跨层共享(AgentService 侧有同名常量)
enum AgentError : int32_t {
    kOk = 0,
    kErrNotReady = 1,
    kErrCancelled = 2,
    kErrInvalidArg = 3,
    kErrModelLoad = 4,
    kErrInference = 5,
    kErrUnsupported = 6,
};

struct IntentResult {
    int32_t code = kErrInference;
    std::string intentJson;   // {"actionId":..,"params":..,"confidence":..}
    float confidence = 0.0f;
};

struct TranscriptResult {
    int32_t code = kErrInference;
    std::string text;
};

// 推理引擎抽象。
//
// 硬性约束:
//  - 纯 C++,不知道 binder / JNI / Android 的存在
//  - 不抛异常(平台默认 -fno-exceptions)
//  - 所有方法只会在单个 worker 线程里被调用,不需要内部加锁
class Engine {
public:
    virtual ~Engine() = default;

    // 加载模型。dir 是模型目录,例如 /data/misc/agent/models
    virtual int32_t load(const std::string& dir) = 0;
    virtual void unload() = 0;
    virtual bool ready() const = 0;
    virtual int32_t modelBytes() const = 0;
    virtual const char* name() const = 0;

    // 文本 -> 意图 JSON。contextJson 可为空。
    virtual IntentResult parseIntent(const std::string& text,
                                     const std::string& contextJson) = 0;

    // 整段 PCM(16-bit LE,单声道) -> 文本
    virtual TranscriptResult transcribe(const std::vector<uint8_t>& pcm,
                                        int32_t sampleRate) = 0;

    static std::unique_ptr<Engine> create();
};
