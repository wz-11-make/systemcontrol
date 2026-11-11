#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <memory>

#include "AgentCoreService.h"

static constexpr char kServiceName[] = "agent_core";

int main() {
    android::base::InitLogging(nullptr, android::base::StderrLogger);

    // binder 线程池只负责收发,推理全在 AgentCoreService 的 worker 线程上
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    std::shared_ptr<AgentCoreService> service =
            ::ndk::SharedRefBase::make<AgentCoreService>();

    const binder_status_t status = AServiceManager_addService(
            service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        // 不要 abort —— init 会自动重启,只是别让它刷屏
        LOG(ERROR) << "AServiceManager_addService(" << kServiceName
                   << ") failed: " << static_cast<int>(status);
        return 1;
    }

    LOG(INFO) << "agentd registered as '" << kServiceName << "'";

    ABinderProcess_joinThreadPool();
    return 0;
}
