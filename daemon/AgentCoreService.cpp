#include "AgentCoreService.h"

#include <android-base/logging.h>

#include <utility>

AgentCoreService::AgentCoreService()
      : mWorker(new Worker()), mEngine(Engine::create()) {
    mWorker->start("agent-infer");
    LOG(INFO) << "engine=" << mEngine->name();
}

AgentCoreService::~AgentCoreService() {
    if (mWorker) mWorker->stop();
}

// ---------- 小工具 ----------

bool AgentCoreService::accept(int64_t requestId) {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCancelled.find(requestId) == mCancelled.end();
}

void AgentCoreService::release(int64_t requestId) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCancelled.erase(requestId);
}

bool AgentCoreService::cancelled(int64_t requestId) {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCancelled.find(requestId) != mCancelled.end();
}

void AgentCoreService::recordError(int32_t code) {
    mLastError.store(code);
}

void AgentCoreService::onEnterWorker() {
    mState.store(2);
}

void AgentCoreService::onLeaveWorker() {
    mState.store(mWorker->depth() > 0 ? 2 : 1);
}

// ---------- 状态 ----------

::ndk::ScopedAStatus AgentCoreService::getVersion(int32_t* _aidl_return) {
    *_aidl_return = 1;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::getStatus(asa::AgentStatus* _aidl_return) {
    _aidl_return->version = 1;
    _aidl_return->state = mState.load();
    _aidl_return->modelState = mEngine->ready() ? 2 : 0;
    _aidl_return->queueDepth = mWorker->depth();
    _aidl_return->modelBytes = mEngine->modelBytes();
    _aidl_return->lastErrorCode = mLastError.load();
    return ::ndk::ScopedAStatus::ok();
}

// ---------- 连通性 ----------

::ndk::ScopedAStatus AgentCoreService::ping(
        int64_t in_requestId, const std::string& in_message, const Cb& in_cb) {
    LOG(INFO) << "ping id=" << in_requestId << " msg=" << in_message;
    if (in_cb != nullptr) {
        (void)in_cb->onPong(in_requestId, "agentd/" + std::string(mEngine->name()) +
                                            " ok: " + in_message);
    }
    return ::ndk::ScopedAStatus::ok();
}

// ---------- 模型生命周期 ----------

::ndk::ScopedAStatus AgentCoreService::loadModel(
        int64_t in_requestId, const std::string& in_modelDir, const Cb& in_cb) {
    if (in_modelDir.empty()) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!accept(in_requestId)) {
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrCancelled);
    }

    const bool queued = mWorker->post([this, in_requestId, in_modelDir, in_cb] {
        onEnterWorker();
        const int32_t rc = mEngine->load(in_modelDir);
        if (rc == kOk) {
            mModelBytes.store(mEngine->modelBytes());
        } else {
            recordError(rc);
        }
        if (in_cb != nullptr) {
            (void)in_cb->onModelLoaded(in_requestId, rc,
                                       rc == kOk ? "loaded" : "load failed");
        }
        release(in_requestId);
        onLeaveWorker();
    });

    if (!queued) {
        release(in_requestId);
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrNotReady);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::unloadModel() {
    // 卸载可能要释放 GB 级内存,同样丢给 worker,别占 binder 线程
    mWorker->post([this] {
        onEnterWorker();
        mEngine->unload();
        mModelBytes.store(0);
        onLeaveWorker();
    });
    return ::ndk::ScopedAStatus::ok();
}

// ---------- 推理 ----------

::ndk::ScopedAStatus AgentCoreService::transcribe(
        int64_t in_requestId, const std::vector<uint8_t>& in_pcm,
        int32_t in_sampleRate, const Cb& in_cb) {
    if (in_sampleRate <= 0) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!accept(in_requestId)) {
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrCancelled);
    }

    // 拷贝一份:调用方的 buffer 在 binder 事务结束后就没了
    std::vector<uint8_t> pcm = in_pcm;
    const bool queued = mWorker->post(
            [this, in_requestId, pcm = std::move(pcm), in_sampleRate, in_cb] {
                onEnterWorker();
                const TranscriptResult r = mEngine->transcribe(pcm, in_sampleRate);
                if (in_cb != nullptr) {
                    if (r.code == kOk) {
                        (void)in_cb->onTranscript(in_requestId, r.text, true);
                    } else {
                        recordError(r.code);
                        (void)in_cb->onError(in_requestId, r.code, "transcribe failed");
                    }
                }
                release(in_requestId);
                onLeaveWorker();
            });

    if (!queued) {
        release(in_requestId);
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrNotReady);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::parseIntent(
        int64_t in_requestId, const std::string& in_text,
        const std::string& in_contextJson, const Cb& in_cb) {
    if (in_text.empty()) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!accept(in_requestId)) {
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrCancelled);
    }

    const bool queued = mWorker->post(
            [this, in_requestId, in_text, in_contextJson, in_cb] {
                onEnterWorker();
                const IntentResult r = mEngine->parseIntent(in_text, in_contextJson);
                if (in_cb != nullptr) {
                    if (r.code == kOk) {
                        (void)in_cb->onIntent(in_requestId, r.intentJson, r.confidence);
                    } else {
                        recordError(r.code);
                        (void)in_cb->onError(in_requestId, r.code, "parseIntent failed");
                    }
                }
                release(in_requestId);
                onLeaveWorker();
            });

    if (!queued) {
        release(in_requestId);
        return ::ndk::ScopedAStatus::fromServiceSpecificError(kErrNotReady);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus AgentCoreService::cancel(int64_t in_requestId) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCancelled.insert(in_requestId);
    }
    LOG(INFO) << "cancel id=" << in_requestId << " depth=" << mWorker->depth();
    return ::ndk::ScopedAStatus::ok();
}
