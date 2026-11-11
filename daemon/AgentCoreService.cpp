#include "AgentCoreService.h"

#include <android-base/logging.h>

namespace asa = ::aidl::android::system::agent;

::ndk::ScopedAStatus AgentCoreService::getVersion(int32_t* _aidl_return) {
    *_aidl_return = 1;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::getStatus(asa::AgentStatus* _aidl_return) {
    _aidl_return->version = 1;
    _aidl_return->state = mEngine.ready() ? 2 : 1;
    _aidl_return->modelState = mEngine.ready() ? 2 : 0;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::ping(
        int64_t in_requestId, const std::string& in_message,
        const std::shared_ptr<asa::IAgentCoreCallback>& in_cb) {
    LOG(INFO) << "ping requestId=" << in_requestId << " message=" << in_message;
    if (in_cb != nullptr) {
        in_cb->onPong(in_requestId, "agentd ok: " + in_message);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::cancel(int64_t in_requestId) {
    mLastCancelled = in_requestId;
    LOG(INFO) << "cancel requestId=" << in_requestId;
    return ::ndk::ScopedAStatus::ok();
}
