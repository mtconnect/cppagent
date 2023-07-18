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

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace std::literals;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::observation;
using namespace mtconnect::sink::rest_sink;
using namespace mtconnect::buffer;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class DataSetTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/data_set.xml", 8, 4, "1.5", 25);
    m_agentId = to_string(getCurrentTimeInSec());

    m_checkpoint = nullptr;
    m_agentId = to_string(getCurrentTimeInSec());
    m_checkpoint = make_unique<Checkpoint>();

    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_dataItem1 = device->getDeviceDataItem("v1");
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

using namespace mtconnect::entity;
using namespace std::literals;
using namespace chrono_literals;
using namespace date::literals;

inline DataSetEntry operator"" _E(const char *c, std::size_t) { return DataSetEntry(c); }

TEST_F(DataSetTest, DataItem)
{
  ASSERT_TRUE(m_dataItem1->isDataSet());

  ASSERT_EQ("DATA_SET", m_dataItem1->get<string>("representation"));
  ASSERT_EQ("VariableDataSet", m_dataItem1->getObservationName());
}

TEST_F(DataSetTest, InitialSet)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto ce =
      Observation::make(m_dataItem1, Properties {{"VALUE", "a=1 b=2 c=3 d=4"s}}, time, errors);
  ASSERT_EQ(0, errors.size());

  auto ds = ce->getValue<DataSet>();

  ASSERT_EQ(4, ds.size());
  ASSERT_EQ(4, ce->get<int64_t>("count"));

  ASSERT_EQ(1, get<int64_t>(ds.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(ds.find("b"_E)->m_value));
  ASSERT_EQ(3, get<int64_t>(ds.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(ds.find("d"_E)->m_value));

  m_checkpoint->addObservation(ce);
  auto ce2 = m_checkpoint->getObservation("v1");
  auto ds2 = ce2->getValue<DataSet>();

  ASSERT_EQ(4, ce2->get<int64_t>("count"));

  ASSERT_EQ(1, get<int64_t>(ds2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(ds2.find("b"_E)->m_value));
  ASSERT_EQ(3, get<int64_t>(ds2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(ds2.find("d"_E)->m_value));
}

TEST_F(DataSetTest, parser_simple_formats)
{
  DataSet s1;
  ASSERT_TRUE(s1.parse("a=10 b=2.0 c=\"abcd\" d= e", false));

  ASSERT_EQ(5, s1.size());
  ASSERT_EQ(10, get<int64_t>(s1.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(s1.find("b"_E)->m_value));
  ASSERT_EQ("abcd", get<string>(s1.find("c"_E)->m_value));
  ASSERT_TRUE(s1.find("d"_E)->m_removed);
  ASSERT_TRUE(s1.find("e"_E)->m_removed);
}

TEST_F(DataSetTest, parser_test_with_braces)
{
  DataSet s2;
  ASSERT_TRUE(s2.parse("abc={ abc 123 }", false));
  ASSERT_EQ(1, s2.size());
  ASSERT_EQ(" abc 123 ", get<string>(s2.find("abc"_E)->m_value));
}

TEST_F(DataSetTest, parser_test_with_escaped_brace)
{
  DataSet s3;
  ASSERT_TRUE(s3.parse("abc={ abc \\} 123 }", false));
  ASSERT_EQ(1, s3.size());
  ASSERT_EQ(" abc } 123 ", get<string>(s3.find("abc"_E)->m_value));
}

TEST_F(DataSetTest, parser_test_with_escaped_quote)
{
  DataSet s4;
  ASSERT_TRUE(s4.parse("abc=' abc \\' 123 '", false));
  ASSERT_EQ(1, s4.size());
  ASSERT_EQ(" abc ' 123 ", get<string>(s4.find("abc"_E)->m_value));
}

TEST_F(DataSetTest, parser_with_bad_data)
{
  DataSet set;
  ASSERT_FALSE(set.parse("a=1 b=2.0 c={horses and dogs d=xxx", false));
  ASSERT_EQ(2, set.size());
  ASSERT_EQ(1, get<int64_t>(set.find("a"_E)->m_value));
  ASSERT_EQ(2.0, get<double>(set.find("b"_E)->m_value));
}

TEST_F(DataSetTest, parser_with_big_data_set)
{
  using namespace std::chrono;

  using namespace std::filesystem;
  path p(TEST_RESOURCE_DIR "/big_data_set.txt");
  auto size = std::filesystem::file_size(p);
  char *buffer = (char *)malloc(size + 1);
  auto file = std::fopen(p.string().c_str(), "r");
  size = std::fread(buffer, 1, size, file);
  buffer[size] = '\0';

  DataSet set;
  auto start = high_resolution_clock::now();
  for (int i = 0; i < 100; i++)
    ASSERT_TRUE(set.parse(buffer, false));
  auto now = high_resolution_clock::now();
  auto delta = floor<microseconds>(now) - floor<microseconds>(start);

  cout << endl << "Parse duration " << (delta.count() / 1000.0) << "ms" << endl << endl;
  ;

  ASSERT_EQ(116, set.size());

  free(buffer);
}

TEST_F(DataSetTest, parser_with_partial_number)
{
  DataSet set;
  ASSERT_TRUE(set.parse("a=1Bch b=2.x c=123 d=4.56", false));
  ASSERT_EQ(4, set.size());
  ASSERT_EQ("1Bch", get<string>(set.find("a"_E)->m_value));
  ASSERT_EQ("2.x", get<string>(set.find("b"_E)->m_value));
  ASSERT_EQ(123, get<int64_t>(set.find("c"_E)->m_value));
  ASSERT_EQ(4.56, get<double>(set.find("d"_E)->m_value));
}

TEST_F(DataSetTest, UpdateOneElement)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  string value("a=1 b=2 c=3 d=4");
  auto ce =
      Observation::make(m_dataItem1, Properties {{"VALUE", "a=1 b=2 c=3 d=4"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce);

  auto cecp = m_checkpoint->getObservation("v1");
  ASSERT_EQ(4, cecp->getValue<DataSet>().size());

  auto ce2 = Observation::make(m_dataItem1, Properties {{"VALUE", "c=5"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce2);

  auto ce3 = m_checkpoint->getObservation("v1");
  ASSERT_EQ(4, ce3->getValue<DataSet>().size());

  auto map1 = ce3->getValue<DataSet>();
  ASSERT_EQ(1, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));

  auto ce4 = Observation::make(m_dataItem1, Properties {{"VALUE", "e=6"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce4);

  auto ce5 = m_checkpoint->getObservation("v1");
  ASSERT_EQ(5, ce5->getValue<DataSet>().size());

  auto map2 = ce5->getValue<DataSet>();
  ASSERT_EQ(1, get<int64_t>(map2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map2.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map2.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map2.find("e"_E)->m_value));
}

TEST_F(DataSetTest, UpdateMany)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  auto ce =
      Observation::make(m_dataItem1, Properties {{"VALUE", "a=1 b=2 c=3 d=4"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce);

  auto cecp = m_checkpoint->getObservation("v1");
  ASSERT_EQ(4, cecp->getValue<DataSet>().size());

  auto ce2 = Observation::make(m_dataItem1, Properties {{"VALUE", "c=5 e=6"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce2);

  auto ce3 = m_checkpoint->getObservation("v1");

  auto map1 = ce3->getValue<DataSet>();
  ASSERT_EQ(5, map1.size());

  ASSERT_EQ(1, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));

  auto ce4 = Observation::make(m_dataItem1, Properties {{"VALUE", "e=7 a=8 f=9"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce4);

  auto ce5 = m_checkpoint->getObservation("v1");

  auto map2 = ce5->getValue<DataSet>();
  ASSERT_EQ(6, map2.size());

  ASSERT_EQ(8, get<int64_t>(map2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map2.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map2.find("d"_E)->m_value));
  ASSERT_EQ(7, get<int64_t>(map2.find("e"_E)->m_value));
  ASSERT_EQ(9, get<int64_t>(map2.find("f"_E)->m_value));
}

TEST_F(DataSetTest, Reset)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  auto ce =
      Observation::make(m_dataItem1, Properties {{"VALUE", "a=1 b=2 c=3 d=4"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce);

  auto cecp = m_checkpoint->getObservation("v1");
  ASSERT_EQ(4, cecp->getValue<DataSet>().size());

  auto ce2 = Observation::make(
      m_dataItem1, Properties {{"VALUE", "c=5 e=6"s}, {"resetTriggered", "MANUAL"}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce2);

  auto ce3 = m_checkpoint->getObservation("v1");
  auto map1 = ce3->getValue<DataSet>();
  ASSERT_EQ(2, map1.size());

  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));

  auto ce4 = Observation::make(m_dataItem1, Properties {{"VALUE", "x=pop y=hop"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce4);

  auto ce5 = m_checkpoint->getObservation("v1");
  auto map2 = ce5->getValue<DataSet>();
  ASSERT_EQ(4, map2.size());

  ASSERT_EQ((string) "pop", get<string>(map2.find("x"_E)->m_value));
  ASSERT_EQ((string) "hop", get<string>(map2.find("y"_E)->m_value));
}

TEST_F(DataSetTest, BadData)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  auto ce = Observation::make(m_dataItem1, Properties {{"VALUE", "12356"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce);

  ASSERT_EQ(1, ce->getValue<DataSet>().size());

  auto ce2 =
      Observation::make(m_dataItem1, Properties {{"VALUE", "  a=2      b3=xxx"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce2);

  ASSERT_EQ(2, ce2->getValue<DataSet>().size());

  auto map1 = ce2->getValue<DataSet>();
  ASSERT_EQ(2, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ("xxx", get<string>(map1.find("b3"_E)->m_value));
}

#define ASSERT_DATA_SET_ENTRY(doc, var, key, expected) \
  ASSERT_XML_PATH_EQUAL(doc, "//m:" var "/m:Entry[@key='" key "']", expected)

TEST_F(DataSetTest, Current)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                          "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@count", "0");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "3");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|c=6");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "c", "6");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "3");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|:MANUAL d=10");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "d", "10");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "1");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@resetTriggered",
                          "MANUAL");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|c=6");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "c", "6");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "d", "10");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "2");
  }
}

TEST_F(DataSetTest, Sample)
{
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars|c=5");
  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 c=8");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "0");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[3]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@count", "1");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|c b=5");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[3]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "b", "5");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "c", "");

    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]/m:Entry[@key='c']@removed", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]/m:Entry[@key='c']@removed", "true");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@count", "2");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "2");
  }
}

TEST_F(DataSetTest, CurrentAt)
{
  using namespace mtconnect::sink::rest_sink;
  m_agentTestHelper->addAdapter();

  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars| c=5 ");
  m_agentTestHelper->m_adapter->processData("TIME|vars|c=8");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_agentTestHelper->m_adapter->processData("TIME|vars|:MANUAL q=hello_there");
  m_agentTestHelper->m_adapter->processData("TIME|vars|r=good_bye");

  {
    QueryMap query {{"at", to_string(seq - 1)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "0");
  }

  {
    QueryMap query {{"at", to_string(seq)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq).c_str());
  }

  {
    QueryMap query {{"at", to_string(seq + 1)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq + 1).c_str());
  }

  {
    QueryMap query {{"at", to_string(seq + 2)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq + 2).c_str());
  }

  {
    QueryMap query {{"at", to_string(seq + 3)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "xxx");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "10");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq + 3).c_str());
  }

  {
    QueryMap query {{"at", to_string(seq + 4)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_XML_PATH_EQUAL(doc, "///m:VariableDataSet@resetTriggered", "MANUAL");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq + 4).c_str());
  }

  {
    QueryMap query {{"at", to_string(seq + 5)}};
    PARSE_XML_RESPONSE_QUERY("/current", query);
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "r", "good_bye");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", to_string(seq + 5).c_str());
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "r", "good_bye");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", nullptr);
  }
}

TEST_F(DataSetTest, DeleteKey)
{
  ErrorList errors;
  auto time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  auto ce =
      Observation::make(m_dataItem1, Properties {{"VALUE", "a=1 b=2 c=3 d=4"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce);

  auto cecp = m_checkpoint->getObservation("v1");
  ASSERT_EQ(4, cecp->getValue<DataSet>().size());

  auto ce2 = Observation::make(m_dataItem1, Properties {{"VALUE", "c e=6 a"s}}, time, errors);
  ASSERT_EQ(0, errors.size());
  m_checkpoint->addObservation(ce2);

  auto &ds = ce2->getValue<DataSet>();
  ASSERT_TRUE(ds.find("a"_E)->m_removed);
  ASSERT_TRUE(ds.find("c"_E)->m_removed);

  auto ce3 = m_checkpoint->getObservation("v1");
  auto &map1 = ce3->getValue<DataSet>();
  ASSERT_EQ(3, map1.size());

  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));
  ASSERT_TRUE(map1.find("c"_E) == map1.end());
  ASSERT_TRUE(map1.find("a"_E) == map1.end());
}

TEST_F(DataSetTest, ResetWithNoItems)
{
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars| c=5 ");
  m_agentTestHelper->m_adapter->processData("TIME|vars|c=8");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_agentTestHelper->m_adapter->processData("TIME|vars|:MANUAL");
  m_agentTestHelper->m_adapter->processData("TIME|vars|r=good_bye");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "0");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[3]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "a", "xxx");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "b", "10");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@count", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[6]", "");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[6]@count", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[6]@resetTriggered", "MANUAL");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[7]", "r", "good_bye");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[7]@count", "1");
  }
}

TEST_F(DataSetTest, DuplicateCompression)
{
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=2");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=2 d=4");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars|b=3 e=4");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "0");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[3]", "d", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@count", "2");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "d", "4");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "5");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|:MANUAL a=1 b=3 c=3 d=4 e=4");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "c", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "d", "4");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@count", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@resetTriggered", "MANUAL");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "d", "4");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "5");
  }
}

TEST_F(DataSetTest, QuoteDelimeter)
{
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a='1 2 3' b=\"x y z\" c={cats and dogs}");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1 2 3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "x y z");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "cats and dogs");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_agentTestHelper->m_adapter->processData("TIME|vars|b='u v w' c={chickens and horses");
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1 2 3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "u v w");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "cats and dogs");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_agentTestHelper->m_adapter->processData(
      "TIME|vars|:MANUAL V123={x1.111 2.2222 3.3333} V124={x1.111 2.2222 3.3333} V1754={\"Part 1\" "
      "2.2222 3.3333}");
  {
    PARSE_XML_RESPONSE("/current");

    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V123", "x1.111 2.2222 3.3333");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V124", "x1.111 2.2222 3.3333");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V1754", "\"Part 1\" 2.2222 3.3333");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }
}

TEST_F(DataSetTest, Discrete)
{
  m_agentTestHelper->addAdapter();

  auto di = m_agentTestHelper->m_agent->getDataItemForDevice("LinuxCNC", "vars2");
  ASSERT_EQ(true, di->isDiscrete());

  m_agentTestHelper->m_adapter->processData("TIME|vars2|a=1 b=2 c=3");
  m_agentTestHelper->m_adapter->processData("TIME|vars2|c=5");
  m_agentTestHelper->m_adapter->processData("TIME|vars2|a=1 c=8");

  {
    PARSE_XML_RESPONSE("/sample");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[1]", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[1]@count", "0");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[3]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[4]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[4]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[4]@count", "2");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[1]@count", "3");
  }
}

TEST_F(DataSetTest, Probe)
{
  m_agentTestHelper->addAdapter();

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars']@representation", "DATA_SET");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars2']@representation", "DATA_SET");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars2']@discrete", "true");
  }
}

TEST_F(DataSetTest, JsonCurrent)
{
  using namespace rest_sink;
  m_agentTestHelper->addAdapter();

  m_agentTestHelper->m_adapter->processData("TIME|vars|a=1 b=2 c=3 d=cow");

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
      if (v == "VariableDataSet")
      {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());

    ASSERT_EQ(4, offsets.at("/VariableDataSet/count"_json_pointer).get<int>());

    ASSERT_EQ(1, offsets.at("/VariableDataSet/value/a"_json_pointer).get<int>());
    ASSERT_EQ(2, offsets.at("/VariableDataSet/value/b"_json_pointer).get<int>());
    ASSERT_EQ(3, offsets.at("/VariableDataSet/value/c"_json_pointer).get<int>());
    ASSERT_EQ(string("cow"), offsets.at("/VariableDataSet/value/d"_json_pointer).get<string>());
  }
}
