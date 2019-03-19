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

#include "Cuti.h"
#include <thread>
#include <date/date.h>
#include "globals.hpp"

using namespace std;
using namespace mtconnect;

TEST_CLASS(GlobalsTest)
{
public:
  void testIntToString();
  void testFloatToString();
  void testToUpperCase();
  void testIsNonNegativeInteger();
  void testTime();
  void testIllegalCharacters();
  void testGetCurrentTime();
  void testGetCurrentTime2();
  void testParseTimeMicro();
  void testAddNamespace();
  void testParseTimeMilli();
  void testInt64ToString();
  
  SET_UP();
  TEAR_DOWN();
  
  CPPUNIT_TEST_SUITE(GlobalsTest);
  CPPUNIT_TEST(testIntToString);
  CPPUNIT_TEST(testFloatToString);
  CPPUNIT_TEST(testToUpperCase);
  CPPUNIT_TEST(testIsNonNegativeInteger);
  CPPUNIT_TEST(testTime);
  CPPUNIT_TEST(testIllegalCharacters);
  CPPUNIT_TEST(testGetCurrentTime);
  CPPUNIT_TEST(testGetCurrentTime2);
  CPPUNIT_TEST(testParseTimeMicro);
  CPPUNIT_TEST(testAddNamespace);
  CPPUNIT_TEST(testParseTimeMilli);
  CPPUNIT_TEST(testInt64ToString);
  CPPUNIT_TEST_SUITE_END();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(GlobalsTest);


void GlobalsTest::setUp()
{
}


void GlobalsTest::tearDown()
{
}


void GlobalsTest::testIntToString()
{
  CPPUNIT_ASSERT_EQUAL((string) "1234", intToString(1234));
  CPPUNIT_ASSERT_EQUAL((string) "0", intToString(0));
  CPPUNIT_ASSERT_EQUAL((string) "123456789", intToString(123456789));
  CPPUNIT_ASSERT_EQUAL((string) "1", intToString(1));
}


void GlobalsTest::testFloatToString()
{
  CPPUNIT_ASSERT_EQUAL((string) "1.234", floatToString(1.234));
  CPPUNIT_ASSERT_EQUAL((string) "0", floatToString(0.0));
  CPPUNIT_ASSERT_EQUAL((string) "0.123456", floatToString(.123456));
  CPPUNIT_ASSERT_EQUAL((string) "1", floatToString(1.0));
}


void GlobalsTest::testToUpperCase()
{
  string lower = "abcDef";
  CPPUNIT_ASSERT_EQUAL((string) "ABCDEF", toUpperCase(lower));
  
  lower = "a|b|CC|ddd";
  CPPUNIT_ASSERT_EQUAL((string) "A|B|CC|DDD", toUpperCase(lower));
  
  lower = "qwerty.asdf|";
  CPPUNIT_ASSERT_EQUAL((string) "QWERTY.ASDF|", toUpperCase(lower));
}


void GlobalsTest::testIsNonNegativeInteger()
{
  CPPUNIT_ASSERT(isNonNegativeInteger("12345"));
  CPPUNIT_ASSERT(isNonNegativeInteger("123456789012345678901234567890"));
  CPPUNIT_ASSERT(isNonNegativeInteger("0"));
  CPPUNIT_ASSERT(!isNonNegativeInteger("-12345"));
  CPPUNIT_ASSERT(!isNonNegativeInteger("123456789012345678901234567890a"));
  CPPUNIT_ASSERT(!isNonNegativeInteger("123.45"));
}


void GlobalsTest::testTime()
{
  auto time1 = getCurrentTime(GMT);
  auto time2 = getCurrentTime(GMT);
  CPPUNIT_ASSERT_EQUAL(time1, time2);
  
  std::this_thread::sleep_for(1s);
  auto time3 = getCurrentTime(GMT);
  CPPUNIT_ASSERT(time1 != time3);
  
  auto time4 = getCurrentTime(GMT);
  auto time5 = getCurrentTime(GMT);
  CPPUNIT_ASSERT_EQUAL(time4, time5);
  
  std::this_thread::sleep_for(1s);
  auto time6 = getCurrentTime(GMT);
  CPPUNIT_ASSERT(time4 != time6);
  
  auto time7 = getCurrentTimeInSec();
  auto time8 = getCurrentTimeInSec();
  CPPUNIT_ASSERT_EQUAL(time7, time8);
  
  std::this_thread::sleep_for(2s);
  auto time9 = getCurrentTimeInSec();
  CPPUNIT_ASSERT(time7 < time9);
}


void GlobalsTest::testIllegalCharacters()
{
  string before1("Don't Change Me"), after1("Don't Change Me");
  replaceIllegalCharacters(before1);
  CPPUNIT_ASSERT_EQUAL(before1, after1);
  
  string before2("(Foo & Bar)"), after2("(Foo &amp; Bar)");
  replaceIllegalCharacters(before2);
  CPPUNIT_ASSERT_EQUAL(before2, after2);
  
  string before3("Crazy<<&>>"), after3("Crazy&lt;&lt;&amp;&gt;&gt;");
  replaceIllegalCharacters(before3);
  CPPUNIT_ASSERT_EQUAL(before3, after3);
}

void GlobalsTest::testGetCurrentTime()
{
  auto gmt = getCurrentTime(GMT);
  auto time = parseTimeMicro(gmt);
  CPPUNIT_ASSERT(time != 0);
  
  auto gmtUsec = getCurrentTime(GMT_UV_SEC);
  time = parseTimeMicro(gmtUsec);
  CPPUNIT_ASSERT(time != 0);
  
  auto local = getCurrentTime(LOCAL);
  time = parseTimeMicro(local);
  CPPUNIT_ASSERT(time != 0);
  
  
  auto human = getCurrentTime(HUM_READ);
  int year, day, hour, min, sec;
  char wday[5] = {0};
  char mon[4] = {0};
  char tzs[32] = {0};
  
  int n = sscanf(human.c_str(), "%3s, %2d %3s %4d %2d:%2d:%2d %5s",
                 wday, &day, mon, &year,
                 &hour, &min, &sec, (char *) &tzs);
  
  CPPUNIT_ASSERT_EQUAL(8, n);
}


void GlobalsTest::testGetCurrentTime2()
{
  // Build a known system time point
  auto knownTimePoint = std::chrono::system_clock::from_time_t(0); // 1 Jan 1970 00:00:00
  
  auto gmt = getCurrentTime(knownTimePoint, GMT);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:00Z"), gmt);
  auto gmtUvSec = getCurrentTime(knownTimePoint, GMT_UV_SEC);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:00.000000Z"), gmtUvSec);
  auto humRead = getCurrentTime(knownTimePoint, HUM_READ);
  CPPUNIT_ASSERT_EQUAL(string("Thu, 01 Jan 1970 00:00:00 GMT"), humRead);
  
  // Add a small amount of time 50.123456 seconds
  auto offsetTimePoint = knownTimePoint + std::chrono::microseconds(50123456);
  
  gmt = getCurrentTime(offsetTimePoint, GMT);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:50Z"), gmt);
  gmtUvSec = getCurrentTime(offsetTimePoint, GMT_UV_SEC);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:50.123456Z"), gmtUvSec);
  humRead = getCurrentTime(offsetTimePoint, HUM_READ);
  CPPUNIT_ASSERT_EQUAL(string("Thu, 01 Jan 1970 00:00:50 GMT"), humRead);
  
  // Offset again but by a time period that should be rounded down for some formats, 10.654321 seconds
  offsetTimePoint = knownTimePoint + std::chrono::microseconds(10654321);
  gmt = getCurrentTime(offsetTimePoint, GMT);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:10Z"), gmt);
  gmtUvSec = getCurrentTime(offsetTimePoint, GMT_UV_SEC);
  CPPUNIT_ASSERT_EQUAL(string("1970-01-01T00:00:10.654321Z"), gmtUvSec);
  humRead = getCurrentTime(offsetTimePoint, HUM_READ);
  CPPUNIT_ASSERT_EQUAL(string("Thu, 01 Jan 1970 00:00:10 GMT"), humRead);
}


void GlobalsTest::testParseTimeMicro()
{
  // This time is 123456 microseconds after the epoch
  CPPUNIT_ASSERT_EQUAL(uint64_t{ 123456 }, parseTimeMicro("1970-01-01T00:00:00.123456Z"));
}


void GlobalsTest::testAddNamespace()
{
  auto result = addNamespace("//Device//Foo", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device//m:Foo"), result);
  
  result = addNamespace("//Device//*", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device//*"), result);
  
  result = addNamespace("//Device//*|//Foo", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device//*|//m:Foo"), result);
  
  result = addNamespace("//Device//x:Foo", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device//x:Foo"), result);
  
  result = addNamespace("//Device//*|//x:Foo", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device//*|//x:Foo"), result);
  
  result = addNamespace("//Device/DataItems/", "m");
  CPPUNIT_ASSERT_EQUAL(string("//m:Device/m:DataItems/"), result);
}


void GlobalsTest::testParseTimeMilli()
{
  string v = "2012-11-20T12:33:22.123456";
  
  auto time = parseTimeMicro(v);
  CPPUNIT_ASSERT(1353414802123456LL == time);
  
  v = "2012-11-20T12:33:22.123";
  time = parseTimeMicro(v);
  CPPUNIT_ASSERT(1353414802123000LL == time);
}


void GlobalsTest::testInt64ToString()
{
  CPPUNIT_ASSERT_EQUAL((string) "8805345009", int64ToString(8805345009ULL));
}
