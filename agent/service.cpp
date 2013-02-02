/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "service.hpp"
#include "string.h"
#include <fstream>
#include "dlib/logger.h"
#include "version.h"

static dlib::logger sLogger("init.service");

#ifdef _WINDOWS
#define stricmp _stricmp
#define snprintf _snprintf
#define strncasecmp _strnicmp
#else
#define strncpy_s strncpy
#endif

MTConnectService::MTConnectService() :
mIsService(false), mIsDebug(false)
{
}

void MTConnectService::initialize(int aArgc, const char *aArgv[])
{
}

#ifdef _WINDOWS

#include <windows.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <dlib/threads.h>

#pragma comment(lib, "advapi32.lib")

#define SVC_ERROR                       ((DWORD)0xC0000001L)
#define SVC_WARNING                     ((DWORD)0x90000001L)
#define SVC_INFO                        ((DWORD)0x50000001L)

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

VOID WINAPI SvcCtrlHandler( DWORD ); 
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
VOID SvcReportEvent( LPTSTR );

static MTConnectService *gService = NULL;

static void agent_termination_handler()
{
    
}

void commandLine()
{
  puts("> ");
  char line[1024];
  while(gets(line) != NULL) {
    if (strncasecmp(line, "QUIT", 4) == 0) {
      gService->stop();
      return;
    }
  }
}

int MTConnectService::main(int argc, const char *argv[]) 
{
  std::set_terminate(agent_termination_handler);
  PrintMTConnectAgentVersion();
  try 
  {
    // If command-line parameter is "install", install the service. If debug or run
    // is specified than just run it as a command line process. 
    // Otherwise, the service is probably being started by the SCM.
    if(argc > 1) {
      if (stricmp( argv[1], "help") == 0 || strncmp(argv[1], "-h", 2) == 0)
      {
        printf("Usage: agent [help|install|debug|run] [configuration_file]\n"
               "       help           Prints this message\n"
               "       install        Installs the service\n"
               "                      install with -h will display additional options\n"
               "       remove         Remove the service\n"
               "       debug          Runs the agent on the command line with verbose logging\n"
               "       run            Runs the agent on the command line\n"
               "       config_file    The configuration file to load\n"
               "                      Default: agent.cfg in current directory\n\n"
               "When the agent is started without any arguments it is assumed it will be running\n"
               "as a service and will begin the service initialization sequence\n");
        exit(0);
      } else if (stricmp( argv[1], "install") == 0 ) {
        initialize(argc - 2, argv + 2);
        install();
        return 0;
      } else if (stricmp( argv[1], "remove") == 0 ) {
        initialize(argc - 2, argv + 2);
        remove();
        return 0;
      } else if (stricmp( argv[1], "debug") == 0 || stricmp( argv[1], "run") == 0) {
        if (stricmp( argv[1], "debug") == 0)
          mIsDebug = true;
          
        initialize(argc - 2, argv + 2);
        start();
        dlib::thread_function cmd(commandLine);
        return 0;
      }
    }
  
    gService = this;
    mIsService = true;
    SERVICE_TABLE_ENTRY DispatchTable[] = 
      { 
        {  "", (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
        { NULL, NULL } 
      }; 

    if (!StartServiceCtrlDispatcher( DispatchTable )) 
    { 
      SvcReportEvent("StartServiceCtrlDispatcher"); 
    } 
  }
  catch (std::exception & e)
  {
    sLogger << dlib::LFATAL << "Agent top level exception: " << e.what();
    std::cerr << "Agent top level exception: " << e.what() << std::endl;
  }
  catch (std::string &s)
  {
    sLogger << dlib::LFATAL << "Agent top level exception: " << s;
    std::cerr << "Agent top level exception: " << s << std::endl;
  }
  
  return 0;
} 

void MTConnectService::install()
{
  SC_HANDLE manager;
  SC_HANDLE service;
  char path[MAX_PATH];

  if( !GetModuleFileName(NULL, path, MAX_PATH ) )
  {
    sLogger << dlib::LERROR << "Cannot install service (" << GetLastError() << ")";
    return;
  }

// Get a handle to the SCM database. 

  manager = OpenSCManager( 
    NULL,                    // local computer
    NULL,                    // ServicesActive database 
    SC_MANAGER_ALL_ACCESS);  // full access rights 

  if (NULL == manager) 
  {
    sLogger << dlib::LERROR << "OpenSCManager failed (" << GetLastError() << ")";
    return;
  }

  service = OpenService(manager, mName.c_str(), SC_MANAGER_ALL_ACCESS);
  if (service != NULL) {
    if (!ChangeServiceConfig( 
      service,            // handle of service 
      SERVICE_NO_CHANGE,     // service type: no change 
      SERVICE_NO_CHANGE,  // service start type 
      SERVICE_NO_CHANGE,     // error control: no change 
      path,                  // binary path: no change 
      NULL,                  // load order group: no change 
      NULL,                  // tag ID: no change 
      NULL,                  // dependencies: no change 
      NULL,                  // account name: no change 
      NULL,                  // password: no change 
      NULL) )                // display name: no change
    {
      sLogger << dlib::LERROR << "ChangeServiceConfig failed (" << GetLastError() << ")";
      CloseServiceHandle(manager);
      return;
    } 
  } else {
    // Create the service
    service = CreateService( 
      manager,              // SCM database 
      mName.c_str(),                   // name of service 
      mName.c_str(),                   // service name to display 
      SERVICE_ALL_ACCESS,        // desired access 
      SERVICE_WIN32_OWN_PROCESS, // service type 
      SERVICE_AUTO_START,      // start type 
      SERVICE_ERROR_NORMAL,      // error control type 
      path,                    // path to service's binary 
      NULL,                      // no load ordering group 
      NULL,                      // no tag identifier 
      NULL,                      // no dependencies 
      NULL,                      // LocalSystem account 
      NULL);                     // no password 

    if (service == NULL) 
    {
      sLogger << dlib::LERROR << "CreateService failed (" << GetLastError() << ")";
      CloseServiceHandle(manager);
      return;
    }
  }

  CloseServiceHandle(service); 
  CloseServiceHandle(manager);

  HKEY software;
  LONG res = RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE", &software);
  if (res != ERROR_SUCCESS)
  {
    sLogger << dlib::LERROR << "Could not open software key (" << res << ")";
    return;
  }

  HKEY mtc;
  res = RegOpenKey(software, "MTConnect", &mtc);
  if (res != ERROR_SUCCESS)
  {
    res = RegCreateKey(software, "MTConnect", &mtc);
    RegCloseKey(software);
    if (res != ERROR_SUCCESS)
    {
      sLogger << dlib::LERROR << "Could not create MTConnect (" << res << ")";
      return;
    }
  }
  RegCloseKey(software);

  // Create Service Key
  HKEY agent;
  res = RegOpenKey(mtc, mName.c_str(), &agent);
  if (res != ERROR_SUCCESS)
  {
    res = RegCreateKey(mtc, mName.c_str(), &agent);
    if (res != ERROR_SUCCESS)
    {
      RegCloseKey(mtc);
      sLogger << dlib::LERROR << "Could not create " << mName << " (" << res << ")";
      return;
    }
  }
  RegCloseKey(mtc);

  // Fully qualify the configuration file name.
  if (mConfigFile[0] != '/' && mConfigFile[0] != '\\' && mConfigFile[1] != ':')
  {
    // Relative file name
    char path[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, path);
    mConfigFile = ((std::string) path) + "\\" + mConfigFile;
  }

  RegSetValueEx(agent, "ConfigurationFile", 0, REG_SZ, (const BYTE*) mConfigFile.c_str(), 
                mConfigFile.size() + 1);
  RegCloseKey(agent);

  sLogger << dlib::LINFO << "Service installed successfully.";
}

void MTConnectService::remove()
{
        SC_HANDLE manager;
        SC_HANDLE service;
        manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  
        if (manager == NULL) {
    sLogger << dlib::LERROR << "Could not open Service Control Manager";
                return;
  }
        service = ::OpenService(manager, mName.c_str(), SERVICE_ALL_ACCESS);
  CloseServiceHandle(manager);
        if (service == NULL) {
    sLogger << dlib::LERROR << "Could not open Service " << mName;
                return;
  }
  
        if(::DeleteService(service) == 0) {
    sLogger << dlib::LERROR << "Could delete service " << mName;
  } else {
    sLogger << dlib::LINFO << "Successfully removed service " << mName;
  }
  
        ::CloseServiceHandle(service);
}


//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
  // Register the handler function for the service
  gService->setName(lpszArgv[0]);

  char path[MAX_PATH];
  if( !GetModuleFileName(NULL, path, MAX_PATH ) )
  {
    sLogger << dlib::LERROR << "Cannot get path of executable (" << GetLastError() << ")";
    return;
  }

  std::string wd = path;
  size_t found = wd.rfind('\\');
  if (found != std::string::npos)
  {
    wd.erase(found);
    SetCurrentDirectory(wd.c_str());
  }

  gSvcStatusHandle = RegisterServiceCtrlHandler( 
    gService->name().c_str(), 
    SvcCtrlHandler);

  if( !gSvcStatusHandle )
  { 
    SvcReportEvent("RegisterServiceCtrlHandler"); 
    return; 
  } 

  // These SERVICE_STATUS members remain as set here

  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
  gSvcStatus.dwServiceSpecificExitCode = 0;    

  // Report initial status to the SCM

  ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );

  // Perform service-specific initialization and work.

  SvcInit( dwArgc, lpszArgv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit( DWORD dwArgc, LPTSTR *lpszArgv)
{
// Get the real arguments from the registry
  char key[1024];
  snprintf(key, 1023, "SOFTWARE\\MTConnect\\%s", gService->name().c_str());
  
  HKEY agent;
  LONG res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &agent);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open MTConnect Agent Key");
    ReportSvcStatus( SERVICE_STOPPED, 1, 0 );
    return;
  }
  
  const char *argp[2];
  BYTE configFile[2048];
  DWORD len = sizeof(configFile) - 1, type;
  res = RegQueryValueEx(agent, "ConfigurationFile", 0, &type, (BYTE*) configFile, &len);
  RegCloseKey(agent);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open ConfigurationFile");
    ReportSvcStatus( SERVICE_STOPPED, 1, 0 );
    return;
  }

  argp[0] = (char*) configFile;
  argp[1] = 0;
  gService->initialize(1, argp);
    
  // Report running status when initialization is complete.

  ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

  gService->start();

  ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus( DWORD dwCurrentState,
  DWORD dwWin32ExitCode,
  DWORD dwWaitHint)
{
  static DWORD dwCheckPoint = 1;

  // Fill in the SERVICE_STATUS structure.

  gSvcStatus.dwCurrentState = dwCurrentState;
  gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
  gSvcStatus.dwWaitHint = dwWaitHint;

  if (dwCurrentState == SERVICE_START_PENDING)
    gSvcStatus.dwControlsAccepted = 0;
  else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  if ( (dwCurrentState == SERVICE_RUNNING) ||
    (dwCurrentState == SERVICE_STOPPED) )
    gSvcStatus.dwCheckPoint = 0;
  else gSvcStatus.dwCheckPoint = dwCheckPoint++;

  // Report the status of the service to the SCM.
  SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler( DWORD dwCtrl )
{
 // Handle the requested control code. 

  switch(dwCtrl) 
  {  
    case SERVICE_CONTROL_STOP: 
      sLogger << dlib::LINFO << "Service stop requested";
      ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
      if (gService != NULL)
        gService->stop();
      ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

    return;

    case SERVICE_CONTROL_INTERROGATE: 
    break; 

    default: 
    break;
  } 

}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction) 
{ 
  HANDLE hEventSource;
  LPCTSTR lpszStrings[2];
  char Buffer[80];

  hEventSource = RegisterEventSource(NULL, gService->name().c_str());

  if( NULL != hEventSource )
  {
    sprintf_s(Buffer, 80, "%s failed with %d", szFunction, GetLastError());
    sLogger << dlib::LERROR << Buffer;

    lpszStrings[0] = gService->name().c_str();
    lpszStrings[1] = Buffer;

    ReportEvent(hEventSource,        // event log handle
      EVENTLOG_ERROR_TYPE, // event type
      0,                   // event category
      SVC_ERROR,           // event identifier
      NULL,                // no security identifier
      2,                   // size of lpszStrings array
      0,                   // no binary data
      lpszStrings,         // array of strings
      NULL);               // no binary data

    DeregisterEventSource(hEventSource);
  }
}

VOID SvcLogEvent(WORD aType, DWORD aId, LPSTR aText) 
{ 
  HANDLE hEventSource;
  LPCTSTR lpszStrings[3];

  hEventSource = RegisterEventSource(NULL, gService->name().c_str());

  if( NULL != hEventSource )
  {
    lpszStrings[0] = gService->name().c_str();
    lpszStrings[1] = "\n\n";
    lpszStrings[2] = aText;

    ReportEvent(hEventSource,        // event log handle
      aType, // event type
      0,                   // event category
      aId,           // event identifier
      NULL,                // no security identifier
      3,                   // size of lpszStrings array
      0,                   // no binary data
      lpszStrings,         // array of strings
      NULL);               // no binary data

    DeregisterEventSource(hEventSource);
  }
}


#else
#include "fcntl.h"
#include "sys/stat.h"
#include <iostream>
#include <signal.h>

static void signal_handler(int sig)
{
  switch(sig) {
  case SIGHUP:
    sLogger << dlib::LWARN << "hangup signal catched";
    break;
    
  case SIGTERM:
    sLogger << dlib::LWARN << "terminate signal catched";
    exit(0);
    break;
  }
}

static std::string sPidFile;
static void cleanup_pid() 
{
  unlink(sPidFile.c_str());
}

void MTConnectService::daemonize()
{
  int i,lfp;
  char str[10];
  if(getppid()==1) return; /* already a daemon */
  
  i=fork();
  if (i<0) exit(1); /* fork error */
  if (i>0) {
    std::cout << "Parent process now exiting, child process started" << std::endl;
    exit(0); /* parent exits */
  }
  
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */

  // Close stdin
  close(0);
  open("/dev/null", O_RDONLY);

  // Redirect stdout and stderr to another file.
  close(1);
  close(2);
  umask(027); /* set newly created file permissions */
  i = open("agent.output", O_WRONLY | O_CREAT, 0640);
  dup(i); /* handle standart I/O */

  // Set cleanup handler
  atexit(cleanup_pid);
  
  // Create the pid file.
  sPidFile = mPidFile;
  lfp = open(mPidFile.c_str(), O_RDWR|O_CREAT, 0640);
  if (lfp<0) exit(1); /* can not open */

  // Lock the pid file.
  if (lockf(lfp, F_TLOCK, 0)<0) exit(0); /* can not lock */
  
  /* first instance continues */
  sprintf(str,"%d\n", getpid());
  write(lfp, str, strlen(str)); /* record pid to lockfile */
  
  signal(SIGCHLD,SIG_IGN); /* ignore child */
  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  
  signal(SIGHUP,signal_handler); /* catch hangup signal */
  signal(SIGTERM,signal_handler); /* catch kill signal */
}

int MTConnectService::main(int argc, const char *argv[]) 
{
  PrintMTConnectAgentVersion();
  if(argc > 1) {
    if (strcasecmp( argv[1], "help") == 0 || strncmp(argv[1], "-h", 2) == 0)
    {
      printf("Usage: agent [help|daemonize|debug|run] [configuration_file]\n"
             "       help           Prints this message\n"
             "       daemonize      Run this process as a background daemon.\n"
             "                      daemonize with -h will display additional options\n"
             "       debug          Runs the agent on the command line with verbose logging\n"
             "       run            Runs the agent on the command line\n"
             "       config_file    The configuration file to load\n"
             "                      Default: agent.cfg in current directory\n\n"
             "When the agent is started without any arguments it will default to run\n");
     exit(0);
    } else if (strcasecmp( argv[1], "daemonize") == 0 ) {
      mIsService = true;
      mPidFile = "agent.pid";
      initialize(argc - 2, argv + 2);
      daemonize();
      sLogger << dlib::LINFO << "Starting daemon";
    } else if (strcasecmp( argv[1], "debug") == 0) {
      mIsDebug = true;
      initialize(argc - 2, argv + 2);
    } else {
      initialize(argc - 2, argv + 2);
    }
  } else {
    initialize(argc - 2, argv + 2);
  }
  
  start();
  return 0;
} 

void MTConnectService::install()
{
}

#endif
