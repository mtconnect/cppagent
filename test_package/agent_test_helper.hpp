//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include <queue>
#include <string>

#include <nlohmann/json.hpp>

#include "mtconnect/agent.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/sink/mqtt_entity_sink/mqtt_entity_sink.hpp"
#include "mtconnect/sink/mqtt_sink/mqtt_service.hpp"
#include "mtconnect/sink/rest_sink/response.hpp"
#include "mtconnect/sink/rest_sink/rest_service.hpp"
#include "mtconnect/sink/rest_sink/routing.hpp"
#include "mtconnect/sink/rest_sink/server.hpp"
#include "mtconnect/sink/rest_sink/session.hpp"
#include "mtconnect/sink/rest_sink/websocket_request_manager.hpp"
#include "mtconnect/sink/rest_sink/websocket_session.hpp"
#include "mtconnect/source/adapter/shdr/shdr_adapter.hpp"
#include "mtconnect/source/loopback_source.hpp"
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
            writeResponse(std::move(response), complete);
          }
        }
        void beginStreaming(const std::string &mimeType, Complete complete,
                            std::optional<std::string> requestId = std::nullopt) override
        {
          m_mimeType = mimeType;
          m_streaming = true;
          complete();
        }
        void writeChunk(const std::string &chunk, Complete complete,
                        std::optional<std::string> requestId = std::nullopt) override
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

      class TestWebsocketSession : public WebsocketSession<TestWebsocketSession>
      {
      public:
        using super = WebsocketSession<TestWebsocketSession>;

        TestWebsocketSession(boost::asio::executor &&exec, RequestPtr &&request, Dispatch dispatch,
                             ErrorFunction func)
          : WebsocketSession(std::move(request), dispatch, func), m_executor(std::move(exec))
        {
          m_isOpen = true;
        }
        ~TestWebsocketSession() {}
        std::shared_ptr<TestWebsocketSession> shared_ptr()
        {
          return std::dynamic_pointer_cast<TestWebsocketSession>(shared_from_this());
        }

        void run() override {}

        void read(const std::string &json)
        {
          if (!m_requestManager.dispatch(shared_ptr(), json))
          {
            LOG(error) << "Dispatch failed for: " << json;
          }
        }

        void closeStream() override {}

        bool isStreamOpen() { return m_isOpen; }

        void sent(beast::error_code ec, std::size_t len, const std::string &id)
        {
          NAMED_SCOPE("WebsocketSession::sent");
          super::sent(ec, len, id);
          m_responsesSent[id]++;
        }

        void asyncSend(WebsocketRequestManager::WebsocketRequest *request)
        {
          auto buffer = beast::buffers_to_string(request->m_streamBuffer->data());

          m_responses[request->m_requestId].emplace(buffer);

          beast::error_code ec;
          boost::asio::post(m_executor, boost::bind(&TestWebsocketSession::sent, shared_ptr(), ec,
                                                    0, request->m_requestId));
        }

        auto &getExecutor() { return m_executor; }

        bool dispatch(const std::string &buffer, std::string &id)
        {
          return m_requestManager.dispatch(shared_ptr(), buffer, &id);
        }

        bool hasResponse(const std::string &id) const
        {
          const auto q = m_responses.find(id);
          return q != m_responses.end() && !q->second.empty();
        }

        std::optional<std::string> getNextResponse(const std::string &id)
        {
          auto q = m_responses.find(id);
          if (q != m_responses.end() && !q->second.empty())
          {
            auto response = q->second.front();
            q->second.pop();
            m_lastResponses[id] = response;
            return response;
          }
          else
          {
            return std::nullopt;
          }
        }

        std::map<std::string, std::queue<std::string>> m_responses;
        std::map<std::string, uint32_t> m_responsesSent;
        std::map<std::string, std::string> m_lastResponses;

      protected:
        boost::asio::executor m_executor;
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
  using Hook = std::function<void(AgentTestHelper &)>;

  AgentTestHelper() : m_incomingIp("127.0.0.1"), m_strand(m_ioContext), m_socket(m_ioContext) {}

  ~AgentTestHelper()
  {
    m_mqttService.reset();
    m_mqttEntitySink.reset();
    m_restService.reset();
    m_adapter.reset();
    if (m_agent)
      m_agent->stop();
    m_agent.reset();
    m_ioContext.stop();
  }

  auto session() { return m_session; }
  auto websocketSession() { return m_websocketSession; }

  void setAgentCreateHook(Hook &hook) { m_agentCreateHook = hook; }

  /// @brief Helper to get a response from the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param aQueries The query parameters
  /// @param doc The returned document
  /// @param path The request path
  /// @param accepts The accepted mime type
  void responseHelper(const char *file, int line,
                      const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                      const char *path, const char *accepts = "text/xml");

  /// @brief Helper to get a streaming response from the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param aQueries The query parameters
  /// @param path The request path
  /// @param accepts The accepted mime type
  void responseStreamHelper(const char *file, int line,
                            const mtconnect::sink::rest_sink::QueryMap &aQueries, const char *path,
                            const char *accepts = "text/xml");

  /// @brief Helper to get a json response from the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param aQueries The query parameters
  /// @param doc The returned document
  /// @param path The request path
  /// @param accepts The accepted mime type
  void responseHelper(const char *file, int line,
                      const mtconnect::sink::rest_sink::QueryMap &aQueries, nlohmann::json &doc,
                      const char *path, const char *accepts = "application/json");

  /// @brief Helper to make a PUT request to the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param body The body of the request
  /// @param aQueries The query parameters
  /// @param doc The returned document
  /// @param path The request path
  /// @param accepts The accepted mime type
  void putResponseHelper(const char *file, int line, const std::string &body,
                         const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                         const char *path, const char *accepts = "text/xml");

  /// @brief Helper to make a POST request to the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param aQueries The query parameters
  /// @param doc The returned document
  /// @param path The request path
  /// @param accepts The accepted mime type
  void deleteResponseHelper(const char *file, int line,
                            const mtconnect::sink::rest_sink::QueryMap &aQueries, xmlDocPtr *doc,
                            const char *path, const char *accepts = "text/xml");

  /// @brief Helper to get a chunked response from the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param doc The returned document
  void chunkStreamHelper(const char *file, int line, xmlDocPtr *doc);

  /// @brief Make a request to the agent
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param verb The HTTP verb
  /// @param body The body for PUT/POST requests
  /// @param aQueries The query parameters
  /// @param path The request path
  /// @param accepts The accepted mime type
  void makeRequest(const char *file, int line, boost::beast::http::verb verb,
                   const std::string &body, const mtconnect::sink::rest_sink::QueryMap &aQueries,
                   const char *path, const char *accepts);

  /// @brief Make a request using a json command to parse and dispatch
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param json the request
  /// @param doc the returned document
  /// @param id the request id
  void makeWebSocketRequest(const char *file, int line, const std::string &json, xmlDocPtr *doc,
                            std::string &id);

  /// @brief Make a request using a json command to parse and dispatch
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param json the request
  /// @param doc the returned document
  /// @param id the request id
  void makeWebSocketRequest(const char *file, int line, const std::string &json,
                            nlohmann::json &doc, std::string &id);

  /// @brief Make a request and don't wait for a response
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param json the request
  /// @param id the request id
  void makeAsyncWebSocketRequest(const char *file, int line, const std::string &json,
                                 std::string &id);

  /// @brief Parse an async respone
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param doc the returned document
  /// @param id the request id
  void parseResponse(const char *file, int line, nlohmann::json &doc, const std::string &id);

  /// @brief Parse an async respone
  /// @param file The source file the request is made from
  /// @param line The line number
  /// @param doc the returned document
  /// @param id the request id
  void parseResponse(const char *file, int line, xmlDocPtr *doc, const std::string &id);

  auto getAgent() { return m_agent.get(); }
  std::shared_ptr<mhttp::RestService> getRestService()
  {
    using namespace mtconnect;
    using namespace mtconnect::sink::rest_sink;
    sink::SinkPtr sink = m_agent->findSink("RestService");
    std::shared_ptr<mhttp::RestService> rest = std::dynamic_pointer_cast<mhttp::RestService>(sink);
    return rest;
  }

  std::shared_ptr<mtconnect::sink::mqtt_entity_sink::MqttEntitySink> getMqttEntitySink()
  {
    using namespace mtconnect::sink::mqtt_entity_sink;
    auto sink = m_agent->findSink("MqttEntitySink");
    return std::dynamic_pointer_cast<MqttEntitySink>(sink);
  }

  std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> getMqttService()
  {
    using namespace mtconnect;
    sink::SinkPtr mqttSink = m_agent->findSink("MqttService");
    std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> mqtt2 =
        std::dynamic_pointer_cast<mtconnect::sink::mqtt_sink::MqttService>(mqttSink);
    return mqtt2;
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
    sink::mqtt_entity_sink::MqttEntitySink::registerFactory(m_sinkFactory);
    source::adapter::shdr::ShdrAdapter::registerFactory(m_sourceFactory);

    ConfigOptions options = ops;
    options.emplace(configuration::BufferSize, bufferSize);
    options.emplace(configuration::MaxAssets, maxAssets);
    options.emplace(configuration::CheckpointFrequency, checkpoint);
    options.emplace(configuration::AllowPut, put);
    options.emplace(configuration::SchemaVersion, version);
    options.emplace(configuration::Pretty, true);
    options.emplace(configuration::Port, 0);
    options.emplace(configuration::ServerIp, std::string("127.0.0.1"));
    options.emplace(configuration::JsonVersion, 1);

    m_agent = std::make_unique<mtconnect::Agent>(m_ioContext, TEST_RESOURCE_DIR + file, options);
    if (m_agentCreateHook)
      m_agentCreateHook(*this);

    m_context = std::make_shared<pipeline::PipelineContext>();
    m_context->m_contract = m_agent->makePipelineContract();

    m_loopback = std::make_shared<LoopbackSource>("TestSource", m_strand, m_context, options);
    m_agent->addSource(m_loopback);

    auto sinkContract = m_agent->makeSinkContract();
    sinkContract->m_pipelineContext = m_context;

    auto sink = m_sinkFactory.make("RestService", "RestService", m_ioContext,
                                   std::move(sinkContract), options, ptree {});
    m_restService = std::dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    m_agent->addSink(m_restService);

    if (HasOption(options, "MqttSink"))
    {
      auto mqttContract = m_agent->makeSinkContract();
      mqttContract->m_pipelineContext = m_context;
      auto mqtt2sink = m_sinkFactory.make("MqttService", "MqttService", m_ioContext,
                                          std::move(mqttContract), options, ptree {});
      m_mqttService = std::dynamic_pointer_cast<sink::mqtt_sink::MqttService>(mqtt2sink);
      m_agent->addSink(m_mqttService);
    }

    if (HasOption(options, "MqttEntitySink"))
    {
      auto mqttEntityContract = m_agent->makeSinkContract();
      mqttEntityContract->m_pipelineContext = m_context;
      auto mqttEntitySink = m_sinkFactory.make("MqttEntitySink", "MqttEntitySink", m_ioContext,
                                               std::move(mqttEntityContract), options, ptree {});
      m_mqttEntitySink =
          std::dynamic_pointer_cast<sink::mqtt_entity_sink::MqttEntitySink>(mqttEntitySink);
      m_agent->addSink(m_mqttEntitySink);
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

    mhttp::RequestPtr request = std::make_shared<mhttp::Request>();
    request->m_verb = boost::beast::http::verb::get;

    auto ex {m_agent->getContext().get().get_executor()};
    m_websocketSession = std::make_shared<mhttp::TestWebsocketSession>(
        std::move(ex), std::move(request),
        [this](mhttp::SessionPtr s, mhttp::RequestPtr r) { return m_server->dispatch(s, r); },
        m_server->getErrorFunction());

    m_server->simulateRun();

    return m_agent.get();
  }

  auto addAdapter(mtconnect::ConfigOptions options = {}, const std::string &host = "localhost",
                  uint16_t port = 7878, const std::string &device = "")
  {
    using namespace mtconnect;
    using namespace mtconnect::source::adapter;

    if (!IsOptionSet(options, configuration::Device))
    {
      options[configuration::Device] = *m_agent->getDefaultDevice()->getComponentName();
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

  template <typename Rep, typename Period>
  bool waitFor(const std::chrono::duration<Rep, Period> &time, std::function<bool()> pred)
  {
    std::decay_t<decltype(time)> run = time / 2;
    if (run > std::chrono::milliseconds(500))
      run = std::chrono::milliseconds(500);

    boost::asio::steady_timer timer(m_ioContext);
    timer.expires_after(time);
    bool timeout = false;
    timer.async_wait([&timeout](boost::system::error_code ec) {
      if (!ec)
      {
        timeout = true;
      }
    });

    while (!timeout && !pred())
    {
      m_ioContext.run_for(run);
    }
    timer.cancel();

    return pred();
  }

  template <typename Rep, typename Period>
  bool waitForResponseSent(const std::chrono::duration<Rep, Period> &time, const std::string &id)
  {
    uint32_t initial = m_websocketSession->m_responsesSent[id];
    return waitFor(time, [this, initial, id]() -> bool {
      return m_websocketSession->m_responsesSent[id] > initial;
    });
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

  void printLastWSResponse(const std::string &id)
  {
    auto it = m_websocketSession->m_lastResponses.find(id);
    if (it != m_websocketSession->m_lastResponses.end())
    {
      std::cout << "WebSocket Response for " << id << ": " << it->second << std::endl
                << "------------------------" << std::endl;
    }
  }

  auto getResponseCount(const std::string &id)
  {
    return m_websocketSession->m_responses[id].size();
  }

  mhttp::Server *m_server {nullptr};
  std::shared_ptr<mtconnect::pipeline::PipelineContext> m_context;
  std::shared_ptr<adpt::shdr::ShdrAdapter> m_adapter;
  std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> m_mqttService;
  std::shared_ptr<mtconnect::sink::mqtt_entity_sink::MqttEntitySink> m_mqttEntitySink;
  std::shared_ptr<mtconnect::sink::rest_sink::RestService> m_restService;
  std::shared_ptr<mtconnect::source::LoopbackSource> m_loopback;

  bool m_dispatched {false};
  std::string m_incomingIp;

  std::unique_ptr<mtconnect::Agent> m_agent;
  std::stringstream m_out;
  mtconnect::sink::rest_sink::RequestPtr m_request;
  mtconnect::configuration::AsyncContext m_ioContext;
  boost::asio::io_context::strand m_strand;
  boost::asio::ip::tcp::socket m_socket;
  mtconnect::sink::rest_sink::Response m_response;
  std::shared_ptr<mtconnect::sink::rest_sink::TestSession> m_session;
  std::shared_ptr<mtconnect::sink::rest_sink::TestWebsocketSession> m_websocketSession;

  mtconnect::sink::SinkFactory m_sinkFactory;
  mtconnect::source::SourceFactory m_sourceFactory;

  Hook m_agentCreateHook;
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

#define PARSE_JSON_RESPONSE_QUERY(path, query) \
  nlohmann::json doc;                          \
  m_agentTestHelper->responseHelper(__FILE__, __LINE__, query, doc, path)

#define PARSE_XML_WS_RESPONSE(req)                                            \
  xmlDocPtr doc = nullptr;                                                    \
  std::string id;                                                             \
  m_agentTestHelper->makeWebSocketRequest(__FILE__, __LINE__, req, &doc, id); \
  ASSERT_TRUE(doc);                                                           \
  XmlDocFreer cleanup(doc)

#define PARSE_JSON_WS_RESPONSE(req) \
  nlohmann::json jdoc;              \
  std::string id;                   \
  m_agentTestHelper->makeWebSocketRequest(__FILE__, __LINE__, req, jdoc, id)

#define BEGIN_ASYNC_WS_REQUEST(req) \
  std::string id;                   \
  m_agentTestHelper->makeAsyncWebSocketRequest(__FILE__, __LINE__, req, id)

#define PARSE_NEXT_XML_RESPONSE(_id)                               \
  xmlDocPtr doc = nullptr;                                         \
  m_agentTestHelper->parseResponse(__FILE__, __LINE__, &doc, _id); \
  XmlDocFreer cleanup(doc)

#define PARSE_NEXT_JSON_RESPONSE(_id) \
  nlohmann::json jdoc;                \
  m_agentTestHelper->parseResponse(__FILE__, __LINE__, kdoc, _id)
