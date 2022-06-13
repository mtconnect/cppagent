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

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "agent_test_helper.hpp"
#include "client/mqtt/mqtt_client.cpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "sink/rest_sink/server.hpp"

using json = nlohmann::json;

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override
  {    
    // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();

    using namespace mtconnect::configuration;
    m_server = make_unique<Server>(m_context, ConfigOptions {{Port, 0}, {ServerIp, "127.0.0.1"s}});
  }

  void TearDown() override
  {
    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper->m_ioContext.run_for(100ms);
    m_agentTestHelper.reset();

    m_server.reset();   
  }  

  void createAgent(ConfigOptions options = {})
  {
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "2.0", 25, false, true,
                                   options);   

    m_agentTestHelper->getAgent()->start();

    m_client = m_agentTestHelper->getMqttService()->getClient();

    if (!m_client)
      createClient(options);
  } 

  void createServer(const ConfigOptions &options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts {{Port, 1883}, {ServerIp, "localhost"s}};
    opts.merge(ConfigOptions(options));
    m_server = make_unique<Server>(m_context, opts);
  }

  void startServer()
  {
    m_server->start();
    while (!m_server->isListening())
      m_context.run_one();
  }

  void createClient(const ConfigOptions &options)
  { 
      m_client = make_shared<mtconnect::mqtt_client::MqttClient>(m_context, options);
  }

  unique_ptr<Server> m_server;
  std::shared_ptr<MqttClientImpl> m_client;
  asio::io_context m_context;

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(MqttSinkTest, dynamic_load_Mqtt_sink)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);
}

TEST_F(MqttSinkTest, publish_Mqtt)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);

 string body;
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ("Body Content", request->m_body);
    body = request->m_body;

    ResponsePtr resp = make_unique<Response>(status::ok);
    session->writeResponse(move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", handler});

  startServer();

  mqttService->getClient()->start();

  //startClient();

 // m_client->spawnRequest(http::verb::get, "/probe", "Body Content", false);
 // ASSERT_TRUE(m_client->m_done);
 // ASSERT_EQ("Body Content", body);
  
}


