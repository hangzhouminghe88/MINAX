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

/* 
 * 
 *  Kern Sibbald, August 2007
 *
 * Note, some of the original Bacula Windows startup and service handling code
 *  was derived from VNC code that was used in apcupsd then ported to 
 *  Bacula.  However, since then the code has been significantly enhanced 
 *  and largely rewritten.  
 *
 * Evidently due to the nature of Windows startup code and service
 *  handling code, certain similarities remain. Thanks to the original
 *  VNC authors.
 *
 * This is a generic main routine, which is used by all three
 *  of the daemons. Each one compiles it with slightly different
 *  #defines.
 *
 */

#define LOCKMGR_COMPLIANT
#include "bacula.h"
#include "win32.h"
#include <signal.h>
#include <pthread.h>

#include <commctrl.h>


/* Globals */
HINSTANCE appInstance;
DWORD mainthreadId;
bool opt_debug = false;
bool have_service_api;
DWORD service_thread_id = 0;
char win_os[300];

bool GetWindowsVersionString(LPTSTR osbuf, int maxsiz);


#define MAX_COMMAND_ARGS 100
static char *command_args[MAX_COMMAND_ARGS] = {(char *)LC_APP_NAME, NULL};
static int num_command_args = 1;
static pid_t main_pid;
static pthread_t main_tid;

const char usage[] = APP_NAME "[/debug] [/service] [/run] [/kill] [/install] [/remove] [/help]\n";

/*
 *
 * Main Windows entry point.
 *
 * We parse the command line and either calls the main App
 *   or starts up the service.
 */
int WINAPI WinMain(HINSTANCE Instance, HINSTANCE /*PrevInstance*/, PSTR CmdLine, 
                   int /*show*/)
{
   char *cmdLine = CmdLine;
   char *wordPtr, *tempPtr;
   int i, quote;
   OSVERSIONINFO osversioninfo;
   osversioninfo.dwOSVersionInfoSize = sizeof(osversioninfo);

   /* Save the application instance and main thread id */
   appInstance = Instance;
   mainthreadId = GetCurrentThreadId();

   if (GetVersionEx(&osversioninfo) && 
       osversioninfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
      have_service_api = true;
   }

   GetWindowsVersionString(win_os, sizeof(win_os));

   main_pid = getpid();
   main_tid = pthread_self();

   INITCOMMONCONTROLSEX initCC = {
      sizeof(INITCOMMONCONTROLSEX), 
      ICC_STANDARD_CLASSES
   };

   InitCommonControlsEx(&initCC);

   /*
    * Funny things happen with the command line if the
    * execution comes from c:/Program Files/bacula/bacula.exe
    * We get a command line like: Files/bacula/bacula.exe" options
    * I.e. someone stops scanning command line on a space, not
    * realizing that the filename is quoted!!!!!!!!!!
    * So if first character is not a double quote and
    * the last character before first space is a double
    * quote, we throw away the junk.
    */

   wordPtr = cmdLine;
   while (*wordPtr && *wordPtr != ' ')
      wordPtr++;
   if (wordPtr > cmdLine)      /* backup to char before space */
      wordPtr--;
   /* if first character is not a quote and last is, junk it */
   if (*cmdLine != '"' && *wordPtr == '"') {
      cmdLine = wordPtr + 1;
   }

   /*
    * Build Unix style argc *argv[] for the main "Unix" code
    *  stripping out any Windows options 
    */

   /* Don't NULL command_args[0] !!! */
   for (i=1;i<MAX_COMMAND_ARGS;i++) {
      command_args[i] = NULL;
   }

   char *pszArgs = bstrdup(cmdLine);
   wordPtr = pszArgs;
   quote = 0;
   while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
      wordPtr++;
   if (*wordPtr == '\"') {
      quote = 1;
      wordPtr++;
   } else if (*wordPtr == '/') {
      /* Skip Windows options */
      while (*wordPtr && (*wordPtr != ' ' && *wordPtr != '\t'))
         wordPtr++;
      while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
         wordPtr++;
   }
   if (*wordPtr) {
      while (*wordPtr && num_command_args < MAX_COMMAND_ARGS) {
         tempPtr = wordPtr;
         if (quote) {
            while (*tempPtr && *tempPtr != '\"')
               tempPtr++;
            quote = 0;
         } else {
            while (*tempPtr && *tempPtr != ' ')
            tempPtr++;
         }
         if (*tempPtr)
            *(tempPtr++) = '\0';
         command_args[num_command_args++] = wordPtr;
         wordPtr = tempPtr;
         while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
            wordPtr++;
         if (*wordPtr == '\"') {
            quote = 1;
            wordPtr++;
         }
      }
   }

   /*
    * Now process Windows command line options. Most of these options
    *  are single shot -- i.e. we accept one option, do something and
    *  terminate.
    */
   for (i = 0; i < (int)strlen(cmdLine); i++) {
      char *p = &cmdLine[i];

      if (*p <= ' ') {
         continue;                    /* toss junk */
      }

      if (*p != '/') {
         break;                       /* syntax error */
      }

      /* Start as a service? */
      if (strncasecmp(p, "/service", 8) == 0) {
         return baculaServiceMain();      /* yes, run as a service */
      }

      /* Stop any running copy? */
      if (strncasecmp(p, "/kill", 5) == 0) {
         return stopRunningBacula();
      }

      /* Run app as user program? */
      if (strncasecmp(p, "/run", 4) == 0) {
         return BaculaAppMain();         /* yes, run as a user program */
      }

      /* Install Bacula in registry? */
      if (strncasecmp(p, "/install", 8) == 0) {
         return installService(p+8);    /* Pass command options */
      }

      /* Remove Bacula registry entry? */
      if (strncasecmp(p, "/remove", 7) == 0) {
         return removeService();
      }

      /* Set debug mode? -- causes more dialogs to be displayed */
      if (strncasecmp(p, "/debug", 6) == 0) {
         opt_debug = true;
         i += 6;                /* skip /debug */
         continue;
      }

      /* Display help? -- displays usage */
      if (strncasecmp(p, "/help", 5) == 0) {
         MessageBox(NULL, usage, APP_DESC, MB_OK|MB_ICONINFORMATION);
         return 0;
      }
      
      MessageBox(NULL, cmdLine, _("Bad Command Line Option"), MB_OK);

      /* Show the usage dialog */
      MessageBox(NULL, usage, APP_DESC, MB_OK | MB_ICONINFORMATION);

      return 1;
   }
   return BaculaAppMain();
}

#ifndef HAVE_TRAY_MONITOR
/* Minimalist winproc when don't have tray monitor */
LRESULT CALLBACK bacWinProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
   case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
   }
   return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
#endif


/*
 * Called as a thread from BaculaAppMain()
 * Here we handle the Windows messages
 */
void *Main_Msg_Loop(LPVOID lpwThreadParam) 
{
   MSG msg;

   pthread_detach(pthread_self());

   /*
    * Since we are the only thread with a message loop
    * mark ourselves as the service thread so that
    * we can receive all the window events.
    */
   service_thread_id = GetCurrentThreadId();

#ifdef HAVE_TRAY_MONITOR
   /* Create tray icon & menu if we're running as an app */
   trayMonitor *monitor = new trayMonitor();
   if (monitor == NULL) {
      PostQuitMessage(0);
   }

#else
   /* Create a window to handle Windows messages */
   WNDCLASSEX baclass;

   baclass.cbSize         = sizeof(baclass);
   baclass.style          = 0;
   baclass.lpfnWndProc    = bacWinProc;
   baclass.cbClsExtra     = 0;
   baclass.cbWndExtra     = 0;
   baclass.hInstance      = appInstance;
   baclass.hIcon          = NULL;
   baclass.hCursor        = NULL;
   baclass.hbrBackground  = NULL;
   baclass.lpszMenuName   = NULL;
   baclass.lpszClassName  = APP_NAME;
   baclass.hIconSm        = NULL;

   RegisterClassEx(&baclass);

   if (CreateWindow(APP_NAME, APP_NAME, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                NULL, NULL, appInstance, NULL) == NULL) {
      PostQuitMessage(0);
   }
#endif

   /* Now enter the Windows message handling loop until told to quit! */
   while (GetMessage(&msg, NULL, 0,0) ) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   /* If we get here, we are shutting down */

#ifdef HAVE_TRAY_MONITOR
   if (monitor != NULL) {
      delete monitor;
   }
#endif

   if (have_service_api) {
      /* Mark that we're no longer running */
      service_thread_id = 0;
      /* Tell the service manager that we've stopped. */
      ReportStatus(SERVICE_STOPPED, service_error, 0);
   }  
   /* Tell main "Unix" program to go away */
   terminate_app(0);

   /* Should not get here */
   pthread_kill(main_tid, SIGTERM);   /* ask main thread to terminate */
   sleep(1);
   kill(main_pid, SIGTERM);           /* kill main thread */
   _exit(0);
}

/*
 * This is the main routine for Bacula when running as an application,
 *  or after the service has started up.
 */
int BaculaAppMain()
{
   pthread_t tid;
   DWORD dwCharsWritten;

   OSDependentInit();
   /* If no arguments were given then just run */
   if (p_AttachConsole == NULL || !p_AttachConsole(ATTACH_PARENT_PROCESS)) {
      if (opt_debug) {
         AllocConsole();
      }
   }

   WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &dwCharsWritten, NULL);

   /* Startup networking */
   WSA_Init();

   /* Set this process to be the last application to be shut down. */
   if (p_SetProcessShutdownParameters) {
      p_SetProcessShutdownParameters(0x100, 0);
   }

   /* Create a thread to handle the Windows messages */
   pthread_create(&tid, NULL,  Main_Msg_Loop, (void *)0);

   /* Call the Unix Bacula daemon */
   BaculaMain(num_command_args, command_args);
   PostQuitMessage(0);                /* terminate our main message loop */

   WSACleanup();
   _exit(0);
}


void pause_msg(const char *file, const char *func, int line, const char *msg)
{
   char buf[1000];
   if (msg) {
      bsnprintf(buf, sizeof(buf), "%s:%s:%d %s", file, func, line, msg);
   } else {
      bsnprintf(buf, sizeof(buf), "%s:%s:%d", file, func, line);
   }
   MessageBox(NULL, buf, "Pause", MB_OK);
}

#include <tchar.h>
#include <stdio.h>

#ifndef PRODUCT_UNLICENSED
#define PRODUCT_UNLICENSED 0xABCDABCD
#define PRODUCT_BUSINESS 0x00000006
#define PRODUCT_BUSINESS_N 0x00000010
#define PRODUCT_CLUSTER_SERVER 0x00000012
#define PRODUCT_DATACENTER_SERVER 0x00000008
#define PRODUCT_DATACENTER_SERVER_CORE 0x0000000C
#define PRODUCT_DATACENTER_SERVER_CORE_V 0x00000027
#define PRODUCT_DATACENTER_SERVER_V 0x00000025
#define PRODUCT_ENTERPRISE 0x00000004
#define PRODUCT_ENTERPRISE_E 0x00000046
#define PRODUCT_ENTERPRISE_N 0x0000001B
#define PRODUCT_ENTERPRISE_SERVER 0x0000000A
#define PRODUCT_ENTERPRISE_SERVER_CORE 0x0000000E
#define PRODUCT_ENTERPRISE_SERVER_CORE_V 0x00000029
#define PRODUCT_ENTERPRISE_SERVER_IA64 0x0000000F
#define PRODUCT_ENTERPRISE_SERVER_V 0x00000026
#define PRODUCT_HOME_BASIC 0x00000002
#define PRODUCT_HOME_BASIC_E 0x00000043
#define PRODUCT_HOME_BASIC_N 0x00000005
#define PRODUCT_HOME_PREMIUM 0x00000003
#define PRODUCT_HOME_PREMIUM_E 0x00000044
#define PRODUCT_HOME_PREMIUM_N 0x0000001A
#define PRODUCT_HYPERV 0x0000002A
#define PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT 0x0000001E
#define PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING 0x00000020
#define PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY 0x0000001F
#define PRODUCT_PROFESSIONAL 0x00000030
#define PRODUCT_PROFESSIONAL_E 0x00000045
#define PRODUCT_PROFESSIONAL_N 0x00000031
#define PRODUCT_SERVER_FOR_SMALLBUSINESS 0x00000018
#define PRODUCT_SERVER_FOR_SMALLBUSINESS_V 0x00000023
#define PRODUCT_SERVER_FOUNDATION 0x00000021
#define PRODUCT_SMALLBUSINESS_SERVER 0x00000009
#define PRODUCT_SOLUTION_EMBEDDEDSERVER 0x00000038
#define PRODUCT_STANDARD_SERVER 0x00000007
#define PRODUCT_STANDARD_SERVER_CORE 0x0000000D
#define PRODUCT_STANDARD_SERVER_CORE_V 0x00000028
#define PRODUCT_STANDARD_SERVER_V 0x00000024
#define PRODUCT_STARTER 0x0000000B
#define PRODUCT_STARTER_E 0x00000042
#define PRODUCT_STARTER_N 0x0000002F
#define PRODUCT_STORAGE_ENTERPRISE_SERVER 0x00000017
#define PRODUCT_STORAGE_EXPRESS_SERVER 0x00000014
#define PRODUCT_STORAGE_STANDARD_SERVER 0x00000015
#define PRODUCT_STORAGE_WORKGROUP_SERVER  0x00000016
#define PRODUCT_UNDEFINED 0x00000000
#define PRODUCT_ULTIMATE 0x00000001
#define PRODUCT_ULTIMATE_E 0x00000047
#define PRODUCT_ULTIMATE_N 0x0000001C
#define PRODUCT_WEB_SERVER 0x00000011
#define PRODUCT_WEB_SERVER_CORE 0x0000001D

#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM 0x19
#define SM_SERVERR2 89
#define VER_SERVER_NT 0x80000000

#define PRODUCT_CLUSTER_SERVER_V 0x00000040
#define PRODUCT_ENTERPRISE_EVALUATION 0x00000048
#define PRODUCT_ENTERPRISE_N_EVALUATION 0x00000054
#define PRODUCT_ENTERPRISE_S 0x0000007D
#define PRODUCT_ENTERPRISE_S_EVALUATION 0x00000081
#define PRODUCT_ENTERPRISE_S_N 0x0000007E
#define PRODUCT_ENTERPRISE_S_N_EVALUATION 0x00000082
#define PRODUCT_HOME_PREMIUM_SERVER 0x00000022
#define PRODUCT_HOME_SERVER 0x00000013
#define PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE 0x0000002E
#define PRODUCT_STORAGE_EXPRESS_SERVER_CORE 0x0000002B
#define PRODUCT_STORAGE_STANDARD_SERVER_CORE 0x0000002C
#define PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER 0x00000060
#define PRODUCT_STORAGE_WORKGROUP_SERVER_CORE 0x0000002D
#define PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER 0x0000005F
#define PRODUCT_CORE 0x00000065
#define PRODUCT_CORE_COUNTRYSPECIFIC 0x00000063
#define PRODUCT_CORE_N 0x00000062
#define PRODUCT_CORE_SINGLELANGUAGE 0x00000064
#define PRODUCT_DATACENTER_EVALUATION_SERVER 0x00000050
#define PRODUCT_DATACENTER_A_SERVER_CORE 0x00000091
#define PRODUCT_STANDARD_A_SERVER_CORE 0x00000092
#define PRODUCT_EDUCATION 0x00000079
#define PRODUCT_EDUCATION_N 0x0000007A
#define PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL 0x0000003C
#define PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC 0x0000003E
#define PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT 0x0000003B
#define PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC 0x0000003D
#define PRODUCT_IOTUAP 0x0000007B
#define PRODUCT_IOTUAPCOMMERCIAL 0x00000083
#define PRODUCT_MOBILE_CORE 0x00000068
#define PRODUCT_MOBILE_ENTERPRISE 0x00000085
#define PRODUCT_MULTIPOINT_PREMIUM_SERVER 0x0000004D
#define PRODUCT_MULTIPOINT_STANDARD_SERVER 0x0000004C
#define PRODUCT_PRO_WORKSTATION 0x000000A1
#define PRODUCT_PRO_WORKSTATION_N 0x000000A2
#define PRODUCT_PROFESSIONAL_WMC 0x00000067
#define PRODUCT_SB_SOLUTION_SERVER 0x00000032
#define PRODUCT_SB_SOLUTION_SERVER_EM 0x00000036
#define PRODUCT_SERVER_FOR_SB_SOLUTIONS 0x00000033
#define PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM 0x00000037
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE 0x0000003F
#define PRODUCT_STANDARD_EVALUATION_SERVER 0x0000004F
#define PRODUCT_STANDARD_SERVER_SOLUTIONS 0x00000034
#define PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE 0x00000035

#endif

#define PRODUCT_DATACENTER_A_SERVER_CORE 0x00000091
#define PRODUCT_IOTUAPCOMMERCIAL 0x00000083
#define PRODUCT_PRO_WORKSTATION 0x000000A1
#define PRODUCT_PRO_WORKSTATION_N 0x000000A2
#define PRODUCT_STANDARD_A_SERVER_CORE 0x00000092




#ifndef PRODUCT_PROFESSIONAL
#define PRODUCT_PROFESSIONAL 0x00000030
#endif
#ifndef VER_SUITE_STORAGE_SERVER
#define VER_SUITE_STORAGE_SERVER 0x00002000
#endif
#ifndef VER_SUITE_COMPUTE_SERVER
#define VER_SUITE_COMPUTE_SERVER 0x00004000
#endif

/* Unknown value */
#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER -1
#endif

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

/*
 * Get Windows version display string
 */
bool GetWindowsVersionString(LPTSTR osbuf, int maxsiz)
{
   OSVERSIONINFOEX osvi;
   SYSTEM_INFO si;
   PGNSI pGNSI;
   PGPI pGPI;
   BOOL bOsVersionInfoEx;
   DWORD dwType;

   memset(&si, 0, sizeof(SYSTEM_INFO));
   memset(&osvi, 0, sizeof(OSVERSIONINFOEX));

   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
      return 1;

   // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

   pGNSI = (PGNSI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), 
           "GetNativeSystemInfo");
   if (pGNSI) {
      pGNSI(&si);
   } else {
      GetSystemInfo(&si);
   }

   if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion > 4) { 
      bstrncpy(osbuf, TEXT("Microsoft "), maxsiz);

      // Test for the specific product.

      if (osvi.dwMajorVersion == 10) {
         if (osvi.dwMinorVersion == 0) {
            if (osvi.wProductType == VER_NT_WORKSTATION)
                bstrncat(osbuf, TEXT("Windows 10 "), maxsiz);
            else 
                bstrncat(osbuf, TEXT("Windows Server 2016 " ), maxsiz);
         }

         pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), 
            "GetProductInfo");

         if (pGPI) {
            pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
         } else {
            dwType = PRODUCT_HOME_BASIC;
         }

         switch (dwType) {
            case PRODUCT_ULTIMATE:
               bstrncat(osbuf, TEXT("Ultimate Edition" ), maxsiz);
               break;
            case PRODUCT_PROFESSIONAL:
               bstrncat(osbuf, TEXT("Professional" ), maxsiz);
               break;
            case PRODUCT_HOME_PREMIUM:
               bstrncat(osbuf, TEXT("Home Premium Edition" ), maxsiz);
               break;
            case PRODUCT_HOME_BASIC:
               bstrncat(osbuf, TEXT("Home Basic Edition" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE:
               bstrncat(osbuf, TEXT("Enterprise Edition" ), maxsiz);
               break;
            case PRODUCT_BUSINESS:
               bstrncat(osbuf, TEXT("Business Edition" ), maxsiz);
               break;
            case PRODUCT_BUSINESS_N:
               bstrncat(osbuf, TEXT("Business Edition N" ), maxsiz);
               break;
            case PRODUCT_CORE:
               bstrncat(osbuf, TEXT("Home Edition" ), maxsiz);
               break;
            case PRODUCT_CORE_COUNTRYSPECIFIC:
               bstrncat(osbuf, TEXT("Home China Edition" ), maxsiz);
               break;
            case PRODUCT_CORE_N:
               bstrncat(osbuf, TEXT("Home N Edition" ), maxsiz);
               break;
            case PRODUCT_CORE_SINGLELANGUAGE:
               bstrncat(osbuf, TEXT("Home Single Language Edition" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_EVALUATION_SERVER:
               bstrncat(osbuf, TEXT("Server Datacenter (evaluation installation)" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_A_SERVER_CORE:
               bstrncat(osbuf, TEXT("Server Datacenter, Semi-Annual Channel (core installation)" ), maxsiz);
               break;
            case PRODUCT_STANDARD_A_SERVER_CORE:
               bstrncat(osbuf, TEXT("Server Standard, Semi-Annual Channel (core installation)" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER_CORE_V:
               bstrncat(osbuf, TEXT("Server Datacenter without Hyper-V (core installation)" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER_V:
               bstrncat(osbuf, TEXT("Server Datacenter without Hyper-V (full installation)" ), maxsiz);
               break;
            case PRODUCT_EDUCATION:
               bstrncat(osbuf, TEXT("Education" ), maxsiz);
               break;
            case PRODUCT_EDUCATION_N:
               bstrncat(osbuf, TEXT("Education N" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_E:
               bstrncat(osbuf, TEXT("Enterprise E" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_EVALUATION:
               bstrncat(osbuf, TEXT("Enterprise Evaluation" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_N:
               bstrncat(osbuf, TEXT("Enterprise N" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_N_EVALUATION:
               bstrncat(osbuf, TEXT("Enterprise N Evaluation" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_S:
               bstrncat(osbuf, TEXT("Enterprise 2015 LTSB" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_S_EVALUATION:
               bstrncat(osbuf, TEXT("Enterprise 2015 LTSB Evaluation" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_S_N:
               bstrncat(osbuf, TEXT("Enterprise 2015 LTSB N" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_S_N_EVALUATION:
               bstrncat(osbuf, TEXT("Enterprise 2015 LTSB N Evaluation" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER:
               bstrncat(osbuf, TEXT("Enterprise (full installation)" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE_V:
               bstrncat(osbuf, TEXT("Server Enterprise without Hyper-V (core installation)" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
               bstrncat(osbuf, TEXT("Server Enterprise for Itanium-based Systems" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_V:
               bstrncat(osbuf, TEXT("Server Enterprise without Hyper-V (full installation)" ), maxsiz);
               break;
            case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL:
               bstrncat(osbuf, TEXT("Essential Server Solution Additional" ), maxsiz);
               break;
            case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC:
               bstrncat(osbuf, TEXT("Essential Server Solution Additional SVC" ), maxsiz);
               break;
            case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT:
               bstrncat(osbuf, TEXT("Essential Server Solution Management" ), maxsiz);
               break;
            case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC:
               bstrncat(osbuf, TEXT("Essential Server Solution Management SVC" ), maxsiz);
               break;
            case PRODUCT_HOME_BASIC_E:
               bstrncat(osbuf, TEXT("Not supported" ), maxsiz);
               break;
            case PRODUCT_HOME_BASIC_N:
               bstrncat(osbuf, TEXT("Home Basic N" ), maxsiz);
               break;
            case PRODUCT_HOME_PREMIUM_E:
               bstrncat(osbuf, TEXT("Not supported" ), maxsiz);
               break;
            case PRODUCT_HOME_PREMIUM_N:
               bstrncat(osbuf, TEXT("Home Premium N" ), maxsiz);
               break;
            case PRODUCT_HOME_PREMIUM_SERVER:
               bstrncat(osbuf, TEXT("Home Server 2011" ), maxsiz);
               break;
            case PRODUCT_HOME_SERVER:
               bstrncat(osbuf, TEXT("Storage Server 2008 R2 Essentials" ), maxsiz);
               break;
            case PRODUCT_HYPERV:
               bstrncat(osbuf, TEXT("Hyper-V Server" ), maxsiz);
               break;
            case PRODUCT_IOTUAP:
               bstrncat(osbuf, TEXT("IoT Core" ), maxsiz);
               break;
            case PRODUCT_IOTUAPCOMMERCIAL:
               bstrncat(osbuf, TEXT("IoT Core Commercial" ), maxsiz);
               break;
            case PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT:
               bstrncat(osbuf, TEXT("Essential Business Server Management Server" ), maxsiz);
               break;
            case PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING:
               bstrncat(osbuf, TEXT("Essential Business Server Messaging Server" ), maxsiz);
               break;
            case PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY:
               bstrncat(osbuf, TEXT("Essential Business Server Security Server" ), maxsiz);
               break;
            case PRODUCT_MOBILE_CORE:
               bstrncat(osbuf, TEXT("Mobile" ), maxsiz);
               break;
            case PRODUCT_MOBILE_ENTERPRISE:
               bstrncat(osbuf, TEXT("Mobile Enterprise" ), maxsiz);
               break;
            case PRODUCT_MULTIPOINT_PREMIUM_SERVER:
               bstrncat(osbuf, TEXT("MultiPoint Server Premium (full installation)" ), maxsiz);
               break;
            case PRODUCT_MULTIPOINT_STANDARD_SERVER:
               bstrncat(osbuf, TEXT("MultiPoint Server Standard (full installation)" ), maxsiz);
               break;
            case PRODUCT_PRO_WORKSTATION:
               bstrncat(osbuf, TEXT("Pro for Workstations" ), maxsiz);
               break;
            case PRODUCT_PRO_WORKSTATION_N:
               bstrncat(osbuf, TEXT("Pro for Workstations N" ), maxsiz);
               break;
            case PRODUCT_PROFESSIONAL_E:
               bstrncat(osbuf, TEXT("Not supported" ), maxsiz);
               break;
            case PRODUCT_PROFESSIONAL_N:
               bstrncat(osbuf, TEXT("Pro N" ), maxsiz);
               break;
            case PRODUCT_PROFESSIONAL_WMC:
               bstrncat(osbuf, TEXT("Professional with Media Center" ), maxsiz);
               break;
            case PRODUCT_SB_SOLUTION_SERVER:
               bstrncat(osbuf, TEXT("Small Business Server 2011 Essentials" ), maxsiz);
               break;
            case PRODUCT_SB_SOLUTION_SERVER_EM:
               bstrncat(osbuf, TEXT("Server For SB Solutions EM" ), maxsiz);
               break;
            case PRODUCT_SERVER_FOR_SB_SOLUTIONS:
               bstrncat(osbuf, TEXT("Server For SB Solutions" ), maxsiz);
               break;
            case PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM:
               bstrncat(osbuf, TEXT("Server For SB Solutions EM" ), maxsiz);
               break;
            case PRODUCT_SERVER_FOR_SMALLBUSINESS:
               bstrncat(osbuf, TEXT("for Windows Essential Server Solutions" ), maxsiz);
               break;
            case PRODUCT_SERVER_FOR_SMALLBUSINESS_V:
               bstrncat(osbuf, TEXT("without Hyper-V for Windows Essential Server Solutions" ), maxsiz);
               break;
            case PRODUCT_SERVER_FOUNDATION:
               bstrncat(osbuf, TEXT("Server Foundation" ), maxsiz);
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE:
               bstrncat(osbuf, TEXT("Small Business Server Premium (core installation)" ), maxsiz);
               break;
            case PRODUCT_SOLUTION_EMBEDDEDSERVER:
               bstrncat(osbuf, TEXT("Windows MultiPoint Server" ), maxsiz);
               break;
            case PRODUCT_STANDARD_EVALUATION_SERVER:
               bstrncat(osbuf, TEXT("Server Standard (evaluation installation)" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER_CORE_V:
               bstrncat(osbuf, TEXT("Server Standard without Hyper-V" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER_SOLUTIONS:
               bstrncat(osbuf, TEXT("Server Solutions Premium" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE:
               bstrncat(osbuf, TEXT("Server Solutions Premium (core installation)" ), maxsiz);
               break;
            case PRODUCT_STARTER_E:
               bstrncat(osbuf, TEXT("Not supported" ), maxsiz);
               break;
            case PRODUCT_STARTER_N:
               bstrncat(osbuf, TEXT("Starter N" ), maxsiz);
               break;
            case PRODUCT_STORAGE_ENTERPRISE_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Enterprise" ), maxsiz);
               break;
            case PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:
               bstrncat(osbuf, TEXT("Storage Server Enterprise (core installation)" ), maxsiz);
               break;
            case PRODUCT_STORAGE_EXPRESS_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Express" ), maxsiz);
               break;
            case PRODUCT_STORAGE_EXPRESS_SERVER_CORE:
               bstrncat(osbuf, TEXT("Storage Server Express (core installation)" ), maxsiz);
               break;
            case PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Standard (evaluation installation)" ), maxsiz);
               break;
            case PRODUCT_STORAGE_STANDARD_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Standard" ), maxsiz);
               break;
            case PRODUCT_STORAGE_STANDARD_SERVER_CORE:
               bstrncat(osbuf, TEXT("Storage Server Standard (core installation)" ), maxsiz);
               break;
            case PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Workgroup (evaluation installation)" ), maxsiz);
               break;
            case PRODUCT_STORAGE_WORKGROUP_SERVER:
               bstrncat(osbuf, TEXT("Storage Server Workgroup" ), maxsiz);
               break;
            case PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:
               bstrncat(osbuf, TEXT("Storage Server Workgroup (core installation)" ), maxsiz);
               break;
            case PRODUCT_ULTIMATE_E:
               bstrncat(osbuf, TEXT("Not supported" ), maxsiz);
               break;
            case PRODUCT_ULTIMATE_N:
               bstrncat(osbuf, TEXT("Ultimate N" ), maxsiz);
               break;
            case PRODUCT_UNDEFINED:
               bstrncat(osbuf, TEXT("Unknown product" ), maxsiz);
               break;
            case PRODUCT_WEB_SERVER_CORE:
               bstrncat(osbuf, TEXT("Web Server (core installation)" ), maxsiz);
               break;
            case PRODUCT_STARTER:
               bstrncat(osbuf, TEXT("Starter Edition" ), maxsiz);
               break;
            case PRODUCT_CLUSTER_SERVER_V:
               bstrncat(osbuf, TEXT("Server Hyper Core V" ), maxsiz);
               break;
            case PRODUCT_CLUSTER_SERVER:
               bstrncat(osbuf, TEXT("Cluster Server Edition" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER:
               bstrncat(osbuf, TEXT("Datacenter Edition" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER_CORE:
               bstrncat(osbuf, TEXT("Datacenter Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
               bstrncat(osbuf, TEXT("Enterprise Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_SMALLBUSINESS_SERVER:
               bstrncat(osbuf, TEXT("Small Business Server" ), maxsiz);
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
               bstrncat(osbuf, TEXT("Small Business Server Premium Edition" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER:
               bstrncat(osbuf, TEXT("Standard Edition" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER_CORE:
               bstrncat(osbuf, TEXT("Standard Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_WEB_SERVER:
               bstrncat(osbuf, TEXT("Web Server Edition" ), maxsiz);
               break;
         }
      }
      if (osvi.dwMajorVersion == 6) {
         if (osvi.dwMinorVersion == 0) {
            if (osvi.wProductType == VER_NT_WORKSTATION)
                bstrncat(osbuf, TEXT("Windows Vista "), maxsiz);
            else 
                bstrncat(osbuf, TEXT("Windows Server 2008 " ), maxsiz);
         }

         if (osvi.dwMinorVersion == 1) {
            if (osvi.wProductType == VER_NT_WORKSTATION )
                bstrncat(osbuf, TEXT("Windows 7 "), maxsiz);
            else 
                bstrncat(osbuf, TEXT("Windows Server 2008 R2 " ), maxsiz);
         }
         if (osvi.dwMinorVersion == 2) {
            if (osvi.wProductType == VER_NT_WORKSTATION )
                bstrncat(osbuf, TEXT("Windows 8 "), maxsiz);
            else 
                bstrncat(osbuf, TEXT("Windows Server 2012 " ), maxsiz);
         }
         if (osvi.dwMinorVersion == 3) {
            if (osvi.wProductType == VER_NT_WORKSTATION )
                bstrncat(osbuf, TEXT("Windows 8.1 "), maxsiz);
            else 
                bstrncat(osbuf, TEXT("Windows Server 2012 RC2 " ), maxsiz);
         }
         
         pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), 
            "GetProductInfo");

         if (pGPI) {
            pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
         } else {
            dwType = PRODUCT_HOME_BASIC;
         }

         switch (dwType) {
            case PRODUCT_ULTIMATE:
               bstrncat(osbuf, TEXT("Ultimate Edition" ), maxsiz);
               break;
            case PRODUCT_PROFESSIONAL:
               bstrncat(osbuf, TEXT("Professional" ), maxsiz);
               break;
            case PRODUCT_HOME_PREMIUM:
               bstrncat(osbuf, TEXT("Home Premium Edition" ), maxsiz);
               break;
            case PRODUCT_HOME_BASIC:
               bstrncat(osbuf, TEXT("Home Basic Edition" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE:
               bstrncat(osbuf, TEXT("Enterprise Edition" ), maxsiz);
               break;
            case PRODUCT_BUSINESS:
               bstrncat(osbuf, TEXT("Business Edition" ), maxsiz);
               break;
            case PRODUCT_STARTER:
               bstrncat(osbuf, TEXT("Starter Edition" ), maxsiz);
               break;
            case PRODUCT_CLUSTER_SERVER:
               bstrncat(osbuf, TEXT("Cluster Server Edition" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER:
               bstrncat(osbuf, TEXT("Datacenter Edition" ), maxsiz);
               break;
            case PRODUCT_DATACENTER_SERVER_CORE:
               bstrncat(osbuf, TEXT("Datacenter Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER:
               bstrncat(osbuf, TEXT("Enterprise Edition" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
               bstrncat(osbuf, TEXT("Enterprise Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
               bstrncat(osbuf, TEXT("Enterprise Edition for Itanium-based Systems" ), maxsiz);
               break;
            case PRODUCT_SMALLBUSINESS_SERVER:
               bstrncat(osbuf, TEXT("Small Business Server" ), maxsiz);
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
               bstrncat(osbuf, TEXT("Small Business Server Premium Edition" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER:
               bstrncat(osbuf, TEXT("Standard Edition" ), maxsiz);
               break;
            case PRODUCT_STANDARD_SERVER_CORE:
               bstrncat(osbuf, TEXT("Standard Edition (core installation)" ), maxsiz);
               break;
            case PRODUCT_WEB_SERVER:
               bstrncat(osbuf, TEXT("Web Server Edition" ), maxsiz);
               break;
         }
      }

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2) {
         if( GetSystemMetrics(SM_SERVERR2) )
            bstrncat(osbuf, TEXT( "Windows Server 2003 R2 "), maxsiz);
         else if (osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER)
            bstrncat(osbuf, TEXT( "Windows Storage Server 2003"), maxsiz);
         else if (osvi.wSuiteMask & VER_SUITE_WH_SERVER )
            bstrncat(osbuf, TEXT( "Windows Home Server"), maxsiz);
         else if (osvi.wProductType == VER_NT_WORKSTATION &&
                  si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
            bstrncat(osbuf, TEXT( "Windows XP Professional x64 Edition"), maxsiz);
         else 
            bstrncat(osbuf, TEXT("Windows Server 2003 "), maxsiz);

         // Test for the server type.
         if (osvi.wProductType != VER_NT_WORKSTATION) {
            if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64) {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   bstrncat(osbuf, TEXT( "Datacenter Edition for Itanium-based Systems" ), maxsiz);
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   bstrncat(osbuf, TEXT( "Enterprise Edition for Itanium-based Systems" ), maxsiz);
            }

            else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64) {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   bstrncat(osbuf, TEXT( "Datacenter x64 Edition" ), maxsiz);
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   bstrncat(osbuf, TEXT( "Enterprise x64 Edition" ), maxsiz);
                else bstrncat(osbuf, TEXT( "Standard x64 Edition" ), maxsiz);
            } else {
                if ( osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
                   bstrncat(osbuf, TEXT( "Compute Cluster Edition" ), maxsiz);
                else if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   bstrncat(osbuf, TEXT( "Datacenter Edition" ), maxsiz);
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   bstrncat(osbuf, TEXT( "Enterprise Edition" ), maxsiz);
                else if ( osvi.wSuiteMask & VER_SUITE_BLADE )
                   bstrncat(osbuf, TEXT( "Web Edition" ), maxsiz);
                else bstrncat(osbuf, TEXT( "Standard Edition" ), maxsiz);
            }
         }
      }

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1) {
         bstrncat(osbuf, TEXT("Windows XP "), maxsiz);
         if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
            bstrncat(osbuf, TEXT( "Home Edition" ), maxsiz);
         else 
            bstrncat(osbuf, TEXT( "Professional" ), maxsiz);
      }

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) {
         bstrncat(osbuf, TEXT("Windows 2000 "), maxsiz);
         if ( osvi.wProductType == VER_NT_WORKSTATION ) {
            bstrncat(osbuf, TEXT( "Professional" ), maxsiz);
         } else {
            if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
               bstrncat(osbuf, TEXT( "Datacenter Server" ), maxsiz);
            else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
               bstrncat(osbuf, TEXT( "Advanced Server" ), maxsiz);
            else bstrncat(osbuf, TEXT( "Server" ), maxsiz);
         }
      }

       // Include service pack (if any) and build number.

      if (_tcslen(osvi.szCSDVersion) > 0) {
          bstrncat(osbuf, TEXT(" ") , maxsiz);
          bstrncat(osbuf, osvi.szCSDVersion, maxsiz);
      }

      char buf[80];

      snprintf(buf, 80, " (build %d)", (int)osvi.dwBuildNumber);
      bstrncat(osbuf, buf, maxsiz);

      if (osvi.dwMajorVersion >= 6) {
         if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            bstrncat(osbuf, TEXT( ", 64-bit" ), maxsiz);
         else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
            bstrncat(osbuf, TEXT(", 32-bit"), maxsiz);
      }
      
      return true; 
   } else {
      bstrncpy(osbuf, "Unknown Windows version.", maxsiz);
      return true;
   }
}
