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

#include <date/date.h>
#include <thread>

#include "mtconnect/utilities.hpp"

using namespace std;
using namespace mtconnect;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(GlobalsTest, IntToString)
{
  ASSERT_EQ((string) "1234", to_string(1234));
  ASSERT_EQ((string) "0", to_string(0));
  ASSERT_EQ((string) "123456789", to_string(123456789));
  ASSERT_EQ((string) "1", to_string(1));
}

TEST(GlobalsTest, FloatToString)
{
  ASSERT_EQ((string) "1.234", format(1.234));
  ASSERT_EQ((string) "0", format(0.0));
  ASSERT_EQ((string) "0.123456", format(.123456));
  ASSERT_EQ((string) "1", format(1.0));
}

TEST(GlobalsTest, ToUpperCase)
{
  string lower = "abcDef";
  ASSERT_EQ((string) "ABCDEF", toUpperCase(lower));

  lower = "a|b|CC|ddd";
  ASSERT_EQ((string) "A|B|CC|DDD", toUpperCase(lower));

  lower = "qwerty.asdf|";
  ASSERT_EQ((string) "QWERTY.ASDF|", toUpperCase(lower));
}

TEST(GlobalsTest, IsNonNegativeInteger)
{
  ASSERT_TRUE(isNonNegativeInteger("12345"));
  ASSERT_TRUE(isNonNegativeInteger("123456789012345678901234567890"));
  ASSERT_TRUE(isNonNegativeInteger("0"));
  ASSERT_TRUE(!isNonNegativeInteger("-12345"));
  ASSERT_TRUE(!isNonNegativeInteger("123456789012345678901234567890a"));
  ASSERT_TRUE(!isNonNegativeInteger("123.45"));
}

TEST(GlobalsTest, Time)
{
  auto time1 = getCurrentTime(GMT);
  auto time2 = getCurrentTime(GMT);
  ASSERT_EQ(time1, time2);

  std::this_thread::sleep_for(1s);
  auto time3 = getCurrentTime(GMT);
  ASSERT_TRUE(time1 != time3);

  auto time4 = getCurrentTime(GMT);
  auto time5 = getCurrentTime(GMT);
  ASSERT_EQ(time4, time5);

  std::this_thread::sleep_for(1s);
  auto time6 = getCurrentTime(GMT);
  ASSERT_TRUE(time4 != time6);

  auto time7 = getCurrentTimeInSec();
  auto time8 = getCurrentTimeInSec();
  ASSERT_EQ(time7, time8);

  std::this_thread::sleep_for(2s);
  auto time9 = getCurrentTimeInSec();
  ASSERT_TRUE(time7 < time9);
}

TEST(GlobalsTest, IllegalCharacters)
{
  string before1("Don't Change Me"), after1("Don't Change Me");
  replaceIllegalCharacters(before1);
  ASSERT_EQ(before1, after1);

  string before2("(Foo & Bar)"), after2("(Foo &amp; Bar)");
  replaceIllegalCharacters(before2);
  ASSERT_EQ(before2, after2);

  string before3("Crazy<<&>>"), after3("Crazy&lt;&lt;&amp;&gt;&gt;");
  replaceIllegalCharacters(before3);
  ASSERT_EQ(before3, after3);
}

TEST(GlobalsTest, GetCurrentTime)
{
  auto gmt = getCurrentTime(GMT);
  auto time = parseTimeMicro(gmt);
  ASSERT_TRUE(time != 0);

  auto gmtUsec = getCurrentTime(GMT_UV_SEC);
  time = parseTimeMicro(gmtUsec);
  ASSERT_TRUE(time != 0);

  auto local = getCurrentTime(LOCAL);
  time = parseTimeMicro(local);
  ASSERT_TRUE(time != 0);

  auto human = getCurrentTime(HUM_READ);
  int year, day, hour, min, sec;
  char wday[5] = {0};
  char mon[4] = {0};
  char tzs[32] = {0};

  int n = sscanf(human.c_str(), "%3s, %2d %3s %4d %2d:%2d:%2d %5s", wday, &day, mon, &year, &hour,
                 &min, &sec, (char *)&tzs);

  ASSERT_EQ(8, n);
}

TEST(GlobalsTest, GetCurrentTime2)
{
  // Build a known system time point
  auto knownTimePoint = std::chrono::system_clock::from_time_t(0);  // 1 Jan 1970 00:00:00

  auto gmt = getCurrentTime(knownTimePoint, GMT);
  ASSERT_EQ(string("1970-01-01T00:00:00Z"), gmt);
  auto gmtUvSec = getCurrentTime(knownTimePoint, GMT_UV_SEC);
  ASSERT_EQ(string("1970-01-01T00:00:00.000000Z"), gmtUvSec);
  auto humRead = getCurrentTime(knownTimePoint, HUM_READ);
  ASSERT_EQ(string("Thu, 01 Jan 1970 00:00:00 GMT"), humRead);

  // Add a small amount of time 50.123456 seconds
  auto offsetTimePoint = knownTimePoint + std::chrono::microseconds(50123456);

  gmt = getCurrentTime(offsetTimePoint, GMT);
  ASSERT_EQ(string("1970-01-01T00:00:50Z"), gmt);
  gmtUvSec = getCurrentTime(offsetTimePoint, GMT_UV_SEC);
  ASSERT_EQ(string("1970-01-01T00:00:50.123456Z"), gmtUvSec);
  humRead = getCurrentTime(offsetTimePoint, HUM_READ);
  ASSERT_EQ(string("Thu, 01 Jan 1970 00:00:50 GMT"), humRead);

  // Offset again but by a time period that should be rounded down for some formats, 10.654321
  // seconds
  offsetTimePoint = knownTimePoint + std::chrono::microseconds(10654321);
  gmt = getCurrentTime(offsetTimePoint, GMT);
  ASSERT_EQ(string("1970-01-01T00:00:10Z"), gmt);
  gmtUvSec = getCurrentTime(offsetTimePoint, GMT_UV_SEC);
  ASSERT_EQ(string("1970-01-01T00:00:10.654321Z"), gmtUvSec);
  humRead = getCurrentTime(offsetTimePoint, HUM_READ);
  ASSERT_EQ(string("Thu, 01 Jan 1970 00:00:10 GMT"), humRead);
}

TEST(GlobalsTest, ParseTimeMicro)
{
  // This time is 123456 microseconds after the epoch
  auto v = parseTimeMicro("1970-01-01T00:00:00.123456Z");
  ASSERT_EQ(uint64_t {123456}, v);
}

TEST(GlobalsTest, AddNamespace)
{
  auto result = addNamespace("//Device//Foo", "m");
  ASSERT_EQ(string("//m:Device//m:Foo"), result);

  result = addNamespace("//Device//*", "m");
  ASSERT_EQ(string("//m:Device//*"), result);

  result = addNamespace("//Device//*|//Foo", "m");
  ASSERT_EQ(string("//m:Device//*|//m:Foo"), result);

  result = addNamespace("//Device//x:Foo", "m");
  ASSERT_EQ(string("//m:Device//x:Foo"), result);

  result = addNamespace("//Device//*|//x:Foo", "m");
  ASSERT_EQ(string("//m:Device//*|//x:Foo"), result);

  result = addNamespace("//Device/DataItems/", "m");
  ASSERT_EQ(string("//m:Device/m:DataItems/"), result);
}

TEST(GlobalsTest, ParseTimeMilli)
{
  string v = "2012-11-20T12:33:22.123456";

  auto time = parseTimeMicro(v);
  ASSERT_TRUE(1353414802123456LL == time);

  v = "2012-11-20T12:33:22.123";
  time = parseTimeMicro(v);
  ASSERT_TRUE(1353414802123000LL == time);
}

TEST(GlobalsTest, Int64ToString) { ASSERT_EQ((string) "8805345009", to_string(8805345009ULL)); }
