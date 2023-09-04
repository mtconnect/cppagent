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

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/device_model/agent_device.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/version.h"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace device_model;
using namespace entity;

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace config = mtconnect::configuration;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class AgentDeviceTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    auto version = to_string(AGENT_VERSION_MAJOR) + "." + to_string(AGENT_VERSION_MINOR);
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, version, 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_agentDevice = m_agentTestHelper->m_agent->getAgentDevice();
  }

  void TearDown() override
  {
    m_agentTestHelper->m_ioContext.stop();
    if (m_server)
      m_server->close();
    m_server.reset();
    m_acceptor.reset();

    m_agentTestHelper.reset();
  }

  void addAdapter(bool suppress = false)
  {
    ConfigOptions options {{config::SuppressIPAddress, suppress}};
    m_agentTestHelper->addAdapter(options, "127.0.0.1", m_port, "LinuxCNC");
    m_agentTestHelper->m_adapter->setReconnectInterval(1s);
  }

  void startServer(const std::string addr = "127.0.0.1")
  {
    tcp::endpoint sp(ip::make_address(addr), m_port);
    m_connected = false;
    m_acceptor = make_unique<tcp::acceptor>(m_agentTestHelper->m_ioContext, sp);
    ASSERT_TRUE(m_acceptor->is_open());
    tcp::endpoint ep = m_acceptor->local_endpoint();
    m_port = ep.port();

    m_acceptor->async_accept([this](sys::error_code ec, tcp::socket socket) {
      if (ec)
        cout << ec.category().message(ec.value()) << ": " << ec.message() << endl;
      ASSERT_FALSE(ec);
      ASSERT_TRUE(socket.is_open());
      m_connected = true;
      m_server = make_unique<tcp::socket>(std::move(socket));
    });
  }

  template <typename Rep, typename Period>
  void runUntil(chrono::duration<Rep, Period> to, function<bool()> pred)
  {
    for (int runs = 0; runs < 10 && !pred(); runs++)
    {
      m_agentTestHelper->m_ioContext.run_one_for(to);
    }

    EXPECT_TRUE(pred());
  }

public:
  AgentDevicePtr m_agentDevice;
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  unsigned short m_port {0};

  std::unique_ptr<asio::ip::tcp::socket> m_server;
  std::unique_ptr<tcp::acceptor> m_acceptor;
  bool m_connected;
  string m_line;
};

TEST_F(AgentDeviceTest, AgentDeviceCreation)
{
  ASSERT_NE(nullptr, m_agentDevice);
  ASSERT_EQ(2, m_agentTestHelper->m_agent->getDevices().size());
  ASSERT_EQ("Agent", m_agentDevice->getName().str());
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
  auto device = agent->findDeviceByUUIDorName("000");
  auto &circ = agent->getCircularBuffer();
  ASSERT_TRUE(device);
  auto uuid = *device->getUuid();
  ASSERT_EQ("000", uuid);
  auto found = false;

  auto rest = m_agentTestHelper->getRestService();

  for (auto seq = circ.getSequence() - 1; !found && seq > 0ull; seq--)
  {
    auto event = circ.getFromBuffer(seq);
    if (event->getDataItem()->getType() == "DEVICE_ADDED" && uuid == event->getValue<string>())
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

#define ID_PREFIX "_d0c33d4315"

TEST_F(AgentDeviceTest, AdapterAddedProbeTest)
{
  m_port = 21788;
  addAdapter();
  {
    PARSE_XML_RESPONSE("/Agent/probe");
    auto version = to_string(AGENT_VERSION_MAJOR) + "." + to_string(AGENT_VERSION_MINOR);
    ASSERT_XML_PATH_EQUAL(doc, AGENT_PATH "@mtconnectVersion", version.c_str());

    ASSERT_XML_PATH_COUNT(doc, ADAPTERS_PATH "/*", 1);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@id", ID_PREFIX);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@name", "127.0.0.1:21788");

    ASSERT_XML_PATH_EQUAL(
        doc, ADAPTER_DATA_ITEMS_PATH "/m:DataItem[@id='" ID_PREFIX "_adapter_uri']@type",
        "ADAPTER_URI");
    ASSERT_XML_PATH_EQUAL(doc,
                          ADAPTER_DATA_ITEMS_PATH "/m:DataItem[@id='" ID_PREFIX
                                                  "_adapter_uri']/m:Constraints/m:Value",
                          m_agentTestHelper->m_adapter->getName().c_str());
  }
}

TEST_F(AgentDeviceTest, adapter_component_with_ip_address_suppressed)
{
  m_port = 21788;
  addAdapter(true);
  {
    PARSE_XML_RESPONSE("/Agent/probe");
    ASSERT_XML_PATH_COUNT(doc, ADAPTERS_PATH "/*", 1);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@id", ID_PREFIX);
    ASSERT_XML_PATH_EQUAL(doc, ADAPTER_PATH "@name", "LinuxCNC");

    ASSERT_XML_PATH_COUNT(
        doc, ADAPTER_DATA_ITEMS_PATH "/m:DataItem[@id='" ID_PREFIX "_adapter_uri']", 0);
  }
}

#define AGENT_DEVICE_STREAM "//m:DeviceStream[@name='Agent']"
#define AGENT_DEVICE_DEVICE_STREAM AGENT_DEVICE_STREAM "/m:ComponentStream[@component='Agent']"
#define AGENT_DEVICE_ADAPTER_STREAM AGENT_DEVICE_STREAM "/m:ComponentStream[@component='Adapter']"

TEST_F(AgentDeviceTest, AdapterAddedCurrentTest)
{
  {
    PARSE_XML_RESPONSE("/Agent/current");
  }
  addAdapter();

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_DEVICE_STREAM "/m:Events/m:Availability", "AVAILABLE");

    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_STREAM "/*", 2);
    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_DEVICE_STREAM "/*", 1);

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_DEVICE_STREAM "/m:Events/m:DeviceAdded", "000");

    ASSERT_XML_PATH_COUNT(doc, AGENT_DEVICE_ADAPTER_STREAM "/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:AdapterURI",
                          m_agentTestHelper->m_adapter->getName().c_str());
  }
}

TEST_F(AgentDeviceTest, TestAdapterConnectionStatus)
{
  srand(int32_t(chrono::system_clock::now().time_since_epoch().count()));
  m_port = rand() % 10000 + 5000;
  addAdapter();

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:AdapterURI",
                          m_agentTestHelper->m_adapter->getName().c_str());
    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->start();
  m_agentTestHelper->m_ioContext.run_for(1500ms);

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "LISTENING");
  }

  startServer();
  runUntil(10s, [this]() { return m_connected; });
  m_agentTestHelper->m_ioContext.run_for(10ms);

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "ESTABLISHED");
  }

  ASSERT_TRUE(m_server);
  m_server->close();
  m_acceptor->close();
  runUntil(1s, [this]() { return !m_agentTestHelper->m_adapter->isConnected(); });

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "CLOSED");
  }

  m_agentTestHelper->m_ioContext.run_for(1500ms);

  {
    PARSE_XML_RESPONSE("/Agent/current");

    ASSERT_XML_PATH_EQUAL(doc, AGENT_DEVICE_ADAPTER_STREAM "/m:Events/m:ConnectionStatus",
                          "LISTENING");
  }
}
