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
#include "globals.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include "observation.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include "checkpoint.hpp"

#include "json_helper.hpp"

using namespace std;
using namespace mtconnect;
using json = nlohmann::json;

TEST_CLASS(JsonPrinterErrorTest)
{
public:
  void testPrintError();
  
  SET_UP();
  TEAR_DOWN() {
    m_printer.reset();
  }
  
protected:
  std::unique_ptr<JsonPrinter> m_printer;

  CPPUNIT_TEST_SUITE(JsonPrinterErrorTest);
  CPPUNIT_TEST(testPrintError);
  CPPUNIT_TEST_SUITE_END();  
};

CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterErrorTest);

void JsonPrinterErrorTest::setUp()
{
  m_printer.reset(new JsonPrinter("1.5", true));
}

void JsonPrinterErrorTest::testPrintError()
{
  auto doc = m_printer->printError(12345u, 1024u, 56u,
                                   "BAD_BAD", "Never do that again");
  //cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  CPPUNIT_ASSERT_EQUAL(string("MTConnectError"), it.key());
  CPPUNIT_ASSERT_EQUAL(12345, jdoc.at("/MTConnectError/Header/@instanceId"_json_pointer).get<int32_t>());
  CPPUNIT_ASSERT_EQUAL(1024, jdoc.at("/MTConnectError/Header/@bufferSize"_json_pointer).get<int32_t>());
  CPPUNIT_ASSERT_EQUAL(false, jdoc.at("/MTConnectError/Header/@testIndicator"_json_pointer).get<bool>());
  
  CPPUNIT_ASSERT_EQUAL(string("BAD_BAD"), jdoc.at("/MTConnectError/Errors/Error/@errorCode"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("Never do that again"), jdoc.at("/MTConnectError/Errors/Error/#text"_json_pointer).get<string>());
  
}
