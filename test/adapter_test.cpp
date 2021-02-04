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
#include "pipeline/pipeline_context.hpp"
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;

TEST(AdapterTest, MultilineData)
{
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  auto pipeline = make_unique<AdapterPipeline>(context);
  auto adapter = make_unique<Adapter>("localhost", 7878, ConfigOptions{}, pipeline);
  
  auto handler = make_unique<Handler>();
  string data;
  handler->m_processData = [&](const string &d, const string &s) { data = d; };
  adapter->setHandler(handler);
  
  adapter->processData("Simple Pass Through");
  EXPECT_EQ("Simple Pass Through", data);
  
  EXPECT_FALSE(adapter->getTerminator());
  adapter->processData("A multiline message: --multiline--ABC1234");
  EXPECT_TRUE(adapter->getTerminator());
  EXPECT_EQ("--multiline--ABC1234", *adapter->getTerminator());
  adapter->processData("Another Line...");
  adapter->processData("--multiline--ABC---");
  adapter->processData("--multiline--ABC1234");
  
  const auto exp = R"DOC(A multiline message: 
Another Line...
--multiline--ABC---)DOC";
  EXPECT_EQ(exp, data);
}

#if 0

// TODO: Move these tests here and just test the Adapter w/o the agent
TEST(AdapterTest, RelativeTime)
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

TEST_F(AdapterTest, RelativeParsedTime)
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
  ASSERT_EQ(uint64_t(1234556000ull), m_adapter->getBaseOffset());
}
#endif
