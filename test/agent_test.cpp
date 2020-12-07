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

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "test_globals.hpp"
#include "xml_printer.hpp"

#include <dlib/server.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;

class AgentTest : public testing::Test
{
 public:
  typedef dlib::map<std::string, std::string>::kernel_1a_c map_type;
  using queue_type = dlib::queue<std::string>::kernel_1a_c;

 protected:
  void SetUp() override
  {
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/test_config.xml", 8, 4, "1.3", 25);
    m_agentId = intToString(getCurrentTimeInSec());
    m_adapter = nullptr;

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();
    m_agentTestHelper->m_queries.clear();
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
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  std::chrono::milliseconds m_delay{};
};

TEST_F(AgentTest, Constructor)
{
  ASSERT_THROW(Agent(PROJECT_ROOT_DIR "/samples/badPath.xml", 17, 8, "1.5"), std::runtime_error);
  ASSERT_NO_THROW(Agent(PROJECT_ROOT_DIR "/samples/test_config.xml", 17, 8, "1.5"));
}

TEST_F(AgentTest, BadPath)
{
  auto pathError = getFile(PROJECT_ROOT_DIR "/samples/test_error.xml");

  {
    m_agentTestHelper->m_path = "/bad_path";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + m_agentTestHelper->m_path;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    m_agentTestHelper->m_path = "/bad/path/";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + m_agentTestHelper->m_path;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    m_agentTestHelper->m_path = "/LinuxCNC/current/blah";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + m_agentTestHelper->m_path;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

TEST_F(AgentTest, BadXPath)
{
  m_agentTestHelper->m_path = "/current";
  key_value_map query;

  {
    query["path"] = "//////Linear";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    query["path"] = "//Axes?//Linear";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    query["path"] = "//Devices/Device[@name=\"I_DON'T_EXIST\"";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string)
    "The path could not be parsed. Invalid syntax: //Devices/Device[@name=\"I_DON'T_EXIST\"";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

TEST_F(AgentTest, BadCount)
{
  m_agentTestHelper->m_path = "/sample";
  key_value_map query;

  {
    query["count"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must an integer.");
  }

  {
    query["count"] = "-500";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than or equal to ");
    value += int32ToString(-m_agent->getBufferSize()) + ".";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    query["count"] = "0";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must not be 0.");
  }

  {
    query["count"] = "500";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than or equal to ");
    value += intToString(m_agent->getBufferSize()) + ".";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    query["count"] = "9999999";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than or equal to ");
    value += intToString(m_agent->getBufferSize()) + ".";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }
  
  {
    query["count"] = "-9999999";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than or equal to ");
    value += int32ToString(-m_agent->getBufferSize()) + ".";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

}

TEST_F(AgentTest, BadFreq)
{
  m_agentTestHelper->m_path = "/sample";
  key_value_map query;

  {
    query["frequency"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
  }

  {
    query["frequency"] = "-123";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
  }

  {
    query["frequency"] = "2147483647";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "'frequency' must be less than or equal to 2147483646.");
  }

  {
    query["frequency"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY(query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "'frequency' must be less than or equal to 2147483646.");
  }
}

TEST_F(AgentTest, GoodPath)
{
  {
    m_agentTestHelper->m_path = "/current?path=//Power";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Power']//m:PowerState",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Path']//m:Condition", "");
  }
}

TEST_F(AgentTest, XPath)
{
  m_agentTestHelper->m_path = "/current";
  key_value_map query;

  {
    query["path"] = "//Rotary[@name='C']//DataItem[@category='SAMPLE' or @category='CONDITION']";
    PARSE_XML_RESPONSE_QUERY(query);

    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:SpindleSpeed",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Load", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Unavailable", "");
  }
}

TEST_F(AgentTest, Probe)
{
  {
    m_agentTestHelper->m_path = "/probe";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }

  {
    m_agentTestHelper->m_path = "/";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }

  {
    m_agentTestHelper->m_path = "/LinuxCNC";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }
}

TEST_F(AgentTest, EmptyStream)
{
  {
    m_agentTestHelper->m_path = "/current";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']@name", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Unavailable",
                          nullptr);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Unavailable@qualifier",
        nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode", "SPINDLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ToolGroup", "UNAVAILABLE");
  }

  {
    m_agentTestHelper->m_path = "/sample";
    char line[80] = {0};
    sprintf(line, "%d", (int)m_agent->getSequence());
    PARSE_XML_RESPONSE_QUERY_KV("from", line);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, BadDevices)
{
  {
    m_agentTestHelper->m_path = "/LinuxCN/probe";
    PARSE_XML_RESPONSE;
    string message = (string) "Could not find the device 'LinuxCN'";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

TEST_F(AgentTest, AddAdapter)
{
  addAdapter();
}

TEST_F(AgentTest, AddToBuffer)
{
  string device("LinuxCNC"), key("badKey"), value("ON");
  auto di1 = m_agent->getDataItemByName(device, key);
  ASSERT_FALSE(di1);
  int seqNum = m_agent->addToBuffer(di1, value, "NOW");
  ASSERT_EQ(0, seqNum);

  auto event1 = m_agent->getFromBuffer(seqNum);
  ASSERT_FALSE(event1);

  {
    string last = to_string(m_agent->getSequence());
    m_agentTestHelper->m_path = "/sample";
    PARSE_XML_RESPONSE_QUERY_KV("from", last);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }

  key = "power";

  auto di2 = m_agent->getDataItemByName(device, key);
  seqNum = m_agent->addToBuffer(di2, value, "NOW");
  auto event2 = m_agent->getFromBuffer(seqNum);
  ASSERT_EQ(2, (int)event2->refCount());

  {
    m_agentTestHelper->m_path = "/current";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "ON");
  }

  {
    m_agentTestHelper->m_path = "/sample";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[2]", "ON");
  }
}

TEST_F(AgentTest, Adapter)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|alarm|code|nativeCode|severity|state|description");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[2]", "description");
  }
}

TEST_F(AgentTest, CurrentAt)
{
  m_agentTestHelper->m_path = "/current";
  string key("at"), value;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Get the current position
  int seq = m_agent->getSequence();
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 100; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 0; i < 100; i++)
  {
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }

  // Test buffer wrapping
  // Add a large many events
  for (int i = 101; i <= 301; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 100; i < 301; i++)
  {
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }

  // Check the first couple of items in the list
  for (int j = 0; j < 10; j++)
  {
    int i = m_agent->getSequence() - m_agent->getBufferSize() - seq + j;
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }

  // Test out of range...
  {
    int i = m_agent->getSequence() - m_agent->getBufferSize() - seq - 1;
    value = intToString(i + seq);
    sprintf(line, "'at' must be greater than or equal to %d.", i + seq + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, CurrentAt64)
{
  m_agentTestHelper->m_path = "/current";
  string key("at"), value;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Get the current position
  char line[80] = {0};

  // Initialize the sliding buffer at a very large number.
  uint64_t start = (((uint64_t)1) << 48) + 1317;
  m_agent->setSequence(start);

  // Add many events
  for (uint64_t i = 1; i <= 500; i++)
  {
    sprintf(line, "TIME|line|%d", (int)i);
    m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (uint64_t i = start + 300; i < start + 500; i++)
  {
    value = int64ToString(i);
    sprintf(line, "%d", (int)(i - start) + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }
}

TEST_F(AgentTest, CurrentAtOutOfRange)
{
  m_agentTestHelper->m_path = "/current";
  string key("at"), value;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 200; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    m_adapter->processData(line);
  }

  int seq = m_agent->getSequence();

  {
    value = intToString(seq);
    sprintf(line, "'at' must be less than or equal to %d.", seq - 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }

  seq = m_agent->getFirstSequence() - 1;

  {
    value = intToString(seq);
    sprintf(line, "'at' must be greater than or equal to %d.", seq + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, SampleAtNextSeq)
{
  m_agentTestHelper->m_path = "/sample";
  string key("from"), value;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 300; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    m_adapter->processData(line);
  }

  int seq = m_agent->getSequence();

  {
    value = intToString(seq);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, SequenceNumberRollover)
{
#ifndef WIN32
  key_value_map kvm;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Set the sequence number near MAX_UINT32
  m_agent->setSequence(0xFFFFFFA0);
  int64_t seq = m_agent->getSequence();
  ASSERT_EQ((int64_t)0xFFFFFFA0, seq);

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    m_adapter->processData(line);

    {
      m_agentTestHelper->m_path = "/current";
      PARSE_XML_RESPONSE;
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
                            int64ToString(seq + i).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", int64ToString(seq + i + 1).c_str());
    }

    {
      m_agentTestHelper->m_path = "/sample";
      kvm["from"] = int64ToString(seq);
      kvm["count"] = "128";

      PARSE_XML_RESPONSE_QUERY(kvm);
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", int64ToString(seq + i + 1).c_str());

      for (int j = 0; j <= i; j++)
      {
        sprintf(line, "//m:DeviceStream//m:Line[%d]@sequence", j + 1);
        ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j).c_str());
      }
    }

    for (int j = 0; j <= i; j++)
    {
      m_agentTestHelper->m_path = "/sample";
      kvm["from"] = int64ToString(seq + j);
      kvm["count"] = "1";

      PARSE_XML_RESPONSE_QUERY(kvm);
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
                            int64ToString(seq + j).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", int64ToString(seq + j + 1).c_str());
    }
  }

  ASSERT_EQ((uint64_t)0xFFFFFFA0 + 128, m_agent->getSequence());
#endif
}

TEST_F(AgentTest, SampleCount)
{
  key_value_map kvm;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  int64_t seq = m_agent->getSequence();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "TIME|line|%d|Xact|%d", i, i);
    m_adapter->processData(line);
  }

  {
    m_agentTestHelper->m_path = "/sample";
    kvm["path"] = "//DataItem[@name='Xact']";
    kvm["from"] = int64ToString(seq);
    kvm["count"] = "10";

    PARSE_XML_RESPONSE_QUERY(kvm);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", int64ToString(seq + 20).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

    // Make sure we got 10 lines
    for (int j = 0; j < 10; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j * 2 + 1).c_str());
    }
  }
}


TEST_F(AgentTest, SampleLastCount)
{
  key_value_map kvm;

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);


  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "TIME|line|%d|Xact|%d", i, i);
    m_adapter->processData(line);
  }
  
  int64_t seq = m_agent->getSequence() - 20;

  {
    m_agentTestHelper->m_path = "/sample";
    kvm["path"] = "//DataItem[@name='Xact']";
    kvm["count"] = "-10";

    PARSE_XML_RESPONSE_QUERY(kvm);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", int64ToString(seq).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

    // Make sure we got 10 lines
    for (int j = 0; j < 10; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j * 2 + 1).c_str());
    }
  }
}


TEST_F(AgentTest, AdapterCommands)
{
  m_agentTestHelper->m_path = "/probe";

  auto device = m_agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->m_preserveUuid);

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->parseBuffer("* uuid: MK-1234\n");
  m_adapter->parseBuffer("* manufacturer: Big Tool\n");
  m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
  m_adapter->parseBuffer("* station: YYYY\n");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
  }

  device->m_preserveUuid = true;
  m_adapter->parseBuffer("* uuid: XXXXXXX\n");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
  }
}

TEST_F(AgentTest, AdapterDeviceCommand)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/two_devices.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();
  m_agentTestHelper->m_path = "/probe";

  auto device1 = m_agent->getDeviceByName("Device1");
  ASSERT_TRUE(device1);
  auto device2 = m_agent->getDeviceByName("Device2");
  ASSERT_TRUE(device2);

  m_adapter = new Adapter("*", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);
  ASSERT_TRUE(nullptr == m_adapter->getDevice());

  m_adapter->parseBuffer("* device: device-2\n");
  ASSERT_TRUE(device2 == m_adapter->getDevice());

  m_adapter->parseBuffer("* device: device-1\n");
  ASSERT_TRUE(device1 == m_adapter->getDevice());

  m_adapter->parseBuffer("* device: Device2\n");
  ASSERT_TRUE(device2 == m_adapter->getDevice());

  m_adapter->parseBuffer("* device: Device1\n");
  ASSERT_TRUE(device1 == m_adapter->getDevice());
}

TEST_F(AgentTest, UUIDChange)
{
  m_agentTestHelper->m_path = "/probe";

  auto device = m_agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->m_preserveUuid);

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->parseBuffer("* uuid: MK-1234\n");
  m_adapter->parseBuffer("* manufacturer: Big Tool\n");
  m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
  m_adapter->parseBuffer("* station: YYYY\n");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
  }

  m_agentTestHelper->m_path = "/current?path=//Device[@uuid=\"MK-1234\"]";
  {
    // TODO: Fix and make sure dom is updated so this xpath will parse correctly.
    // PARSE_XML_RESPONSE_QUERY_KV("path", "//Device[@uuid=\"MK-1234\"]");
    // ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream@uuid", "MK-1234");
  }
}

TEST_F(AgentTest, FileDownload)
{
  string uri("/schemas/MTConnectDevices_1.1.xsd");

  // Register a file with the agent.
  m_agent->registerFile(uri, PROJECT_ROOT_DIR "/schemas/MTConnectDevices_1.1.xsd");

  // Reqyest the file...
  IncomingThings incoming("", "", 0, 0);
  OutgoingThings outgoing;
  incoming.request_type = "GET";
  incoming.path = uri;
  incoming.queries = m_agentTestHelper->m_queries;
  incoming.cookies = m_agentTestHelper->m_cookies;
  incoming.headers = m_agentTestHelper->m_incomingHeaders;

  outgoing.m_out = &m_agentTestHelper->m_out;

  m_agentTestHelper->m_result = m_agent->httpRequest(incoming, outgoing);
  ASSERT_TRUE(m_agentTestHelper->m_result.empty());
  ASSERT_TRUE(m_agentTestHelper->m_out.bad());
  ASSERT_TRUE(m_agentTestHelper->m_out.str().find_last_of("TEST SCHEMA FILE 1234567890\n") !=
              string::npos);
}

TEST_F(AgentTest, FailedFileDownload)
{
  m_agentTestHelper->m_path = "/schemas/MTConnectDevices_1.1.xsd";
  string error = "The following path is invalid: " + m_agentTestHelper->m_path;

  // Register a file with the agent.
  m_agent->registerFile(m_agentTestHelper->m_path, "./BadFileName.xsd");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "UNSUPPORTED");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", error.c_str());
  }
}

TEST_F(AgentTest, DuplicateCheck)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  m_adapter->setDupCheck(true);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }

  m_adapter->processData("TIME|line|204");
  m_adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
  }
}

TEST_F(AgentTest, DuplicateCheckAfterDisconnect)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);
  m_adapter->setDupCheck(true);

  m_adapter->processData("TIME|line|204");
  m_adapter->processData("TIME|line|204");
  m_adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
  }

  m_adapter->disconnected();

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
  }

  m_adapter->connected();

  m_adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[5]", "205");
  }
}

TEST_F(AgentTest, AutoAvailable)
{
  m_agentTestHelper->m_path = "/LinuxCNC/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);

  m_adapter->setAutoAvailable(true);
  auto d = m_agent->getDevices()[0];
  std::vector<Device *> devices;
  devices.emplace_back(d);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
  }

  m_agent->connected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
  }

  m_agent->disconnected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
  }

  m_agent->connected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[4]", "AVAILABLE");
  }
}

TEST_F(AgentTest, MultipleDisconnect)
{
  m_agentTestHelper->m_path = "/LinuxCNC/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);
  
  auto d = m_agent->getDevices()[0];
  std::vector<Device *> devices;
  devices.emplace_back(d);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
  }

  m_agent->connected(m_adapter, devices);
  m_adapter->processData("TIME|block|GTH");
  m_adapter->processData("TIME|cmp|normal||||");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 2);

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);
  }

  m_agent->disconnected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
  }

  m_agent->disconnected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
  }

  m_agent->connected(m_adapter, devices);
  m_adapter->processData("TIME|block|GTH");
  m_adapter->processData("TIME|cmp|normal||||");

  m_agent->disconnected(m_adapter, devices);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 3);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 2);

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 5);
  }
}

TEST_F(AgentTest, IgnoreTimestamps)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->processData("TIME|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
  }

  m_adapter->setIgnoreTimestamps(true);
  m_adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]@timestamp", "!TIME");
  }
}

TEST_F(AgentTest, AssetStorage)
{
  m_agent->enablePut();
  m_agentTestHelper->m_path = "/asset/123";
  string body = "<Part>TEST</Part>";
  key_value_map queries;

  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
  }

  // The device should generate an asset changed event as well.
  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentTest, AssetBuffer)
{
  m_agent->enablePut();
  m_agentTestHelper->m_path = "/asset/1";
  string body = "<Part>TEST 1</Part>";
  key_value_map queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
    ASSERT_EQ(1, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
    ASSERT_EQ(1, m_agent->getAssetCount("Part"));
  }

  m_agentTestHelper->m_path = "/asset/2";
  body = "<Part>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());
    ASSERT_EQ(2, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  m_agentTestHelper->m_path = "/asset/3";
  body = "<Part>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());
    ASSERT_EQ(3, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  m_agentTestHelper->m_path = "/asset/4";
  body = "<Part>TEST 4</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)4, m_agent->getAssetCount());
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
    ASSERT_EQ(4, m_agent->getAssetCount("Part"));
  }

  // Test multiple asset get
  m_agentTestHelper->m_path = "/assets";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
  }

  // Test multiple asset get with filter
  m_agentTestHelper->m_path = "/assets";
  {
    PARSE_XML_RESPONSE_QUERY(queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
  }

  queries["count"] = "2";
  {
    PARSE_XML_RESPONSE_QUERY(queries);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
  }

  queries.erase("count");

  m_agentTestHelper->m_path = "/asset/5";
  body = "<Part>TEST 5</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)4, m_agent->getAssetCount());
    ASSERT_EQ(4, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
  }

  m_agentTestHelper->m_path = "/asset/1";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 1");
  }

  m_agentTestHelper->m_path = "/asset/3";
  body = "<Part>TEST 6</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)4, m_agent->getAssetCount());
    ASSERT_EQ(4, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 6");
  }

  m_agentTestHelper->m_path = "/asset/2";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  m_agentTestHelper->m_path = "/asset/2";
  body = "<Part>TEST 7</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)4, m_agent->getAssetCount());
    ASSERT_EQ(4, m_agent->getAssetCount("Part"));
  }

  m_agentTestHelper->m_path = "/asset/6";
  body = "<Part>TEST 8</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)4, m_agent->getAssetCount());
    ASSERT_EQ(4, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 8");
  }

  // Now since two and three have been modified, asset 4 should be removed.
  m_agentTestHelper->m_path = "/asset/4";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 4");
  }
}

TEST_F(AgentTest, AssetError)
{
  m_agentTestHelper->m_path = "/asset/123";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 123");
  }
}

TEST_F(AgentTest, AdapterAddAsset)
{
  addAdapter();

  m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/asset/111";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }
}

TEST_F(AgentTest, MultiLineAsset)
{
  addAdapter();

  m_adapter->parseBuffer("TIME|@ASSET@|111|Part|--multiline--AAAA\n");
  m_adapter->parseBuffer(
      "<Part>\n"
      "  <PartXXX>TEST 1</PartXXX>\n"
      "  Some Text\n"
      "  <Extra>XXX</Extra>\n");
  m_adapter->parseBuffer(
      "</Part>\n"
      "--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/asset/111";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "TIME");
  }

  // Make sure we can still add a line and we are out of multiline mode...
  m_agentTestHelper->m_path = "/current";
  m_adapter->processData("TIME|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "204");
  }
}

TEST_F(AgentTest, AssetRefCounts)
{
  addAdapter();

  const auto assets = m_agent->getAssets();

  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.0738Z|@ASSET@|M8010N9172N:1.0|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:1.0"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">1</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">1</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.0</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="0">0</FunctionalLength><CuttingDiameterMax code="DC" nominal="0">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  // Asset has two secondary indexes
  AssetPtr first(*(assets->front()));
  ASSERT_EQ((unsigned int)4, first.getObject()->refCount());

  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.1749Z|@ASSET@|M8010N9172N:1.2|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="1" removed="False" assetId="M8010N9172N:1.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.2</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="649640">649640</FunctionalLength><CuttingDiameterMax code="DC" nominal="-177708">100</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());
  ASSERT_EQ((unsigned int)2, first.getObject()->refCount());

  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.2760Z|@ASSET@|M8010N9172N:1.0|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:1.0"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.0</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="0">0</FunctionalLength><CuttingDiameterMax code="DC" nominal="0">0</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.3771Z|@ASSET@|M8010N9172N:2.5|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:2.5"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">11</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">4</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">2</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>2.5</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="615207">615207</FunctionalLength><CuttingDiameterMax code="DC" nominal="-174546">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.4782Z|@ASSET@|M8010N9172N:2.2|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:2.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">11</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">4</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">2</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>2.2</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="615207">615207</FunctionalLength><CuttingDiameterMax code="DC" nominal="174546">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  // First asset should now be removed (we are holding the one ref)
  ASSERT_EQ((unsigned int)1, first.getObject()->refCount());

  // Check next asset
  AssetPtr second(*(assets->front()));
  ASSERT_EQ((unsigned int)2, second.getObject()->refCount());
  ASSERT_EQ(string("M8010N9172N:1.2"), second.getObject()->getAssetId());

  // Update the asset
  m_adapter->parseBuffer(
      R"ASSET(2018-02-19T22:54:03.1749Z|@ASSET@|M8010N9172N:1.2|CuttingTool|--multiline--SMOOTH
<CuttingTool toolId="0" serialNumber="1" removed="False" assetId="M8010N9172N:1.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.2</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="649640">649640</FunctionalLength><CuttingDiameterMax code="DC" nominal="-177708">100</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
--multiline--SMOOTH
)ASSET");

  // should be deleted
  ASSERT_EQ((unsigned int)1, second.getObject()->refCount());
}

TEST_F(AgentTest, BadAsset)
{
  addAdapter();

  m_adapter->parseBuffer("TIME|@ASSET@|111|CuttingTool|--multiline--AAAA\n");
  m_adapter->parseBuffer((getFile("asset4.xml") + "\n").c_str());
  m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());
}

TEST_F(AgentTest, AssetProbe)
{
  m_agent->enablePut();
  m_agentTestHelper->m_path = "/asset/1";
  string body = "<Part>TEST 1</Part>";
  key_value_map queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  m_agentTestHelper->m_path = "/asset/1";
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
  }
  m_agentTestHelper->m_path = "/asset/2";
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());
  }

  {
    m_agentTestHelper->m_path = "/probe";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
  }
}

TEST_F(AgentTest, AssetRemoval)
{
  m_agent->enablePut();
  m_agentTestHelper->m_path = "/asset/1";
  string body = "<Part>TEST 1</Part>";
  key_value_map queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
    ASSERT_EQ(1, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());
    ASSERT_EQ(1, m_agent->getAssetCount("Part"));
  }

  m_agentTestHelper->m_path = "/asset/2";
  body = "<Part>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());
    ASSERT_EQ(2, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  m_agentTestHelper->m_path = "/asset/3";
  body = "<Part>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());
    ASSERT_EQ(3, m_agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  m_agentTestHelper->m_path = "/asset/2";
  body = "<Part removed='true'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());
    ASSERT_EQ(3, m_agent->getAssetCount("Part"));
  }

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  m_agentTestHelper->m_path = "/assets";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  m_agentTestHelper->m_queries["removed"] = "true";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]@removed", "true");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
  }
}

TEST_F(AgentTest, AssetRemovalByAdapter)
{
  addAdapter();

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());

  m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_adapter->processData("TIME|@ASSET@|112|Part|<Part>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());

  m_adapter->processData("TIME|@ASSET@|113|Part|<Part>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "113");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_adapter->processData("TIME|@REMOVE_ASSET@|112\r");
  ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "112");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  m_agentTestHelper->m_path = "/assets";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  m_agentTestHelper->m_queries["removed"] = "true";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentTest, AssetStorageWithoutType)
{
  m_agent->enablePut();
  m_agentTestHelper->m_path = "/asset/123";
  string body = "<Part>TEST</Part>";
  key_value_map queries;

  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_EQ((unsigned int)0, m_agent->getAssetCount());
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetChanged12)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/min_config.xml", 8, 4, "1.2", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  {
    m_agentTestHelper->m_path = "/LinuxCNC/probe";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 0);
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetRemoved13)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/min_config.xml", 8, 4, "1.3", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  {
    m_agentTestHelper->m_path = "/LinuxCNC/probe";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetRemoved15)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/min_config.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  {
    m_agentTestHelper->m_path = "/LinuxCNC/probe";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", "true");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentTest, AssetPrependId)
{
  addAdapter();

  m_adapter->processData("TIME|@ASSET@|@1|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/asset/0001";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "0001");
  }
}

TEST_F(AgentTest, AssetWithSimpleCuttingItems)
{
  XmlPrinter *printer = dynamic_cast<XmlPrinter *>(m_agent->getPrinter("xml"));
  ASSERT_TRUE(printer != nullptr);

  printer->clearAssetsNamespaces();
  printer->addAssetsNamespace("urn:machine.com:MachineAssets:1.3",
                              "http://www.machine.com/schemas/MachineAssets_1.3.xsd", "x");

  addAdapter();

  m_adapter->parseBuffer("TIME|@ASSET@|XXX.200|CuttingTool|--multiline--AAAA\n");
  m_adapter->parseBuffer((getFile("asset5.xml") + "\n").c_str());
  m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/asset/XXX.200";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@type", "PART_COUNT");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@countDirection", "UP");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@initial", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@limit", "0");

    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/x:ItemCutterStatus/m:Status",
                          "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='2']/x:ItemCutterStatus/m:Status", "USED");

    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@type", "PART_COUNT");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@countDirection", "UP");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@initial", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@limit", "0");
  }

  printer->clearAssetsNamespaces();
}

TEST_F(AgentTest, RemoveLastAssetChanged)
{
  addAdapter();

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());

  m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_adapter->processData("TIME|@REMOVE_ASSET@|111");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentTest, RemoveAssetUsingHttpDelete)
{
  addAdapter();
  m_agent->enablePut();


  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());

  m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_path = "/asset/111";
  {
    PARSE_XML_RESPONSE_DELETE;
  }
  
  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }
}


TEST_F(AgentTest, AssetChangedWhenUnavailable)
{
  addAdapter();

  {
    m_agentTestHelper->m_path = "/current";
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "");
  }
}

TEST_F(AgentTest, RemoveAllAssets)
{
  addAdapter();

  ASSERT_EQ((unsigned int)4, m_agent->getMaxAssets());

  m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, m_agent->getAssetCount());

  m_adapter->processData("TIME|@ASSET@|112|Part|<Part>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, m_agent->getAssetCount());

  m_adapter->processData("TIME|@ASSET@|113|Part|<Part>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "113");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_adapter->processData("TIME|@REMOVE_ALL_ASSETS@|Part");
  ASSERT_EQ((unsigned int)3, m_agent->getAssetCount());

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "111");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_path = "/assets";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 0);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  m_agentTestHelper->m_queries["removed"] = "true";
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentTest, Put)
{
  key_value_map queries;
  string body;
  m_agent->enablePut();

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";
  m_agentTestHelper->m_path = "/LinuxCNC";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
  }

  m_agentTestHelper->m_path = "/LinuxCNC/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line@timestamp", "TIME");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:PowerState", "ON");
  }
}

// Test diabling of HTTP PUT or POST
TEST_F(AgentTest, PutBlocking)
{
  key_value_map queries;
  string body;

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";
  m_agentTestHelper->m_path = "/LinuxCNC";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Only the HTTP GET request is supported");
  }
}

// Test diabling of HTTP PUT or POST
TEST_F(AgentTest, PutBlockingFrom)
{
  key_value_map queries;
  string body;
  m_agent->enablePut();

  m_agent->allowPutFrom("192.168.0.1");

  queries["time"] = "TIME";
  queries["line"] = "205";
  m_agentTestHelper->m_path = "/LinuxCNC";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "HTTP PUT, POST, and DELETE are not allowed from 127.0.0.1");
  }

  m_agentTestHelper->m_path = "/LinuxCNC/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "UNAVAILABLE");
  }

  // Retry request after adding ip address
  m_agentTestHelper->m_path = "/LinuxCNC";
  m_agent->allowPutFrom("127.0.0.1");

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
  }

  m_agentTestHelper->m_path = "/LinuxCNC/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
  }
}

TEST_F(AgentTest, StreamData)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  auto heartbeatFreq{200ms};

  // Start a thread...
  key_value_map query;
  query["interval"] = "50";
  query["heartbeat"] = to_string(heartbeatFreq.count());
  query["from"] = int64ToString(m_agent->getSequence());
  m_agentTestHelper->m_path = "/LinuxCNC/sample";

  // Heartbeat test. Heartbeat should be sent in 200ms. Give
  // 25ms range.
  {
    auto startTime = system_clock::now();

    auto killThreadLambda = [](AgentTest *test) {
      this_thread::sleep_for(test->m_delay);
      test->m_agentTestHelper->m_out.setstate(ios::eofbit);
    };

    m_delay = 20ms;
    auto killThread = std::thread{killThreadLambda, this};
    try
    {
      PARSE_XML_RESPONSE_QUERY(query);
      ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);

      auto delta = system_clock::now() - startTime;
      ASSERT_TRUE(delta < (heartbeatFreq + 25ms));
      ASSERT_TRUE(delta > heartbeatFreq);
      killThread.join();
    }
    catch (...)
    {
      killThread.join();
      throw;
    }
  }

  m_agentTestHelper->m_out.clear();
  m_agentTestHelper->m_out.str("");

  // Set some data and make sure we get data within 40ms.
  // Again, allow for some slop.
  auto minExpectedResponse{40ms};
  {
    auto startTime = system_clock::now();

    auto AddThreadLambda = [](AgentTest *test) {
      this_thread::sleep_for(test->m_delay);
      test->m_adapter->processData("TIME|line|204");
      test->m_agentTestHelper->m_out.setstate(ios::eofbit);
    };

    m_delay = 10ms;
    auto addThread = std::thread{AddThreadLambda, this};
    try
    {
      PARSE_XML_RESPONSE_QUERY(query);

      auto delta = system_clock::now() - startTime;
      ASSERT_TRUE(delta < (minExpectedResponse + 30ms));
      ASSERT_TRUE(delta > minExpectedResponse);
      addThread.join();
    }
    catch (...)
    {
      addThread.join();
      throw;
    }
  }
}

TEST_F(AgentTest, FailWithDuplicateDeviceUUID)
{
  ASSERT_THROW(new Agent(PROJECT_ROOT_DIR "/samples/dup_uuid.xml", 8, 4, "1.5", 25),
               std::runtime_error);
}

TEST_F(AgentTest, StreamDataObserver)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Start a thread...
  key_value_map query;
  query["interval"] = "100";
  query["heartbeat"] = "1000";
  query["count"] = "10";
  query["from"] = int64ToString(m_agent->getSequence());
  m_agentTestHelper->m_path = "/LinuxCNC/sample";

  // Test to make sure the signal will push the sequence number forward and capture
  // the new data.
  {
    auto streamThreadLambda = [](AgentTest *test) {
      this_thread::sleep_for(test->m_delay);
      test->m_agent->setSequence(test->m_agent->getSequence() + 20ull);
      test->m_adapter->processData("TIME|line|204");
      this_thread::sleep_for(120ms);
      test->m_agentTestHelper->m_out.setstate(ios::eofbit);
    };

    m_delay = 50ms;
    auto seq = int64ToString(m_agent->getSequence() + 20ull);

    auto streamThread = std::thread{streamThreadLambda, this};
    try
    {
      PARSE_XML_RESPONSE_QUERY(query);
      ASSERT_XML_PATH_EQUAL(doc, "//m:Line@sequence", seq.c_str());
      streamThread.join();
    }
    catch (...)
    {
      streamThread.join();
      throw;
    }
  }
}

TEST_F(AgentTest, RelativeTime)
{
  {
    m_agentTestHelper->m_path = "/sample";

    m_adapter = new Adapter("LinuxCNC", "server", 7878);
    m_agent->addAdapter(m_adapter);
    ASSERT_TRUE(m_adapter);

    m_adapter->setRelativeTime(true);
    m_adapter->setBaseOffset(1000);
    m_adapter->setBaseTime(1353414802123456LL);  // 2012-11-20 12:33:22.123456 UTC

    // Add a 10.654321 seconds
    m_adapter->processData("10654|line|204");

    {
      PARSE_XML_RESPONSE;
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp",
                            "2012-11-20T12:33:32.776456Z");
    }
  }
}

TEST_F(AgentTest, RelativeParsedTime)
{
  {
    m_agentTestHelper->m_path = "/sample";

    m_adapter = new Adapter("LinuxCNC", "server", 7878);
    m_agent->addAdapter(m_adapter);
    ASSERT_TRUE(m_adapter);

    m_adapter->setRelativeTime(true);
    m_adapter->setParseTime(true);
    m_adapter->setBaseOffset(1354165286555666);  // 2012-11-29 05:01:26.555666 UTC
    m_adapter->setBaseTime(1353414802123456);    // 2012-11-20 12:33:22.123456 UTC

    // Add a 10.111000 seconds
    m_adapter->processData("2012-11-29T05:01:36.666666|line|100");

    {
      PARSE_XML_RESPONSE;
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp",
                            "2012-11-20T12:33:32.234456Z");
    }
  }
}

TEST_F(AgentTest, RelativeParsedTimeDetection)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->setRelativeTime(true);

  // Add a 10.111000 seconds
  m_adapter->processData("2012-11-29T05:01:26.555666|line|100");

  ASSERT_TRUE(m_adapter->isParsingTime());
  ASSERT_EQ((uint64_t)1354165286555666LL, m_adapter->getBaseOffset());
}

TEST_F(AgentTest, RelativeOffsetDetection)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->setRelativeTime(true);

  // Add a 10.111000 seconds
  m_adapter->processData("1234556|line|100");

  ASSERT_FALSE(m_adapter->isParsingTime());
  ASSERT_EQ((uint64_t)1234556000LL, m_adapter->getBaseOffset());
}

TEST_F(AgentTest, DynamicCalibration)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  // Add a 10.111000 seconds
  m_adapter->protocolCommand("* calibration:Yact|.01|200.0|Zact|0.02|300|Xts|0.01|500");
  auto di = m_agent->getDataItemByName("LinuxCNC", "Yact");
  ASSERT_TRUE(di);

  ASSERT_TRUE(di->hasFactor());
  ASSERT_EQ(0.01, di->getConversionFactor());
  ASSERT_EQ(200.0, di->getConversionOffset());

  di = m_agent->getDataItemByName("LinuxCNC", "Zact");
  ASSERT_TRUE(di);

  ASSERT_TRUE(di->hasFactor());
  ASSERT_EQ(0.02, di->getConversionFactor());
  ASSERT_EQ(300.0, di->getConversionOffset());

  m_adapter->processData("TIME|Yact|200|Zact|600");
  m_adapter->processData(
      "TIME|Xts|25|| 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5119 5119 5118 "
      "5118 5117 5117 5119 5119 5118 5118 5118 5118 5118");

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='y1']", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='z1']", "18");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']",
        "56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.19 56.19 56.18 "
        "56.18 56.17 56.17 56.19 56.19 56.18 56.18 56.18 56.18 56.18");
  }
}

TEST_F(AgentTest, InitialTimeSeriesValues)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']",
                          "UNAVAILABLE");
  }
}

TEST_F(AgentTest, FilterValues13)
{
  m_agent.reset();
  m_agent =
      make_unique<Agent>(PROJECT_ROOT_DIR "/samples/filter_example_1.3.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|load|100");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
  }

  m_adapter->processData("TIME|load|103");
  m_adapter->processData("TIME|load|106");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
  }

  m_adapter->processData("TIME|load|106|load|108|load|112");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
  }

  auto item = m_agent->getDataItemByName((string) "LinuxCNC", "pos");
  ASSERT_TRUE(item);
  ASSERT_TRUE(item->hasMinimumDelta());

  ASSERT_FALSE(item->isFiltered(0.0, NAN));
  ASSERT_TRUE(item->isFiltered(5.0, NAN));
  ASSERT_FALSE(item->isFiltered(20.0, NAN));
}

TEST_F(AgentTest, FilterValues)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/filter_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_adapter->processData("2018-04-27T05:00:26.555666|load|100|pos|20");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_adapter->processData("2018-04-27T05:00:32.000666|load|103|pos|25");
  m_adapter->processData("2018-04-27T05:00:36.888666|load|106|pos|30");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  m_adapter->processData("2018-04-27T05:00:40.25|load|106|load|108|load|112|pos|35|pos|40");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  m_adapter->processData("2018-04-27T05:00:47.50|pos|45|pos|50");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[4]", "45");
  }

  // Test period filter with ignore timestamps
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/filter_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_adapter->setIgnoreTimestamps(true);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_adapter->processData("2018-04-27T05:00:26.555666|load|100|pos|20");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_adapter->processData("2018-04-27T05:01:32.000666|load|103|pos|25");
  dlib::sleep(11 * 1000);
  m_adapter->processData("2018-04-27T05:01:40.888666|load|106|pos|30");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  // Test period filter with relative time
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/filter_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  m_adapter->setRelativeTime(true);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_adapter->processData("0|load|100|pos|20");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_adapter->processData("5000|load|103|pos|25");
  m_adapter->processData("11000|load|106|pos|30");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  DataItem *item = m_agent->getDataItemByName((string) "LinuxCNC", "load");
  ASSERT_TRUE(item);
  ASSERT_TRUE(item->hasMinimumDelta());

  ASSERT_FALSE(item->isFiltered(0.0, NAN));
  ASSERT_TRUE(item->isFiltered(4.0, NAN));
  ASSERT_FALSE(item->isFiltered(20.0, NAN));
}

TEST_F(AgentTest, ResetTriggered)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/sample";

  m_adapter->processData("TIME1|pcount|0");
  m_adapter->processData("TIME2|pcount|1");
  m_adapter->processData("TIME3|pcount|2");
  m_adapter->processData("TIME4|pcount|0:DAY");
  m_adapter->processData("TIME3|pcount|5");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[2]", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[3]", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[3]@resetTriggered", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[4]", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[5]", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[5]@resetTriggered", "DAY");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[6]", "5");
  }
}

TEST_F(AgentTest, References)
{
  m_agent.reset();
  m_agent =
      make_unique<Agent>(PROJECT_ROOT_DIR "/samples/reference_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  string id = "mf";
  auto item = m_agent->getDataItemByName((string) "LinuxCNC", id);
  auto comp = item->getComponent();

  const auto refs = comp->getReferences();
  const auto ref = refs[0];

  ASSERT_EQ((string) "c4", ref.m_id);
  ASSERT_EQ((string) "chuck", ref.m_name);
  ASSERT_EQ(mtconnect::Component::Reference::DATA_ITEM, ref.m_type);

  ASSERT_TRUE(ref.m_dataItem) << "DataItem was not resolved";

  const mtconnect::Component::Reference &ref2 = refs[1];
  ASSERT_EQ((string) "d2", ref2.m_id);
  ASSERT_EQ((string) "door", ref2.m_name);
  ASSERT_EQ(mtconnect::Component::Reference::DATA_ITEM, ref2.m_type);

  const mtconnect::Component::Reference &ref3 = refs[2];
  ASSERT_EQ((string) "ele", ref3.m_id);
  ASSERT_EQ((string) "electric", ref3.m_name);
  ASSERT_EQ(mtconnect::Component::Reference::COMPONENT, ref3.m_type);

  ASSERT_TRUE(ref3.m_component) << "DataItem was not resolved";

  m_agentTestHelper->m_path = "/current";
  key_value_map query;
  query["path"] = "//BarFeederInterface";

  // Additional data items should be included
  {
    PARSE_XML_RESPONSE_QUERY(query);

    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@component='BarFeederInterface']//m:MaterialFeed", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Door']//m:DoorState",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:ChuckState",
                          "UNAVAILABLE");
  }
}

TEST_F(AgentTest, Discrete)
{
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/discrete_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  m_adapter->setDupCheck(true);
  ASSERT_TRUE(m_adapter);

  auto msg = m_agent->getDataItemByName("LinuxCNC", "message");
  ASSERT_TRUE(msg);
  ASSERT_EQ(true, msg->isDiscreteRep());

  // Validate we are dup checking.
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|line|204");
  m_adapter->processData("TIME|line|204");
  m_adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|message|Hi|Hello");
  m_adapter->processData("TIME|message|Hi|Hello");
  m_adapter->processData("TIME|message|Hi|Hello");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[2]", "Hello");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[3]", "Hello");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[4]", "Hello");
  }
}

TEST_F(AgentTest, UpcaseValues)
{
  m_agentTestHelper->m_path = "/current";
  m_agent.reset();
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/discrete_example.xml", 8, 4, "1.5", 25);
  m_agentTestHelper->m_agent = m_agent.get();

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_adapter->setDupCheck(true);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);
  ASSERT_TRUE(m_adapter->upcaseValue());

  m_adapter->processData("TIME|mode|Hello");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "HELLO");
  }

  m_adapter->setUpcaseValue(false);
  m_adapter->processData("TIME|mode|Hello");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "Hello");
  }
}

TEST_F(AgentTest, ConditionSequence)
{
  m_agentTestHelper->m_path = "/current";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_adapter->setDupCheck(true);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  auto logic = m_agent->getDataItemByName("LinuxCNC", "lp");
  ASSERT_TRUE(logic);

  // Validate we are dup checking.
  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Unavailable[@dataItemId='lp']",
                          1);
  }

  m_adapter->processData("TIME|lp|NORMAL||||XXX");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        "XXX");
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
  }

  m_adapter->processData(
      "TIME|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault",
        "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault@nativeCode",
                          "2218");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault@nativeSeverity",
                          "ALARM_B");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault@qualifier",
                          "HIGH");
  }

  m_adapter->processData("TIME|lp|NORMAL||||");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        1);
  }

  m_adapter->processData(
      "TIME|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault",
        "4200 ALARM_D Power on effective parameter set");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault@nativeCode",
                          "4200");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault@nativeSeverity",
                          "ALARM_D");
  }

  m_adapter->processData(
      "TIME|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 2);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]",
        "4200 ALARM_D Power on effective parameter set");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]",
        "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[2]@nativeCode",
                          "2218");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[2]@nativeSeverity",
                          "ALARM_B");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[2]@qualifier",
                          "HIGH");
  }

  m_adapter->processData(
      "TIME|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 2);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]",
        "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[1]@nativeCode",
                          "2218");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[1]@nativeSeverity",
                          "ALARM_B");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[1]@qualifier",
                          "HIGH");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]",
        "4200 ALARM_D Power on effective parameter set");
  }

  m_adapter->processData("TIME|lp|NORMAL|2218|||");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Fault[1]@nativeCode",
                          "4200");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]",
        "4200 ALARM_D Power on effective parameter set");
  }

  m_adapter->processData("TIME|lp|NORMAL||||");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        1);
  }
}

TEST_F(AgentTest, EmptyLastItemFromAdapter)
{
  m_agentTestHelper->m_path = "/current";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_adapter->setDupCheck(true);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  auto program = m_agent->getDataItemByName("LinuxCNC", "program");
  ASSERT_TRUE(program);

  auto tool_id = m_agent->getDataItemByName("LinuxCNC", "block");
  ASSERT_TRUE(tool_id);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|program|A|block|B");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
  }

  m_adapter->processData("TIME|program||block|B");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
  }

  m_adapter->processData("TIME|program||block|");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }

  m_adapter->processData("TIME|program|A|block|B");
  m_adapter->processData("TIME|program|A|block|");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }

  m_adapter->processData("TIME|program|A|block|B|line|C");
  m_adapter->processData("TIME|program|D|block||line|E");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "D");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "E");
  }
}

TEST_F(AgentTest, ConstantValue)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  
  auto di = m_agent->getDataItemByName("LinuxCNC", "block");
  ASSERT_TRUE(di);
  di->addConstrainedValue("UNAVAILABLE");

  ASSERT_TRUE(m_adapter);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|block|G01X00|Smode|INDEX|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Block", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode[1]", "SPINDLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:RotaryMode", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }
}

TEST_F(AgentTest, BadDataItem)
{
  m_agentTestHelper->m_path = "/sample";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_adapter->processData("TIME|bad|ignore|dummy|1244|line|204");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }
}

TEST_F(AgentTest, Composition)
{
  m_agentTestHelper->m_path = "/current";

  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  m_adapter->setDupCheck(true);
  ASSERT_TRUE(m_adapter);

  DataItem *motor = m_agent->getDataItemByName("LinuxCNC", "zt1");
  ASSERT_TRUE(motor);

  DataItem *amp = m_agent->getDataItemByName("LinuxCNC", "zt2");
  ASSERT_TRUE(amp);

  m_adapter->processData("TIME|zt1|100|zt2|200");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']", "200");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']@compositionId",
                          "zmotor");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']@compositionId",
                          "zamp");
  }
}
