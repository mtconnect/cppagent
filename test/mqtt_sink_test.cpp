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
#include <boost/dll/alias.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "configuration/agent_config.hpp"
#include "entity/entity.hpp"
#include "entity/xml_parser.hpp"
#include "json_helper.hpp"
#include "pipeline/pipeline_context.hpp"
#include "printer/json_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "source/adapter/agent_adapter/agent_adapter.hpp"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

using json = nlohmann::json;

using namespace std;
using namespace mtconnect;
using namespace mtconnect::asset;
using namespace mtconnect::configuration;
using namespace mtconnect::entity;
using namespace mtconnect::pipeline;
using namespace mtconnect::sink;
using namespace mtconnect::sink::mqtt_sink;
using namespace mtconnect::source::adapter;
using namespace mtconnect::source::adapter::agent_adapter;

namespace asio = boost::asio;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override
  {    
    m_config = std::make_unique<AgentConfiguration>();
    m_config->setDebug(true);
    m_cwd = std::filesystem::current_path();

    // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_context = make_shared<PipelineContext>();
  }

  void TearDown() override
  {
    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper->m_ioContext.run_for(100ms);
    m_agentTestHelper.reset();

    m_config.reset();
    chdir(m_cwd.string().c_str());
  }  

  void createAgent(ConfigOptions options = {})
  {
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, false, true,
                                   options);
    m_agentTestHelper->getAgent()->start();
  }

   auto createAdapter(int port, mtconnect::ConfigOptions options = {}, const std::string path = "",
                     int hb = 500)
  {
    using namespace mtconnect;
    using namespace mtconnect::source::adapter;

    string url = "http://127.0.0.1:"s + boost::lexical_cast<string>(port) + "/"s + path;
    options.emplace(configuration::Url, url);
    options.emplace(configuration::Device, "LinuxCNC"s);
    options.emplace(configuration::SourceDevice, "LinuxCNC"s);
    options.emplace(configuration::Port, port);
    options.emplace(configuration::Count, 100);
    options.emplace(configuration::Heartbeat, Milliseconds(hb));
    options.emplace(configuration::ReconnectInterval, 500ms);

    boost::property_tree::ptree tree;
    m_adapter = std::make_shared<agent_adapter::AgentAdapter>(m_agentTestHelper->m_ioContext,
                                                              m_context, options, tree);

    return m_adapter;
  }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->defaultDevice()->getName());
  }

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::shared_ptr<AgentAdapter> m_adapter;
  shared_ptr<PipelineContext> m_context;
  std::unique_ptr<AgentConfiguration> m_config;
  std::filesystem::path m_cwd;
};

TEST_F(MqttSinkTest, dynamic_load_Mqtt_sink)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);
}

TEST_F(MqttSinkTest, load_Mqtt_sink)
{
  createAgent();

  chdir(TEST_BIN_ROOT_DIR);

  m_config->updateWorkingDirectory();

  string str(R"(
Sinks {
      MqttService {
    }
}
)");

  m_config->loadConfig(str);

  auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

  ASSERT_TRUE(agent);

  const auto sink = agent->findSink("MqttService");
  ASSERT_TRUE(sink);

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService != nullptr);
}

TEST_F(MqttSinkTest, publish_Mqtt)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);

 /* auto port = 1883;

  auto adapter = createAdapter(port, {{configuration::Device, "Sub_Machine"s}}, "", 5000);

  addAdapter();

  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });*/
}
