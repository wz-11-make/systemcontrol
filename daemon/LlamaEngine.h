#pragma once
#include <memory>

#include "Engine.h"

// llama.cpp 实现。用 pimpl 隔离 API,这样本头文件不需要 llama.h。
//
// 编译开关:在 Android.bp 的 cflags 里加 -DAGENT_WITH_LLAMA 并补上
// shared_libs: ["libllama"] 即可切换,其余代码零改动。
class LlamaEngine final : public Engine {
public:
    LlamaEngine();
    ~LlamaEngine() override;

    int32_t load(const std::string& dir) override;
    void unload() override;
    bool ready() const override;
    int32_t modelBytes() const override;
    const char* name() const override;

    IntentResult parseIntent(const std::string& text,
                             const std::string& contextJson) override;
    TranscriptResult transcribe(const std::vector<uint8_t>& pcm,
                                int32_t sampleRate) override;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
