package com.android.agent;

import android.app.Activity;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.system.agent.AgentStatus;
import android.system.agent.IAgentService;
import android.system.agent.IAgentServiceCallback;
import android.util.Log;
import android.widget.TextView;

/**
 * 第三层:纯 UI。骨架阶段只做一件事 —— 证明能调到 AgentService。
 *
 * 刻意不写 getSystemService 那套 SystemServiceRegistry 管线,
 * 直接 ServiceManager.getService(),少一层框架改动。
 */
public class AgentActivity extends Activity {
    private static final String TAG = "Agent.UI";
    private static final String SERVICE_NAME = "agent";

    private TextView mText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mText = new TextView(this);
        mText.setTextSize(18f);
        setContentView(mText);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mText.post(this::refresh);
    }

    private void refresh() {
        final IBinder b = ServiceManager.getService(SERVICE_NAME);
        if (b == null) {
            mText.setText("AgentService 未注册\n\n(SERVICE_NAME=" + SERVICE_NAME + ")");
            return;
        }

        final IAgentService svc = IAgentService.Stub.asInterface(b);
        try {
            final AgentStatus s = svc.getStatus();
            final String head = "AgentService v" + svc.getVersion()
                    + "\nstate=" + s.state
                    + "  modelState=" + s.modelState
                    + "  err=" + s.lastErrorCode
                    + "  queue=" + s.queueDepth;
            // 立刻显示状态 —— 不能只在成功回调里设文字,否则出错时是白屏
            mText.setText(head + "\n\n(推理中…)");

            svc.processText(1L, "打开微信", "ui", new IAgentServiceCallback.Stub() {
                @Override
                public void onResult(long requestId, String summary, boolean needsConfirm) {
                    Log.i(TAG, "onResult id=" + requestId + " summary=" + summary);
                    runOnUiThread(() -> mText.setText(head + "\n\n" + summary));
                }

                @Override
                public void onError(long requestId, int code, String message) {
                    Log.w(TAG, "onError id=" + requestId + " code=" + code + " " + message);
                    runOnUiThread(() -> mText.setText(
                            head + "\n\n[error " + code + "] " + message));
                }
            });
        } catch (RemoteException e) {
            mText.setText("调用失败: " + e);
        } catch (Throwable t) {
            mText.setText("异常: " + t);
        }
    }
}
