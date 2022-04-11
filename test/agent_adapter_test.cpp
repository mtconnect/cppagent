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

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "adapter/adapter.hpp"
#include "adapter/agent/agent_adapter.hpp"
#include "agent.hpp"
#include "device_model/reference.hpp"
#include "test_utilities.hpp"
#include "xml_printer.hpp"
#include "adapter/agent/url_parser.hpp"
#include "agent_test_helper.hpp"

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::rest_sink;
using namespace mtconnect::adapter;
using namespace mtconnect::observation;
using namespace mtconnect::adapter::agent;
using namespace mtconnect::pipeline;
using namespace mtconnect::asset;
using namespace mtconnect::observation;

using status = boost::beast::http::status;

struct MockPipelineContract : public PipelineContract
{
  MockPipelineContract(std::map<string, DataItemPtr> &items) : m_dataItems(items) {}
  DevicePtr findDevice(const std::string &device) override { return nullptr; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_dataItems[name];
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override
  {
    m_observations.push_back(obs);
  }
  void deliverAsset(AssetPtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}

  std::map<string, DataItemPtr> &m_dataItems;

  std::vector<ObservationPtr> m_observations;
};

class AgentAdapterTest : public testing::Test
{
public:
  typedef std::map<std::string, std::string> map_type;
  using queue_type = list<string>;

protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, true);
    m_agentTestHelper->getAgent()->start();
    m_agentId = to_string(getCurrentTimeInSec());
    
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems);
  }

  void TearDown() override
  {
    if (m_adapter)
    {
      m_adapter->stop();
      m_adapter.reset();
    }
    
    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper.reset();
  }
  
  auto createAdapter(int port, mtconnect::ConfigOptions options = {},
                     const std::string path = "")
  {
    using namespace mtconnect;
    using namespace mtconnect::adapter;

    boost::property_tree::ptree tree;
    tree.put(configuration::Url, "http://127.0.0.1:"s +
             boost::lexical_cast<string>(port) + "/"s + path);
    tree.put(configuration::Port, port);
    m_adapter = std::make_shared<agent::AgentAdapter>(m_agentTestHelper->m_ioContext, m_context, options, tree);

    return m_adapter;

  }

public:
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::shared_ptr<AgentAdapter> m_adapter;
  std::chrono::milliseconds m_delay {};
  shared_ptr<PipelineContext> m_context;
  std::map<string, DataItemPtr> m_dataItems;
};

TEST_F(AgentAdapterTest, should_connect_to_agent)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);
  
  adapter->start();
}
