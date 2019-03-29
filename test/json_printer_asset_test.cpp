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
    m_parser.reset(new XmlParser());
  }
  
  TEAR_DOWN()
  {
    m_printer.reset();
    m_parser.reset();
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
  
  void testCuttingTool()
  {
    auto xml = getFile(PROJECT_ROOT_DIR "/test/asset1.xml");
    AssetPtr asset = m_parser->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", xml);
    vector<AssetPtr> assetList = { asset };
    auto doc = m_printer->printAssets(123, 1024, 10, assetList);
    auto jdoc = json::parse(doc);
    
    auto assets = jdoc.at("/MTConnectAssets/Assets"_json_pointer);
    CPPUNIT_ASSERT(assets.is_array());
    CPPUNIT_ASSERT_EQUAL(1_S, assets.size());
    
    auto cuttingTool = assets.at(0);
    CPPUNIT_ASSERT_EQUAL(string("1"), cuttingTool.at("/CuttingTool/@serialNumber"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("KSSP300R4SD43L240"), cuttingTool.at("/CuttingTool/@toolId"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("KSSP300R4SD43L240.1"), cuttingTool.at("/CuttingTool/@assetId"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("2011-05-11T13:55:22"), cuttingTool.at("/CuttingTool/@timestamp"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("KMT"), cuttingTool.at("/CuttingTool/@manufacturers/0"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("Parlec"), cuttingTool.at("/CuttingTool/@manufacturers/1"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("Cutting tool ..."), cuttingTool.at("/CuttingTool/Description"_json_pointer).get<string>());
  }
  
  void testCuttingToolLifeCycle()
  {
    auto xml = getFile(PROJECT_ROOT_DIR "/test/asset1.xml");
    AssetPtr asset = m_parser->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", xml);
    vector<AssetPtr> assetList = { asset };
    auto doc = m_printer->printAssets(123, 1024, 10, assetList);
    auto jdoc = json::parse(doc);
    
    auto assets = jdoc.at("/MTConnectAssets/Assets"_json_pointer);
    CPPUNIT_ASSERT(assets.is_array());
    CPPUNIT_ASSERT_EQUAL(1_S, assets.size());
    
    auto cuttingTool = assets.at(0);
    cout << cuttingTool.dump(2) << endl;
    auto lifeCycle = cuttingTool.at("/CuttingTool/CuttingToolLifeCycle"_json_pointer);
    CPPUNIT_ASSERT(lifeCycle.is_object());
    
    auto status = lifeCycle.at("/CutterStatus"_json_pointer);
    CPPUNIT_ASSERT(status.is_array());
    CPPUNIT_ASSERT_EQUAL(string("NEW"), status.at(0).get<string>());
    
    auto toolLife = lifeCycle.at("/ToolLife"_json_pointer);
    CPPUNIT_ASSERT(toolLife.is_array());
    auto life = toolLife.at(0);
    CPPUNIT_ASSERT(life.is_object());
    CPPUNIT_ASSERT_EQUAL(string("PART_COUNT"), life.at("/@type"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("DOWN"), life.at("/@countDirection"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(300, life.at("/@maximum"_json_pointer).get<int32_t>());
    CPPUNIT_ASSERT_EQUAL(200, life.at("/Value"_json_pointer).get<int32_t>());

    auto speed = lifeCycle.at("/ProcessSpindleSpeed"_json_pointer);
    CPPUNIT_ASSERT_EQUAL(13300.0, speed.at("/@maximum"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(605.0, speed.at("/@nominal"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(10000.0, speed.at("/Value"_json_pointer).get<double>());

    auto feed = lifeCycle.at("/ProcessFeedRate"_json_pointer);
    CPPUNIT_ASSERT_EQUAL(222.0, feed.at("/Value"_json_pointer).get<double>());
  }
  
  void testCuttingMeasurements()
  {
    auto xml = getFile(PROJECT_ROOT_DIR "/test/asset1.xml");
    AssetPtr asset = m_parser->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", xml);
    vector<AssetPtr> assetList = { asset };
    auto doc = m_printer->printAssets(123, 1024, 10, assetList);
    auto jdoc = json::parse(doc);
    
    auto lifeCycle = jdoc.at("/MTConnectAssets/Assets/0/CuttingTool/CuttingToolLifeCycle"_json_pointer);
    CPPUNIT_ASSERT(lifeCycle.is_object());
    
    auto measurements = lifeCycle.at("/Measurements"_json_pointer);
    CPPUNIT_ASSERT(measurements.is_array());
    CPPUNIT_ASSERT_EQUAL(7_S, measurements.size());
    
    auto diameter = measurements.at(0);
    CPPUNIT_ASSERT(diameter.is_object());
    CPPUNIT_ASSERT_EQUAL("BDX", diameter.at("/BodyDiameterMax/@code"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(73.25, diameter.at("/BodyDiameterMax/Value"_json_pointer).get<double>());

    auto length = measurements.at(1);
    CPPUNIT_ASSERT(length.is_object());
    CPPUNIT_ASSERT_EQUAL("LF", length.at("/BodyLengthMax/@code"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(120.65, length.at("/BodyLengthMax/@nominal"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(120.404, length.at("/BodyLengthMax/@minimum"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(120.904, length.at("/BodyLengthMax/@maximum"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(120.65, length.at("/BodyLengthMax/Value"_json_pointer).get<double>());
  }
  
  void testCuttingItem()
  {
    auto xml = getFile(PROJECT_ROOT_DIR "/test/asset1.xml");
    AssetPtr asset = m_parser->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", xml);
    vector<AssetPtr> assetList = { asset };
    auto doc = m_printer->printAssets(123, 1024, 10, assetList);
    auto jdoc = json::parse(doc);
    
    auto items = jdoc.at("/MTConnectAssets/Assets/0/"
                         "CuttingTool/CuttingToolLifeCycle/"
                         "CuttingItems"_json_pointer);
    CPPUNIT_ASSERT(items.is_array());
    CPPUNIT_ASSERT_EQUAL(6_S, items.size());
    
    auto item = items.at(0);
    CPPUNIT_ASSERT(item.is_object());
    
    CPPUNIT_ASSERT_EQUAL("1-4", item.at("/CuttingItem/@indices"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("SDET43PDER8GB"), item.at("/CuttingItem/@itemId"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("KC725M"), item.at("/CuttingItem/@grade"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("KMT"), item.at("/CuttingItem/@manufacturers/0"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(string("FLANGE: 1-4, ROW: 1"), item.at("/CuttingItem/Locus/Value"_json_pointer).get<string>());
    
    auto measurements = item.at("/CuttingItem/Measurements"_json_pointer);
    CPPUNIT_ASSERT(measurements.is_array());
    CPPUNIT_ASSERT_EQUAL(4_S, measurements.size());
    
    CPPUNIT_ASSERT_EQUAL("RE", measurements.at("/0/CornerRadius/@code"_json_pointer).get<string>());
    CPPUNIT_ASSERT_EQUAL(0.8, measurements.at("/0/CornerRadius/@nominal"_json_pointer).get<double>());
    CPPUNIT_ASSERT_EQUAL(0.8, measurements.at("/0/CornerRadius/Value"_json_pointer).get<double>());
  }
  
  void testCuttingToolArchitype() {}
  void testUnknownAssetType() {}

protected:
  std::unique_ptr<JsonPrinter> m_printer;
  std::unique_ptr<XmlParser> m_parser;

public:
  CPPUNIT_TEST_SUITE(JsonPrinterAssetTest);
  CPPUNIT_TEST(testAssetHeader);
  CPPUNIT_TEST(testCuttingTool);
  CPPUNIT_TEST(testCuttingToolLifeCycle);
  CPPUNIT_TEST(testCuttingMeasurements);
  CPPUNIT_TEST(testCuttingItem);
  CPPUNIT_TEST(testCuttingToolArchitype);
  CPPUNIT_TEST(testUnknownAssetType);
  CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterAssetTest);
