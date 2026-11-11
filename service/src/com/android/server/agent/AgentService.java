package com.android.server.agent;

import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.system.agent.AgentStatus;
import android.system.agent.IAgentCore;
import android.system.agent.IAgentCoreCallback;
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
 * 与 agentd 的关系:只通过 IAgentCore 通信,拿到的是一段意图 JSON,
 * 这里负责把它翻译成动作、做风险分级、必要时弹确认。
 */
public class AgentService extends SystemService {
    private static final String TAG = "AgentService";
    private static final String SERVICE_NAME = "agent";
    private static final String CORE_SERVICE_NAME = "agent_core";
    private static final int VERSION = 1;

    /** 和 agentd 的 Engine.h 保持一致。 */
    private static final int MODEL_STATE_UNLOADED = 0;
    private static final int MODEL_STATE_LOADING = 1;
    private static final int MODEL_STATE_READY = 2;
    private static final int MODEL_STATE_ERROR = 3;

    private final Context mContext;
    private final AgentBinder mBinder = new AgentBinder();

    private final Object mCoreLock = new Object();
    private IAgentCore mCore;

    private OpenAppExecutor mOpenApp;

    public AgentService(Context context) {
        super(context);
        mContext = context;
    }

    @Override
    public void onStart() {
        publishBinderService(SERVICE_NAME, mBinder);
        // 在 onStart 里初始化,而不是等 onBootPhase(PHASE_SYSTEM_SERVICES_READY) ——
        // 我们在 startOtherServices 末尾才被 startService,那时该 phase 可能已经过去了。
        // OpenAppExecutor 只持有 Context,真正的 PMS 查询发生在 execute() 时。
        mOpenApp = new OpenAppExecutor(mContext);
        Slog.i(TAG, "published binder service '" + SERVICE_NAME + "', executors ready");
    }

    @Override
    public void onBootPhase(int phase) {
        if (phase == PHASE_BOOT_COMPLETED) {
            Slog.i(TAG, "boot completed");
        }
    }

    /**
     * 拿 agentd 的 binder,并挂死亡通知。
     *
     * agentd 比 AgentService 起得晚(late_start),所以第一次拿不到是正常的,
     * 调用方应该重试而不是报错。
     */
    private IAgentCore getCore() {
        synchronized (mCoreLock) {
            if (mCore != null && mCore.asBinder().isBinderAlive()) {
                return mCore;
            }
            mCore = null;

            final IBinder b = ServiceManager.getService(CORE_SERVICE_NAME);
            if (b == null) {
                return null;
            }
            try {
                b.linkToDeath(() -> {
                    synchronized (mCoreLock) {
                        mCore = null;
                    }
                    Slog.w(TAG, "agentd died");
                }, 0);
            } catch (RemoteException e) {
                Slog.w(TAG, "linkToDeath failed", e);
                return null;
            }
            mCore = IAgentCore.Stub.asInterface(b);
            Slog.i(TAG, "connected to agentd");
            return mCore;
        }
    }

    /** 组一个 status:核心是 agentd 的状态,叠加我们自己的。 */
    private AgentStatus buildStatus() {
        final IAgentCore core = getCore();
        if (core == null) {
            final AgentStatus s = new AgentStatus();
            s.version = VERSION;
            s.state = 1;
            s.modelState = MODEL_STATE_UNLOADED;
            s.queueDepth = 0;
            s.modelBytes = 0;
            s.lastErrorCode = AgentPolicy.ERR_NOT_READY;
            return s;
        }
        try {
            return core.getStatus();
        } catch (RemoteException e) {
            Slog.w(TAG, "getStatus from agentd failed", e);
            final AgentStatus s = new AgentStatus();
            s.version = VERSION;
            s.state = 3;
            s.modelState = MODEL_STATE_ERROR;
            s.lastErrorCode = AgentPolicy.ERR_NOT_READY;
            return s;
        }
    }

    private final class AgentBinder extends IAgentService.Stub {
        @Override
        public int getVersion() {
            return VERSION;
        }

        @Override
        public AgentStatus getStatus() {
            return buildStatus();
        }

        @Override
        public void processText(long requestId, String text, String origin,
                                IAgentServiceCallback cb) {
            Slog.i(TAG, "processText id=" + requestId + " origin=" + origin);

            final IAgentCore core = getCore();
            if (core == null) {
                replyError(cb, requestId, AgentPolicy.ERR_NOT_READY, "agentd 未就绪");
                return;
            }

            final IAgentCoreCallback.Stub coreCb = new IAgentCoreCallback.Stub() {
                @Override
                public void onIntent(long id, String intentJson, float confidence) {
                    Slog.i(TAG, "intent id=" + id + " conf=" + confidence + " " + intentJson);
                    // 骨架:直接把意图回给 UI。
                    // 下一步在这里:解析 intentJson -> 查 AgentActionRegistry
                    //             -> 风险分级 -> 需要确认就走 confirm()
                    //             -> 否则 executor.execute()
                    replyResult(cb, requestId, intentJson, false);
                }

                @Override
                public void onError(long id, int code, String message) {
                    Slog.w(TAG, "agentd error id=" + id + " " + AgentPolicy.errorName(code)
                            + " " + message);
                    replyError(cb, requestId, code, message);
                }

                @Override public void onPong(long id, String message) { /* unused */ }
                @Override public void onModelLoaded(long id, int result, String message) { }
                @Override public void onTranscript(long id, String text, boolean isFinal) { }
            };

            try {
                core.parseIntent(requestId, text, "{}", coreCb);
            } catch (RemoteException e) {
                Slog.w(TAG, "parseIntent failed", e);
                replyError(cb, requestId, AgentPolicy.ERR_INFERENCE, "agentd 调用失败");
            }
        }

        @Override
        public boolean cancel(long requestId) {
            final IAgentCore core = getCore();
            if (core == null) return false;
            try {
                core.cancel(requestId);
                return true;
            } catch (RemoteException e) {
                Slog.w(TAG, "cancel failed", e);
                return false;
            }
        }

        @Override
        public void confirm(long requestId, boolean approved) {
            Slog.i(TAG, "confirm id=" + requestId + " approved=" + approved);
        }
    }

    private static void replyResult(IAgentServiceCallback cb, long id, String summary,
                                    boolean needsConfirm) {
        if (cb == null) return;
        try {
            cb.onResult(id, summary, needsConfirm);
        } catch (RemoteException e) {
            Slog.w(TAG, "callback died", e);
        }
    }

    private static void replyError(IAgentServiceCallback cb, long id, int code, String msg) {
        if (cb == null) return;
        try {
            cb.onError(id, code, msg);
        } catch (RemoteException e) {
            Slog.w(TAG, "callback died", e);
        }
    }
}
