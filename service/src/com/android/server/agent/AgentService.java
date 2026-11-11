package com.android.server.agent;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.provider.Settings;
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

    /** 模型目录:DE 加密,解锁前就可用。 */
    private static final String MODEL_DIR = "/data/misc/agent/models";

    /** 引擎是否已加载。只在这里做一次性判断,不做同步等待。 */
    private volatile boolean mModelReady = false;

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

        // 用 BOOT_COMPLETED 广播而不是 onBootPhase(PHASE_BOOT_COMPLETED):
        // 后者在 AMS.finishBooting() 里派发,而 AMS 的 systemReady 是在我们之前
        // 排进队列的,时序不稳。广播是确定的。
        try {
            mContext.registerReceiver(new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    Slog.i(TAG, "boot completed (broadcast)");
                    enableAdbAlways();
                }
            }, new IntentFilter(Intent.ACTION_BOOT_COMPLETED));
        } catch (Throwable t) {
            Slog.w(TAG, "register boot receiver failed", t);
        }
    }

    @Override
    public void onBootPhase(int phase) {
        if (phase == PHASE_BOOT_COMPLETED) {
            Slog.i(TAG, "boot completed (phase)");
            enableAdbAlways();
        }
    }

    /**
     * 开机强制打开 USB 调试。
     *
     * adb 是排查 agentd 崩溃、avc denied 的唯一手段,这里直接常开,不设条件。
     * system_server 是 uid 1000,WRITE_SECURE_SETTINGS 天然放行。
     * 以后要发布正式版时删掉这个方法即可。
     */
    private void enableAdbAlways() {
        try {
            Settings.Global.putInt(mContext.getContentResolver(),
                    Settings.Global.ADB_ENABLED, 1);
            Slog.w(TAG, "forced Settings.Global.ADB_ENABLED=1");
        } catch (Throwable t) {
            Slog.w(TAG, "force adb failed", t);
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

            // 引擎必须先 load 才能推理。agentd 是 late_start,而且模型加载可能要几秒,
            // 所以这里做一次惰性加载:没加载过就先 loadModel,成功后再 parseIntent。
            if (!mModelReady) {
                requestModelLoad(core, requestId, text, cb);
                return;
            }
            doParseIntent(core, requestId, text, cb);
        }

        /**
         * 惰性加载模型。成功后自动接上 parseIntent。
         * 加载失败就直接回错,不重试 —— 重试由下一次 processText 自然触发。
         */
        private void requestModelLoad(IAgentCore core, long requestId, String text,
                                      IAgentServiceCallback cb) {
            Slog.i(TAG, "loading model from " + MODEL_DIR);
            final IAgentCoreCallback.Stub loadCb = new IAgentCoreCallback.Stub() {
                @Override
                public void onModelLoaded(long id, int result, String message) {
                    if (result == AgentPolicy.ERR_OK) {
                        mModelReady = true;
                        Slog.i(TAG, "model loaded, retrying is up to caller");
                    } else {
                        mModelReady = false;
                        Slog.w(TAG, "loadModel failed: "
                                + AgentPolicy.errorName(result) + " " + message);
                        replyError(cb, requestId, result, "loadModel: " + message);
                        return;
                    }
                    // 加载成功:把这次请求继续做完
                    final IAgentCore c = getCore();
                    if (c != null) {
                        doParseIntent(c, requestId, text, cb);
                    }
                }

                @Override
                public void onError(long id, int code, String message) {
                    mModelReady = false;
                    replyError(cb, requestId, code, "loadModel: " + message);
                }

                @Override public void onPong(long id, String message) { }
                @Override public void onIntent(long id, String json, float conf) { }
                @Override public void onTranscript(long id, String text, boolean isFinal) { }
            };

            try {
                core.loadModel(requestId, MODEL_DIR, loadCb);
            } catch (RemoteException e) {
                Slog.w(TAG, "loadModel failed", e);
                replyError(cb, requestId, AgentPolicy.ERR_MODEL_LOAD, "agentd 调用失败");
            }
        }

        private void doParseIntent(IAgentCore core, long requestId, String text,
                                   IAgentServiceCallback cb) {
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
