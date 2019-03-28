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

#include "component_event.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include "globals.hpp"
#include "checkpoint.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "json_helper.hpp"

#include "test_globals.hpp"

using namespace std;
using namespace mtconnect;
using json = nlohmann::json;


TEST_CLASS(JsonPrinterProbeTest)
{
public:
  void testDeviceRootAndDescription();
  void testTopLevelDataItems();
  void testSubComponents();
  void testDataItemConstraints();
  void testDataItemSource();
  void testInitialValue();
  void testDataItemFilters();
  void testComposition();
  void testConfiguration();
  
  SET_UP();
  TEAR_DOWN();
  
protected:
  std::unique_ptr<JsonPrinter> m_printer;
  std::vector<Device *> m_devices;
  
  std::unique_ptr<XmlParser> m_config;
  std::unique_ptr<XmlPrinter> m_xmlPrinter;
  
  CPPUNIT_TEST_SUITE(JsonPrinterProbeTest);
  CPPUNIT_TEST(testDeviceRootAndDescription);
  CPPUNIT_TEST(testTopLevelDataItems);
  CPPUNIT_TEST(testSubComponents);
  CPPUNIT_TEST(testDataItemConstraints);
  CPPUNIT_TEST(testDataItemSource);
  CPPUNIT_TEST(testInitialValue);
  CPPUNIT_TEST(testDataItemFilters);
  CPPUNIT_TEST(testComposition);
  CPPUNIT_TEST(testConfiguration);
  CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterProbeTest);

void JsonPrinterProbeTest::setUp()
{
  m_xmlPrinter.reset(new XmlPrinter("1.5"));
  m_printer.reset(new JsonPrinter("1.5", true));
  
  m_config.reset(new XmlParser());
  m_devices = m_config->parseFile(PROJECT_ROOT_DIR "/samples/SimpleDevlce.xml", m_xmlPrinter.get());
}

void JsonPrinterProbeTest::tearDown()
{
  m_config.reset();
  m_xmlPrinter.reset();
  m_printer.reset();
  m_devices.clear();
}

void JsonPrinterProbeTest::testDeviceRootAndDescription()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  CPPUNIT_ASSERT_EQUAL(string("MTConnectDevices"), it.key());
  CPPUNIT_ASSERT_EQUAL(123, jdoc.at("/MTConnectDevices/Header/@instanceId"_json_pointer).get<int32_t>());
  CPPUNIT_ASSERT_EQUAL(9999, jdoc.at("/MTConnectDevices/Header/@bufferSize"_json_pointer).get<int32_t>());
  CPPUNIT_ASSERT_EQUAL(1024, jdoc.at("/MTConnectDevices/Header/@assetBufferSize"_json_pointer).get<int32_t>());
  CPPUNIT_ASSERT_EQUAL(10, jdoc.at("/MTConnectDevices/Header/@assetCount"_json_pointer).get<int32_t>());
  
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(2_S, devices.size());
  
  auto device = devices.at(0).at("/Device"_json_pointer);
  auto device2 = devices.at(1).at("/Device"_json_pointer);
  
  CPPUNIT_ASSERT_EQUAL(string("x872a3490"), device.at("/@id"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("SimpleCnc"), device.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("872a3490-bd2d-0136-3eb0-0c85909298d9"), device.at("/@uuid"_json_pointer).get<string>());
  
  CPPUNIT_ASSERT_EQUAL(string("This is a simple CNC example"), device.at("/Description/#text"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("MTConnectInstitute"), device.at("/Description/@manufacturer"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("12"), device.at("/Description/@serialNumber"_json_pointer).get<string>());
  
  CPPUNIT_ASSERT_EQUAL(string("This is another simple CNC example"), device2.at("/Description/#text"_json_pointer).get<string>());
}

void JsonPrinterProbeTest::testTopLevelDataItems()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  //cout << "\n" << doc << endl;
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto device = devices.at(0).at("/Device"_json_pointer);
  
  auto dataItems = device.at("/DataItems"_json_pointer);
  CPPUNIT_ASSERT(dataItems.is_array());
  CPPUNIT_ASSERT_EQUAL(3_S, dataItems.size());
  
  // Alarm event
  auto avail = dataItems.at(0);
  CPPUNIT_ASSERT_EQUAL(string("AVAILABILITY"), avail.at("/DataItem/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("EVENT"), avail.at("/DataItem/@category"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("d5b078a0"), avail.at("/DataItem/@id"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("avail"), avail.at("/DataItem/@name"_json_pointer).get<string>());
  
  // Availability event
  auto change = dataItems.at(1);
  CPPUNIT_ASSERT_EQUAL(string("ASSET_CHANGED"), change.at("/DataItem/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("EVENT"), change.at("/DataItem/@category"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("e4a300e0"), change.at("/DataItem/@id"_json_pointer).get<string>());
  
  auto remove = dataItems.at(2);
  CPPUNIT_ASSERT_EQUAL(string("ASSET_REMOVED"), remove.at("/DataItem/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("EVENT"), remove.at("/DataItem/@category"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("f2df7550"), remove.at("/DataItem/@id"_json_pointer).get<string>());
}

void JsonPrinterProbeTest::testSubComponents()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto device = devices.at(0).at("/Device"_json_pointer);
  
  auto components = device.at("/Components"_json_pointer);
  CPPUNIT_ASSERT(components.is_array());
  CPPUNIT_ASSERT_EQUAL(3_S, components.size());
  
  auto axes = components.at(0);
  CPPUNIT_ASSERT(axes.is_object());
  CPPUNIT_ASSERT_EQUAL(string("Axes"), axes.begin().key());
  CPPUNIT_ASSERT_EQUAL(string("a62a1050"), axes.at("/Axes/@id"_json_pointer).get<string>());
  
  auto subAxes = axes.at("/Axes/Components"_json_pointer);
  CPPUNIT_ASSERT(subAxes.is_array());
  CPPUNIT_ASSERT_EQUAL(2_S, subAxes.size());
  
  auto rotary = subAxes.at(0);
  CPPUNIT_ASSERT(rotary.is_object());
  auto rc = rotary.at("/Linear"_json_pointer);
  CPPUNIT_ASSERT(rc.is_object());
  CPPUNIT_ASSERT_EQUAL(string("X1"), rc.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("X"), rc.at("/@nativeName"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("e373fec0"), rc.at("/@id"_json_pointer).get<string>());
  
  auto dataItems = rc.at("/DataItems"_json_pointer);
  CPPUNIT_ASSERT(dataItems.is_array());
  CPPUNIT_ASSERT_EQUAL(3_S, dataItems.size());
  
  auto ss = dataItems.at(0).at("/DataItem"_json_pointer);
  CPPUNIT_ASSERT(ss.is_object());
  CPPUNIT_ASSERT_EQUAL(string("POSITION"), ss.at("/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("MILLIMETER"), ss.at("/@units"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("ACTUAL"), ss.at("/@subType"_json_pointer).get<string>());
}

void JsonPrinterProbeTest::testDataItemConstraints()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto rotary = devices.at(0).at("/Device/Components/0/Axes/Components/1/Rotary"_json_pointer);
  CPPUNIT_ASSERT(rotary.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("zf476090"), rotary.at("/@id"_json_pointer).get<string>());
  
  auto di = rotary.at("/DataItems/0/DataItem"_json_pointer);
  CPPUNIT_ASSERT(di.is_object());
  CPPUNIT_ASSERT_EQUAL(string("ROTARY_MODE"), di.at("/@type"_json_pointer).get<string>());
  
  auto constraint = di.at("/Constraints/Value"_json_pointer);
  CPPUNIT_ASSERT(constraint.is_array());
  CPPUNIT_ASSERT_EQUAL(string("SPINDLE"), constraint.at(0).get<string>());
  
  auto rv = rotary.at("/DataItems/2/DataItem"_json_pointer);
  CPPUNIT_ASSERT(rv.is_object());
  CPPUNIT_ASSERT_EQUAL(string("ROTARY_VELOCITY"), rv.at("/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("ACTUAL"), rv.at("/@subType"_json_pointer).get<string>());
  auto min = rv.at("/Constraints/Minimum"_json_pointer);
  CPPUNIT_ASSERT(min.is_number());
  CPPUNIT_ASSERT_EQUAL(0.0, min.get<double>());
  auto max = rv.at("/Constraints/Maximum"_json_pointer);
  CPPUNIT_ASSERT(max.is_number());
  CPPUNIT_ASSERT_EQUAL(7000.0, max.get<double>());
}

void JsonPrinterProbeTest::testDataItemSource()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  //cout << "\n" << doc << endl;
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto rotary = devices.at(0).at("/Device/Components/0/Axes/Components/1/Rotary"_json_pointer);
  CPPUNIT_ASSERT(rotary.is_object());
  
  auto amp = rotary.at("/DataItems/5/DataItem"_json_pointer);
  CPPUNIT_ASSERT(amp.is_object());
  CPPUNIT_ASSERT_EQUAL(string("AMPERAGE"), amp.at("/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("CONDITION"), amp.at("/@category"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("taa7a0f0"), amp.at("/Source/@dataItemId"_json_pointer).get<string>());
}

void JsonPrinterProbeTest::testInitialValue()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto path = devices.at(0).at("/Device/Components/1/Controller/Components/0/Path"_json_pointer);
  auto items = path.at("/DataItems"_json_pointer);
  CPPUNIT_ASSERT(items.is_array());
  
  json count = ::find(items, "/DataItem/@id", "d2e9e4a0");
  CPPUNIT_ASSERT(count.is_object());
  CPPUNIT_ASSERT_EQUAL(1.0, count.at("/DataItem/InitialValue"_json_pointer).get<double>());
}

void JsonPrinterProbeTest::testDataItemFilters()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  
  auto electric = devices.at(0).at("/Device/Components/2/Systems/Components/0/Electric"_json_pointer);
  CPPUNIT_ASSERT(electric.is_object());
  
  auto temp = electric.at("/DataItems/0"_json_pointer);
  CPPUNIT_ASSERT(temp.is_object());
  CPPUNIT_ASSERT_EQUAL(string("x52ca7e0"), temp.at("/DataItem/@id"_json_pointer).get<string>());
  
  auto filter = temp.at("/DataItem/Filters/0"_json_pointer);
  CPPUNIT_ASSERT(filter.is_object());
  CPPUNIT_ASSERT_EQUAL(string("PERIOD"), filter.at("/Filter/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(60.0, filter.at("/Filter/Value"_json_pointer).get<double>());
  
  auto volt = electric.at("/DataItems/1"_json_pointer);
  CPPUNIT_ASSERT(volt.is_object());
  CPPUNIT_ASSERT_EQUAL(string("r1e58cf0"), volt.at("/DataItem/@id"_json_pointer).get<string>());
  
  auto filter2 = volt.at("/DataItem/Filters/0"_json_pointer);
  CPPUNIT_ASSERT(filter2.is_object());
  CPPUNIT_ASSERT_EQUAL(string("MINIMUM_DELTA"), filter2.at("/Filter/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(10.0, filter2.at("/Filter/Value"_json_pointer).get<double>());
}

void JsonPrinterProbeTest::testComposition()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  
  auto coolant = devices.at(0).at("/Device/Components/2/Systems/Components/1/Coolant"_json_pointer);
  CPPUNIT_ASSERT(coolant.is_object());
  
  auto comp1 = coolant.at("/Compositions/0/Composition"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(string("main"), comp1.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TANK"), comp1.at("/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("t59d1170"), comp1.at("/@id"_json_pointer).get<string>());
  
  auto comp2 = coolant.at("/Compositions/1/Composition"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(string("reserve"), comp2.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TANK"), comp2.at("/@type"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("a7973930"), comp2.at("/@id"_json_pointer).get<string>());
}

void JsonPrinterProbeTest::testConfiguration()
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto sensorObj = devices.at(0).at("/Device/Components/2/Systems/Components/0/Electric/Components/0"_json_pointer);
  CPPUNIT_ASSERT(sensorObj.is_object());
  auto sensor = sensorObj["Sensor"];
  CPPUNIT_ASSERT(sensor.is_object());
  
  auto config = sensor.at("/Configuration/SensorConfiguration"_json_pointer);
  
  CPPUNIT_ASSERT_EQUAL(string("23"), config.at("/FirmwareVersion"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("2018-08-12"), config.at("/CalibrationDate"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("1"), config.at("/Channels/0/Channel/@number"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("Temperature Probe"), config.at("/Channels/0/Channel/Description"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("2018-09-11"), config.at("/Channels/0/Channel/CalibrationDate"_json_pointer).get<string>());
}

      
