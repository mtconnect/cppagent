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

#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::entity;
using namespace std;
using namespace std::literals;
using namespace date;
using namespace date::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(TimestampExtractorTest, TestTimeExtraction)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = {"2021-01-19T12:00:00.12345Z", "hello"};

  auto extractor = make_shared<ExtractTimestamp>(false);
  extractor->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T12:00:00.12345Z", format(timestamped->m_timestamp));
  ASSERT_FALSE(timestamped->m_duration);
}

TEST(TimestampExtractorTest, TestTimeExtractionWithDuration)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = {"2021-01-19T12:00:00.12345Z@100.0", "hello"};

  auto extractor = make_shared<ExtractTimestamp>(false);
  extractor->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T12:00:00.12345Z", format(timestamped->m_timestamp));
  ASSERT_TRUE(timestamped->m_duration);
  ASSERT_EQ(100.0, *timestamped->m_duration);
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTimeOffset)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = {"1000.0", "hello"};

  auto extractor = make_shared<ExtractTimestamp>(true);
  extractor->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  extractor->m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };

  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T10:00:00Z", format(timestamped->m_timestamp));

  tokens = make_shared<Tokens>("Tokens", Properties {});
  tokens->m_tokens = {"2000.0", "hello"};

  out = (*extractor)(tokens);
  timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ("2021-01-19T10:00:01Z", format(timestamped->m_timestamp));
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTime)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties {});
  tokens->m_tokens = {"2021-01-19T10:01:00Z", "hello"};

  auto extractor = make_shared<ExtractTimestamp>(true);
  extractor->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  extractor->m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };

  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T10:00:00Z", format(timestamped->m_timestamp));

  tokens = make_shared<Tokens>("Tokens", Properties {});
  tokens->m_tokens = {"2021-01-19T10:01:10Z", "hello"};
  out = (*extractor)(tokens);
  timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ("2021-01-19T10:00:10Z", format(timestamped->m_timestamp));
}

// TODO: Make sure these RelativeTime test cases are covered.

#if 0

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
