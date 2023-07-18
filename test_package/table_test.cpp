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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::buffer;
using namespace mtconnect::source::adapter;
using namespace mtconnect::observation;
using namespace mtconnect::entity;
using namespace mtconnect::sink::rest_sink;
using namespace std::literals;
using namespace chrono_literals;
using namespace date::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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
  ASSERT_EQ((string) "WorkOffsetTable", m_dataItem1->getObservationName());
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
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 "
      "Y=8.0 Z=9 U=10.0}");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "3");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "X", "1");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "Y", "2");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "Z", "3");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "X", "4");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "Y", "5");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "Z", "6");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "X", "7");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "Y", "8");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "Z", "9");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "U", "10");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|G53.1={X=1.0 Y=2.0 Z=3.0} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 "
      "Y=8.0 Z=9 U=11.0}");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "3");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "X", "1");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "Y", "2");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.1", "Z", "3");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "X", "4");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "Y", "5");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.2", "Z", "6");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "X", "7");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "Y", "8");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "Z", "9");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "G53.3", "U", "11");
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
      if (v == "WorkOffsetTable")
      {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());

    ASSERT_EQ(3, offsets.at("/WorkOffsetTable/count"_json_pointer).get<int>());

    ASSERT_EQ(1.0, offsets.at("/WorkOffsetTable/value/G53.1/X"_json_pointer).get<double>());
    ASSERT_EQ(2.0, offsets.at("/WorkOffsetTable/value/G53.1/Y"_json_pointer).get<double>());
    ASSERT_EQ(3.0, offsets.at("/WorkOffsetTable/value/G53.1/Z"_json_pointer).get<double>());
    ASSERT_EQ(4.0, offsets.at("/WorkOffsetTable/value/G53.2/X"_json_pointer).get<double>());
    ASSERT_EQ(5.0, offsets.at("/WorkOffsetTable/value/G53.2/Y"_json_pointer).get<double>());
    ASSERT_EQ(6.0, offsets.at("/WorkOffsetTable/value/G53.2/Z"_json_pointer).get<double>());
    ASSERT_EQ(7.0, offsets.at("/WorkOffsetTable/value/G53.3/X"_json_pointer).get<double>());
    ASSERT_EQ(8.0, offsets.at("/WorkOffsetTable/value/G53.3/Y"_json_pointer).get<double>());
    ASSERT_EQ(9, offsets.at("/WorkOffsetTable/value/G53.3/Z"_json_pointer).get<int64_t>());
    ASSERT_EQ(10.0, offsets.at("/WorkOffsetTable/value/G53.3/U"_json_pointer).get<double>());
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
      if (v == "WorkOffsetTable")
      {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());

    ASSERT_EQ(3, offsets.at("/WorkOffsetTable/count"_json_pointer).get<int>());

    ASSERT_EQ(string("string with space"),
              offsets.at("/WorkOffsetTable/value/G53.1/s"_json_pointer).get<string>());
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

TEST_F(TableTest, shoud_correctly_parse_data_with_colon)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']@count", "0");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|A0={NAME='CHECK LINEAR GUIDE LUB-OIL LEVEL' VALUE=22748038 "
      "TARGET=0 LAST_SERVICE_DATE=2022-04-06T04:00:00.0000Z} A1={NAME='CHECK SPINDLE LUB-OIL "
      "LEVEL' VALUE=8954 TARGET=22676400 LAST_SERVICE_DATE=2022-04-07T04:00:00.0000Z} "
      "A2={NAME='CHECK COOLANT LEVEL' VALUE=22751515 TARGET=0 "
      "LAST_SERVICE_DATE=2021-07-14T04:00:00.0000Z} A3={NAME='CHECK SPINDLE COOLANT LEVEL' "
      "VALUE=27098873 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A4={NAME='CHECK "
      "HYDRAULIC UNITOIL LEVEL' VALUE=27098872 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A5={NAME='CLEAN COOLANT FILTER' VALUE=27098871 "
      "TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A6={NAME='CHECK HYDRAULIC UNIT "
      "PRESSURE' VALUE=27098889 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} "
      "A7={NAME='CHECK AIR PRESSURE' VALUE=27098890 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A8={NAME='CLEAN CHIPS FROM WAY COVERS' "
      "VALUE=27098892 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A9={NAME='CHECK CHIP "
      "LEVEL IN CHIP BUCKET' VALUE=27098893 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} "
      "A10={NAME='CLEAN CNC & CHILLER AIR FILTERS' VALUE=27098895 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z}");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "11");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@resetTriggered",
                          nullptr);

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "NAME",
                       "CHECK LINEAR GUIDE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "VALUE", "22748038");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "NAME",
                       "CHECK SPINDLE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "VALUE", "8954");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|:RESET A0={NAME='CHECK LINEAR GUIDE LUB-OIL LEVEL' VALUE=22748038 "
      "TARGET=0 LAST_SERVICE_DATE=2022-04-06T04:00:00.0000Z} A1={NAME='CHECK SPINDLE LUB-OIL "
      "LEVEL' VALUE=8954 TARGET=22676400 LAST_SERVICE_DATE=2022-04-07T04:00:00.0000Z} "
      "A2={NAME='CHECK COOLANT LEVEL' VALUE=22751515 TARGET=0 "
      "LAST_SERVICE_DATE=2021-07-14T04:00:00.0000Z} A3={NAME='CHECK SPINDLE COOLANT LEVEL' "
      "VALUE=27098873 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A4={NAME='CHECK "
      "HYDRAULIC UNITOIL LEVEL' VALUE=27098872 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A5={NAME='CLEAN COOLANT FILTER' VALUE=27098871 "
      "TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A6={NAME='CHECK HYDRAULIC UNIT "
      "PRESSURE' VALUE=27098889 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} "
      "A7={NAME='CHECK AIR PRESSURE' VALUE=27098890 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A8={NAME='CLEAN CHIPS FROM WAY COVERS' "
      "VALUE=27098892 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A9={NAME='CHECK CHIP "
      "LEVEL IN CHIP BUCKET' VALUE=27098893 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} "
      "A10={NAME='CLEAN CNC & CHILLER AIR FILTERS' VALUE=27098895 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z}");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "11");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@resetTriggered",
                          "RESET");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "NAME",
                       "CHECK LINEAR GUIDE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "VALUE", "22748038");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "NAME",
                       "CHECK SPINDLE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "VALUE", "8954");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|wpo|    :DAILY A0={NAME='CHECK LINEAR GUIDE LUB-OIL LEVEL' "
      "VALUE=22748038 TARGET=0 LAST_SERVICE_DATE=2022-04-06T04:00:00.0000Z} A1={NAME='CHECK "
      "SPINDLE LUB-OIL LEVEL' VALUE=8954 TARGET=22676400 "
      "LAST_SERVICE_DATE=2022-04-07T04:00:00.0000Z} A2={NAME='CHECK COOLANT LEVEL' VALUE=22751515 "
      "TARGET=0 LAST_SERVICE_DATE=2021-07-14T04:00:00.0000Z} A3={NAME='CHECK SPINDLE COOLANT "
      "LEVEL' VALUE=27098873 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A4={NAME='CHECK "
      "HYDRAULIC UNITOIL LEVEL' VALUE=27098872 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A5={NAME='CLEAN COOLANT FILTER' VALUE=27098871 "
      "TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A6={NAME='CHECK HYDRAULIC UNIT "
      "PRESSURE' VALUE=27098889 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} "
      "A7={NAME='CHECK AIR PRESSURE' VALUE=27098890 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A8={NAME='CLEAN CHIPS FROM WAY COVERS' "
      "VALUE=27098892 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z} A9={NAME='CHECK CHIP "
      "LEVEL IN CHIP BUCKET' VALUE=27098893 TARGET=0 LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z } "
      "A10={NAME='CLEAN CNC & CHILLER AIR FILTERS' VALUE=27098895 TARGET=0 "
      "LAST_SERVICE_DATE=2021-05-19T04:00:00.0000Z } ");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "11");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@resetTriggered",
                          "DAILY");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "NAME",
                       "CHECK LINEAR GUIDE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A0", "VALUE", "22748038");

    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "NAME",
                       "CHECK SPINDLE LUB-OIL LEVEL");
    ASSERT_TABLE_ENTRY(doc, "WorkOffsetTable[@dataItemId='wp1']", "A1", "VALUE", "8954");
  }
}

TEST_F(TableTest, shoud_parse_keys_with_hypens)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']@count", "0");
  }

  m_agentTestHelper->m_adapter->processData(
      R"(2021-02-01T12:00:00Z|wpo||wpo|DAILY/1={NAME='CHECK SPINDLE LUB-OIL LEVEL' VALUE=85128 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/2={NAME='CHECK COOLANT LEVEL' VALUE=85127 TARGET=216000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/3={NAME='CHECK SPINDLE COOLANT LEVEL' VALUE=85126 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/4={NAME='CHECK HYDRAULIC UNITOIL LEVEL' VALUE=85126 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/5={NAME='CHECK HYDRAULIC UNIT PRESSURE' VALUE=85125 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/6={NAME='CHECK AIR PRESSURE' VALUE=85125 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/7={NAME='CLEAN MACHINING AREA' VALUE=85122 TARGET=216000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/8={NAME='CHECK CHIP LEVEL IN CHIP BUCKET' VALUE=85121 TARGET=216000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/9={NAME='CLEAN CNC & CHILLER AIR FILTERS' VALUE=85120 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/10={NAME='CHECK LINEAR GUIDE LUB-OIL LEVEL' VALUE=85120 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/11={NAME='CHECK AXIS SERVO LOAD' VALUE=85119 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/12={NAME='CHECK TABLE COOLANT LEVEL' VALUE=85119 TARGET=900000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} DAILY/14={NAME='Supplying grease to the ATC arm' VALUE=85117 TARGET=2700000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/1={NAME='CLEAN COOLANT TANK' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/2={NAME='CLEAN CHIP CONVEYOR' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/3={NAME='CHECK WIPERS-WAYCOVERS/LIN.GUIDES' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/4={NAME='VISUALLY CHECK HYD/AIR/LUB-OIL HOSES' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/5={NAME='CHECK OPERATION OF SAFETY DEVICES' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/6={NAME='VISUALLY CHECK CONTROL INTERIOR' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/7={NAME='CLEAN COOLING UNIT AND RADIATORS' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/8={NAME='CHECK AIR FILTER OPERATION' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/9={NAME='LUBRICATE TO LINEAR OF ATC COVER' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS1500/10={NAME='CHECK CONTROL CABINET FILTER' VALUE=85130 TARGET=5400000 LAST_SERVICE_DATE="2022-05-05T04:00:00.0000Z"} HRS3000/1={NAME='CHANGE HYDRAULIC UNIT OIL' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/2={NAME='REPLACE HYDRAULIC UNIT LINE FILTER' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/3={NAME='CLEAN SPINDLE LUB-OIL TANK & FILTER' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/4={NAME='CLEAN/CHANGE SPINDLE CHILLER TANK' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/5={NAME='CLEAN CHILLER FAN' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/6={NAME='VISUALLY CHECK ELECTRICAL WIRING' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/7={NAME='CHECK MACHINE LEVEL' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/8={NAME='CHECK Z-AXIS BRAKE FUNCTION' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/9={NAME='CLEAN/CHANGE TABLE CHILLER TANK' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/10={NAME='ADJUST CONVEYOR HINGE BELT TENSION' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/11={NAME='CHANGE LUB-OIL OF B-axis' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/12={NAME='CHANGE OIL OF 2PC,SHIFTER & ATC' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/13={NAME='CHECK WORKLIGHT' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} HRS3000/14={NAME='CHECK MAGAZINE CHAIN TENSION' VALUE=11878 TARGET=10800000 LAST_SERVICE_DATE="2022-05-06T04:00:00.0000Z"} )");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "37");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@resetTriggered",
                          nullptr);
  }
}

TEST_F(TableTest, shoud_parse_table_with_no_space)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:WorkOffsetTable[@dataItemId='wp1']@count", "0");
  }
  m_agentTestHelper->m_adapter->processData(
      R"(2021-02-01T12:00:00Z|wpo||wpo|G54={X=787.4 Y=-254 Z=-254 A=0 C=0 }G55={X=-590.0421 Y=-615.696 Z=-641.35 A=-26.26 C=-27.27 }G56={X=-314.9999 Y=-600.0001 Z=-508 A=0 C=0 }G57={X=0 Y=0 Z=0 A=0 C=0 }G58={X=0 Y=0 Z=0 A=0 C=0 }G59={X=1498.6 Y=1498.6 Z=1498.6 A=59.001 C=59.001 }G54.1P1={X=25.4 Y=50.8 Z=76.2 A=4 C=5 }G54.1P2={X=0 Y=0 Z=0 A=0 C=0 }G54.1P3={X=0 Y=0 Z=0 A=0 C=0 }G54.1P4={X=0 Y=0 Z=0 A=0 C=0 }G54.1P5={X=0 Y=0 Z=0 A=0 C=0 }G54.1P6={X=0 Y=0 Z=0 A=0 C=0 }G54.1P7={X=-558.8 Y=-584.2 Z=-609.6 A=-25 C=0 }G54.1P8={X=0 Y=0 Z=0 A=0 C=0 }G54.1P9={X=0 Y=0 Z=0 A=0 C=0 }G54.1P10={X=0 Y=0 Z=0 A=0 C=0 }G54.1P11={X=0 Y=0 Z=0 A=0 C=0 }G54.1P12={X=0 Y=0 Z=0 A=0 C=0 }G54.1P13={X=0 Y=0 Z=0 A=0 C=0 }G54.1P14={X=0 Y=0 Z=0 A=0 C=0 }G54.1P15={X=0 Y=0 Z=0 A=0 C=0 }G54.1P16={X=0 Y=0 Z=0 A=0 C=0 }G54.1P17={X=0 Y=0 Z=0 A=0 C=0 }G54.1P18={X=0 Y=0 Z=0 A=0 C=0 }G54.1P19={X=0 Y=0 Z=0 A=0 C=0 }G54.1P20={X=0 Y=0 Z=0 A=0 C=0 }G54.1P21={X=0 Y=0 Z=0 A=0 C=0 }G54.1P22={X=0 Y=0 Z=0 A=0 C=0 }G54.1P23={X=0 Y=0 Z=0 A=0 C=0 }G54.1P24={X=0 Y=0 Z=0 A=0 C=0 }G54.1P25={X=0 Y=0 Z=0 A=0 C=0 }G54.1P26={X=0 Y=0 Z=0 A=0 C=0 }G54.1P27={X=0 Y=0 Z=0 A=0 C=0 }G54.1P28={X=0 Y=0 Z=0 A=0 C=0 }G54.1P29={X=0 Y=0 Z=0 A=0 C=0 }G54.1P30={X=0 Y=0 Z=0 A=0 C=0 }G54.1P31={X=0 Y=0 Z=0 A=0 C=0 }G54.1P32={X=0 Y=0 Z=0 A=0 C=0 }G54.1P33={X=0 Y=0 Z=0 A=0 C=0 }G54.1P34={X=0 Y=0 Z=0 A=0 C=0 }G54.1P35={X=0 Y=0 Z=0 A=0 C=0 }G54.1P36={X=0 Y=0 Z=0 A=0 C=0 }G54.1P37={X=0 Y=0 Z=0 A=0 C=0 }G54.1P38={X=0 Y=0 Z=0 A=0 C=0 }G54.1P39={X=0 Y=0 Z=0 A=0 C=0 }G54.1P40={X=0 Y=0 Z=0 A=0 C=0 }G54.1P41={X=0 Y=0 Z=0 A=0 C=0 }G54.1P42={X=0 Y=0 Z=0 A=0 C=0 }G54.1P43={X=0 Y=0 Z=0 A=0 C=0 }G54.1P44={X=0 Y=0 Z=0 A=0 C=0 }G54.1P45={X=0 Y=0 Z=0 A=0 C=0 }G54.1P46={X=0 Y=0 Z=0 A=0 C=0 }G54.1P47={X=0 Y=0 Z=0 A=0 C=0 }G54.1P48={X=0 Y=0 Z=0 A=0 C=0 }G54.1P49={X=0 Y=0 Z=0 A=0 C=0 }G54.1P50={X=0 Y=0 Z=0 A=0 C=0 }G54.1P51={X=0 Y=0 Z=0 A=0 C=0 }G54.1P52={X=0 Y=0 Z=0 A=0 C=0 }G54.1P53={X=0 Y=0 Z=0 A=0 C=0 }G54.1P54={X=0 Y=0 Z=0 A=0 C=0 }G54.1P55={X=0 Y=0 Z=0 A=0 C=0 }G54.1P56={X=0 Y=0 Z=0 A=0 C=0 }G54.1P57={X=0 Y=0 Z=0 A=0 C=0 }G54.1P58={X=0 Y=0 Z=0 A=0 C=0 }G54.1P59={X=0 Y=0 Z=0 A=0 C=0 }G54.1P60={X=0 Y=0 Z=0 A=0 C=0 }G54.1P61={X=0 Y=0 Z=0 A=0 C=0 }G54.1P62={X=0 Y=0 Z=0 A=0 C=0 }G54.1P63={X=0 Y=0 Z=0 A=0 C=0 }G54.1P64={X=0 Y=0 Z=0 A=0 C=0 }G54.1P65={X=0 Y=0 Z=0 A=0 C=0 }G54.1P66={X=0 Y=0 Z=0 A=0 C=0 }G54.1P67={X=0 Y=0 Z=0 A=0 C=0 }G54.1P68={X=0 Y=0 Z=0 A=0 C=0 }G54.1P69={X=0 Y=0 Z=0 A=0 C=0 }G54.1P70={X=0 Y=0 Z=0 A=0 C=0 }G54.1P71={X=0 Y=0 Z=0 A=0 C=0 }G54.1P72={X=0 Y=0 Z=0 A=0 C=0 }G54.1P73={X=0 Y=0 Z=0 A=0 C=0 }G54.1P74={X=0 Y=0 Z=0 A=0 C=0 }G54.1P75={X=0 Y=0 Z=0 A=0 C=0 }G54.1P76={X=0 Y=0 Z=0 A=0 C=0 }G54.1P77={X=0 Y=0 Z=0 A=0 C=0 }G54.1P78={X=0 Y=0 Z=0 A=0 C=0 }G54.1P79={X=0 Y=0 Z=0 A=0 C=0 }G54.1P80={X=0 Y=0 Z=0 A=0 C=0 }G54.1P81={X=0 Y=0 Z=0 A=0 C=0 }G54.1P82={X=0 Y=0 Z=0 A=0 C=0 }G54.1P83={X=0 Y=0 Z=0 A=0 C=0 }G54.1P84={X=0 Y=0 Z=0 A=0 C=0 }G54.1P85={X=0 Y=0 Z=0 A=0 C=0 }G54.1P86={X=0 Y=0 Z=0 A=0 C=0 }G54.1P87={X=0 Y=0 Z=0 A=0 C=0 }G54.1P88={X=0 Y=0 Z=0 A=0 C=0 }G54.1P89={X=0 Y=0 Z=0 A=0 C=0 }G54.1P90={X=0 Y=0 Z=0 A=0 C=0 }G54.1P91={X=0 Y=0 Z=0 A=0 C=0 }G54.1P92={X=0 Y=0 Z=0 A=0 C=0 }G54.1P93={X=0 Y=0 Z=0 A=0 C=0 }G54.1P94={X=0 Y=0 Z=0 A=0 C=0 }G54.1P95={X=0 Y=0 Z=0 A=0 C=0 }G54.1P96={X=0 Y=0 Z=0 A=0 C=0 }G54.1P97={X=0 Y=0 Z=0 A=0 C=0 }G54.1P98={X=0 Y=0 Z=0 A=0 C=0 }G54.1P99={X=0 Y=0 Z=0 A=0 C=0 }G54.1P100={X=0 Y=0 Z=0 A=0 C=0 }G54.1P101={X=0 Y=0 Z=0 A=0 C=0 }G54.1P102={X=0 Y=0 Z=0 A=0 C=0 }G54.1P103={X=0 Y=0 Z=0 A=0 C=0 }G54.1P104={X=0 Y=0 Z=0 A=0 C=0 }G54.1P105={X=0 Y=0 Z=0 A=0 C=0 }G54.1P106={X=0 Y=0 Z=0 A=0 C=0 }G54.1P107={X=0 Y=0 Z=0 A=0 C=0 }G54.1P108={X=0 Y=0 Z=0 A=0 C=0 }G54.1P109={X=0 Y=0 Z=0 A=0 C=0 }G54.1P110={X=0 Y=0 Z=0 A=0 C=0 }G54.1P111={X=0 Y=0 Z=0 A=0 C=0 }G54.1P112={X=0 Y=0 Z=0 A=0 C=0 }G54.1P113={X=0 Y=0 Z=0 A=0 C=0 }G54.1P114={X=0 Y=0 Z=0 A=0 C=0 }G54.1P115={X=0 Y=0 Z=0 A=0 C=0 }G54.1P116={X=0 Y=0 Z=0 A=0 C=0 }G54.1P117={X=0 Y=0 Z=0 A=0 C=0 }G54.1P118={X=0 Y=0 Z=0 A=0 C=0 }G54.1P119={X=0 Y=0 Z=0 A=0 C=0 }G54.1P120={X=0 Y=0 Z=0 A=0 C=0 }G54.1P121={X=0 Y=0 Z=0 A=0 C=0 }G54.1P122={X=0 Y=0 Z=0 A=0 C=0 }G54.1P123={X=0 Y=0 Z=0 A=0 C=0 }G54.1P124={X=0 Y=0 Z=0 A=0 C=0 }G54.1P125={X=0 Y=0 Z=0 A=0 C=0 }G54.1P126={X=0 Y=0 Z=0 A=0 C=0 }G54.1P127={X=0 Y=0 Z=0 A=0 C=0 }G54.1P128={X=0 Y=0 Z=0 A=0 C=0 }G54.1P129={X=0 Y=0 Z=0 A=0 C=0 }G54.1P130={X=0 Y=0 Z=0 A=0 C=0 }G54.1P131={X=0 Y=0 Z=0 A=0 C=0 }G54.1P132={X=0 Y=0 Z=0 A=0 C=0 }G54.1P133={X=0 Y=0 Z=0 A=0 C=0 }G54.1P134={X=0 Y=0 Z=0 A=0 C=0 }G54.1P135={X=0 Y=0 Z=0 A=0 C=0 }G54.1P136={X=0 Y=0 Z=0 A=0 C=0 }G54.1P137={X=0 Y=0 Z=0 A=0 C=0 }G54.1P138={X=0 Y=0 Z=0 A=0 C=0 }G54.1P139={X=0 Y=0 Z=0 A=0 C=0 }G54.1P140={X=0 Y=0 Z=0 A=0 C=0 }G54.1P141={X=0 Y=0 Z=0 A=0 C=0 }G54.1P142={X=0 Y=0 Z=0 A=0 C=0 }G54.1P143={X=0 Y=0 Z=0 A=0 C=0 }G54.1P144={X=0 Y=0 Z=0 A=0 C=0 }G54.1P145={X=0 Y=0 Z=0 A=0 C=0 }G54.1P146={X=0 Y=0 Z=0 A=0 C=0 }G54.1P147={X=0 Y=0 Z=0 A=0 C=0 }G54.1P148={X=0 Y=0 Z=0 A=0 C=0 }G54.1P149={X=0 Y=0 Z=0 A=0 C=0 }G54.1P150={X=0 Y=0 Z=0 A=0 C=0 }G54.1P151={X=0 Y=0 Z=0 A=0 C=0 }G54.1P152={X=0 Y=0 Z=0 A=0 C=0 }G54.1P153={X=0 Y=0 Z=0 A=0 C=0 }G54.1P154={X=0 Y=0 Z=0 A=0 C=0 }G54.1P155={X=0 Y=0 Z=0 A=0 C=0 }G54.1P156={X=0 Y=0 Z=0 A=0 C=0 }G54.1P157={X=0 Y=0 Z=0 A=0 C=0 }G54.1P158={X=0 Y=0 Z=0 A=0 C=0 }G54.1P159={X=0 Y=0 Z=0 A=0 C=0 }G54.1P160={X=0 Y=0 Z=0 A=0 C=0 }G54.1P161={X=0 Y=0 Z=0 A=0 C=0 }G54.1P162={X=0 Y=0 Z=0 A=0 C=0 }G54.1P163={X=0 Y=0 Z=0 A=0 C=0 }G54.1P164={X=0 Y=0 Z=0 A=0 C=0 }G54.1P165={X=0 Y=0 Z=0 A=0 C=0 }G54.1P166={X=0 Y=0 Z=0 A=0 C=0 }G54.1P167={X=0 Y=0 Z=0 A=0 C=0 }G54.1P168={X=0 Y=0 Z=0 A=0 C=0 }G54.1P169={X=0 Y=0 Z=0 A=0 C=0 }G54.1P170={X=0 Y=0 Z=0 A=0 C=0 }G54.1P171={X=0 Y=0 Z=0 A=0 C=0 }G54.1P172={X=0 Y=0 Z=0 A=0 C=0 }G54.1P173={X=0 Y=0 Z=0 A=0 C=0 }G54.1P174={X=0 Y=0 Z=0 A=0 C=0 }G54.1P175={X=0 Y=0 Z=0 A=0 C=0 }G54.1P176={X=0 Y=0 Z=0 A=0 C=0 }G54.1P177={X=0 Y=0 Z=0 A=0 C=0 }G54.1P178={X=0 Y=0 Z=0 A=0 C=0 }G54.1P179={X=0 Y=0 Z=0 A=0 C=0 }G54.1P180={X=0 Y=0 Z=0 A=0 C=0 }G54.1P181={X=0 Y=0 Z=0 A=0 C=0 }G54.1P182={X=0 Y=0 Z=0 A=0 C=0 }G54.1P183={X=0 Y=0 Z=0 A=0 C=0 }G54.1P184={X=0 Y=0 Z=0 A=0 C=0 }G54.1P185={X=0 Y=0 Z=0 A=0 C=0 }G54.1P186={X=0 Y=0 Z=0 A=0 C=0 }G54.1P187={X=0 Y=0 Z=0 A=0 C=0 }G54.1P188={X=0 Y=0 Z=0 A=0 C=0 }G54.1P189={X=0 Y=0 Z=0 A=0 C=0 }G54.1P190={X=0 Y=0 Z=0 A=0 C=0 }G54.1P191={X=0 Y=0 Z=0 A=0 C=0 }G54.1P192={X=0 Y=0 Z=0 A=0 C=0 }G54.1P193={X=0 Y=0 Z=0 A=0 C=0 }G54.1P194={X=0 Y=0 Z=0 A=0 C=0 }G54.1P195={X=0 Y=0 Z=0 A=0 C=0 }G54.1P196={X=0 Y=0 Z=0 A=0 C=0 }G54.1P197={X=0 Y=0 Z=0 A=0 C=0 }G54.1P198={X=0 Y=0 Z=0 A=0 C=0 }G54.1P199={X=0 Y=0 Z=0 A=0 C=0 }G54.1P200={X=0 Y=0 Z=0 A=0 C=0 }G54.1P201={X=0 Y=0 Z=0 A=0 C=0 }G54.1P202={X=0 Y=0 Z=0 A=0 C=0 }G54.1P203={X=0 Y=0 Z=0 A=0 C=0 }G54.1P204={X=0 Y=0 Z=0 A=0 C=0 }G54.1P205={X=0 Y=0 Z=0 A=0 C=0 }G54.1P206={X=0 Y=0 Z=0 A=0 C=0 }G54.1P207={X=0 Y=0 Z=0 A=0 C=0 }G54.1P208={X=0 Y=0 Z=0 A=0 C=0 }G54.1P209={X=0 Y=0 Z=0 A=0 C=0 }G54.1P210={X=0 Y=0 Z=0 A=0 C=0 }G54.1P211={X=0 Y=0 Z=0 A=0 C=0 }G54.1P212={X=0 Y=0 Z=0 A=0 C=0 }G54.1P213={X=0 Y=0 Z=0 A=0 C=0 }G54.1P214={X=0 Y=0 Z=0 A=0 C=0 }G54.1P215={X=0 Y=0 Z=0 A=0 C=0 }G54.1P216={X=0 Y=0 Z=0 A=0 C=0 }G54.1P217={X=0 Y=0 Z=0 A=0 C=0 }G54.1P218={X=0 Y=0 Z=0 A=0 C=0 }G54.1P219={X=0 Y=0 Z=0 A=0 C=0 }G54.1P220={X=0 Y=0 Z=0 A=0 C=0 }G54.1P221={X=0 Y=0 Z=0 A=0 C=0 }G54.1P222={X=0 Y=0 Z=0 A=0 C=0 }G54.1P223={X=0 Y=0 Z=0 A=0 C=0 }G54.1P224={X=0 Y=0 Z=0 A=0 C=0 }G54.1P225={X=0 Y=0 Z=0 A=0 C=0 }G54.1P226={X=0 Y=0 Z=0 A=0 C=0 }G54.1P227={X=0 Y=0 Z=0 A=0 C=0 }G54.1P228={X=0 Y=0 Z=0 A=0 C=0 }G54.1P229={X=0 Y=0 Z=0 A=0 C=0 }G54.1P230={X=0 Y=0 Z=0 A=0 C=0 }G54.1P231={X=0 Y=0 Z=0 A=0 C=0 }G54.1P232={X=0 Y=0 Z=0 A=0 C=0 }G54.1P233={X=0 Y=0 Z=0 A=0 C=0 }G54.1P234={X=0 Y=0 Z=0 A=0 C=0 }G54.1P235={X=0 Y=0 Z=0 A=0 C=0 }G54.1P236={X=0 Y=0 Z=0 A=0 C=0 }G54.1P237={X=0 Y=0 Z=0 A=0 C=0 }G54.1P238={X=0 Y=0 Z=0 A=0 C=0 }G54.1P239={X=0 Y=0 Z=0 A=0 C=0 }G54.1P240={X=0 Y=0 Z=0 A=0 C=0 }G54.1P241={X=0 Y=0 Z=0 A=0 C=0 }G54.1P242={X=0 Y=0 Z=0 A=0 C=0 }G54.1P243={X=0 Y=0 Z=0 A=0 C=0 }G54.1P244={X=0 Y=0 Z=0 A=0 C=0 }G54.1P245={X=0 Y=0 Z=0 A=0 C=0 }G54.1P246={X=0 Y=0 Z=0 A=0 C=0 }G54.1P247={X=0 Y=0 Z=0 A=0 C=0 }G54.1P248={X=0 Y=0 Z=0 A=0 C=0 }G54.1P249={X=0 Y=0 Z=0 A=0 C=0 }G54.1P250={X=0 Y=0 Z=0 A=0 C=0 }G54.1P251={X=0 Y=0 Z=0 A=0 C=0 }G54.1P252={X=0 Y=0 Z=0 A=0 C=0 }G54.1P253={X=0 Y=0 Z=0 A=0 C=0 }G54.1P254={X=0 Y=0 Z=0 A=0 C=0 }G54.1P255={X=0 Y=0 Z=0 A=0 C=0 }G54.1P256={X=0 Y=0 Z=0 A=0 C=0 }G54.1P257={X=0 Y=0 Z=0 A=0 C=0 }G54.1P258={X=0 Y=0 Z=0 A=0 C=0 }G54.1P259={X=0 Y=0 Z=0 A=0 C=0 }G54.1P260={X=0 Y=0 Z=0 A=0 C=0 }G54.1P261={X=0 Y=0 Z=0 A=0 C=0 }G54.1P262={X=0 Y=0 Z=0 A=0 C=0 }G54.1P263={X=0 Y=0 Z=0 A=0 C=0 }G54.1P264={X=0 Y=0 Z=0 A=0 C=0 }G54.1P265={X=0 Y=0 Z=0 A=0 C=0 }G54.1P266={X=0 Y=0 Z=0 A=0 C=0 }G54.1P267={X=0 Y=0 Z=0 A=0 C=0 }G54.1P268={X=0 Y=0 Z=0 A=0 C=0 }G54.1P269={X=0 Y=0 Z=0 A=0 C=0 }G54.1P270={X=0 Y=0 Z=0 A=0 C=0 }G54.1P271={X=0 Y=0 Z=0 A=0 C=0 }G54.1P272={X=0 Y=0 Z=0 A=0 C=0 }G54.1P273={X=0 Y=0 Z=0 A=0 C=0 }G54.1P274={X=0 Y=0 Z=0 A=0 C=0 }G54.1P275={X=0 Y=0 Z=0 A=0 C=0 }G54.1P276={X=0 Y=0 Z=0 A=0 C=0 }G54.1P277={X=0 Y=0 Z=0 A=0 C=0 }G54.1P278={X=0 Y=0 Z=0 A=0 C=0 }G54.1P279={X=0 Y=0 Z=0 A=0 C=0 }G54.1P280={X=0 Y=0 Z=0 A=0 C=0 }G54.1P281={X=0 Y=0 Z=0 A=0 C=0 }G54.1P282={X=0 Y=0 Z=0 A=0 C=0 }G54.1P283={X=0 Y=0 Z=0 A=0 C=0 }G54.1P284={X=0 Y=0 Z=0 A=0 C=0 }G54.1P285={X=0 Y=0 Z=0 A=0 C=0 }G54.1P286={X=0 Y=0 Z=0 A=0 C=0 }G54.1P287={X=0 Y=0 Z=0 A=0 C=0 }G54.1P288={X=0 Y=0 Z=0 A=0 C=0 }G54.1P289={X=0 Y=0 Z=0 A=0 C=0 }G54.1P290={X=0 Y=0 Z=0 A=0 C=0 }G54.1P291={X=0 Y=0 Z=0 A=0 C=0 }G54.1P292={X=0 Y=0 Z=0 A=0 C=0 }G54.1P293={X=0 Y=0 Z=0 A=0 C=0 }G54.1P294={X=0 Y=0 Z=0 A=0 C=0 }G54.1P295={X=0 Y=0 Z=0 A=0 C=0 }G54.1P296={X=0 Y=0 Z=0 A=0 C=0 }G54.1P297={X=0 Y=0 Z=0 A=0 C=0 }G54.1P298={X=0 Y=0 Z=0 A=0 C=0 }G54.1P299={X=0 Y=0 Z=0 A=0 C=0 }G54.1P300={X=7620 Y=7620 Z=7620 A=300.001 C=300.001 })");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@count",
                          "306");

    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:WorkOffsetTable"
                          "[@dataItemId='wp1']@resetTriggered",
                          nullptr);
  }
}
