//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
#include "mtconnect/source/adapter/mqtt/mqtt_adapter.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::source::adapter::mqtt_adapter;
using namespace mtconnect::pipeline;
using namespace mtconnect::asset;

namespace asio = boost::asio;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(int32_t schemaVersion)
  : m_schemaVersion(schemaVersion)
  {}
  DevicePtr findDevice(const std::string &) override { return nullptr; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return nullptr;
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevices(std::list<DevicePtr>) override {}
  void deliverDevice(DevicePtr) override {}
  int32_t getSchemaVersion() const override { return m_schemaVersion; }
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }
  
  int32_t m_schemaVersion;
};

class MqttAdapterTest : public testing::Test
{
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(MqttAdapterTest, should_create_a_unique_id_based_on_topic)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Url, "mqtt://mybroker.com:1883"s},
    {configuration::Host, "mybroker.com"s},
    {configuration::Port, 1883},
    {configuration::Protocol, "mqtt"s},
    {configuration::Topics, StringList {"pipeline/#"s}}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
  auto adapter = make_unique<MqttAdapter>(ioc, context, options, tree);

  ASSERT_EQ("mqtt://mybroker.com:1883/", adapter->getName());
  ASSERT_EQ("_89c11f795e", adapter->getIdentity());
}

TEST_F(MqttAdapterTest, should_change_if_topics_change)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Url, "mqtt://mybroker.com:1883"s},
    {configuration::Host, "mybroker.com"s},
    {configuration::Port, 1883},
    {configuration::Protocol, "mqtt"s},
    {configuration::Topics, StringList {"pipeline/#"s}}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
  auto adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://mybroker.com:1883/", adapter->getName());
  ASSERT_EQ("_89c11f795e", adapter->getIdentity());
  
  options[configuration::Topics] = StringList {"pipline/#"s, "topic/"s};
  adapter = make_unique<MqttAdapter>(ioc, context, options, tree);

  ASSERT_EQ("_29e17b8870", adapter->getIdentity());
}

TEST_F(MqttAdapterTest, should_change_if_port_changes)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Url, "mqtt://mybroker.com:1883"s},
    {configuration::Host, "mybroker.com"s},
    {configuration::Port, 1883},
    {configuration::Protocol, "mqtt"s},
    {configuration::Topics, StringList {"pipeline/#"s}}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
  auto adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://mybroker.com:1883/", adapter->getName());
  ASSERT_EQ("_89c11f795e", adapter->getIdentity());
  
  options[configuration::Port] = 1882;
  adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://mybroker.com:1882/", adapter->getName());
  ASSERT_EQ("_7042e8f45e", adapter->getIdentity());
}

TEST_F(MqttAdapterTest, should_change_if_host_changes)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Url, "mqtt://mybroker.com:1883"s},
    {configuration::Host, "mybroker.com"s},
    {configuration::Port, 1883},
    {configuration::Protocol, "mqtt"s},
    {configuration::Topics, StringList {"pipeline/#"s}}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
  auto adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://mybroker.com:1883/", adapter->getName());
  ASSERT_EQ("_89c11f795e", adapter->getIdentity());
  
  options[configuration::Host] = "localhost";
  adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://localhost:1883/", adapter->getName());
  ASSERT_EQ("_4cd2e64d4e", adapter->getIdentity());
}

TEST_F(MqttAdapterTest, should_be_able_to_set_adapter_identity)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Url, "mqtt://mybroker.com:1883"s},
    {configuration::Host, "mybroker.com"s},
    {configuration::Port, 1883},
    {configuration::Protocol, "mqtt"s},
    {configuration::AdapterIdentity, "MyIdentity"s},
    {configuration::Topics, StringList {"pipeline/#"s}}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
  auto adapter = make_unique<MqttAdapter>(ioc, context, options, tree);
  
  ASSERT_EQ("mqtt://mybroker.com:1883/", adapter->getName());
  ASSERT_EQ("MyIdentity", adapter->getIdentity());
}
