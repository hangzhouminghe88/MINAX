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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

import static org.baculasystems.bmob.alfa.BaculaFDService.COMMAND;
import static org.baculasystems.bmob.alfa.BaculaFDService.FD_RUNNING;


/** Receiver that is called when the phone boots **/
public class BaculaBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        SharedPreferences prefs = context.getSharedPreferences(COMMAND, Context.MODE_PRIVATE);
        boolean fdRunning = prefs.getBoolean(FD_RUNNING, false);

        if(fdRunning) {
            BaculaFDService.start(context, null, null, null);
        }
    }
}
