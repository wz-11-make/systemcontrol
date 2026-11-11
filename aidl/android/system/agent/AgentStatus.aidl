package android.system.agent;

// 各层通用的健康状态。只用 int,避免跨语言 marshal 的坑。
parcelable AgentStatus {
    int version;
    int state;            // 0=init 1=ready 2=busy 3=error
    int modelState;       // 0=unloaded 1=loading 2=ready 3=error
    int queueDepth;       // 待处理请求数
    int modelBytes;       // 已加载模型占用
    int lastErrorCode;
}
