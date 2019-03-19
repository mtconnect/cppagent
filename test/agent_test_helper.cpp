//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "Cuti.h"
#include <stdio.h>
#include "agent_test_helper.hpp"
#include "agent.hpp"

using namespace std;
using namespace std::chrono;
using namespace mtconnect;

xmlDocPtr AgentTestHelper::responseHelper(
                                          const char *file, int line,
                                          key_value_map &aQueries)
{
  IncomingThings incoming("", "", 0, 0);
  OutgoingThings outgoing;
  incoming.request_type = "GET";
  incoming.path = m_path;
  incoming.queries = aQueries;
  incoming.cookies = m_cookies;
  incoming.headers = m_incomingHeaders;
  
  outgoing.m_out = &m_out;
  
  m_result = m_agent->httpRequest(incoming, outgoing);
  
  if (m_result.empty())
  {
    m_result = m_out.str();
    auto pos = m_result.rfind("\n--");
    if (pos != string::npos)
    {
      pos = m_result.find('<', pos);
      if (pos != string::npos)
        m_result.erase(0, pos);
    }
  }
  
  string message = (string) "No response to request" + m_path + " with: ";
  
  key_value_map::iterator iter;
  
  for (iter = aQueries.begin(); iter != aQueries.end(); ++iter)
    message += iter->first + "=" + iter->second + ",";
  
#ifndef CUTI_NO_INTEGRATION
  stringstream msg;
  msg << message << " -- " << file << "(" << line << ")";
  CPPUNIT_ASSERT_MESSAGE(msg.str(), outgoing.http_return == 200);
#else
  CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, CPPUNIT_NS::SourceLine(file, line));
#endif
  
  return xmlParseMemory(m_result.c_str(), m_result.length());
}


xmlDocPtr AgentTestHelper::putResponseHelper(
                                             const char *file, int line,
                                             string body,
                                             key_value_map &aQueries)
{
  IncomingThings incoming("", "", 0, 0);
  OutgoingThings outgoing;
  incoming.request_type = "PUT";
  incoming.path = m_path;
  incoming.queries = aQueries;
  incoming.cookies = m_cookies;
  incoming.headers = m_incomingHeaders;
  incoming.body = body;
  incoming.foreign_ip = m_incomingIp;
  
  outgoing.m_out = &m_out;
  
  m_result = m_agent->httpRequest(incoming, outgoing);
  
  string message = (string) "No response to request" + m_path;
  
#ifndef CUTI_NO_INTEGRATION
  stringstream msg;
  msg << message << " -- " << file << "(" << line << ")";
  CPPUNIT_ASSERT_MESSAGE(msg.str(), outgoing.http_return == 200);
#else
  CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, CPPUNIT_NS::SourceLine(file, line));
#endif
  
  return xmlParseMemory(m_result.c_str(), m_result.length());
}
