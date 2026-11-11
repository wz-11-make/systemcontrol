package android.system.agent;

import android.system.agent.IAgentCoreCallback;
import android.system.agent.AgentStatus;

// AgentService(system_server) -> agentd(native)
// 只有推理:输入文本/音频,输出文本/意图 JSON。
// 这里永远不该出现 openApp / setVolume 这类动作方法。
interface IAgentCore {
    int getVersion();
    AgentStatus getStatus();

    // 骨架阶段的最小连通性验证
    void ping(long requestId, @utf8InCpp String message, IAgentCoreCallback cb);

    void cancel(long requestId);
}
