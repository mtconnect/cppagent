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

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/parser/xml_parser.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/utilities.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using json = nlohmann::json;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class JsonPrinterProbeTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_xmlPrinter = std::make_unique<printer::XmlPrinter>("1.5");
    m_printer = std::make_unique<printer::JsonPrinter>(1, true);

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/SimpleDevlce.xml", 8, 4, "1.5", 25);

    // Asset types are registered in the agent.
    m_devices = m_agentTestHelper->m_agent->getDevices();
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
    m_xmlPrinter.reset();
    m_printer.reset();
    m_devices.clear();
  }

  std::unique_ptr<printer::JsonPrinter> m_printer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  std::list<DevicePtr> m_devices;

  std::unique_ptr<printer::XmlPrinter> m_xmlPrinter;
};

TEST_F(JsonPrinterProbeTest, DeviceRootAndDescription)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();

  ASSERT_EQ(string("MTConnectDevices"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectDevices/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(9999, jdoc.at("/MTConnectDevices/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(1024, jdoc.at("/MTConnectDevices/Header/assetBufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(10, jdoc.at("/MTConnectDevices/Header/assetCount"_json_pointer).get<int32_t>());

  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  ASSERT_EQ(2_S, devices.size());

  auto device = devices.at(0).at("/Device"_json_pointer);
  auto device2 = devices.at(1).at("/Device"_json_pointer);

  ASSERT_EQ(string("x872a3490"), device.at("/id"_json_pointer).get<string>());
  ASSERT_EQ(string("SimpleCnc"), device.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("872a3490-bd2d-0136-3eb0-0c85909298d9"),
            device.at("/uuid"_json_pointer).get<string>());

  ASSERT_EQ(string("This is a simple CNC example"),
            device.at("/Description/value"_json_pointer).get<string>());
  ASSERT_EQ(string("MTConnectInstitute"),
            device.at("/Description/manufacturer"_json_pointer).get<string>());
  ASSERT_EQ(string("12"), device.at("/Description/serialNumber"_json_pointer).get<string>());

  ASSERT_EQ(string("This is another simple CNC example"),
            device2.at("/Description/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, TopLevelDataItems)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto device = devices.at(0).at("/Device"_json_pointer);

  auto dataItems = device.at("/DataItems"_json_pointer);
  ASSERT_TRUE(dataItems.is_array());
  ASSERT_EQ(3_S, dataItems.size());

  // Alarm event
  auto avail = dataItems.at(0);
  ASSERT_EQ(string("AVAILABILITY"), avail.at("/DataItem/type"_json_pointer).get<string>());
  ASSERT_EQ(string("EVENT"), avail.at("/DataItem/category"_json_pointer).get<string>());
  ASSERT_EQ(string("d5b078a0"), avail.at("/DataItem/id"_json_pointer).get<string>());
  ASSERT_EQ(string("avail"), avail.at("/DataItem/name"_json_pointer).get<string>());

  // Availability event
  // cout << "\n" << dataItems << endl;
  auto change = dataItems.at(1);

  ASSERT_EQ(string("ASSET_CHANGED"), change.at("/DataItem/type"_json_pointer).get<string>());
  ASSERT_EQ(true, change.at("/DataItem/discrete"_json_pointer).get<bool>());
  ASSERT_EQ(string("EVENT"), change.at("/DataItem/category"_json_pointer).get<string>());
  ASSERT_EQ(string("e4a300e0"), change.at("/DataItem/id"_json_pointer).get<string>());

  auto remove = dataItems.at(2);
  ASSERT_EQ(string("ASSET_REMOVED"), remove.at("/DataItem/type"_json_pointer).get<string>());
  ASSERT_EQ(string("EVENT"), remove.at("/DataItem/category"_json_pointer).get<string>());
  ASSERT_EQ(string("f2df7550"), remove.at("/DataItem/id"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, SubComponents)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto device = devices.at(0).at("/Device"_json_pointer);

  auto components = device.at("/Components"_json_pointer);
  ASSERT_TRUE(components.is_array());
  ASSERT_EQ(3_S, components.size());

  auto axes = components.at(0);
  ASSERT_TRUE(axes.is_object());
  ASSERT_EQ(string("Axes"), axes.begin().key());
  ASSERT_EQ(string("a62a1050"), axes.at("/Axes/id"_json_pointer).get<string>());

  auto subAxes = axes.at("/Axes/Components"_json_pointer);
  ASSERT_TRUE(subAxes.is_array());
  ASSERT_EQ(2_S, subAxes.size());

  auto rotary = subAxes.at(0);
  ASSERT_TRUE(rotary.is_object());
  auto rc = rotary.at("/Linear"_json_pointer);
  ASSERT_TRUE(rc.is_object());
  ASSERT_EQ(string("X1"), rc.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("X"), rc.at("/nativeName"_json_pointer).get<string>());
  ASSERT_EQ(string("e373fec0"), rc.at("/id"_json_pointer).get<string>());

  auto dataItems = rc.at("/DataItems"_json_pointer);
  ASSERT_TRUE(dataItems.is_array());
  ASSERT_EQ(3_S, dataItems.size());

  auto ss = dataItems.at(0).at("/DataItem"_json_pointer);
  ASSERT_TRUE(ss.is_object());
  ASSERT_EQ(string("POSITION"), ss.at("/type"_json_pointer).get<string>());
  ASSERT_EQ(string("MILLIMETER"), ss.at("/units"_json_pointer).get<string>());
  ASSERT_EQ(string("ACTUAL"), ss.at("/subType"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, DataItemConstraints)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto rotary = devices.at(0).at("/Device/Components/0/Axes/Components/1/Rotary"_json_pointer);
  ASSERT_TRUE(rotary.is_object());

  ASSERT_EQ(string("zf476090"), rotary.at("/id"_json_pointer).get<string>());

  auto di = rotary.at("/DataItems/0/DataItem"_json_pointer);
  ASSERT_TRUE(di.is_object());
  ASSERT_EQ(string("ROTARY_MODE"), di.at("/type"_json_pointer).get<string>());

  auto constraint = di.at("/Constraints"_json_pointer);
  ASSERT_TRUE(constraint.is_array());
  ASSERT_EQ(string("SPINDLE"), constraint.at("/0/Value/value"_json_pointer).get<string>());

  auto rv = rotary.at("/DataItems/2/DataItem"_json_pointer);
  ASSERT_TRUE(rv.is_object());
  ASSERT_EQ(string("ROTARY_VELOCITY"), rv.at("/type"_json_pointer).get<string>());
  ASSERT_EQ(string("ACTUAL"), rv.at("/subType"_json_pointer).get<string>());
  auto min = rv.at("/Constraints/0/Minimum/value"_json_pointer);
  ASSERT_TRUE(min.is_number());
  ASSERT_EQ(0.0, min.get<double>());
  auto max = rv.at("/Constraints/1/Maximum/value"_json_pointer);
  ASSERT_TRUE(max.is_number());
  ASSERT_EQ(7000.0, max.get<double>());
}

TEST_F(JsonPrinterProbeTest, DataItemSource)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  // cout << "\n" << doc << endl;
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto rotary = devices.at(0).at("/Device/Components/0/Axes/Components/1/Rotary"_json_pointer);
  ASSERT_TRUE(rotary.is_object());

  auto amp = rotary.at("/DataItems/5/DataItem"_json_pointer);
  ASSERT_TRUE(amp.is_object());
  ASSERT_EQ(string("AMPERAGE"), amp.at("/type"_json_pointer).get<string>());
  ASSERT_EQ(string("CONDITION"), amp.at("/category"_json_pointer).get<string>());
  ASSERT_EQ(string("taa7a0f0"), amp.at("/Source/dataItemId"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, InitialValue)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto path = devices.at(0).at("/Device/Components/1/Controller/Components/0/Path"_json_pointer);
  auto items = path.at("/DataItems"_json_pointer);
  ASSERT_TRUE(items.is_array());

  json count = ::find(items, "/DataItem/id", "d2e9e4a0");
  ASSERT_TRUE(count.is_object());
  ASSERT_EQ(1.0, count.at("/DataItem/InitialValue"_json_pointer).get<double>());
}

TEST_F(JsonPrinterProbeTest, DataItemFilters)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);

  auto electric =
      devices.at(0).at("/Device/Components/2/Systems/Components/0/Electric"_json_pointer);
  ASSERT_TRUE(electric.is_object());

  auto temp = electric.at("/DataItems/0"_json_pointer);
  ASSERT_TRUE(temp.is_object());
  ASSERT_EQ(string("x52ca7e0"), temp.at("/DataItem/id"_json_pointer).get<string>());

  auto filter = temp.at("/DataItem/Filters/0"_json_pointer);
  ASSERT_TRUE(filter.is_object());
  ASSERT_EQ(string("PERIOD"), filter.at("/Filter/type"_json_pointer).get<string>());
  ASSERT_EQ(60.0, filter.at("/Filter/value"_json_pointer).get<double>());

  auto volt = electric.at("/DataItems/1"_json_pointer);
  ASSERT_TRUE(volt.is_object());
  ASSERT_EQ(string("r1e58cf0"), volt.at("/DataItem/id"_json_pointer).get<string>());

  auto filter2 = volt.at("/DataItem/Filters/0"_json_pointer);
  ASSERT_TRUE(filter2.is_object());
  ASSERT_EQ(string("MINIMUM_DELTA"), filter2.at("/Filter/type"_json_pointer).get<string>());
  ASSERT_EQ(10.0, filter2.at("/Filter/value"_json_pointer).get<double>());
}

TEST_F(JsonPrinterProbeTest, Composition)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);

  auto coolant = devices.at(0).at("/Device/Components/2/Systems/Components/1/Coolant"_json_pointer);
  ASSERT_TRUE(coolant.is_object());

  auto comp1 = coolant.at("/Compositions/0/Composition"_json_pointer);
  ASSERT_EQ(string("main"), comp1.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("TANK"), comp1.at("/type"_json_pointer).get<string>());
  ASSERT_EQ(string("t59d1170"), comp1.at("/id"_json_pointer).get<string>());

  auto comp2 = coolant.at("/Compositions/1/Composition"_json_pointer);
  ASSERT_EQ(string("reserve"), comp2.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("TANK"), comp2.at("/type"_json_pointer).get<string>());
  ASSERT_EQ(string("a7973930"), comp2.at("/id"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, Configuration)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto sensorObj = devices.at(0).at(
      "/Device/Components/2/Systems/Components/0/Electric/Components/0"_json_pointer);
  ASSERT_TRUE(sensorObj.is_object());
  auto sensor = sensorObj["Sensor"];
  ASSERT_TRUE(sensor.is_object());

  auto config = sensor.at("/Configuration/SensorConfiguration"_json_pointer);

  ASSERT_EQ(string("23"), config.at("/FirmwareVersion"_json_pointer).get<string>());
  ASSERT_EQ(string("2018-08-12"), config.at("/CalibrationDate"_json_pointer).get<string>());
  ASSERT_EQ(string("1"), config.at("/Channels/0/Channel/number"_json_pointer).get<string>());
  ASSERT_EQ(string("Temperature Probe"),
            config.at("/Channels/0/Channel/Description"_json_pointer).get<string>());
  ASSERT_EQ(string("2018-09-11"),
            config.at("/Channels/0/Channel/CalibrationDate"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, PrintDeviceMTConnectVersion)
{
  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);
  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto device = devices.at(0).at("/Device"_json_pointer);

  ASSERT_EQ(string("1.7"), device.at("/mtconnectVersion"_json_pointer).get<string>());
}

TEST_F(JsonPrinterProbeTest, PrintDataItemRelationships)
{
  auto server = std::make_unique<sink::rest_sink::Server>(m_agentTestHelper->m_ioContext);
  auto cache = std::make_unique<sink::rest_sink::FileCache>();

  m_agentTestHelper->createAgent("/samples/relationship_test.xml", 8, 4, "1.7", 25);
  auto printer = m_agentTestHelper->m_agent->getPrinter("json");
  m_devices = m_agentTestHelper->m_agent->getDevices();
  auto doc = printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);

  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  auto linear = devices.at(1).at("/Device/Components/0/Axes/Components/0/Linear"_json_pointer);
  ASSERT_TRUE(linear.is_object());

  ASSERT_FALSE(printer->getModelChangeTime().empty());
  ASSERT_EQ(printer->getModelChangeTime(),
            jdoc.at("/MTConnectDevices/Header/deviceModelChangeTime"_json_pointer).get<string>());

  auto load = linear.at("/DataItems/4/DataItem"_json_pointer);
  ASSERT_TRUE(load.is_object());
  ASSERT_EQ(string("xlc"), load["id"].get<string>());

  auto dir1 = load.at("/Relationships/0"_json_pointer);
  ASSERT_TRUE(dir1.is_object());
  ASSERT_EQ(string("archie"), dir1.at("/DataItemRelationship/name"_json_pointer));
  ASSERT_EQ(string("LIMIT"), dir1.at("/DataItemRelationship/type"_json_pointer));
  ASSERT_EQ(string("xlcpl"), dir1.at("/DataItemRelationship/idRef"_json_pointer));

  auto dir2 = load.at("/Relationships/1"_json_pointer);
  ASSERT_TRUE(dir2.is_object());
  ASSERT_EQ(string("LIMIT"), dir2.at("/SpecificationRelationship/type"_json_pointer));
  ASSERT_EQ(string("spec1"), dir2.at("/SpecificationRelationship/idRef"_json_pointer));

  auto limits = linear.at("/DataItems/5/DataItem"_json_pointer);
  ASSERT_TRUE(load.is_object());
  ASSERT_EQ(string("xlcpl"), limits["id"].get<string>());

  auto dir3 = limits.at("/Relationships/0"_json_pointer);
  ASSERT_TRUE(dir3.is_object());
  ASSERT_EQ(string("bob"), dir3.at("/DataItemRelationship/name"_json_pointer));
  ASSERT_EQ(string("OBSERVATION"), dir3.at("/DataItemRelationship/type"_json_pointer));
  ASSERT_EQ(string("xlc"), dir3.at("/DataItemRelationship/idRef"_json_pointer));
}

TEST_F(JsonPrinterProbeTest, version_2_with_multiple_devices)
{
  m_printer = std::make_unique<printer::JsonPrinter>(2, true);
  m_agentTestHelper->createAgent("/samples/two_devices.xml", 8, 4, "1.5", 25);
  m_devices = m_agentTestHelper->m_agent->getDevices();

  auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
  auto jdoc = json::parse(doc);

  ASSERT_EQ(2, jdoc["MTConnectDevices"]["jsonVersion"].get<int>());

  auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);
  ASSERT_TRUE(devices.is_object());

  auto device = jdoc.at("/MTConnectDevices/Devices/Device"_json_pointer);
  ASSERT_TRUE(device.is_array());
  ASSERT_EQ(2_S, device.size());

  ASSERT_EQ("device-1", device.at("/0/uuid"_json_pointer));
  ASSERT_EQ("device-2", device.at("/1/uuid"_json_pointer));
}
