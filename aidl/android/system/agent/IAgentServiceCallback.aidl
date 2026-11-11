package android.system.agent;

oneway interface IAgentServiceCallback {
    // needsConfirm=true 时 UI 必须弹确认,approved 后走 confirm()
    void onResult(long requestId, @utf8InCpp String summary, boolean needsConfirm);
    void onError(long requestId, int code, @utf8InCpp String message);
}
