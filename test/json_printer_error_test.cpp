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

#include <gtest/gtest.h>

#include "globals.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "checkpoint.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include <nlohmann/json.hpp>
#include "observation.hpp"

#include "json_helper.hpp"

using namespace std;
using namespace mtconnect;
using json = nlohmann::json;

class JsonPrinterErrorTest : public testing::Test {
 protected:
  void SetUp() override { m_printer.reset(new JsonPrinter("1.5", true)); }

  std::unique_ptr<JsonPrinter> m_printer;
};

TEST_F(JsonPrinterErrorTest, PrintError) {
  auto doc = m_printer->printError(12345u, 1024u, 56u, "BAD_BAD", "Never do that again");
  // cout << doc << endl;
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectError"), it.key());
  ASSERT_EQ(12345, jdoc.at("/MTConnectError/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(1024, jdoc.at("/MTConnectError/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(false, jdoc.at("/MTConnectError/Header/testIndicator"_json_pointer).get<bool>());

  ASSERT_EQ(string("BAD_BAD"),
            jdoc.at("/MTConnectError/Errors/Error/errorCode"_json_pointer).get<string>());
  ASSERT_EQ(string("Never do that again"),
            jdoc.at("/MTConnectError/Errors/Error/text"_json_pointer).get<string>());
}
