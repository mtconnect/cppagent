//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace mtconnect::observation;
using namespace mtconnect::entity;
using namespace mtconnect::sink::rest_sink;
using namespace std::literals;
using namespace chrono_literals;
using namespace date::literals;

class TableTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/data_set.xml", 8, 4, "1.6", 25);
    m_agentId = to_string(getCurrentTimeInSec());

    m_checkpoint = nullptr;
    m_checkpoint = make_unique<Checkpoint>();

    std::map<string, string> attributes;
    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_dataItem1 = device->getDeviceDataItem("wp1");
  }

  void TearDown() override
  {
    m_checkpoint.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Checkpoint> m_checkpoint;
  std::string m_agentId;
  DataItemPtr m_dataItem1;

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

inline DataSetEntry operator"" _E(const char *c, std::size_t) { return DataSetEntry(c); }

TEST_F(TableTest, DataItem)
{
  ASSERT_TRUE(m_dataItem1->isTable());
  ASSERT_TRUE(m_dataItem1->isDataSet());

  ASSERT_EQ((string) "TABLE", m_dataItem1->get<string>("representation"));
  ASSERT_EQ((string) "WorkpieceOffsetTable", m_dataItem1->getObservationName());
}

TEST_F(TableTest, test_simple_table_formats)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc={a=1 b=2.0 c='abc'}", true));

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(3, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
}

TEST_F(TableTest, test_simple_table_formats_with_whitespace)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc={ a=1 b=2.0 c='abc' }", true));

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(3, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
}

TEST_F(TableTest, test_simple_table_formats_with_quotes)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc=' a=1 b=2.0 c='abc''", true));

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(3, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
}

TEST_F(TableTest, test_simple_table_formats_with_double_quotes)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc=\" a=1 b=2.0 c='abc'\"", true));

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(3, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
}

TEST_F(TableTest, test_simple_table_formats_with_nested_braces)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc={ a=1 b=2.0 c={abc}}", true));

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(3, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
}

TEST_F(TableTest, test_simple_table_formats_with_removed_key)
{
  DataSet s1;
  s1.parse("abc={ a=1 b=2.0 c={abc} d= e}", true);

  ASSERT_EQ(1, s1.size());
  const DataSet &abc1 = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(5, abc1.size());
  ASSERT_EQ(1, get<int64_t>(abc1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc1.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc1.find("c"_E)->m_value));
  ASSERT_TRUE(abc1.find("d"_E)->m_removed);
  ASSERT_TRUE(abc1.find("e"_E)->m_removed);
}

TEST_F(TableTest, test_mulitple_entries)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("abc={ a=1 b=2.0 c={abc} d= e} def={x=1.0 y=2.0}", true));
  ASSERT_EQ(2, s1.size());

  const DataSet &abc = get<DataSet>(s1.find("abc"_E)->m_value);
  ASSERT_EQ(5, abc.size());
  ASSERT_EQ(1, get<int64_t>(abc.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(abc.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(abc.find("c"_E)->m_value));
  ASSERT_TRUE(abc.find("d"_E)->m_removed);
  ASSERT_TRUE(abc.find("e"_E)->m_removed);

  const DataSet &def = get<DataSet>(s1.find("def"_E)->m_value);
  ASSERT_EQ(2, def.size());
  ASSERT_EQ(1.0, get<double>(def.find("x"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(def.find("y"_E)->m_value));
}

TEST_F(TableTest, test_removed_table_entry)
{
  DataSet set;
  ASSERT_TRUE(set.parse("abc={ a=1 b=2.0 c={abc d= e}} xxx= yyy def={x=1.0 y=2.0}", true));
  ASSERT_EQ(4, set.size());
  ASSERT_TRUE(set.find("xxx"_E)->m_removed);
  ASSERT_TRUE(set.find("yyy"_E)->m_removed);
}

TEST_F(TableTest, test_malformed_entry)
{
  DataSet set;
  ASSERT_FALSE(set.parse("abc={ a=1 b=2.0 c={abc d= e} def={x=1.0 y=2.0}", true));
  ASSERT_EQ(0, set.size());

  ASSERT_FALSE(set.parse("abc={ a=1 b=2.0 c={abc} d= e} def={x=1.0 y=2.0", true));
  ASSERT_EQ(1, set.size());
}

TEST_F(TableTest, test_non_table_entry_should_fail)
{
  DataSet set;
  ASSERT_FALSE(set.parse("abc={a=1} xx=123 def={x=1.0}", true));
  ASSERT_EQ(1, set.size());
}

TEST_F(TableTest, InitialSet)
{
  string value(
      "G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto ce = Observation::make(m_dataItem1, Properties {{"VALUE", value}}, time, errors);
  ASSERT_EQ(0, errors.size());
  auto set1 = ce->getValue<DataSet>();

  ASSERT_EQ(3, set1.size());
  ASSERT_EQ(3, ce->get<int64_t>("count"));

  auto g531 = get<DataSet>(set1.find("G53.1"_E)->m_value);
  ASSERT_EQ((size_t)3, g531.size());
  ASSERT_EQ(1.0, get<double>(g531.find("X"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(g531.find("Y"_E)->m_value));
  ASSERT_EQ(3.0, get<double>(g531.find("Z"_E)->m_value));

  auto g532 = get<DataSet>(set1.find("G53.2"_E)->m_value);
  ASSERT_EQ((size_t)3, g532.size());
  ASSERT_EQ(4.0, get<double>(g532.find("X"_E)->m_value));
  ASSERT_EQ(5.0, get<double>(g532.find("Y"_E)->m_value));
  ASSERT_EQ(6.0, get<double>(g532.find("Z"_E)->m_value));

  auto g533 = get<DataSet>(set1.find("G53.3"_E)->m_value);
  ASSERT_EQ((size_t)4, g533.size());
  ASSERT_EQ(7.0, get<double>(g533.find("X"_E)->m_value));
  ASSERT_EQ(8.0, get<double>(g533.find("Y"_E)->m_value));
  ASSERT_EQ(9, get<int64_t>(g533.find("Z"_E)->m_value));
  ASSERT_EQ(10.0, get<double>(g533.find("U"_E)->m_value));
}

#define ASSERT_TABLE_ENTRY(doc, var, key, cell, expected)                                   \
  ASSERT_XML_PATH_EQUAL(doc, "//m:" var "/m:Entry[@key='" key "']/m:Cell[@key='" cell "']", \
                        expected)

TEST_F(TableTest, Current)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkpieceOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 "
      "Y=8.0 Z=9 U=10.0}");

  {
    PARSE_XML_RESPONSE("/current");
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

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 "
      "Y=8.0 Z=9 U=11.0}");

  {
    PARSE_XML_RESPONSE("/current");
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
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData(
      "TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 "
      "U=10.0}");

  {
    PARSE_JSON_RESPONSE("/current");

    auto streams = doc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
    ASSERT_EQ(4_S, streams.size());

    json stream;
    for (auto &s : streams)
    {
      auto id = s.at("/ComponentStream/componentId"_json_pointer);
      ASSERT_TRUE(id.is_string());
      if (id.get<string>() == "path1")
      {
        stream = s;
        break;
      }
    }
    ASSERT_TRUE(stream.is_object());

    auto events = stream.at("/ComponentStream/Events"_json_pointer);
    ASSERT_TRUE(events.is_array());
    json offsets;
    for (auto &o : events)
    {
      ASSERT_TRUE(o.is_object());
      auto v = o.begin().key();
      if (v == "WorkpieceOffsetTable")
      {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());

    ASSERT_EQ(3, offsets.at("/WorkpieceOffsetTable/count"_json_pointer).get<int>());

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
  m_agentTestHelper->addAdapter();
  m_agentTestHelper->m_adapter->processData(
      "TIME|wpo|G53.1={X=1.0 Y=2.0 Z=3.0 s='string with space'} G53.2={X=4.0 Y=5.0 Z=6.0} "
      "G53.3={X=7.0 Y=8.0 Z=9 U=10.0}");

  {
    PARSE_JSON_RESPONSE("/current");

    auto streams = doc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
    ASSERT_EQ(4_S, streams.size());

    json stream;
    for (auto &s : streams)
    {
      auto id = s.at("/ComponentStream/componentId"_json_pointer);
      ASSERT_TRUE(id.is_string());
      if (id.get<string>() == "path1")
      {
        stream = s;
        break;
      }
    }
    ASSERT_TRUE(stream.is_object());

    auto events = stream.at("/ComponentStream/Events"_json_pointer);
    ASSERT_TRUE(events.is_array());
    json offsets;
    for (auto &o : events)
    {
      ASSERT_TRUE(o.is_object());
      auto v = o.begin().key();
      if (v == "WorkpieceOffsetTable")
      {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());

    ASSERT_EQ(3, offsets.at("/WorkpieceOffsetTable/count"_json_pointer).get<int>());

    ASSERT_EQ(string("string with space"),
              offsets.at("/WorkpieceOffsetTable/value/G53.1/s"_json_pointer).get<string>());
  }
}

TEST_F(TableTest, XmlCellDefinitions)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:Description",
                          "A Complex Workpiece Offset Table");

    ASSERT_XML_PATH_COUNT(
        doc, "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/m:CellDefinition", 7);

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='X']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='X']@units",
                          "MILLIMETER");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='Y']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='Z']@type",
                          "POSITION");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='A']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='B']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='C']@type",
                          "ANGLE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='C']@units",
                          "DEGREE");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@key='C']/m:Description",
                          "Spindle Angle");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:CellDefinitions/"
                          "m:CellDefinition[@keyType='FEATURE_ID']@type",
                          "UUID");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/"
                          "m:EntryDefinition/m:Description",
                          "Some Pressure thing");
    ASSERT_XML_PATH_COUNT(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/"
                          "m:EntryDefinition/m:CellDefinitions/m:CellDefinition",
                          1);
    ASSERT_XML_PATH_EQUAL(
        doc,
        "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/"
        "m:EntryDefinition[@key='G54']/m:CellDefinitions/m:CellDefinition[@key='P']@units",
        "PASCAL");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/"
                          "m:EntryDefinition/m:CellDefinitions/m:CellDefinition/m:Description",
                          "Pressure of the P");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:Path//m:DataItem[@id='wp1']/m:Definition/m:EntryDefinitions/"
                          "m:EntryDefinition[@keyType='FEATURE_ID']@type",
                          "UUID");
  }
}

TEST_F(TableTest, JsonDefinitionTest)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_JSON_RESPONSE("/probe");

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

    ASSERT_EQ("wpo", wo.at("/name"_json_pointer).get<string>());

    auto d1 = wo.at("/Definition/Description"_json_pointer);
    ASSERT_EQ("A Complex Workpiece Offset Table", d1.get<string>());

    auto cells = wo.at("/Definition/CellDefinitions"_json_pointer);
    ASSERT_TRUE(cells.is_array());

    ASSERT_EQ("MILLIMETER", cells[0].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("POSITION", cells[0].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("X", cells[0].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("MILLIMETER", cells[1].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("POSITION", cells[1].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("Y", cells[1].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("MILLIMETER", cells[2].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("POSITION", cells[2].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("Z", cells[2].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("DEGREE", cells[3].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("ANGLE", cells[3].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("A", cells[3].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("DEGREE", cells[3].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("ANGLE", cells[3].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("A", cells[3].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("DEGREE", cells[4].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("ANGLE", cells[4].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("B", cells[4].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("DEGREE", cells[4].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("ANGLE", cells[4].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("B", cells[4].at("/CellDefinition/key"_json_pointer).get<string>());

    ASSERT_EQ("DEGREE", cells[5].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("ANGLE", cells[5].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("C", cells[5].at("/CellDefinition/key"_json_pointer).get<string>());
    ASSERT_EQ("Spindle Angle",
              cells[5].at("/CellDefinition/Description"_json_pointer).get<string>());

    ASSERT_EQ("FEATURE_ID", cells[6].at("/CellDefinition/keyType"_json_pointer).get<string>());
    ASSERT_EQ("UUID", cells[6].at("/CellDefinition/type"_json_pointer).get<string>());

    auto entries = wo.at("/Definition/EntryDefinitions"_json_pointer);
    ASSERT_TRUE(entries.is_array());

    auto entry = entries[0].at("/EntryDefinition"_json_pointer);
    ASSERT_EQ("Some Pressure thing", entry.at("/Description"_json_pointer).get<string>());
    ASSERT_EQ("G54", entry.at("/key"_json_pointer).get<string>());

    auto ecells = entry.at("/CellDefinitions"_json_pointer);
    ASSERT_EQ("PASCAL", ecells[0].at("/CellDefinition/units"_json_pointer).get<string>());
    ASSERT_EQ("PRESSURE", ecells[0].at("/CellDefinition/type"_json_pointer).get<string>());
    ASSERT_EQ("P", ecells[0].at("/CellDefinition/key"_json_pointer).get<string>());
    ASSERT_EQ("Pressure of the P",
              ecells[0].at("/CellDefinition/Description"_json_pointer).get<string>());

    ASSERT_EQ("FEATURE_ID", entries[2].at("/EntryDefinition/keyType"_json_pointer).get<string>());
    ASSERT_EQ("UUID", entries[2].at("/EntryDefinition/type"_json_pointer).get<string>());
  }
}
