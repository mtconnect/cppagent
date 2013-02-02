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

#include "agent.hpp"
#include "fcntl.h"
#include "sys/stat.h"
#include "string.h"
#include "config.hpp"
#include "dlib/config_reader.h"

using namespace std;
using namespace dlib;

#include <dlib/logger.h>
#include <dlib/threads.h>

static logger sLogger("main");

#ifdef _WINDOWS
#define strncasecmp strnicmp
#endif

int main(int aArgc, const char *aArgv[])
{
  AgentConfiguration config;

  int ret = config.main(aArgc, aArgv);
  fclose(stdin);

  return ret;
}

