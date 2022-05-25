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
#include "version.h"

#include <cstdio>
#include <cstring>
#include <sstream>

std::string GetAgentVersion()
{
  std::stringstream verStream;
  verStream << "MTConnect Agent Version " << AGENT_VERSION_MAJOR << "." << AGENT_VERSION_MINOR
            << "." << AGENT_VERSION_PATCH << "." << AGENT_VERSION_BUILD;
  if (strlen(AGENT_VERSION_RC))
    verStream << " (" << AGENT_VERSION_RC << ")";
  return verStream.str();
}

void PrintMTConnectAgentVersion()
{
  printf("%s - built on " __TIMESTAMP__ "\n", GetAgentVersion().c_str());
}
