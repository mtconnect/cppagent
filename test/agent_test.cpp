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

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "test_globals.hpp"
#include "xml_printer.hpp"
#include "assets/file_asset.hpp"

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
using namespace mtconnect::http_server;
using namespace mtconnect::adapter;
using namespace mtconnect::observation;

class AgentTest : public testing::Test
{
 public:
  typedef dlib::map<std::string, std::string>::kernel_1a_c map_type;
  using queue_type = dlib::queue<std::string>::kernel_1a_c;

 protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml",
                                   8, 4, "1.3", 25);
    m_agentId = to_string(getCurrentTimeInSec());
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
  }

  void addAdapter(ConfigOptions options = ConfigOptions{})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878, m_agentTestHelper->m_agent->defaultDevice()->getName());
  }
  
 public:
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  std::chrono::milliseconds m_delay{};
};

TEST_F(AgentTest, Constructor)
{
  auto server1 = make_unique<http_server::Server>();
  auto cache1 = make_unique<http_server::FileCache>();
  unique_ptr<Agent> agent = make_unique<Agent>(server1, cache1, PROJECT_ROOT_DIR "/samples/badPath.xml", 17, 8, "1.7");
  auto context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();

  ASSERT_THROW(agent->initialize(context, {}), std::runtime_error);
  agent.reset();
  
  auto server2 = make_unique<http_server::Server>();
  auto cache2 = make_unique<http_server::FileCache>();
  agent = make_unique<Agent>(server2, cache2, PROJECT_ROOT_DIR "/samples/test_config.xml", 17, 8, "1.7");
  
  context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();
  ASSERT_NO_THROW(agent->initialize(context, {}));
}

TEST_F(AgentTest, Probe)
{
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }

  {
    PARSE_XML_RESPONSE("/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }
}

TEST_F(AgentTest, FailWithDuplicateDeviceUUID)
{
  auto server1 = make_unique<http_server::Server>();
  auto cache1 = make_unique<http_server::FileCache>();
  unique_ptr<Agent> agent = make_unique<Agent>(server1, cache1, PROJECT_ROOT_DIR "/samples/dup_uuid.xml", 17, 8, "1.5");
  auto context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();

  ASSERT_THROW(agent->initialize(context, {}), std::runtime_error);
}

TEST_F(AgentTest, BadDevices)
{
  {
    PARSE_XML_RESPONSE("/LinuxCN/probe");
    string message = (string) "Could not find the device 'LinuxCN'";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
    ASSERT_EQ(NOT_FOUND, m_agentTestHelper->m_response.m_code);
  }
}

TEST_F(AgentTest, BadXPath)
{
  {
    Routing::QueryMap query{{ "path", "//////Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    Routing::QueryMap query{{ "path", "//Axes?//Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    Routing::QueryMap query{{ "path", "//Devices/Device[@name=\"I_DON'T_EXIST\""}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string)
    "The path could not be parsed. Invalid syntax: //Devices/Device[@name=\"I_DON'T_EXIST\"";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

TEST_F(AgentTest, GoodPath)
{
  {
    Routing::QueryMap query{{ "path", "//Power"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:ComponentStream[@component='Power']//m:PowerState",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:ComponentStream", 1);
  }
  
  {
    Routing::QueryMap query{{ "path",
      "//Rotary[@name='C']//DataItem[@category='SAMPLE' or @category='CONDITION']"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:ComponentStream[@component='Rotary']//m:SpindleSpeed",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:ComponentStream[@component='Rotary']//m:Load",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:ComponentStream[@component='Rotary']//m:Unavailable",
                          "");
  }
}

TEST_F(AgentTest, BadPath)
{
  using namespace http_server;
  {
    PARSE_XML_RESPONSE("/bad_path");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Error processing request from:  - No matching route for: GET /bad_path");
    EXPECT_EQ(BAD_REQUEST, m_agentTestHelper->m_response.m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }

  {
    PARSE_XML_RESPONSE("/bad/path/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Error processing request from:  - No matching route for: GET /bad/path/");
    EXPECT_EQ(BAD_REQUEST, m_agentTestHelper->m_response.m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current/blah");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Error processing request from:  - No matching route for: GET /LinuxCNC/current/blah");
    EXPECT_EQ(BAD_REQUEST, m_agentTestHelper->m_response.m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
}

TEST_F(AgentTest, CurrentAt)
{
  Routing::QueryMap query;
  PARSE_XML_RESPONSE_QUERY("/current", query);

  auto agent = m_agentTestHelper->m_agent.get();
  
  addAdapter();

  // Get the current position
  int seq = agent->getSequence();
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 100; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 0; i < 100; i++)
  {
    query["at"] = to_string(i + seq);;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line",
                          to_string(i + 1).c_str());
  }

  // Test buffer wrapping
  // Add a large many events
  for (int i = 101; i <= 301; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 100; i < 301; i++)
  {
    query["at"] = to_string(i + seq);;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line",
                          to_string(i + 1).c_str());
  }

  // Check the first couple of items in the list
  for (int j = 0; j < 10; j++)
  {
    int i = agent->getSequence() - agent->getBufferSize() - seq + j;
    query["at"] = to_string(i + seq);;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line",
                          to_string(i + 1).c_str());

  }

  // Test out of range...
  {
    int i = agent->getSequence() - agent->getBufferSize() - seq - 1;
    sprintf(line, "'at' must be greater than %d", i + seq);
    query["at"] = to_string(i + seq);;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, CurrentAt64)
{
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;

  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Initialize the sliding buffer at a very large number.
  uint64_t start = (((uint64_t)1) << 48) + 1317;
  agent->setSequence(start);

  // Add many events
  for (int i = 1; i <= 500; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  // Check each current at all the positions.
  for (uint64_t i = start + 300; i < start + 500; i++)
  {
    query["at"] = to_string(i);
    sprintf(line, "%d", (int)(i - start) + 1);
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }
}

TEST_F(AgentTest, CurrentAtOutOfRange)
{
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;

  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 200; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  int seq = agent->getSequence();

  {
    query["at"] = to_string(seq);
    sprintf(line, "'at' must be less than %d", seq);
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }

  seq = agent->getFirstSequence() - 1;

  {
    query["at"] = to_string(seq);
    sprintf(line, "'at' must be greater than %d", seq);
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, AddAdapter)
{
  addAdapter();
}

TEST_F(AgentTest, FileDownload)
{
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;

  string uri("/schemas/MTConnectDevices_1.1.xsd");

  // Register a file with the agent.
  agent->getFileCache()->registerFile(uri, PROJECT_ROOT_DIR "/schemas/MTConnectDevices_1.1.xsd", "1.1");

  // Reqyest the file...
  PARSE_TEXT_RESPONSE(uri.c_str());
  ASSERT_FALSE(m_agentTestHelper->m_response.m_body.empty());
  ASSERT_TRUE(m_agentTestHelper->m_response.m_body.find_last_of("TEST SCHEMA FILE 1234567890\n") != string::npos);
}

TEST_F(AgentTest, FailedFileDownload)
{
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;

  string uri("/schemas/MTConnectDevices_1.1.xsd");

  // Register a file with the agent.
  agent->getFileCache()->registerFile(uri, "./BadFileName.xsd", "1.1");

  {
    PARSE_XML_RESPONSE(uri.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          ("Error processing request from:  - No matching route for: GET " + uri).c_str());
  }
}

TEST_F(AgentTest, Composition)
{
  auto agent = m_agentTestHelper->m_agent.get();
  addAdapter();

  DataItem *motor = agent->getDataItemByName("LinuxCNC", "zt1");
  ASSERT_TRUE(motor);

  DataItem *amp = agent->getDataItemByName("LinuxCNC", "zt2");
  ASSERT_TRUE(amp);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|zt1|100|zt2|200");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']",
                          "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']",
                          "200");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']@compositionId",
                          "zmotor");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']@compositionId",
                          "zamp");
  }
}

TEST_F(AgentTest, BadCount)
{
  int size = m_agentTestHelper->m_agent->getBufferSize() + 1;
  {
    Routing::QueryMap query{{"count", "NON_INTEGER"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Parameter Error processing request from:  - for query parameter 'count': cannot convert string 'NON_INTEGER' to integer");
  }

  {
    Routing::QueryMap query{{"count", "-500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than ");
    value += to_string(-size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    Routing::QueryMap query{{"count", "0"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must not be zero(0)");
  }

  {
    Routing::QueryMap query{{"count", "500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than ");
    value += to_string(size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    Routing::QueryMap query{{"count", "9999999"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than ");
    value += to_string(size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }
  
  {
    Routing::QueryMap query{{"count", "-9999999"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than ");
    value += to_string(-size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

}

TEST_F(AgentTest, Adapter)
{
  addAdapter();

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|alarm|code|nativeCode|severity|state|description");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[2]", "description");
  }
}

TEST_F(AgentTest, SampleAtNextSeq)
{
  Routing::QueryMap query;
  addAdapter();
  auto agent = m_agentTestHelper->m_agent.get();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 300; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  int seq = agent->getSequence();
  {
    query["from"] = to_string(seq);
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, SampleCount)
{
  Routing::QueryMap query;
  addAdapter();
  auto agent = m_agentTestHelper->m_agent.get();

  auto seq = agent->getSequence();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d|Xact|%d", i, i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  {
    query["path"] = "//DataItem[@name='Xact']";
    query["from"] = to_string(seq);
    query["count"] = "10";

    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + 20).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

    // Make sure we got 10 lines
    for (int j = 0; j < 10; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, to_string(seq + j * 2 + 1).c_str());
    }
  }
}

TEST_F(AgentTest, SampleLastCount)
{
  Routing::QueryMap query;
  addAdapter();
  auto agent = m_agentTestHelper->m_agent.get();


  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d|Xact|%d", i, i);
    m_agentTestHelper->m_adapter->processData(line);
  }
  
  auto seq = agent->getSequence() - 20;

  {
    query["path"] = "//DataItem[@name='Xact']";
    query["count"] = "-10";

    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

    // Make sure we got 10 lines
    for (int j = 0; j < 10; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, to_string(seq + j * 2 + 1).c_str());
    }
  }
}


TEST_F(AgentTest, EmptyStream)
{
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']@name", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Unavailable",
                          nullptr);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Unavailable@qualifier",
        nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode", "SPINDLE");
  }

  {
    auto agent = m_agentTestHelper->m_agent.get();
    Routing::QueryMap query{{"from", to_string(agent->getSequence())}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, AddToBuffer)
{
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;

  string device("LinuxCNC"), key("badKey"), value("ON");
  auto seqNum = 0;
  auto event1 = agent->getFromBuffer(seqNum);
  ASSERT_FALSE(event1);

  {
    query["from"] = to_string(agent->getSequence());
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }

  key = "power";

  auto di2 = agent->getDataItemByName(device, key);
  seqNum = m_agentTestHelper->addToBuffer(di2, {{"VALUE", value}}, chrono::system_clock::now());
  auto event2 = agent->getFromBuffer(seqNum);
  ASSERT_EQ(3, event2.use_count());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "ON");
  }

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[2]", "ON");
  }
}


TEST_F(AgentTest, SequenceNumberRollover)
{
#ifndef WIN32
  auto agent = m_agentTestHelper->m_agent.get();
  Routing::QueryMap query;
  addAdapter();

  // Set the sequence number near MAX_UINT32
  agent->setSequence(0xFFFFFFA0);
  SequenceNumber_t seq = agent->getSequence();
  ASSERT_EQ((int64_t)0xFFFFFFA0, seq);

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);

    {
      PARSE_XML_RESPONSE("/current");
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
                            to_string(seq + i).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
                            to_string(seq + i + 1).c_str());
    }

    {
      query["from"] = to_string(seq);
      query["count"] = "128";

      PARSE_XML_RESPONSE_QUERY("/sample", query);
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
                            to_string(seq + i + 1).c_str());

      for (int j = 0; j <= i; j++)
      {
        sprintf(line, "//m:DeviceStream//m:Line[%d]@sequence", j + 1);
        ASSERT_XML_PATH_EQUAL(doc, line, to_string(seq + j).c_str());
      }
    }

    for (int j = 0; j <= i; j++)
    {
      query["from"] = to_string(seq + j);
      query["count"] = "1";

      PARSE_XML_RESPONSE_QUERY("/sample", query);
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
                            to_string(seq + j).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + j + 1).c_str());
    }
  }

  ASSERT_EQ(uint64_t(0xFFFFFFA0) + 128ul, agent->getSequence());
#endif
}

TEST_F(AgentTest, DuplicateCheck)
{
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
  }
}

TEST_F(AgentTest, DuplicateCheckAfterDisconnect)
{
  addAdapter({{"FilterDuplicates", true}});

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
  }

  m_agentTestHelper->m_adapter->disconnected();

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->connected();

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[5]", "205");
  }
}

TEST_F(AgentTest, AutoAvailable)
{
  addAdapter({{"AutoAvailable", true}});
  auto agent = m_agentTestHelper->m_agent.get();
  auto adapter = m_agentTestHelper->m_adapter;
  auto id = adapter->getIdentity();
  auto d = agent->getDevices().front();
  StringList devices;
  devices.emplace_back(d->getName());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
  }

  agent->connected(id, devices, true);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
  }

  agent->disconnected(id, devices, true);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
  }

  agent->connected(id, devices, true);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[4]", "AVAILABLE");
  }
}

TEST_F(AgentTest, MultipleDisconnect)
{
  addAdapter();
  auto agent = m_agentTestHelper->m_agent.get();
  auto adapter = m_agentTestHelper->m_adapter;
  auto id = adapter->getIdentity();

  auto d = agent->getDevices().front();
  StringList devices;
  devices.emplace_back(d->getName());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
  }

  agent->connected(id, devices, false);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|block|GTH");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|cmp|normal||||");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 2);

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);
  }

  agent->disconnected(id, devices, false);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
  }

  agent->disconnected(id, devices, false);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
  }

  agent->connected(id, devices, false);
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|block|GTH");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|cmp|normal||||");

  agent->disconnected(id, devices, false);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/sample");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 3);
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 2);

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 5);
  }
}

TEST_F(AgentTest, IgnoreTimestamps)
{
  addAdapter();

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2021-02-01T12:00:00.000000Z");
  }

  m_agentTestHelper->m_adapter->setOptions({{"IgnoreTimestamps", true}});
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2021-02-01T12:00:00.000000Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]@timestamp", "!2021-02-01T12:00:00.000000Z");
  }
}

TEST_F(AgentTest, InitialTimeSeriesValues)
{
  addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']",
                          "UNAVAILABLE");
  }
}

TEST_F(AgentTest, DynamicCalibration)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  
  // Add a 10.111000 seconds
  m_agentTestHelper->m_adapter->protocolCommand("* calibration:Yact|.01|200.0|Zact|0.02|300|Xts|0.01|500");
  auto di = agent->getDataItemByName("LinuxCNC", "Yact");
  ASSERT_TRUE(di);

  ASSERT_TRUE(di->hasFactor());
  ASSERT_EQ(0.01, di->getConversionFactor());
  ASSERT_EQ(200.0, di->getConversionOffset());

  di = agent->getDataItemByName("LinuxCNC", "Zact");
  ASSERT_TRUE(di);

  ASSERT_TRUE(di->hasFactor());
  ASSERT_EQ(0.02, di->getConversionFactor());
  ASSERT_EQ(300.0, di->getConversionOffset());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|Yact|200|Zact|600");
  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|Xts|25|| 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5119 5119 5118 "
      "5118 5117 5117 5119 5119 5118 5118 5118 5118 5118");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='y1']", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='z1']", "18");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']",
        "56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.19 56.19 56.18 "
        "56.18 56.17 56.17 56.19 56.19 56.18 56.18 56.18 56.18 56.18");
  }
}

TEST_F(AgentTest, FilterValues13)
{
  m_agentTestHelper->createAgent("/samples/filter_example_1.3.xml", 8, 4, "1.5", 25);
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|load|100");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|load|103");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|load|106");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|load|106|load|108|load|112");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
  }

#if 0
  auto item = m_agentTestHelper->getAgent()->getDataItemByName((string) "LinuxCNC", "pos");
  ASSERT_TRUE(item);
  ASSERT_TRUE(item->hasMinimumDelta());

  ASSERT_FALSE(item->isFiltered(0.0, NAN));
  ASSERT_TRUE(item->isFiltered(5.0, NAN));
  ASSERT_FALSE(item->isFiltered(20.0, NAN));
#endif
}

TEST_F(AgentTest, FilterValues)
{
  m_agentTestHelper->createAgent("/samples/filter_example_1.3.xml", 8, 4, "1.5", 25);
  addAdapter();

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:26.555666|load|100|pos|20");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:32.000666|load|103|pos|25");
  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:36.888666|load|106|pos|30");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:40.25|load|106|load|108|load|112|pos|35|pos|40");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:47.50|pos|45|pos|50");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[4]", "40");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[5]", "50");
  }
}

TEST_F(AgentTest, TestPeriodFilterWithIgnoreTimestamps)
{
  // Test period filter with ignore timestamps
  m_agentTestHelper->createAgent("/samples/filter_example_1.3.xml", 8, 4, "1.5", 25);
  addAdapter({{"IgnoreTimestamps", true}});

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:00:26.555666|load|100|pos|20");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_agentTestHelper->m_adapter->processData("2018-04-27T05:01:32.000666|load|103|pos|25");
  dlib::sleep(11 * 1000);
  m_agentTestHelper->m_adapter->processData("2018-04-27T05:01:40.888666|load|106|pos|30");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }
}

TEST_F(AgentTest, TestPeriodFilterWithRelativeTime)
{
  // Test period filter with relative time
  m_agentTestHelper->createAgent("/samples/filter_example_1.3.xml", 8, 4, "1.5", 25);
  addAdapter({{"RelativeTime", true}});

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("0|load|100|pos|20");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
  }

  m_agentTestHelper->m_adapter->processData("5000|load|103|pos|25");
  m_agentTestHelper->m_adapter->processData("11000|load|106|pos|30");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[2]", "20");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[3]", "30");
  }

#if 0
  DataItem *item = m_agentTestHelper->getAgent()->getDataItemByName((string) "LinuxCNC", "load");
  ASSERT_TRUE(item);
  ASSERT_TRUE(item->hasMinimumDelta());

  ASSERT_FALSE(item->isFiltered(0.0, NAN));
  ASSERT_TRUE(item->isFiltered(4.0, NAN));
  ASSERT_FALSE(item->isFiltered(20.0, NAN));
#endif
}

TEST_F(AgentTest, ResetTriggered)
{
  addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME1|pcount|0");
  m_agentTestHelper->m_adapter->processData("TIME2|pcount|1");
  m_agentTestHelper->m_adapter->processData("TIME3|pcount|2");
  m_agentTestHelper->m_adapter->processData("TIME4|pcount|0:DAY");
  m_agentTestHelper->m_adapter->processData("TIME3|pcount|5");

  {
    PARSE_XML_RESPONSE("/sample");
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
  m_agentTestHelper->createAgent("/samples/reference_example.xml");
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  string id = "mf";
  auto item = agent->getDataItemByName((string) "LinuxCNC", id);
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


  // Additional data items should be included
  {
    Routing::QueryMap query {{"path", "//BarFeederInterface"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);

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
  m_agentTestHelper->createAgent("/samples/discrete_example.xml");
  addAdapter({{"FilterDuplicates", true}});
  auto agent = m_agentTestHelper->getAgent();

  auto msg = agent->getDataItemByName("LinuxCNC", "message");
  ASSERT_TRUE(msg);
  ASSERT_EQ(true, msg->isDiscreteRep());

  // Validate we are dup checking.
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|message|Hi|Hello");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|message|Hi|Hello");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|message|Hi|Hello");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[2]", "Hello");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[3]", "Hello");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[4]", "Hello");
  }
}

// ------------------------------

TEST_F(AgentTest, UpcaseValues)
{
  addAdapter({{"FilterDuplicates", true}, {"UpcaseDataItemValue", true}});

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|mode|Hello");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "HELLO");
  }

  m_agentTestHelper->m_adapter->setOptions({{"UpcaseDataItemValue", false}});
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|mode|Hello");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "Hello");
  }
}

TEST_F(AgentTest, ConditionSequence)
{
  addAdapter({{"FilterDuplicates", true}});
  auto agent = m_agentTestHelper->getAgent();
  auto logic = agent->getDataItemByName("LinuxCNC", "lp");
  ASSERT_TRUE(logic);

  // Validate we are dup checking.
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_COUNT(doc,
                          "//m:DeviceStream//"
                          "m:ComponentStream[@component='Controller']/m:Condition/"
                          "m:Unavailable[@dataItemId='lp']",
                          1);
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|lp|NORMAL||||XXX");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        "XXX");
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|lp|NORMAL||||");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        1);
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|lp|NORMAL|2218|||");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|lp|NORMAL||||");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
    ASSERT_XML_PATH_COUNT(
        doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal",
        1);
  }
}

TEST_F(AgentTest, EmptyLastItemFromAdapter)
{
  addAdapter({{"FilterDuplicates", true}});
  auto agent = m_agentTestHelper->getAgent();

  auto program = agent->getDataItemByName("LinuxCNC", "program");
  ASSERT_TRUE(program);

  auto tool_id = agent->getDataItemByName("LinuxCNC", "block");
  ASSERT_TRUE(tool_id);

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program|A|block|B");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program||block|B");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program||block|");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program|A|block|B");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program|A|block|");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program|A|block|B|line|C");
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|program|D|block||line|E");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "D");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "E");
  }
}

TEST_F(AgentTest, ConstantValue)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  auto di = agent->getDataItemByName("LinuxCNC", "block");
  ASSERT_TRUE(di);
  di->addConstrainedValue("UNAVAILABLE");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|block|G01X00|Smode|INDEX|line|204");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Block", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode[1]", "SPINDLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:RotaryMode", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }
}

TEST_F(AgentTest, BadDataItem)
{
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|bad|ignore|dummy|1244|line|204");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }
}


// --------------------- Adapter Commands ----------------------

TEST_F(AgentTest, AdapterCommands)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  auto device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->m_preserveUuid);

  m_agentTestHelper->m_adapter->parseBuffer("* uuid: MK-1234\n");
  m_agentTestHelper->m_adapter->parseBuffer("* manufacturer: Big Tool\n");
  m_agentTestHelper->m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
  m_agentTestHelper->m_adapter->parseBuffer("* station: YYYY\n");

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
  }

  device->m_preserveUuid = true;
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: XXXXXXX\n");

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
  }
}

TEST_F(AgentTest, AdapterDeviceCommand)
{
  m_agentTestHelper->createAgent("/samples/two_devices.xml");
  auto agent = m_agentTestHelper->getAgent();

  auto device1 = agent->getDeviceByName("Device1");
  ASSERT_TRUE(device1);
  auto device2 = agent->getDeviceByName("Device2");
  ASSERT_TRUE(device2);
  
  addAdapter();
#if 0
  ASSERT_TRUE(nullptr == adapter->getDevice());

  adapter->parseBuffer("* device: device-2\n");
  ASSERT_TRUE(device2 == adapter->getDevice());

  adapter->parseBuffer("* device: device-1\n");
  ASSERT_TRUE(device1 == adapter->getDevice());

  adapter->parseBuffer("* device: Device2\n");
  ASSERT_TRUE(device2 == adapter->getDevice());

  adapter->parseBuffer("* device: Device1\n");
  ASSERT_TRUE(device1 == adapter->getDevice());
#endif
}

TEST_F(AgentTest, UUIDChange)
{
  auto agent = m_agentTestHelper->getAgent();
  auto device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->m_preserveUuid);

  addAdapter();
  
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: MK-1234\n");
  m_agentTestHelper->m_adapter->parseBuffer("* manufacturer: Big Tool\n");
  m_agentTestHelper->m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
  m_agentTestHelper->m_adapter->parseBuffer("* station: YYYY\n");

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
  }

  {
    // TODO: Fix and make sure dom is updated so this xpath will parse correctly.
    // PARSE_XML_RESPONSE("/current?path=//Device[@uuid=\"MK-1234\"]");
    // PARSE_XML_RESPONSE_QUERY_KV("path", "//Device[@uuid=\"MK-1234\"]");
    // ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream@uuid", "MK-1234");
  }
}

// ------------------------- Asset Tests ---------------------------------

TEST_F(AgentTest, AssetStorage)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml",
                                              8, 4, "1.3", 4, true);

  ASSERT_TRUE(agent->getServer()->isPutEnabled());
  string body = "<Part assetId='P1' deviceUuid='LinuxCNC'>TEST</Part>";
  Routing::QueryMap queries;

  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset/123", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
  }

  m_agentTestHelper->m_request.m_verb = "GET";
  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
  }

  // The device should generate an asset changed event as well.
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentTest, AssetBuffer)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml",
                                              8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  Routing::QueryMap queries;

  queries["device"] = "000";
  queries["type"] = "Part";

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
    ASSERT_EQ(1, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
    ASSERT_EQ(1, agent->getAssetCount("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)2, agent->getAssetCount());
    ASSERT_EQ(2, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)3, agent->getAssetCount());
    ASSERT_EQ(3, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P4'>TEST 4</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, agent->getAssetCount());
  }

  {
    PARSE_XML_RESPONSE("/asset/P4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
    ASSERT_EQ(4, agent->getAssetCount("Part"));
  }

  // Test multiple asset get
  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
  }

  // Test multiple asset get with filter
  {
    PARSE_XML_RESPONSE_QUERY("/assets", queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
  }

  queries["count"] = "2";
  {
    PARSE_XML_RESPONSE_QUERY("/assets", queries);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
  }

  queries.erase("count");

  body = "<Part assetId='P5'>TEST 5</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, agent->getAssetCount());
    ASSERT_EQ(4, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset for assetId: P1");
  }

  body = "<Part assetId='P3'>TEST 6</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, agent->getAssetCount());
    ASSERT_EQ(4, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 6");
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P2'>TEST 7</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, agent->getAssetCount());
    ASSERT_EQ(4, agent->getAssetCount("Part"));
  }

  body = "<Part assetId='P6'>TEST 8</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, agent->getAssetCount());
    ASSERT_EQ(4, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P6");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 8");
  }

  // Now since two and three have been modified, asset 4 should be removed.
  {
    PARSE_XML_RESPONSE("/asset/P4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset for assetId: P4");
  }
}

TEST_F(AgentTest, AssetError)
{
  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset for assetId: 123");
  }
}

TEST_F(AgentTest, AdapterAddAsset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }
}

TEST_F(AgentTest, MultiLineAsset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  
  m_agentTestHelper->m_adapter->parseBuffer("2021-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "<Part assetId='P1'>\n"
      "  <PartXXX>TEST 1</PartXXX>\n"
      "  Some Text\n"
      "  <Extra>XXX</Extra>\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "</Part>\n"
      "--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "TIME");
  }

  // Make sure we can still add a line and we are out of multiline mode...
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "204");
  }
}

TEST_F(AgentTest, BadAsset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  m_agentTestHelper->m_adapter->parseBuffer("2021-02-01T12:00:00Z|@ASSET@|111|CuttingTool|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer((getFile("asset4.xml") + "\n").c_str());
  m_agentTestHelper->m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)0, agent->getAssetCount());
}

TEST_F(AgentTest, AssetRemoval)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml",
                                              8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  Routing::QueryMap query;

  query["device"] = "LinuxCNC";
  query["type"] = "Part";

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
    ASSERT_EQ(1, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
    ASSERT_EQ(1, agent->getAssetCount("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)2, agent->getAssetCount());
    ASSERT_EQ(2, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, agent->getAssetCount());
    ASSERT_EQ(3, agent->getAssetCount("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P2' removed='true'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, agent->getAssetCount(false));
    ASSERT_EQ(3, agent->getAssetCount("Part", false));
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/assets", query);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]@removed", "true");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 3");
  }
}

TEST_F(AgentTest, AssetRemovalByAdapter)
{
  addAdapter();
  Routing::QueryMap query;
  auto agent = m_agentTestHelper->getAgent();
  
  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, agent->getAssetCount());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P2\r");
  ASSERT_EQ((unsigned int)3, agent->getAssetCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/assets", query);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetChanged12)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.2", 25);
  
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 0);
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetRemoved13)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.3", 25);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentTest, AssetAdditionOfAssetRemoved15)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.5", 25);
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", "true");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentTest, AssetPrependId)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|@1|Part|<Part assetId='1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/asset/0001");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "0001");
  }
}

TEST_F(AgentTest, RemoveLastAssetChanged)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");
  ASSERT_EQ((unsigned int)1, agent->getAssetCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentTest, RemoveAssetUsingHttpDelete)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml",
                                              8, 4, "1.3", 4, true);
  addAdapter();

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, agent->getAssetCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE_DELETE("/asset/P1");
  }
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }
}


TEST_F(AgentTest, AssetChangedWhenUnavailable)
{
  addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "");
  }
}

TEST_F(AgentTest, RemoveAllAssets)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  ASSERT_EQ((unsigned int)4, agent->getMaxAssets());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, agent->getAssetCount());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, agent->getAssetCount());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ALL_ASSETS@|Part");
  ASSERT_EQ((unsigned int)3, agent->getAssetCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }
  
  ASSERT_EQ((unsigned int)0, agent->getAssetCount());

  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 0);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "0");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  {
    Routing::QueryMap q{{ "removed", "true" }};
    PARSE_XML_RESPONSE_QUERY("/assets", q);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentTest, AssetProbe)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml",
                                              8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  Routing::QueryMap queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/asset/P2", body, queries);
    ASSERT_EQ((unsigned int)2, agent->getAssetCount());
  }

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
  }
}

TEST_F(AgentTest, ResponseToHTTPAssetPutErrors)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml",
                                 8, 4, "1.3", 4, true);

  const string body {
R"DOC(<CuttingTool assetId="M8010N9172N:1.0" serialNumber="1234" toolId="CAT">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>NEW</Status>
    </CutterStatus>
    <Measurements>
      <FunctionalLength code="LF" maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER"/>
      <CuttingDiameterMax code="DC" maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER"/>
    </Measurements>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC" };
  
  Routing::QueryMap queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "CuttingTool";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]",
                          "Asset parsed with errors.");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]",
                          "FunctionalLength(VALUE): Property VALUE is required and not provided");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]",
                          "Measurements: Invalid element 'FunctionalLength'");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]",
                          "CuttingDiameterMax(VALUE): Property VALUE is required and not provided");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]",
                          "Measurements: Invalid element 'CuttingDiameterMax'");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]",
                          "Measurements(Measurement): Entity list requirement Measurement must have at least 1 entries, 0 found");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]",
                          "CuttingToolLifeCycle: Invalid element 'Measurements'");
  }
}


//  ---------------- Srreaming Tests ---------------------

TEST_F(AgentTest, BadInterval)
{
  Routing::QueryMap query;

  {
    query["interval"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Parameter Error processing request from:  - for query parameter 'interval': cannot convert string 'NON_INTEGER' to integer");
  }

  {
    query["interval"] = "-123";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be greater than -1");
  }

  {
    query["interval"] = "2147483647";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "'interval' must be less than 2147483647");
  }

  {
    query["interval"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "'interval' must be greater than -1");
  }
}


TEST_F(AgentTest, StreamData)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  auto heartbeatFreq{200ms};

  // Start a thread...
  Routing::QueryMap query;
  query["interval"] = "50";
  query["heartbeat"] = to_string(heartbeatFreq.count());
  query["from"] = to_string(agent->getSequence());

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
      PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
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
      test->m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
      test->m_agentTestHelper->m_out.setstate(ios::eofbit);
    };

    m_delay = 10ms;
    auto addThread = std::thread{AddThreadLambda, this};
    try
    {
      PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);

      auto delta = system_clock::now() - startTime;
      ASSERT_LT(delta, (minExpectedResponse + 50ms));
      ASSERT_GT(delta, minExpectedResponse);
      addThread.join();
    }
    catch (...)
    {
      addThread.join();
      throw;
    }
  }
}

TEST_F(AgentTest, StreamDataObserver)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  
  // Start a thread...
  key_value_map query;
  query["interval"] = "100";
  query["heartbeat"] = "1000";
  query["count"] = "10";
  query["from"] = to_string(agent->getSequence());

  // Test to make sure the signal will push the sequence number forward and capture
  // the new data.
  {
    auto streamThreadLambda = [](AgentTest *test) {
      auto agent = test->m_agentTestHelper->getAgent();
      this_thread::sleep_for(test->m_delay);
      agent->setSequence(agent->getSequence() + 20ull);
      test->m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
      this_thread::sleep_for(120ms);
      test->m_agentTestHelper->m_out.setstate(ios::eofbit);
    };

    m_delay = 50ms;
    auto seq = to_string(agent->getSequence() + 20ull);

    auto streamThread = std::thread{streamThreadLambda, this};
    try
    {
      PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
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

// ------------- Put tests

TEST_F(AgentTest, Put)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml",
                                 8, 4, "1.3", 4, true);

  Routing::QueryMap queries;
  string body;

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";

  {
    PARSE_XML_RESPONSE_PUT("/LinuxCNC", body, queries);
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line@timestamp", "TIME");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:PowerState", "ON");
  }
  
}
