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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class TableTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_checkpoint = nullptr;
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/data_set.xml", 4, 4, "1.5");
    m_agentId = int64ToString(getCurrentTimeInSec());
    m_adapter = nullptr;
    m_checkpoint = make_unique<Checkpoint>();

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();

    std::map<string, string> attributes;
    auto device = m_agent->getDeviceByName("LinuxCNC");
    m_dataItem1 = device->getDeviceDataItem("wp1");
  }

  void TearDown() override
  {
    m_agent.reset();
    m_checkpoint.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Checkpoint> m_checkpoint;
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  DataItem *m_dataItem1{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

inline DataSetEntry operator"" _E(const char *c, std::size_t)
{
  return DataSetEntry(c);
}

TEST_F(TableTest, DataItem)
{
  ASSERT_TRUE(m_dataItem1->isTable());
  ASSERT_TRUE(m_dataItem1->isDataSet());
  auto &attrs = m_dataItem1->getAttributes();

  ASSERT_EQ((string) "TABLE", attrs.at("representation"));
  ASSERT_EQ((string) "WorkpieceOffsetTable", m_dataItem1->getElementName());
}

TEST_F(TableTest, InitialSet)
{
  string value("G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");
  auto ce = new Observation(*m_dataItem1, 2, "time", value);

  ASSERT_EQ((size_t)3, ce->getDataSet().size());
  auto &al = ce->getAttributes();
  std::map<string, string> attrs;

  for (const auto &attr : al)
    attrs[attr.first] = attr.second;

  ASSERT_EQ((string) "3", attrs.at("count"));

  auto set1 = ce->getDataSet();
  auto g531 = get<DataSet>(set1.find("G53.1"_E)->m_value);
  ASSERT_EQ((size_t) 3, g531.size());
  ASSERT_EQ(1.0, get<double>(g531.find("X"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(g531.find("Y"_E)->m_value));
  ASSERT_EQ(3.0, get<double>(g531.find("Z"_E)->m_value));

  auto set2 = ce->getDataSet();
  auto g532 = get<DataSet>(set1.find("G53.2"_E)->m_value);
  ASSERT_EQ((size_t) 3, g532.size());
  ASSERT_EQ(4.0, get<double>(g532.find("X"_E)->m_value));
  ASSERT_EQ(5.0, get<double>(g532.find("Y"_E)->m_value));
  ASSERT_EQ(6.0, get<double>(g532.find("Z"_E)->m_value));

  auto set3 = ce->getDataSet();
  auto g533 = get<DataSet>(set1.find("G53.3"_E)->m_value);
  ASSERT_EQ((size_t) 4, g533.size());
  ASSERT_EQ(7.0, get<double>(g533.find("X"_E)->m_value));
  ASSERT_EQ(8.0, get<double>(g533.find("Y"_E)->m_value));
  ASSERT_EQ(9, get<int64_t>(g533.find("Z"_E)->m_value));
  ASSERT_EQ(10.0, get<double>(g533.find("U"_E)->m_value));
}

#define ASSERT_TABLE_ENTRY(doc, var, key, cell, expected) \
  ASSERT_XML_PATH_EQUAL(doc, "//m:" var "/m:Entry[@key='" key "']/m:Cell[@key='" cell "']", expected)

TEST_F(TableTest, Current)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkpieceOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
  }

  m_adapter->processData("TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkpieceOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "3");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "X", "1");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "Y", "2");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "Z", "3");
    
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "X", "4");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "Y", "5");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "Z", "6");
    
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "X", "7");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "Y", "8");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "Z", "9");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "U", "10");
  }
  
  m_adapter->processData("TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=11.0}");

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkpieceOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "3");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "X", "1");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "Y", "2");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.1", "Z", "3");
    
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "X", "4");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "Y", "5");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.2", "Z", "6");
    
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "X", "7");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "Y", "8");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "Z", "9");
    ASSERT_TABLE_ENTRY(doc, "WorkpieceOffsetTable[@dataItemId='wp1']", "G53.3", "U", "11");
  }

}

TEST_F(TableTest, JsonCurrent)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);
  
  m_agentTestHelper->m_path = "/current";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  m_adapter->processData("TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");

  {
    PARSE_JSON_RESPONSE;
    
    auto streams = doc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
    ASSERT_EQ(4_S, streams.size());
    
    json stream;
    for (auto &s : streams) {
      auto id = s.at("/ComponentStream/componentId"_json_pointer);
      ASSERT_TRUE(id.is_string());
      if (id.get<string>() == "path1") {
        stream = s;
        break;
      }
    }
    ASSERT_TRUE(stream.is_object());

    auto events = stream.at("/ComponentStream/Events"_json_pointer);
    ASSERT_TRUE(events.is_array());
    json offsets;
    for (auto &o : events) {
      ASSERT_TRUE(o.is_object());
      auto v = o.begin().key();
      if (v == "WorkpieceOffsetTable") {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());
    
    ASSERT_EQ(string("3"), offsets.at("/WorkpieceOffsetTable/count"_json_pointer).get<string>());

    ASSERT_EQ(1.0, offsets.at("/WorkpieceOffsetTable/value/G53.1/X"_json_pointer).get<double>());
    ASSERT_EQ(2.0, offsets.at("/WorkpieceOffsetTable/value/G53.1/Y"_json_pointer).get<double>());
    ASSERT_EQ(3.0, offsets.at("/WorkpieceOffsetTable/value/G53.1/Z"_json_pointer).get<double>());
    ASSERT_EQ(4.0, offsets.at("/WorkpieceOffsetTable/value/G53.2/X"_json_pointer).get<double>());
    ASSERT_EQ(5.0, offsets.at("/WorkpieceOffsetTable/value/G53.2/Y"_json_pointer).get<double>());
    ASSERT_EQ(6.0, offsets.at("/WorkpieceOffsetTable/value/G53.2/Z"_json_pointer).get<double>());
    ASSERT_EQ(7.0, offsets.at("/WorkpieceOffsetTable/value/G53.3/X"_json_pointer).get<double>());
    ASSERT_EQ(8.0, offsets.at("/WorkpieceOffsetTable/value/G53.3/Y"_json_pointer).get<double>());
    ASSERT_EQ(9, offsets.at("/WorkpieceOffsetTable/value/G53.3/Z"_json_pointer).get<int64_t>());
    ASSERT_EQ(10.0, offsets.at("/WorkpieceOffsetTable/value/G53.3/U"_json_pointer).get<double>());

  }
}

TEST_F(TableTest, JsonCurrentText)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);
  
  m_agentTestHelper->m_path = "/current";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  m_adapter->processData("TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0 s='string with space'} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");
  
  {
    PARSE_JSON_RESPONSE;
    
    auto streams = doc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
    ASSERT_EQ(4_S, streams.size());
    
    json stream;
    for (auto &s : streams) {
      auto id = s.at("/ComponentStream/componentId"_json_pointer);
      ASSERT_TRUE(id.is_string());
      if (id.get<string>() == "path1") {
        stream = s;
        break;
      }
    }
    ASSERT_TRUE(stream.is_object());
    
    auto events = stream.at("/ComponentStream/Events"_json_pointer);
    ASSERT_TRUE(events.is_array());
    json offsets;
    for (auto &o : events) {
      ASSERT_TRUE(o.is_object());
      auto v = o.begin().key();
      if (v == "WorkpieceOffsetTable") {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());
    
    ASSERT_EQ(string("3"), offsets.at("/WorkpieceOffsetTable/count"_json_pointer).get<string>());
    
    ASSERT_EQ(string("string with space"), offsets.at("/WorkpieceOffsetTable/value/G53.1/s"_json_pointer).get<string>());
    
  }
}


TEST_F(TableTest, XmlCellDefinitions)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/probe";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:Description",
                          "A Complex Workpiece Offset Table");
    
    ASSERT_XML_PATH_COUNT(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition", 7);

    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='X']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='X']@units",
                          "MILLIMETER");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='Y']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='Z']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='A']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='B']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='C']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='C']@units",
                          "DEGREE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@key='C']/m:Description",
                          "Spindle Angle");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition[@keyType='FEATURE_ID']@type", "UUID");

    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/m:EntryDefinition/m:Description",
                          "Some Pressure thing");
    ASSERT_XML_PATH_COUNT(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/m:EntryDefinition/m:CellDefinitions/m:CellDefinition", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/m:EntryDefinition[@key='G54']/m:CellDefinitions/m:CellDefinition[@key='P']@units",
                          "PASCAL");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/m:EntryDefinition/m:CellDefinitions/m:CellDefinition/m:Description",
                          "Pressure of the P");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/m:EntryDefinition[@keyType='FEATURE_ID']@type", "UUID");

  }

}

TEST_F(TableTest, JsonDefinitionTest)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);
  
  m_agentTestHelper->m_path = "/probe";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  {
    PARSE_JSON_RESPONSE;
    
    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto components = device.at("/Components"_json_pointer);
    ASSERT_TRUE(components.is_array());
    ASSERT_EQ(3_S, components.size());

    auto controller = components.at(1);
    ASSERT_TRUE(controller.is_object());
    ASSERT_EQ(string("Controller"), controller.begin().key());

    auto paths = controller.at("/Controller/Components"_json_pointer);
    ASSERT_TRUE(components.is_array());
    ASSERT_EQ(3_S, components.size());

    auto path = paths.at(0);
    ASSERT_TRUE(path.is_object());
    ASSERT_EQ(string("Path"), path.begin().key());

    auto dataItems = path.at("/Path/DataItems"_json_pointer);
    ASSERT_TRUE(dataItems.is_array());
    ASSERT_EQ(7_S, dataItems.size());

    auto offset = dataItems.at(6);
    ASSERT_TRUE(offset.is_object());
    ASSERT_EQ(string("DataItem"), offset.begin().key());
    auto wo = offset.at("/DataItem"_json_pointer);
    
    ASSERT_EQ(string("wpo"), wo.at("/name"_json_pointer).get<string>());
    
    //cout << "\n" << offset << endl;
    //cout << "\n" << wo << endl;

    auto d1 = wo.at("/Definition/Description"_json_pointer);
    ASSERT_EQ(string("A Complex Workpiece Offset Table"), d1.get<string>());

    auto cells = wo.at("/Definition/CellDefinitions"_json_pointer);
    ASSERT_TRUE(cells.is_object());

    ASSERT_EQ(string("MILLIMETER"), cells.at("/X/units"_json_pointer).get<string>());
    ASSERT_EQ(string("POSITION"), cells.at("/X/type"_json_pointer).get<string>());

    ASSERT_EQ(string("MILLIMETER"), cells.at("/Y/units"_json_pointer).get<string>());
    ASSERT_EQ(string("POSITION"), cells.at("/Y/type"_json_pointer).get<string>());

    ASSERT_EQ(string("MILLIMETER"), cells.at("/Z/units"_json_pointer).get<string>());
    ASSERT_EQ(string("POSITION"), cells.at("/Z/type"_json_pointer).get<string>());

    ASSERT_EQ(string("DEGREE"), cells.at("/A/units"_json_pointer).get<string>());
    ASSERT_EQ(string("ANGLE"), cells.at("/A/type"_json_pointer).get<string>());

    ASSERT_EQ(string("DEGREE"), cells.at("/B/units"_json_pointer).get<string>());
    ASSERT_EQ(string("ANGLE"), cells.at("/B/type"_json_pointer).get<string>());

    ASSERT_EQ(string("DEGREE"), cells.at("/C/units"_json_pointer).get<string>());
    ASSERT_EQ(string("ANGLE"), cells.at("/C/type"_json_pointer).get<string>());
    ASSERT_EQ(string("Spindle Angle"), cells.at("/C/Description"_json_pointer).get<string>());

    ASSERT_EQ(string("FEATURE_ID"), cells.at("/./keyType"_json_pointer).get<string>());
    ASSERT_EQ(string("UUID"), cells.at("/./type"_json_pointer).get<string>());

    
    auto entries = wo.at("/Definition/EntryDefinitions"_json_pointer);
    ASSERT_TRUE(entries.is_object());

    ASSERT_EQ(string("Some Pressure thing"), entries.at("/G54/Description"_json_pointer).get<string>());

    ASSERT_EQ(string("PASCAL"), entries.at("/G54/CellDefinitions/P/units"_json_pointer).get<string>());
    ASSERT_EQ(string("PRESSURE"), entries.at("/G54/CellDefinitions/P/type"_json_pointer).get<string>());
    ASSERT_EQ(string("Pressure of the P"), entries.at("/G54/CellDefinitions/P/Description"_json_pointer).get<string>());
    ASSERT_EQ(string("FEATURE_ID"), entries.at("/./keyType"_json_pointer).get<string>());
    ASSERT_EQ(string("UUID"), entries.at("/./type"_json_pointer).get<string>());

  }
}
