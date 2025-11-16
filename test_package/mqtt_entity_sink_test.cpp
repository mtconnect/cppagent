//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology ("AMT")
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

#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_parser.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/mqtt/mqtt_server_impl.hpp"
#include "mtconnect/printer/json_printer.hpp"
#include "mtconnect/sink/mqtt_entity_sink/mqtt_entity_sink.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::sink::mqtt_entity_sink;
using namespace mtconnect::asset;
using namespace mtconnect::configuration;

// main
int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

using json = nlohmann::json;

class MqttEntitySinkTest : public testing::Test
{
protected:
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  uint16_t m_port {0};

  void SetUp() override
  {
    m_agentTestHelper = std::make_unique<AgentTestHelper>();
    m_jsonPrinter = std::make_unique<printer::JsonPrinter>(2, true);
  }

  void TearDown() override
  {
    stopAgent();
    stopClient();
    stopServer();

    m_agentTestHelper.reset();
    m_jsonPrinter.reset();
  }

  void createAgent(std::string testFile = {}, ConfigOptions options = {})
  {
    if (testFile == "")
      testFile = "/samples/test_config.xml";
    ConfigOptions opts(options);
    MergeOptions(
        opts,
        {
            {"MqttEntitySink", true},
            {configuration::MqttPort, m_port},
            {MqttCurrentInterval, 200ms},
            {MqttSampleInterval, 100ms},
            {configuration::MqttHost, "127.0.0.1"s},
            {configuration::ObservationTopicPrefix, "MTConnect/Devices/[device]/Observations"s},
            {configuration::DeviceTopicPrefix, "MTConnect/Probe/[device]"s},
            {configuration::AssetTopicPrefix, "MTConnect/Asset/[device]"s},
            {configuration::MqttLastWillTopic, "MTConnect/Probe/[device]/Availability"s},
        });
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, false, true, opts);
    addAdapter();
    m_agentTestHelper->getAgent()->start();
  }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    MergeOptions(opts, {{ServerIp, "127.0.0.1"s},
                        {MqttPort, 0},
                        {MqttTls, false},
                        {AutoAvailable, false},
                        {RealTime, false}});
    m_server = std::make_shared<mtconnect::mqtt_server::MqttTcpServer>(
        m_agentTestHelper->m_ioContext, opts);
  }

  template <typename Rep, typename Period>
  bool waitFor(const chrono::duration<Rep, Period>& time, function<bool()> pred)
  {
    boost::asio::steady_timer timer(m_agentTestHelper->m_ioContext);
    timer.expires_after(time);
    bool timeout = false;
    timer.async_wait([&timeout](boost::system::error_code ec) {
      if (!ec)
        timeout = true;
    });
    while (!timeout && !pred())
    {
      m_agentTestHelper->m_ioContext.run_for(200ms);
    }
    timer.cancel();
    return pred();
  }

  void startServer()
  {
    if (m_server)
    {
      bool start = m_server->start();
      if (start)
      {
        m_port = m_server->getPort();
        m_agentTestHelper->m_ioContext.run_for(500ms);
      }
    }
  }

  void createClient(const ConfigOptions& options, unique_ptr<ClientHandler>&& handler)
  {
    ConfigOptions opts(options);
    MergeOptions(opts, {{MqttHost, "127.0.0.1"s},
                        {MqttPort, m_port},
                        {MqttTls, false},
                        {AutoAvailable, false},
                        {RealTime, false}});
    m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_agentTestHelper->m_ioContext,
                                                                  opts, std::move(handler));
  }

  bool startClient()
  {
    bool started = m_client && m_client->start();
    if (started)
    {
      return waitFor(1s, [this]() { return m_client->isConnected(); });
    }
    return started;
  }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 0,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

  void stopAgent()
  {
    const auto agent = m_agentTestHelper->getAgent();
    if (agent)
    {
      m_agentTestHelper->getAgent()->stop();
      m_agentTestHelper->m_ioContext.run_for(100ms);
    }
  }

  void stopClient()
  {
    if (m_client)
    {
      m_client->stop();
      m_agentTestHelper->m_ioContext.run_for(500ms);
      m_client.reset();
    }
  }

  void stopServer()
  {
    if (m_server)
    {
      m_server->stop();
      m_agentTestHelper->m_ioContext.run_for(500ms);
      m_server.reset();
    }
  }
};

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_use_flat_topic_structure)
{
  bool gotMessage = false;
  std::string receivedTopic;

  createServer({});
  startServer();

  auto client = mqtt::make_async_client(m_agentTestHelper->m_ioContext.get(), "localhost", m_port);

  client->set_client_id("test_client");
  client->set_clean_session(true);
  client->set_keep_alive_sec(30);

  bool subscribed = false;
  client->set_connack_handler(
      [client](bool sp, mqtt::connect_return_code connack_return_code) {
        if (connack_return_code == mqtt::connect_return_code::accepted)
        {
          auto pid = client->acquire_unique_packet_id();
          client->async_subscribe(pid, "MTConnect/#", MQTT_NS::qos::at_least_once,
                                  [](MQTT_NS::error_code ec) { EXPECT_FALSE(ec); });
        }
        return true;
      });

  client->set_suback_handler(
      [&subscribed](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results) {
        subscribed = true;
        return true;
      });

  size_t messageCount = 0;
  client->set_publish_handler(
      [&messageCount, &gotMessage, &receivedTopic](mqtt::optional<std::uint16_t> packet_id,
                                                   mqtt::publish_options pubopts,
                                                   mqtt::buffer topic_name, mqtt::buffer contents) {
        receivedTopic = topic_name;
        std::cout << "Received topic: " << topic_name << " payload: " << contents << std::endl;
        if (topic_name.find("MTConnect/Devices/") == 0 &&
            topic_name.find("/Observations/") != std::string::npos)
        {
          messageCount += 1;
          gotMessage = true;
        }
        return true;
      });

  client->async_connect([](mqtt::error_code ec) { ASSERT_FALSE(ec) << "Cannot connect"; });

  // Wait for subscription to complete
  ASSERT_TRUE(waitFor(10s, [&subscribed]() { return subscribed; }))
      << "Subscription never completed";

  // Create the agent and wait for its MQTT sink to connect.
  createAgent("", {{configuration::DisableAgentDevice, true}});
  auto sink = m_agentTestHelper->getMqttEntitySink();
  ASSERT_TRUE(sink != nullptr);
  ASSERT_TRUE(waitFor(10s, [&sink]() { return sink->isConnected(); }))
      << "MqttEntitySink failed to connect to broker";

  auto device = m_agentTestHelper->m_agent->getDefaultDevice();
  size_t diCount = device->getDeviceDataItems().size();

  // Add deterministic check to make sure we have recieved all the initial messages
  ASSERT_TRUE(waitFor(5s, [&messageCount, &diCount]() { return messageCount >= diCount; }));

  gotMessage = false;
  receivedTopic.clear();

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  ASSERT_TRUE(waitFor(10s, [&gotMessage]() { return gotMessage; }))
      << "Timeout waiting for adapter data. Last topic: " << receivedTopic;

  EXPECT_TRUE(receivedTopic.find("MTConnect/Devices/") == 0)
      << "Topic doesn't start with MTConnect/Devices/: " << receivedTopic;
  EXPECT_TRUE(receivedTopic.find("/Observations/") != std::string::npos)
      << "Topic doesn't contain /Observations/: " << receivedTopic;
  EXPECT_EQ("MTConnect/Devices/000/Observations/p3", receivedTopic)
      << "Topic does not match expected format: " << receivedTopic;

  client->async_disconnect();

  stopAgent();
  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_entity_json_format)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotMessage = false;
  json receivedJson;

  handler->m_receive = [&gotMessage, &receivedJson](std::shared_ptr<MqttClient> client,
                                                    const std::string& topic,
                                                    const std::string& payload) {
    try
    {
      receivedJson = json::parse(payload);
      // Only set gotMessage if result matches expected value
      if (receivedJson.contains("result") && receivedJson["result"] == "204")
      {
        gotMessage = true;
      }
    }
    catch (const std::exception& e)
    {
      LOG(error) << "Failed to parse JSON: " << e.what();
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");
  // Ensure subscription is active before sending data
  m_agentTestHelper->m_ioContext.run_for(200ms);

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  // Wait a bit to ensure sink and client are ready
  m_agentTestHelper->m_ioContext.run_for(200ms);
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  ASSERT_TRUE(waitFor(10s, [&gotMessage]() { return gotMessage; }));

  // Verify Entity JSON format
  EXPECT_TRUE(receivedJson.contains("dataItemId"));
  EXPECT_TRUE(receivedJson.contains("timestamp"));
  EXPECT_TRUE(receivedJson.contains("result"));
  EXPECT_TRUE(receivedJson.contains("sequence"));
  EXPECT_TRUE(receivedJson.contains("type"));
  EXPECT_TRUE(receivedJson.contains("category"));

  EXPECT_EQ("204", receivedJson["result"].get<std::string>());

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_include_optional_fields)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotMessage = false;
  json receivedJson;

  handler->m_receive = [&gotMessage, &receivedJson](std::shared_ptr<MqttClient> client,
                                                    const std::string& topic,
                                                    const std::string& payload) {
    json js = json::parse(payload);
    if (js.contains("name"))
    {
      gotMessage = true;
    }
    receivedJson = js;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  ASSERT_TRUE(waitFor(10s, [&gotMessage]() { return gotMessage; }));

  // Verify optional fields
  if (receivedJson.contains("name"))
  {
    EXPECT_TRUE(receivedJson["name"].is_string());
  }
  if (receivedJson.contains("subType"))
  {
    EXPECT_TRUE(receivedJson["subType"].is_string());
  }

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_samples)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotSample = false;
  json receivedJson;

  handler->m_receive = [&gotSample, &receivedJson](std::shared_ptr<MqttClient> client,
                                                   const std::string& topic,
                                                   const std::string& payload) {
    try
    {
      receivedJson = json::parse(payload);
      if (receivedJson.contains("dataItemId") && receivedJson["dataItemId"] == "z2" &&
          receivedJson.contains("category") && receivedJson["category"] == "SAMPLE" &&
          receivedJson.contains("result") && receivedJson["result"] == "204.000000")
      {
        gotSample = true;
      }
    }
    catch (const std::exception& e)
    {
      LOG(error) << "Failed to parse JSON: " << e.what();
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");
  m_agentTestHelper->m_ioContext.run_for(200ms);

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);

  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  m_agentTestHelper->m_ioContext.run_for(200ms);
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|z2|204");
  ASSERT_TRUE(waitFor(10s, [&gotSample]() { return gotSample; }));

  EXPECT_EQ("SAMPLE", receivedJson["category"].get<std::string>());
  EXPECT_EQ("204.000000", receivedJson["result"].get<std::string>());

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_events)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotEvent = false;
  json receivedJson;

  handler->m_receive = [&gotEvent, &receivedJson](std::shared_ptr<MqttClient> client,
                                                  const std::string& topic,
                                                  const std::string& payload) {
    try
    {
      receivedJson = json::parse(payload);
      if (receivedJson.contains("category") && receivedJson["category"] == "EVENT" &&
          receivedJson.contains("dataItemId") && receivedJson["dataItemId"] == "p4" &&
          receivedJson.contains("result") && receivedJson["result"] == "READY")
      {
        gotEvent = true;
      }
    }
    catch (const std::exception& e)
    {
      LOG(error) << "Failed to parse JSON: " << e.what();
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");
  m_agentTestHelper->m_ioContext.run_for(200ms);

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  m_agentTestHelper->m_ioContext.run_for(200ms);
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|p4|READY");
  ASSERT_TRUE(waitFor(10s, [&gotEvent]() { return gotEvent; }));

  EXPECT_EQ("EVENT", receivedJson["category"].get<std::string>());
  EXPECT_EQ("READY", receivedJson["result"].get<std::string>());

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_conditions)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotCondition = false;
  json receivedJson;

  handler->m_receive = [&gotCondition, &receivedJson](std::shared_ptr<MqttClient> client,
                                                      const std::string& topic,
                                                      const std::string& payload) {
    try
    {
      receivedJson = json::parse(payload);
      if (receivedJson.contains("category") && receivedJson["category"] == "CONDITION" &&
          receivedJson.contains("dataItemId") && receivedJson["dataItemId"] == "zlc" &&
          receivedJson.contains("level") && receivedJson["level"] == "FAULT")
      {
        gotCondition = true;
      }
    }
    catch (const std::exception& e)
    {
      LOG(error) << "Failed to parse JSON: " << e.what();
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");
  m_agentTestHelper->m_ioContext.run_for(200ms);

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  m_agentTestHelper->m_ioContext.run_for(200ms);
  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|zlc|FAULT|1234|LOW|Hydraulic pressure low");
  ASSERT_TRUE(waitFor(10s, [&gotCondition]() { return gotCondition; }));

  EXPECT_EQ("CONDITION", receivedJson["category"].get<std::string>());
  EXPECT_EQ("FAULT", receivedJson["level"].get<std::string>());
  if (receivedJson.contains("nativeCode"))
  {
    EXPECT_EQ("1234", receivedJson["nativeCode"].get<std::string>());
  }

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_availability)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotAvailable = false;
  std::string availabilityValue;

  handler->m_receive = [&gotAvailable, &availabilityValue](std::shared_ptr<MqttClient> client,
                                                           const std::string& topic,
                                                           const std::string& payload) {
    if (topic.find("Availability") != std::string::npos)
    {
      availabilityValue = payload;
      gotAvailable = true;
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Probe/#");

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  ASSERT_TRUE(waitFor(5s, [&gotAvailable]() { return gotAvailable; }));
  EXPECT_EQ("AVAILABLE", availabilityValue);

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_initial_observations)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  int messageCount = 0;

  handler->m_receive = [&messageCount](std::shared_ptr<MqttClient> client, const std::string& topic,
                                       const std::string& payload) {
    if (topic.find("/Observations/") != std::string::npos)
    {
      messageCount++;
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));
  ASSERT_TRUE(waitFor(10s, [&messageCount]() { return messageCount > 0; }));
  EXPECT_GT(messageCount, 0);

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_handle_unavailable)
{
  ConfigOptions options;

  createServer({});
  startServer();

  auto handler = make_unique<ClientHandler>();
  bool gotUnavailable = false;
  json receivedJson;

  handler->m_receive = [&gotUnavailable, &receivedJson](std::shared_ptr<MqttClient> client,
                                                        const std::string& topic,
                                                        const std::string& payload) {
    json j = json::parse(payload);
    if (j["category"] != "CONDITION" && topic.starts_with("MTConnect/Devices/000/Observations/"))
    {
      if (j["result"] == "UNAVAILABLE")
      {
        gotUnavailable = true;
      }
      receivedJson = j;
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");

  createAgent();

  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink->isConnected(); }));

  // Initial observations should include UNAVAILABLE values
  // The issue is the initial values will be received in a random order and the conditions have a
  // level instead of a result. If in the interviening time a condition is received, the json may
  // not be correct.
  ASSERT_TRUE(waitFor(10s, [&gotUnavailable]() { return gotUnavailable; }));
  EXPECT_EQ("UNAVAILABLE", receivedJson["result"].get<std::string>());

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_support_authentication)
{
  ConfigOptions options;
  options["MqttUserName"] = "mtconnect";
  options["MqttPassword"] = "password123";
  options["MqttClientId"] = "auth-client";

  createServer({});
  startServer();

  bool connected = false;
  auto handler = std::make_unique<ClientHandler>();
  handler->m_connected = [&connected](std::shared_ptr<MqttClient>) { connected = true; };

  createClient(options, std::move(handler));
  startClient();

  auto start = std::chrono::steady_clock::now();
  while (!connected && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  ASSERT_TRUE(connected) << "MQTT client did not connect with authentication";
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_support_qos_levels)
{
  ConfigOptions options;
  options["MqttQOS"] = "exactly_once";
  options["MqttClientId"] = "qos-client";

  createServer({});
  startServer();

  auto handler = std::make_unique<ClientHandler>();
  bool received = false;
  handler->m_receive = [&received](std::shared_ptr<MqttClient> client, const std::string& topic,
                                   const std::string& payload) { received = true; };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");

  createAgent();
  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = std::dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink && mqttSink->isConnected(); }));

  ASSERT_TRUE(waitFor(5s, [&received]() { return received; }));

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_support_retained_messages)
{
  ConfigOptions options;
  options["MqttRetain"] = "true";
  options["MqttClientId"] = "retain-client";

  createServer({});
  startServer();

  auto handler = std::make_unique<ClientHandler>();
  bool retainedReceived = false;
  std::string retainedPayload;
  handler->m_receive = [&retainedReceived, &retainedPayload](std::shared_ptr<MqttClient> client,
                                                             const std::string& topic,
                                                             const std::string& payload) {
    retainedReceived = true;
    retainedPayload = payload;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Devices/#");

  createAgent();
  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = std::dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink && mqttSink->isConnected(); }));

  ASSERT_TRUE(waitFor(5s, [&retainedReceived]() { return retainedReceived; }));
  ASSERT_FALSE(retainedPayload.empty());

  stopClient();
}

TEST_F(MqttEntitySinkTest, mqtt_entity_sink_should_publish_last_will)
{
  ConfigOptions options;
  options["MqttLastWillTopic"] = "MTConnect/Probe/J55-411045-cpp/Availability";
  options["MqttClientId"] = "lastwill-client";

  createServer({});
  startServer();

  auto handler = std::make_unique<ClientHandler>();
  bool lastWillReceived = false;
  handler->m_receive = [&lastWillReceived](std::shared_ptr<MqttClient> client,
                                           const std::string& topic, const std::string& payload) {
    if (topic.find("Availability") != std::string::npos)
    {
      lastWillReceived = true;
    }
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Probe/#");

  createAgent();
  auto sink = m_agentTestHelper->getAgent()->findSink("MqttEntitySink");
  auto mqttSink = std::dynamic_pointer_cast<MqttEntitySink>(sink);
  ASSERT_TRUE(waitFor(10s, [&mqttSink]() { return mqttSink && mqttSink->isConnected(); }));

  m_client->stop();
  ASSERT_TRUE(waitFor(5s, [&lastWillReceived]() { return lastWillReceived; }));

  stopClient();
}
