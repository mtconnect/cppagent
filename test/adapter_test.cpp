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

#include "adapter/adapter.hpp"

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
  auto adapter = make_unique<Adapter>("device", "localhost", 7878);
  auto handler = make_unique<Handler>();
  
  string data;
  handler->m_processData = [&](const string &d) { data = d; };
  adapter->setHandler(handler);
  
  adapter->processData("Simple Pass Through");
  EXPECT_EQ("Simple Pass Through", data);
  
  EXPECT_FALSE(adapter->getTerminator());
  adapter->processData("A multiline message: __multiline__ABC1234");
  EXPECT_TRUE(adapter->getTerminator());
  EXPECT_EQ("__multiline__ABC1234", *adapter->getTerminator());
  adapter->processData("Another Line...");
  adapter->processData("__multiline__ABC---");
  adapter->processData("__multiline__ABC1234");
  
  const auto exp = R"DOC(A multiline message: 
Another Line...
__multiline__ABC---)DOC";
  EXPECT_EQ(exp, data);
}

#if 0
TEST(AdapterTest, EscapedLine)
{
  std::map<std::string, std::vector<std::string>> data;
  // correctly escaped
  data[R"("a\|b")"] = {"a|b"};
  data[R"("a\|b"|z)"] = {"a|b", "z"};
  data[R"(y|"a\|b")"] = {"y", "a|b"};
  data[R"(y|"a\|b"|z)"] = {"y", "a|b", "z"};

  // correctly escaped with multiple pipes
  data[R"("a\|b\|c")"] = {"a|b|c"};
  data[R"("a\|b\|c"|z)"] = {"a|b|c", "z"};
  data[R"(y|"a\|b\|c")"] = {"y", "a|b|c"};
  data[R"(y|"a\|b\|c"|z)"] = {"y", "a|b|c", "z"};

  // correctly escaped with pipe at front
  data[R"("\|b\|c")"] = {"|b|c"};
  data[R"("\|b\|c"|z)"] = {"|b|c", "z"};
  data[R"(y|"\|b\|c")"] = {"y", "|b|c"};
  data[R"(y|"\|b\|c"|z)"] = {"y", "|b|c", "z"};

  // correctly escaped with pipes at end
  data[R"("a\|b\|")"] = {"a|b|"};
  data[R"("a\|b\|"|z)"] = {"a|b|", "z"};
  data[R"(y|"a\|b\|")"] = {"y", "a|b|"};
  data[R"(y|"a\|b\|"|z)"] = {"y", "a|b|", "z"};

  // missing first quote
  data["a\\|b\""] = {"a\\", "b\""};
  data["a\\|b\"|z"] = {"a\\", "b\"", "z"};
  data["y|a\\|b\""] = {"y", "a\\", "b\""};
  data["y|a\\|b\"|z"] = {"y", "a\\", "b\"", "z"};

  // missing first quote and multiple pipes
  data[R"(a\|b\|c")"] = {"a\\", "b\\", "c\""};
  data[R"(a\|b\|c"|z)"] = {"a\\", "b\\", "c\"", "z"};
  data[R"(y|a\|b\|c")"] = {"y", "a\\", "b\\", "c\""};
  data[R"(y|a\|b\|c"|z)"] = {"y", "a\\", "b\\", "c\"", "z"};

  // missing last quote
  data["\"a\\|b"] = {"\"a\\", "b"};
  data["\"a\\|b|z"] = {"\"a\\", "b", "z"};
  data["y|\"a\\|b"] = {"y", "\"a\\", "b"};
  data["y|\"a\\|b|z"] = {"y", "\"a\\", "b", "z"};

  // missing last quote and pipe at end et al.
  data["\"a\\|"] = {"\"a\\", ""};
  data["y|\"a\\|"] = {"y", "\"a\\", ""};
  data["y|\"a\\|z"] = {"y", "\"a\\", "z"};
  data[R"(y|"a\|"z)"] = {"y", "\"a\\", "\"z"};

  for (const auto &test : data)
  {
    std::string value;
    std::istringstream toParse(test.first);
    for (const std::string &expected : test.second)
    {
      // TODO: Need to fix...
      //mtconnect::adapter::Adapter::getEscapedLine(toParse, value);
      ASSERT_EQ(expected, value);
    }
  }
}

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
