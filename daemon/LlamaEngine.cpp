#include "LlamaEngine.h"

#include <android-base/logging.h>

#ifdef AGENT_WITH_LLAMA

#include <llama.h>

#include <mutex>
#include <string>
#include <vector>

struct LlamaEngine::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    int32_t bytes = 0;
    std::mutex samplerMutex;

    ~Impl() { reset(); }

    void reset() {
        if (ctx != nullptr) {
            llama_free(ctx);
            ctx = nullptr;
        }
        if (model != nullptr) {
            llama_model_free(model);
            model = nullptr;
        }
        bytes = 0;
    }
};

LlamaEngine::LlamaEngine() : mImpl(new Impl()) {
    llama_backend_init();
}

LlamaEngine::~LlamaEngine() {
    mImpl->reset();
    llama_backend_free();
}

int32_t LlamaEngine::load(const std::string& dir) {
    mImpl->reset();

    const std::string path = dir + "/intent.gguf";

    llama_model_params mp = llama_model_default_params();
    mp.use_mmap = true;      // 必须:mmap 的页缓存可回收,LMK 不会因此杀进程
    mp.use_mlock = false;    // 不锁页,避免系统内存紧张
    mp.n_gpu_layers = 0;     // 一期纯 CPU;之后接 HTP 时在这里改

    mImpl->model = llama_model_load_from_file(path.c_str(), mp);
    if (mImpl->model == nullptr) {
        LOG(ERROR) << "llama_model_load_from_file failed: " << path;
        return kErrModelLoad;
    }

    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 512;          // 意图分类用不了长上下文
    cp.n_batch = 128;
    cp.n_threads = 2;        // 别和 UI 抢核
    cp.n_threads_batch = 2;

    mImpl->ctx = llama_init_from_model(mImpl->model, cp);
    if (mImpl->ctx == nullptr) {
        LOG(ERROR) << "llama_init_from_model failed";
        mImpl->reset();
        return kErrModelLoad;
    }

    mImpl->bytes = static_cast<int32_t>(llama_model_size(mImpl->model));
    LOG(INFO) << "llama model loaded, " << mImpl->bytes << " bytes";
    return kOk;
}

void LlamaEngine::unload() {
    mImpl->reset();
}

bool LlamaEngine::ready() const {
    return mImpl->model != nullptr && mImpl->ctx != nullptr;
}

int32_t LlamaEngine::modelBytes() const {
    return mImpl->bytes;
}

const char* LlamaEngine::name() const {
    return "llama.cpp";
}

IntentResult LlamaEngine::parseIntent(const std::string& text,
                                      const std::string& /*contextJson*/) {
    IntentResult r;
    if (!ready()) {
        r.code = kErrNotReady;
        return r;
    }

    // 说明:这里刻意留成"骨架"。
    // 真实实现要按固定的 prompt 模板喂入,并用 grammar sampler 把输出
    // 约束在 {"actionId":..,"params":..,"confidence":..} 的结构上,
    // 否则模型会吐自然语言,上层无法解析。
    //
    // 关键点:
    //   1. 用 llama_sampler_init_grammar 约束 JSON 结构
    //   2. n_predict 设小(意图只需要几十个 token)
    //   3. 加超时/abort 回调,配合 cancel()
    LOG(WARNING) << "LlamaEngine::parseIntent not implemented yet";
    r.code = kErrUnsupported;
    (void)text;
    return r;
}

TranscriptResult LlamaEngine::transcribe(const std::vector<uint8_t>& /*pcm*/,
                                         int32_t /*sampleRate*/) {
    TranscriptResult r;
    r.code = kErrUnsupported;
    return r;
}

#else  // !AGENT_WITH_LLAMA

struct LlamaEngine::Impl {};

LlamaEngine::LlamaEngine() : mImpl(new Impl()) {}
LlamaEngine::~LlamaEngine() = default;

int32_t LlamaEngine::load(const std::string&) { return kErrUnsupported; }
void LlamaEngine::unload() {}
bool LlamaEngine::ready() const { return false; }
int32_t LlamaEngine::modelBytes() const { return 0; }
const char* LlamaEngine::name() const { return "llama(not linked)"; }
IntentResult LlamaEngine::parseIntent(const std::string&, const std::string&) {
    IntentResult r;
    r.code = kErrUnsupported;
    return r;
}
TranscriptResult LlamaEngine::transcribe(const std::vector<uint8_t>&, int32_t) {
    TranscriptResult r;
    r.code = kErrUnsupported;
    return r;
}

#endif  // AGENT_WITH_LLAMA
