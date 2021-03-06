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

#include "device_model/agent_device.hpp"
#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;

class AgentDeviceTest : public testing::Test
{
 protected:

  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml",
                                   8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_agentDevice = m_agentTestHelper->m_agent->getAgentDevice();
  }

  void TearDown() override
  {
    if (m_server)
    {
      m_agentTestHelper->m_adapter->stop();
      m_server.reset();
      m_serverSocket.reset();
    }

    m_agentTestHelper.reset();
  }

  void addAdapter(bool suppress = false)
  {
    ConfigOptions options{{configuration::SuppressIPAddress, suppress}};
    m_agentTestHelper->addAdapter(options, "127.0.0.1", m_port, "LinuxCNC");
    m_agentTestHelper->m_adapter->setReconnectInterval(1s);
  }
  
  void createListener()
  {
    ASSERT_TRUE(!create_listener(m_server, m_port, "127.0.0.1"));
    m_port = m_server->get_listening_port();
  }
  
 public:
  AgentDevice *m_agentDevice{nullptr};
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port{21788};
  dlib::scoped_ptr<dlib::listener> m_server;
  dlib::scoped_ptr<dlib::connection> m_serverSocket;
};

TEST_F(AgentDeviceTest, AgentDeviceCreation)
{
  ASSERT_NE(nullptr, m_agentDevice);
  ASSERT_EQ(2, m_agentTestHelper->m_agent->getDevices().size());
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
  auto agent = m_agentTestHelper->getAgent();
  auto &uuid = agent->getDevices().back()->getUuid();
  ASSERT_EQ("000", uuid);
  auto found = false;
  
  for (auto seq = agent->getSequence() - 1; !found && seq > 0ull; seq--)
  {
    auto event = agent->getFromBuffer(seq);
    if (event->getDataItem()->getType() == "DEVICE_ADDED" &&
        uuid == event->getValue<string>())
    {
      found = true;
    }
  }

  ASSERT_TRUE(found);
}

#define AGENT_PATH "//m:Agent"
#define AGENT_DATA_ITEMS_PATH AGENT_PATH "/m:DataItems"
#define ADAPTERS_PATH AGENT_PATH "/m:Components/m:Adapters"
#define ADAPTER_PATH ADAPTERS_PATH "/m:Components/m:Adapter"
#define ADAPTER_DATA_ITEMS_PATH ADAPTER_PATH "/m:DataItems"

#define ID_PREFIX "_83a0a5df80"

TEST_F(AgentDeviceTest, AdapterAddedProbeTest)
{
  addAdapter();
  {
    PARSE_XML_RESPONSE("/Agent/probe");
    ASSERT_XML_PATH_COUNT(doc, ADAPTERS_PATH "/*", 1);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@id", ID_PREFIX);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@name", "127.0.0.1:21788");
    
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_DATA_ITEMS_PATH
                          "/m:DataItem[@id='" ID_PREFIX "_adapter_uri']@type",
                          "ADAPTER_URI");
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_DATA_ITEMS_PATH
                          "/m:DataItem[@id='" ID_PREFIX "_adapter_uri']/m:Constraints/m:Value",
                          m_agentTestHelper->m_adapter->getUrl().c_str());
  }
}

TEST_F(AgentDeviceTest, adapter_component_with_ip_address_suppressed)
{
  addAdapter(true);
  {
    PARSE_XML_RESPONSE("/Agent/probe");
    m_agentTestHelper->printResponse();
    ASSERT_XML_PATH_COUNT(doc, ADAPTERS_PATH "/*", 1);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@id", ID_PREFIX);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@name", "LinuxCNC");
    
    ASSERT_XML_PATH_COUNT(doc, ADAPTER_DATA_ITEMS_PATH
                          "/m:DataItem[@id='" ID_PREFIX "_adapter_uri']", 0);
  }
}

#define AGENT_DEVICE_STREAM "//m:DeviceStream[@name='Agent']"
#define AGENT_DEVICE_DEVICE_STREAM AGENT_DEVICE_STREAM "/m:ComponentStream[@component='Agent']"
#define AGENT_DEVICE_ADAPTER_STREAM AGENT_DEVICE_STREAM "/m:ComponentStream[@component='Adapter']"


TEST_F(AgentDeviceTest, AdapterAddedCurrentTest)
{
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/Agent/current");
    
    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_STREAM "/*", 2);
    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_DEVICE_STREAM "/*", 1);

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_DEVICE_STREAM "/m:Events/m:DeviceAdded",
                          "000");

    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_ADAPTER_STREAM "/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:AdapterURI",
                          m_agentTestHelper->m_adapter->getUrl().c_str());
  }
}

TEST_F(AgentDeviceTest, TestAdapterConnectionStatus)
{
  m_port = rand() % 10000 + 5000;
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/Agent/current");
          
    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:AdapterURI",
                          m_agentTestHelper->m_adapter->getUrl().c_str());
    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "UNAVAILABLE");

  }
  m_agentTestHelper->m_adapter->start();
  this_thread::sleep_for(100ms);

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "LISTENING");

  }
  createListener();
  this_thread::sleep_for(100ms);

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  this_thread::sleep_for(100ms);
  ASSERT_TRUE(m_serverSocket.get());

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "ESTABLISHED");
    
  }
  
  m_serverSocket->shutdown();
  m_server.reset();
  this_thread::sleep_for(100ms);

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "CLOSED");
    
  }
  this_thread::sleep_for(1100ms);
  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "LISTENING");
    
  }
}
