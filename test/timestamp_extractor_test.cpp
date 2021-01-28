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

#include "source/shdr_tokenizer.hpp"
#include "source/timestamp_extractor.hpp"

#include <chrono>

using namespace mtconnect;
using namespace mtconnect::source;
using namespace mtconnect::entity;
using namespace std;
using namespace std::literals;
using namespace date;
using namespace date::literals;

TEST(TimestampExtractorTest, TestTimeExtraction)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = { "2021-01-19T12:00:00.12345Z", "hello" };

  auto extractor = make_shared<ExtractTimestamp>();
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T12:00:00.123450Z", format("%FT%TZ", timestamped->m_timestamp));
  ASSERT_FALSE(timestamped->m_duration);
}

TEST(TimestampExtractorTest, TestTimeExtractionWithDuration)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = { "2021-01-19T12:00:00.12345Z@100.0", "hello" };

  auto extractor = make_shared<ExtractTimestamp>();
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T12:00:00.123450Z", format("%FT%TZ", timestamped->m_timestamp));
  ASSERT_TRUE(timestamped->m_duration);
  ASSERT_EQ(100.0, *timestamped->m_duration);
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTimeOffset)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties());
  tokens->m_tokens = { "1000.0", "hello" };

  auto extractor = make_shared<ExtractTimestamp>();
  extractor->m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };
  extractor->m_relativeTime = true;
  
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);
  
  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T10:00:00.000000Z", format("%FT%TZ", timestamped->m_timestamp));

  tokens = make_shared<Tokens>("Tokens", Properties{});
  tokens->m_tokens = { "2000.0", "hello" };

  out = (*extractor)(tokens);
  timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ("2021-01-19T10:00:01.000000Z", format("%FT%TZ", timestamped->m_timestamp));
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTime)
{
  auto tokens = make_shared<Tokens>("Tokens", Properties{});
  tokens->m_tokens = {"2021-01-19T10:01:00Z", "hello"};

  auto extractor = make_shared<ExtractTimestamp>();
  extractor->m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };
  extractor->m_relativeTime = true;
  
  auto out = (*extractor)(tokens);
  auto timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ(1, out->getProperties().size());
  ASSERT_EQ("hello", timestamped->m_tokens.front());
  ASSERT_EQ("2021-01-19T10:00:00.000000Z", format("%FT%TZ", timestamped->m_timestamp));

  tokens = make_shared<Tokens>("Tokens", Properties{});
  tokens->m_tokens = {"2021-01-19T10:01:10Z", "hello"};
  out = (*extractor)(tokens);
  timestamped = dynamic_pointer_cast<Timestamped>(out);
  ASSERT_TRUE(timestamped);

  ASSERT_EQ("2021-01-19T10:00:10.000000Z", format("%FT%TZ", timestamped->m_timestamp));  
}
