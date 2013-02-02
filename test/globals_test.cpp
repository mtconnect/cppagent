
/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "globals_test.hpp"

#ifdef _WINDOWS
#include <windows.h>
#define sleep(s) ::Sleep(s * 1000)
#endif

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(GlobalsTest);

using namespace std;

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
  string time1 = getCurrentTime(GMT);
  string time2 = getCurrentTime(GMT);
  CPPUNIT_ASSERT_EQUAL(time1, time2);
  
  sleep(1);
  string time3 = getCurrentTime(GMT);
  CPPUNIT_ASSERT(time1 != time3);
  
  string time4 = getCurrentTime(GMT);
  string time5 = getCurrentTime(GMT);
  CPPUNIT_ASSERT_EQUAL(time4, time5);
  
  sleep(1);
  string time6 = getCurrentTime(GMT);
  CPPUNIT_ASSERT(time4 != time6);
  
  unsigned int time7 = getCurrentTimeInSec();
  unsigned int time8 = getCurrentTimeInSec();
  CPPUNIT_ASSERT_EQUAL(time7, time8);
  
  sleep(2);
  unsigned int time9 = getCurrentTimeInSec();
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

void GlobalsTest::testGetEnumerations()
{
  unsigned int size = 7;
  const string week[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
  };
  
  CPPUNIT_ASSERT_EQUAL(0, getEnumeration("Sunday", week, size));
  CPPUNIT_ASSERT_EQUAL(1, getEnumeration("Monday", week, size));
  CPPUNIT_ASSERT_EQUAL(2, getEnumeration("Tuesday", week, size));
  CPPUNIT_ASSERT_EQUAL(3, getEnumeration("Wednesday", week, size));
  CPPUNIT_ASSERT_EQUAL(4, getEnumeration("Thursday", week, size));
  CPPUNIT_ASSERT_EQUAL(5, getEnumeration("Friday", week, size));
  CPPUNIT_ASSERT_EQUAL(6, getEnumeration("Saturday", week, size));
  
  CPPUNIT_ASSERT_EQUAL(ENUM_MISS, getEnumeration("Notaday", week, size));
  CPPUNIT_ASSERT_EQUAL(ENUM_MISS, getEnumeration("SUNDAY", week, size));
}


void GlobalsTest::testGetCurrentTime()
{
  string gmt = getCurrentTime(GMT);
  string gmtUsec = getCurrentTime(GMT_UV_SEC);
  string local = getCurrentTime(LOCAL);
  
  cout << gmt << endl;
  cout << gmtUsec << endl;
  cout << local << endl;
  
}

void GlobalsTest::testAddNamespace()
{
  string result;
  result = addNamespace("//Device//Foo", "m");
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
  
  uint64_t time = parseTimeMicro(v);
  CPPUNIT_ASSERT(1353414802123456LL == time);

  v = "2012-11-20T12:33:22.123";
  time = parseTimeMicro(v);
  CPPUNIT_ASSERT(1353414802123000LL == time);
}
