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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <cstdio>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/sink/rest_sink/server.hpp"

using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;
namespace beast = boost::beast;
namespace http = beast::http;

void AgentTestHelper::makeRequest(const char *file, int line, boost::beast::http::verb verb,
                                  const std::string &body,
                                  const mtconnect::sink::rest_sink::QueryMap &aQueries,
                                  const char *path, const char *accepts)
{
  m_request = make_shared<Request>();

  m_dispatched = false;
  m_out.str("");
  m_request->m_verb = verb;
  m_request->m_query = aQueries;
  m_request->m_body = body;
  m_request->m_accepts = accepts;
  m_request->m_parameters.clear();

  if (path != nullptr)
    m_request->m_path = path;

  ASSERT_FALSE(m_request->m_path.empty());

  m_dispatched = m_restService->getServer()->dispatch(m_session, m_request);
}

void AgentTestHelper::responseHelper(const char *file, int line, const QueryMap &aQueries,
                                     xmlDocPtr *doc, const char *path, const char *accepts)
{
  makeRequest(file, line, http::verb::get, "", aQueries, path, accepts);
  *doc = xmlParseMemory(m_session->m_body.c_str(), int32_t(m_session->m_body.size()));
}

void AgentTestHelper::responseStreamHelper(const char *file, int line, const QueryMap &aQueries,
                                           const char *path, const char *accepts)
{
  makeRequest(file, line, http::verb::get, "", aQueries, path, accepts);
}

void AgentTestHelper::putResponseHelper(const char *file, int line, const string &body,
                                        const QueryMap &aQueries, xmlDocPtr *doc, const char *path,
                                        const char *accepts)
{
  makeRequest(file, line, http::verb::put, body, aQueries, path, accepts);
  if (ends_with(m_session->m_mimeType, "xml"))
    *doc = xmlParseMemory(m_session->m_body.c_str(), int32_t(m_session->m_body.size()));
}

void AgentTestHelper::deleteResponseHelper(const char *file, int line, const QueryMap &aQueries,
                                           xmlDocPtr *doc, const char *path, const char *accepts)
{
  makeRequest(file, line, http::verb::delete_, "", aQueries, path, accepts);
  if (ends_with(m_session->m_mimeType, "xml"))
    *doc = xmlParseMemory(m_session->m_body.c_str(), int32_t(m_session->m_body.size()));
}

void AgentTestHelper::chunkStreamHelper(const char *file, int line, xmlDocPtr *doc)
{
  *doc = xmlParseMemory(m_session->m_chunkBody.c_str(), int32_t(m_session->m_chunkBody.size()));
}

void AgentTestHelper::responseHelper(const char *file, int line, const QueryMap &aQueries,
                                     nlohmann::json &doc, const char *path, const char *accepts)
{
  makeRequest(file, line, http::verb::get, "", aQueries, path, accepts);
  doc = nlohmann::json::parse(m_session->m_body);
}
