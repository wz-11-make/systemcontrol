package android.system.agent;

import android.system.agent.IAgentServiceCallback;
import android.system.agent.AgentStatus;

// Agent.apk(UI) -> AgentService(system_server)
interface IAgentService {
    int getVersion();
    AgentStatus getStatus();

    void processText(long requestId, @utf8InCpp String text, @utf8InCpp String origin,
                     IAgentServiceCallback cb);

    boolean cancel(long requestId);
    void confirm(long requestId, boolean approved);
}
