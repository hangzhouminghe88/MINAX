#!/bin/sh
#
#  This script builds the dependency packages that
#  are needed to cross compile the android version of the Bacula
#  Tray Monitor.
#  
#  Dependencies will be installed on ~/android-depkgs
#

get_source()
{
   URL=$1
   HTTP_HEADERS=$2

   if [ ! -e "$SRC_DIR" ]; then 
      mkdir ${SRC_DIR}
   fi

   cd ${SRC_DIR}
   echo "Downloading ${URL} into ${SRC_DIR}"
   ARCHIVE=`basename ${URL}`
 
   case ${ARCHIVE} in
   *.tar.gz)      
      ARCHIVER="tar xzf"
      [ -z "${SRC_DIR}" ] && SRC_DIR=`expr "${ARCHIVE}" : '\(.*\)\.tar\.gz'`
      ;;
   *.tar.bz2)
      ARCHIVER="tar xjf"
      [ -z "${SRC_DIR}" ] && SRC_DIR=`expr "${ARCHIVE}" : '\(.*\)\.tar\.bz2'`
      ;;
   *.zip)
      ARCHIVER="unzip -d ."
      [ -z "${SRC_DIR}" ] && SRC_DIR=`expr "${ARCHIVE}" : '\(.*\)\.zip'`
      ;;
   *.xz)
      ARCHIVER="tar xf"
      [ -z "${SRC_DIR}" ] && SRC_DIR=`expr "${ARCHIVE}" : '\(.*\)\.tar\.xz'`
      ;;
   *)
      echo "Unsupported archive type - ${ARCHIVE}"
      exit 1
      ;;
   esac

   if [ ! -e "${ARCHIVE}" ]; then 
      echo Downloading "${URL}"
      if wget --passive-ftp --header "${HTTP_HEADERS}" "${URL}"
      then
         :
      else
         echo "Unable to download ${ARCHIVE}"
         exit 1
      fi
   fi

   echo Extracting ${ARCHIVE}
   ${ARCHIVER} ${ARCHIVE} > ${ARCHIVE}.log 2>&1
   return 1
}

process_jdk()
{
   HTTP_HEADERS="Cookie: oraclelicense=accept-securebackup-cookie"
   JDK_URL="http://download.oracle.com/otn-pub/java/jdk/8u131-b11/d54c1d3a095b4ff2b6607d096fa80163/jdk-8u131-linux-x64.tar.gz"
   JDK_SRC_DIR=${SRC_DIR}/jdk1.8.0_131
   JDK_TARGET_DIR="${DEPKGS_DIR}/java" 
   
   rm -rf ${JDK_SRC_DIR} 
   get_source "${JDK_URL}" "${HTTP_HEADERS}"
   rm -rf ${JDK_TARGET_DIR}
   mkdir ${JDK_TARGET_DIR}
   mv ${JDK_SRC_DIR}/* ${JDK_TARGET_DIR}
   rm -rf ${JDK_SRC_DIR} 
}

process_android_sdk()  
{
   SDK_URL="https://dl.google.com/android/repository/sdk-tools-linux-3859397.zip"
   SDK_SRC_DIR=${SRC_DIR}/tools
   SDK_TARGET_DIR="${DEPKGS_DIR}/sdk" 
   
   rm -rf ${SDK_SRC_DIR} 
   get_source "${SDK_URL}"
   rm -rf ${SDK_TARGET_DIR}
   mkdir ${SDK_TARGET_DIR}
   mv ${SDK_SRC_DIR} ${SDK_TARGET_DIR}
   rm -rf ${SDK_SRC_DIR} 

   export ANDROID_HOME=${SDK_TARGET_DIR}
   export PATH="$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools:$PATH"

   yes | ${SDK_TARGET_DIR}/tools/bin/sdkmanager --licenses 
   ${SDK_TARGET_DIR}/tools/bin/sdkmanager "tools" "build-tools;28.0.0" "extras;android;m2repository" "extras;google;google_play_services" "platforms;android-28" "platform-tools" 
}

process_android_ndk()
{
   NDK_URL="https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip"
   NDK_SRC_DIR=${SRC_DIR}/android-ndk-r10e
   NDK_TARGET_DIR="${DEPKGS_DIR}/ndk" 
   
   rm -rf ${NDK_SRC_DIR} 
   get_source "${NDK_URL}"
   rm -rf ${NDK_TARGET_DIR}
   mkdir ${NDK_TARGET_DIR}
   mv ${NDK_SRC_DIR}/* ${NDK_TARGET_DIR}
   rm -rf ${NDK_SRC_DIR} 
}

process_openssl()
{
   OPENSSL_URL="https://www.openssl.org/source/openssl-1.0.2r.tar.gz"
   OPENSSL_SRC_DIR=${SRC_DIR}/openssl-1.0.2r
   OPENSSL_TARGET_DIR="${SRC_DIR}/openssl" 
   
   rm -rf ${OPENSSL_SRC_DIR} 
   get_source "${OPENSSL_URL}"
   rm -rf ${OPENSSL_TARGET_DIR}
   mkdir ${OPENSSL_TARGET_DIR}
   mv ${OPENSSL_SRC_DIR}/* ${OPENSSL_TARGET_DIR}
   rm -rf ${OPENSSL_SRC_DIR}
}

process_qt()
{
   QT_TARGET_DIR="${DEPKGS_DIR}/qt"

   if [ ! -e "${QT_TARGET_DIR}" ]; then 
      mkdir ${QT_TARGET_DIR}
   fi
}

DEPKGS_DIR=$HOME/android-depkgs
ANDROID_CFG_DIR=$HOME/.android
SRC_DIR=$DEPKGS_DIR/src

if [ ! -e "${DEPKGS_DIR}" ]; then 
   mkdir ${DEPKGS_DIR}
fi    

if [ ! -e "${ANDROID_CFG_DIR}" ]; then 
   mkdir ${ANDROID_CFG_DIR}
fi

touch ${ANDROID_CFG_DIR}/repositories.cfg

process_android_sdk
process_android_ndk
process_jdk
process_openssl
process_qt
