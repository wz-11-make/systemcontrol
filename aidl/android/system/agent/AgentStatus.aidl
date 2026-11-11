package android.system.agent;

// 各层通用的健康状态。故意只用 int,避免跨语言 marshal 的坑。
parcelable AgentStatus {
    int version;
    int state;       // 0=uninit 1=ready 2=busy 3=error
    int modelState;  // 0=unloaded 1=loading 2=ready 3=error
}
