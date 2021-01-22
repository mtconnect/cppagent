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
