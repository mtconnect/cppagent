//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "json_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/sink/rest_sink/error.hpp"
#include "mtconnect/utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

using json = nlohmann::json;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class JsonPrinterErrorTest : public testing::Test
{
protected:
  void SetUp() override { m_printer = std::make_unique<printer::JsonPrinter>(1, true); }

  std::unique_ptr<printer::JsonPrinter> m_printer;
};

TEST_F(JsonPrinterErrorTest, should_print_legacy_error)
{
  m_printer->setSchemaVersion("2.5");
  auto error = Error::make(Error::ErrorCode::INVALID_REQUEST, "ERROR TEXT!");
  auto doc = m_printer->printError(123, 9999, 1, error, true);

  // cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectError"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectError/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(9999, jdoc.at("/MTConnectError/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(false, jdoc.at("/MTConnectError/Header/testIndicator"_json_pointer).get<bool>());

  ASSERT_EQ(string("INVALID_REQUEST"),
            jdoc.at("/MTConnectError/Errors/0/Error/errorCode"_json_pointer).get<string>());
  ASSERT_EQ(string("ERROR TEXT!"),
            jdoc.at("/MTConnectError/Errors/0/Error/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterErrorTest, should_print_error_with_2_6_invalid_request)
{
  m_printer->setSchemaVersion("2.6");
  m_printer->setSenderName("MachineXXX");

  auto error = Error::make(Error::ErrorCode::INVALID_REQUEST, "ERROR TEXT!");
  auto doc = m_printer->printError(123, 9999, 1, error, true);

  // cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectError"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectError/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(9999, jdoc.at("/MTConnectError/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(false, jdoc.at("/MTConnectError/Header/testIndicator"_json_pointer).get<bool>());

  ASSERT_EQ(
      string("INVALID_REQUEST"),
      jdoc.at("/MTConnectError/Errors/0/InvalidRequest/errorCode"_json_pointer).get<string>());
  ASSERT_EQ(
      string("ERROR TEXT!"),
      jdoc.at("/MTConnectError/Errors/0/InvalidRequest/ErrorMessage"_json_pointer).get<string>());
}

TEST_F(JsonPrinterErrorTest, should_print_error_with_2_6_invalid_parameter_value)
{
  m_printer->setSchemaVersion("2.6");
  m_printer->setSenderName("MachineXXX");

  auto error = InvalidParameterValue::make("interval", "XXX", "integer", "int64", "Bad Value");
  auto doc = m_printer->printError(123, 9999, 1, error, true);

  // cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectError"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectError/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(9999, jdoc.at("/MTConnectError/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(false, jdoc.at("/MTConnectError/Header/testIndicator"_json_pointer).get<bool>());

  ASSERT_EQ(string("INVALID_PARAMETER_VALUE"),
            jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/errorCode"_json_pointer)
                .get<string>());
  ASSERT_EQ(string("Bad Value"),
            jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/ErrorMessage"_json_pointer)
                .get<string>());
  ASSERT_EQ(
      string("interval"),
      jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/QueryParameter/name"_json_pointer)
          .get<string>());
  ASSERT_EQ(
      string("XXX"),
      jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/QueryParameter/Value"_json_pointer)
          .get<string>());
  ASSERT_EQ(
      string("integer"),
      jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/QueryParameter/Type"_json_pointer)
          .get<string>());
  ASSERT_EQ(
      string("int64"),
      jdoc.at("/MTConnectError/Errors/0/InvalidParameterValue/QueryParameter/Format"_json_pointer)
          .get<string>());
}

TEST_F(JsonPrinterErrorTest, should_print_error_with_2_6_out_of_range)
{
  m_printer->setSchemaVersion("2.6");
  m_printer->setSenderName("MachineXXX");

  auto error = OutOfRange::make("from", 9999999, 10904772, 12907777, "Bad Value");
  auto doc = m_printer->printError(123, 9999, 1, error, true);

  // cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectError"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectError/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(9999, jdoc.at("/MTConnectError/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(false, jdoc.at("/MTConnectError/Header/testIndicator"_json_pointer).get<bool>());

  ASSERT_EQ(string("OUT_OF_RANGE"),
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/errorCode"_json_pointer).get<string>());
  ASSERT_EQ(string("Bad Value"),
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/ErrorMessage"_json_pointer).get<string>());
  ASSERT_EQ(string("from"),
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/QueryParameter/name"_json_pointer)
                .get<string>());
  ASSERT_EQ(9999999,
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/QueryParameter/Value"_json_pointer)
                .get<int32_t>());
  ASSERT_EQ(10904772,
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/QueryParameter/Minimum"_json_pointer)
                .get<int32_t>());
  ASSERT_EQ(12907777,
            jdoc.at("/MTConnectError/Errors/0/OutOfRange/QueryParameter/Maximum"_json_pointer)
                .get<int32_t>());
}
