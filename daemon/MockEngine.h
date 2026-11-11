#pragma once
#include "Engine.h"

// 没有 llama.cpp / sherpa-onnx 时的实现。
//
// 它不是"假的" —— 它用规则真的产出一份符合契约的意图 JSON,
// 让 UI -> AgentService -> agentd 的整条链路今天就能跑通。
// 接真模型时,IAgentCore 的接口一个字都不用改。
class MockEngine final : public Engine {
public:
    int32_t load(const std::string& dir) override;
    void unload() override;
    bool ready() const override { return mReady; }
    int32_t modelBytes() const override { return mReady ? mBytes : 0; }
    const char* name() const override { return "mock"; }

    IntentResult parseIntent(const std::string& text,
                             const std::string& contextJson) override;
    TranscriptResult transcribe(const std::vector<uint8_t>& pcm,
                                int32_t sampleRate) override;

private:
    bool mReady = false;
    int32_t mBytes = 0;
};
