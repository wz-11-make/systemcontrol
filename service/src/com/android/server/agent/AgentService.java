package com.android.server.agent;

import android.content.Context;
import android.os.RemoteException;
import android.system.agent.AgentStatus;
import android.system.agent.IAgentService;
import android.system.agent.IAgentServiceCallback;
import android.util.Slog;

import com.android.server.SystemService;
import com.android.server.agent.exec.OpenAppExecutor;

/**
 * 动作执行层,跑在 system_server 里(AMS 式)。
 *
 * 硬性纪律:
 *  - 不做推理,不加载模型,没有 native 代码
 *  - 不在 binder 线程上做任何可能阻塞的事
 *  - 任何异常自己吞掉并记日志 —— 这里崩了就是整机软重启
 *
 * 骨架阶段:只注册服务 + 返回状态 + 一个假的结果回调。
 */
public class AgentService extends SystemService {
    private static final String TAG = "AgentService";
    private static final String SERVICE_NAME = "agent";
    private static final int VERSION = 1;

    private final Context mContext;
    private final AgentBinder mBinder = new AgentBinder();
    private OpenAppExecutor mOpenApp;

    public AgentService(Context context) {
        super(context);
        mContext = context;
    }

    @Override
    public void onStart() {
        publishBinderService(SERVICE_NAME, mBinder);
        Slog.i(TAG, "published binder service '" + SERVICE_NAME + "'");
    }

    @Override
    public void onBootPhase(int phase) {
        if (phase == PHASE_SYSTEM_SERVICES_READY) {
            mOpenApp = new OpenAppExecutor(mContext);
            Slog.i(TAG, "executors ready");
        }
    }

    private static AgentStatus makeStatus(int state, int modelState) {
        final AgentStatus s = new AgentStatus();
        s.version = VERSION;
        s.state = state;
        s.modelState = modelState;
        return s;
    }

    private final class AgentBinder extends IAgentService.Stub {
        @Override
        public int getVersion() {
            return VERSION;
        }

        @Override
        public AgentStatus getStatus() {
            return makeStatus(1, 0);
        }

        @Override
        public void processText(long requestId, String text, String origin,
                                IAgentServiceCallback cb) {
            Slog.i(TAG, "processText id=" + requestId + " origin=" + origin + " text=" + text);
            // 骨架:先证明链路通,不做真实推理
            final int risk = AgentPolicy.RISK_L0_READ_ONLY;
            try {
                if (cb != null) {
                    cb.onResult(requestId,
                            "skeleton: received \"" + text + "\"",
                            AgentPolicy.requiresConfirmation(risk));
                }
            } catch (RemoteException e) {
                Slog.w(TAG, "callback died", e);
            }
        }

        @Override
        public boolean cancel(long requestId) {
            Slog.i(TAG, "cancel id=" + requestId);
            return true;
        }

        @Override
        public void confirm(long requestId, boolean approved) {
            Slog.i(TAG, "confirm id=" + requestId + " approved=" + approved);
        }
    }
}
