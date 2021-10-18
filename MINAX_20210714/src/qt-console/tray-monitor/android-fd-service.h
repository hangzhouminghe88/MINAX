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

#ifndef ANDROID_FD_SERVICE_H
#define ANDROID_FD_SERVICE_H

#include "bacula.h"
#include "common.h"
#include <QtAndroid>
#include <android/log.h>

/*
   AndroidFD - Contains the methods we use to setup the Android File Daemon. Many
   methods involve calling Java code, such as:

   1 - The Android Service that creates the Android File Daemon process
   2 - The Screen that scans QR Codes
   3 - Some data that requires native Java calls (deviceId and devicePwd)
*/

class AndroidFD {
public:

   static int64_t logLevel;

   // Starts the screen that scans QR Codes. The receiver will be responsible
   // for handling the scanned QR Code data.
   static void startQRCodeReader(QAndroidActivityResultReceiver *receiver) {
      bool hasPermission = AndroidFD::getPermission("android.permission.CAMERA");

      if(!hasPermission) {
         return;
      }

      QAndroidJniObject jPackageName = QAndroidJniObject::fromString("org.baculasystems.bmob.alfa");
      QAndroidJniObject jClassName = QAndroidJniObject::fromString("org.baculasystems.bmob.alfa.QRCodeReaderActivity");
      QAndroidJniObject intent("android/content/Intent","()V");
      intent.callObjectMethod("setClassName",
                              "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                              jPackageName.object<jstring>(), jClassName.object<jstring>());
      QtAndroid::startActivity(intent, 0, receiver);
   }

   static QString deviceId() {
      return QAndroidJniObject::callStaticObjectMethod("org/baculasystems/bmob/alfa/BAndroidUtil",
                                                       "getDeviceId", "(Landroid/content/Context;)Ljava/lang/String;",
                                                       QtAndroid::androidActivity().object()
                                                       ).toString();
   }

   static QString devicePwd() {
      return QAndroidJniObject::callStaticObjectMethod("org/baculasystems/bmob/alfa/BAndroidUtil",
                                                       "getDevicePwd", "(Landroid/content/Context;)Ljava/lang/String;",
                                                       QtAndroid::androidActivity().object()
                                                       ).toString();
   }

   static QString sdcardPath() {
      return QAndroidJniObject::callStaticObjectMethod("org/baculasystems/bmob/alfa/BAndroidUtil",
                                                       "getSDcardPath", "()Ljava/lang/String;",
                                                       QtAndroid::androidActivity().object()
                                                       ).toString();
   }

   static QString tracePath() {
      return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bacula-fd.trace";
   }

   // Reads Bacula FD trace and send it in a request for Android to start an Mail App
   static void composeTraceMail() {
      QString mailSubject = "Bacula File Daemon Log";
      QString fdTrace = AndroidFD::tracePath();
      QByteArray fileBytes;
      QFile file(fdTrace);
      file.open(QIODevice::ReadOnly);
      file.seek(file.size() - 20000);
      fileBytes = file.read(20000);
      QString mailText = QString(fileBytes);
      QAndroidJniObject::callStaticMethod<void>("org/baculasystems/bmob/alfa/BAndroidUtil",
                                                "startMailApp",
                                                "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
                                                QtAndroid::androidActivity().object(),
                                                QAndroidJniObject::fromString(mailSubject).object<jstring>(),
                                                QAndroidJniObject::fromString(mailText).object<jstring>());
   }

   // Changes the bacula-fd.conf template by adding information related to the device
   static void setupFdConfig(QString configPath) {
      QByteArray fileBytes;
      QFile file(configPath);
      file.open(QIODevice::ReadWrite);
      fileBytes = file.readAll();
      QString text(fileBytes);
      text.replace("@DEVICE-ID@", AndroidFD::deviceId());
      text.replace("@DEVICE-PWD@", AndroidFD::devicePwd());
      file.seek(0);
      file.write(text.toUtf8());
      file.close();
   }

   // Creates the dialog that asks the user for a permission
   static bool getPermission(QString permission) {
      QtAndroid::PermissionResult res = QtAndroid::checkPermission(permission);

      if(res == QtAndroid::PermissionResult::Denied) {
         QtAndroid::requestPermissionsSync(QStringList() << permission);
         res = QtAndroid::checkPermission(permission);

         if(res == QtAndroid::PermissionResult::Denied) {
            Dmsg1(0, "Permission denied by the user: %s\n", permission.toLatin1().constData());
            return false;
         }
      }

      return true;
   }

   // Copies the file to a folder where we have permissions to do
   // whatever we want (read, write, execute)
   static void unpackAsset(QString srcAsset, QString destPath) {

      if (QFile::exists(srcAsset)) {
         Dmsg2(0, "Found %s. Copying to %s\n", srcAsset.toLatin1().constData(), destPath.toLatin1().constData());
         QFile asset(srcAsset);

         if (asset.copy(destPath)) {
            QFile::setPermissions(destPath, QFile::WriteOwner | QFile::ReadOwner | QFile::ExeGroup
                                  | QFile::ExeOther | QFile::ExeOwner | QFile::ExeUser);
         } else {
            Dmsg1(0, "ERROR - could no create %s\n", destPath.toLatin1().constData());
         }

      } else {
         Dmsg1(0, "%s not found.\n", srcAsset.toLatin1().constData());
      }

   }

   // Installs the Android File Daemon into the phone
   static void install() {
      // Ask the user for permissions needed for performing backups
      AndroidFD::getPermission("android.permission.READ_EXTERNAL_STORAGE");
      AndroidFD::getPermission("android.permission.WRITE_EXTERNAL_STORAGE");
      QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QString fdAsset = "assets:/static-bacula-fd";
      QString fdPath = appDir + "/bin/static-bacula-fd";
      QString configAsset = "assets:/bacula-fd.conf";
      QString configPath = appDir + "/etc/bacula-fd.conf";
      QString fdTrace = AndroidFD::tracePath();

      // If the Android File Daemon is not found, then install it
      if (!QFile::exists(fdPath)) {
         Dmsg0(0, "Unpacking Bacula FD...");
         QDir qd(appDir);
         qd.mkdir("bin");
         qd.mkdir("etc");
         qd.mkdir("lib");
         AndroidFD::unpackAsset(fdAsset, fdPath);
         AndroidFD::unpackAsset(configAsset, configPath);
         AndroidFD::setupFdConfig(configPath);

         // Create empty Trace File with proper permissions
         const char *tPath = fdTrace.toLatin1().constData();
         fclose(fopen(tPath, "ab+"));
         QFile::setPermissions(fdTrace, QFile::WriteOther | QFile::ReadOther
                                      | QFile::WriteUser  | QFile::ReadUser
                                      | QFile::WriteOwner | QFile::ReadOwner
                                      | QFile::WriteGroup | QFile::ReadGroup
                               );
      }
   }

   // Starts the Android Service that's responsible for creating the Android File Daemon process
   static void start () {
      QString fdDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QString fdCommand = fdDir + "/bin/static-bacula-fd";
      QString fdTrace = AndroidFD::tracePath();
      QString lLevel = QString::number(logLevel);
      QAndroidJniObject::callStaticMethod<void>("org/baculasystems/bmob/alfa/BaculaFDService",
                                                "start",
                                                "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                                QtAndroid::androidActivity().object(),
                                                QAndroidJniObject::fromString(fdCommand).object<jstring>(),
                                                QAndroidJniObject::fromString(fdTrace).object<jstring>(),
                                                QAndroidJniObject::fromString(lLevel).object<jstring>());
   }

   // Stops our Android Service
   static void stop() {
      QAndroidJniObject::callStaticMethod<void>("org/baculasystems/bmob/alfa/BaculaFDService",
                                                "stop",
                                                "(Landroid/content/Context;)V",
                                                QtAndroid::androidActivity().object());
   }

};

#endif // ANDROID_FD_SERVICE_H
