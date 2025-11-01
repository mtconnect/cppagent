//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

/// @file
/// Integration tests for the agent. Covers many behaviours of the agent across many modules.

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
  
  ASSERT_THROW(agent->initialize(context), FatalException);
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
  
  ASSERT_THROW(agent->initialize(context), FatalException);
}

TEST_F(AgentTest, should_return_error_for_unknown_device)
{
  {
    PARSE_XML_RESPONSE("/LinuxCN/probe");
    string message = (string) "Could not find the device 'LinuxCN'";
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
    ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
  }
}

TEST_F(AgentTest, should_return_2_6_error_for_unknown_device)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false,
                                              true, {{configuration::Validation, false}});
  
  {
    PARSE_XML_RESPONSE("/LinuxCN/probe");
    string message = (string) "Could not find the device 'LinuxCN'";
    ASSERT_XML_PATH_EQUAL(doc, "//m:NoDevice@errorCode", "NO_DEVICE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:NoDevice/m:ErrorMessage", message.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:NoDevice/m:Request", "MTConnectDevices");
    ASSERT_XML_PATH_EQUAL(doc, "//m:NoDevice/m:URI", "/LinuxCN/probe");
    ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
  }
}

TEST_F(AgentTest, should_return_error_when_path_cannot_be_parsed)
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

TEST_F(AgentTest, should_return_2_6_error_when_path_cannot_be_parsed)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  
  {
    QueryMap query {{"path", "//////Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:ErrorMessage", message.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:URI", "/current?path=//////Linear");
  }
  
  {
    QueryMap query {{"path", "//Axes?//Linear"}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:ErrorMessage", message.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:URI", "/current?path=//Axes?//Linear");
  }
  
  {
    QueryMap query {{"path", "//Devices/Device[@name=\"I_DON'T_EXIST\""}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    string message = (string)
    "The path could not be parsed. Invalid syntax: //Devices/Device[@name=\"I_DON'T_EXIST\"";
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath@errorCode", "INVALID_XPATH");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:ErrorMessage", message.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidXPath/m:URI",
                          "/current?path=//Devices/Device[@name=\"I_DON'T_EXIST\"");
  }
}

TEST_F(AgentTest, should_handle_a_correct_path)
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

TEST_F(AgentTest, should_report_an_invalid_uri)
{
  using namespace rest_sink;
  {
    PARSE_XML_RESPONSE("/bad_path");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "0.0.0.0: Cannot find handler for: GET /bad_path");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
  
  {
    PARSE_XML_RESPONSE("/bad/path/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "0.0.0.0: Cannot find handler for: GET /bad/path/");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
  
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current/blah");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "0.0.0.0: Cannot find handler for: GET /LinuxCNC/current/blah");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
}

TEST_F(AgentTest, should_report_a_2_6_invalid_uri)
{
  using namespace rest_sink;
  
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  
  {
    PARSE_XML_RESPONSE("/bad_path");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:ErrorMessage",
                          "0.0.0.0: Cannot find handler for: GET /bad_path");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:URI", "/bad_path");
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
  
  {
    PARSE_XML_RESPONSE("/bad/path/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:ErrorMessage",
                          "0.0.0.0: Cannot find handler for: GET /bad/path/");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:URI", "/bad/path/");
    
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
  
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current/blah");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:ErrorMessage",
                          "0.0.0.0: Cannot find handler for: GET /LinuxCNC/current/blah");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:URI", "/LinuxCNC/current/blah");
    
    EXPECT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    EXPECT_FALSE(m_agentTestHelper->m_dispatched);
  }
}

TEST_F(AgentTest, should_handle_current_at)
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

TEST_F(AgentTest, should_handle_64_bit_current_at)
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

TEST_F(AgentTest, should_report_out_of_range_for_current_at)
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

TEST_F(AgentTest, should_report_2_6_out_of_range_for_current_at)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  
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
  auto max = seq - 1;
  {
    auto s = to_string(seq);
    query["at"] = s;
    sprintf(line, "'at' must be less than %d", int32_t(seq));
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", line);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:Request", "MTConnectStreams");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", ("/current?at=" + s).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "at");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", s.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum", to_string(max).c_str());
  }
  
  seq = circ.getFirstSequence() - 1;
  
  {
    query["at"] = to_string(seq);
    sprintf(line, "'at' must be greater than 0");
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", line);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:Request", "MTConnectStreams");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/current?at=0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "at");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum", to_string(max).c_str());
  }
}

TEST_F(AgentTest, AddAdapter) { addAdapter(); }

TEST_F(AgentTest, should_download_file)
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

TEST_F(AgentTest, should_report_not_found_when_cannot_find_file)
{
  QueryMap query;
  
  string uri("/schemas/MTConnectDevices_1.1.xsd");
  
  // Register a file with the agent.
  auto rest = m_agentTestHelper->getRestService();
  rest->getFileCache()->registerFile(uri, string("./BadFileName.xsd"), "1.1");
  
  {
    PARSE_XML_RESPONSE(uri.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
                          ("0.0.0.0: Cannot find handler for: GET " + uri).c_str());
  }
}

TEST_F(AgentTest, should_report_2_6_not_found_when_cannot_find_file)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  
  QueryMap query;
  
  string uri("/schemas/MTConnectDevices_1.1.xsd");
  
  // Register a file with the agent.
  auto rest = m_agentTestHelper->getRestService();
  rest->getFileCache()->registerFile(uri, string("./BadFileName.xsd"), "1.1");
  
  {
    PARSE_XML_RESPONSE(uri.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI@errorCode", "INVALID_URI");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:ErrorMessage",
                          ("0.0.0.0: Cannot find handler for: GET " + uri).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidURI/m:URI", "/schemas/MTConnectDevices_1.1.xsd");
  }
}

TEST_F(AgentTest, should_include_composition_ids_in_observations)
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

TEST_F(AgentTest, should_report_an_error_when_the_count_is_out_of_range)
{
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  int size = circ.getBufferSize() + 1;
  {
    QueryMap query {{"count", "NON_INTEGER"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_PARAMETER_VALUE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "query parameter 'count': cannot convert "
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

TEST_F(AgentTest, should_report_a_2_6_error_when_the_count_is_out_of_range)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  int size = circ.getBufferSize() + 1;
  {
    QueryMap query {{"count", "NON_INTEGER"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue@errorCode", "INVALID_PARAMETER_VALUE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:URI", "/sample?count=NON_INTEGER");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:ErrorMessage",
                          "query parameter 'count': cannot convert "
                          "string 'NON_INTEGER' to integer");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Format", "int32");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Type", "integer");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Value", "NON_INTEGER");
  }
  
  {
    QueryMap query {{"count", "-500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    string value("'count' must be greater than ");
    value += to_string(-size);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:Request", "MTConnectStreams");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", value.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?count=-500");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "-500");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum",
                          to_string(size - 1).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum",
                          to_string(-size + 1).c_str());
  }
  
  {
    QueryMap query {{"count", "0"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", "'count' must not be zero(0)");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?count=0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum",
                          to_string(size - 1).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum",
                          to_string(-size + 1).c_str());
  }
  
  {
    QueryMap query {{"count", "500"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    string value("'count' must be less than ");
    value += to_string(size);
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", value.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?count=500");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "500");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum",
                          to_string(size - 1).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum",
                          to_string(-size + 1).c_str());
  }
  
  {
    QueryMap query {{"count", "9999999"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    string value("'count' must be less than ");
    value += to_string(size);
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", value.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?count=9999999");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "9999999");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum",
                          to_string(size - 1).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum",
                          to_string(-size + 1).c_str());
  }
  
  {
    QueryMap query {{"count", "-9999999"}};
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    string value("'count' must be greater than ");
    value += to_string(-size);
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage", value.c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?count=-9999999");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "count");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "-9999999");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum",
                          to_string(size - 1).c_str());
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum",
                          to_string(-size + 1).c_str());
  }
}

TEST_F(AgentTest, should_process_addapter_data)
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

TEST_F(AgentTest, should_get_samples_using_next_sequence)
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

TEST_F(AgentTest, should_give_correct_number_of_samples_with_count)
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

TEST_F(AgentTest, should_give_correct_number_of_samples_with_negative_count)
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

TEST_F(AgentTest, should_give_correct_number_of_samples_with_to_parameter)
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

TEST_F(AgentTest, should_give_empty_stream_with_no_new_samples)
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

TEST_F(AgentTest, should_not_leak_observations_when_added_to_buffer)
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

TEST_F(AgentTest, should_int_64_sequences_should_not_truncate_at_32_bits)
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

TEST_F(AgentTest, should_not_allow_duplicates_values)
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

TEST_F(AgentTest, should_not_duplicate_unavailable_when_disconnected)
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

TEST_F(AgentTest, should_handle_auto_available_if_adapter_option_is_set)
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

TEST_F(AgentTest, should_handle_multiple_disconnnects)
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

TEST_F(AgentTest, should_ignore_timestamps_if_configured_to_do_so)
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

TEST_F(AgentTest, should_support_dynamic_calibration_data)
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

TEST_F(AgentTest, should_filter_as_specified_in_1_3_test_1)
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

TEST_F(AgentTest, should_filter_as_specified_in_1_3_test_2)
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

TEST_F(AgentTest, period_filter_should_work_with_ignore_timestamps)
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

TEST_F(AgentTest, period_filter_should_work_with_relative_time)
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

TEST_F(AgentTest, reset_triggered_should_work)
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

TEST_F(AgentTest, should_honor_references_when_getting_current_or_sample)
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

TEST_F(AgentTest, should_honor_discrete_data_items_and_not_filter_dups)
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

TEST_F(AgentTest, should_honor_upcase_values)
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

TEST_F(AgentTest, should_handle_condition_activation)
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

TEST_F(AgentTest, should_handle_empty_entry_as_last_pair_from_adapter)
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

TEST_F(AgentTest, should_handle_constant_values)
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

TEST_F(AgentTest, should_handle_bad_data_item_from_adapter)
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

TEST_F(AgentTest, adapter_should_receive_commands)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  
  auto device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->preserveUuid());
  
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: MK-1234\n");
  m_agentTestHelper->m_ioContext.run_for(2000ms);
  
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
  
  device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  
  device->setPreserveUuid(true);
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: XXXXXXX\n");
  m_agentTestHelper->m_ioContext.run_for(1000ms);
  
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
  }
  
  auto &options = m_agentTestHelper->m_adapter->getOptions();
  ASSERT_EQ("MK-1234", *GetOption<string>(options, configuration::Device));
}

TEST_F(AgentTest, adapter_should_not_process_uuid_command_with_preserve_uuid)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.3", 4, false,
                                              true, {{configuration::PreserveUUID, true}});
  addAdapter();
  
  auto device = agent->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  ASSERT_TRUE(device->preserveUuid());
  
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: MK-1234\n");
  
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "000");
  }
  
  m_agentTestHelper->m_adapter->processData(
                                            "2021-02-01T12:00:00Z|block|G01X00|mode|AUTOMATIC|execution|READY");
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Block", "G01X00");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ControllerMode", "AUTOMATIC");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Execution", "READY");
  }
}

TEST_F(AgentTest, adapter_should_receive_device_commands)
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
  
  device2 = agent->getDeviceByName("Device2");
  ASSERT_TRUE(device2);
  
  ASSERT_EQ("new-uuid", string(*device2->getUuid()));
  
  m_agentTestHelper->m_adapter->parseBuffer("* device: device-1\n");
  device = GetOption<string>(m_agentTestHelper->m_adapter->getOptions(), configuration::Device);
  ASSERT_EQ(string(*device1->getUuid()), device);
  
  m_agentTestHelper->m_adapter->parseBuffer("* uuid: another-uuid\n");
  device1 = agent->getDeviceByName("Device1");
  ASSERT_TRUE(device1);
  
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

TEST_F(AgentTest, should_handle_uuid_change)
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
  
  auto *pipe = static_cast<shdr::ShdrPipeline *>(m_agentTestHelper->m_adapter->getPipeline());
  
  ASSERT_EQ("MK-1234", pipe->getDevice());
  
  {
    // TODO: Fix and make sure dom is updated so this xpath will parse correctly.
    // PARSE_XML_RESPONSE("/current?path=//Device[@uuid=\"MK-1234\"]");
    // PARSE_XML_RESPONSE_QUERY_KV("path", "//Device[@uuid=\"MK-1234\"]");
    // ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream@uuid", "MK-1234");
  }
}

/// @name Streaming Tests
/// Tests that validate HTTP long poll behavior of the agent

/// @test ensure an error is returned when the interval has an invalid value
TEST_F(AgentTest, interval_should_be_a_valid_integer_value)
{
  QueryMap query;
  
  ///    - Cannot be test or a non-integer value
  {
    query["interval"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_PARAMETER_VALUE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
                          "query parameter 'interval': cannot "
                          "convert string 'NON_INTEGER' to integer");
  }
  
  ///     - Cannot be nagative
  {
    query["interval"] = "-123";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be greater than -1");
  }
  
  ///    - Cannot be >= 2147483647
  {
    query["interval"] = "2147483647";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be less than 2147483647");
  }
  
  ///     - Cannot wrap around and create a negative number was set as a int32
  {
    query["interval"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'interval' must be greater than -1");
  }
}

/// @test ensure an error is returned when the interval has an invalid value using 2.6 error
/// reporting
TEST_F(AgentTest, interval_should_be_a_valid_integer_value_in_2_6)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});
  QueryMap query;
  
  ///    - Cannot be test or a non-integer value
  {
    query["interval"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue@errorCode", "INVALID_PARAMETER_VALUE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:URI", "/sample?interval=NON_INTEGER");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:ErrorMessage",
                          "query parameter 'interval': cannot convert "
                          "string 'NON_INTEGER' to integer");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter@name", "interval");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Format", "int32");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Type", "integer");
    ASSERT_XML_PATH_EQUAL(doc, "//m:InvalidParameterValue/m:QueryParameter/m:Value", "NON_INTEGER");
  }
  
  ///     - Cannot be nagative
  {
    query["interval"] = "-123";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?interval=-123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage",
                          "'interval' must be greater than -1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "interval");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "-123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum", "2147483646");
  }
  
  ///    - Cannot be >= 2147483647
  {
    query["interval"] = "2147483647";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?interval=2147483647");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage",
                          "'interval' must be less than 2147483647");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "interval");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "2147483647");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum", "2147483646");
  }
  
  ///     - Cannot wrap around and create a negative number was set as a int32
  {
    query["interval"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY("/sample", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange@errorCode", "OUT_OF_RANGE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:URI", "/sample?interval=999999999999999999");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:ErrorMessage",
                          "'interval' must be greater than -1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter@name", "interval");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Value", "-1486618625");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Minimum", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:OutOfRange/m:QueryParameter/m:Maximum", "2147483646");
  }
}

/// @test check streaming of data every 50ms
TEST_F(AgentTest, should_stream_data_with_interval)
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
    auto slop {35ms};
    
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
    auto slop {35ms};
    
    PARSE_XML_STREAM_QUERY("/LinuxCNC/sample", query);
    m_agentTestHelper->m_ioContext.run_for(delay);
    
    auto startTime = system_clock::now();
    m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
    m_agentTestHelper->m_ioContext.run_for(5ms);
    auto delta = system_clock::now() - startTime;
    cout << "Delta after data: " << delta.count() << endl;
    
    ASSERT_FALSE(m_agentTestHelper->m_session->m_chunkBody.empty());
    PARSE_XML_CHUNK();
    
    auto deltaMS = duration_cast<milliseconds>(delta);
    EXPECT_GT(slop, deltaMS) << "delta " << deltaMS.count() << " < delay " << slop.count();
  }
}

/// @test Should stream data when observations arrive within the interval
TEST_F(AgentTest, should_signal_observer_when_observations_arrive)
{
  addAdapter();
  auto rest = m_agentTestHelper->getRestService();
  rest->start();
  
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  
  ///     - Set up streaming every 100ms with a 1000ms heartbeat
  std::map<string, string> query;
  query["interval"] = "100";
  query["heartbeat"] = "1000";
  query["count"] = "10";
  query["from"] = to_string(circ.getSequence());
  query["path"] = "//DataItem[@name='line']";
  
  ///    - Test to make sure the signal will push the sequence number forward and capture
  ///      the new data.
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

/// @test check request with from out of range
TEST_F(AgentTest, should_fail_if_from_is_out_of_range)
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
  query["from"] = to_string(circ.getSequence() + 5);
  query["path"] = "//DataItem[@name='line']";
  
  // Test to make sure the signal will push the sequence number forward and capture
  // the new data.
  {
    PARSE_XML_RESPONSE_QUERY("/LinuxCNC/sample", query);
    auto seq = to_string(circ.getSequence() + 20ull);
    m_agentTestHelper->m_ioContext.run_for(200ms);
    
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
  }
}

// ------------- Put tests

/// @name Put Tests
/// Tests that validate the HTTP PUT and POST behavior of the Agent when `AllowPuts` is
/// enabled in the configuration file.

/// @test check if the agent allows making observations when put is allowed
TEST_F(AgentTest, should_allow_making_observations_via_http_put)
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

/// @test putting a condition requires the SHDR formatted data
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

TEST_F(AgentTest, should_not_add_spaces_to_output)
{
  addAdapter();
  
  m_agentTestHelper->m_adapter->processData("2024-01-22T20:00:00Z|program|");
  m_agentTestHelper->m_adapter->processData("2024-01-22T20:00:00Z|block|");
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }
  
  m_agentTestHelper->m_adapter->processData(
                                            "2024-01-22T20:00:00Z|program|              |block|       ");
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
  }
}

TEST_F(AgentTest, should_set_sender_from_config_in_XML_header)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, false,
                                              true, {{configuration::Sender, "MachineXXX"s}});
  ASSERT_TRUE(agent);
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@sender", "MachineXXX");
  }
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@sender", "MachineXXX");
  }
}

TEST_F(AgentTest, should_not_set_validation_flag_in_header_when_validation_is_false)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.5", 4, false,
                                              true, {{configuration::Validation, false}});
  ASSERT_TRUE(agent);
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
}

TEST_F(AgentTest, should_set_validation_flag_in_header_when_version_2_5_validation_on)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.5", 4, false,
                                              true, {{configuration::Validation, true}});
  ASSERT_TRUE(agent);
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", "true");
  }
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", "true");
  }
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", "true");
  }
}

TEST_F(AgentTest, should_not_set_validation_flag_in_header_when_version_below_2_5)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.4", 4, false,
                                              true, {{configuration::Validation, true}});
  ASSERT_TRUE(agent);
  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
  
  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@validation", nullptr);
  }
}

TEST_F(AgentTest, should_initialize_observaton_to_initial_value_when_available)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.2", 4, true);
  
  auto device = m_agentTestHelper->getAgent()->getDeviceByName("LinuxCNC");
  ASSERT_TRUE(device);
  
  addAdapter();
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount", "UNAVAILABLE");
  }
  
  m_agentTestHelper->m_adapter->processData("2024-01-22T20:00:00Z|avail|AVAILABLE");
  
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount", "0");
  }
}
