//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "agent_device.hpp"
#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

using namespace std;
using namespace mtconnect;

class AgentDeviceTest : public testing::Test
{
 protected:

  void SetUp() override
  {
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/test_config.xml", 8, 4, "1.7", 25);
    m_agentId = intToString(getCurrentTimeInSec());
    m_adapter = nullptr;
    m_agentDevice = nullptr;
    
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();
    m_agentTestHelper->m_queries.clear();
    m_agentDevice = m_agent->getAgentDevice();
  }

  void TearDown() override
  {
    m_agent.reset();
    m_adapter = nullptr;
    m_agentTestHelper.reset();
  }

  void addAdapter()
  {
    ASSERT_FALSE(m_adapter);
    m_adapter = new Adapter("LinuxCNC", "server", 7878);
    m_agent->addAdapter(m_adapter);
    ASSERT_TRUE(m_adapter);
  }

 public:
  AgentDevice *m_agentDevice{nullptr};
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(AgentDeviceTest, AgentDeviceCreation)
{
  ASSERT_NE(nullptr, m_agentDevice);
  ASSERT_EQ(2, m_agent->getDevices().size());
  ASSERT_EQ("Agent", m_agentDevice->getName());
}

TEST_F(AgentDeviceTest, VerifyRequiredDataItems)
{
  ASSERT_NE(nullptr, m_agentDevice);
  auto avail = m_agentDevice->getDeviceDataItem("agent_avail");
  ASSERT_NE(nullptr, avail);
  ASSERT_EQ("AVAILABILITY", avail->getType());

  auto added = m_agentDevice->getDeviceDataItem("device_added");
  ASSERT_NE(nullptr, added);
  ASSERT_EQ("DEVICE_ADDED", added->getType());

  auto removed = m_agentDevice->getDeviceDataItem("device_removed");
  ASSERT_NE(nullptr, removed);
  ASSERT_EQ("DEVICE_REMOVED", removed->getType());

  auto changed = m_agentDevice->getDeviceDataItem("device_changed");
  ASSERT_NE(nullptr, changed);
  ASSERT_EQ("DEVICE_CHANGED", changed->getType());
}

TEST_F(AgentDeviceTest, DeviceAddedItemsInBuffer)
{
  auto &uuid = m_agent->getDevices()[1]->getUuid();
  ASSERT_EQ("000", uuid);
  auto found = false;
  
  for (auto seq = m_agent->getSequence() - 1; !found && seq > 0ull; seq--)
  {
    auto event = m_agent->getFromBuffer(seq);
    if (event->getDataItem()->getType() == "DEVICE_ADDED" &&
        uuid == event->getValue())
    {
      found = true;
    }
  }

  ASSERT_TRUE(found);
}

#define AGENT_PATH "//m:Agent"
#define AGENT_DATA_ITEMS_PATH AGENT_PATH "/m:DataItems"
#define ADAPTER_PATH AGENT_PATH "/m:Components/m:Adapters/m:Components/m:Adapter"

TEST_F(AgentDeviceTest, AdapterAddedTest)
{
  addAdapter();
  
  {
    m_agentTestHelper->m_path = "/Agent/probe";
    {
      PARSE_XML_RESPONSE;
      
    }

  }
}
