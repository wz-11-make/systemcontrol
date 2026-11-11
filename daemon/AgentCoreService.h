#pragma once
#include <aidl/android/system/agent/BnAgentCore.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "Engine.h"
#include "Worker.h"

namespace asa = ::aidl::android::system::agent;

// binder 层。只做三件事:
//   1. 参数校验 + 入队(绝不在这里跑推理)
//   2. 维护取消表
//   3. 把 worker 的结果通过 oneway 回调送回去
class AgentCoreService : public asa::BnAgentCore {
public:
    AgentCoreService();
    ~AgentCoreService() override;

    ::ndk::ScopedAStatus getVersion(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getStatus(asa::AgentStatus* _aidl_return) override;

    ::ndk::ScopedAStatus ping(int64_t in_requestId, const std::string& in_message,
                              const std::shared_ptr<asa::IAgentCoreCallback>& in_cb) override;

    ::ndk::ScopedAStatus loadModel(int64_t in_requestId, const std::string& in_modelDir,
                                   const std::shared_ptr<asa::IAgentCoreCallback>& in_cb) override;
    ::ndk::ScopedAStatus unloadModel() override;

    ::ndk::ScopedAStatus transcribe(int64_t in_requestId, const std::vector<uint8_t>& in_pcm,
                                    int32_t in_sampleRate,
                                    const std::shared_ptr<asa::IAgentCoreCallback>& in_cb) override;

    ::ndk::ScopedAStatus parseIntent(int64_t in_requestId, const std::string& in_text,
                                     const std::string& in_contextJson,
                                     const std::shared_ptr<asa::IAgentCoreCallback>& in_cb) override;

    ::ndk::ScopedAStatus cancel(int64_t in_requestId) override;

private:
    using Cb = std::shared_ptr<asa::IAgentCoreCallback>;

    // 入队前检查:false = 已被取消,不要执行
    bool accept(int64_t requestId);
    // 请求结束(正常或取消),清理取消表
    void release(int64_t requestId);
    bool cancelled(int64_t requestId);

    void recordError(int32_t code);
    void onEnterWorker();
    void onLeaveWorker();

    std::unique_ptr<Worker> mWorker;
    std::unique_ptr<Engine> mEngine;

    mutable std::mutex mMutex;
    std::set<int64_t> mCancelled;

    std::atomic<int32_t> mState{1};
    std::atomic<int32_t> mLastError{0};
    std::atomic<int32_t> mModelBytes{0};
};
