package android.system.agent;

// 所有回调 oneway:绝不阻塞 agentd 的 worker 线程。
oneway interface IAgentCoreCallback {
    void onPong(long requestId, @utf8InCpp String message);

    void onModelLoaded(long requestId, int result, @utf8InCpp String message);

    // ASR 结果。isFinal=false 为中间结果(流式场景),当前实现只发 final。
    void onTranscript(long requestId, @utf8InCpp String text, boolean isFinal);

    // intentJson 形如:
    //   {"actionId":"openApp","params":{"package":"com.tencent.mm"},"confidence":0.92}
    // agentd 不知道 actionId 的语义,只负责产出。
    void onIntent(long requestId, @utf8InCpp String intentJson, float confidence);

    void onError(long requestId, int code, @utf8InCpp String message);
}
