/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "agent.hpp"
#include "fcntl.h"
#include "sys/stat.h"
#include "options.hpp"
#include "string.h"

using namespace std;
using namespace dlib;

void terminateServerThread(Agent *server)
{
  std::cout << "Press enter at anytime to terminate web server" << std::endl;
  std::cin.get();
 
  // Shut down server by unblocking Agent::start()
  server->clear();
}

void addToBufferThread(Agent *server)
{
  while (true)
  {
    std::string device, dataItem, value;

    std::cout << std::endl << "Device: ";
    std::cin >> device;

    std::cout << std::endl << "Data item: ";
    std::cin >> dataItem;
    
    std::cout << "Value: ";
    std::cin >> value;

    DataItem *di = server->getDataItemByName(device, dataItem);
    if (di != NULL)
    {
      unsigned int seqNum = server->addToBuffer(di, value);
      std::cout << "Sequence Number: " << seqNum << std::endl;
    }
    else
    {
      std::cout << "Could not find data item: " << dataItem << endl;
    }
  }
}

#ifndef WIN32
void signal_handler(int sig)
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
#endif

void daemonize()
{
#ifndef WIN32
  int i,lfp;
  char str[10];
  if(getppid()==1) return; /* already a daemon */
  
  i=fork();
  if (i<0) exit(1); /* fork error */
  if (i>0)
  {
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
  
  // chdir(RUNNING_DIR); /* change running directory */

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
#endif
}


int main(int aArgc, char *aArgv[])
{
  int listenPort = 5000;
  const char *config_file = "probe.xml";
  list<string> adapters;
  bool interactive = false;
  bool daemonize_proc = false;
  const char *adapters_file = 0;
  gLogFile = "agent.log";
  
  //void initializeGlobals();
  
  OptionsList option_list;
  option_list.append(new Option("p", listenPort, "HTTP Server Port\nDefault: 5000", "port"));
  option_list.append(new Option("f", config_file, "Configuration file\nDefault: probe.xml", "file"));
  option_list.append(new Option("l", gLogFile, "Log file\nDefault: agent.log", "file"));
  option_list.append(new Option("i", interactive, "Interactive shell", false));
  option_list.append(new Option("d", daemonize_proc, "Daemonize\nDefault: false", false));
  option_list.append(new Option("c", adapters_file, "Adapter configuration file", "file"));
  option_list.append(new Option("a", adapters, "Location of adapter\n'device:address:port'", "adapters"));
  option_list.parse(aArgc, (const char**) aArgv);
  
  if (daemonize_proc)
  {
    daemonize();
  }
  
  try
  {
    Agent * agent = new Agent(config_file);
    if (adapters_file)
    {
      ifstream ad_file(adapters_file);
      if (ad_file)
      {
        while (ad_file.good())
        {
          char line[256];
          ad_file.getline(line, 255);
          line[255] = '\0';
          if (strchr(line, ':')) 
            adapters.push_back(line);
        }
      }
      else
      {
        cerr << "Cannot open file for adapters: " << adapters_file << endl;
        option_list.usage();
      }
    }

    if (adapters.empty())
    {
      cerr << "Either -a or -c must be specified" << endl;
      option_list.usage();
    }
    
    for (list<string>::iterator iter = adapters.begin(); iter != adapters.end(); iter++)
    {
      // Should have the format device:address:port
      string &adapter = *iter;
      string device, address, adapter_port;
      int pos1 = adapter.find_first_of(':');
      if (pos1 == string::npos) {
        cerr << "Bad format for adapter specification, must be: device:address:port" << endl;
        option_list.usage();
      }
      
      device = adapter.substr(0, pos1);
      // Make sure the device exists
      if (agent->getDeviceByName(device) == 0)
      {
        cerr << "Can't locate device " << device << " in XML configuration file." << endl;
        option_list.usage();
      }
      
      int pos2 = adapter.find_first_of(':', pos1 + 1);
      if (pos2 == string::npos) {
        cerr << "Bad format for adapter specification, must be: device:address:port" << endl;
        option_list.usage();
      }
      address = adapter.substr(pos1 + 1, pos2 - pos1 - 1);

      if (adapter.length() <= pos2) {
        cerr << "Bad format for adapter specification, must be: device:address:port" << endl;
        option_list.usage();
      }
      adapter_port = adapter.substr(pos2 + 1);
      
      cout << "Device " << device << endl;
      cout << "Address " << address << endl;
      cout << "Port " << adapter_port << endl;

      agent->addAdapter(device, address, atoi(adapter_port.c_str()));
    }
        
    // ***** DEBUGGING TOOLS *****
    
    // Create a thread that will listen for the user to end this program
    //thread_function t(terminateServerThread, &agent);
    cout << "Listening on port " << listenPort << endl;
    
    thread_function *interactiveThread = 0;
    
    if (interactive)
    {
      // Use the addToBuffer API to allow user input for data
      interactiveThread = new thread_function(addToBufferThread, agent);
    }
    
    agent->set_listening_port(listenPort);
    agent->start();

    if (interactive)
      delete interactiveThread;
    
    delete agent;
  }
  catch (std::exception & e)
  {
    logEvent("Cppagent::Main", e.what());
    std::cerr << "Agent failed to load: " << e.what() << std::endl;
    return -1;
  }
  
  return 0;
}

