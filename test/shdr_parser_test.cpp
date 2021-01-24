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
#include "adapter/shdr_tokenizer.hpp"

#include <chrono>

using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace mtconnect::observation;
using namespace std;
using namespace date::literals;
using namespace std::literals;

class ShdrParserTest : public testing::Test
{
protected:
  void SetUp() override
  {
    set_all_logging_output_streams(cout);
    set_all_logging_levels(LDEBUG);

    m_context.m_getDevice = [](const std::string &uuid) { return nullptr; };
    m_context.m_getDataItem = [this](const Device *, const std::string &name) { return m_dataItems[name].get(); };
    
    m_parser = make_unique<ShdrParser>();
    m_parser->m_observationHandler = [this](Observation2Ptr &observation){
      m_observations.emplace_back(observation);
    };
  }

  void TearDown() override
  {
    m_dataItems.clear();
    m_parser.reset();
    m_observations.clear();
  }
  
  DataItem *makeDataItem(std::map<string,string> attributes)
  {
    auto di = make_unique<DataItem>(attributes);
    DataItem *r = di.get();
    m_dataItems.emplace(attributes["id"], move(di));
    
    return r;
  }
 
  std::list<Observation2Ptr> m_observations;
  std::map<string,unique_ptr<DataItem>> m_dataItems;
  Context m_context;
  unique_ptr<ShdrParser> m_parser;
};

TEST_F(ShdrParserTest, SimpleTokens)
{
  std::map<std::string, std::list<std::string>> data {
    { "   |hello   |   kitty| cat | ", { "", "hello", "kitty", "cat", "" } },
    { "hello|kitty", { "hello", "kitty" } },
    { "hello|kitty|", { "hello", "kitty", "" } },
    { "|hello|kitty|", { "", "hello", "kitty", "" } },
    { R"D(hello|xxx={b="12345", c="xxxxx"}}|bbb)D",
      { "hello", R"D(xxx={b="12345", c="xxxxx"}})D", "bbb" } },
  };
  
  ShdrTokenizer tok;
  
  for (const auto &test : data)
  {
    std::string value;
    auto tokens = tok.tokenize(test.first);
    EXPECT_EQ(tokens, test.second) << " given text: " << test.first;
  }

}

TEST_F(ShdrParserTest, EscapedLine)
{
  std::map<std::string, std::list<std::string>> data;
  // correctly escaped
  data[R"("a\|b")"] = {"a|b"};
  data[R"("a\|b"|z)"] = {"a|b", "z"};
  data[R"(y|"a\|b")"] = {"y", "a|b"};
  data[R"(y|"a\|b"|z)"] = {"y", "a|b", "z"};

  // correctly escaped with multiple pipes
  data[R"("a\|b\|c")"] = {"a|b|c"};
  data[R"("a\|b\|c"|z)"] = {"a|b|c", "z"};
  data[R"(y|"a\|b\|c")"] = {"y", "a|b|c"};
  data[R"(y|"a\|b\|c"|z)"] = {"y", "a|b|c", "z"};

  // correctly escaped with pipe at front
  data[R"("\|b\|c")"] = {"|b|c"};
  data[R"("\|b\|c"|z)"] = {"|b|c", "z"};
  data[R"(y|"\|b\|c")"] = {"y", "|b|c"};
  data[R"(y|"\|b\|c"|z)"] = {"y", "|b|c", "z"};

  // correctly escaped with pipes at end
  data[R"("a\|b\|")"] = {"a|b|"};
  data[R"("a\|b\|"|z)"] = {"a|b|", "z"};
  data[R"(y|"a\|b\|")"] = {"y", "a|b|"};
  data[R"(y|"a\|b\|"|z)"] = {"y", "a|b|", "z"};

  // missing first quote
  data["a\\|b\""] = {"a\\", "b\""};
  data["a\\|b\"|z"] = {"a\\", "b\"", "z"};
  data["y|a\\|b\""] = {"y", "a\\", "b\""};
  data["y|a\\|b\"|z"] = {"y", "a\\", "b\"", "z"};

  // missing first quote and multiple pipes
  data[R"(a\|b\|c")"] = {"a\\", "b\\", "c\""};
  data[R"(a\|b\|c"|z)"] = {"a\\", "b\\", "c\"", "z"};
  data[R"(y|a\|b\|c")"] = {"y", "a\\", "b\\", "c\""};
  data[R"(y|a\|b\|c"|z)"] = {"y", "a\\", "b\\", "c\"", "z"};

  // missing last quote
  data["\"a\\|b"] = {"\"a\\", "b"};
  data["\"a\\|b|z"] = {"\"a\\", "b", "z"};
  data["y|\"a\\|b"] = {"y", "\"a\\", "b"};
  data["y|\"a\\|b|z"] = {"y", "\"a\\", "b", "z"};

  // missing last quote and pipe at end et al.
  data["\"a\\|"] = {"\"a\\", ""};
  data["y|\"a\\|"] = {"y", "\"a\\", ""};
  data["y|\"a\\|z"] = {"y", "\"a\\", "z"};
  data[R"(y|"a\|"z)"] = {"y", "\"a\\", "\"z"};

  ShdrTokenizer tok;
  
  for (const auto &test : data)
  {
    std::string value;
    auto tokens = tok.tokenize(test.first);
    EXPECT_EQ(tokens, test.second) << " given text: " << test.first;
  }
}

inline DataSetEntry operator"" _E(const char *c, std::size_t)
{
  return DataSetEntry(c);
}

TEST_F(ShdrParserTest, create_one_simple_event)
{
  makeDataItem({{"id", "a"}, {"type", "PROGRAM"}, {"category", "EVENT"}});
  m_parser->processData("2021-01-19T10:01:00Z|a|Hello.Kitty", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(Event), typeid(r));
  ASSERT_EQ("Hello.Kitty", get<string>(o->getValue()));
  ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
}

TEST_F(ShdrParserTest, create_two_simple_events)
{
  
  makeDataItem({{"id", "a"}, {"type", "PROGRAM"}, {"category", "EVENT"}});
  m_parser->processData("2021-01-19T10:01:00Z|a|Hello.Kitty|a|Goodbye.Kitty", m_context);
  
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  
  ASSERT_EQ(2, m_observations.size());
  auto it = m_observations.begin();
  {
    auto &o = *it++;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Event), typeid(r));
    ASSERT_EQ("Hello.Kitty", get<string>(o->getValue()));
    ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
  }

  {
    auto &o = *it;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Event), typeid(r));
    ASSERT_EQ("Goodbye.Kitty", get<string>(o->getValue()));
    ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
  }
}

TEST_F(ShdrParserTest, create_a_asset_removed_observation)
{
  makeDataItem({{"id", "a"}, {"type", "ASSET_REMOVED"}, {"category", "EVENT"}});
  m_parser->processData("2021-01-19T10:01:00Z|a|CuttingTool|ABC123", m_context);
  
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  
  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(AssetEvent), typeid(r));
  ASSERT_EQ("ABC123", get<string>(o->getValue()));
  ASSERT_EQ("CuttingTool", get<string>(o->getProperty("assetType")));
  ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
}

TEST_F(ShdrParserTest, create_simple_sample)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"}});
  m_parser->processData("2021-01-19T10:01:00Z|a|1234.5", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(Sample), typeid(r));
  ASSERT_EQ(1234.5, get<double>(o->getValue()));
  ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
}


TEST_F(ShdrParserTest, create_unavailable_sample)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"}});
  m_parser->processData("2021-01-19T10:01:00Z|a|UNAVAILABLE", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(Sample), typeid(r));
  ASSERT_TRUE(o->isUnavailable());
  ASSERT_EQ("UNAVAILABLE", o->getValue<string>());
  ASSERT_EQ(ts, o->get<Timestamp>("timestamp"));
}

TEST_F(ShdrParserTest, create_sample_time_series)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"representation", "TIME_SERIES"}
  });
  makeDataItem({{"id", "b"}, {"type", "POSITION"}, {"category", "SAMPLE"} });

  m_parser->processData("2021-01-19T10:01:00Z|a|10|100|1 2 3 4 5 6 7 8 9 10|b|200.0", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(2, m_observations.size());
  auto it = m_observations.begin();
  {
    auto o = *it++;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Timeseries), typeid(r));
    EXPECT_EQ(entity::Vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}), o->getValue<entity::Vector>());
    EXPECT_EQ(ts, o->get<Timestamp>("timestamp"));
    EXPECT_EQ(10, o->get<int64_t>("sampleCount"));
    EXPECT_EQ(100.0, o->get<double>("sampleRate"));
  }
  
  {
    auto o = *it;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Sample), typeid(r));
    ASSERT_EQ(200.0, o->getValue<double>());
    ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
  }
}

TEST_F(ShdrParserTest, create_data_set_observation)
{
  makeDataItem({{"id", "a"}, {"type", "USER_VARIABLE"}, {"category", "EVENT"},
    {"representation", "DATA_SET"}
  });
  
  m_parser->processData("2021-01-19T10:01:00Z|a|a=1 b={hello there} c=\"see\" d='dee'", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(DataSetEvent), typeid(r));
  EXPECT_EQ(ts, o->get<Timestamp>("timestamp"));
  EXPECT_EQ(4, o->get<int64_t>("count"));
  
  auto &value = o->getValue<DataSet>();
  ASSERT_EQ(4, value.size());
  EXPECT_EQ(1, value.get<int64_t>("a"));
  EXPECT_EQ("hello there", value.get<string>("b"));
  EXPECT_EQ("see", value.get<string>("c"));
  EXPECT_EQ("dee", value.get<string>("d"));
}

TEST_F(ShdrParserTest, create_table_observation)
{
  makeDataItem({{"id", "a"}, {"type", "USER_VARIABLE"}, {"category", "EVENT"},
    {"representation", "TABLE"}
  });
  
  m_parser->processData("2021-01-19T10:01:00Z|a|a={x=1 y=2 z=3} b={s='abc' t=1.2}", m_context);
  auto ts = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(DataSetEvent), typeid(r));
  EXPECT_EQ(ts, o->get<Timestamp>("timestamp"));
  EXPECT_EQ(2, o->get<int64_t>("count"));
  
  auto &value = o->getValue<DataSet>();
  ASSERT_EQ(2, value.size());
  
  auto &dsa = value.get<DataSet>("a");
  ASSERT_EQ(3, dsa.size());
  EXPECT_EQ(1, dsa.get<int64_t>("x"));
  EXPECT_EQ(2, dsa.get<int64_t>("y"));
  EXPECT_EQ(3, dsa.get<int64_t>("z"));

  auto &dsb = value.get<DataSet>("b");
  ASSERT_EQ(2, dsb.size());
  EXPECT_EQ("abc", dsb.get<string>("s"));
  EXPECT_EQ(1.2, dsb.get<double>("t"));
}
