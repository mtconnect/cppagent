#include "service.hpp"
#include "string.h"

#ifdef WIN32
#define stricmp _stricmp
#else
#define strncpy_s strncpy
#endif

MTConnectService::MTConnectService() :
mIsService(false)
{
}

void MTConnectService::initialize(int aArgc, const char *aArgv[])
{
  if (mIsService)
    gLogger = new ServiceLogger();
  else
    gLogger = new Logger();
}

#ifdef WIN32

#include <windows.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

#pragma comment(lib, "advapi32.lib")

#define SVC_ERROR                        ((DWORD)0xC0000001L)
#define SVC_WARNING                        ((DWORD)0x90000001L)
#define SVC_INFO                        ((DWORD)0x50000001L)

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

VOID WINAPI SvcCtrlHandler( DWORD ); 
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
VOID SvcReportEvent( LPTSTR );

MTConnectService *gService = NULL;

int MTConnectService::main(int argc, const char *argv[]) 
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
             "       debug          Runs the agent on the command line with verbose logging\n"
             "       run            Runs the agent on the command line\n"
             "       config_file    The configuration file to load\n"
             "                      Default: agent.cfg in current directory\n\n"
             "When the agent is started without any arguments it is assumed it will be running\n"
             "as a service and will begin the service initialization sequence\n");
      
    } else if (stricmp( argv[1], "install") == 0 ) {
      initialize(argc - 2, argv + 2);
      install();
      return 0;
    } else if (stricmp( argv[1], "debug") == 0 || stricmp( argv[1], "run") == 0) {
      initialize(argc - 2, argv + 2);
      start();
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

  return 0;
} 

void MTConnectService::install()
{
  SC_HANDLE manager;
  SC_HANDLE service;
  char path[MAX_PATH];

  if( !GetModuleFileName(NULL, path, MAX_PATH ) )
  {
    printf("Cannot install service (%d)\n", GetLastError());
    return;
  }

// Get a handle to the SCM database. 

  manager = OpenSCManager( 
    NULL,                    // local computer
    NULL,                    // ServicesActive database 
    SC_MANAGER_ALL_ACCESS);  // full access rights 

  if (NULL == manager) 
  {
    printf("OpenSCManager failed (%d)\n", GetLastError());
    return;
  }

  service = OpenService(manager, mName.c_str(), SC_MANAGER_ALL_ACCESS);
  if (service != NULL) {
    if (! ChangeServiceConfig( 
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
      printf("ChangeServiceConfig failed (%d)\n", GetLastError());
    } else {
     printf("Service updated successfully.\n"); 
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
      printf("CreateService failed (%d)\n", GetLastError()); 
      CloseServiceHandle(manager);
      return;
    }
    else {
      printf("Service installed successfully\n"); 
    }
  }

  CloseServiceHandle(service); 
  CloseServiceHandle(manager);

  HKEY software;
  LONG res = RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE", &software);
  if (res != ERROR_SUCCESS)
  {
    printf("Could not open software key: %d\n", res);
    return;
  }

  HKEY mtc;
  res = RegOpenKey(software, "MTConnect", &mtc);
  if (res != ERROR_SUCCESS)
  {
    printf("Could not open MTConnect, creating: %d\n", res);
    res = RegCreateKey(software, "MTConnect", &mtc);
    RegCloseKey(software);
    if (res != ERROR_SUCCESS)
    {
      printf("Could not create MTConnect: %d\n", res);
      return;
    }
  }
  RegCloseKey(software);

  // Create Service Key
  HKEY agent;
  res = RegOpenKey(mtc, mName.c_str(), &agent);
  if (res != ERROR_SUCCESS)
  {
    printf("Could not open %s, creating: %d\n", mName, res);
    res = RegCreateKey(mtc, mName.c_str(), &agent);
    if (res != ERROR_SUCCESS)
    {
      RegCloseKey(mtc);
      printf("Could not create %s: %d\n", mName.c_str(), res);
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
    printf("Cannot install service (%d)\n", GetLastError());
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
  HKEY software;
  LONG res = RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE", &software);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open SOFTWARE");
    return;
  }

  HKEY mtc;
  res = RegOpenKey(software, "MTConnect", &mtc);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open MTConnect");
    RegCloseKey(software);
    return;
  }

  // Create Service Key
  HKEY adapter;
  res = RegOpenKey(mtc, gService->name().c_str(), &adapter);
  RegCloseKey(software);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open Adapter");
    RegCloseKey(mtc);
    return;
  }

  const char *argp[2];
  BYTE configFile[2048];
  DWORD len, type;
  res = RegQueryValueEx(adapter, "ConfigurationFile", 0, &type, (BYTE*) configFile, &len);
  RegCloseKey(mtc);
  if (res != ERROR_SUCCESS)
  {
    SvcReportEvent("RegOpenKey: Could not open ConfigurationFile");
    return;
  }
  RegCloseKey(adapter);

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


void ServiceLogger::error(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  va_list args;
  va_start (args, aFormat);
  SvcLogEvent(EVENTLOG_ERROR_TYPE, SVC_ERROR, (LPSTR) format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}

void ServiceLogger::warning(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  va_list args;
  va_start (args, aFormat);
  SvcLogEvent(EVENTLOG_WARNING_TYPE, SVC_WARNING, (LPSTR) format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}

void ServiceLogger::info(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  va_list args;
  va_start (args, aFormat);
  SvcLogEvent(EVENTLOG_INFORMATION_TYPE, SVC_INFO, (LPSTR) format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}


#else
#include "fcntl.h"
#include "sys/stat.h"
#include <iostream>

static void signal_handler(int sig)
{
  switch(sig) {
  case SIGHUP:
    std::cout << "hangup signal catched" << std::endl;
    break;
    
  case SIGTERM:
    std::cout << "terminate signal catched" << std::endl;
    exit(0);
    break;
  }
}

static void cleanup_pid() 
{
  unlink("agent.pid");
}

static void daemonize()
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
  i = open("agent.log", O_WRONLY | O_CREAT, 0640);
  dup(i); /* handle standart I/O */

  // Set cleanup handler
  atexit(cleanup_pid);
  
  // Create the pid file.
  lfp = open("agent.pid", O_RDWR|O_CREAT, 0640);
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
      
    } else if (strcasecmp( argv[1], "daemonize") == 0 ) {
      daemonize();
    }
  }
  
  initialize(argc - 2, argv + 2);
  start();
  return 0;
} 

void MTConnectService::install()
{
}

#endif