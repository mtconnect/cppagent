//
//  json_printer_asset_test.cpp
//  agent_test
//
//  Created by William Sobel on 3/28/19.
//

#include <stdio.h>

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
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "component_event.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include "globals.hpp"
#include "checkpoint.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "test_globals.hpp"
#include "json_helper.hpp"
#include "cutting_tool.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

TEST_CLASS(JsonPrinterAssetTest)
{
public:
  SET_UP()
  {
    m_printer.reset(new JsonPrinter("1.5", true));

  }
  
  TEAR_DOWN()
  {
    m_printer.reset();
  }
  
  void testAssetHeader()
  {
    std::vector<AssetPtr> assets;
    auto doc = m_printer->printAssets(123, 1024, 10, assets);
    auto jdoc = json::parse(doc);
    auto it = jdoc.begin();
    
    CPPUNIT_ASSERT_EQUAL(string("MTConnectAssets"), it.key());
    CPPUNIT_ASSERT_EQUAL(123, jdoc.at("/MTConnectAssets/Header/@instanceId"_json_pointer).get<int32_t>());
    CPPUNIT_ASSERT_EQUAL(1024, jdoc.at("/MTConnectAssets/Header/@assetBufferSize"_json_pointer).get<int32_t>());
    CPPUNIT_ASSERT_EQUAL(10, jdoc.at("/MTConnectAssets/Header/@assetCount"_json_pointer).get<int32_t>());
  }
  
  void testCuttingTool() {}
  void testCuttingToolArchitype() {}
  void testCuttingToolLifeCycle() {}
  void testCuttingItem() {}
  void testUnknownAssetType() {}

protected:
  std::unique_ptr<JsonPrinter> m_printer;

public:
  CPPUNIT_TEST_SUITE(JsonPrinterAssetTest);
  CPPUNIT_TEST(testAssetHeader);
  CPPUNIT_TEST(testCuttingTool);
  CPPUNIT_TEST(testCuttingToolArchitype);
  CPPUNIT_TEST(testCuttingToolLifeCycle);
  CPPUNIT_TEST(testCuttingItem);
  CPPUNIT_TEST(testUnknownAssetType);
  CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterAssetTest);
