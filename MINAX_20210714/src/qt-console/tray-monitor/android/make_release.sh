#!/bin/sh
#
#

usage()
{
   echo "usage: $0 [-f <keystore-file> -p <keystore-password> -a <android-arch>] ..."
   echo "  -h                  Displays this usage"
   echo "  -f <keystore-file>  Path to Keystore File"
   echo "  -p <keystore-pwd>   Keystore Password"
   echo "  -a <android-arch>   Desired android architecture"
   echo "  (Accepted values: "arm", "aarch64", "x86, "x86_64", "mips", "mips64)"
   echo "  -s                  Skip clean"
   echo "  -e                  Generate enterprise version"
   echo ""
}

setenvs()
{
_TOOLCHAIN_NAME=$1

#Used to create NDK's standalone toolchain compiler
_GCC_VERSION="4.9"

# Set _ANDROID_API to the API you want to use. You should set it
# to one of: android-26, android-24, etc.
export ANDROID_API="21"

#####################################################################

# Error checking

# ANDROID_NDK_ROOT should always be set by the user (even when not running this script)
# http://groups.google.com/group/android-ndk/browse_thread/thread/a998e139aca71d77
if [ -z "$ANDROID_NDK_ROOT" ] || [ ! -d "$ANDROID_NDK_ROOT" ]; then
  echo "Error: ANDROID_NDK_ROOT not found. Please, create this Environment Variable"
  exit 1
fi

if [ ! -d "$ANDROID_NDK_ROOT/build/tools" ]; then
  echo "Error: ANDROID_NDK_ROOT/build/tools is not a valid path. Please edit this script."
  exit 1
fi

if [ ! -e "$ANDROID_NDK_ROOT/build/tools/make-standalone-toolchain.sh" ]; then
  echo "Error: $ANDROID_NDK_ROOT/build/tools/make-standalone-toolchain.sh not found. Please edit this script."
  exit 1
fi

if [ -z "$QT_ROOT" ] || [ ! -d "$QT_ROOT" ]; then
  echo "Error: QT_ROOT not found. Please, create this Environment Variable"
  exit 1
fi

#####################################################################

#Uses config.guess script to save the system's canonical name
#This will be used by Bacula's configure script
#(needed to perform a cross-compilation)

export BUILD_SYSTEM=$(${TOP_DIR}/autoconf/config.guess)

######################################################################

# Figure it out the Standalone Toolchain.
# If it isn't been created yet, we create one
# with the "make-standalone-toolchain.sh" script
# Also, set up $ANDROID_ARCH variable to be used
# with Bacula's "configure" script

_FULL_TOOLCHAIN_NAME=""
_ANDROID_ARCH=""
_QT_LIB_FOLDER=""
_QT_BIN_FOLDER=""
_OPENSSL_MACHINE=""
_OPENSSL_RELEASE=2.6.37
_OPENSSL_SYSTEM=""
_OPENSSL_ARCH=""

case ${_TOOLCHAIN_NAME} in
	"arm")
      _FULL_TOOLCHAIN_NAME="arm-linux-androideabi"
      _ANDROID_ARCH="armv7-none-linux-androideabi"
      _QT_LIB_FOLDER="armeabi-v7a"
      _QT_BIN_FOLDER="armv7"
      _OPENSSL_MACHINE=armv7
      _OPENSSL_SYSTEM=armv7
      _OPENSSL_ARCH=arm
	  ;;
	"aarch64")
      _FULL_TOOLCHAIN_NAME="aarch64-linux-android"
      _ANDROID_ARCH="aarch64-none-linux-android"
      _QT_LIB_FOLDER="arm64-v8a"
      _QT_BIN_FOLDER="$_TOOLCHAIN_NAME"
      _OPENSSL_MACHINE=armv7
      _OPENSSL_SYSTEM=armv7
      _OPENSSL_ARCH=arm
	  ;;
	"x86")
      _FULL_TOOLCHAIN_NAME="x86"
      _ANDROID_ARCH="i686-none-linux-android"
      _QT_LIB_FOLDER="$_TOOLCHAIN_NAME"
      _QT_BIN_FOLDER="$_TOOLCHAIN_NAME"
      _OPENSSL_MACHINE=i686
      _OPENSSL_SYSTEM=x86
      _OPENSSL_ARCH=x86
	  ;;
	"x86_64")
      _FULL_TOOLCHAIN_NAME="x86_64-linux-android"
      _ANDROID_ARCH="x86_64-none-linux-android"
      _QT_LIB_FOLDER="$_TOOLCHAIN_NAME"
      _QT_BIN_FOLDER="$_TOOLCHAIN_NAME"
      _OPENSSL_MACHINE=i686
      _OPENSSL_SYSTEM=x86
      _OPENSSL_ARCH=x86
	  ;;
	"mips")
      _FULL_TOOLCHAIN_NAME="mipsel-linux-android"
      _ANDROID_ARCH="mipsel-none-linux-android"
      _QT_LIB_FOLDER="$_TOOLCHAIN_NAME"
      _QT_BIN_FOLDER="$_TOOLCHAIN_NAME"
	  ;;
	"mips64")
      _FULL_TOOLCHAIN_NAME="mips64el-linux-android"
      _ANDROID_ARCH="mips64el-none-linux-android"
      _QT_LIB_FOLDER="$_TOOLCHAIN_NAME"
      _QT_BIN_FOLDER="$_TOOLCHAIN_NAME"
	  ;;
	*)
	  echo "Invalid Android architecture. Possible values are: 'arm', 'aarch64', 'x86', 'x86_64', 'mips', 'mips64'"
     exit 1
	  ;;
esac

export ANDROID_ARCH="$_ANDROID_ARCH"
export ANDROID_SYSROOT="$ANDROID_NDK_ROOT/platforms/android-$ANDROID_API/arch-$_TOOLCHAIN_NAME/"
export ANDROID_QT_BUILD_LIBS="libs/$_QT_LIB_FOLDER"
export ANDROID_QT_QMAKE="$QT_ROOT/android_$_QT_BIN_FOLDER/bin/qmake"
export ANDROID_QT_APK_MAKE="$QT_ROOT/android_$_QT_BIN_FOLDER/bin/androiddeployqt"

export MACHINE="$_OPENSSL_MACHINE"
export RELEASE="$_OPENSSL_RELEASE"
export SYSTEM="$_OPENSSL_SYSTEM"
export ARCH="$_OPENSSL_ARCH"

export TOOLCHAIN_PATH="$ANDROID_NDK_ROOT"/standalone/"$_TOOLCHAIN_NAME-android-$ANDROID_API"

if [ ! -d "$TOOLCHAIN_PATH" ]; then
  echo "Creating Standalone Toolchain... (this may take a few seconds)"
"$ANDROID_NDK_ROOT"/build/tools/make-standalone-toolchain.sh \
                 --platform="android-$ANDROID_API" \
                 --toolchain="$_FULL_TOOLCHAIN_NAME-$_GCC_VERSION" \
                 --install-dir="$TOOLCHAIN_PATH"
fi


######################################################################

# Tries to find the C and C++ compilers
# inside the Standalone Toolchain location

CC_NAME=""
CXX_NAME=""

case ${_TOOLCHAIN_NAME} in
	"x86")
      CC_NAME="i686-linux-android-gcc"
      CXX_NAME="i686-linux-android-g++"
	  ;;
	*)
	  CC_NAME="$_FULL_TOOLCHAIN_NAME-gcc"
      CXX_NAME="$_FULL_TOOLCHAIN_NAME-g++"
	  ;;
esac

export CC="$TOOLCHAIN_PATH/bin/$CC_NAME"
export CXX="$TOOLCHAIN_PATH/bin/$CXX_NAME"
export CPPFLAGS="-D__ANDROID_API__=$ANDROID_API -fPIC"
export CXXFLAGS="-fPIC"
export LDFLAGS="-fPIE -pie"
export ANDROID_INSTALL_DIR="/data/user/0/org.baculasystems.bmob.alfa/files"

if [ ! -e "$CC" ]; then
  echo "Error: Toolchain C compiler not found at $CC. Please edit this script."
  exit 1
fi

if [ ! -e "$CXX" ]; then
  echo "Error: Toolchain C++ compiler not found at $CXX. Please edit this script."
  exit 1
fi

VERBOSE=1
if [ ! -z "$VERBOSE" ] && [ "$VERBOSE" != "0" ]; then

  echo "" 
  echo "######### COMPILER VARIABLES ########" 
  echo ""
  echo "BUILD_SYSTEM: $BUILD_SYSTEM"
  echo "CC:           $CC"
  echo "CXX:          $CXX"
  echo "CPPFLAGS:     $CPPFLAGS"
  echo "LDFLAGS:      $LDFLAGS"
  echo ""
  
  echo "######### ANDROID VARIABLES ########" 
  echo ""
  echo "ANDROID_API_LEVEL:   $ANDROID_API"
  echo "ANDROID_ARCH:        $ANDROID_ARCH"
  echo "TOOLCHAIN_FULL_NAME: $_FULL_TOOLCHAIN_NAME"
  echo "ANDROID_INSTALL_DIR: $ANDROID_INSTALL_DIR" 
  echo "ANDROID_NDK_ROOT:    $ANDROID_NDK_ROOT"
  echo "ANDROID_SYSROOT:     $ANDROID_SYSROOT"
  echo "TOOLCHAIN_PATH:      $TOOLCHAIN_PATH"
  echo ""

  echo "######### QT VARIABLES ########" 
  echo ""
  echo "ANDROID_QT_QMAKE:      $ANDROID_QT_QMAKE" 
  echo "ANDROID_QT_APK_MAKE:   $ANDROID_QT_APK_MAKE" 
  echo "ANDROID_QT_BUILD_LIBS: (ANDROID-BUILD-DIR)/$ANDROID_QT_BUILD_LIBS" 
  echo ""

  echo "######### OPENSSL VARIABLES ########" 
  echo ""
  echo "OPENSSL_MACHINE:       $MACHINE"
  echo "OPENSSL_RELEASE:       $RELEASE"
  echo "OPENSSL_SYSTEM:        $SYSTEM"
  echo "OPENSSL_ARCH:          $ARCH"
  echo ""
fi
}

#TODO - 'guess' top dir
TOP_DIR=../../../..
CWD=${PWD}
FILED_DIR=${TOP_DIR}/src/filed
TRAYMON_DIR=${TOP_DIR}/src/qt-console/tray-monitor
SKIP_CLEAN=
ENTERPRISE=

while getopts ":f:p:a:sehH:" opt; do
   case ${opt} in
   f)
      export KEYSTORE_FILE="${OPTARG}"
      ;;
   p)
      export KEYSTORE_PASSWORD="${OPTARG}"
      ;;
   a)
      export BMOB_ARCH="${OPTARG}"
      ;;
   s) 
      export SKIP_CLEAN="true"
      ;;
   e) 
      export ENTERPRISE="true"
      ;;
   H|h|\?)
      usage
      exit 1
      ;;
   esac
done

if [ ! -e "${KEYSTORE_FILE}" ]; then
  echo "Error: Keystore file not found"
  exit 1
fi

export DEPKGS_DIR="${HOME}/android-depkgs"

if [ ! -d "$DEPKGS_DIR" ]; then
  echo "Error: Android Depkgs folder not found. Please, run 'build-depkgs-android.sh' first."
  exit 1
fi

export QT_ROOT=${HOME}/android-depkgs/qt
export ANDROID_OPENSSL_DIR=${DEPKGS_DIR}/openssl/build-${BMOB_ARCH}
export ANDROID_NDK_ROOT=${DEPKGS_DIR}/ndk
export ANDROID_SDK_ROOT=${DEPKGS_DIR}/sdk
export ANDROID_HOME=${ANDROID_SDK_ROOT}
export PATH=${PATH}:${ANDROID_HOME}

setenvs $BMOB_ARCH

if [ ! -d "${ANDROID_OPENSSL_DIR}" ]; then
   rm -rf $ANDROID_OPENSSL_DIR
   mkdir $ANDROID_OPENSSL_DIR
   (cd ${DEPKGS_DIR}/src/openssl; make clean)
   (cd ${DEPKGS_DIR}/src/openssl; ./Configure android-${SYSTEM} no-shared no-asm no-comp no-hw no-engine zlib threads --prefix="${ANDROID_OPENSSL_DIR}")
   (cd ${DEPKGS_DIR}/src/openssl; make depend)
   (cd ${DEPKGS_DIR}/src/openssl; make install)
fi

if [ -z "${SKIP_CLEAN}" ]; then
   (cd ${TOP_DIR}; ./platforms/android/configure_android.sh)
   (cd ${TOP_DIR}; make clean)
   (cd ${TOP_DIR}/src/lib; make)
   (cd ${TOP_DIR}/src/findlib; make)
   (cd ${FILED_DIR}; make static-bacula-fd)
fi

cd ${TRAYMON_DIR}
./make_release_apk.sh
cd ${CWD}
cp -a ${TRAYMON_DIR}/android-build/build/outputs/apk/release/android-build-release.apk ./bacula_${BMOB_ARCH}.apk
