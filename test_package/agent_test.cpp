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

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "agent_test_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/file_asset.hpp"
#include "mtconnect/device_model/reference.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "test_utilities.hpp"

#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;
using namespace mtconnect::source::adapter;
using namespace mtconnect::observation;

using status = boost::beast::http::status;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class AgentTest : public testing::Test
{
public:
  typedef std::map<std::string, std::string> map_type;
  using queue_type = list<string>;

protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 25, true);
    m_agentId = to_string(getCurrentTimeInSec());
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

public:
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  std::chrono::milliseconds m_delay {};
};

TEST_F(AgentTest, Constructor)
{
  using namespace configuration;
  ConfigOptions options {{BufferSize, 17}, {MaxAssets, 8}, {SchemaVersion, "1.7"s}};

  unique_ptr<Agent> agent = make_unique<Agent>(m_agentTestHelper->m_ioContext,
                                               TEST_RESOURCE_DIR "/samples/badPath.xml", options);
  auto context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();

  ASSERT_THROW(agent->initialize(context), std::runtime_error);
  agent.reset();

  agent = make_unique<Agent>(m_agentTestHelper->m_ioContext,
                             TEST_RESOURCE_DIR "/samples/test_config.xml", options);

  context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();
  ASSERT_NO_THROW(agent->initialize(context));
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
  using namespace configuration;
  ConfigOptions options {{BufferSize, 17}, {MaxAssets, 8}, {SchemaVersion, "1.5"s}};

  unique_ptr<Agent> agent = make_unique<Agent>(m_agentTestHelper->m_ioContext,
                                               TEST_RESOURCE_DIR "/samples/dup_uuid.xml", options);
  auto context = std::make_shared<pipeline::PipelineContext>();
  context->m_contract = agent->makePipelineContract();

  ASSERT_THROW(agent->initialize(context), std::runtime_error);
}

TEST_F(AgentTest, BadDevices)
{
  {
    PARSE_XML_RESPONSE("/LinuxCN/probe");
    string message = (string) "Could not find the device 'LinuxCN'";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
    ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
  }
}

TEST_F(AgentTest, BadXPath)
{
  {
    QueryMap query {{"path", "//////Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    QueryMap query {{"path", "//Axes?//Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }

  {
    QueryMap query {{"path", "//Devices/Device[@name=\"I_DON'T_EXIST\""}};
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
    QueryMap query {{"path", "//Power"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Power']//m:PowerState",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_COUNT(doc, "//m:ComponentStream", 1);
  }

  {
    QueryMap query {
        {"path", "//Rotary[@name='C']//DataItem[@category='SAMPLE' or @category='CONDITION']"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);

    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:SpindleSpeed",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Load", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Unavailable", "");
  }
}

TEST_F(AgentTest, BadPath)
{
  using namespace rest_sink;
  {
    PARSE_XML_RESPONSE("/bad_path");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "0.0.0.0: Cannot find handler for: GET /bad_path");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }

  {
    PARSE_XML_RESPONSE("/bad/path/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "0.0.0.0: Cannot find handler for: GET /bad/path/");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current/blah");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "0.0.0.0: Cannot find handler for: GET /LinuxCNC/current/blah");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
}

TEST_F(AgentTest, CurrentAt)
{
  QueryMap query;
  PARSE_XML_RESPONSE_QUERY("/current", query);

  addAdapter();

  // Get the current position
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();
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
    // cout << "Line #: " << i + 1 << " at " << i + seq << endl;
    query["at"] = to_string(i + seq);
    ;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    // m_agentTestHelper->printsession();
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", to_string(i + 1).c_str());
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
    query["at"] = to_string(i + seq);
    ;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", to_string(i + 1).c_str());
  }

  // Check the first couple of items in the list
  for (int j = 0; j < 10; j++)
  {
    auto i = circ.getSequence() - circ.getBufferSize() - seq + j;
    query["at"] = to_string(i + seq);
    ;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", to_string(i + 1).c_str());
  }

  // Test out of range...
  {
    auto i = circ.getSequence() - circ.getBufferSize() - seq - 1;
    sprintf(line, "'at' must be greater than %d", int32_t(i + seq));
    query["at"] = to_string(i + seq);
    ;
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, CurrentAt64)
{
  QueryMap query;

  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Initialize the sliding buffer at a very large number.
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();

  uint64_t start = (((uint64_t)1) << 48) + 1317;
  circ.setSequence(start);

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
  QueryMap query;

  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 200; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();

  {
    query["at"] = to_string(seq);
    sprintf(line, "'at' must be less than %d", int32_t(seq));
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }

  seq = circ.getFirstSequence() - 1;

  {
    query["at"] = to_string(seq);
    sprintf(line, "'at' must be greater than %d", int32_t(seq));
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

TEST_F(AgentTest, AddAdapter) { addAdapter(); }

TEST_F(AgentTest, FileDownload)
{
  QueryMap query;

  string uri("/schemas/MTConnectDevices_1.1.xsd");

  // Register a file with the agent.
  auto rest = m_agentTestHelper->getRestService();
  rest->getFileCache()->setMaxCachedFileSize(100 * 1024);
  rest->getFileCache()->registerFile(
      uri, string(PROJECT_ROOT_DIR "/schemas/MTConnectDevices_1.1.xsd"), "1.1");

  // Reqyest the file...
  PARSE_TEXT_RESPONSE(uri.c_str());
  ASSERT_FALSE(m_agentTestHelper->session()->m_body.empty());
  ASSERT_TRUE(m_agentTestHelper->session()->m_body.find_last_of("TEST SCHEMA FILE 1234567890\n") !=
              string::npos);
}

TEST_F(AgentTest, FailedFileDownload)
{
  QueryMap query;

  string uri("/schemas/MTConnectDevices_1.1.xsd");

  // Register a file with the agent.
  auto rest = m_agentTestHelper->getRestService();
  rest->getFileCache()->registerFile(uri, string("./BadFileName.xsd"), "1.1");

  {
    PARSE_XML_RESPONSE(uri.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          ("0.0.0.0: Cannot find handler for: GET " + uri).c_str());
  }
}

TEST_F(AgentTest, Composition)
{
  auto agent = m_agentTestHelper->m_agent.get();
  addAdapter();

  DataItemPtr motor = agent->getDataItemForDevice("LinuxCNC", "zt1");
  ASSERT_TRUE(motor);

  DataItemPtr amp = agent->getDataItemForDevice("LinuxCNC", "zt2");
  ASSERT_TRUE(amp);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|zt1|100|zt2|200");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']", "100");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']", "200");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']@compositionId",
                          "zmotor");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']@compositionId",
                          "zamp");
  }
}

TEST_F(AgentTest, BadCount)
{
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  int size = circ.getBufferSize() + 1;
  {
    QueryMap query {{"count", "NON_INTEGER"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "0.0.0.0: Parameter Error: for query parameter 'count': cannot convert "
                          "string 'NON_INTEGER' to integer");
  }

  {
    QueryMap query {{"count", "-500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than ");
    value += to_string(-size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    QueryMap query {{"count", "0"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must not be zero(0)");
  }

  {
    QueryMap query {{"count", "500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than ");
    value += to_string(size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    QueryMap query {{"count", "9999999"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than ");
    value += to_string(size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

  {
    QueryMap query {{"count", "-9999999"}};
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

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|alarm|code|nativeCode|severity|state|description");

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
  QueryMap query;
  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 1; i <= 300; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d", i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();
  {
    query["from"] = to_string(seq);
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, SampleCount)
{
  QueryMap query;
  addAdapter();
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();

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
  QueryMap query;
  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d|Xact|%d", i, i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence() - 20;

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

TEST_F(AgentTest, SampleToParameter)
{
  QueryMap query;
  addAdapter();

  // Get the current position
  char line[80] = {0};

  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "2021-02-01T12:00:00Z|line|%d|Xact|%d", i, i);
    m_agentTestHelper->m_adapter->processData(line);
  }

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence() - 20;

  {
    query["path"] = "//DataItem[@name='Xact']";
    query["count"] = "10";
    query["to"] = to_string(seq);

    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + 1).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

    // Make sure we got 10 lines
    auto start = seq - 20;
    for (int j = 0; j < 10; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, to_string(start + j * 2 + 1).c_str());
    }
  }

  {
    query["path"] = "//DataItem[@name='Xact']";
    query["count"] = "10";
    query["to"] = to_string(seq);
    query["from"] = to_string(seq - 10);

    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + 1).c_str());

    ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 5);

    // Make sure we got 10 lines
    auto start = seq - 10;
    for (int j = 0; j < 5; j++)
    {
      sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
      ASSERT_XML_PATH_EQUAL(doc, line, to_string(start + j * 2 + 1).c_str());
    }
  }

  // TODO: Test negative conditions
  // count < 0
  // to > nextSequence
  // to > from
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
    auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
    QueryMap query {{"from", to_string(circ.getSequence())}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }
}

TEST_F(AgentTest, AddToBuffer)
{
  auto agent = m_agentTestHelper->m_agent.get();
  QueryMap query;

  string device("LinuxCNC"), key("badKey"), value("ON");
  SequenceNumber_t seqNum {0};
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto event1 = circ.getFromBuffer(seqNum);
  ASSERT_FALSE(event1);

  {
    query["from"] = to_string(circ.getSequence());
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
  }

  key = "power";

  auto di2 = agent->getDataItemForDevice(device, key);
  seqNum = m_agentTestHelper->addToBuffer(di2, {{"VALUE", value}}, chrono::system_clock::now());
  auto event2 = circ.getFromBuffer(seqNum);
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
  QueryMap query;
  addAdapter();

  // Set the sequence number near MAX_UINT32
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  circ.setSequence(0xFFFFFFA0);
  SequenceNumber_t seq = circ.getSequence();
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
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence", to_string(seq + i).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + i + 1).c_str());
    }

    {
      query["from"] = to_string(seq);
      query["count"] = "128";

      PARSE_XML_RESPONSE_QUERY("/sample", query);
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + i + 1).c_str());

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
      ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence", to_string(seq + j).c_str());
      ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", to_string(seq + j + 1).c_str());
    }
  }

  ASSERT_EQ(uint64_t(0xFFFFFFA0) + 128ul, circ.getSequence());
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
  addAdapter({{configuration::FilterDuplicates, true}});

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
  addAdapter({{configuration::AutoAvailable, true}});
  auto agent = m_agentTestHelper->m_agent.get();
  auto adapter = m_agentTestHelper->m_adapter;
  auto id = adapter->getIdentity();
  auto d = agent->getDevices().front();
  StringList devices;
  devices.emplace_back(*d->getComponentName());

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
  devices.emplace_back(*d->getComponentName());

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
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2021-02-01T12:00:00Z");
  }

  m_agentTestHelper->m_adapter->setOptions({{configuration::IgnoreTimestamps, true}});
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|205");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2021-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]@timestamp", "!2021-02-01T12:00:00Z");
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
  addAdapter({{configuration::ConversionRequired, true}});
  auto agent = m_agentTestHelper->getAgent();

  // Add a 10.111000 seconds
  m_agentTestHelper->m_adapter->protocolCommand(
      "* calibration:Yact|.01|200.0|Zact|0.02|300|Xts|0.01|500");
  auto di = agent->getDataItemForDevice("LinuxCNC", "Yact");
  ASSERT_TRUE(di);

  // TODO: Fix conversions
  auto &conv1 = di->getConverter();
  ASSERT_TRUE(conv1);
  ASSERT_EQ(0.01, conv1->factor());
  ASSERT_EQ(200.0, conv1->offset());

  di = agent->getDataItemForDevice("LinuxCNC", "Zact");
  ASSERT_TRUE(di);

  auto &conv2 = di->getConverter();
  ASSERT_TRUE(conv2);
  ASSERT_EQ(0.02, conv2->factor());
  ASSERT_EQ(300.0, conv2->offset());

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|Yact|200|Zact|600");
  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|Xts|25|| 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 "
      "5119 5119 5118 "
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

  m_agentTestHelper->m_adapter->processData(
      "2018-04-27T05:00:40.25|load|106|load|108|load|112|pos|35|pos|40");

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
  addAdapter({{configuration::IgnoreTimestamps, true}});

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
  this_thread::sleep_for(11s);
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
  addAdapter({{configuration::RelativeTime, true}});

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
  using namespace device_model;

  m_agentTestHelper->createAgent("/samples/reference_example.xml");
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();

  string id = "mf";
  auto item = agent->getDataItemForDevice((string) "LinuxCNC", id);
  auto comp = item->getComponent();

  auto references = comp->getList("References");
  ASSERT_TRUE(references);
  ASSERT_EQ(3, references->size());
  auto reference = references->begin();

  EXPECT_EQ("DataItemRef", (*reference)->getName());

  EXPECT_EQ("chuck", (*reference)->get<string>("name"));
  EXPECT_EQ("c4", (*reference)->get<string>("idRef"));

  auto ref = dynamic_pointer_cast<Reference>(*reference);
  ASSERT_TRUE(ref);

  ASSERT_EQ(Reference::DATA_ITEM, ref->getReferenceType());
  ASSERT_TRUE(ref->getDataItem().lock()) << "DataItem was not resolved";
  reference++;

  EXPECT_EQ("door", (*reference)->get<string>("name"));
  EXPECT_EQ("d2", (*reference)->get<string>("idRef"));

  ref = dynamic_pointer_cast<Reference>(*reference);
  ASSERT_TRUE(ref);

  ASSERT_EQ(Reference::DATA_ITEM, ref->getReferenceType());
  ASSERT_TRUE(ref->getDataItem().lock()) << "DataItem was not resolved";

  reference++;
  EXPECT_EQ("electric", (*reference)->get<string>("name"));
  EXPECT_EQ("ele", (*reference)->get<string>("idRef"));

  ref = dynamic_pointer_cast<Reference>(*reference);
  ASSERT_TRUE(ref);

  ASSERT_EQ(Reference::COMPONENT, ref->getReferenceType());
  ASSERT_TRUE(ref->getComponent().lock()) << "DataItem was not resolved";

  // Additional data items should be included
  {
    QueryMap query {{"path", "//BarFeederInterface"}};
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
  addAdapter({{configuration::FilterDuplicates, true}});
  auto agent = m_agentTestHelper->getAgent();

  auto msg = agent->getDataItemForDevice("LinuxCNC", "message");
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
  addAdapter({{configuration::FilterDuplicates, true}, {configuration::UpcaseDataItemValue, true}});

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|mode|Hello");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "HELLO");
  }

  m_agentTestHelper->m_adapter->setOptions({{configuration::UpcaseDataItemValue, false}});
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|mode|Hello");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "Hello");
  }
}

TEST_F(AgentTest, ConditionSequence)
{
  addAdapter({{configuration::FilterDuplicates, true}});
  auto agent = m_agentTestHelper->getAgent();
  auto logic = agent->getDataItemForDevice("LinuxCNC", "lp");
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
      "2021-02-01T12:00:00Z|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side "
      "FFFFFFFF");

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
      "2021-02-01T12:00:00Z|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side "
      "FFFFFFFF");

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
      "2021-02-01T12:00:00Z|lp|FAULT|4200|ALARM_D|LOW|4200 ALARM_D Power on effective parameter "
      "set");

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
  addAdapter({{configuration::FilterDuplicates, true}});
  auto agent = m_agentTestHelper->getAgent();

  auto program = agent->getDataItemForDevice("LinuxCNC", "program");
  ASSERT_TRUE(program);

  auto tool_id = agent->getDataItemForDevice("LinuxCNC", "block");
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
  auto di = agent->getDataItemForDevice("LinuxCNC", "block");
  ASSERT_TRUE(di);

  di->setConstantValue("UNAVAILABLE");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|block|G01X00|Smode|INDEX|line|204");

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
  ASSERT_FALSE(device->preserveUuid());

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

  device->setPreserveUuid(true);
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

  auto device =
      GetOption<string>(m_agentTestHelper->m_adapter->getOptions(), configuration::Device);
  ASSERT_EQ(device1->getComponentName(), device);

  m_agentTestHelper->m_adapter->parseBuffer("* device: device-2\n");
  device = GetOption<string>(m_agentTestHelper->m_adapter->getOptions(), configuration::Device);
  ASSERT_EQ(string(*device2->getUuid()), device);

  m_agentTestHelper->m_adapter->parseBuffer("* uuid: new-uuid\n");
  ASSERT_EQ("new-uuid", string(*device2->getUuid()));

  m_agentTestHelper->m_adapter->parseBuffer("* device: device-1\n");
  device = GetOption<string>(m_agentTestHelper->m_adapter->getOptions(), configuration::Device);
  ASSERT_EQ(string(*device1->getUuid()), device);

  m_agentTestHelper->m_adapter->parseBuffer("* uuid: another-uuid\n");
  ASSERT_EQ("another-uuid", string(*device1->getUuid()));
}

TEST_F(AgentTest, adapter_command_should_set_adapter_and_mtconnect_versions)
{
  m_agentTestHelper->createAgent("/samples/kinematics.xml", 8, 4, "1.7", 25);
  addAdapter();

  auto printer = m_agentTestHelper->m_agent->getPrinter("xml");
  ASSERT_FALSE(printer->getModelChangeTime().empty());

  {
    PARSE_XML_RESPONSE("/Agent/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AdapterSoftwareVersion", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectVersion", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->parseBuffer("* adapterVersion: 2.10\n");
  m_agentTestHelper->m_adapter->parseBuffer("* mtconnectVersion: 1.7\n");

  {
    PARSE_XML_RESPONSE("/Agent/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AdapterSoftwareVersion", "2.10");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectVersion", "1.7");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@deviceModelChangeTime",
                          printer->getModelChangeTime().c_str());
    ;
  }

  // Test updating device change time
  string old = printer->getModelChangeTime();
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: another-uuid\n");
  ASSERT_GT(printer->getModelChangeTime(), old);

  {
    PARSE_XML_RESPONSE("/Agent/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@deviceModelChangeTime",
                          printer->getModelChangeTime().c_str());
    ;
  }

  // Test Case insensitivity

  m_agentTestHelper->m_adapter->parseBuffer("* adapterversion: 3.10\n");
  m_agentTestHelper->m_adapter->parseBuffer("* mtconnectversion: 1.6\n");

  {
    PARSE_XML_RESPONSE("/Agent/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AdapterSoftwareVersion", "3.10");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectVersion", "1.6");
  }
}

TEST_F(AgentTest, UUIDChange)
{
  auto agent = m_agentTestHelper->getAgent();
  auto device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->preserveUuid());

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
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

  auto rest = m_agentTestHelper->getRestService();
  ASSERT_TRUE(rest->getServer()->arePutsAllowed());
  string body = "<Part assetId='P1' deviceUuid='LinuxCNC'>TEST</Part>";
  QueryMap queries;

  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, agent->getAssetStorage()->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetStorage()->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset/123", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetStorage()->getCount());
  }

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
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;

  queries["device"] = "000";
  queries["type"] = "Part";

  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
    ASSERT_EQ(2, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)3, storage->getCount());
    ASSERT_EQ(3, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P4'>TEST 4</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/asset/P4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
    ASSERT_EQ(4, storage->getCountForType("Part"));
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
    PARSE_XML_RESPONSE_QUERY("/asset", queries);
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
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          "Cannot find asset for asset Ids: P1");
  }

  body = "<Part assetId='P3'>TEST 6</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
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
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P6'>TEST 8</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
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
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          "Cannot find asset for asset Ids: P4");
  }
}

TEST_F(AgentTest, AssetError)
{
  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          "Cannot find asset for asset Ids: 123");
  }
}

TEST_F(AgentTest, AdapterAddAsset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

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
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "<Part assetId='P1'>\n"
      "  <PartXXX>TEST 1</PartXXX>\n"
      "  Some Text\n"
      "  <Extra>XXX</Extra>\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "</Part>\n"
      "--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2021-02-01T12:00:00Z");
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
  const auto &storage = m_agentTestHelper->m_agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      "2021-02-01T12:00:00Z|@ASSET@|111|CuttingTool|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer((getFile("asset4.xml") + "\n").c_str());
  m_agentTestHelper->m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)0, storage->getCount());
}

TEST_F(AgentTest, AssetRemoval)
{
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap query;

  query["device"] = "LinuxCNC";
  query["type"] = "Part";

  const auto &storage = m_agentTestHelper->m_agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)2, storage->getCount());
    ASSERT_EQ(2, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, storage->getCount());
    ASSERT_EQ(3, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P2' removed='true'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, storage->getCount(false));
    ASSERT_EQ(3, storage->getCountForType("Part", false));
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/asset");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/asset", query);
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
  QueryMap query;
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P2\r");
  ASSERT_EQ((unsigned int)3, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/asset");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/asset", query);
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
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|@1|Part|<Part assetId='1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

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
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");
  ASSERT_EQ((unsigned int)1, storage->getCount(false));

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
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  addAdapter();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount(false));

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
  }
}

TEST_F(AgentTest, RemoveAllAssets)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ALL_ASSETS@|Part");
  ASSERT_EQ((unsigned int)3, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 0);
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  {
    QueryMap q {{"removed", "true"}};
    PARSE_XML_RESPONSE_QUERY("/asset", q);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentTest, AssetProbe)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/asset/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
  }
}

TEST_F(AgentTest, ResponseToHTTPAssetPutErrors)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

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
)DOC"};

  QueryMap queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "CuttingTool";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]",
                          "Asset parsed with errors.");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]",
                          "FunctionalLength(VALUE): Property VALUE is required and not provided");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]",
                          "Measurements: Invalid element 'FunctionalLength'");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]",
                          "CuttingDiameterMax(VALUE): Property VALUE is required and not provided");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]",
                          "Measurements: Invalid element 'CuttingDiameterMax'");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]",
                          "Measurements(Measurement): Entity list requirement Measurement must "
                          "have at least 1 entries, 0 found");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]",
                          "CuttingToolLifeCycle: Invalid element 'Measurements'");
  }
}

TEST_F(AgentTest, update_asset_count_data_item_v2_0)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 10, "2.0", 4, true);
  addAdapter();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry@key", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 1</Part>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry@key", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T1|Tool|<Tool assetId='T1'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "1");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T2|Tool|<Tool assetId='T2'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "2");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T3|Tool|<Tool assetId='T3'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P2");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_COUNT(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", 0);
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ALL_ASSETS@|");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_COUNT(doc, "//m:AssetCountDataSet/*", 0);
  }
}

//  ---------------- Srreaming Tests ---------------------

TEST_F(AgentTest, BadInterval)
{
  QueryMap query;

  {
    query["interval"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "0.0.0.0: Parameter Error: for query parameter 'interval': cannot "
                          "convert string 'NON_INTEGER' to integer");
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
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be less than 2147483647");
  }

  {
    query["interval"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be greater than -1");
  }
}

#ifndef APPVEYOR

TEST_F(AgentTest, StreamData)
{
  addAdapter();
  auto heartbeatFreq {200ms};
  auto rest = m_agentTestHelper->getRestService();
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  rest->start();

  // Start a thread...
  QueryMap query;
  query["interval"] = "50";
  query["heartbeat"] = to_string(heartbeatFreq.count());
  query["from"] = to_string(circ.getSequence());

  // Heartbeat test. Heartbeat should be sent in 200ms. Give
  // 25ms range.
  {
#ifdef APPVEYOR
    auto slop {160ms};
#else
    auto slop {35ms};
#endif

    auto startTime = system_clock::now();
    PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
    while (m_agentTestHelper->m_session->m_chunkBody.empty() &&
           (system_clock::now() - startTime) < 230ms)
      m_agentTestHelper->m_ioContext.run_one_for(5ms);
    auto delta = system_clock::now() - startTime;
    cout << "Delta after heartbeat: " << delta.count() << endl;
    ASSERT_FALSE(m_agentTestHelper->m_session->m_chunkBody.empty());

    PARSE_XML_CHUNK();
    ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", nullptr);
    EXPECT_GT((heartbeatFreq + slop), delta)
        << "delta " << delta.count() << " < hbf " << (heartbeatFreq + slop).count();
    EXPECT_LT(heartbeatFreq, delta) << "delta > hbf: " << delta.count();

    m_agentTestHelper->m_session->closeStream();
  }

  // Set some data and make sure we get data within 40ms.
  // Again, allow for some slop.
  {
    auto delay {40ms};
#ifdef APPVEYOR
    auto slop {160ms};
#else
    auto slop {35ms};
#endif

    PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
    m_agentTestHelper->m_ioContext.run_for(delay);

    auto startTime = system_clock::now();
    m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
    m_agentTestHelper->m_ioContext.run_for(5ms);
    auto delta = system_clock::now() - startTime;
    cout << "Delta after data: " << delta.count() << endl;

    ASSERT_FALSE(m_agentTestHelper->m_session->m_chunkBody.empty());
    PARSE_XML_CHUNK();

    EXPECT_GT(slop, delta) << "delta " << delta.count() << " < delay " << slop.count();
  }
}
#endif

TEST_F(AgentTest, StreamDataObserver)
{
  addAdapter();
  auto rest = m_agentTestHelper->getRestService();
  rest->start();

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();

  // Start a thread...
  std::map<string, string> query;
  query["interval"] = "100";
  query["heartbeat"] = "1000";
  query["count"] = "10";
  query["from"] = to_string(circ.getSequence());
  query["path"] = "//DataItem[@name='line']";

  // Test to make sure the signal will push the sequence number forward and capture
  // the new data.
  {
    PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
    auto seq = to_string(circ.getSequence() + 20ull);
    for (int i = 0; i < 20; i++)
    {
      m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|block|" + to_string(i));
    }
    m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
    m_agentTestHelper->m_ioContext.run_for(200ms);

    PARSE_XML_CHUNK();
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line@sequence", seq.c_str());
  }
}

// ------------- Put tests

TEST_F(AgentTest, Put)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

  QueryMap queries;
  string body;

  queries["time"] = "2021-02-01T12:00:00Z";
  queries["line"] = "205";
  queries["power"] = "ON";

  {
    PARSE_XML_RESPONSE_PUT("/LinuxCNC", body, queries);
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line@timestamp", "2021-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
    ASSERT_XML_PATH_EQUAL(doc, "//m:PowerState", "ON");
  }
}

TEST_F(AgentTest, put_condition_should_parse_condition_data)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

  QueryMap queries;
  string body;

  queries["time"] = "2021-02-01T12:00:00Z";
  queries["lp"] = "FAULT|2001|1||SCANHISTORYRESET";

  {
    PARSE_XML_RESPONSE_PUT("/LinuxCNC", body, queries);
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Fault@timestamp", "2021-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Fault@nativeCode", "2001");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Fault@nativeSeverity", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Fault", "SCANHISTORYRESET");
  }
}

TEST_F(AgentTest, shound_add_asset_count_when_20)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "2.0", 25);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", "true");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_COUNT']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_COUNT']@representation", "DATA_SET");
  }
}

TEST_F(AgentTest, asset_count_should_not_occur_in_header_post_20)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/asset/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:Header/*", 0);
  }
}

TEST_F(AgentTest, asset_count_should_track_asset_additions_by_type)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body1 = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body1, queries);
    ASSERT_EQ(1u, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
  }

  string body2 = "<PartThing assetId='P2'>TEST 2</PartThing>";
  queries["type"] = "PartThing";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body2, queries);
    ASSERT_EQ(2u, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "1");
  }

  {
    PARSE_XML_RESPONSE_PUT("/asset/P3", body2, queries);
    ASSERT_EQ(3u, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "2");
  }

  body2 = "<PartThing assetId='P3' removed='true'>TEST 2</PartThing>";

  {
    PARSE_XML_RESPONSE_PUT("/asset/P3", body2, queries);
    ASSERT_EQ(2u, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "1");
  }
}

TEST_F(AgentTest, asset_should_also_work_using_post_with_assets)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  {
    PARSE_XML_RESPONSE_PUT("/assets", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/assets/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }
}

TEST_F(AgentTest, pre_start_hook_should_be_called)
{
  bool called = false;
  Agent::Hook lambda = [&](Agent &agent) { called = true; };
  AgentTestHelper::Hook helperHook = [&](AgentTestHelper &helper) {
    helper.getAgent()->beforeStartHooks().add(lambda);
  };
  m_agentTestHelper->setAgentCreateHook(helperHook);
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  ASSERT_FALSE(called);
  agent->start();
  ASSERT_TRUE(called);
  agent->stop();
}

TEST_F(AgentTest, pre_initialize_hooks_should_be_called)
{
  bool called = false;
  Agent::Hook lambda = [&](Agent &agent) { called = true; };
  AgentTestHelper::Hook helperHook = [&](AgentTestHelper &helper) {
    helper.getAgent()->beforeInitializeHooks().add(lambda);
  };
  m_agentTestHelper->setAgentCreateHook(helperHook);
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  ASSERT_TRUE(called);
}

TEST_F(AgentTest, post_initialize_hooks_should_be_called)
{
  bool called = false;
  Agent::Hook lambda = [&](Agent &agent) { called = true; };
  AgentTestHelper::Hook helperHook = [&](AgentTestHelper &helper) {
    helper.getAgent()->afterInitializeHooks().add(lambda);
  };
  m_agentTestHelper->setAgentCreateHook(helperHook);
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  ASSERT_TRUE(called);
}

TEST_F(AgentTest, pre_stop_hook_should_be_called)
{
  static bool called = false;
  Agent::Hook lambda = [&](Agent &agent) { called = true; };
  AgentTestHelper::Hook helperHook = [&lambda](AgentTestHelper &helper) {
    helper.getAgent()->beforeStopHooks().add(lambda);
  };
  m_agentTestHelper->setAgentCreateHook(helperHook);
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  ASSERT_FALSE(called);
  agent->start();
  ASSERT_FALSE(called);
  agent->stop();
  ASSERT_TRUE(called);
}

TEST_F(AgentTest, device_should_have_hash_for_2_2)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.2", 4, true);

  auto device = m_agentTestHelper->getAgent()->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);

  auto hash = device->get<string>("hash");
  ASSERT_EQ(28, hash.length());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@hash", hash.c_str());
  }

  auto devices = m_agentTestHelper->getAgent()->getDevices();
  auto di = devices.begin();

  {
    PARSE_XML_RESPONSE("/Agent/sample");

    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceAdded[2]@hash", (*di++)->get<string>("hash").c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceAdded[3]@hash", (*di)->get<string>("hash").c_str());
  }
}
