package com.android.server.agent;

/**
 * 风险分级 + 错误码。骨架阶段只放常量,后续接 Mika 的 RiskJudge 逻辑。
 *
 * 原则:L3 不可逆动作的判定必须在代码里硬编码,绝不允许模型输出决定。
 */
public final class AgentPolicy {
    public static final int RISK_L0_READ_ONLY = 0;
    public static final int RISK_L1_REVERSIBLE = 1;
    public static final int RISK_L2_VISIBLE = 2;
    public static final int RISK_L3_IRREVERSIBLE = 3;
    public static final int RISK_L4_FORBIDDEN = 4;

    /** 与 agentd 侧 Engine.h 的 AgentError 保持一致。 */
    public static final int ERR_OK = 0;
    public static final int ERR_NOT_READY = 1;
    public static final int ERR_CANCELLED = 2;
    public static final int ERR_INVALID_ARG = 3;
    public static final int ERR_MODEL_LOAD = 4;
    public static final int ERR_INFERENCE = 5;
    public static final int ERR_UNSUPPORTED = 6;

    private AgentPolicy() {}

    /** L3 及以上必须二次确认。 */
    public static boolean requiresConfirmation(int risk) {
        return risk >= RISK_L3_IRREVERSIBLE;
    }

    /** L4 直接拒绝执行。 */
    public static boolean isForbidden(int risk) {
        return risk >= RISK_L4_FORBIDDEN;
    }

    public static String errorName(int code) {
        switch (code) {
            case ERR_OK: return "OK";
            case ERR_NOT_READY: return "NOT_READY";
            case ERR_CANCELLED: return "CANCELLED";
            case ERR_INVALID_ARG: return "INVALID_ARG";
            case ERR_MODEL_LOAD: return "MODEL_LOAD";
            case ERR_INFERENCE: return "INFERENCE";
            case ERR_UNSUPPORTED: return "UNSUPPORTED";
            default: return "UNKNOWN(" + code + ")";
        }
    }
}
