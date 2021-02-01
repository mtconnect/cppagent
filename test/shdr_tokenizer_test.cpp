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

#include "pipeline/shdr_tokenizer.hpp"
#include "pipeline/timestamp_extractor.hpp"
#include "entity/entity.hpp"

#include <chrono>

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace std;
using namespace date::literals;
using namespace std::literals;

class ShdrTokenizerTest : public testing::Test
{
protected:
  void SetUp() override
  {
    dlib::set_all_logging_output_streams(cout);
    dlib::set_all_logging_levels(dlib::LDEBUG);

    m_tokenizer = make_shared<ShdrTokenizer>();
    m_tokenizer->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }
  
  void TearDown() override
  {
    m_tokenizer.reset();
  }
  
  shared_ptr<ShdrTokenizer> m_tokenizer;
};

inline std::list<std::string> extract(const Properties &props)
{
  std::list<string> list;
  for (auto &p : props)
    list.emplace_back(get<string>(p.second));
  
  return list;
}

template<typename T>
inline bool isOfType(const EntityPtr &p)
{
  const auto &o = *p;
  return typeid(T) == typeid(o);
}

TEST_F(ShdrTokenizerTest, SimpleTokens)
{
  std::map<std::string, std::list<std::string>> data {
    { "   |hello   |   kitty| cat | ", { "", "hello", "kitty", "cat", "" } },
    { "hello|kitty", { "hello", "kitty" } },
    { "hello|kitty|", { "hello", "kitty", "" } },
    { "|hello|kitty|", { "", "hello", "kitty", "" } },
    { R"D(hello|xxx={b="12345", c="xxxxx"}}|bbb)D",
      { "hello", R"D(xxx={b="12345", c="xxxxx"}})D", "bbb" } },
  };
    
  for (const auto &test : data)
  {
    auto data = std::make_shared<entity::Entity>("Data", Properties{{"VALUE", test.first}});
    auto entity = (*m_tokenizer)(data);
    ASSERT_TRUE(entity);
    auto tokens = dynamic_pointer_cast<Tokens>(entity);
    ASSERT_TRUE(tokens);
    EXPECT_EQ(test.second, tokens->m_tokens) << " given text: " << test.first;
  }
}

TEST_F(ShdrTokenizerTest, escaped_line)
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
  data["x|y||z"] = { "x", "y", "", "z"};
  
  for (const auto &test : data)
  {
    auto data = std::make_shared<entity::Entity>("Data", Properties{{"VALUE", test.first}});
    auto entity = (*m_tokenizer)(data);
    ASSERT_TRUE(entity);
    auto tokens = dynamic_pointer_cast<Tokens>(entity);
    ASSERT_TRUE(tokens);
    EXPECT_EQ(test.second, tokens->m_tokens) << " given text: " << test.first;
  }
}

#if 0
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
  EXPECT_EQ("Program", o->getName());
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
    EXPECT_EQ("Program", o->getName());
  }

  {
    auto &o = *it;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Event), typeid(r));
    ASSERT_EQ("Goodbye.Kitty", get<string>(o->getValue()));
    ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
    EXPECT_EQ("Program", o->getName());
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
  EXPECT_EQ("AssetRemoved", o->getName());
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
  EXPECT_EQ("Position", o->getName());
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
  EXPECT_EQ("Position", o->getName());
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
    EXPECT_EQ("PositionTimeSeries", o->getName());
  }
  
  {
    auto o = *it;
    auto &r = *o.get();
    ASSERT_EQ(typeid(Sample), typeid(r));
    ASSERT_EQ(200.0, o->getValue<double>());
    ASSERT_EQ(ts, get<Timestamp>(o->getProperty("timestamp")));
    EXPECT_EQ("Position", o->getName());
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
  EXPECT_EQ("UserVariableDataSet", o->getName());

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
  EXPECT_EQ("UserVariableTable", o->getName());
  
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

TEST_F(ShdrParserTest, create_3d_sample)
{
  makeDataItem({{"id", "a"}, {"type", "PATH_POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER_3D"}
  });
  m_parser->processData("2021-01-19T10:01:00Z|a|1.2 2.3 3.4", m_context);

  ASSERT_EQ(1, m_observations.size());
  auto o = m_observations.front();
  auto &r = *o.get();
  ASSERT_EQ(typeid(ThreeSpaceSample), typeid(r));
  EXPECT_EQ("PathPosition", o->getName());
  
  auto &value = o->getValue<entity::Vector>();
  ASSERT_EQ(3, value.size());

  ASSERT_EQ(1.2, value[0]);
  ASSERT_EQ(2.3, value[1]);
  ASSERT_EQ(3.4, value[2]);
  
  m_observations.clear();
  m_parser->processData("2021-01-19T10:01:00Z|a|1.2 2.3", m_context);
  ASSERT_EQ(0, m_observations.size());
}

TEST_F(ShdrParserTest, create_condition)
{
  makeDataItem({{"id", "a"}, {"type", "SYSTEM"}, {"category", "CONDITION"}});
  
  // <level>|<native_code>|<native_severity>|<qualifier>|<message>
  m_parser->processData("2021-01-19T10:01:00Z|a|normal||||", m_context);
  
  {
    ASSERT_EQ(1, m_observations.size());
    auto o = m_observations.front();
    auto &r = *o.get();
    ASSERT_EQ(typeid(Condition), typeid(r));
    EXPECT_EQ("Normal", o->getName());
    EXPECT_EQ("SYSTEM", o->get<string>("type"));
  }
  
  m_observations.clear();
  m_parser->processData("2021-01-19T10:01:00Z|a|fault|ABC|100|HIGH|Message", m_context);

  {
    ASSERT_EQ(1, m_observations.size());
    auto o = m_observations.front();
    auto &r = *o.get();
    ASSERT_EQ(typeid(Condition), typeid(r));
    EXPECT_EQ("Fault", o->getName());
    EXPECT_EQ("SYSTEM", o->get<string>("type"));
    EXPECT_EQ("ABC", o->get<string>("nativeCode"));
    EXPECT_EQ("100", o->get<string>("nativeSeverity"));
    EXPECT_EQ("HIGH", o->get<string>("qualifier"));
    EXPECT_EQ("Message", o->getValue<string>());
  }

  m_observations.clear();
  m_parser->processData("2021-01-19T10:01:00Z|a|warning|ABC|100||Message", m_context);

  {
    ASSERT_EQ(1, m_observations.size());
    auto o = m_observations.front();
    auto &r = *o.get();
    ASSERT_EQ(typeid(Condition), typeid(r));
    EXPECT_EQ("Warning", o->getName());
    EXPECT_EQ("SYSTEM", o->get<string>("type"));
    EXPECT_EQ("ABC", o->get<string>("nativeCode"));
    EXPECT_EQ("100", o->get<string>("nativeSeverity"));
    EXPECT_EQ("Message", o->getValue<string>());
    EXPECT_FALSE(o->hasProperty("qualifier"));
  }

}
#endif
