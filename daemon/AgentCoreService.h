#pragma once
#include <aidl/android/system/agent/BnAgentCore.h>

#include <memory>
#include <string>

#include "Engine.h"

class AgentCoreService : public ::aidl::android::system::agent::BnAgentCore {
public:
    AgentCoreService() = default;
    ~AgentCoreService() override = default;

    ::ndk::ScopedAStatus getVersion(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getStatus(
            ::aidl::android::system::agent::AgentStatus* _aidl_return) override;
    ::ndk::ScopedAStatus ping(
            int64_t in_requestId, const std::string& in_message,
            const std::shared_ptr<::aidl::android::system::agent::IAgentCoreCallback>& in_cb)
            override;
    ::ndk::ScopedAStatus cancel(int64_t in_requestId) override;

private:
    Engine mEngine;
    int64_t mLastCancelled = -1;
};
