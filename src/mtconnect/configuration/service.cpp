//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

/* -- this file needs to be first for the servics to build correctly */
#ifdef _WINDOWS
#include "windows.h"
#endif
/* keep this file first */

#include <boost/program_options.hpp>

#include <cstring>
#include <fstream>
#include <thread>

#include "logging.hpp"
#include "service.hpp"
#include "version.h"

#ifdef _WINDOWS
#define stricmp _stricmp
#define snprintf _snprintf
#define strncasecmp _strnicmp
#else
#define strncpy_s strncpy
#include <unistd.h>
#endif

namespace mtconnect {
  namespace configuration {

    void MTConnectService::usage(int ec)
    {
      printf(
#ifndef _WINDOWS
          "Usage: agent [help|daemonize|debug|run] [config-file]"
#else
          "Usage: agent [help|install|remove|debug|run] [config-file]"
#endif
          R"(       help           Prints this message and exits
       version        Prints the agent version and exits
)"
#ifndef _WINDOWS
          R"(       daemonize      Run this process as a background daemon.
                      daemonize with -h will display additional options
)"
#else
          R"(       install        Install the agent as a service.
       remove            Removes the agent service.

)"
#endif
          R"(       debug          Runs the agent on the command line with verbose logging
       run            Runs the agent on the command line
       config-file    The configuration file to load
                      Default: agent.cfg in current directory
)");
      exit(ec);
    }

    boost::program_options::variables_map MTConnectService::parseOptions(
        int argc, const char *argv[], boost::optional<std::string> &command,
        boost::optional<std::string> &config)
    {
      namespace po = boost::program_options;
      using namespace std;

      po::options_description desc("Options");
      desc.add_options()("help,h", "Show this help message")("version",
                                                             "Show the current agent version");

      po::options_description hidden("Hidden");
      hidden.add_options()("command", po::value(&command), "The command")(
          "config-file", po::value(&config), "The configuration file");

      po::positional_options_description pos;
      pos.add("command", 1);
      pos.add("config-file", 1);

      po::options_description all("All Options");
      all.add(desc).add(hidden);

      po::options_description visible("Visible Options");
      visible.add(desc);

      po::variables_map options;
      po::store(po::command_line_parser(argc, argv).options(all).positional(pos).run(), options);
      po::notify(options);

      if (options.count("help") > 0 || (command && *command == "help"))
      {
        usage();
      }
      else if (options.count("version") > 0 || (command && *command == "version"))
      {
        exit(0);
      }

      return options;
    }

  }  // namespace configuration
}  // namespace mtconnect

#ifdef _WINDOWS

// Don't include WinSock.h when processing <windows.h>
#define _WINSOCKAPI_
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <strsafe.h>
#include <sys/stat.h>
#include <windows.h>

#if _MSC_VER >= 1900
#define gets gets_s
#endif

#pragma comment(lib, "advapi32.lib")

#define SVC_ERROR ((DWORD)0xC0000001L)
#define SVC_WARNING ((DWORD)0x90000001L)
#define SVC_INFO ((DWORD)0x50000001L)

namespace mtconnect {
  namespace configuration {
    SERVICE_STATUS g_svcStatus;
    SERVICE_STATUS_HANDLE g_svcStatusHandle;
    HANDLE g_hSvcStopEvent = nullptr;

    VOID WINAPI SvcCtrlHandler(DWORD);
    VOID WINAPI SvcMain(DWORD, LPTSTR *);

    VOID ReportSvcStatus(DWORD, DWORD, DWORD);
    VOID SvcInit(DWORD, LPTSTR *);
    VOID SvcReportEvent(LPSTR);

    static MTConnectService *g_service = nullptr;

    static void agent_termination_handler() {}

    void commandLine()
    {
      puts("> ");
      char line[1024] = {0};
      while (gets(line))
      {
        if (!strncasecmp(line, "QUIT", 4u))
        {
          g_service->stop();
          return;
        }
      }
    }

    int MTConnectService::main(int argc, const char *argv[])
    {
      std::set_terminate(agent_termination_handler);
      PrintMTConnectAgentVersion();

      using namespace std;

      try
      {
        // If command-line parameter is "install", install the service. If debug or run
        // is specified than just run it as a command line process.
        // Otherwise, the service is probably being started by the SCM.
        using namespace std;

        boost::optional<string> command;
        boost::optional<string> config;

        auto options = parseOptions(argc, argv, command, config);

        if (command)
        {
          if (*command == "install")
          {
            initialize(options);
            install();
            return 0;
          }
          else if (*command == "remove")
          {
            initialize(options);
            remove();
            return 0;
          }
          else if (*command == "debug" || *command == "run")
          {
            if (*command == "debug")
              m_isDebug = true;

            initialize(options);
            start();
            std::thread cmd(commandLine);
            return 0;
          }
          else
          {
            usage(1);
          }
        }

        g_service = this;
        m_isService = true;
        SERVICE_TABLE_ENTRY DispatchTable[] = {{"", (LPSERVICE_MAIN_FUNCTION)SvcMain},
                                               {nullptr, nullptr}};

        if (StartServiceCtrlDispatcher(DispatchTable) == 0)
        {
          usage(1);
        }
        else
        {
          SvcReportEvent("StartServiceCtrlDispatcher");
        }
      }
      catch (std::exception &e)
      {
        LOG(fatal) << "Agent top level exception: " << e.what();
        std::cerr << "Agent top level exception: " << e.what() << std::endl;
      }
      catch (std::string &s)
      {
        LOG(fatal) << "Agent top level exception: " << s;
        std::cerr << "Agent top level exception: " << s << std::endl;
      }

      return 0;
    }

    bool MTConnectService::isElevated()
    {
      // Only applicable to Windows Vista and later
      OSVERSIONINFO osver = {sizeof(osver)};
      if (GetVersionExA(&osver) && osver.dwMajorVersion < 6ul)
        return true;

      HANDLE token = nullptr;
      if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
      {
        std::cerr << "OpenProcessToken failed (" << GetLastError() << ")" << std::endl;
        LOG(error) << "OpenProcessToken (" << GetLastError() << ")";
        return false;
      }

      DWORD size = 0ul;
      TOKEN_ELEVATION tokenInformation;
      if (!GetTokenInformation(token, TokenElevation, &tokenInformation, sizeof(TOKEN_ELEVATION),
                               &size))
      {
        return false;
      }

      CloseHandle(token);
      token = nullptr;
      return tokenInformation.TokenIsElevated > 0;
    }

    void MTConnectService::install()
    {
      char path[MAX_PATH] = {0};
      if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
      {
        LOG(error) << "Cannot install service (" << GetLastError() << ")";
        std::cerr << "Cannot install service GetModuleFileName failed (" << GetLastError() << ")"
                  << std::endl;
        return;
      }

      if (!isElevated())
      {
        LOG(error) << "Process must have elevated permissions to run";
        std::cerr << "Process must have elevated permissions to run" << std::endl;
        return;
      }

      // Get a handle to the SCM database.
      auto manager = OpenSCManagerA(nullptr,                 // local computer
                                    nullptr,                 // ServicesActive database
                                    SC_MANAGER_ALL_ACCESS);  // full access rights

      if (!manager)
      {
        LOG(error) << "OpenSCManager failed (" << GetLastError() << ")";
        std::cerr << "OpenSCManager failed (" << GetLastError() << ")" << std::endl;
        return;
      }

      auto service = OpenServiceA(manager, m_name.c_str(), SC_MANAGER_ALL_ACCESS);
      if (service)
      {
        if (!ChangeServiceConfigA(service,            // handle of service
                                  SERVICE_NO_CHANGE,  // service type: no change
                                  SERVICE_NO_CHANGE,  // service start type
                                  SERVICE_NO_CHANGE,  // error control: no change
                                  path,               // binary path (updated if nescessary)
                                  nullptr,            // load order group: no change
                                  nullptr,            // tag ID: no change
                                  nullptr,            // dependencies: no change
                                  nullptr,            // account name: no change
                                  nullptr,            // password: no change
                                  nullptr))           // display name: no change
        {
          LOG(error) << "ChangeServiceConfig failed (" << GetLastError() << ")";
          std::cerr << std::endl
                    << "ChangeServiceConfig failed (" << GetLastError() << ")" << std::endl;
          CloseServiceHandle(service);
          CloseServiceHandle(manager);
          return;
        }
      }
      else
      {
        // Create the service
        service = CreateService(manager,                      // SCM database
                                m_name.c_str(),               // name of service
                                m_name.c_str(),               // service name to display
                                SERVICE_ALL_ACCESS,           // desired access
                                SERVICE_WIN32_OWN_PROCESS,    // service type
                                SERVICE_AUTO_START,           // start type
                                SERVICE_ERROR_NORMAL,         // error control type
                                path,                         // path to service's binary
                                nullptr,                      // no load ordering group
                                nullptr,                      // no tag identifier
                                "Tcpip\0Eventlog\0Netman\0",  // dependencies
                                nullptr,                      // LocalSystem account
                                nullptr);                     // no password

        if (!service)
        {
          LOG(error) << "CreateService failed (" << GetLastError() << ")";
          std::cerr << std::endl << "CreateService failed (" << GetLastError() << ")" << std::endl;
          CloseServiceHandle(manager);
          return;
        }
      }

      // Build a description string for the service to make it easy to identify an instance
      // by it build version and the configuration file path (if not empty)
      std::string description = GetAgentVersion();
      if (!m_configFile.empty())
      {
        description.append(" - ");
        description.append(m_configFile.string());
      }
      SERVICE_DESCRIPTIONA serviceDescription = {0};
      serviceDescription.lpDescription = const_cast<char *>(description.c_str());
      ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &serviceDescription);

      CloseServiceHandle(service);
      service = nullptr;
      CloseServiceHandle(manager);
      manager = nullptr;

      HKEY software = nullptr;
      auto res = RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE", &software);
      if (res != ERROR_SUCCESS)
      {
        LOG(error) << "Could not open software key (" << res << ")";
        std::cerr << std::endl << "Could not open software key (" << res << ")" << std::endl;
        return;
      }

      HKEY mtc = nullptr;
      res = RegOpenKeyA(software, "MTConnect", &mtc);
      if (res != ERROR_SUCCESS)
      {
        res = RegCreateKeyA(software, "MTConnect", &mtc);
        RegCloseKey(software);
        if (res != ERROR_SUCCESS)
        {
          LOG(error) << "Could not create MTConnect (" << res << ")";
          std::cerr << std::endl << "Could not create MTConnect key (" << res << ")" << std::endl;
          return;
        }
      }
      RegCloseKey(software);

      // Create Service Key
      HKEY agent = nullptr;
      res = RegOpenKeyA(mtc, m_name.c_str(), &agent);
      if (res != ERROR_SUCCESS)
      {
        res = RegCreateKeyA(mtc, m_name.c_str(), &agent);
        if (res != ERROR_SUCCESS)
        {
          RegCloseKey(mtc);
          LOG(error) << "Could not create " << m_name << " (" << res << ")";
          std::cerr << std::endl
                    << "Could not create " << m_name << " (" << res << ")" << std::endl;
          return;
        }
      }
      RegCloseKey(mtc);

      RegSetValueExA(agent, "ConfigurationFile", 0ul, REG_SZ, (const BYTE *)m_configFile.c_str(),
                     m_configFile.string().size() + 1);
      RegCloseKey(agent);

      LOG(info) << "Service installed successfully.";
      std::cerr << std::endl
                << std::endl
                << "Service installed successfully." << std::endl
                << std::endl;
    }

    void MTConnectService::remove()
    {
      if (!isElevated())
      {
        LOG(error) << "Process must have elevated permissions to run";
        std::cerr << "Process must have elevated permissions to run" << std::endl;
        return;
      }

      auto manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
      if (!manager)
      {
        LOG(error) << "Could not open Service Control Manager";
        return;
      }

      auto service = ::OpenService(manager, m_name.c_str(), SERVICE_ALL_ACCESS);
      CloseServiceHandle(manager);
      manager = nullptr;
      if (!service)
      {
        LOG(error) << "Could not open Service " << m_name;
        return;
      }

      // Check if service is running, if it is, stop the service.
      SERVICE_STATUS status;
      if (QueryServiceStatus(service, &status) && status.dwCurrentState != SERVICE_STOPPED)
      {
        // Stop the service
        if (!ControlService(service, SERVICE_CONTROL_STOP, &status))
          LOG(error) << "Could not stop service " << m_name;
        else
          LOG(info) << "Successfully stopped service " << m_name;
      }

      if (!::DeleteService(service))
      {
        LOG(error) << "Could remove service " << m_name;
        std::cerr << std::endl
                  << std::endl
                  << "Could remove service " << m_name << std::endl
                  << std::endl;
      }
      else
      {
        LOG(info) << "Successfully removed service " << m_name;
        std::cerr << std::endl
                  << std::endl
                  << "Service removed successfully." << std::endl
                  << std::endl;
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
    VOID WINAPI SvcMain(DWORD dwArgc, LPSTR *lpszArgv)
    {
      // Register the handler function for the service
      g_service->setName(lpszArgv[0]);

      char path[MAX_PATH] = {0};
      if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
      {
        LOG(error) << "Cannot get path of executable (" << GetLastError() << ")";
        return;
      }

      std::string wd = path;
      auto found = wd.rfind('\\');
      if (found != std::string::npos)
      {
        wd.erase(found);
        SetCurrentDirectoryA(wd.c_str());
      }

      g_svcStatusHandle = RegisterServiceCtrlHandlerA(g_service->name().c_str(), SvcCtrlHandler);

      if (!g_svcStatusHandle)
      {
        SvcReportEvent("RegisterServiceCtrlHandler");
        return;
      }

      // These SERVICE_STATUS members remain as set here
      g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
      g_svcStatus.dwServiceSpecificExitCode = 0ul;

      // Report initial status to the SCM
      ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000ul);

      // Perform service-specific initialization and work.
      SvcInit(dwArgc, lpszArgv);
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
    VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
    {
      // Get the real arguments from the registry
      char key[1024] = {0};
      snprintf(key, 1023u, "SOFTWARE\\MTConnect\\%s", g_service->name().c_str());

      HKEY agent = nullptr;
      auto res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0ul, KEY_READ, &agent);
      if (res != ERROR_SUCCESS)
      {
        SvcReportEvent("RegOpenKey: Could not open MTConnect Agent Key");
        ReportSvcStatus(SERVICE_STOPPED, 1ul, 0ul);
        return;
      }

      BYTE configFile[2048] = {};
      DWORD len = sizeof(configFile) - 1ul, type(0ul);
      res = RegQueryValueExA(agent, "ConfigurationFile", 0ul, &type, (BYTE *)configFile, &len);
      RegCloseKey(agent);
      agent = nullptr;
      if (res != ERROR_SUCCESS)
      {
        SvcReportEvent("RegOpenKey: Could not open ConfigurationFile");
        ReportSvcStatus(SERVICE_STOPPED, 1ul, 0ul);
        return;
      }

      boost::optional<std::string> command;
      boost::optional<std::string> config;

      const char *argp[3] = {"run", nullptr, nullptr};
      argp[1] = (char *)configFile;
      auto options = g_service->parseOptions(1, argp, command, config);
      g_service->initialize(options);

      // Report running status when initialization is complete.
      ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0ul);
      g_service->start();
      ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0ul);
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
    VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
    {
      static DWORD dwCheckPoint = 1ul;

      // Fill in the SERVICE_STATUS structure.
      g_svcStatus.dwCurrentState = dwCurrentState;
      g_svcStatus.dwWin32ExitCode = dwWin32ExitCode;
      g_svcStatus.dwWaitHint = dwWaitHint;

      if (dwCurrentState == SERVICE_START_PENDING)
        g_svcStatus.dwControlsAccepted = 0ul;
      else
        g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

      if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        g_svcStatus.dwCheckPoint = 0ul;
      else
        g_svcStatus.dwCheckPoint = dwCheckPoint++;

      // Report the status of the service to the SCM.
      SetServiceStatus(g_svcStatusHandle, &g_svcStatus);
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
    VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
    {
      // Handle the requested control code.
      switch (dwCtrl)
      {
        case SERVICE_CONTROL_STOP:
          LOG(info) << "Service stop requested";
          ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0ul);
          if (g_service)
            g_service->stop();
          LOG(info) << "Service stop completed";

          ReportSvcStatus(g_svcStatus.dwCurrentState, NO_ERROR, 0ul);
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
    VOID SvcReportEvent(LPSTR szFunction)
    {
      auto hEventSource = RegisterEventSourceA(nullptr, g_service->name().c_str());

      if (hEventSource)
      {
        LPCSTR lpszStrings[2] = {nullptr, nullptr};
        char Buffer[80] = {0};
        sprintf_s(Buffer, 80u, "%s failed with %d", szFunction, GetLastError());
        LOG(error) << Buffer;

        lpszStrings[0] = g_service->name().c_str();
        lpszStrings[1] = Buffer;

        ReportEventA(hEventSource,         // event log handle
                     EVENTLOG_ERROR_TYPE,  // event type
                     0,                    // event category
                     SVC_ERROR,            // event identifier
                     nullptr,              // no security identifier
                     2,                    // size of lpszStrings array
                     0ul,                  // no binary data
                     lpszStrings,          // array of strings
                     nullptr);             // no binary data

        DeregisterEventSource(hEventSource);
      }
    }

    VOID SvcLogEvent(WORD eventType, DWORD eventId, LPSTR logText)
    {
      auto hEventSource = RegisterEventSourceA(nullptr, g_service->name().c_str());
      if (hEventSource)
      {
        LPCSTR lpszStrings[3] = {g_service->name().c_str(), "\n\n", logText};

        ReportEventA(hEventSource,  // event log handle
                     eventType,     // event type
                     0,             // event category
                     eventId,       // event identifier
                     nullptr,       // no security identifier
                     3,             // size of lpszStrings array
                     0,             // no binary data
                     lpszStrings,   // array of strings
                     nullptr);      // no binary data

        DeregisterEventSource(hEventSource);
      }
    }
  }  // namespace configuration
}  // namespace mtconnect

#else

#include <csignal>
#include <iostream>

#include "fcntl.h"
#include "sys/stat.h"

namespace mtconnect {
  namespace configuration {
    static void signal_handler(int sig)
    {
      switch (sig)
      {
        case SIGHUP:
          LOG(warning) << "hangup signal catched";
          break;

        case SIGTERM:
          LOG(warning) << "terminate signal catched";
          exit(0);
          break;
      }
    }

    static std::string s_pidFile;
    static void cleanup_pid() { unlink(s_pidFile.c_str()); }

    void MTConnectService::daemonize()
    {
      if (getppid() == 1)
        return;  // already a daemon

      auto i = fork();
      if (i < 0)
        exit(1);  // fork error
      if (i > 0)
      {
        std::cout << "Parent process now exiting, child process started" << std::endl;
        exit(0);  // parent exits
      }

      // child (daemon) continues
      setsid();  // obtain a new process group

      // Close stdin
      close(0);
      open("/dev/null", O_RDONLY);

      // Redirect stdout and stderr to another file.
      close(1);
      close(2);
      umask(027);  // set newly created file permissions
      i = open("agent.output", O_WRONLY | O_CREAT, 0640);
      dup(i);  // handle standart I/O

      // Set cleanup handler
      atexit(cleanup_pid);

      // Create the pid file.
      s_pidFile = m_pidFile;
      auto lfp = open(m_pidFile.c_str(), O_RDWR | O_CREAT, 0640);
      if (lfp < 0)
        exit(1);  // can not open

      // Lock the pid file.
      if (lockf(lfp, F_TLOCK, 0) < 0)
        exit(0);  // can not lock

      // first instance continues
      char str[10] = {0};
      sprintf(str, "%d\n", getpid());
      write(lfp, str, strlen(str));  // record pid to lockfile

      signal(SIGCHLD, SIG_IGN);  // ignore child
      signal(SIGTSTP, SIG_IGN);  // ignore tty signals
      signal(SIGTTOU, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);

      signal(SIGHUP, signal_handler);   // catch hangup signal
      signal(SIGTERM, signal_handler);  // catch kill signal
    }

    int MTConnectService::main(int argc, const char *argv[])
    {
      PrintMTConnectAgentVersion();

      using namespace std;

      boost::optional<string> command;
      boost::optional<string> config;

      auto options = parseOptions(argc, argv, command, config);

      if (command)
      {
        if (*command == "daemonize")
        {
          m_isService = true;
          m_pidFile = "agent.pid";
          initialize(options);
          daemonize();
          LOG(info) << "Starting daemon";
        }
        else if (*command == "debug")
        {
          m_isDebug = true;
          initialize(options);
        }
        else if (*command == "run")
        {
          initialize(options);
        }
        else
        {
          usage(1);
        }
      }
      else
      {
        usage(1);
      }

      start();
      return 0;
    }

    void MTConnectService::install() {}
  }  // namespace configuration
}  // namespace mtconnect
#endif
