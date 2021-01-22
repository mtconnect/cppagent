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

#include "adapter/shdr_parser.hpp"
#include "adapter/timestamp_extractor.hpp"
#include "adapter/data_item_mapper.hpp"

#include <chrono>

using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace std;

class MockDevice : public Device
{
public:
  
};

class DataItemMappingTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context.m_getDevice = [](const std::string &uuid) { return nullptr; };
    m_context.m_getDataItem = [this](const Device *, const std::string &name) { return m_dataItems[name].get(); };
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
 
  std::map<string,unique_ptr<DataItem>> m_dataItems;
  Context m_context;
};

inline DataSetEntry operator"" _E(const char *c, std::size_t)
{
  return DataSetEntry(c);
}

TEST_F(DataItemMappingTest, SimpleEvent)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  TokenList tokens{"a", "READY"};
  
  ShdrObservation obs;
  auto token = tokens.cbegin();
  MapTokensToDataItem(obs, token, tokens.end(), m_context);
  auto &dio = get<DataItemObservation>(obs.m_observed);
  
  ASSERT_EQ(di, dio.m_dataItem);
  ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
  ASSERT_EQ("READY", get<string>(obs.m_properties["VALUE"]));
}

TEST_F(DataItemMappingTest, SimpleUnavailableEvent)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  TokenList tokens{"a", "unavailable"};
  
  ShdrObservation obs;
  auto token = tokens.cbegin();
  MapTokensToDataItem(obs, token, tokens.end(), m_context);
  auto &dio = get<DataItemObservation>(obs.m_observed);
  
  ASSERT_EQ(di, dio.m_dataItem);
  ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
  ASSERT_EQ("UNAVAILABLE", get<string>(obs.m_properties["VALUE"]));
  ASSERT_TRUE(dio.m_unavailable);
}

TEST_F(DataItemMappingTest, TwoSimpleEvents)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});
  TokenList tokens{"a", "READY", "a", "ACTIVE"};
  auto token = tokens.cbegin();

  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("READY", get<string>(obs.m_properties["VALUE"]));
  }
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("ACTIVE", get<string>(obs.m_properties["VALUE"]));
  }

}

TEST_F(DataItemMappingTest, Message)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "MESSAGE"}, {"category", "EVENT"}});
  TokenList tokens{"a", "A123", "some text"};
  auto token = tokens.cbegin();
  
  m_context.m_upcaseValue = false;

  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isMessage());
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("some text", get<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("A123", get<string>(obs.m_properties["nativeCode"]));
  }
  
  token = tokens.cbegin();

  m_context.m_upcaseValue = true;
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isMessage());
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("SOME TEXT", get<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("A123", get<string>(obs.m_properties["nativeCode"]));
  }
}

TEST_F(DataItemMappingTest, SampleTest)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  TokenList tokens{"a", "1.23456"};
  auto token = tokens.cbegin();
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isSample());
    ASSERT_TRUE(holds_alternative<double>(obs.m_properties["VALUE"]));
    ASSERT_EQ(1.23456, get<double>(obs.m_properties["VALUE"]));
  }
}

TEST_F(DataItemMappingTest, SampleTestFormatIssue)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  TokenList tokens{"a", "ABC"};
  auto token = tokens.cbegin();
  
  {
    ShdrObservation obs;
    ASSERT_THROW(MapTokensToDataItem(obs, token, tokens.end(), m_context),
                 entity::EntityError);
  }
}


TEST_F(DataItemMappingTest, SampleTimeseries)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}, {"representation", "TIME_SERIES"}
  });
  TokenList tokens{"a", "5", "100", "1.1 1.2 1.3 1.4 1.5"};
  auto token = tokens.cbegin();
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isSample());
    ASSERT_TRUE(di->isTimeSeries());
    ASSERT_TRUE(holds_alternative<entity::Vector>(obs.m_properties["VALUE"]));
    ASSERT_EQ(entity::Vector({1.1, 1.2, 1.3, 1.4, 1.5}), get<entity::Vector>(obs.m_properties["VALUE"]));
    ASSERT_EQ(5, get<int64_t>(obs.m_properties["sampleCount"]));
    ASSERT_EQ(100, get<double>(obs.m_properties["sampleRate"]));
  }
}

TEST_F(DataItemMappingTest, SampleResetTrigger)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  di->setResetTrigger("MANUAL");
  TokenList tokens{"a", "1.23456:MANUAL"};
  auto token = tokens.cbegin();
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isSample());
    ASSERT_TRUE(holds_alternative<double>(obs.m_properties["VALUE"]));
    ASSERT_EQ(1.23456, get<double>(obs.m_properties["VALUE"]));
    ASSERT_EQ("MANUAL", get<string>(obs.m_properties["resetTriggered"]));
  }
}

TEST_F(DataItemMappingTest, Condition)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}
  });
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  TokenList tokens{"a", "fault", "A123", "bad", "HIGH", "Something Bad"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isCondition());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isCondition());
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("Something Bad", get<string>(obs.m_properties["VALUE"]));
    ASSERT_EQ("A123", get<string>(obs.m_properties["nativeCode"]));
    ASSERT_EQ("HIGH", get<string>(obs.m_properties["qualifier"]));
    ASSERT_EQ("fault", get<string>(obs.m_properties["level"]));
  }
}

TEST_F(DataItemMappingTest, ConditionNormal)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}
  });
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  TokenList tokens{"a", "normal", "", "", "", ""};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isCondition());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isCondition());
    ASSERT_TRUE(holds_alternative<string>(obs.m_properties["VALUE"]));
    ASSERT_TRUE(get<string>(obs.m_properties["VALUE"]).empty());
    ASSERT_TRUE(get<string>(obs.m_properties["nativeCode"]).empty());
    ASSERT_TRUE(get<string>(obs.m_properties["qualifier"]).empty());
    ASSERT_EQ("normal", get<string>(obs.m_properties["level"]));
  }
}

TEST_F(DataItemMappingTest, ConditionNormalPartial)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "CONDITION"}});
//  <data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

  TokenList tokens{"a", "normal"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isCondition());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_TRUE(di->isCondition());
    ASSERT_EQ(1, obs.m_properties.size());
    ASSERT_EQ("normal", get<string>(obs.m_properties["level"]));
  }
}

TEST_F(DataItemMappingTest, DataSet)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "DATA_SET" }
  });

  TokenList tokens{"a", "a=1 b=2 c={abc}"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isDataSet());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_EQ(1, obs.m_properties.size());
    ASSERT_TRUE(holds_alternative<DataSet>(obs.m_properties["VALUE"]));
    
    auto &ds = get<DataSet>(obs.m_properties["VALUE"]);
    ASSERT_EQ(3, ds.size());
    ASSERT_EQ(1, get<int64_t>(ds.find("a"_E)->m_value));
    ASSERT_EQ(2, get<int64_t>(ds.find("b"_E)->m_value));
    ASSERT_EQ("abc", get<string>(ds.find("c"_E)->m_value));
  }
}

TEST_F(DataItemMappingTest, Table)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "TABLE" }
  });

  TokenList tokens{"a", "a={c=1 n=3.0} b={d=2 e=3} c={x=abc y=def}"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isTable());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    auto &dio = get<DataItemObservation>(obs.m_observed);
    
    ASSERT_EQ(di, dio.m_dataItem);
    ASSERT_EQ(1, obs.m_properties.size());
    ASSERT_TRUE(holds_alternative<DataSet>(obs.m_properties["VALUE"]));
    
    auto &ds = get<DataSet>(obs.m_properties["VALUE"]);
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
}

TEST_F(DataItemMappingTest, DataSetResetTriggered)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "DATA_SET" }
  });

  TokenList tokens{"a", ":MANUAL a=1 b=2 c={abc}"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isDataSet());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    
    ASSERT_EQ(2, obs.m_properties.size());
    ASSERT_EQ("MANUAL", get<string>(obs.m_properties["resetTriggered"]));
    auto &ds = get<DataSet>(obs.m_properties["VALUE"]);
    ASSERT_EQ(3, ds.size());
  }
}

TEST_F(DataItemMappingTest, TableResetTriggered)
{
  auto di = makeDataItem({{"id", "a"}, {"type", "SOMETHING"}, {"category", "EVENT"},
    { "representation", "TABLE" }
  });

  TokenList tokens{"a", ":DAY a={c=1 n=3.0} b={d=2 e=3} c={x=abc y=def}"};
  auto token = tokens.cbegin();
  ASSERT_TRUE(di->isTable());
  
  {
    ShdrObservation obs;
    MapTokensToDataItem(obs, token, tokens.end(), m_context);
    
    ASSERT_EQ(2, obs.m_properties.size());
    ASSERT_EQ("DAY", get<string>(obs.m_properties["resetTriggered"]));
    auto &ds = get<DataSet>(obs.m_properties["VALUE"]);
    ASSERT_EQ(3, ds.size());
  }
}
