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

#pragma once

#include <chrono>
#include <iosfwd>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "agent.hpp"
#include "configuration/agent_config.hpp"
#include "configuration/config_options.hpp"
#include "pipeline/pipeline.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "sink/rest_sink/response.hpp"
#include "sink/rest_sink/rest_service.hpp"
#include "sink/rest_sink/routing.hpp"
#include "sink/rest_sink/server.hpp"
#include "sink/rest_sink/session.hpp"
#include "source/adapter/shdr/shdr_adapter.hpp"
#include "source/loopback_source.hpp"
#include "test_utilities.hpp"

namespace mtconnect {
  class Agent;

  namespace sink {
    namespace rest_sink {
      class TestSession : public Session
      {
      public:
        using Session::Session;
        ~TestSession() {}
        std::shared_ptr<TestSession> shared_ptr()
        {
          return std::dynamic_pointer_cast<TestSession>(shared_from_this());
        }

        void run() override {}
        void writeResponse(ResponsePtr &&response, Complete complete = nullptr) override
        {
          m_code = response->m_status;
          if (response->m_file)
            m_body = response->m_file->m_buffer;
          else
            m_body = response->m_body;
          m_mimeType = response->m_mimeType;
          if (complete)
            complete();
        }
        void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) override
        {
          if (m_streaming)
          {
            writeChunk(response->m_body, complete);
          }
          else
          {
            writeResponse(move(response), complete);
          }
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
        void close() override { m_streaming = false; }
        void closeStream() override { m_streaming = false; }

        std::string m_body;
        std::string m_mimeType;
        boost::beast::http::status m_code;
        std::chrono::seconds m_expires;

        std::string m_chunkBody;
        std::string m_chunkMimeType;
        bool m_streaming {false};
      };

    }  // namespace rest_sink
  }    // namespace sink
}  // namespace mtconnect

namespace mhttp = mtconnect::sink::rest_sink;
namespace adpt = mtconnect::source::adapter;
namespace observe = mtconnect::observation;

class AgentTestHelper
{
public:
  AgentTestHelper() : m_incomingIp("127.0.0.1"), m_strand(m_ioContext), m_socket(m_ioContext) {}

  ~AgentTestHelper()
  {
    m_mqttService.reset();
    m_restService.reset();
    m_adapter.reset();
    if (m_agent)
      m_agent->stop();
    m_agent.reset();
  }

  auto session() { return m_session; }

  // Helper method to test expected string, given optional query, & run tests
  void responseHelper(const char *file, int line,
                      const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                      const char *path, const char *accepts = "text/xml");
  void responseStreamHelper(const char *file, int line,
                            const mtconnect::sink::rest_sink::QueryMap &aQueries, const char *path,
                            const char *accepts = "text/xml");
  void responseHelper(const char *file, int line,
                      const mtconnect::sink::rest_sink::QueryMap &aQueries, nlohmann::json &doc,
                      const char *path, const char *accepts = "application/json");
  void putResponseHelper(const char *file, int line, const std::string &body,
                         const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                         const char *path, const char *accepts = "text/xml");
  void deleteResponseHelper(const char *file, int line,
                            const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                            const char *path, const char *accepts = "text/xml");

  void chunkStreamHelper(const char *file, int line, xmlDocPtr *doc);

  void makeRequest(const char *file, int line, boost::beast::http::verb verb,
                   const std::string &body, const mtconnect::sink::rest_sink::QueryMap &aQueries,
                   const char *path, const char *accepts);

  auto getAgent() { return m_agent.get(); }
  std::shared_ptr<mhttp::RestService> getRestService()
  {
    using namespace mtconnect;
    using namespace mtconnect::sink::rest_sink;
    sink::SinkPtr sink = m_agent->findSink("RestService");
    std::shared_ptr<mhttp::RestService> rest = std::dynamic_pointer_cast<mhttp::RestService>(sink);
    return rest;
  }

  std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> getMqttService()
  {
    using namespace mtconnect;
    sink::SinkPtr sink = m_agent->findSink("MqttService");
    std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> mqtt =
        std::dynamic_pointer_cast<mtconnect::sink::mqtt_sink::MqttService>(sink);
    return mqtt;
  }

  auto createAgent(const std::string &file, int bufferSize = 8, int maxAssets = 4,
                   const std::string &version = "1.7", int checkpoint = 25, bool put = false,
                   bool observe = true, const mtconnect::ConfigOptions ops = {})
  {
    using namespace mtconnect;
    using namespace mtconnect::pipeline;
    using namespace mtconnect::source;
    using ptree = boost::property_tree::ptree;

    sink::rest_sink::RestService::registerFactory(m_sinkFactory);
    sink::mqtt_sink::MqttService::registerFactory(m_sinkFactory);
    source::adapter::shdr::ShdrAdapter::registerFactory(m_sourceFactory);

    ConfigOptions options = ops;
    options.emplace(configuration::BufferSize, bufferSize);
    options.emplace(configuration::MaxAssets, maxAssets);
    options.emplace(configuration::CheckpointFrequency, checkpoint);
    options.emplace(configuration::AllowPut, put);
    options.emplace(configuration::SchemaVersion, version);
    options.emplace(configuration::Pretty, true);
    options.emplace(configuration::Port, 0);
    options.emplace(configuration::JsonVersion, 1);

    m_agent = std::make_unique<mtconnect::Agent>(m_ioContext, PROJECT_ROOT_DIR + file, options);
    m_context = std::make_shared<pipeline::PipelineContext>();
    m_context->m_contract = m_agent->makePipelineContract();

    m_loopback = std::make_shared<LoopbackSource>("TestSource", m_strand, m_context, options);
    m_agent->addSource(m_loopback);

    auto sinkContract = m_agent->makeSinkContract();
    sinkContract->m_pipelineContext = m_context;

    auto sink = m_sinkFactory.make("RestService", "RestService", m_ioContext, move(sinkContract),
                                   options, ptree {});
    m_restService = std::dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    m_agent->addSink(m_restService);

    if (HasOption(options, "MqttSink"))
    {
      auto mqttContract = m_agent->makeSinkContract();
      mqttContract->m_pipelineContext = m_context;
      auto mqttsink = m_sinkFactory.make("MqttService", "MqttService", m_ioContext,
                                         move(mqttContract), options, ptree {});
      m_mqttService = std::dynamic_pointer_cast<sink::mqtt_sink::MqttService>(mqttsink);
      m_agent->addSink(m_mqttService);
    }

    m_agent->initialize(m_context);

    if (observe)
    {
      m_agent->initialDataItemObservations();
      auto ad = m_agent->getAgentDevice();
      if (ad)
      {
        auto d = ad->getDeviceDataItem("agent_avail");
        if (d)
          addToBuffer(d, {{"VALUE", std::string("AVAILABLE")}}, std::chrono::system_clock::now());
      }
    }

    m_server = m_restService->getServer();

    m_session = std::make_shared<mhttp::TestSession>(
        [](mhttp::SessionPtr, mhttp::RequestPtr) { return true; }, m_server->getErrorFunction());
    return m_agent.get();
  }

  auto addAdapter(mtconnect::ConfigOptions options = {}, const std::string &host = "localhost",
                  uint16_t port = 7878, const std::string &device = "")
  {
    using namespace mtconnect;
    using namespace mtconnect::source::adapter;

    if (!IsOptionSet(options, configuration::Device))
    {
      options[configuration::Device] = *m_agent->defaultDevice()->getComponentName();
    }
    boost::property_tree::ptree tree;
    tree.put(configuration::Host, host);
    tree.put(configuration::Port, port);
    m_adapter = std::make_shared<shdr::ShdrAdapter>(m_ioContext, m_context, options, tree);
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
              << m_session->m_body << std::endl
              << "------------------------" << std::endl;
  }

  void printResponseStream()
  {
    std::cout << "Status " << m_response.m_status << " " << std::endl
              << m_out.str() << std::endl
              << "------------------------" << std::endl;
  }

  mhttp::Server *m_server {nullptr};
  std::shared_ptr<mtconnect::pipeline::PipelineContext> m_context;
  std::shared_ptr<adpt::shdr::ShdrAdapter> m_adapter;
  std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> m_mqttService;
  std::shared_ptr<mtconnect::sink::rest_sink::RestService> m_restService;
  std::shared_ptr<mtconnect::source::LoopbackSource> m_loopback;

  bool m_dispatched {false};
  std::string m_incomingIp;

  std::unique_ptr<mtconnect::Agent> m_agent;
  std::stringstream m_out;
  mtconnect::sink::rest_sink::RequestPtr m_request;
  boost::asio::io_context m_ioContext;
  boost::asio::io_context::strand m_strand;
  boost::asio::ip::tcp::socket m_socket;
  mtconnect::sink::rest_sink::Response m_response;
  std::shared_ptr<mtconnect::sink::rest_sink::TestSession> m_session;

  mtconnect::sink::SinkFactory m_sinkFactory;
  mtconnect::source::SourceFactory m_sourceFactory;
};

struct XmlDocFreer
{
  XmlDocFreer(xmlDocPtr doc) : m_doc(doc) {}
  ~XmlDocFreer()
  {
    if (m_doc)
      xmlFreeDoc(m_doc);
  }
  xmlDocPtr m_doc;
};

#define PARSE_XML_RESPONSE(path)                                         \
  xmlDocPtr doc = nullptr;                                               \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, &doc, path); \
  ASSERT_TRUE(doc);                                                      \
  XmlDocFreer cleanup(doc)

#define PARSE_TEXT_RESPONSE(path)                                        \
  xmlDocPtr doc = nullptr;                                               \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, &doc, path); \
  XmlDocFreer cleanup(doc)

#define PARSE_XML_RESPONSE_QUERY(path, queries)                               \
  xmlDocPtr doc = nullptr;                                                    \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, queries, &doc, path); \
  ASSERT_TRUE(doc);                                                           \
  XmlDocFreer cleanup(doc)

#define PARSE_XML_STREAM_QUERY(path, queries) \
  m_agentTestHelper->responseStreamHelper(__FILE__, __LINE__, queries, path);

#define PARSE_XML_CHUNK()                                         \
  xmlDocPtr doc = nullptr;                                        \
  m_agentTestHelper->chunkStreamHelper(__FILE__, __LINE__, &doc); \
  ASSERT_TRUE(doc);                                               \
  XmlDocFreer cleanup(doc)

#define PARSE_XML_RESPONSE_PUT(path, body, queries)                                    \
  xmlDocPtr doc = nullptr;                                                             \
  m_agentTestHelper->putResponseHelper(__FILE__, __LINE__, body, queries, &doc, path); \
  ASSERT_TRUE(doc)

#define PARSE_XML_RESPONSE_DELETE(path)                                        \
  xmlDocPtr doc = nullptr;                                                     \
  m_agentTestHelper->deleteResponseHelper(__FILE__, __LINE__, {}, &doc, path); \
  ASSERT_TRUE(doc);                                                            \
  XmlDocFreer cleanup(doc)

#define PARSE_JSON_RESPONSE(path) \
  nlohmann::json doc;             \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, {}, doc, path)
