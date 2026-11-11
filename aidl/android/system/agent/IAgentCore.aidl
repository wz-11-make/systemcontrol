package android.system.agent;

import android.system.agent.IAgentCoreCallback;
import android.system.agent.AgentStatus;

// AgentService(system_server) -> agentd(native)
//
// 职责边界(重要):
//   ✅ 推理:文本 -> 意图 JSON,PCM -> 文本
//   ❌ 绝不出现 openApp / setVolume 这类动作方法
//   ❌ 绝不调用 AMS/PMS —— agentd 不知道系统服务的存在
interface IAgentCore {
    int getVersion();
    AgentStatus getStatus();

    // 连通性自检
    void ping(long requestId, @utf8InCpp String message, IAgentCoreCallback cb);

    // 模型生命周期。modelDir 下按约定放模型文件。
    void loadModel(long requestId, @utf8InCpp String modelDir, IAgentCoreCallback cb);
    void unloadModel();

    // 语音 -> 文本(整段 PCM,非流式)
    void transcribe(long requestId, in byte[] pcm, int sampleRate, IAgentCoreCallback cb);

    // 文本 -> 意图 JSON
    void parseIntent(long requestId, @utf8InCpp String text, @utf8InCpp String contextJson,
                     IAgentCoreCallback cb);

    void cancel(long requestId);
}
