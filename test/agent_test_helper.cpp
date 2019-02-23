//
//  agent_test_helper.cpp
//  agent_test
//
//  Created by William Sobel on 11/13/18.
//

#include <stdio.h>
#include "agent_test_helper.hpp"

using namespace std;
using namespace std::chrono;

namespace mtconnect {
  namespace test {
    xmlDocPtr AgentTestHelper::responseHelper(
                                              CPPUNIT_NS::SourceLine sourceLine,
                                              key_value_map &aQueries)
    {
      IncomingThings incoming("", "", 0, 0);
      OutgoingThings outgoing;
      incoming.request_type = "GET";
      incoming.path = m_path;
      incoming.queries = aQueries;
      incoming.cookies = m_cookies;
      incoming.headers = m_incomingHeaders;
      
      outgoing.out = &m_out;
      
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
      
      CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);
      
      return xmlParseMemory(m_result.c_str(), m_result.length());
    }
    
    
    xmlDocPtr AgentTestHelper::putResponseHelper(
                                                 CPPUNIT_NS::SourceLine sourceLine,
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
      
      outgoing.out = &m_out;
      
      m_result = m_agent->httpRequest(incoming, outgoing);
      
      string message = (string) "No response to request" + m_path;
      
      CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);
      
      return xmlParseMemory(m_result.c_str(), m_result.length());
    }
  }
}
