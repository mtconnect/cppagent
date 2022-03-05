//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/asio.hpp>
#include "agent.hpp"
#include "configuration/agent_config.hpp"

#include "fcntl.h"
#include "sys/stat.h"

#include <cstring>

#ifdef WITH_RUBY
#include "ruby.h"
extern "C" void Init_ext(void);
extern "C" void Init_enc(void);
#endif

using namespace std;
using namespace mtconnect;

int main(int aArgc, const char *aArgv[])
{
  NAMED_SCOPE("MAIN");

#ifdef WITH_RUBY
  int argc = 3;
  char *a1 = (char*) "agent";
  char *a2 = (char*) "-e";
  char *a3 = (char*) "puts 'starting ruby'";

  char* pargv[] = {a1, a2, a3};

  RUBY_INIT_STACK;
  
  ruby_sysinit(&argc, (char***) &pargv);
  ruby_init();

  Init_ext();

  ruby_script("agent");
  ruby_options(argc, pargv);
#endif
  
  configuration::AgentConfiguration config;

  int ret = config.main(aArgc, aArgv);
  fclose(stdin);

  return ret;
}
