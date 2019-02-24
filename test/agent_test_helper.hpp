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

#pragma once

#include <map>
#include <string>
#include <iosfwd>
#include <chrono>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"

namespace mtconnect {
  namespace test {
    class AgentTestHelper {
    public:
      AgentTestHelper()
      : m_agent(nullptr), m_incomingIp("127.0.0.1")
      {
      }
      
      Agent *m_agent;
      std::ostringstream m_out;
      
      std::string m_incomingIp;
      
      std::string m_path;
      dlib::key_value_map m_queries;
      std::string m_result;
      dlib::key_value_map m_cookies;
      dlib::key_value_map_ci m_incomingHeaders;
      
      // Helper method to test expected string, given optional query, & run tests
      xmlDocPtr responseHelper(CPPUNIT_NS::SourceLine sourceLine, key_value_map &aQueries);
      xmlDocPtr putResponseHelper(CPPUNIT_NS::SourceLine sourceLine, std::string body,
                                  key_value_map &aQueries);
      
    };
    
#define PARSE_XML_RESPONSE \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), m_agentTestHelper.m_queries); \
CPPUNIT_ASSERT(doc)
    
#define PARSE_XML_RESPONSE_QUERY_KV(key, value) \
m_agentTestHelper.m_queries[key] = value; \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), m_agentTestHelper.m_queries); \
m_agentTestHelper.m_queries.clear(); \
CPPUNIT_ASSERT(doc)
    
#define PARSE_XML_RESPONSE_QUERY(queries) \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), queries); \
CPPUNIT_ASSERT(doc)
    
#define PARSE_XML_RESPONSE_PUT(body, queries) \
xmlDocPtr doc = m_agentTestHelper.putResponseHelper(CPPUNIT_SOURCELINE(), body, queries); \
CPPUNIT_ASSERT(doc)
  }
}

