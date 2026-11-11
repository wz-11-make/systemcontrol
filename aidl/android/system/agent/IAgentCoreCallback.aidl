package android.system.agent;

oneway interface IAgentCoreCallback {
    void onPong(long requestId, @utf8InCpp String message);
    void onError(long requestId, int code, @utf8InCpp String message);
}
