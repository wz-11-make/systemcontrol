package com.android.server.agent;

/**
 * 风险分级策略。骨架阶段只放常量,后续接 Mika 的 RiskJudge 逻辑。
 *
 * 原则:L3 不可逆动作的判定必须在代码里硬编码,绝不允许模型输出决定。
 */
public final class AgentPolicy {
    public static final int RISK_L0_READ_ONLY = 0;
    public static final int RISK_L1_REVERSIBLE = 1;
    public static final int RISK_L2_VISIBLE = 2;
    public static final int RISK_L3_IRREVERSIBLE = 3;
    public static final int RISK_L4_FORBIDDEN = 4;

    private AgentPolicy() {}

    /** L3 及以上必须二次确认。 */
    public static boolean requiresConfirmation(int risk) {
        return risk >= RISK_L3_IRREVERSIBLE;
    }

    /** L4 直接拒绝执行。 */
    public static boolean isForbidden(int risk) {
        return risk >= RISK_L4_FORBIDDEN;
    }
}
