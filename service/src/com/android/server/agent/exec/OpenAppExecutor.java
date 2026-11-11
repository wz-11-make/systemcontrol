package com.android.server.agent.exec;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.util.Slog;

/**
 * 骨架:唯一的动作执行器。其余动作照这个形状加。
 *
 * 注意:动作执行方在 system_server,不再需要 START_ACTIVITIES_FROM_BACKGROUND
 * 之外的任何"申请",因为 system uid 天然放行。
 */
public final class OpenAppExecutor {
    private static final String TAG = "Agent.OpenApp";

    private final Context mContext;

    public OpenAppExecutor(Context context) {
        mContext = context;
    }

    public boolean execute(String packageName) {
        final PackageManager pm = mContext.getPackageManager();
        final Intent launch = pm.getLaunchIntentForPackage(packageName);
        if (launch == null) {
            Slog.w(TAG, "no launch intent for " + packageName);
            return false;
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivityAsUser(launch, android.os.UserHandle.SYSTEM);
        Slog.i(TAG, "launched " + packageName);
        return true;
    }
}
