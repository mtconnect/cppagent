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

#include "adapter/shdr_parser.hpp"
#include "adapter/timestamp_extractor.hpp"

#include <chrono>

using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace std;
using namespace std::literals;
using namespace date;
using namespace date::literals;

TEST(TimestampExtractorTest, TestTimeExtraction)
{  
  TokenList tokens { "2021-01-19T12:00:00.12345Z", "hello" };
  Context context;
  
  auto token = tokens.cbegin();
  auto end = tokens.end();
  
  ShdrObservation obsservation;
  ExtractTimestamp(obsservation, token, end, context);

  ASSERT_EQ("hello", *token);
  ASSERT_EQ("2021-01-19T12:00:00.123450Z", format("%FT%TZ", obsservation.m_timestamp));
}

TEST(TimestampExtractorTest, TestTimeExtractionWithDuration)
{
  TokenList tokens { "2021-01-19T12:00:00.12345Z@100.0", "hello" };
  Context context;
  
  auto token = tokens.cbegin();
  auto end = tokens.end();
  
  ShdrObservation obsservation;
  ExtractTimestamp(obsservation, token, end, context);

  ASSERT_EQ("hello", *token);
  ASSERT_EQ("2021-01-19T12:00:00.123450Z", format("%FT%TZ", obsservation.m_timestamp));
  ASSERT_TRUE(obsservation.m_duration);
  ASSERT_EQ(100.0, *obsservation.m_duration);
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTimeOffset)
{
  TokenList tokens { "1000.0", "hello" };
  Context context;
  context.m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };
  context.m_relativeTime = true;
  
  auto token = tokens.cbegin();
  auto end = tokens.end();
  
  ShdrObservation obsservation;
  ExtractTimestamp(obsservation, token, end, context);

  ASSERT_EQ("hello", *token);
  ASSERT_EQ("2021-01-19T10:00:00.000000Z", format("%FT%TZ", obsservation.m_timestamp));
  
  tokens = { "2000.0", "hello" };
  token = tokens.cbegin();
  ExtractTimestamp(obsservation, token, end, context);

  ASSERT_EQ("2021-01-19T10:00:01.000000Z", format("%FT%TZ", obsservation.m_timestamp));
}

TEST(TimestampExtractorTest, TestTimeExtractionRelativeTime)
{
  TokenList tokens { "2021-01-19T10:01:00Z", "hello" };
  Context context;
  context.m_now = []() -> Timestamp {
    // 2021-01-19T10:00:00Z
    return std::chrono::system_clock::from_time_t(1611050400);
  };
  context.m_relativeTime = true;
  
  auto token = tokens.cbegin();
  auto end = tokens.end();
  
  ShdrObservation obsservation;
  ExtractTimestamp(obsservation, token, end, context);
  ASSERT_EQ("2021-01-19T10:00:00.000000Z", format("%FT%TZ", obsservation.m_timestamp));
  
  tokens = { "2021-01-19T10:01:10Z", "hello" };
  token = tokens.cbegin();
  ExtractTimestamp(obsservation, token, end, context);

  ASSERT_EQ("2021-01-19T10:00:10.000000Z", format("%FT%TZ", obsservation.m_timestamp));
}
