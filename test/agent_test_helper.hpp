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

#pragma once

#include "test_utilities.hpp"
#include "rest_sink/session.hpp"
#include "rest_sink/server.hpp"
#include "rest_sink/response.hpp"
#include "rest_sink/routing.hpp"
#include "adapter/adapter.hpp"
#include "pipeline/pipeline.hpp"
#include "configuration/agent_config.hpp"
#include "agent.hpp"
#include "configuration/config_options.hpp"
#include "rest_sink/rest_service.hpp"
#include "loopback_source.hpp"

#include <chrono>
#include <iosfwd>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace mtconnect
{
  class Agent;
  
  namespace rest_sink
  {
    class TestSession : public Session
    {
    public:
      using Session::Session;
      ~TestSession() {}
      std::shared_ptr<TestSession> shared_ptr() {
        return std::dynamic_pointer_cast<TestSession>(shared_from_this());
      }
      
      void run() override
      {
        
      }
      void writeResponse(const Response &response, Complete complete = nullptr) override
      {
        m_code = response.m_status;
        m_body = response.m_body;
        m_mimeType = response.m_mimeType;
        if (complete)
          complete();
      }
      void beginStreaming(const std::string &mimeType, Complete complete) override
      {
        m_mimeType = mimeType;
        m_streaming = true;
        complete();
      }
      void writeChunk(const std::string &chunk, Complete complete) override
      {
        m_chunkBody = chunk;
        if (m_streaming)
          complete();
        else
          std::cout << "Streaming done" << std::endl;
      }
      void close() override
      {
        m_streaming = false;
      }
      void closeStream() override
      {
        m_streaming = false;
      }

      std::string m_body;
      std::string m_mimeType;
      boost::beast::http::status m_code;
      std::chrono::seconds m_expires;
      
      std::string m_chunkBody;
      std::string m_chunkMimeType;
      bool m_streaming{false};
    };

  }
}

namespace mhttp = mtconnect::rest_sink;
namespace adpt = mtconnect::adapter;
namespace observe = mtconnect::observation;

class AgentTestHelper
{
 public:
  AgentTestHelper()
  : m_incomingIp("127.0.0.1"),
    m_socket(m_ioContext)
  {
  }
  
  ~AgentTestHelper()
  {
    m_restService.reset();    
    m_adapter.reset();
    if (m_agent)
      m_agent->stop();
    m_agent.reset();
  }
  
  auto session() { return m_session; }
    
  // Helper method to test expected string, given optional query, & run tests
  void responseHelper(const char *file, int line,
                      const mtconnect::rest_sink::QueryMap &aQueries,
                      xmlDocPtr *doc, const char *path,
                      const char *accepts = "text/xml");
  void responseStreamHelper(const char *file, int line,
                            const mtconnect::rest_sink::QueryMap &aQueries,
                            const char *path,
                            const char *accepts = "text/xml");
  void responseHelper(const char *file, int line,
                      const mtconnect::rest_sink::QueryMap& aQueries, nlohmann::json &doc, const char *path,
                      const char *accepts = "application/json");
  void putResponseHelper(const char *file, int line, const std::string &body,
                         const mtconnect::rest_sink::QueryMap &aQueries,
                         xmlDocPtr *doc, const char *path,
                         const char *accepts = "text/xml");
  void deleteResponseHelper(const char *file, int line, 
                            const mtconnect::rest_sink::QueryMap &aQueries, xmlDocPtr *doc, const char *path,
                            const char *accepts = "text/xml");
  
  void chunkStreamHelper(const char *file, int line, xmlDocPtr *doc);

  void makeRequest(const char *file, int line, boost::beast::http::verb verb,
                   const std::string &body,
                   const mtconnect::rest_sink::QueryMap &aQueries,
                   const char *path, const char *accepts);
  
  auto getAgent() { return m_agent.get(); }
  std::shared_ptr<mtconnect::rest_sink::RestService> getRestService()
  {
    using namespace mtconnect;
    using namespace mtconnect::rest_sink;
    SinkPtr sink = m_agent->findSink("RestService");
    std::shared_ptr<RestService> rest = std::dynamic_pointer_cast<RestService>(sink);
    return rest;
  }
    
  auto createAgent(const std::string &file, int bufferSize = 8, int maxAssets = 4,
                   const std::string &version = "1.7", int checkpoint = 25,
                   bool put = false)
  {
    using namespace mtconnect;
    using namespace mtconnect::pipeline;

    auto cache = std::make_unique<mhttp::FileCache>();
    ConfigOptions options{{configuration::BufferSize, bufferSize},
      {configuration::MaxAssets, maxAssets},
      {configuration::CheckpointFrequency, checkpoint},
      {configuration::AllowPut, put},
      {configuration::SchemaVersion, version},
      {configuration::Pretty, true}
    };
    m_agent = std::make_unique<mtconnect::Agent>(m_ioContext,
                                                 PROJECT_ROOT_DIR + file,
                                                 options);
    m_context = std::make_shared<pipeline::PipelineContext>();
    m_context->m_contract = m_agent->makePipelineContract();
    
    boost::asio::io_context::strand strand(m_ioContext);
    m_loopback = std::make_shared<LoopbackSource>("TestSource", m_context, strand,                                                  options);
    m_agent->addSource(m_loopback);
    
    auto sinkContract = m_agent->makeSinkContract();
    m_restService = std::make_shared<rest_sink::RestService>(m_ioContext, move(sinkContract), options);
    m_restService->makeLoopbackSource(m_context);
    m_agent->addSink(m_restService);    
    m_agent->initialize(m_context);
    m_agent->initialDataItemObservations();

    m_server = m_restService->getServer();
    
    m_session = std::make_shared<mhttp::TestSession>([](mhttp::SessionPtr, mhttp::RequestPtr) { return true; }, m_server->getErrorFunction());
    return m_agent.get();
  }
  
  auto addAdapter(mtconnect::ConfigOptions options = {},
                  const std::string &host = "localhost", uint16_t port = 7878,
                  const std::string &device = "")
  {
    using namespace mtconnect;
    using namespace mtconnect::adapter;
    
    if (!IsOptionSet(options, configuration::Device))
    {
      options[configuration::Device] = *m_agent->defaultDevice()->getComponentName();
    }
    auto pipeline = std::make_unique<AdapterPipeline>(m_context);
    m_adapter = std::make_shared<adpt::Adapter>(m_ioContext, host, port, options, pipeline);
    m_agent->addSource(m_adapter);

    return m_adapter;
  }
  
  uint64_t addToBuffer(mtconnect::DataItemPtr di, const mtconnect::entity::Properties &shdr,
                       const mtconnect::Timestamp &time)
  {
    using namespace mtconnect;
    using namespace mtconnect::observation;
    using namespace mtconnect::entity;
    ErrorList errors;
    auto obs = Observation::make(di, shdr, time, errors);
    if (errors.size() == 0 && obs)
    {
      return m_loopback->receive(obs);
    }
    return 0;
  }
  
  void printResponse()
  {
    std::cout << "Status " << m_session->m_code << " " << std::endl
              << m_session->m_body << std::endl << "------------------------"
              << std::endl;
  }

  void printResponseStream()
  {
    std::cout << "Status " << m_response.m_status << " " << std::endl
              << m_out.str() << std::endl << "------------------------"
              << std::endl;
  }

  mhttp::Server *m_server{nullptr};
  std::shared_ptr<mtconnect::pipeline::PipelineContext> m_context;
  std::shared_ptr<adpt::Adapter> m_adapter;
  std::shared_ptr<mtconnect::rest_sink::RestService> m_restService;
  std::shared_ptr<mtconnect::LoopbackSource> m_loopback;
  
  bool m_dispatched { false };
  std::string m_incomingIp;
  
  std::unique_ptr<mtconnect::Agent> m_agent;
  std::stringstream m_out;
  mtconnect::rest_sink::RequestPtr m_request;
  boost::asio::io_context m_ioContext;
  boost::asio::ip::tcp::socket m_socket;
  mtconnect::rest_sink::Response m_response;
  std::shared_ptr<mtconnect::rest_sink::TestSession> m_session;
};

struct XmlDocFreer {
  XmlDocFreer(xmlDocPtr doc) : m_doc(doc) {}
  ~XmlDocFreer() {
    if (m_doc)
      xmlFreeDoc(m_doc);
  }
  xmlDocPtr m_doc;
};


#define PARSE_XML_RESPONSE(path)                                                           \
  xmlDocPtr doc = nullptr;                                                                   \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, &doc, path); \
  ASSERT_TRUE(doc); \
  XmlDocFreer cleanup(doc)

#define PARSE_TEXT_RESPONSE(path)                                                           \
  xmlDocPtr doc = nullptr;                                                                   \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, &doc, path); \
  XmlDocFreer cleanup(doc)



#define PARSE_XML_RESPONSE_QUERY(path, queries)                               \
  xmlDocPtr doc = nullptr;                                              \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, queries, &doc, path); \
  ASSERT_TRUE(doc); \
  XmlDocFreer cleanup(doc)


#define PARSE_XML_STREAM_QUERY(path, queries)                               \
  m_agentTestHelper->responseStreamHelper(__FILE__, __LINE__, queries, path); \

#define PARSE_XML_CHUNK()                               \
  xmlDocPtr doc = nullptr;                                              \
  m_agentTestHelper->chunkStreamHelper(__FILE__, __LINE__, &doc); \
  ASSERT_TRUE(doc); \
  XmlDocFreer cleanup(doc)


#define PARSE_XML_RESPONSE_PUT(path, body, queries)                                    \
  xmlDocPtr doc = nullptr;                                                       \
  m_agentTestHelper->putResponseHelper(__FILE__, __LINE__, body, queries, &doc, path); \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_DELETE(path)                                    \
  xmlDocPtr doc = nullptr;                                                       \
  m_agentTestHelper->deleteResponseHelper(__FILE__, __LINE__, {}, &doc, path); \
  ASSERT_TRUE(doc); \
  XmlDocFreer cleanup(doc)

#define PARSE_JSON_RESPONSE(path) \
  nlohmann::json doc; \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, doc, path)

