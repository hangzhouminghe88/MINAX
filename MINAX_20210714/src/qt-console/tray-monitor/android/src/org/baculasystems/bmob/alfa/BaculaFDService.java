/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

package org.baculasystems.bmob.alfa;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.IBinder;
import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationCompat;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/*
    BaculaFDService - Responsible for managing the Android File Daemon

    This Service was designed to not be killed by Android unless under
    heavy memory or battery pressure.

*/
public class BaculaFDService extends Service {

    public static final String LOG_TAG = "BACULA_FD_SERVICE";
    public static final String COMMAND = "prefs.fdcommand";
    public static final String TRACE_PATH = "prefs.tracepath";
    public static final String LOG_LEVEL = "prefs.loglevel";
    public static final String FD_RUNNING = "prefs.fdrunning";
    public static final String CHANNEL_ID = "bsys.bmob.fd";
    public static final String ACTION_STOP_SERVICE = "bsys.stop.service";
    public static final int NOTIFICATION_ID = 42;

    // The Android File Deamon Process
    Process fdProcess = null;
    Notification notification = null;
    Runnable traceRunnable = null;
    // This thread reads the FD Process stdout and fills the trace file
    Thread traceThread = null;
    Runnable watcherRunnable = null;
    // This thread watches the FD Process to see if it's still alive
    Thread watcherThread = null;
    boolean readTrace = true;

    // Called by our c++ code.
    public static void start(Context ctx, String command, String tracePath, String logLevel) {
        Intent it = new Intent(ctx, BaculaFDService.class);

        if (command != null) {
            SharedPreferences prefs = ctx.getSharedPreferences(COMMAND, Context.MODE_MULTI_PROCESS);
            SharedPreferences.Editor editor = prefs.edit();
            editor.putString(COMMAND, command);
            editor.putString(TRACE_PATH, tracePath);
            editor.putString(LOG_LEVEL, logLevel);
            editor.putBoolean(FD_RUNNING, true);
            editor.commit();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ctx.startForegroundService(it);
        } else {
            ctx.startService(it);
        }
    }

    // Called by our c++ code.
    public static void stop(Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(COMMAND, Context.MODE_MULTI_PROCESS);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(FD_RUNNING, false);
        editor.commit();
        Intent it = new Intent(ctx, BaculaFDService.class);
        ctx.stopService(it);
    }

    private void logException(String tag, Exception e) {
        String msg = e.getMessage();

        if(msg != null) {
            Log.e(tag, msg);
        } else {
            Log.e(tag,"Null message");
        }
    }

    private void sendNotification(String text) {
        NotificationCompat.Builder builder = new NotificationCompat.Builder(getBaseContext(), CHANNEL_ID)
                .setSmallIcon(R.drawable.logo)
                .setContentTitle("Bacula")
                .setContentText(text)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT);
        Intent stopSelf = new Intent(this, BaculaFDService.class);
        stopSelf.setAction(ACTION_STOP_SERVICE);
        PendingIntent pit = PendingIntent.getService(this, 0, stopSelf, PendingIntent.FLAG_CANCEL_CURRENT);
        builder.addAction(R.mipmap.ic_launcher, "Stop", pit);
        startForeground(NOTIFICATION_ID, builder.build());
    }

    private void saveTrace(final InputStream pOutput, final String tracePath)  {
        traceRunnable = new Runnable() {
            public void run() {
                try {
                    File targetFile = new File(tracePath);
                    OutputStream traceStream = new FileOutputStream(targetFile, true);
                    byte[] buffer = new byte[2048];
                    int bytesRead;

                    while (readTrace) {
                        while ((bytesRead = pOutput.read(buffer, 0, pOutput.available())) != -1) {
                            traceStream.write(buffer, 0, bytesRead);
                        }
                    }

                    pOutput.close();
                    traceStream.close();
                } catch (Exception e) {
                    logException("FD Trace Error", e);
                }
            }
        };

        traceThread = new Thread(traceRunnable);
        traceThread.start();
    }

    private void startProcess(String command, String tracePath, String logLevel) {
        try {
            ProcessBuilder pb = new ProcessBuilder(command, "-f", "-d", logLevel, "-dt");
            fdProcess = pb.start();
            saveTrace(fdProcess.getInputStream(), tracePath);
        } catch (Exception e) {
            logException("FD Process start error", e);
        }
    }

    private boolean isRunning(Process p) {
        try {
            int eval = p.exitValue();
            Log.e(LOG_TAG, "File Daemon stopped with exit code " + eval);
            return false;
        } catch (Exception e) {
            return true;
        }
    }

    private void startWatcher() {
        watcherRunnable = new Runnable() {
            public void run() {
                while (true) {
                    Log.e(LOG_TAG, "CHECKING FD PROCESS...");
                    try {
                        if (!isRunning(fdProcess)) {
                            break;
                        }
                        Thread.sleep(1000);
                    } catch (Exception e) {
                        logException("Watcher Thread Error", e);
                        break;
                    }
                }
            }
        };

        watcherThread = new Thread(watcherRunnable);
        watcherThread.start();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private void startNotificationChannel() {
        CharSequence name = "Bacula Channel";
        String description = "Channel desc";
        int importance = NotificationManager.IMPORTANCE_DEFAULT;
        NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
        channel.setDescription(description);
        // Register the channel with the system; you can't change the importance
        // or other notification behaviors after this
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(channel);
    }

    public void stopFileDaemon() {
        if (notification != null) {
            stopForeground(true);
        }

        if (fdProcess != null) {
            fdProcess.destroy();
        }

        SharedPreferences prefs = getSharedPreferences(COMMAND, Context.MODE_MULTI_PROCESS);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(FD_RUNNING, false);
        editor.commit();

        readTrace = false;

        if (traceThread != null) {
            try {
                traceThread.join();
            } catch (InterruptedException e) {
                logException("FD Trace Error - join()", e);
            }
        }
    }

    /** No need to bind this service to the caller activity */
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /** The service is starting, due to a call to startService() */
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        int ret = super.onStartCommand(intent, flags, startId);
        Log.i(LOG_TAG, "Service created");
        SharedPreferences prefs = getSharedPreferences(COMMAND, Context.MODE_MULTI_PROCESS);
        String command = prefs.getString(COMMAND, null);
        String tracePath = prefs.getString(TRACE_PATH, null);
        String logLevel = prefs.getString(LOG_LEVEL, null);

        if (intent != null) {
            String iaction = intent.getAction();

            if (iaction != null && iaction.equals(ACTION_STOP_SERVICE)) {
                stopSelf();
                return ret;
            }
        }

        if (command != null && fdProcess == null) {
            Log.i(LOG_TAG, "Starting FD Process");
            startProcess(command, tracePath, logLevel);
            startWatcher();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                startNotificationChannel();
            }

            sendNotification("File Daemon is Running");
        }

        return ret;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopFileDaemon();
        Log.i(LOG_TAG, "Service destroyed");
    }
}
