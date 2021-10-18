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

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Environment;
import android.util.Base64;
import android.content.Intent;
import android.net.Uri;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.Log;

import java.security.SecureRandom;
import java.util.Random;

public final class BAndroidUtil {

    public static final String PREFS_NAME = "prefs";
    public static final String DEVICE_ID = "prefs.device_id";
    public static final String DEVICE_PWD = "prefs.device_pwd";

    static String getSDcardPath() {
        return Environment.getExternalStorageDirectory().getPath();
    }

    static String getDeviceId(Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String deviceId = prefs.getString(DEVICE_ID, null);

        if (deviceId == null) {
            Random rand = new Random();
            StringBuilder builder = new StringBuilder();
            builder.append(Build.MODEL);
            builder.append("-");

            for (int i = 0; i < 5; i++) {
                builder.append(rand.nextInt(10));
            }

            deviceId = builder.toString();
            SharedPreferences.Editor editor = prefs.edit();
            editor.putString(DEVICE_ID, deviceId);
            editor.commit();
        }

        return deviceId;
    }

    static String getDevicePwd(Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String devicePwd = prefs.getString(DEVICE_PWD, null);

        if (devicePwd == null) {
            SecureRandom random = new SecureRandom();
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            devicePwd = Base64.encodeToString(bytes, Base64.NO_PADDING);
            devicePwd = devicePwd.substring(0, devicePwd.length() - 2);
            SharedPreferences.Editor editor = prefs.edit();
            editor.putString(DEVICE_PWD, devicePwd);
            editor.commit();
        }

        return devicePwd;
    }

    static public void startMailApp(Context ctx, String subject, String message) {
        String[] adresses = new String[0];
        Intent intent = new Intent(Intent.ACTION_SENDTO);
        // Only email apps will handle the Intent
        intent.setData(Uri.parse("mailto:"));
        intent.putExtra(Intent.EXTRA_EMAIL, adresses);
        intent.putExtra(Intent.EXTRA_SUBJECT, subject);
        intent.putExtra(Intent.EXTRA_TEXT, message);
        ctx.startActivity(intent);
    }
}
