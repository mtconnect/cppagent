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

#include "printer/json_printer.hpp"

#include "configuration/agent_config.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "pipeline/pipeline_context.hpp"
#include "configuration/agent_config.hpp"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;
using namespace mtconnect::sink;
using namespace mtconnect::sink::mqtt_sink;
namespace asio = boost::asio;
//using json = nlohmann::json;

namespace mtconnect {

  class mqtt_sink_plugin_test : public sink::Sink
  {
  public:
    mqtt_sink_plugin_test(const string &name, boost::asio::io_context &context,
                          sink::SinkContractPtr &&contract, const ConfigOptions &config)
      : sink::Sink(name, move(contract))
    {}

    ~mqtt_sink_plugin_test() = default;

    // Sink Methods
    void start() override {}

    void stop() override {}

    uint64_t publish(observation::ObservationPtr &observation) override { return 0; }

    bool publish(asset::AssetPtr asset) override { return false; }

    static sink::SinkPtr create(const std::string &name, boost::asio::io_context &io,
                                sink::SinkContractPtr &&contract, const ConfigOptions &options,
                                const boost::property_tree::ptree &block)
    {
      return std::make_shared<mqtt_sink_plugin_test>(name, io, std::move(contract), options);
    }

    static void register_factory(const boost::property_tree::ptree &block,
                                 configuration::AgentConfiguration &config)
    {
      mtconnect::configuration::gAgentLogger = config.getLogger();
      PLUGIN_LOG(debug) << "Registering sink factory for mqtt_sink_plugin_test";
      config.getSinkFactory().registerFactory("mqtt_sink_plugin_test", &mqtt_sink_plugin_test::create);
    }
  };

  BOOST_DLL_ALIAS(mqtt_sink_plugin_test::register_factory, initialize_plugin)
}

class MqttSinkTest : public testing::Test
{
protected:

  void SetUp() override
  {
    m_config = std::make_unique<AgentConfiguration>();
    m_config->setDebug(true);
    m_cwd = std::filesystem::current_path();
  }

  void TearDown() override
  {
    m_config.reset();
    chdir(m_cwd.string().c_str());
  }

  std::unique_ptr<AgentConfiguration> m_config;
  std::filesystem::path m_cwd;
};


TEST_F(MqttSinkTest, dynamic_load_Mqtt_sink)
{
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

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService != nullptr);
}

TEST_F(MqttSinkTest, dynamic_load_mqttsinks_with_plugin_block)
{
  chdir(TEST_BIN_ROOT_DIR);

  m_config->updateWorkingDirectory();

  string str(R"(
Plugins {
   mqtt_sink_plugin_test {
   }
}
Sinks {
      mqtt_sink_plugin_test {
    }
}
)");

  m_config->loadConfig(str);
  auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

  ASSERT_TRUE(agent);

  //const auto sink = agent->findSink("mqtt_sink_plugin_test");
  //ASSERT_TRUE(sink != nullptr);
}


