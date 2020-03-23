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

#include "test_globals.hpp"

#include <dlib/server.h>

#include <chrono>
#include <iosfwd>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace mtconnect
{
  class Agent;
}

class AgentTestHelper
{
 public:
  AgentTestHelper() : m_agent(nullptr), m_incomingIp("127.0.0.1")
  {
  }

  mtconnect::Agent *m_agent;
  std::ostringstream m_out;

  std::string m_incomingIp;

  std::string m_path;
  dlib::key_value_map m_queries;
  std::string m_result;
  dlib::key_value_map m_cookies;
  dlib::key_value_map_ci m_incomingHeaders;

  // Helper method to test expected string, given optional query, & run tests
  void responseHelper(const char *file, int line, dlib::key_value_map &aQueries, xmlDocPtr *doc);
  void responseHelper(const char *file, int line, dlib::key_value_map &aQueries, nlohmann::json &doc);
  void putResponseHelper(const char *file, int line, const std::string &body,
                         dlib::key_value_map &aQueries, xmlDocPtr *doc);
  void deleteResponseHelper(const char *file, int line, 
                            dlib::key_value_map &aQueries, xmlDocPtr *doc);

  void makeRequest(const char *file, int line, const char *request, const std::string &body, dlib::key_value_map &aQueries);
};

#define PARSE_XML_RESPONSE                                                                   \
  xmlDocPtr doc = nullptr;                                                                   \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, m_agentTestHelper->m_queries, &doc); \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_QUERY_KV(key, value)                                              \
  m_agentTestHelper->m_queries[key] = value;                                                 \
  xmlDocPtr doc = nullptr;                                                                   \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, m_agentTestHelper->m_queries, &doc); \
  m_agentTestHelper->m_queries.clear();                                                      \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_QUERY(queries)                               \
  xmlDocPtr doc = nullptr;                                              \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, queries, &doc); \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_PUT(body, queries)                                    \
  xmlDocPtr doc = nullptr;                                                       \
  m_agentTestHelper->putResponseHelper(__FILE__, __LINE__, body, queries, &doc); \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_DELETE                                    \
  xmlDocPtr doc = nullptr;                                                       \
  m_agentTestHelper->deleteResponseHelper(__FILE__, __LINE__, m_agentTestHelper->m_queries, &doc); \
  ASSERT_TRUE(doc)

#define PARSE_JSON_RESPONSE \
  nlohmann::json doc; \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, m_agentTestHelper->m_queries, doc)

