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

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class DataSetTest : public testing::Test
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
    m_dataItem1 = device->getDeviceDataItem("v1");
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

TEST_F(DataSetTest, DataItem)
{
  ASSERT_TRUE(m_dataItem1->isDataSet());
  auto &attrs = m_dataItem1->getAttributes();

  ASSERT_EQ((string) "DATA_SET", attrs.at("representation"));
  ASSERT_EQ((string) "VariableDataSet", m_dataItem1->getElementName());
}

TEST_F(DataSetTest, InitialSet)
{
  string value("a=1 b=2 c=3 d=4");
  auto ce = new Observation(*m_dataItem1, 2, "time", value);

  ASSERT_EQ((size_t)4, ce->getDataSet().size());
  auto &al = ce->getAttributes();
  std::map<string, string> attrs;

  for (const auto &attr : al)
    attrs[attr.first] = attr.second;

  ASSERT_EQ((string) "4", attrs.at("count"));

  auto map1 = ce->getDataSet();

  ASSERT_EQ(1, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(3, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));

  m_checkpoint->addObservation(ce);
  auto c2 = *m_checkpoint->getEventPtr("v1");
  auto al2 = c2->getAttributes();

  attrs.clear();
  for (const auto &attr : al2)
    attrs[attr.first] = attr.second;

  ASSERT_EQ((string) "4", attrs.at("count"));

  auto map2 = c2->getDataSet();
  ASSERT_EQ(1, get<int64_t>(map2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map2.find("b"_E)->m_value));
  ASSERT_EQ(3, get<int64_t>(map2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map2.find("d"_E)->m_value));

  ce->unrefer();
}

TEST_F(DataSetTest, UpdateOneElement)
{
  string value("a=1 b=2 c=3 d=4");
  ObservationPtr ce(new Observation(*m_dataItem1, 2, "time", value));
  m_checkpoint->addObservation(ce);

  auto cecp = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, cecp->getDataSet().size());

  string value2("c=5");
  ObservationPtr ce2(new Observation(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addObservation(ce2);

  auto ce3 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, ce3->getDataSet().size());

  auto map1 = ce3->getDataSet();
  ASSERT_EQ(1, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));

  string value3("e=6");
  ObservationPtr ce4(new Observation(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addObservation(ce4);

  auto ce5 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)5, ce5->getDataSet().size());

  auto map2 = ce5->getDataSet();
  ASSERT_EQ(1, get<int64_t>(map2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map2.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map2.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map2.find("e"_E)->m_value));
}

TEST_F(DataSetTest, UpdateMany)
{
  string value("a=1 b=2 c=3 d=4");
  ObservationPtr ce(new Observation(*m_dataItem1, 2, "time", value));
  m_checkpoint->addObservation(ce);

  auto cecp = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, cecp->getDataSet().size());

  string value2("c=5 e=6");
  ObservationPtr ce2(new Observation(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addObservation(ce2);

  auto ce3 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)5, ce3->getDataSet().size());

  auto map1 = ce3->getDataSet();
  ASSERT_EQ(1, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));

  string value3("e=7 a=8 f=9");
  ObservationPtr ce4(new Observation(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addObservation(ce4);

  auto ce5 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)6, ce5->getDataSet().size());

  auto map2 = ce5->getDataSet();
  ASSERT_EQ(8, get<int64_t>(map2.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(map2.find("b"_E)->m_value));
  ASSERT_EQ(5, get<int64_t>(map2.find("c"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map2.find("d"_E)->m_value));
  ASSERT_EQ(7, get<int64_t>(map2.find("e"_E)->m_value));
  ASSERT_EQ(9, get<int64_t>(map2.find("f"_E)->m_value));
}

TEST_F(DataSetTest, Reset)
{
  string value("a=1 b=2 c=3 d=4");
  ObservationPtr ce(new Observation(*m_dataItem1, 2, "time", value));
  m_checkpoint->addObservation(ce);

  auto cecp = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, cecp->getDataSet().size());

  string value2(":MANUAL c=5 e=6");
  ObservationPtr ce2(new Observation(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addObservation(ce2);

  auto ce3 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)2, ce3->getDataSet().size());

  auto map1 = ce3->getDataSet();
  ASSERT_EQ(5, get<int64_t>(map1.find("c"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));

  string value3("x=pop y=hop");
  ObservationPtr ce4(new Observation(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addObservation(ce4);

  auto ce5 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, ce5->getDataSet().size());

  auto map2 = ce5->getDataSet();
  ASSERT_EQ((string) "pop", get<string>(map2.find("x"_E)->m_value));
  ASSERT_EQ((string) "hop", get<string>(map2.find("y"_E)->m_value));
}

TEST_F(DataSetTest, BadData)
{
  string value("12356");
  auto ce = new Observation(*m_dataItem1, 2, "time", value);

  ASSERT_EQ((size_t)1, ce->getDataSet().size());

  string value1("  a=2      b3=xxx");
  auto ce2 = new Observation(*m_dataItem1, 2, "time", value1);

  ASSERT_EQ((size_t)2, ce2->getDataSet().size());

  auto map1 = ce2->getDataSet();
  ASSERT_EQ(2, get<int64_t>(map1.find("a"_E)->m_value));
  ASSERT_EQ((string) "xxx", get<string>(map1.find("b3"_E)->m_value));
}

#define ASSERT_DATA_SET_ENTRY(doc, var, key, expected) \
  ASSERT_XML_PATH_EQUAL(doc, "//m:" var "/m:Entry[@key='" key "']", expected)

TEST_F(DataSetTest, Current)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                          "UNAVAILABLE");
  }

  m_adapter->processData("TIME|vars|a=1 b=2 c=3");

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "3");
  }

  m_adapter->processData("TIME|vars|c=6");

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[@dataItemId='v1']", "c", "6");
    ASSERT_XML_PATH_EQUAL(doc,
                          "//m:DeviceStream//m:VariableDataSet"
                          "[@dataItemId='v1']@count",
                          "3");
  }

  m_adapter->processData("TIME|vars|:MANUAL d=10");

  {
    PARSE_XML_RESPONSE;
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

  m_adapter->processData("TIME|vars|c=6");

  {
    PARSE_XML_RESPONSE;
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
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars|c=5");
  m_adapter->processData("TIME|vars|a=1 c=8");

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[2]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@count", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[3]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@count", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[4]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@count", "1");
  }

  m_agentTestHelper->m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_agentTestHelper->m_path = "/sample";
  m_adapter->processData("TIME|vars|c b=5");

  {
    PARSE_XML_RESPONSE;
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

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "2");
  }
}

TEST_F(DataSetTest, CurrentAt)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  auto seq = m_agent->getSequence();

  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars| c=5 ");
  m_adapter->processData("TIME|vars|c=8");
  m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_adapter->processData("TIME|vars|:MANUAL q=hello_there");
  m_adapter->processData("TIME|vars|r=good_bye");

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq - 1));
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq).c_str());
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 1));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq + 1).c_str());
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 2));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq + 2).c_str());
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 3));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "xxx");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "10");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq + 3).c_str());
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 4));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_XML_PATH_EQUAL(doc, "///m:VariableDataSet@resetTriggered", "MANUAL");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq + 4).c_str());
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 5));
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "r", "good_bye");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", nullptr);
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sequence", int64ToString(seq + 5).c_str());
  }

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "q", "hello_there");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "r", "good_bye");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", nullptr);
  }
}

TEST_F(DataSetTest, DeleteKey)
{
  string value("a=1 b=2 c=3 d=4");
  ObservationPtr ce(new Observation(*m_dataItem1, 2, "time", value));
  m_checkpoint->addObservation(ce);

  auto cecp = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)4, cecp->getDataSet().size());

  string value2("c e=6 a");
  ObservationPtr ce2(new Observation(*m_dataItem1, 4, "time", value2));
  m_checkpoint->addObservation(ce2);

  auto ce3 = *m_checkpoint->getEventPtr("v1");
  ASSERT_EQ((size_t)3, ce3->getDataSet().size());

  auto &ds = ce2->getDataSet();
  ASSERT_TRUE(ds.find("a"_E)->m_removed);

  auto map1 = ce3->getDataSet();
  ASSERT_EQ(2, get<int64_t>(map1.find("b"_E)->m_value));
  ASSERT_EQ(4, get<int64_t>(map1.find("d"_E)->m_value));
  ASSERT_EQ(6, get<int64_t>(map1.find("e"_E)->m_value));
  ASSERT_TRUE(map1.find("c"_E) == map1.end());
  ASSERT_TRUE(map1.find("a"_E) == map1.end());
}

TEST_F(DataSetTest, ResetWithNoItems)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars| c=5 ");
  m_adapter->processData("TIME|vars|c=8");
  m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_adapter->processData("TIME|vars|:MANUAL");
  m_adapter->processData("TIME|vars|r=good_bye");

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
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
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars|b=2");
  m_adapter->processData("TIME|vars|b=2 d=4");
  m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_adapter->processData("TIME|vars|b=3 e=4");

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
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

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "d", "4");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "5");
  }

  m_adapter->processData("TIME|vars|:MANUAL a=1 b=3 c=3 d=4 e=4");

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "b", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "c", "3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "d", "4");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[5]", "e", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@count", "5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@resetTriggered", "MANUAL");
  }

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
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
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_adapter->processData("TIME|vars|a='1 2 3' b=\"x y z\" c={cats and dogs}");

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1 2 3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "x y z");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "cats and dogs");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_adapter->processData("TIME|vars|b='u v w' c={chickens and horses");
  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "a", "1 2 3");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "b", "u v w");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "c", "cats and dogs");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }

  m_adapter->processData(
      "TIME|vars|:MANUAL V123={x1.111 2.2222 3.3333} V124={x1.111 2.2222 3.3333} V1754={\"Part 1\" "
      "2.2222 3.3333}");
  {
    PARSE_XML_RESPONSE;

    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V123", "x1.111 2.2222 3.3333");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V124", "x1.111 2.2222 3.3333");
    ASSERT_DATA_SET_ENTRY(doc, "VariableDataSet[1]", "V1754", "\"Part 1\" 2.2222 3.3333");
    ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@count", "3");
  }
}

TEST_F(DataSetTest, Discrete)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  auto di = m_agent->getDataItemByName("LinuxCNC", "vars2");
  ASSERT_EQ(true, di->isDiscrete());

  m_adapter->processData("TIME|vars2|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars2|c=5");
  m_adapter->processData("TIME|vars2|a=1 c=8");

  m_agentTestHelper->m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[1]", "UNAVAILABLE");
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

  m_agentTestHelper->m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "a", "1");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "b", "2");
    ASSERT_DATA_SET_ENTRY(doc, "BlockDataSet[1]", "c", "8");
    ASSERT_XML_PATH_EQUAL(doc, "//m:BlockDataSet[1]@count", "3");
  }
}

TEST_F(DataSetTest, Probe)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/probe";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars']@representation", "DATA_SET");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars2']@representation", "DATA_SET");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='vars2']@discrete", "true");
  }
}

TEST_F(DataSetTest, JsonCurrent)
{
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  m_agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);

  m_agentTestHelper->m_path = "/current";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3 d=cow");
  
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
      if (v == "VariableDataSet") {
        offsets = o;
        break;
      }
    }
    ASSERT_TRUE(offsets.is_object());
    
    
    ASSERT_EQ(string("4"), offsets.at("/VariableDataSet/count"_json_pointer).get<string>());

    ASSERT_EQ(1, offsets.at("/VariableDataSet/value/a"_json_pointer).get<int>());
    ASSERT_EQ(2, offsets.at("/VariableDataSet/value/b"_json_pointer).get<int>());
    ASSERT_EQ(3, offsets.at("/VariableDataSet/value/c"_json_pointer).get<int>());
    ASSERT_EQ(string("cow"), offsets.at("/VariableDataSet/value/d"_json_pointer).get<string>());

  }
}
