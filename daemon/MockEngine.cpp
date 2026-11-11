#include "MockEngine.h"

#include <android-base/logging.h>
#include <strings.h>

#include <string>
#include <vector>

namespace {

struct Rule {
    const char* keyword;
    const char* actionId;
    const char* paramsJson;
    float confidence;
};

// 规则表。真模型接进来后这张表还能当"快速路径"用。
const Rule kRules[] = {
    {"微信",   "openApp", R"({"package":"com.tencent.mm"})",        0.95f},
    {"wechat", "openApp", R"({"package":"com.tencent.mm"})",        0.95f},
    {"设置",   "openApp", R"({"package":"com.android.settings"})",  0.92f},
    {"settings","openApp",R"({"package":"com.android.settings"})",  0.92f},
    {"相机",   "openApp", R"({"package":"org.lineageos.aperture"})",0.90f},
    {"camera", "openApp", R"({"package":"org.lineageos.aperture"})",0.90f},
    {"音量",   "setVolume", R"({"stream":"music","delta":-1})",     0.88f},
    {"volume", "setVolume", R"({"stream":"music","delta":-1})",     0.88f},
    {"静音",   "setVolume", R"({"stream":"music","delta":0})",      0.90f},
    {"mute",   "setVolume", R"({"stream":"music","delta":0})",      0.90f},
};

std::string toLowerAscii(const std::string& in) {
    std::string out = in;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

}  // namespace

int32_t MockEngine::load(const std::string& dir) {
    LOG(INFO) << "MockEngine::load dir=" << dir;
    // 构造一个"已加载"的假象,数值用于展示 status
    mBytes = 0;
    mReady = true;
    return kOk;
}

void MockEngine::unload() {
    mReady = false;
    mBytes = 0;
}

IntentResult MockEngine::parseIntent(const std::string& text,
                                     const std::string& /*contextJson*/) {
    IntentResult r;
    if (!mReady) {
        r.code = kErrNotReady;
        return r;
    }
    if (text.empty()) {
        r.code = kErrInvalidArg;
        return r;
    }

    const std::string lower = toLowerAscii(text);
    for (const Rule& rule : kRules) {
        if (lower.find(toLowerAscii(rule.keyword)) != std::string::npos) {
            r.code = kOk;
            r.confidence = rule.confidence;
            r.intentJson = std::string("{\"actionId\":\"") + rule.actionId +
                           "\",\"params\":" + rule.paramsJson +
                           ",\"confidence\":" + std::to_string(rule.confidence) +
                           ",\"rule\":\"" + escapeJson(rule.keyword) + "\"}";
            return r;
        }
    }

    // 未命中:低置信度 + unknown,让上层决定是否升级到云端
    r.code = kOk;
    r.confidence = 0.0f;
    r.intentJson = std::string("{\"actionId\":\"unknown\",\"params\":{},"
                               "\"confidence\":0,\"text\":\"") +
                   escapeJson(text) + "\"}";
    return r;
}

TranscriptResult MockEngine::transcribe(const std::vector<uint8_t>& pcm,
                                        int32_t sampleRate) {
    TranscriptResult r;
    if (!mReady) {
        r.code = kErrNotReady;
        return r;
    }
    if (pcm.empty() || sampleRate <= 0) {
        r.code = kErrInvalidArg;
        return r;
    }
    // 没有 ASR 引擎:明确报"不支持",而不是编一个假文本出来
    r.code = kErrUnsupported;
    r.text.clear();
    LOG(WARNING) << "transcribe: ASR engine not linked, pcm=" << pcm.size()
                 << "B rate=" << sampleRate;
    return r;
}
