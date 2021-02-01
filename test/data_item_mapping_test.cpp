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

#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/timestamp_extractor.hpp"
#include "pipeline/pipeline_context.hpp"
#include "observation/observation.hpp"
#include <chrono>

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace std;

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(std::map<string,unique_ptr<DataItem>> &items)
  : m_dataItems(items)
  {
  }
  DataItem *findDataItem(const std::string &device, const std::string &name) override
  {
    return m_dataItems[name].get();
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr )override {}
  void deliverAssetCommand(entity::EntityPtr ) override {}
  void deliverCommand(entity::EntityPtr )override {}
  void deliverConnectStatus(entity::EntityPtr )override {}
  
  std::map<string,unique_ptr<DataItem>> &m_dataItems;
};

class DataItemMappingTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems);
    m_mapper = make_shared<ShdrTokenMapper>(m_context);
    m_mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override
  {
    m_dataItems.clear();
  }
  
  DataItem *makeDataItem(std::map<string,string> attributes)
  {
    auto di = make_unique<DataItem>(attributes);
    DataItem *r = di.get();
    m_dataItems.emplace(attributes["id"], move(di));
    
    return r;
  }
  
  TimestampedPtr makeTimestamped(TokenList tokens)
  {
    auto ts = make_shared<Timestamped>();
    ts->m_tokens = tokens;
    ts->m_timestamp = chrono::system_clock::now();
    ts->setProperty("timestamp", ts->m_timestamp);
    return ts;
  }

  shared_ptr<PipelineContext> m_context;
  shared_ptr<ShdrTokenMapper> m_mapper;
  std::map<string,unique_ptr<DataItem>> m_dataItems;
};

inline DataSetEntry operator"" _E(const char *c, std::size_t)
{
  return DataSetEntry(c);
}

TEST_F(DataItemMappingTest, SimpleEvent)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  auto ts = makeTimestamped({"a", "READY"});
  
  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
    
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());
  auto obs = oblist.front();
  auto event = dynamic_pointer_cast<Event>(obs);
  ASSERT_TRUE(event);
  
  ASSERT_EQ(di, event->getDataItem());
  ASSERT_TRUE(event->hasProperty("VALUE"));
  ASSERT_EQ("READY", event->getValue<string>());
}

TEST_F(DataItemMappingTest, SimpleUnavailableEvent)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  auto ts = makeTimestamped({"a", "unavailable"});
  
  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
    
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());
  auto event = dynamic_pointer_cast<Event>(oblist.front());
  ASSERT_TRUE(event);
  
  ASSERT_EQ(di, event->getDataItem());
  ASSERT_EQ("UNAVAILABLE", event->getValue<string>());
  ASSERT_TRUE(event->isUnavailable());
}

TEST_F(DataItemMappingTest, TwoSimpleEvents)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  auto ts = makeTimestamped({"a", "READY", "a", "ACTIVE"});
  
  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
    
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(2, oblist.size());

  auto oi = oblist.begin();
    
  {
    auto event = dynamic_pointer_cast<Event>(*oi++);
    ASSERT_TRUE(event);
    ASSERT_EQ(di, event->getDataItem());
    ASSERT_EQ("READY", event->getValue<string>());
  }
  
  {
    auto event = dynamic_pointer_cast<Event>(*oi++);
    ASSERT_TRUE(event);
    ASSERT_EQ(di, event->getDataItem());
    ASSERT_EQ("ACTIVE", event->getValue<string>());
  }
}

TEST_F(DataItemMappingTest, Message)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "MESSAGE"}, {"category", "EVENT"}});
  auto ts = makeTimestamped({"a", "A123", "some text"});

  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
    
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  {
    auto event = dynamic_pointer_cast<Message>(oblist.front());
    ASSERT_TRUE(event);
    ASSERT_EQ(di, event->getDataItem());
    ASSERT_TRUE(di->isMessage());
    ASSERT_EQ("some text", event->getValue<string>());
    ASSERT_EQ("A123", event->get<string>("nativeCode"));
  }
}

TEST_F(DataItemMappingTest, SampleTest)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  auto ts = makeTimestamped({"a", "1.23456"});
  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
    
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto sample = dynamic_pointer_cast<Sample>(oblist.front());
  ASSERT_TRUE(sample);
  ASSERT_EQ(di, sample->getDataItem());
  ASSERT_TRUE(di->isSample());
  ASSERT_EQ(1.23456, sample->getValue<double>());
}

TEST_F(DataItemMappingTest, SampleTestFormatIssue)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  auto ts = makeTimestamped({"a", "ABC"});
  auto observations = (*m_mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(0, oblist.size());
  // entity::EntityError
}

TEST_F(DataItemMappingTest, SampleTimeseries)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}, {"representation", "TIME_SERIES"}
  });
  auto ts = makeTimestamped({"a", "5", "100", "1.1 1.2 1.3 1.4 1.5"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto sample = dynamic_pointer_cast<Timeseries>(oblist.front());
  ASSERT_TRUE(sample);
  ASSERT_EQ(di, sample->getDataItem());
  ASSERT_TRUE(di->isTimeSeries());
  ASSERT_EQ(entity::Vector({1.1, 1.2, 1.3, 1.4, 1.5}), sample->getValue<entity::Vector>());
  ASSERT_EQ(5, sample->get<int64_t>("sampleCount"));
  ASSERT_EQ(100.0, sample->get<double>("sampleRate"));
}

TEST_F(DataItemMappingTest, SampleResetTrigger)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  di->setResetTrigger("MANUAL");
  auto ts = makeTimestamped({"a", "1.23456:MANUAL"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto sample = dynamic_pointer_cast<Sample>(oblist.front());
  ASSERT_TRUE(sample);

  ASSERT_EQ(di, sample->getDataItem());
  ASSERT_TRUE(di->isSample());
  ASSERT_EQ(1.23456, sample->getValue<double>());
  ASSERT_EQ("MANUAL", sample->get<string>("resetTriggered"));
}

TEST_F(DataItemMappingTest, Condition)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}
  });
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  auto ts = makeTimestamped({"a", "fault", "A123", "bad", "HIGH", "Something Bad"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto cond = dynamic_pointer_cast<Condition>(oblist.front());
  ASSERT_TRUE(cond);
  
  ASSERT_EQ(di, cond->getDataItem());
  ASSERT_TRUE(di->isCondition());
  ASSERT_EQ("Something Bad", cond->getValue<string>());
  ASSERT_EQ("A123", cond->get<string>("nativeCode"));
  ASSERT_EQ("HIGH", cond->get<string>("qualifier"));
  ASSERT_EQ("Fault", cond->getName());
}

TEST_F(DataItemMappingTest, ConditionNormal)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}
  });
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  auto ts = makeTimestamped({"a", "normal", "", "", "", ""});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto cond = dynamic_pointer_cast<Condition>(oblist.front());
  ASSERT_TRUE(cond);
  
  ASSERT_EQ(di, cond->getDataItem());
  ASSERT_TRUE(di->isCondition());
  ASSERT_FALSE(cond->hasProperty("VALUE"));
  ASSERT_FALSE(cond->hasProperty("nativeCode"));
  ASSERT_FALSE(cond->hasProperty("qualifier"));
  ASSERT_EQ("Normal", cond->getName());
}


TEST_F(DataItemMappingTest, ConditionNormalPartial)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}});
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  auto ts = makeTimestamped({"a", "normal"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto cond = dynamic_pointer_cast<Condition>(oblist.front());
  ASSERT_TRUE(cond);
  
  ASSERT_EQ(di, cond->getDataItem());
  ASSERT_TRUE(di->isCondition());
  ASSERT_FALSE(cond->hasProperty("VALUE"));
  ASSERT_FALSE(cond->hasProperty("nativeCode"));
  ASSERT_FALSE(cond->hasProperty("qualifier"));
  ASSERT_EQ("Normal", cond->getName());
}

TEST_F(DataItemMappingTest, DataSet)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "DATA_SET" }
  });

  auto ts = makeTimestamped({"a", "a=1 b=2 c={abc}"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto set = dynamic_pointer_cast<DataSetEvent>(oblist.front());
  ASSERT_TRUE(set);
  ASSERT_EQ("SomethingDataSet", set->getName());

  ASSERT_EQ(di, set->getDataItem());
  
  auto &ds = set->getValue<DataSet>();
  ASSERT_EQ(3, ds.size());
  ASSERT_EQ(1, get<int64_t>(ds.find("a"_E)->m_value));
  ASSERT_EQ(2, get<int64_t>(ds.find("b"_E)->m_value));
  ASSERT_EQ("abc", get<string>(ds.find("c"_E)->m_value));
}

TEST_F(DataItemMappingTest, Table)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "TABLE" }
  });

  auto ts = makeTimestamped({"a", "a={c=1 n=3.0} b={d=2 e=3} c={x=abc y=def}"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto set = dynamic_pointer_cast<DataSetEvent>(oblist.front());
  ASSERT_TRUE(set);

  ASSERT_EQ(di, set->getDataItem());
  ASSERT_EQ("SomethingTable", set->getName());
  
  auto &ds = set->getValue<DataSet>();
  ASSERT_EQ(3, ds.size());
  auto a = get<DataSet>(ds.find("a"_E)->m_value);
  ASSERT_EQ(2, a.size());
  ASSERT_EQ(1, get<int64_t>(a.find("c"_E)->m_value));
  ASSERT_EQ(3.0, get<double>(a.find("n"_E)->m_value));
  
  auto b = get<DataSet>(ds.find("b"_E)->m_value);
  ASSERT_EQ(2, a.size());
  ASSERT_EQ(2, get<int64_t>(b.find("d"_E)->m_value));
  ASSERT_EQ(3, get<int64_t>(b.find("e"_E)->m_value));
  
  auto c = get<DataSet>(ds.find("c"_E)->m_value);
  ASSERT_EQ(2, c.size());
  ASSERT_EQ("abc", get<string>(c.find("x"_E)->m_value));
  ASSERT_EQ("def", get<string>(c.find("y"_E)->m_value));
}

TEST_F(DataItemMappingTest, DataSetResetTriggered)
{
  makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "DATA_SET" }
  });

  auto ts = makeTimestamped({"a", ":MANUAL a=1 b=2 c={abc}"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto set = dynamic_pointer_cast<DataSetEvent>(oblist.front());
  ASSERT_TRUE(set);

  ASSERT_EQ("SomethingDataSet", set->getName());
  ASSERT_EQ("MANUAL", set->get<string>("resetTriggered"));
  auto &ds = set->getValue<DataSet>();
  ASSERT_EQ(3, ds.size());
}

TEST_F(DataItemMappingTest, TableResetTriggered)
{
  makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "TABLE" }
  });

  auto ts = makeTimestamped({"a", ":DAY a={c=1 n=3.0} b={d=2 e=3} c={x=abc y=def}"});
  auto observations = (*m_mapper)(ts);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto set = dynamic_pointer_cast<DataSetEvent>(oblist.front());
  ASSERT_TRUE(set);
    
  ASSERT_EQ("DAY", set->get<string>("resetTriggered"));
  auto &ds = set->getValue<DataSet>();
  ASSERT_EQ(3, ds.size());
}
