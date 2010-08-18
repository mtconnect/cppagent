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
#include "string.h"
#include "config.hpp"
#include "dlib/config_reader.h"

using namespace std;
using namespace dlib;

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

void daemonize(int aArgc, char *aArgv[])
{
#ifndef WIN32
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


int main(int aArgc, const char *aArgv[])
{
  Sleep(10000);
  AgentConfiguration config;
  return config.main(aArgc, aArgv);
}

